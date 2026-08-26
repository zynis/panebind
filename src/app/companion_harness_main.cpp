#include "core/geometry/checked_arithmetic.h"
#include "core/movement/move_plan.h"
#include "core/movement/translation.h"
#include "core/topology/window_adjacency.h"
#include "platform/windows/companion/companion_session.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace companion = panebind::platform::windows::companion;
namespace geometry = panebind::core::geometry;
namespace model = panebind::core::model;
namespace movement = panebind::core::movement;
namespace protocol = panebind::platform::windows::companion::protocol;
namespace topology = panebind::core::topology;

using companion::CompanionOperationResult;
using companion::CompanionOperationStage;
using companion::CompanionOperationStatus;
using companion::CompanionSession;
using companion::CompanionWindowOperations;
using companion::CompanionWindowSnapshot;
using companion::CompanionWindowToken;
using companion::CompanionWindowTranslationRequest;

constexpr std::size_t kWindowCount = protocol::kWindowCount;
constexpr std::size_t kLeaderIndex = 0U;
constexpr std::size_t kDestroyedIndex = 2U;

[[nodiscard]] std::string json_quote(const std::string_view value) {
    std::string result;
    result.reserve(value.size() + 2U);
    result.push_back('"');
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (character < 0x20U) {
                result += '?';
            } else {
                result.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    result.push_back('"');
    return result;
}

[[nodiscard]] std::string narrow_ascii(const std::wstring_view value) {
    std::string result;
    result.reserve(value.size());
    for (const wchar_t character : value) {
        result.push_back(character <= 0x7f ? static_cast<char>(character) : '?');
    }
    return result;
}

void append_rect(std::ostream& output, const geometry::Rect& rect) {
    output << "{\"left\":" << rect.left() << ",\"top\":" << rect.top()
           << ",\"right\":" << rect.right() << ",\"bottom\":"
           << rect.bottom() << '}';
}

[[nodiscard]] char logical_label(const protocol::LogicalWindowId logical_id) {
    return static_cast<char>('A' + static_cast<std::uint32_t>(logical_id) - 1U);
}

[[nodiscard]] std::string logical_name(
    const protocol::LogicalWindowId logical_id) {
    return std::string(1U, logical_label(logical_id));
}

[[nodiscard]] std::size_t logical_index(
    const protocol::LogicalWindowId logical_id) {
    return static_cast<std::size_t>(logical_label(logical_id) - 'A');
}

[[nodiscard]] protocol::LogicalWindowId logical_id(const std::size_t index) {
    return static_cast<protocol::LogicalWindowId>(index + 1U);
}

[[nodiscard]] std::string_view stage_name(
    const CompanionOperationStage stage) noexcept {
    switch (stage) {
    case CompanionOperationStage::Preflight:
        return "preflight";
    case CompanionOperationStage::TargetArm:
        return "target_arm";
    case CompanionOperationStage::SingleApply:
        return "single_apply";
    case CompanionOperationStage::BeginBatch:
        return "begin_batch";
    case CompanionOperationStage::DeferBatch:
        return "defer_batch";
    case CompanionOperationStage::EndBatch:
        return "end_batch";
    case CompanionOperationStage::PostVerification:
        return "post_verification";
    }
    return "unknown";
}

[[nodiscard]] std::string_view status_name(
    const CompanionOperationStatus status) noexcept {
    switch (status) {
    case CompanionOperationStatus::Succeeded:
        return "succeeded";
    case CompanionOperationStatus::InvalidRequest:
        return "invalid_request";
    case CompanionOperationStatus::DuplicateToken:
        return "duplicate_token";
    case CompanionOperationStatus::StaleToken:
        return "stale_token";
    case CompanionOperationStatus::SessionExited:
        return "session_exited";
    case CompanionOperationStatus::SessionPoisoned:
        return "session_poisoned";
    case CompanionOperationStatus::InvalidWindow:
        return "invalid_window";
    case CompanionOperationStatus::WrongProcess:
        return "wrong_process";
    case CompanionOperationStatus::WrongThread:
        return "wrong_thread";
    case CompanionOperationStatus::WrongWindowClass:
        return "wrong_window_class";
    case CompanionOperationStatus::NotIndependentTopLevel:
        return "not_independent_top_level";
    case CompanionOperationStatus::MarkerFailure:
        return "marker_failure";
    case CompanionOperationStatus::GeometryCaptureFailed:
        return "geometry_capture_failed";
    case CompanionOperationStatus::DpiContextMismatch:
        return "dpi_context_mismatch";
    case CompanionOperationStatus::EmptyGeometry:
        return "empty_geometry";
    case CompanionOperationStatus::ResizeRejected:
        return "resize_rejected";
    case CompanionOperationStatus::ArithmeticOverflow:
        return "arithmetic_overflow";
    case CompanionOperationStatus::NativeCoordinateOutOfRange:
        return "native_coordinate_out_of_range";
    case CompanionOperationStatus::TargetArmFailed:
        return "target_arm_failed";
    case CompanionOperationStatus::SingleApplyFailed:
        return "single_apply_failed";
    case CompanionOperationStatus::BeginBatchFailed:
        return "begin_batch_failed";
    case CompanionOperationStatus::DeferBatchFailed:
        return "defer_batch_failed";
    case CompanionOperationStatus::EndBatchFailed:
        return "end_batch_failed";
    case CompanionOperationStatus::PostVerificationFailed:
        return "post_verification_failed";
    case CompanionOperationStatus::WindowInvalidatedDuringApply:
        return "window_invalidated_during_apply";
    }
    return "unknown";
}

class TestRecorder final {
public:
    void check(const bool condition, const std::string_view name) {
        std::cout << "{\"schema_version\":1,\"record_kind\":\"test\""
                  << ",\"name\":" << json_quote(name)
                  << ",\"result\":" << json_quote(condition ? "PASS" : "FAIL")
                  << "}\n";
        std::cout.flush();
        if (!condition) {
            ++failures_;
        }
    }

    void require(const bool condition, const std::string_view name) {
        check(condition, name);
        if (!condition) {
            throw std::runtime_error{std::string{name}};
        }
    }

    void not_tested(const std::string_view name,
                    const std::string_view reason) const {
        std::cout << "{\"schema_version\":1,\"record_kind\":\"test\""
                  << ",\"name\":" << json_quote(name)
                  << ",\"result\":\"NOT_TESTED\",\"reason\":"
                  << json_quote(reason) << "}\n";
    }

    [[nodiscard]] int failures() const noexcept { return failures_; }

private:
    int failures_{};
};

[[nodiscard]] std::filesystem::path target_executable_path() {
    std::wstring buffer(32768U, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0U || length >= buffer.size()) {
        throw std::runtime_error{"GetModuleFileNameW failed"};
    }
    buffer.resize(length);
    return std::filesystem::path{buffer}.parent_path() /
           L"panebind-companion-target.exe";
}

[[nodiscard]] const CompanionWindowToken& token_for(
    const std::vector<CompanionWindowToken>& tokens,
    const protocol::LogicalWindowId sought) {
    const auto found = std::find_if(tokens.begin(), tokens.end(), [&](const auto& token) {
        return token.logical_id() == sought;
    });
    if (found == tokens.end()) {
        throw std::logic_error{"companion token set is incomplete"};
    }
    return *found;
}

[[nodiscard]] CompanionWindowSnapshot capture_required(
    CompanionWindowOperations& operations,
    const CompanionWindowToken& token,
    TestRecorder& tests,
    const std::string_view name) {
    auto capture = operations.capture(token);
    tests.require(capture.succeeded(), name);
    return *capture.snapshot;
}

[[nodiscard]] std::array<CompanionWindowSnapshot, kWindowCount> capture_all(
    CompanionWindowOperations& operations,
    const std::vector<CompanionWindowToken>& tokens,
    TestRecorder& tests,
    const std::string_view phase) {
    std::array<CompanionWindowSnapshot, kWindowCount> snapshots{};
    for (std::size_t index = 0; index < snapshots.size(); ++index) {
        snapshots[index] = capture_required(
            operations,
            token_for(tokens, logical_id(index)),
            tests,
            std::string{phase} + " capture " +
                logical_name(logical_id(index)));
    }
    return snapshots;
}

[[nodiscard]] topology::WindowAdjacencyGraph build_graph(
    const std::array<CompanionWindowSnapshot, kWindowCount>& snapshots) {
    std::vector<topology::WindowGeometry> windows;
    windows.reserve(snapshots.size());
    for (std::size_t index = 0; index < snapshots.size(); ++index) {
        windows.push_back({model::WindowId{logical_name(logical_id(index))},
                           snapshots[index].visible_rect});
    }
    return topology::WindowAdjacencyGraph::build(windows,
                                                  {.edge_tolerance = 0});
}

[[nodiscard]] geometry::Coordinate checked_sum(
    const geometry::Coordinate value,
    const geometry::Distance delta) {
    const auto sum = geometry::checked_add(value, delta);
    if (!sum.has_value()) {
        throw std::overflow_error{"companion harness coordinate overflow"};
    }
    return *sum;
}

[[nodiscard]] std::array<geometry::Rect, kWindowCount> desired_two_by_two(
    const std::array<CompanionWindowSnapshot, kWindowCount>& snapshots,
    TestRecorder& tests) {
    const auto width = snapshots.front().visible_rect.width();
    const auto height = snapshots.front().visible_rect.height();
    tests.require(width > 0 && height > 0, "companion visible frames are nonempty");
    for (const auto& snapshot : snapshots) {
        tests.require(snapshot.visible_rect.size() ==
                          snapshots.front().visible_rect.size(),
                      "companion visible sizes match");
        tests.require(snapshot.dpi == snapshots.front().dpi,
                      "companion initial DPI is common");
        tests.require(snapshot.monitor_device_name ==
                          snapshots.front().monitor_device_name,
                      "companion initial monitor is common");
    }

    const auto double_width = geometry::checked_add(width, width);
    const auto double_height = geometry::checked_add(height, height);
    tests.require(double_width.has_value() && double_height.has_value(),
                  "companion 2x2 extent is representable");
    const auto required_width = geometry::checked_add(*double_width, 120);
    const auto required_height = geometry::checked_add(*double_height, 60);
    tests.require(required_width.has_value() && required_height.has_value(),
                  "companion scripted extent is representable");
    const auto& work = snapshots.front().monitor_work_area;
    tests.require(work.width() >= *required_width &&
                      work.height() >= *required_height,
                  "companion 2x2 plus script fits work area");
    const auto spare_width = geometry::checked_difference(work.width(),
                                                           *required_width);
    const auto spare_height = geometry::checked_difference(work.height(),
                                                            *required_height);
    tests.require(spare_width.has_value() && spare_height.has_value(),
                  "companion layout centering is representable");
    const auto left = checked_sum(work.left(), *spare_width / 2);
    const auto top = checked_sum(work.top(), *spare_height / 2);
    const auto right_column = checked_sum(left, width);
    const auto bottom_row = checked_sum(top, height);

    const std::array<geometry::Point, kWindowCount> origins{
        geometry::Point{left, top},
        geometry::Point{right_column, top},
        geometry::Point{left, bottom_row},
        geometry::Point{right_column, bottom_row},
    };
    std::array<geometry::Rect, kWindowCount> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = {origins[index].x,
                         origins[index].y,
                         checked_sum(origins[index].x, width),
                         checked_sum(origins[index].y, height)};
    }
    return result;
}

