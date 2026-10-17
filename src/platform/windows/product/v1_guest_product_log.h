#pragma once

#include "platform/windows/product/v1_guest_up_evidence.h"
#include <istream>
#include <string>
#include <string_view>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

namespace panebind::platform::windows::product::guest {

// TEST driver evidence is append-only, unlike the rotating product log.
// Bound the requested suffix, not the whole file: a long but finite I11
// phase must not turn an earlier valid byte mark into substr(empty, mark).
inline std::uint64_t driver_log_mark(std::istream& stream){
    stream.seekg(0,std::ios::end);const auto end=stream.tellg();
    if(!stream||end<std::streampos{})throw std::runtime_error("driver_log_size_unavailable");
    return static_cast<std::uint64_t>(static_cast<std::streamoff>(end));
}
inline std::string driver_log_suffix(std::istream& stream,std::uint64_t mark,std::size_t cap=2*1024*1024){
    const auto end=driver_log_mark(stream);
    if(mark>end)throw std::runtime_error("driver_log_mark_after_actual_end");
    const auto bytes=end-mark;
    if(bytes>cap)throw std::runtime_error("driver_log_suffix_limit");
    stream.seekg(static_cast<std::streamoff>(mark),std::ios::beg);
    std::string result(static_cast<std::size_t>(bytes),'\0');
    if(bytes)stream.read(result.data(),static_cast<std::streamsize>(bytes));
    if(!stream||(bytes&&static_cast<std::uint64_t>(stream.gcount())!=bytes))
        throw std::runtime_error("driver_log_suffix_read_incomplete");
    return result;
}

// Only a known ordinary file disappearing between attributes and file_size
// can consume the existing pair's one retry. Permission and size failures
// never become "rotation". These are Windows system error values, not errno.
inline bool product_log_metadata_pair_retry_allowed(unsigned pair_attempt,bool plain_attributes,
    const std::error_code& size_error,bool size_over_limit){
    return pair_attempt==0&&plain_attributes&&!size_over_limit&&size_error.category()==std::system_category()&&
        (size_error.value()==2||size_error.value()==3);
}
// The product logs this GUI-owner fact after finishing the Paused gesture.
// It carries no gesture generation: callers must read only the validated
// current-product suffix marked before the one real test hotkey.
inline bool matching_emergency_hotkey_receipt(std::string_view row,std::uint64_t product_pid,
    std::string_view product_sha){
    using namespace up_evidence_detail;
    const auto count=number(row,"\"received_count\":");
    return row.size()<=16384&&!row.empty()&&row.front()=='{'&&row.back()=='}'&&
        row.find_first_of("\r\n")==row.npos&&product_pid&&product_sha.size()==40&&
        quoted(row,"\"schema\":","panebind-v1/1")&&quoted(row,"\"source_sha\":",product_sha)&&
        number(row,"\"process_id\":")==product_pid&&number(row,"\"sequence\":")>0&&
        number(row,"\"tick\":")>0&&quoted(row,"\"type\":","emergency_hotkey")&&
        quoted(row,"\"native_message\":","WM_HOTKEY")&&
        number(row,"\"registration_id\":")==910&&count&&*count>0;
}

// TEST reader only. Physical .1/current rotation must not change an already
// issued logical byte mark. Actual launch identity starts a new segment;
// sequence continuity and duplicate original text are checked, not invented.
class ProductLogEvidence {
public:
    void begin(std::uint64_t pid,std::uint64_t birth,std::uint64_t earliest_tick,std::string source_sha){
        if(!pid||!birth||source_sha.size()!=40)throw std::runtime_error("product_log_launch_identity_unknown");
        pid_=pid;birth_=birth;earliest_tick_=earliest_tick;source_sha_=std::move(source_sha);
        epoch_begin_=text_.size();row_offsets_.clear();rejected_=false;
    }
    [[nodiscard]] bool bound()const{return pid_!=0;}
    [[nodiscard]] std::uint64_t pid()const{return pid_;}
    [[nodiscard]] std::uint64_t birth()const{return birth_;}
    [[nodiscard]] bool rejected()const{return rejected_;}
    void reject(){rejected_=true;}
    [[nodiscard]] const std::string& text()const{return text_;}
    [[nodiscard]] std::string_view current()const{return std::string_view(text_).substr(epoch_begin_);}
    [[nodiscard]] std::optional<std::string_view> since(std::size_t mark)const{
        if(mark==0)return current();
        if(mark<epoch_begin_||mark>text_.size())return {};
        return std::string_view(text_).substr(mark);
    }
    // A failed physical snapshot is not committed: the caller may reread the
    // pair ONCE for a concurrent rotation. Persistent gaps/conflicts fail closed.
    [[nodiscard]] bool observe(std::string_view previous,std::string_view current,std::string& why){
        using namespace up_evidence_detail;
        if(rejected_){why="previous_evidence_rejected";return false;}
        std::vector<std::string_view> append;std::size_t added{};
        for(const auto file:{previous,current}){
            std::size_t at{};
            while(at<file.size()){
                const auto end=file.find('\n',at);if(end==file.npos)break; // incomplete tail is not a receipt
                const auto original=file.substr(at,end-at+1);auto row=original.substr(0,original.size()-1);at=end+1;
                if(row.ends_with('\r'))row.remove_suffix(1);
                if(row.empty())continue;
                const auto pid=number(row,"\"process_id\":");if(!pid||*pid!=pid_)continue;
                const auto tick=number(row,"\"tick\":");if(tick&&*tick<earliest_tick_)continue;
                if(!tick||row.size()>32768||row.front()!='{'||row.back()!='}'||
                   !quoted(row,"\"schema\":","panebind-v1/1")||!quoted(row,"\"source_sha\":",source_sha_)){
                    why="current_launch_record_identity_or_shape_invalid";return false;
                }
                const auto sequence=number(row,"\"sequence\":");
                if(!sequence||!*sequence){why="current_launch_sequence_unknown";return false;}
                const auto known=row_offsets_.size();
                if(*sequence<=known){
                    const auto begin=row_offsets_[static_cast<std::size_t>(*sequence-1)];
                    const auto finish=*sequence<known?row_offsets_[static_cast<std::size_t>(*sequence)]:text_.size();
                    if(std::string_view(text_).substr(begin,finish-begin)!=original){why="duplicate_sequence_original_text_conflict";return false;}
                }else if(*sequence<=known+append.size()){
                    if(append[static_cast<std::size_t>(*sequence-known-1)]!=original){why="snapshot_duplicate_sequence_conflict";return false;}
                }else if(*sequence==known+append.size()+1){
                    append.push_back(original);added+=original.size();
                }else{why="current_launch_sequence_gap";return false;}
            }
        }
        constexpr std::size_t maximum_bytes=16*1024*1024;
        if(added>maximum_bytes-text_.size()){why="bounded_test_evidence_cache_limit";return false;}
        for(const auto row:append){row_offsets_.push_back(text_.size());text_.append(row);}
        why.clear();return true;
    }
private:
    std::uint64_t pid_{},birth_{},earliest_tick_{};
    std::string source_sha_,text_;std::size_t epoch_begin_{};
    std::vector<std::size_t> row_offsets_;bool rejected_{};
};

}
