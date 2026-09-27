#include "platform/windows/operations/magnet_owned_abort_diagnostic.h"
#include <iostream>
#include <stdexcept>

namespace a=panebind::test::abort;
namespace {
int checks{};
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);++checks;}
a::AbortAuthority safe(){
    a::AbortAuthority s;
    s.ledger={10,9,11,12,1134816073663LL,1134816073673LL,1134816073666LL,1134816073670LL,1134816073680LL,8,true,true,true,true,false,0,0};
    s.fixture_scope_active=s.own_identity=s.desktop_ready=s.source_visible=s.foreground_matches=true;
    s.gui_query_succeeded=s.capture_is_source=s.move_size_is_source=s.expected_native_mode=s.menu_clear=true;
    s.cursor_available=s.root_is_source=s.left_down=s.other_input_clear=s.receiver_healthy=s.log_healthy=true;
    s.writer_quiescent=s.acceptance_retired=s.input_mapping_supported=s.api_boundary_stable=true;
    return s;
}
}
int main(){
    try{
        a::FenceFailureSnapshot snapshot;
        check(!a::classify_fence(snapshot).passed,"empty/unknown snapshot cannot silently pass mandatory predicates");
        for(std::size_t i=0;i<snapshot.count;++i)snapshot.set(static_cast<a::FencePredicate>(i),a::PredicateState::Pass);
        check(a::classify_fence(snapshot).passed,"complete original fence can pass");
        snapshot.set(a::FencePredicate::CursorPosition,a::PredicateState::Fail);
        snapshot.set(a::FencePredicate::LeftState,a::PredicateState::Fail);
        auto result=a::classify_fence(snapshot);
        check(!result.passed&&result.first==a::FencePredicate::CursorPosition&&result.failed_count==2,"same ordered evidence retains first failure and compound known failures");
        check(a::fence_subreason(result.first)=="CURSOR_DEVIATION"&&a::fence_reason(result.first)=="BLOCKED_BY_INPUT_INTERFERENCE","specific subreason preserves old generic failure class");
        snapshot.set(a::FencePredicate::CursorPosition,a::PredicateState::Unknown);
        snapshot.set(a::FencePredicate::LeftState,a::PredicateState::NotEvaluated);
        result=a::classify_fence(snapshot);
        check(!result.passed&&result.failed_count==0,"unknown/unevaluated facts are not fabricated as failure or pass");
        snapshot.set(a::FencePredicate::CursorPosition,a::PredicateState::Pass);
        snapshot.set(a::FencePredicate::LeftState,a::PredicateState::Pass);
        snapshot.set(a::FencePredicate::CancelledGuiSafe,a::PredicateState::NotEvaluated,false);
        check(a::classify_fence(snapshot).passed,"not-applicable cancelled branch does not alter original normal fence");
        const auto authority=safe();check(authority.eligible(),"exact own active native loop and committed pending DOWN can qualify");
        check(authority.ledger.raw_down_receiver_qpc<authority.ledger.down_return_qpc&&authority.eligible(),"real Raw DOWN may arrive before parent API return/log");
        for(bool a::AbortAuthority::* member:{&a::AbortAuthority::fixture_scope_active,&a::AbortAuthority::own_identity,&a::AbortAuthority::desktop_ready,&a::AbortAuthority::source_visible,&a::AbortAuthority::foreground_matches,&a::AbortAuthority::gui_query_succeeded,&a::AbortAuthority::capture_is_source,&a::AbortAuthority::move_size_is_source,&a::AbortAuthority::expected_native_mode,&a::AbortAuthority::menu_clear,&a::AbortAuthority::cursor_available,&a::AbortAuthority::root_is_source,&a::AbortAuthority::left_down,&a::AbortAuthority::other_input_clear,&a::AbortAuthority::receiver_healthy,&a::AbortAuthority::log_healthy,&a::AbortAuthority::writer_quiescent,&a::AbortAuthority::acceptance_retired,&a::AbortAuthority::input_mapping_supported,&a::AbortAuthority::api_boundary_stable}){
            auto bad=authority;bad.*member=false;check(!bad.eligible(),"each missing native-abort prerequisite denies input");
        }
        auto bad=authority;bad.foreign_capture_transferred=true;check(!bad.eligible(),"foreign capture transfer denies input");
        bad=authority;bad.ledger.down_sent=false;check(!bad.eligible(),"unsuccessful API does not own DOWN");
        bad=authority;bad.ledger.raw_down_matches=false;check(!bad.eligible(),"SendInput success alone does not own DOWN");
        bad=authority;bad.ledger.matching_up_seen=true;check(!bad.eligible(),"already released DOWN never gets another UP");
        bad=authority;bad.ledger.up_command_sent=true;check(!bad.eligible(),"an already inserted UP without confirmed receipt cannot authorize another UP");
        bad=authority;bad.ledger.non_test_button_transitions=1;check(!bad.eligible(),"non-test button transition makes ownership ambiguous");
        bad=authority;bad.ledger.unmatched_button_transitions=1;check(!bad.eligible(),"same-tag unmatched transition also denies input");
        bad=authority;bad.ledger.raw_down_receiver_sequence=bad.ledger.receiver_watermark;check(!bad.eligible(),"old preflight receipt cannot prove current DOWN");
        bad=authority;bad.ledger.native_enter_matches=false;check(!bad.eligible(),"capture without actual native ENTER is not this authority");
        const auto boundary_bad=bad;check(authority.eligible()&&!boundary_bad.eligible(),"changed fresh API boundary denies input without retry");
        a::AbortCompletion completion{true,true,true,true,true,true,true,true,true,true,true,true,true,true};
        check(completion.result()=="PASS","independent Raw/EXIT/matchedEND/final release closes cleanup only");
        for(bool a::AbortCompletion::* member:{&a::AbortCompletion::raw_up_matches,&a::AbortCompletion::native_exit_observed,&a::AbortCompletion::winevent_end_matches,&a::AbortCompletion::final_context_reliable,&a::AbortCompletion::final_capture_clear,&a::AbortCompletion::final_mode_clear,&a::AbortCompletion::final_left_up,&a::AbortCompletion::ledger_settled,&a::AbortCompletion::writer_quiescent,&a::AbortCompletion::acceptance_retired,&a::AbortCompletion::no_pending_work,&a::AbortCompletion::cleanup_scope}){
            auto missing=completion;missing.*member=false;check(missing.result()=="UNCONFIRMED","missing independent completion evidence cannot pass cleanup");
        }
        completion.sent=false;check(completion.result()=="FAILED","attempted API failure is not skipped");
        completion.attempted=false;check(completion.result()=="SKIPPED_NO_AUTHORITY","zero API with no authority is skipped");
        std::cout<<"owned-abort diagnostic synthetic_only=true checks="<<checks<<" PASS\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