void emit_operation(const std::string_view phase,
                    const CompanionOperationResult& result) {
    std::cout << "{\"schema_version\":1,\"record_kind\":\"operation\""
              << ",\"phase\":" << json_quote(phase)
              << ",\"operation_batch_id\":" << result.operation_batch_id
              << ",\"status\":" << json_quote(status_name(result.status))
              << ",\"stage\":" << json_quote(stage_name(result.stage))
              << ",\"requested_count\":" << result.requested_count
              << ",\"deferred_count\":" << result.deferred_count
              << ",\"verified_count\":" << result.verified_count
              << ",\"native_apply_attempted\":"
              << (result.native_apply_attempted ? "true" : "false")
              << ",\"native_outcome_known\":"
              << (result.native_outcome_known ? "true" : "false")
              << ",\"recaptured_after_native_failure\":"
              << (result.recaptured_after_native_failure ? "true" : "false")
              << "}\n";
    for (const auto& receipt : result.windows) {
        std::cout << "{\"schema_version\":1,\"record_kind\":\"operation_window\""
                  << ",\"phase\":" << json_quote(phase)
                  << ",\"operation_batch_id\":" << result.operation_batch_id
                  << ",\"window\":"
                  << json_quote(logical_name(receipt.token.logical_id()))
                  << ",\"generation\":" << receipt.token.generation()
                  << ",\"target_pid\":" << receipt.target_process_id
                  << ",\"target_tid\":" << receipt.target_thread_id
                  << ",\"requested_visible\":";
        append_rect(std::cout, receipt.requested_visible_rect);
        std::cout << ",\"requested_positioning\":";
        if (receipt.requested_positioning_rect.has_value()) {
            append_rect(std::cout, *receipt.requested_positioning_rect);
        } else {
            std::cout << "null";
        }
        std::cout << ",\"actual_visible\":";
        if (receipt.actual.has_value()) {
            append_rect(std::cout, receipt.actual->visible_rect);
        } else {
            std::cout << "null";
        }
        std::cout << ",\"actual_positioning\":";
        if (receipt.actual.has_value()) {
            append_rect(std::cout, receipt.actual->positioning_rect);
        } else {
            std::cout << "null";
        }
        std::cout << ",\"actual_dpi\":";
        if (receipt.actual.has_value()) {
            std::cout << receipt.actual->dpi;
        } else {
            std::cout << "null";
        }
        std::cout << ",\"actual_monitor\":";
        if (receipt.actual.has_value()) {
            std::cout << json_quote(
                narrow_ascii(receipt.actual->monitor_device_name));
        } else {
            std::cout << "null";
        }
        std::cout << ",\"visible_target_verified\":"
                  << (receipt.visible_target_verified ? "true" : "false")
                  << ",\"positioning_target_verified\":"
                  << (receipt.positioning_target_verified ? "true" : "false")
                  << ",\"size_preserved\":"
                  << (receipt.size_preserved ? "true" : "false")
                  << ",\"dpi_and_monitor_stable\":"
                  << (receipt.dpi_and_monitor_stable ? "true" : "false")
                  << "}\n";
    }
    std::cout.flush();
}

