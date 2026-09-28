#include "core/behavior/move_handoff_gate.h"

#include <iostream>

using panebind::core::behavior::MoveHandoffGate;
using panebind::core::behavior::MoveHandoffEscapeReason;

namespace {
int checks{}, failures{};
void check(bool value, const char* name) {
    ++checks;
    if (!value) {
        ++failures;
        std::cerr << "FAIL " << name << '\n';
    }
}
}

int main() {
    {
        MoveHandoffGate gate;
        check(!gate.begin(0, true, true, true), "zero gesture rejected");
        check(!gate.begin(1, false, true, true), "unbound source rejected");
        check(!gate.begin(1, true, false, true), "missing held DOWN rejected");
        check(!gate.begin(1, true, true, false), "Ctrl or Resize rejected");
        check(gate.begin(1, true, true, true), "exact ordinary Move may begin");
        check(!gate.begin(2, true, true, true), "one gesture per gate");
        check(!gate.may_cancel(1) && !gate.may_write(1, true, true, true),
              "no cancel or write before actual isolation");
        check(!gate.isolation_ready(1, false), "style or creation alone not hit proof");
        check(gate.isolation_ready(1, true) && gate.may_cancel(1),
              "proven isolation precedes cancel");
        check(!gate.may_cancel(2) && !gate.cancel_issued(2), "foreign generation rejected");
        check(gate.cancel_issued(1) && !gate.cancel_issued(1), "one cancel maximum");
        check(!gate.may_write(1, true, true, true), "no write before matching END");
        check(!gate.native_end_observed(1, false), "wrong END rejected");
        check(gate.native_end_observed(1, true), "matching native END recorded");
        check(!gate.may_write(1, false, true, true) &&
              !gate.may_write(1, true, false, true) &&
              !gate.may_write(1, true, true, false),
              "fresh authority actual and hit are per-quantum requirements");
        check(gate.may_write(1, true, true, true), "post-END fresh quantum permitted");
        check(!gate.legacy_up_delivery_observed(1, false), "assumed legacy UP rejected");
        check(gate.raw_up_observed(1, true), "physical Raw UP recorded");
        check(!gate.may_write(1, true, true, true) && !gate.may_cancel(1),
              "Raw UP immediately retires pending writes and cancel");
        check(!gate.normal_removal_allowed(1), "Raw UP alone cannot remove overlay normally");
        check(gate.legacy_up_delivery_observed(1, true) && gate.normal_removal_allowed(1),
              "actual legacy delivery permits normal teardown");
        check(gate.isolation_removed(1) && !gate.may_write(1, true, true, true),
              "removed isolation cannot grant later write");
    }
    {
        MoveHandoffGate gate;
        check(gate.begin(2, true, true, true), "early-UP fixture begin");
        check(gate.raw_up_observed(2, true) && !gate.isolation_ready(2, true) &&
              !gate.may_cancel(2), "UP before setup avoids isolation and cancel");
        check(gate.escape(2, MoveHandoffEscapeReason::EarlyUp) &&
              !gate.may_write(2, true, true, true),
              "early UP cannot become writer");
    }
    {
        MoveHandoffGate gate;
        check(gate.begin(3, true, true, true) && gate.isolation_ready(3, true),
              "UP-between setup");
        check(gate.cancel_issued(3), "UP-between cancel issued");
        check(gate.raw_up_observed(3, true), "UP arrived before END");
        check(gate.native_end_observed(3, true) && !gate.may_write(3, true, true, true),
              "END does not revive completed physical gesture");
        check(!gate.removal_allowed(3), "missing legacy delivery cannot normal-remove");
        check(gate.escape(3, MoveHandoffEscapeReason::LegacyUpMissing) &&
              gate.removal_allowed(3) &&
              gate.isolation_removed(3), "failure escape removes own overlay without PASS");
    }
    {
        MoveHandoffGate gate;
        check(gate.begin(4, true, true, true) && gate.isolation_ready(4, true),
              "natural END fixture setup");
        check(gate.native_end_observed(4, true) && !gate.may_cancel(4) &&
              !gate.may_write(4, true, true, true),
              "natural END before cancel is native-only");
        check(gate.raw_up_observed(4, true) && gate.legacy_up_delivery_observed(4, true) &&
              gate.normal_removal_allowed(4), "actual UP delivery permits teardown without cancel");
    }
    {
        MoveHandoffGate gate;
        check(gate.begin(5, true, true, true) && gate.isolation_ready(5, true) &&
              gate.cancel_issued(5) && gate.native_end_observed(5, true),
              "deadline fixture active");
        check(gate.may_write(5, true, true, true), "active writer before deadline");
        check(gate.escape(5, MoveHandoffEscapeReason::Deadline) &&
              !gate.may_write(5, true, true, true),
              "deadline revokes writer independently of UP");
        check(!gate.normal_removal_allowed(5) && gate.removal_allowed(5) &&
              gate.isolation_removed(5), "deadline escape restores desktop but is not acceptance");
        check(gate.escape_reason() == MoveHandoffEscapeReason::Deadline,
              "escape reason retained without a dangling string view");
    }
    std::cout << "move-handoff-gate checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
