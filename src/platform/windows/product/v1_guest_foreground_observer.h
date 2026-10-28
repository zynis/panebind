#pragma once

// Disposable-guest evidence only. This observes foreground changes; it never
// activates, queries text, changes input routing, or grants product authority.
// Install/unhook on the fixture UI STA. The callback only fills immutable,
// bounded slots; the driver STA drains them to its existing evidence sink.
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

namespace panebind::platform::windows::product::guest {

enum class ForegroundTaskProcessRole {Fixture,MainProduct,ProductCommand,ExplicitHelper};

class ForegroundObserver {
public:
    static constexpr std::size_t capacity=256;
    ForegroundObserver()=default;
    ForegroundObserver(const ForegroundObserver&)=delete;
    ForegroundObserver& operator=(const ForegroundObserver&)=delete;

    // Read only on the same serialized consumer as drain(). This is a bounded
    // buffer/lifecycle snapshot, not proof of unobserved OS events or hook health.
    struct EvidenceSnapshot {bool installed{},stopped{};std::uint64_t reserved{},flushed{},overflow{};};
    [[nodiscard]] EvidenceSnapshot evidence_snapshot()const noexcept {
        const bool installed=lifecycle_ready_[0].load(std::memory_order_acquire)&&lifecycle_[0].returned;
        const auto overflow=overflow_.load(std::memory_order_acquire);
        const auto reserved=reserved_.load(std::memory_order_acquire);
        return {installed,lifecycle_ready_[1].load(std::memory_order_acquire),reserved,drained_,overflow};
    }

    // Call only after the exact guest/RunId guards. The object must outlive
    // its hook (the fixture uses process-lifetime storage, not a stack lease).
    bool start() noexcept {
        if(start_attempted_)return false;start_attempted_=true;owner_tid_=GetCurrentThreadId();
        auto& fact=lifecycle_[0];fact.start=true;fact.owner_tid=owner_tid_;fact.call_tid=owner_tid_;
        fact.before=qpc();LARGE_INTEGER frequency{};fact.frequency_known=QueryPerformanceFrequency(&frequency)!=FALSE;
        fact.frequency=frequency.QuadPart;ForegroundObserver* empty{};
        if(active_.compare_exchange_strong(empty,this,std::memory_order_acq_rel)){
            SetLastError(0);hook_=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,
                callback,0,0,WINEVENT_OUTOFCONTEXT);
            fact.returned=hook_!=nullptr;fact.raw_last_error=GetLastError();fact.hook=reinterpret_cast<std::uintptr_t>(hook_);
            if(!hook_){auto* expected=this;active_.compare_exchange_strong(expected,nullptr,std::memory_order_acq_rel);}
        }else fact.existing_observer=true;
        fact.after=qpc();lifecycle_ready_[0].store(true,std::memory_order_release);return fact.returned;
    }

    // Only this instance's actual hook, on its original UI thread. No waiting
    // for input, no process shutdown, and no disk IO belongs in this method.
    bool stop() noexcept {
        if(stop_attempted_)return stop_succeeded_;stop_attempted_=true;
        auto& fact=lifecycle_[1];fact.owner_tid=owner_tid_;fact.call_tid=GetCurrentThreadId();
        fact.hook=reinterpret_cast<std::uintptr_t>(hook_);fact.before=qpc();
        if(hook_&&fact.call_tid==owner_tid_){
            fact.attempted=true;SetLastError(0);fact.returned=UnhookWinEvent(hook_)!=FALSE;fact.raw_last_error=GetLastError();
            if(fact.returned){hook_=nullptr;auto* expected=this;active_.compare_exchange_strong(expected,nullptr,std::memory_order_acq_rel);}
        }
        fact.after=qpc();stop_succeeded_=fact.returned;lifecycle_ready_[1].store(true,std::memory_order_release);return stop_succeeded_;
    }

