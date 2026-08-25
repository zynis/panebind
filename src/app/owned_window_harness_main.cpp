#include "core/geometry/checked_arithmetic.h"
#include "core/movement/move_plan.h"
#include "core/movement/translation.h"
#include "core/topology/window_adjacency.h"
#include "platform/windows/operations/owned_window_operations.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace geometry = panebind::core::geometry;
namespace movement = panebind::core::movement;
namespace model = panebind::core::model;
namespace topology = panebind::core::topology;
namespace operations = panebind::platform::windows::operations;

using operations::OperationResult;
using operations::OperationStage;
using operations::OperationStatus;
using operations::OwnedWindowOperations;
using operations::OwnedWindowRegistry;
using operations::OwnedWindowSnapshot;
using operations::OwnedWindowToken;
using operations::WindowTranslationRequest;

constexpr std::size_t kWindowCount = 4U;
constexpr std::size_t kLeaderIndex = 0U;
constexpr std::size_t kDestroyedFollowerIndex = 2U;
constexpr int kInitialPositioningWidth = 300;
constexpr int kInitialPositioningHeight = 210;
constexpr auto kObserverSettleDelay = std::chrono::milliseconds{150};

std::uint64_t next_message_sequence = 1U;

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
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
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
                constexpr char hexadecimal[] = "0123456789abcdef";
                result += "\\u00";
                result.push_back(hexadecimal[(character >> 4U) & 0x0fU]);
                result.push_back(hexadecimal[character & 0x0fU]);
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
        result.push_back(character >= 0 && character <= 0x7f
                             ? static_cast<char>(character)
                             : '?');
    }
    return result;
}

void append_rect(std::ostream& output, const geometry::Rect& rect) {
    output << "{\"left\":" << rect.left() << ",\"top\":" << rect.top()
           << ",\"right\":" << rect.right() << ",\"bottom\":"
           << rect.bottom() << '}';
}

[[nodiscard]] std::string_view stage_name(const OperationStage stage) noexcept {
    switch (stage) {
    case OperationStage::None:
        return "none";
    case OperationStage::Registration:
        return "registration";
    case OperationStage::Preflight:
        return "preflight";
    case OperationStage::SingleApply:
        return "single_apply";
    case OperationStage::BeginBatch:
        return "begin_batch";
    case OperationStage::DeferBatch:
        return "defer_batch";
    case OperationStage::EndBatch:
        return "end_batch";
    case OperationStage::PostVerification:
        return "post_verification";
    }
    return "unknown";
}

[[nodiscard]] std::string_view status_name(const OperationStatus status) noexcept {
    switch (status) {
    case OperationStatus::Succeeded:
        return "succeeded";
    case OperationStatus::InvalidRequest:
        return "invalid_request";
    case OperationStatus::DuplicateToken:
        return "duplicate_token";
    case OperationStatus::StaleToken:
        return "stale_token";
    case OperationStatus::InvalidWindow:
        return "invalid_window";
    case OperationStatus::WrongProcess:
        return "wrong_process";
    case OperationStatus::WrongThread:
        return "wrong_thread";
    case OperationStatus::WrongWindowClass:
        return "wrong_window_class";
    case OperationStatus::NotIndependentTopLevel:
        return "not_independent_top_level";
    case OperationStatus::MarkerConflict:
        return "marker_conflict";
    case OperationStatus::MarkerFailure:
        return "marker_failure";
    case OperationStatus::GenerationExhausted:
        return "generation_exhausted";
    case OperationStatus::GeometryCaptureFailed:
        return "geometry_capture_failed";
    case OperationStatus::DpiContextMismatch:
        return "dpi_context_mismatch";
    case OperationStatus::EmptyGeometry:
        return "empty_geometry";
    case OperationStatus::ResizeRejected:
        return "resize_rejected";
    case OperationStatus::ArithmeticOverflow:
        return "arithmetic_overflow";
    case OperationStatus::NativeCoordinateOutOfRange:
        return "native_coordinate_out_of_range";
    case OperationStatus::SingleApplyFailed:
        return "single_apply_failed";
    case OperationStatus::BeginBatchFailed:
        return "begin_batch_failed";
    case OperationStatus::DeferBatchFailed:
        return "defer_batch_failed";
    case OperationStatus::EndBatchFailed:
        return "end_batch_failed";
    case OperationStatus::PostVerificationFailed:
        return "post_verification_failed";
    case OperationStatus::WindowInvalidatedDuringApply:
        return "window_invalidated_during_apply";
    }
    return "unknown";
}