void emit_launch(const companion::CompanionLaunchFacts& facts,
                 const std::string_view session_name) {
    std::cout << "{\"schema_version\":1,\"record_kind\":\"companion_launch\""
              << ",\"session\":" << json_quote(session_name)
              << ",\"controller_pid\":" << facts.controller_process_id
              << ",\"controller_tid\":" << facts.controller_thread_id
              << ",\"target_pid\":" << facts.target_process_id
              << ",\"target_ui_tid\":" << facts.target_ui_thread_id
              << ",\"session_high\":" << facts.session_authority.high
              << ",\"session_low\":" << facts.session_authority.low
              << ",\"controller_integrity\":"
              << facts.controller_security.integrity_rid
              << ",\"target_integrity\":" << facts.target_security.integrity_rid
              << ",\"ui_access\":"
              << (facts.target_security.ui_access ? "true" : "false")
              << ",\"app_container\":"
              << (facts.target_security.app_container ? "true" : "false")
              << "}\n";
}

void emit_evidence(const protocol::EvidencePayload& evidence,
                   TestRecorder& tests) {
    tests.check(evidence.overflowed == 0U && evidence.dropped_record_count == 0U,
                "target evidence queue has no overflow or drop");
    std::array<std::uint64_t, 5U> message_counts{};
    bool identities_valid = true;
    bool sequence_contiguous = true;
    bool registration_facts_valid = true;
    bool operation_step_observed = false;
    for (std::size_t index = 0; index < evidence.record_count; ++index) {
        const auto& record = evidence.records[index];
        if (index != 0U) {
            sequence_contiguous = sequence_contiguous &&
                                  record.sequence ==
                                      evidence.records[index - 1U].sequence + 1U;
        }
        const auto message_index = static_cast<std::size_t>(record.message) - 1U;
        if (message_index < message_counts.size()) {
            ++message_counts[message_index];
        }
        identities_valid = identities_valid &&
                           record.target_process_id == evidence.target_process_id &&
                           record.target_ui_thread_id == evidence.target_ui_thread_id &&
                           protocol::is_valid_logical_window_id(
                               static_cast<std::uint32_t>(record.logical_id));
        registration_facts_valid = registration_facts_valid &&
                                   record.generation ==
                                       protocol::kInitialWindowGeneration &&
                                   record.native_window != 0U;
        operation_step_observed = operation_step_observed || record.step_id != 0U;
        std::cout << "{\"schema_version\":1,\"record_kind\":\"target_message\""
                  << ",\"sequence\":" << record.sequence
                  << ",\"step_id\":" << record.step_id
                  << ",\"window\":"
                  << json_quote(logical_name(record.logical_id))
                  << ",\"message\":"
                  << static_cast<std::uint32_t>(record.message)
                  << ",\"target_pid\":" << record.target_process_id
                  << ",\"target_tid\":" << record.target_ui_thread_id
                  << ",\"proposed_x\":" << record.proposed_x
                  << ",\"effective_x\":" << record.effective_x
                  << ",\"test_offset_x\":" << record.applied_test_offset_x
                  << "}\n";
    }
    tests.check(identities_valid, "target evidence PID/TID/logical identities match");
    tests.check(sequence_contiguous,
                "target evidence sequence has no duplicate or missing record");
    tests.check(registration_facts_valid,
                "target evidence generation and native registration facts are valid");
    tests.check(operation_step_observed,
                "target evidence contains operation-correlated step IDs");
    tests.check(message_counts[0] > 0U && message_counts[1] > 0U &&
                    message_counts[2] > 0U,
                "target observed CHANGING CHANGED and MOVE");
    std::cout << "{\"schema_version\":1,\"record_kind\":\"target_message_counts\""
              << ",\"WM_WINDOWPOSCHANGING\":" << message_counts[0]
              << ",\"WM_WINDOWPOSCHANGED\":" << message_counts[1]
              << ",\"WM_MOVE\":" << message_counts[2]
              << ",\"WM_SIZE\":" << message_counts[3]
              << ",\"WM_NCDESTROY\":" << message_counts[4]
              << "}\n";
}