    // Serialized driver-side calls, using handles already retained by the
    // test launcher. No handle is adopted or modified. Register immediately
    // after CreateProcess, including short-lived --status/--command children.
    // A later registration can correlate an earlier callback snapshot only
    // through BOTH its sampled PID and sampled process birth, never flush PID.
    template<class Emit> bool register_process(HANDLE process,ForegroundTaskProcessRole role,Emit&& emit){
        ProcessIdentity identity;identity.before=qpc();identity.pid=GetProcessId(process);identity.role=role;
        identity.known=identity.pid&&birth(process,identity.birth,identity.birth_error);identity.after=qpc();
        const bool stored=identity.known&&registrations_used_<registrations_.size();
        if(stored)registrations_[registrations_used_++]=identity;else if(identity.known)++registration_overflow_;
        emit("guest_foreground_process_registration","\"pid\":"+std::to_string(identity.pid)+
            ",\"process_birth_filetime\":"+(identity.known?std::to_string(identity.birth):"null")+
            ",\"identity_known\":"+boolean(identity.known)+",\"birth_query_error\":"+std::to_string(identity.birth_error)+
            ",\"role\":"+quoted(role_name(role))+",\"registered\":"+boolean(stored)+
            ",\"query_begin_qpc\":"+std::to_string(identity.before)+",\"query_end_qpc\":"+std::to_string(identity.after)+
            ",\"registration_overflow\":"+std::to_string(registration_overflow_)+",\"not_window_control_authority\":true");
        return stored;
    }

