#pragma once

#include "platform/windows/product/v1_guest_up_evidence.h"
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>

namespace panebind::platform::windows::product::guest {

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