[[nodiscard]] int run_self_test(const std::chrono::seconds hold_duration) {
    TestRecorder tests;
    const auto target_path = target_executable_path();
    tests.require(std::filesystem::is_regular_file(target_path),
                  "companion target executable exists beside harness");

    auto launch1 = CompanionSession::launch(target_path);
    tests.require(launch1.succeeded(), "companion session 1 launch and handshake");
    auto session1 = std::move(launch1.session);
    emit_launch(session1->launch_facts(), "session_1");
    tests.require(session1->launch_facts().controller_process_id !=
                      session1->launch_facts().target_process_id &&
                      session1->launch_facts().controller_thread_id !=
                          session1->launch_facts().target_ui_thread_id,
                  "controller and target are distinct process/thread identities");
    tests.require(session1->launch_facts().controller_security ==
                      session1->launch_facts().target_security,
                  "controller and target security baselines match");

    CompanionWindowOperations operations1{*session1};
    const auto tokens1 = session1->tokens();
    tests.require(tokens1.size() == kWindowCount,
                  "session 1 issued exactly A/B/C/D tokens");
    auto startup = capture_all(operations1, tokens1, tests, "startup");
    const auto desired = desired_two_by_two(startup, tests);
    std::vector<CompanionWindowTranslationRequest> layout_requests;
    for (std::size_t index = 0; index < kWindowCount; ++index) {
        layout_requests.push_back(
            {token_for(tokens1, logical_id(index)), desired[index]});
    }
    auto layout = operations1.apply(layout_requests);
    emit_operation("startup_2x2_cross_process_defer", layout);
    tests.require(layout.succeeded() && layout.deferred_count == kWindowCount &&
                      layout.verified_count == kWindowCount,
                  "cross-process startup Defer batch verified 4/4");

    const auto initial = capture_all(operations1, tokens1, tests, "initial_2x2");
    const auto graph = build_graph(initial);
    tests.require(graph.relations().size() == 4U &&
                      graph.connected_component(model::WindowId{"A"}).size() ==
                          kWindowCount,
                  "companion visible geometry forms exact connected 2x2");
    movement::TranslationSession translation_session{graph, model::WindowId{"A"}};

    constexpr std::array<movement::TranslationDelta, 5U> deltas{
        movement::TranslationDelta{30, 20},
        movement::TranslationDelta{80, 50},
        movement::TranslationDelta{80, 50},
        movement::TranslationDelta{40, 10},
        movement::TranslationDelta{120, 60},
    };
    std::array<CompanionWindowSnapshot, kWindowCount> current = initial;
    for (std::size_t step = 0; step < deltas.size(); ++step) {
        const auto& delta = deltas[step];
        const std::string phase = "step_" + std::to_string(step + 1U) +
                                  "_dx_" + std::to_string(delta.dx) +
                                  "_dy_" + std::to_string(delta.dy);
        const auto leader_target = movement::translate_rect(
            initial[kLeaderIndex].visible_rect, delta);
        auto leader = operations1.apply_one(
            {token_for(tokens1, protocol::LogicalWindowId::A), leader_target});
        emit_operation(phase + "_leader_setwindowpos", leader);
        tests.require(leader.succeeded(), phase + " cross-process leader verified");

        const auto leader_actual = capture_required(
            operations1,
            token_for(tokens1, protocol::LogicalWindowId::A),
            tests,
            phase + " leader capture");
        const auto plan = translation_session.plan(leader_actual.visible_rect);
        tests.require(plan.has_value() && plan->size() == 3U,
                      phase + " R1-A follower plan");
        std::vector<CompanionWindowTranslationRequest> followers;
        for (const auto& planned : *plan) {
            const auto id_value = static_cast<protocol::LogicalWindowId>(
                static_cast<std::uint32_t>(planned.id.value().front() - 'A') + 1U);
            followers.push_back(
                {token_for(tokens1, id_value), planned.target_visible_rect});
        }
        auto follower_result = operations1.apply(followers);
        emit_operation(phase + "_followers_defer", follower_result);
        tests.require(follower_result.succeeded() &&
                          follower_result.deferred_count == 3U &&
                          follower_result.verified_count == 3U,
                      phase + " cross-process followers verified");
        current = capture_all(operations1, tokens1, tests, phase);
        for (std::size_t index = 0; index < kWindowCount; ++index) {
            tests.check(current[index].visible_rect == movement::translate_rect(
                            initial[index].visible_rect, delta),
                        phase + " visible " + logical_name(logical_id(index)));
            tests.check(current[index].positioning_rect == movement::translate_rect(
                            initial[index].positioning_rect, delta),
                        phase + " positioning " + logical_name(logical_id(index)));
        }
        const auto translated_graph = build_graph(current);
        tests.check(std::equal(graph.relations().begin(),
                               graph.relations().end(),
                               translated_graph.relations().begin(),
                               translated_graph.relations().end()),
                    phase + " topology preserved");
    }
    tests.check(current.front().visible_rect == movement::translate_rect(
                    initial.front().visible_rect, deltas.back()),
                "repeat/backtrack sequence has no cumulative drift");

    const auto before_uncooperative = current;
    constexpr movement::TranslationDelta uncooperative_delta{15, 8};
    std::vector<CompanionWindowTranslationRequest> uncooperative_requests;
    for (std::size_t index = 1U; index < kWindowCount; ++index) {
        uncooperative_requests.push_back(
            {token_for(tokens1, logical_id(index)),
             movement::translate_rect(before_uncooperative[index].visible_rect,
                                      uncooperative_delta)});
    }
    companion::CompanionApplyOptions uncooperative_options{
        protocol::logical_window_bit(protocol::LogicalWindowId::D), 5};
    auto uncooperative = operations1.apply(uncooperative_requests,
                                           uncooperative_options);
    emit_operation("uncooperative_D_plus_5", uncooperative);
    tests.check(uncooperative.status ==
                    CompanionOperationStatus::PostVerificationFailed &&
                    uncooperative.native_apply_attempted &&
                    uncooperative.native_outcome_known &&
                    uncooperative.windows.size() == 3U,
                "uncooperative D yields explicit post-verification mismatch");
    for (const auto& receipt : uncooperative.windows) {
        tests.require(receipt.actual.has_value(),
                      "uncooperative result recaptures every follower");
        if (receipt.token.logical_id() == protocol::LogicalWindowId::D) {
            tests.check(receipt.actual->visible_rect.left() ==
                            receipt.requested_visible_rect.left() + 5,
                        "uncooperative D actual X is requested plus five");
        } else {
            tests.check(receipt.actual->visible_rect ==
                            receipt.requested_visible_rect,
                        "cooperative follower remains at requested geometry");
        }
    }

    std::vector<CompanionWindowTranslationRequest> recovery_requests;
    for (std::size_t index = 1U; index < kWindowCount; ++index) {
        recovery_requests.push_back(
            {token_for(tokens1, logical_id(index)),
             before_uncooperative[index].visible_rect});
    }
    auto recovery = operations1.apply(recovery_requests);
    emit_operation("uncooperative_cleanup_not_rollback", recovery);
    tests.require(recovery.succeeded(),
                  "separate cleanup operation restores fixture layout");

    if (hold_duration.count() > 0) {
        std::cout << "{\"schema_version\":1,\"record_kind\":\"hold\""
                  << ",\"seconds\":" << hold_duration.count() << "}\n";
        std::cout.flush();
        std::this_thread::sleep_for(hold_duration);
    }

    const auto stale_c = token_for(tokens1, protocol::LogicalWindowId::C);
    const auto destroyed_c_before = capture_required(
        operations1, stale_c, tests, "C before destroy");
    const auto destroy = session1->destroy_window(stale_c);
    tests.require(destroy.succeeded(), "target UI thread destroys C via IPC");
    tests.check(!session1->contains(stale_c),
                "destroyed C token is immediately stale");
    const auto before_b = capture_required(
        operations1,
        token_for(tokens1, protocol::LogicalWindowId::B),
        tests,
        "B before invalid-member batch");
    const auto before_d = capture_required(
        operations1,
        token_for(tokens1, protocol::LogicalWindowId::D),
        tests,
        "D before invalid-member batch");
    constexpr movement::TranslationDelta invalid_delta{7, 5};
    std::vector<CompanionWindowTranslationRequest> invalid_requests{
        {token_for(tokens1, protocol::LogicalWindowId::B),
         movement::translate_rect(before_b.visible_rect, invalid_delta)},
        {stale_c,
         movement::translate_rect(destroyed_c_before.visible_rect, invalid_delta)},
        {token_for(tokens1, protocol::LogicalWindowId::D),
         movement::translate_rect(before_d.visible_rect, invalid_delta)},
    };
    auto invalid = operations1.apply(invalid_requests);
    emit_operation("destroyed_member_preflight", invalid);
    tests.check(invalid.status == CompanionOperationStatus::StaleToken &&
                    invalid.stage == CompanionOperationStage::Preflight &&
                    !invalid.native_apply_attempted,
                "B stale-C D batch fails before native apply");
    tests.check(capture_required(operations1,
                                 token_for(tokens1, protocol::LogicalWindowId::B),
                                 tests,
                                 "B after invalid-member batch") == before_b,
                "B unchanged after invalid-member preflight");
    tests.check(capture_required(operations1,
                                 token_for(tokens1, protocol::LogicalWindowId::D),
                                 tests,
                                 "D after invalid-member batch") == before_d,
                "D unchanged after invalid-member preflight");

    auto evidence = session1->query_evidence();
    tests.require(evidence.succeeded(), "target-side evidence query succeeds");
    emit_evidence(*evidence.evidence, tests);

    const auto session1_authority = session1->launch_facts().session_authority;
    auto shutdown1 = session1->shutdown();
    tests.require(shutdown1.succeeded() &&
                      shutdown1.graceful_request_acknowledged &&
                      !shutdown1.terminate_fallback_used,
                  "session 1 exits gracefully without forced cleanup");
    for (const auto& token : tokens1) {
        tests.check(!session1->contains(token),
                    "session 1 exit retires token " +
                        logical_name(token.logical_id()));
    }
    auto after_exit = operations1.capture(
        token_for(tokens1, protocol::LogicalWindowId::A));
    tests.check(after_exit.status == CompanionOperationStatus::SessionExited ||
                    after_exit.status == CompanionOperationStatus::StaleToken,
                "session 1 operation fails after process exit");

    auto launch2 = CompanionSession::launch(target_path);
    tests.require(launch2.succeeded(), "companion session 2 launch and handshake");
    auto session2 = std::move(launch2.session);
    emit_launch(session2->launch_facts(), "session_2");
    tests.check(session2->launch_facts().session_authority != session1_authority,
                "session 2 receives a distinct launch authority");
    const auto tokens2 = session2->tokens();
    tests.require(tokens2.size() == kWindowCount,
                  "session 2 reissues logical A/B/C/D");
    CompanionWindowOperations operations2{*session2};
    auto old_token_in_new_session = operations2.apply_one(
        {token_for(tokens1, protocol::LogicalWindowId::A),
         initial[kLeaderIndex].visible_rect});
    emit_operation("cross_session_old_token", old_token_in_new_session);
    tests.check(old_token_in_new_session.status ==
                    CompanionOperationStatus::StaleToken &&
                    !old_token_in_new_session.native_apply_attempted,
                "session 2 rejects session 1 token before native apply");
    static_cast<void>(capture_required(
        operations2,
        token_for(tokens2, protocol::LogicalWindowId::A),
        tests,
        "session 2 local A token resolves"));
    auto shutdown2 = session2->shutdown();
    tests.require(shutdown2.succeeded() &&
                      !shutdown2.terminate_fallback_used,
                  "session 2 exits gracefully");

    tests.not_tested("numeric PID/HWND reuse",
                     "real numeric reuse cannot be forced reliably; process-handle and cross-session authority are tested");
    tests.not_tested("destroy exactly during EndDeferWindowPos",
                     "unsafe nondeterministic sleep races are not an R1-C1 gate");
    tests.not_tested("elevated, UIAccess, AppContainer, mixed-DPI, multi-monitor",
                     "R1-C1 baseline is same-integrity single-monitor PMv2 only");

    std::cout << "{\"schema_version\":1,\"record_kind\":\"self_test_summary\""
              << ",\"result\":"
              << json_quote(tests.failures() == 0 ? "PASS" : "FAIL")
              << ",\"failures\":" << tests.failures()
              << ",\"third_party_window_control\":false"
              << ",\"global_input_control\":false"
              << ",\"r1c2_started\":false}\n";
    return tests.failures() == 0 ? 0 : 1;
}