    // Invoke outside native callbacks, on the existing driver STA after its
    // bounded message pump and in drive's final cleanup. A final drain may be
    // made on the UI thread only AFTER driver.join(), unhook and Raw removal;
    // all sink/registry/consumer calls must remain serialized.
    template<class Emit> void drain(Emit&& emit){
        for(std::size_t n=0;n<lifecycle_.size();++n){
            if(lifecycle_emitted_[n]||!lifecycle_ready_[n].load(std::memory_order_acquire))continue;
            const auto& f=lifecycle_[n];emit("guest_foreground_observer_lifecycle","\"operation\":"+quoted(f.start?"install":"unhook")+
                ",\"owner_tid\":"+std::to_string(f.owner_tid)+",\"call_tid\":"+std::to_string(f.call_tid)+
                ",\"hook\":"+std::to_string(f.hook)+",\"native_attempted\":"+boolean(f.start?!f.existing_observer:f.attempted)+
                ",\"native_return\":"+boolean(f.returned)+",\"raw_last_error\":"+std::to_string(f.raw_last_error)+
                ",\"last_error_is_not_documented_root_cause\":true,\"call_begin_qpc\":"+std::to_string(f.before)+
                ",\"call_end_qpc\":"+std::to_string(f.after)+",\"qpc_frequency_known\":"+boolean(f.frequency_known)+
                ",\"qpc_frequency\":"+std::to_string(f.frequency)+",\"skip_own_process\":false,\"skip_own_thread\":false"+
                ",\"event_min\":3,\"event_max\":3,\"out_of_context\":true,\"capacity\":"+std::to_string(capacity));
            lifecycle_emitted_[n]=true;
        }
        while(drained_<capacity&&slots_[drained_].ready.load(std::memory_order_acquire)){
            const auto& f=slots_[drained_].fact;
            DWORD flush_pid{};const auto flush_tid=f.hwnd?GetWindowThreadProcessId(f.hwnd,&flush_pid):0;
            const auto flush_qpc=qpc();std::string roles="[";
            if(f.window_identity_stable&&f.birth_known)for(std::size_t n=0;n<registrations_used_;++n){
                const auto& known=registrations_[n];if(known.pid!=f.pid_before||known.birth!=f.birth)continue;
                if(roles.size()>1)roles+=',';roles+=quoted(role_name(known.role));
            }
            const bool registered=roles.size()>1;roles+=']';bool class_utf8_known{},image_basename_known{};
            const auto class_utf8=utf8_class(f,class_utf8_known);
            const auto image_basename=utf8_image_basename(f,image_basename_known);
            emit("guest_foreground_observed","\"observer_slot\":"+std::to_string(drained_)+
                ",\"event\":"+std::to_string(f.event)+",\"hwnd\":"+std::to_string(reinterpret_cast<std::uintptr_t>(f.hwnd))+
                ",\"object_id\":"+std::to_string(f.object)+",\"child_id\":"+std::to_string(f.child)+
                ",\"event_tid\":"+std::to_string(f.event_tid)+",\"native_event_time_u32\":"+std::to_string(f.native_time)+
                ",\"callback_tid\":"+std::to_string(f.callback_tid)+",\"callback_begin_qpc\":"+std::to_string(f.before)+
                ",\"callback_end_qpc\":"+std::to_string(f.after)+",\"callback_window_pid_before\":"+std::to_string(f.pid_before)+
                ",\"callback_window_tid_before\":"+std::to_string(f.tid_before)+",\"callback_window_pid_after\":"+std::to_string(f.pid_after)+
                ",\"callback_window_tid_after\":"+std::to_string(f.tid_after)+",\"callback_window_identity_stable\":"+boolean(f.window_identity_stable)+
                ",\"callback_process_birth_known\":"+boolean(f.birth_known)+",\"callback_process_birth_filetime\":"+(f.birth_known?std::to_string(f.birth):"null")+
                ",\"process_open_error\":"+std::to_string(f.open_error)+",\"process_birth_error\":"+std::to_string(f.birth_error)+
                ",\"process_image_query_attempted\":"+boolean(f.image_attempted)+",\"process_image_query_return\":"+boolean(f.image_returned)+
                ",\"process_image_query_error\":"+std::to_string(f.image_error)+",\"process_image_size_returned\":"+std::to_string(f.image_length)+
                ",\"callback_process_image_basename_known\":"+boolean(image_basename_known)+
                ",\"callback_process_image_basename\":"+(image_basename_known?quoted(image_basename):"null")+
                ",\"process_full_path_logged\":false,\"basename_is_not_role_or_activation_authority\":true"+
                ",\"process_handle_closed\":"+boolean(f.process_closed)+",\"class_query_return\":"+std::to_string(f.class_length)+
                ",\"class_query_error\":"+std::to_string(f.class_error)+",\"class_may_be_truncated\":"+boolean(f.class_truncated)+
                ",\"class_utf8_known\":"+boolean(class_utf8_known)+",\"callback_class\":"+(class_utf8_known?quoted(class_utf8):"null")+
                ",\"callback_foreground_before\":"+std::to_string(reinterpret_cast<std::uintptr_t>(f.foreground_before))+
                ",\"callback_foreground_after\":"+std::to_string(reinterpret_cast<std::uintptr_t>(f.foreground_after))+
                ",\"registered_task_PID_birth_match\":"+boolean(registered)+",\"registered_task_roles\":"+roles+
                ",\"flush_current_window_pid\":"+std::to_string(flush_pid)+",\"flush_current_window_tid\":"+std::to_string(flush_tid)+
                ",\"flush_window_matches_callback_PID_TID\":"+boolean(flush_pid&&flush_pid==f.pid_before&&flush_tid==f.tid_before)+
                ",\"flush_qpc\":"+std::to_string(flush_qpc)+",\"identity_sample_phase\":\"callback_delivery_not_native_event_time\""+
                ",\"event_producer_or_activation_initiator_proven\":false,\"JSONL_order_is_not_cross_producer_native_order\":true"+
                ",\"title_or_content_read\":false,\"not_window_control_authority\":true");
            ++drained_;
        }
        const auto reserved=reserved_.load(std::memory_order_acquire),overflow=overflow_.load(std::memory_order_acquire);
        if(reserved!=last_reserved_||overflow!=last_overflow_||lifecycle_ready_[1].load(std::memory_order_acquire)&&!summary_after_stop_){
            const bool stopped=lifecycle_ready_[1].load(std::memory_order_acquire);
            emit("guest_foreground_observer_buffer","\"reserved\":"+std::to_string(reserved)+",\"flushed\":"+std::to_string(drained_)+
                ",\"capacity\":"+std::to_string(capacity)+",\"overflow\":"+std::to_string(overflow)+
                ",\"observations_overwritten\":false,\"overflow_means_incomplete_evidence\":true,\"stop_receipt_available\":"+boolean(stopped));
            last_reserved_=reserved;last_overflow_=overflow;summary_after_stop_=stopped;
        }
    }

private:
    struct Fact {
        HWND hwnd{},foreground_before{},foreground_after{};DWORD event{},event_tid{},native_time{},callback_tid{};LONG object{},child{};
        std::uint64_t before{},after{},birth{};DWORD pid_before{},tid_before{},pid_after{},tid_after{},open_error{},birth_error{},class_error{};
        std::array<wchar_t,256> class_name{};int class_length{};bool class_truncated{},birth_known{},window_identity_stable{},process_closed{};
        // Private callback POD only. Never serialize this path: the driver
        // emits just its basename, and failure remains UNKNOWN without retry.
        std::array<wchar_t,1024> process_image{};DWORD image_length{},image_error{};bool image_attempted{},image_returned{};
    };
    struct Slot {Fact fact;std::atomic<bool> ready{};};
    struct Lifecycle {bool start{},attempted{},returned{},existing_observer{},frequency_known{};DWORD owner_tid{},call_tid{},raw_last_error{};
        std::uint64_t before{},after{};std::int64_t frequency{};std::uintptr_t hook{};};
    struct ProcessIdentity {DWORD pid{},birth_error{};std::uint64_t birth{},before{},after{};ForegroundTaskProcessRole role{};bool known{};};
    static std::uint64_t qpc() noexcept {LARGE_INTEGER value{};return QueryPerformanceCounter(&value)&&value.QuadPart>0?static_cast<std::uint64_t>(value.QuadPart):0;}
    static bool birth(HANDLE process,std::uint64_t& value,DWORD& error) noexcept {
        FILETIME created{},ended{},kernel{},user{};SetLastError(0);if(!GetProcessTimes(process,&created,&ended,&kernel,&user)){error=GetLastError();return false;}
        value=(static_cast<std::uint64_t>(created.dwHighDateTime)<<32)|created.dwLowDateTime;return value!=0;
    }
    static const char* boolean(bool value){return value?"true":"false";}
    static std::string_view role_name(ForegroundTaskProcessRole role){switch(role){case ForegroundTaskProcessRole::Fixture:return "fixture";
        case ForegroundTaskProcessRole::MainProduct:return "main_product";case ForegroundTaskProcessRole::ProductCommand:return "product_command";
        case ForegroundTaskProcessRole::ExplicitHelper:return "explicit_helper";}return "UNKNOWN";}
    static std::string quoted(std::string_view value){std::string out="\"";constexpr char hex[]="0123456789ABCDEF";
        for(const unsigned char c:value){if(c=='\\'||c=='\"'){out+='\\';out+=static_cast<char>(c);}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=static_cast<char>(c);}out+='\"';return out;}
    static std::string utf8_class(const Fact& fact,bool& known){known=false;if(fact.class_length<=0||fact.class_truncated)return {};
        std::array<char,1024> bytes{};const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,fact.class_name.data(),fact.class_length,
            bytes.data(),static_cast<int>(bytes.size()),nullptr,nullptr);known=count>0;return known?std::string(bytes.data(),static_cast<std::size_t>(count)):std::string{};}
    static std::string utf8_image_basename(const Fact& fact,bool& known){
        known=false;if(!fact.image_returned||!fact.image_length||fact.image_length>=fact.process_image.size())return {};
        const std::wstring_view path(fact.process_image.data(),fact.image_length);const auto separator=path.find_last_of(L"\\/");
        const auto name=separator==path.npos?path:path.substr(separator+1);if(name.empty())return {};
        std::array<char,4096> bytes{};const auto count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,name.data(),static_cast<int>(name.size()),
            bytes.data(),static_cast<int>(bytes.size()),nullptr,nullptr);known=count>0;
        return known?std::string(bytes.data(),static_cast<std::size_t>(count)):std::string{};
    }
    static void CALLBACK callback(HWINEVENTHOOK hook,DWORD event,HWND hwnd,LONG object,LONG child,DWORD event_tid,DWORD native_time) noexcept {
        const DWORD saved_error=GetLastError();auto* self=active_.load(std::memory_order_acquire);
        if(!self||self->hook_!=hook||event!=EVENT_SYSTEM_FOREGROUND){SetLastError(saved_error);return;}
        const auto index=self->reserved_.fetch_add(1,std::memory_order_acq_rel);
        if(index>=capacity){self->overflow_.fetch_add(1,std::memory_order_relaxed);SetLastError(saved_error);return;}
        Fact fact;fact.before=qpc();fact.hwnd=hwnd;fact.event=event;fact.object=object;fact.child=child;
        fact.event_tid=event_tid;fact.native_time=native_time;fact.callback_tid=GetCurrentThreadId();fact.foreground_before=GetForegroundWindow();
        if(hwnd){
            fact.tid_before=GetWindowThreadProcessId(hwnd,&fact.pid_before);SetLastError(0);
            fact.class_length=GetClassNameW(hwnd,fact.class_name.data(),static_cast<int>(fact.class_name.size()));
            fact.class_error=fact.class_length?0:GetLastError();fact.class_truncated=fact.class_length>=static_cast<int>(fact.class_name.size()-1);
            if(fact.pid_before&&fact.tid_before){SetLastError(0);HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,fact.pid_before);
                fact.open_error=process?0:GetLastError();if(process){fact.birth_known=birth(process,fact.birth,fact.birth_error);
                    fact.image_attempted=true;fact.image_length=static_cast<DWORD>(fact.process_image.size());SetLastError(0);
                    fact.image_returned=QueryFullProcessImageNameW(process,0,fact.process_image.data(),&fact.image_length)!=FALSE;
                    fact.image_error=fact.image_returned?0:GetLastError();
                    fact.process_closed=CloseHandle(process)!=FALSE;}}
            fact.tid_after=GetWindowThreadProcessId(hwnd,&fact.pid_after);
            fact.window_identity_stable=fact.pid_before&&fact.tid_before&&fact.pid_before==fact.pid_after&&fact.tid_before==fact.tid_after;
        }
        fact.foreground_after=GetForegroundWindow();fact.after=qpc();self->slots_[static_cast<std::size_t>(index)].fact=fact;
        self->slots_[static_cast<std::size_t>(index)].ready.store(true,std::memory_order_release);SetLastError(saved_error);
    }

    inline static std::atomic<ForegroundObserver*> active_{};
    std::array<Slot,capacity> slots_{};std::array<ProcessIdentity,capacity> registrations_{};
    std::array<Lifecycle,2> lifecycle_{};std::array<std::atomic<bool>,2> lifecycle_ready_{};std::array<bool,2> lifecycle_emitted_{};
    std::atomic<std::uint64_t> reserved_{},overflow_{};std::uint64_t last_reserved_{},last_overflow_{};
    std::size_t drained_{},registrations_used_{},registration_overflow_{};bool summary_after_stop_{};
    HWINEVENTHOOK hook_{};DWORD owner_tid_{};bool start_attempted_{},stop_attempted_{},stop_succeeded_{};
};

} // namespace panebind::platform::windows::product::guest