[[nodiscard]] std::string_view diagnostic_domain_name(
    const operations::DiagnosticDomain domain) noexcept {
    switch (domain) {
    case operations::DiagnosticDomain::Win32:
        return "win32";
    case operations::DiagnosticDomain::HResult:
        return "hresult";
    case operations::DiagnosticDomain::Adapter:
        return "adapter";
    }
    return "unknown";
}

struct MessageCounts {
    std::uint64_t window_pos_changing{};
    std::uint64_t window_pos_changed{};
    std::uint64_t move{};
    std::uint64_t size{};
    std::uint64_t nonclient_destroy{};

    void reset() noexcept { *this = {}; }
};

struct HarnessWindowState {
    wchar_t label{};
    HWND window{};
    OwnedWindowRegistry* registry{};
    std::optional<OwnedWindowToken> token;
    std::string phase{"creation"};
    MessageCounts messages;
};

[[nodiscard]] std::string label_string(const wchar_t label) {
    return std::string(1U, static_cast<char>(label));
}

void emit_window_message(HarnessWindowState& state,
                         const std::string_view message_name,
                         const WINDOWPOS* position = nullptr,
                         const std::optional<std::pair<int, int>> payload =
                             std::nullopt) {
    std::cout << "{\"schema_version\":1,\"record_kind\":\"harness_message\""
              << ",\"message_sequence\":" << next_message_sequence++
              << ",\"tick_ms\":" << GetTickCount64()
              << ",\"window\":" << json_quote(label_string(state.label))
              << ",\"phase\":" << json_quote(state.phase)
              << ",\"message\":" << json_quote(message_name);
    if (state.token.has_value()) {
        std::cout << ",\"token_id\":" << state.token->logical_id()
                  << ",\"generation\":" << state.token->generation();
    } else {
        std::cout << ",\"token_id\":null,\"generation\":null";
    }
    if (position != nullptr) {
        std::cout << ",\"windowpos\":{\"x\":" << position->x
                  << ",\"y\":" << position->y << ",\"cx\":"
                  << position->cx << ",\"cy\":" << position->cy
                  << ",\"flags\":" << position->flags << '}';
    }
    if (payload.has_value()) {
        std::cout << ",\"payload_first\":" << payload->first
                  << ",\"payload_second\":" << payload->second;
    }
    std::cout << "}\n";
    std::cout.flush();
}