struct CommandLineOptions {
    bool self_test{};
    std::chrono::seconds hold_duration{};
};

[[nodiscard]] std::optional<std::uint32_t> parse_positive_seconds(
    const std::string_view value) {
    std::uint32_t seconds{};
    const auto [position, error] = std::from_chars(
        value.data(), value.data() + value.size(), seconds);
    if (error != std::errc{} || position != value.data() + value.size() ||
        seconds == 0U || seconds > 60U) {
        return std::nullopt;
    }
    return seconds;
}

[[nodiscard]] std::optional<CommandLineOptions> parse_arguments(
    const int argc,
    char* argv[]) {
    CommandLineOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--self-test") {
            options.self_test = true;
        } else if (argument == "--hold-seconds" && index + 1 < argc) {
            const auto seconds = parse_positive_seconds(argv[++index]);
            if (!seconds.has_value()) {
                return std::nullopt;
            }
            options.hold_duration = std::chrono::seconds{*seconds};
        } else {
            return std::nullopt;
        }
    }
    return options.self_test ? std::optional{options} : std::nullopt;
}

void print_usage(std::ostream& output) {
    output << "Usage:\n"
              "  panebind-companion-harness --self-test [--hold-seconds N]\n"
              "  panebind-companion-harness --help\n";
}

} // namespace

int main(const int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    static_cast<void>(SetConsoleOutputCP(CP_UTF8));
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        print_usage(std::cout);
        return 0;
    }
    const auto options = parse_arguments(argc, argv);
    if (!options.has_value()) {
        print_usage(std::cerr);
        return 2;
    }
    try {
        return run_self_test(options->hold_duration);
    } catch (const std::exception& error) {
        std::cout << "{\"schema_version\":1,\"record_kind\":\"fatal\""
                  << ",\"error\":" << json_quote(error.what()) << "}\n";
        return 1;
    }
}