LRESULT CALLBACK harness_window_proc(const HWND window,
                                     const UINT message,
                                     const WPARAM wparam,
                                     const LPARAM lparam) {
    auto* state = reinterpret_cast<HarnessWindowState*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        state = static_cast<HarnessWindowState*>(creation->lpCreateParams);
        if (state == nullptr) {
            return FALSE;
        }
        state->window = window;
        SetLastError(ERROR_SUCCESS);
        const LONG_PTR previous = SetWindowLongPtrW(
            window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
            state->window = nullptr;
            return FALSE;
        }
    }

    if (state != nullptr) {
        switch (message) {
        case WM_WINDOWPOSCHANGING:
            ++state->messages.window_pos_changing;
            emit_window_message(*state,
                                "WM_WINDOWPOSCHANGING",
                                reinterpret_cast<const WINDOWPOS*>(lparam));
            break;
        case WM_WINDOWPOSCHANGED:
            ++state->messages.window_pos_changed;
            // Record at message entry. DefWindowProc may synchronously emit
            // WM_MOVE/WM_SIZE from this message.
            emit_window_message(*state,
                                "WM_WINDOWPOSCHANGED",
                                reinterpret_cast<const WINDOWPOS*>(lparam));
            break;
        case WM_MOVE:
            ++state->messages.move;
            emit_window_message(
                *state,
                "WM_MOVE",
                nullptr,
                std::pair{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
            break;
        case WM_SIZE:
            ++state->messages.size;
            emit_window_message(
                *state,
                "WM_SIZE",
                nullptr,
                std::pair{static_cast<int>(LOWORD(lparam)),
                          static_cast<int>(HIWORD(lparam))});
            break;
        case WM_NCDESTROY:
            ++state->messages.nonclient_destroy;
            emit_window_message(*state, "WM_NCDESTROY");
            if (state->registry != nullptr) {
                static_cast<void>(state->registry->invalidate_window(window));
            }
            state->window = nullptr;
            break;
        default:
            break;
        }
    }

    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC device = BeginPaint(window, &paint);
        if (device != nullptr && state != nullptr) {
            RECT client{};
            static_cast<void>(GetClientRect(window, &client));
            static_cast<void>(SetBkMode(device, TRANSPARENT));
            std::wstring text = L"PaneBind R1-B\n";
            text.push_back(state->label);
            static_cast<void>(DrawTextW(device,
                                        text.c_str(),
                                        static_cast<int>(text.size()),
                                        &client,
                                        DT_CENTER | DT_VCENTER | DT_WORDBREAK));
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_CLOSE:
        static_cast<void>(DestroyWindow(window));
        return 0;
    case WM_NCDESTROY: {
        const LRESULT result = DefWindowProcW(window, message, wparam, lparam);
        SetLastError(ERROR_SUCCESS);
        static_cast<void>(SetWindowLongPtrW(window, GWLP_USERDATA, 0));
        return result;
    }
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

void pump_messages_for(const std::chrono::milliseconds duration) {
    const auto deadline = std::chrono::steady_clock::now() + duration;
    for (;;) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE) {
            if (message.message != WM_QUIT) {
                static_cast<void>(TranslateMessage(&message));
                static_cast<void>(DispatchMessageW(&message));
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return;
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - now);
        const auto bounded = std::clamp<std::int64_t>(
            remaining.count(), 1, std::numeric_limits<DWORD>::max());
        static_cast<void>(MsgWaitForMultipleObjectsEx(
            0,
            nullptr,
            static_cast<DWORD>(bounded),
            QS_ALLINPUT,
            MWMO_INPUTAVAILABLE));
    }
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
        std::cout.flush();
    }

    [[nodiscard]] int failures() const noexcept { return failures_; }

private:
    int failures_{};
};

class HarnessEnvironment final {
public:
    HarnessEnvironment()
        : instance_(GetModuleHandleW(nullptr)), operations_(registry_) {
        if (instance_ == nullptr) {
            throw std::runtime_error{"GetModuleHandleW failed"};
        }
    }

    ~HarnessEnvironment() {
        destroy_all();
        if (class_registered_) {
            static_cast<void>(UnregisterClassW(operations::kOwnedWindowClassName,
                                               instance_));
        }
    }

    HarnessEnvironment(const HarnessEnvironment&) = delete;
    HarnessEnvironment& operator=(const HarnessEnvironment&) = delete;

    void create_windows() {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = harness_window_proc;
        window_class.hInstance = instance_;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        window_class.lpszClassName = operations::kOwnedWindowClassName;
        if (RegisterClassExW(&window_class) == 0U) {
            throw std::runtime_error{"RegisterClassExW failed"};
        }
        class_registered_ = true;

        constexpr std::array<wchar_t, kWindowCount> labels{L'A', L'B', L'C', L'D'};
        for (std::size_t index = 0; index < windows_.size(); ++index) {
            auto state = std::make_unique<HarnessWindowState>();
            state->label = labels[index];
            state->registry = &registry_;
            std::wstring title = L"PaneBind R1-B - ";
            title.push_back(labels[index]);

            const int offset = static_cast<int>(index) * 24;
            HWND window = CreateWindowExW(
                WS_EX_NOACTIVATE,
                operations::kOwnedWindowClassName,
                title.c_str(),
                WS_OVERLAPPEDWINDOW,
                40 + offset,
                40 + offset,
                kInitialPositioningWidth,
                kInitialPositioningHeight,
                nullptr,
                nullptr,
                instance_,
                state.get());
            if (window == nullptr) {
                throw std::runtime_error{"CreateWindowExW failed"};
            }

            auto registration = registry_.register_window(window);
            if (!registration.succeeded()) {
                static_cast<void>(DestroyWindow(window));
                throw std::runtime_error{"owned-window registration failed"};
            }
            state->token = *registration.token;
            windows_[index] = std::move(state);

            static_cast<void>(ShowWindow(window, SW_SHOWNOACTIVATE));
            static_cast<void>(UpdateWindow(window));
        }

        pump_messages_for(std::chrono::milliseconds{250});
        static_cast<void>(DwmFlush());
    }

    [[nodiscard]] OwnedWindowRegistry& registry() noexcept { return registry_; }
    [[nodiscard]] OwnedWindowOperations& operations_adapter() noexcept {
        return operations_;
    }

    [[nodiscard]] const OwnedWindowToken& token(const std::size_t index) const {
        return *windows_.at(index)->token;
    }

    [[nodiscard]] wchar_t label(const std::size_t index) const {
        return windows_.at(index)->label;
    }

    [[nodiscard]] std::string label_for(const OwnedWindowToken& token) const {
        for (const auto& window : windows_) {
            if (window != nullptr && window->token.has_value() &&
                *window->token == token) {
                return label_string(window->label);
            }
        }
        return "unknown";
    }

    void set_phase(const std::span<const std::size_t> indices,
                   const std::string_view phase) {
        for (const std::size_t index : indices) {
            windows_.at(index)->phase = std::string{phase};
        }
    }

    void reset_message_counts() noexcept {
        for (const auto& window : windows_) {
            if (window != nullptr) {
                window->messages.reset();
            }
        }
    }

    [[nodiscard]] const MessageCounts& message_counts(
        const std::size_t index) const {
        return windows_.at(index)->messages;
    }

    [[nodiscard]] bool destroy_window(const std::size_t index) {
        auto& state = *windows_.at(index);
        if (state.window == nullptr) {
            return true;
        }
        return DestroyWindow(state.window) != FALSE;
    }

    void destroy_all() noexcept {
        for (auto& state : windows_) {
            if (state != nullptr && state->window != nullptr) {
                static_cast<void>(DestroyWindow(state->window));
            }
        }
    }

private:
    HINSTANCE instance_{};
    OwnedWindowRegistry registry_;
    OwnedWindowOperations operations_;
    std::array<std::unique_ptr<HarnessWindowState>, kWindowCount> windows_;
    bool class_registered_{};
};

void emit_operation_result(const HarnessEnvironment& environment,
                           const std::string_view phase,
                           const OperationResult& result) {
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
              << (result.recaptured_after_native_failure ? "true" : "false");
    if (result.diagnostic.has_value()) {
        std::cout << ",\"diagnostic\":{\"domain\":"
                  << json_quote(diagnostic_domain_name(
                         result.diagnostic->domain))
                  << ",\"api\":" << json_quote(result.diagnostic->api)
                  << ",\"detail\":"
                  << json_quote(result.diagnostic->detail) << ",\"code\":"
                  << result.diagnostic->code << '}';
    } else {
        std::cout << ",\"diagnostic\":null";
    }
    std::cout << "}\n";

    for (const auto& receipt : result.windows) {
        std::cout << "{\"schema_version\":1,\"record_kind\":\"operation_window\""
                  << ",\"phase\":" << json_quote(phase)
                  << ",\"operation_batch_id\":" << result.operation_batch_id
                  << ",\"window\":"
                  << json_quote(environment.label_for(receipt.token))
                  << ",\"token_id\":" << receipt.token.logical_id()
                  << ",\"generation\":" << receipt.token.generation()
                  << ",\"requested_visible\":";
        append_rect(std::cout, receipt.requested_visible_rect);
        std::cout << ",\"requested_positioning\":";
        if (receipt.requested_positioning_rect.has_value()) {
            append_rect(std::cout, *receipt.requested_positioning_rect);
        } else {
            std::cout << "null";
        }
        std::cout << ",\"before_visible\":";
        if (receipt.before.has_value()) {
            append_rect(std::cout, receipt.before->visible_rect);
        } else {
            std::cout << "null";
        }
        std::cout << ",\"before_positioning\":";
        if (receipt.before.has_value()) {
            append_rect(std::cout, receipt.before->positioning_rect);
        } else {
            std::cout << "null";
        }
        std::cout << ",\"before_dpi\":";
        if (receipt.before.has_value()) {
            std::cout << receipt.before->dpi;
        } else {
            std::cout << "null";
        }
        std::cout << ",\"before_monitor\":";
        if (receipt.before.has_value()) {
            std::cout << json_quote(
                narrow_ascii(receipt.before->monitor_device_name));
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

[[nodiscard]] OwnedWindowSnapshot capture_required(
    OwnedWindowOperations& adapter,
    const OwnedWindowToken& token,
    TestRecorder& tests,
    const std::string_view name) {
    auto result = adapter.capture(token);
    tests.require(result.succeeded(), name);
    return *result.snapshot;
}

[[nodiscard]] std::array<OwnedWindowSnapshot, kWindowCount> capture_all(
    HarnessEnvironment& environment,
    TestRecorder& tests,
    const std::string_view phase) {
    std::array<OwnedWindowSnapshot, kWindowCount> snapshots{};
    for (std::size_t index = 0; index < snapshots.size(); ++index) {
        snapshots[index] = capture_required(
            environment.operations_adapter(),
            environment.token(index),
            tests,
            std::string{phase} + " capture " + label_string(environment.label(index)));
    }
    return snapshots;
}

[[nodiscard]] topology::WindowAdjacencyGraph build_graph(
    const std::array<OwnedWindowSnapshot, kWindowCount>& snapshots) {
    std::vector<topology::WindowGeometry> windows;
    windows.reserve(snapshots.size());
    for (std::size_t index = 0; index < snapshots.size(); ++index) {
        windows.push_back({model::WindowId{label_string(
                               static_cast<wchar_t>(L'A' + index))},
                           snapshots[index].visible_rect});
    }
    return topology::WindowAdjacencyGraph::build(windows,
                                                  {.edge_tolerance = 0});
}

[[nodiscard]] geometry::Coordinate checked_sum(
    const geometry::Coordinate value,
    const geometry::Distance delta) {
    const auto result = geometry::checked_add(value, delta);
    if (!result.has_value()) {
        throw std::overflow_error{"harness layout coordinate overflow"};
    }
    return *result;
}

[[nodiscard]] std::array<geometry::Rect, kWindowCount> desired_two_by_two(
    const std::array<OwnedWindowSnapshot, kWindowCount>& snapshots,
    TestRecorder& tests) {
    const auto width = snapshots.front().visible_rect.width();
    const auto height = snapshots.front().visible_rect.height();
    tests.require(width > 0 && height > 0, "initial visible size is nonempty");
    for (const auto& snapshot : snapshots) {
        tests.require(snapshot.visible_rect.size() ==
                          snapshots.front().visible_rect.size(),
                      "four visible frame sizes match");
        tests.require(snapshot.dpi == snapshots.front().dpi,
                      "initial DPI is common");
        tests.require(snapshot.monitor_device_name ==
                          snapshots.front().monitor_device_name,
                      "initial monitor is common");
    }

    const auto double_width = geometry::checked_add(width, width);
    const auto double_height = geometry::checked_add(height, height);
    tests.require(double_width.has_value() && double_height.has_value(),
                  "2x2 extent arithmetic is representable");
    const auto layout_width = geometry::checked_add(*double_width, 120);
    const auto layout_height = geometry::checked_add(*double_height, 60);
    tests.require(layout_width.has_value() && layout_height.has_value(),
                  "translated 2x2 extent arithmetic is representable");

    const auto& work = snapshots.front().monitor_work_area;
    tests.require(work.width() >= *layout_width && work.height() >= *layout_height,
                  "2x2 plus scripted translation fits monitor work area");
    const auto spare_width = geometry::checked_difference(work.width(), *layout_width);
    const auto spare_height = geometry::checked_difference(work.height(), *layout_height);
    tests.require(spare_width.has_value() && spare_height.has_value(),
                  "work-area centering arithmetic is representable");

    const auto origin_x = checked_sum(work.left(), *spare_width / 2);
    const auto origin_y = checked_sum(work.top(), *spare_height / 2);
    const auto right_column_x = checked_sum(origin_x, width);
    const auto bottom_row_y = checked_sum(origin_y, height);

    std::array<geometry::Rect, kWindowCount> desired{};
    const std::array<geometry::Point, kWindowCount> origins{
        geometry::Point{origin_x, origin_y},
        geometry::Point{right_column_x, origin_y},
        geometry::Point{origin_x, bottom_row_y},
        geometry::Point{right_column_x, bottom_row_y},
    };
    for (std::size_t index = 0; index < desired.size(); ++index) {
        desired[index] = {origins[index].x,
                          origins[index].y,
                          checked_sum(origins[index].x, width),
                          checked_sum(origins[index].y, height)};
    }
    return desired;
}

void emit_runtime_environment(const OwnedWindowSnapshot& snapshot) {
    std::cout << "{\"schema_version\":1,\"record_kind\":\"runtime_environment\""
              << ",\"monitor\":"
              << json_quote(narrow_ascii(snapshot.monitor_device_name))
              << ",\"dpi\":" << snapshot.dpi << ",\"monitor_rect\":";
    append_rect(std::cout, snapshot.monitor_rect);
    std::cout << ",\"work_area\":";
    append_rect(std::cout, snapshot.monitor_work_area);
    std::cout << ",\"dpi_awareness\":\"PerMonitorV2\"}\n";
    std::cout.flush();
}

void emit_message_counts(const HarnessEnvironment& environment) {
    for (std::size_t index = 0; index < kWindowCount; ++index) {
        const auto& counts = environment.message_counts(index);
        std::cout << "{\"schema_version\":1,\"record_kind\":\"message_counts\""
                  << ",\"window\":"
                  << json_quote(label_string(environment.label(index)))
                  << ",\"WM_WINDOWPOSCHANGING\":"
                  << counts.window_pos_changing
                  << ",\"WM_WINDOWPOSCHANGED\":" << counts.window_pos_changed
                  << ",\"WM_MOVE\":" << counts.move
                  << ",\"WM_SIZE\":" << counts.size
                  << ",\"WM_NCDESTROY\":" << counts.nonclient_destroy
                  << "}\n";
    }
    std::cout.flush();
}

[[nodiscard]] int run_self_test(const std::chrono::seconds hold_duration) {
    TestRecorder tests;
    HarnessEnvironment environment;
    environment.create_windows();

    auto startup = capture_all(environment, tests, "startup");
    emit_runtime_environment(startup.front());
    const auto desired = desired_two_by_two(startup, tests);

    constexpr std::array<std::size_t, kWindowCount> all_indices{0U, 1U, 2U, 3U};
    environment.set_phase(all_indices, "startup_layout");
    std::vector<WindowTranslationRequest> layout_requests;
    layout_requests.reserve(kWindowCount);
    for (std::size_t index = 0; index < kWindowCount; ++index) {
        layout_requests.push_back({environment.token(index), desired[index]});
    }
    auto layout_result = environment.operations_adapter().apply(layout_requests);
    emit_operation_result(environment, "startup_layout", layout_result);
    tests.require(layout_result.succeeded(), "owned 2x2 startup layout batch");
    pump_messages_for(kObserverSettleDelay);

    const auto initial = capture_all(environment, tests, "initial_2x2");
    for (std::size_t index = 0; index < kWindowCount; ++index) {
        tests.check(initial[index].visible_rect == desired[index],
                    "initial requested visible frame equals actual " +
                        label_string(environment.label(index)));
    }

    const auto initial_graph = build_graph(initial);
    tests.require(initial_graph.relations().size() == 4U,
                  "exact visible 2x2 has four side adjacencies");
    tests.require(initial_graph.connected_component(model::WindowId{"A"}).size() ==
                      kWindowCount,
                  "exact visible 2x2 is one connected component");
    movement::TranslationSession session{initial_graph, model::WindowId{"A"}};
    environment.reset_message_counts();

    constexpr std::array<movement::TranslationDelta, 5U> scripted_deltas{
        movement::TranslationDelta{30, 20},
        movement::TranslationDelta{80, 50},
        movement::TranslationDelta{80, 50},
        movement::TranslationDelta{40, 10},
        movement::TranslationDelta{120, 60},
    };

    std::array<OwnedWindowSnapshot, kWindowCount> current = initial;
    for (std::size_t step = 0; step < scripted_deltas.size(); ++step) {
        const auto& delta = scripted_deltas[step];
        const std::string step_name = "step_" + std::to_string(step + 1U) +
                                      "_dx_" + std::to_string(delta.dx) +
                                      "_dy_" + std::to_string(delta.dy);
        const std::array<std::size_t, 1U> leader_indices{kLeaderIndex};
        environment.set_phase(leader_indices, step_name + "_leader_setwindowpos");

        const geometry::Rect leader_target = movement::translate_rect(
            initial[kLeaderIndex].visible_rect, delta);
        auto leader_result = environment.operations_adapter().apply_one(
            {environment.token(kLeaderIndex), leader_target});
        emit_operation_result(environment,
                              step_name + "_leader_setwindowpos",
                              leader_result);
        tests.require(leader_result.succeeded(),
                      step_name + " leader SetWindowPos verified");
        pump_messages_for(kObserverSettleDelay);

        const auto leader_actual = capture_required(
            environment.operations_adapter(),
            environment.token(kLeaderIndex),
            tests,
            step_name + " leader recapture");
        const auto plan = session.plan(leader_actual.visible_rect);
        tests.require(plan.has_value(), step_name + " R1-A plan exists");
        tests.require(plan->size() == 3U,
                      step_name + " R1-A plan contains B/C/D");

        constexpr std::array<std::size_t, 3U> follower_indices{1U, 2U, 3U};
        environment.set_phase(follower_indices,
                              step_name + "_followers_defer_batch");
        std::vector<WindowTranslationRequest> follower_requests;
        follower_requests.reserve(plan->size());
        for (const auto& planned : *plan) {
            const auto label = planned.id.value();
            tests.require(label.size() == 1U && label.front() >= 'B' &&
                              label.front() <= 'D',
                          step_name + " planned follower identity");
            const auto index = static_cast<std::size_t>(label.front() - 'A');
            follower_requests.push_back(
                {environment.token(index), planned.target_visible_rect});
        }

        auto follower_result =
            environment.operations_adapter().apply(follower_requests);
        emit_operation_result(environment,
                              step_name + "_followers_defer_batch",
                              follower_result);
        tests.require(follower_result.succeeded(),
                      step_name + " follower Defer batch verified");
        pump_messages_for(kObserverSettleDelay);

        current = capture_all(environment, tests, step_name);
        for (std::size_t index = 0; index < kWindowCount; ++index) {
            const auto expected_visible = movement::translate_rect(
                initial[index].visible_rect, delta);
            const auto expected_positioning = movement::translate_rect(
                initial[index].positioning_rect, delta);
            tests.check(current[index].visible_rect == expected_visible,
                        step_name + " visible target " +
                            label_string(environment.label(index)));
            tests.check(current[index].positioning_rect == expected_positioning,
                        step_name + " equal positioning delta " +
                            label_string(environment.label(index)));
            tests.check(current[index].dpi == initial[index].dpi &&
                            current[index].monitor_device_name ==
                                initial[index].monitor_device_name,
                        step_name + " DPI and monitor stable " +
                            label_string(environment.label(index)));
        }

        const auto current_graph = build_graph(current);
        tests.check(std::equal(initial_graph.relations().begin(),
                               initial_graph.relations().end(),
                               current_graph.relations().begin(),
                               current_graph.relations().end()),
                    step_name + " adjacency topology preserved");
    }

    tests.check(current.front().visible_rect ==
                    movement::translate_rect(initial.front().visible_rect,
                                             scripted_deltas.back()),
                "repeated and backtrack sequence has no accumulated drift");

    if (hold_duration.count() > 0) {
        std::cout << "{\"schema_version\":1,\"record_kind\":\"hold\""
                  << ",\"seconds\":" << hold_duration.count() << "}\n";
        std::cout.flush();
        pump_messages_for(std::chrono::duration_cast<std::chrono::milliseconds>(
            hold_duration));
    }

    const OwnedWindowToken stale_token =
        environment.token(kDestroyedFollowerIndex);
    const auto destroyed_before = current[kDestroyedFollowerIndex];
    const std::array<std::size_t, 1U> destroyed_index{kDestroyedFollowerIndex};
    environment.set_phase(destroyed_index, "destroy_follower_C");
    tests.require(environment.destroy_window(kDestroyedFollowerIndex),
                  "DestroyWindow follower C succeeds");
    pump_messages_for(kObserverSettleDelay);
    tests.check(!environment.registry().contains(stale_token),
                "WM_NCDESTROY invalidates C token generation");

    const auto before_b = capture_required(environment.operations_adapter(),
                                           environment.token(1U),
                                           tests,
                                           "mixed invalid preflight B before");
    const auto before_d = capture_required(environment.operations_adapter(),
                                           environment.token(3U),
                                           tests,
                                           "mixed invalid preflight D before");
    constexpr movement::TranslationDelta invalid_batch_delta{7, 5};
    std::vector<WindowTranslationRequest> invalid_batch{
        {environment.token(1U),
         movement::translate_rect(before_b.visible_rect, invalid_batch_delta)},
        {stale_token,
         movement::translate_rect(destroyed_before.visible_rect,
                                  invalid_batch_delta)},
        {environment.token(3U),
         movement::translate_rect(before_d.visible_rect, invalid_batch_delta)},
    };
    constexpr std::array<std::size_t, 2U> surviving_followers{1U, 3U};
    environment.set_phase(surviving_followers, "invalid_member_preflight");
    auto invalid_result = environment.operations_adapter().apply(invalid_batch);
    emit_operation_result(environment, "invalid_member_preflight", invalid_result);
    tests.check(invalid_result.status == OperationStatus::StaleToken &&
                    invalid_result.stage == OperationStage::Preflight,
                "mixed valid-stale-valid batch fails preflight");
    tests.check(!invalid_result.native_apply_attempted,
                "mixed invalid batch performs no native apply");

    const auto after_b = capture_required(environment.operations_adapter(),
                                          environment.token(1U),
                                          tests,
                                          "mixed invalid preflight B after");
    const auto after_d = capture_required(environment.operations_adapter(),
                                          environment.token(3U),
                                          tests,
                                          "mixed invalid preflight D after");
    tests.check(before_b == after_b, "B unchanged after mixed invalid batch");
    tests.check(before_d == after_d, "D unchanged after mixed invalid batch");

    auto stale_result = environment.operations_adapter().apply_one(
        {stale_token, destroyed_before.visible_rect});
    emit_operation_result(environment, "stale_token_single", stale_result);
    tests.check(stale_result.status == OperationStatus::StaleToken &&
                    stale_result.stage == OperationStage::Preflight &&
                    !stale_result.native_apply_attempted,
                "destroyed token fails before SetWindowPos");
    tests.not_tested("numeric HWND reuse does not revive stale token",
                     "Windows handle-value reuse cannot be forced reliably; generation semantics are unit-tested");

    emit_message_counts(environment);
    std::vector<OwnedWindowToken> issued_tokens;
    issued_tokens.reserve(kWindowCount);
    for (std::size_t index = 0; index < kWindowCount; ++index) {
        issued_tokens.push_back(environment.token(index));
    }
    environment.destroy_all();
    for (const auto& token : issued_tokens) {
        tests.check(!environment.registry().contains(token),
                    "cleanup invalidates token " +
                        std::to_string(token.logical_id()));
    }

    std::cout << "{\"schema_version\":1,\"record_kind\":\"self_test_summary\""
              << ",\"result\":"
              << json_quote(tests.failures() == 0 ? "PASS" : "FAIL")
              << ",\"failures\":" << tests.failures()
              << ",\"third_party_window_control\":false"
              << ",\"global_input_control\":false"
              << ",\"r1c_started\":false}\n";
    std::cout.flush();
    return tests.failures() == 0 ? 0 : 1;
}

struct CommandLineOptions {
    bool self_test{};
    std::chrono::seconds hold_duration{};
};

[[nodiscard]] std::optional<std::uint32_t> parse_positive_seconds(
    const std::string_view value) {
    std::uint32_t seconds = 0U;
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto [position, error] = std::from_chars(begin, end, seconds);
    if (error != std::errc{} || position != end || seconds == 0U ||
        seconds > 86400U) {
        return std::nullopt;
    }
    return seconds;
}

void print_usage(std::ostream& output) {
    output << "Usage:\n"
              "  panebind-owned-window-harness --self-test [--hold-seconds N]\n"
              "  panebind-owned-window-harness --help\n";
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
    if (!options.self_test) {
        return std::nullopt;
    }
    return options;
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
        std::cout.flush();
        return 1;
    }
}
