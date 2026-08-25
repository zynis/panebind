#include "platform/windows/companion/companion_protocol.h"

#include <windows.h>
#include <windowsx.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace {

namespace protocol =
    panebind::platform::windows::companion::protocol;

constexpr UINT kUiRequestMessage = WM_APP + 0x61U;
constexpr int kInitialWidth = 300;
constexpr int kInitialHeight = 210;

class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(const HANDLE value) noexcept : value_{value} {}

    ~UniqueHandle() { reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept
        : value_{std::exchange(other.value_, nullptr)} {}

    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.value_, nullptr));
        }
        return *this;
    }

    [[nodiscard]] HANDLE get() const noexcept { return value_; }

    [[nodiscard]] bool valid() const noexcept {
        return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
    }

    void reset(const HANDLE replacement = nullptr) noexcept {
        if (valid()) {
            static_cast<void>(CloseHandle(value_));
        }
        value_ = replacement;
    }

private:
    HANDLE value_{};
};

struct TargetOptions {
    protocol::SessionMarker session{};
    HANDLE command_read_handle{};
    HANDLE response_write_handle{};
};

[[nodiscard]] bool parse_unsigned_decimal(const std::wstring_view text,
                                          std::uint64_t& output) noexcept {
    if (text.empty()) {
        return false;
    }

    std::uint64_t value = 0U;
    for (const wchar_t character : text) {
        if (character < L'0' || character > L'9') {
            return false;
        }
        const auto digit = static_cast<std::uint64_t>(character - L'0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) /
                        10U) {
            return false;
        }
        value = value * 10U + digit;
    }
    output = value;
    return true;
}

[[nodiscard]] std::optional<TargetOptions> parse_options(const int argc,
                                                          wchar_t* argv[]) {
    if (argc != 9) {
        return std::nullopt;
    }

    std::optional<std::uint64_t> session_high;
    std::optional<std::uint64_t> session_low;
    std::optional<std::uint64_t> command_read;
    std::optional<std::uint64_t> response_write;

    for (int index = 1; index < argc; index += 2) {
        const std::wstring_view name{argv[index]};
        const std::wstring_view value_text{argv[index + 1]};
        std::uint64_t value = 0U;
        if (!parse_unsigned_decimal(value_text, value)) {
            return std::nullopt;
        }

        auto assign_once = [value](std::optional<std::uint64_t>& destination) {
            if (destination.has_value()) {
                return false;
            }
            destination = value;
            return true;
        };

        if (name == L"--session-high") {
            if (!assign_once(session_high)) {
                return std::nullopt;
            }
        } else if (name == L"--session-low") {
            if (!assign_once(session_low)) {
                return std::nullopt;
            }
        } else if (name == L"--command-read-handle") {
            if (!assign_once(command_read)) {
                return std::nullopt;
            }
        } else if (name == L"--response-write-handle") {
            if (!assign_once(response_write)) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }

    if (!session_high.has_value() || !session_low.has_value() ||
        !command_read.has_value() || !response_write.has_value() ||
        *command_read == 0U || *response_write == 0U ||
        *command_read > std::numeric_limits<std::uintptr_t>::max() ||
        *response_write > std::numeric_limits<std::uintptr_t>::max()) {
        return std::nullopt;
    }

    TargetOptions options;
    options.session = {*session_high, *session_low};
    if (!options.session.valid()) {
        return std::nullopt;
    }
    options.command_read_handle = reinterpret_cast<HANDLE>(
        static_cast<std::uintptr_t>(*command_read));
    options.response_write_handle = reinterpret_cast<HANDLE>(
        static_cast<std::uintptr_t>(*response_write));
    if (options.command_read_handle == options.response_write_handle ||
        options.command_read_handle == INVALID_HANDLE_VALUE ||
        options.response_write_handle == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }
    return options;
}

enum class IoStatus {
    Succeeded,
    EndOfFile,
    Failed,
};

struct IoResult {
    IoStatus status{IoStatus::Failed};
    DWORD error{};
};

[[nodiscard]] IoResult read_exact(const HANDLE handle,
                                  void* const destination,
                                  const std::size_t byte_count) noexcept {
    auto* cursor = static_cast<std::byte*>(destination);
    std::size_t remaining = byte_count;
    while (remaining != 0U) {
        const auto requested = static_cast<DWORD>(remaining);
        DWORD received = 0U;
        if (ReadFile(handle, cursor, requested, &received, nullptr) == FALSE) {
            const DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_HANDLE_EOF ||
                error == ERROR_NO_DATA) {
                return {IoStatus::EndOfFile, error};
            }
            return {IoStatus::Failed, error};
        }
        if (received == 0U) {
            return {IoStatus::EndOfFile, ERROR_HANDLE_EOF};
        }
        cursor += received;
        remaining -= received;
    }
    return {IoStatus::Succeeded, ERROR_SUCCESS};
}

[[nodiscard]] IoResult write_exact(const HANDLE handle,
                                   const void* const source,
                                   const std::size_t byte_count) noexcept {
    const auto* cursor = static_cast<const std::byte*>(source);
    std::size_t remaining = byte_count;
    while (remaining != 0U) {
        const auto requested = static_cast<DWORD>(remaining);
        DWORD written = 0U;
        if (WriteFile(handle, cursor, requested, &written, nullptr) == FALSE) {
            const DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
                return {IoStatus::EndOfFile, error};
            }
            return {IoStatus::Failed, error};
        }
        if (written == 0U) {
            return {IoStatus::Failed, ERROR_WRITE_FAULT};
        }
        cursor += written;
        remaining -= written;
    }
    return {IoStatus::Succeeded, ERROR_SUCCESS};
}

[[nodiscard]] IoResult write_frame(const HANDLE handle,
                                   const protocol::SessionMarker session,
                                   const protocol::FrameKind kind,
                                   const std::uint64_t request_id,
                                   const void* const payload,
                                   const std::size_t payload_size) noexcept {
    if (payload_size > protocol::kMaximumPayloadBytes ||
        payload_size > std::numeric_limits<std::uint32_t>::max()) {
        return {IoStatus::Failed, ERROR_INVALID_DATA};
    }

    protocol::FrameHeader header;
    header.kind = kind;
    header.payload_size = static_cast<std::uint32_t>(payload_size);
    header.session = session;
    header.request_id = request_id;

    auto result = write_exact(handle, &header, sizeof(header));
    if (result.status != IoStatus::Succeeded || payload_size == 0U) {
        return result;
    }
    return write_exact(handle, payload, payload_size);
}

template <typename Payload>
[[nodiscard]] IoResult write_frame(const HANDLE handle,
                                   const protocol::SessionMarker session,
                                   const protocol::FrameKind kind,
                                   const std::uint64_t request_id,
                                   const Payload& payload) noexcept {
    static_assert(std::is_trivially_copyable_v<Payload>);
    return write_frame(handle,
                       session,
                       kind,
                       request_id,
                       &payload,
                       sizeof(payload));
}

struct ReceivedFrame {
    protocol::FrameHeader header{};
    std::array<std::byte, protocol::kMaximumPayloadBytes> payload{};
};

[[nodiscard]] IoResult read_frame(const HANDLE handle,
                                  ReceivedFrame& frame) noexcept {
    auto result = read_exact(handle, &frame.header, sizeof(frame.header));
    if (result.status != IoStatus::Succeeded) {
        return result;
    }
    if (frame.header.payload_size > frame.payload.size()) {
        return {IoStatus::Failed, ERROR_INVALID_DATA};
    }
    if (frame.header.payload_size == 0U) {
        return {IoStatus::Succeeded, ERROR_SUCCESS};
    }
    return read_exact(handle,
                      frame.payload.data(),
                      frame.header.payload_size);
}

template <typename Payload>
[[nodiscard]] bool decode_exact_payload(const ReceivedFrame& frame,
                                        Payload& payload) noexcept {
    static_assert(std::is_trivially_copyable_v<Payload>);
    if (frame.header.payload_size != sizeof(Payload)) {
        return false;
    }
    std::memcpy(&payload, frame.payload.data(), sizeof(payload));
    return true;
}

struct TargetContext;

struct WindowState {
    TargetContext* context{};
    protocol::LogicalWindowId logical_id{};
    std::uint64_t generation{protocol::kInitialWindowGeneration};
    HWND window{};
    std::uint64_t active_step_id{};
    std::int32_t d_window_x_offset{};
};

class EvidenceStore {
public:
    void append(protocol::EvidenceRecord record) noexcept {
        std::scoped_lock lock{mutex_};
        record.sequence = next_sequence_++;
        if (count_ < records_.size()) {
            records_[count_++] = record;
        } else {
            ++dropped_record_count_;
        }
    }

    void snapshot(protocol::EvidencePayload& output,
                  const DWORD process_id,
                  const DWORD ui_thread_id) const noexcept {
        std::scoped_lock lock{mutex_};
        output.record_count = static_cast<std::uint32_t>(count_);
        output.overflowed = dropped_record_count_ == 0U ? 0U : 1U;
        output.dropped_record_count = dropped_record_count_;
        output.next_sequence = next_sequence_;
        output.target_process_id = process_id;
        output.target_ui_thread_id = ui_thread_id;
        for (std::size_t index = 0U; index < count_; ++index) {
            output.records[index] = records_[index];
        }
    }

private:
    mutable std::mutex mutex_;
    std::array<protocol::EvidenceRecord,
               protocol::kMaximumEvidenceRecords>
        records_{};
    std::size_t count_{};
    std::uint64_t next_sequence_{1U};
    std::uint64_t dropped_record_count_{};
};

struct TargetContext {
    protocol::SessionMarker session{};
    std::wstring property_name;
    HINSTANCE instance{};
    DWORD process_id{};
    DWORD ui_thread_id{};
    HANDLE ready_event{};
    HANDLE exit_event{};
    bool startup_succeeded{};
    DWORD startup_error{};
    std::atomic_bool ui_running{false};
    std::array<WindowState, protocol::kWindowCount> windows{};
    EvidenceStore evidence;
};

void record_position_message(WindowState& state,
                             const protocol::EvidenceMessage message,
                             const WINDOWPOS* const position,
                             const int proposed_x,
                             const int effective_x,
                             const int applied_offset,
                             const WPARAM wparam = 0U) noexcept {
    protocol::EvidenceRecord record;
    record.step_id = state.active_step_id;
    record.tick_ms = GetTickCount64();
    record.logical_id = state.logical_id;
    record.message = message;
    record.generation = state.generation;
    record.native_window = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(state.window));
    record.target_process_id = state.context->process_id;
    record.target_ui_thread_id = state.context->ui_thread_id;
    record.proposed_x = proposed_x;
    record.effective_x = effective_x;
    record.applied_test_offset_x = applied_offset;
    record.message_wparam = static_cast<std::uint64_t>(wparam);
    if (position != nullptr) {
        record.y = position->y;
        record.width = position->cx;
        record.height = position->cy;
        record.flags = position->flags;
    }
    state.context->evidence.append(record);
}

void record_simple_message(WindowState& state,
                           const protocol::EvidenceMessage message,
                           const int first,
                           const int second,
                           const WPARAM wparam = 0U) noexcept {
    protocol::EvidenceRecord record;
    record.step_id = state.active_step_id;
    record.tick_ms = GetTickCount64();
    record.logical_id = state.logical_id;
    record.message = message;
    record.generation = state.generation;
    record.native_window = static_cast<std::uint64_t>(
        reinterpret_cast<std::uintptr_t>(state.window));
    record.target_process_id = state.context->process_id;
    record.target_ui_thread_id = state.context->ui_thread_id;
    record.proposed_x = first;
    record.effective_x = first;
    if (message == protocol::EvidenceMessage::Move) {
        record.y = second;
    } else if (message == protocol::EvidenceMessage::Size) {
        record.width = first;
        record.height = second;
    }
    record.message_wparam = static_cast<std::uint64_t>(wparam);
    state.context->evidence.append(record);
}

[[nodiscard]] WindowState* window_state_from(const HWND window) noexcept {
    return reinterpret_cast<WindowState*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
}

LRESULT CALLBACK companion_window_proc(const HWND window,
                                       const UINT message,
                                       const WPARAM wparam,
                                       const LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        auto* state = static_cast<WindowState*>(create->lpCreateParams);
        state->window = window;
        SetWindowLongPtrW(window,
                          GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(state));
    }

    WindowState* const state = window_state_from(window);
    if (state == nullptr) {
        return DefWindowProcW(window, message, wparam, lparam);
    }

    switch (message) {
    case WM_WINDOWPOSCHANGING: {
        auto* position = reinterpret_cast<WINDOWPOS*>(lparam);
        if (position != nullptr) {
            const int proposed_x = position->x;
            int applied_offset = 0;
            if (state->logical_id == protocol::LogicalWindowId::D &&
                state->d_window_x_offset != 0 &&
                (position->flags & SWP_NOMOVE) == 0U) {
                const auto candidate =
                    static_cast<std::int64_t>(position->x) +
                    state->d_window_x_offset;
                if (candidate >= std::numeric_limits<int>::min() &&
                    candidate <= std::numeric_limits<int>::max()) {
                    position->x = static_cast<int>(candidate);
                    applied_offset = state->d_window_x_offset;
                }
            }
            record_position_message(*state,
                                    protocol::EvidenceMessage::WindowPosChanging,
                                    position,
                                    proposed_x,
                                    position->x,
                                    applied_offset,
                                    wparam);
        }
        break;
    }
    case WM_WINDOWPOSCHANGED: {
        const auto* position = reinterpret_cast<const WINDOWPOS*>(lparam);
        if (position != nullptr) {
            record_position_message(*state,
                                    protocol::EvidenceMessage::WindowPosChanged,
                                    position,
                                    position->x,
                                    position->x,
                                    0,
                                    wparam);
        }
        break;
    }
    case WM_MOVE:
        record_simple_message(*state,
                              protocol::EvidenceMessage::Move,
                              GET_X_LPARAM(lparam),
                              GET_Y_LPARAM(lparam),
                              wparam);
        break;
    case WM_SIZE:
        record_simple_message(*state,
                              protocol::EvidenceMessage::Size,
                              static_cast<int>(LOWORD(lparam)),
                              static_cast<int>(HIWORD(lparam)),
                              wparam);
        break;
    case WM_NCDESTROY:
        record_simple_message(*state,
                              protocol::EvidenceMessage::NonClientDestroy,
                              0,
                              0,
                              wparam);
        static_cast<void>(RemovePropW(
            window, state->context->property_name.c_str()));
        state->window = nullptr;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        break;
    case WM_CLOSE:
        static_cast<void>(DestroyWindow(window));
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

enum class UiRequestKind {
    SetStep,
    DestroyWindow,
    Shutdown,
};

struct UiRequest {
    UiRequestKind kind{};
    protocol::SetStepPayload set_step{};
    protocol::DestroyWindowPayload destroy{};
    HANDLE completion_event{};
    protocol::CommandStatus status{protocol::CommandStatus::NativeFailure};
    DWORD native_error{};
};

void destroy_remaining_windows(TargetContext& context) noexcept {
    for (auto& state : context.windows) {
        if (state.window != nullptr && IsWindow(state.window) != FALSE) {
            static_cast<void>(DestroyWindow(state.window));
        }
    }
}

void process_ui_request(TargetContext& context, UiRequest& request) noexcept {
    request.status = protocol::CommandStatus::Succeeded;
    request.native_error = ERROR_SUCCESS;

    switch (request.kind) {
    case UiRequestKind::SetStep:
        for (auto& state : context.windows) {
            state.active_step_id = request.set_step.step_id;
            state.d_window_x_offset =
                state.logical_id == protocol::LogicalWindowId::D &&
                        (request.set_step.uncooperative_window_mask &
                         protocol::logical_window_bit(
                             protocol::LogicalWindowId::D)) != 0U
                    ? request.set_step.d_window_x_offset
                    : 0;
        }
        break;
    case UiRequestKind::DestroyWindow: {
        const auto raw_id =
            static_cast<std::uint32_t>(request.destroy.logical_id);
        if (!protocol::is_valid_logical_window_id(raw_id)) {
            request.status = protocol::CommandStatus::InvalidWindow;
            request.native_error = ERROR_INVALID_PARAMETER;
            break;
        }
        auto& state = context.windows[raw_id - 1U];
        if (state.window == nullptr || IsWindow(state.window) == FALSE) {
            request.status = protocol::CommandStatus::InvalidWindow;
            request.native_error = ERROR_INVALID_WINDOW_HANDLE;
            break;
        }
        if (DestroyWindow(state.window) == FALSE) {
            request.status = protocol::CommandStatus::NativeFailure;
            request.native_error = GetLastError();
        }
        break;
    }
    case UiRequestKind::Shutdown:
        destroy_remaining_windows(context);
        context.ui_running.store(false, std::memory_order_release);
        PostQuitMessage(0);
        break;
    }

    static_cast<void>(SetEvent(request.completion_event));
}

[[nodiscard]] bool install_marker(TargetContext& context,
                                  WindowState& state) noexcept {
    const auto value = protocol::marker_property_value(state.logical_id,
                                                        state.generation);
    return SetPropW(state.window,
                    context.property_name.c_str(),
                    reinterpret_cast<HANDLE>(
                        static_cast<std::uintptr_t>(value))) != FALSE;
}

[[nodiscard]] bool create_windows(TargetContext& context) noexcept {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = companion_window_proc;
    window_class.hInstance = context.instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = protocol::kWindowClassName;
    if (RegisterClassExW(&window_class) == 0U) {
        context.startup_error = GetLastError();
        return false;
    }

    for (std::size_t index = 0U; index < context.windows.size(); ++index) {
        auto& state = context.windows[index];
        state.context = &context;
        state.logical_id = static_cast<protocol::LogicalWindowId>(index + 1U);
        state.generation = protocol::kInitialWindowGeneration;

        wchar_t title[] = L"PaneBind R1-C1 Companion A";
        title[sizeof(title) / sizeof(title[0]) - 2U] =
            static_cast<wchar_t>(L'A' + static_cast<int>(index));
        const int column = static_cast<int>(index % 2U);
        const int row = static_cast<int>(index / 2U);
        HWND const window = CreateWindowExW(
            WS_EX_NOACTIVATE,
            protocol::kWindowClassName,
            title,
            WS_OVERLAPPEDWINDOW,
            80 + column * kInitialWidth,
            80 + row * kInitialHeight,
            kInitialWidth,
            kInitialHeight,
            nullptr,
            nullptr,
            context.instance,
            &state);
        if (window == nullptr) {
            context.startup_error = GetLastError();
            destroy_remaining_windows(context);
            static_cast<void>(UnregisterClassW(protocol::kWindowClassName,
                                               context.instance));
            return false;
        }
        state.window = window;
        if (!install_marker(context, state)) {
            context.startup_error = GetLastError();
            destroy_remaining_windows(context);
            static_cast<void>(UnregisterClassW(protocol::kWindowClassName,
                                               context.instance));
            return false;
        }
    }

    for (const auto& state : context.windows) {
        static_cast<void>(ShowWindow(state.window, SW_SHOWNOACTIVATE));
        static_cast<void>(UpdateWindow(state.window));
    }
    return true;
}

void ui_thread_main(TargetContext& context) noexcept {
    context.ui_thread_id = GetCurrentThreadId();
    MSG message{};
    static_cast<void>(PeekMessageW(&message, nullptr, WM_USER, WM_USER,
                                   PM_NOREMOVE));

    context.startup_succeeded = create_windows(context);
    context.ui_running.store(context.startup_succeeded,
                             std::memory_order_release);
    static_cast<void>(SetEvent(context.ready_event));
    if (!context.startup_succeeded) {
        static_cast<void>(SetEvent(context.exit_event));
        return;
    }

    while (true) {
        const BOOL result = GetMessageW(&message, nullptr, 0U, 0U);
        if (result <= 0) {
            break;
        }
        if (message.hwnd == nullptr && message.message == kUiRequestMessage) {
            auto* request = reinterpret_cast<UiRequest*>(message.lParam);
            if (request != nullptr) {
                process_ui_request(context, *request);
            }
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    destroy_remaining_windows(context);
    static_cast<void>(UnregisterClassW(protocol::kWindowClassName,
                                       context.instance));
    context.ui_running.store(false, std::memory_order_release);
    static_cast<void>(SetEvent(context.exit_event));
}

[[nodiscard]] bool invoke_ui(TargetContext& context,
                             UiRequest& request) noexcept {
    if (!context.ui_running.load(std::memory_order_acquire)) {
        request.status = protocol::CommandStatus::UiThreadUnavailable;
        request.native_error = ERROR_INVALID_THREAD_ID;
        return false;
    }

    UniqueHandle completion{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    if (!completion.valid()) {
        request.status = protocol::CommandStatus::NativeFailure;
        request.native_error = GetLastError();
        return false;
    }
    request.completion_event = completion.get();
    if (PostThreadMessageW(context.ui_thread_id,
                           kUiRequestMessage,
                           0U,
                           reinterpret_cast<LPARAM>(&request)) == FALSE) {
        request.status = protocol::CommandStatus::UiThreadUnavailable;
        request.native_error = GetLastError();
        return false;
    }
    const std::array<HANDLE, 2U> wait_handles{completion.get(),
                                              context.exit_event};
    const DWORD wait_result = WaitForMultipleObjects(
        static_cast<DWORD>(wait_handles.size()),
        wait_handles.data(),
        FALSE,
        INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        request.status = protocol::CommandStatus::UiThreadUnavailable;
        request.native_error = wait_result == WAIT_OBJECT_0 + 1U
                                   ? ERROR_INVALID_THREAD_ID
                                   : GetLastError();
        return false;
    }
    return request.status == protocol::CommandStatus::Succeeded;
}

[[nodiscard]] protocol::HandshakePayload make_handshake(
    const TargetContext& context) noexcept {
    protocol::HandshakePayload handshake;
    handshake.target_process_id = context.process_id;
    handshake.target_ui_thread_id = context.ui_thread_id;
    handshake.window_count = protocol::kWindowCount;
    for (std::size_t index = 0U; index < context.windows.size(); ++index) {
        const auto& state = context.windows[index];
        auto& fact = handshake.windows[index];
        fact.logical_id = state.logical_id;
        fact.generation = state.generation;
        fact.native_window = static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(state.window));
        fact.process_id = context.process_id;
        fact.ui_thread_id = context.ui_thread_id;
        fact.marker_value = protocol::marker_property_value(
            state.logical_id, state.generation);
    }
    return handshake;
}

[[nodiscard]] IoResult send_command_result(
    const HANDLE response_handle,
    const TargetContext& context,
    const protocol::FrameKind response_kind,
    const std::uint64_t request_id,
    const protocol::FrameKind command,
    const protocol::CommandStatus status,
    const DWORD native_error,
    const protocol::LogicalWindowId logical_id = {},
    const std::uint64_t step_id = 0U) noexcept {
    protocol::CommandResultPayload payload;
    payload.command = command;
    payload.status = status;
    payload.native_error = native_error;
    payload.target_process_id = context.process_id;
    payload.target_ui_thread_id = context.ui_thread_id;
    payload.logical_id = logical_id;
    payload.step_id = step_id;
    return write_frame(response_handle,
                       context.session,
                       response_kind,
                       request_id,
                       payload);
}

[[nodiscard]] bool validate_set_step(
    const protocol::SetStepPayload& payload) noexcept {
    const auto d_bit =
        protocol::logical_window_bit(protocol::LogicalWindowId::D);
    if ((payload.uncooperative_window_mask & ~d_bit) != 0U ||
        payload.d_window_x_offset < -protocol::kMaximumTestOffset ||
        payload.d_window_x_offset > protocol::kMaximumTestOffset) {
        return false;
    }
    return payload.d_window_x_offset == 0 ||
           (payload.uncooperative_window_mask & d_bit) != 0U;
}

[[nodiscard]] int run_command_loop(const HANDLE command_handle,
                                   const HANDLE response_handle,
                                   TargetContext& context) {
    std::uint64_t last_request_id = 0U;
    bool shutdown_requested = false;

    while (!shutdown_requested) {
        ReceivedFrame frame;
        const IoResult read_result = read_frame(command_handle, frame);
        if (read_result.status != IoStatus::Succeeded) {
            return read_result.status == IoStatus::EndOfFile ? 0 : 20;
        }

        const bool common_header_valid =
            frame.header.magic == protocol::kFrameMagic &&
            frame.header.version == protocol::kProtocolVersion &&
            frame.header.flags == 0U;
        if (!common_header_valid || frame.header.session != context.session ||
            frame.header.request_id == 0U ||
            frame.header.request_id <= last_request_id) {
            const auto status = frame.header.session == context.session
                                    ? protocol::CommandStatus::InvalidFrame
                                    : protocol::CommandStatus::SessionMismatch;
            static_cast<void>(send_command_result(
                response_handle,
                context,
                protocol::FrameKind::FatalError,
                frame.header.request_id,
                frame.header.kind,
                status,
                ERROR_INVALID_DATA));
            return 21;
        }
        last_request_id = frame.header.request_id;

        switch (frame.header.kind) {
        case protocol::FrameKind::SetStep: {
            protocol::SetStepPayload payload;
            if (!decode_exact_payload(frame, payload) ||
                !validate_set_step(payload)) {
                const auto result = send_command_result(
                    response_handle,
                    context,
                    protocol::FrameKind::CommandResult,
                    frame.header.request_id,
                    frame.header.kind,
                    protocol::CommandStatus::InvalidPayload,
                    ERROR_INVALID_PARAMETER);
                if (result.status != IoStatus::Succeeded) {
                    return 22;
                }
                break;
            }
            UiRequest request;
            request.kind = UiRequestKind::SetStep;
            request.set_step = payload;
            static_cast<void>(invoke_ui(context, request));
            const auto result = send_command_result(
                response_handle,
                context,
                protocol::FrameKind::CommandResult,
                frame.header.request_id,
                frame.header.kind,
                request.status,
                request.native_error,
                {},
                payload.step_id);
            if (result.status != IoStatus::Succeeded) {
                return 22;
            }
            break;
        }
        case protocol::FrameKind::DestroyWindow: {
            protocol::DestroyWindowPayload payload;
            if (!decode_exact_payload(frame, payload) ||
                payload.reserved != 0U ||
                !protocol::is_valid_logical_window_id(
                    static_cast<std::uint32_t>(payload.logical_id))) {
                const auto result = send_command_result(
                    response_handle,
                    context,
                    protocol::FrameKind::CommandResult,
                    frame.header.request_id,
                    frame.header.kind,
                    protocol::CommandStatus::InvalidPayload,
                    ERROR_INVALID_PARAMETER);
                if (result.status != IoStatus::Succeeded) {
                    return 22;
                }
                break;
            }
            UiRequest request;
            request.kind = UiRequestKind::DestroyWindow;
            request.destroy = payload;
            static_cast<void>(invoke_ui(context, request));
            const auto result = send_command_result(
                response_handle,
                context,
                protocol::FrameKind::CommandResult,
                frame.header.request_id,
                frame.header.kind,
                request.status,
                request.native_error,
                payload.logical_id);
            if (result.status != IoStatus::Succeeded) {
                return 22;
            }
            break;
        }
        case protocol::FrameKind::QueryEvidence: {
            if (frame.header.payload_size != 0U) {
                const auto result = send_command_result(
                    response_handle,
                    context,
                    protocol::FrameKind::CommandResult,
                    frame.header.request_id,
                    frame.header.kind,
                    protocol::CommandStatus::InvalidPayload,
                    ERROR_INVALID_PARAMETER);
                if (result.status != IoStatus::Succeeded) {
                    return 22;
                }
                break;
            }
            auto evidence = std::make_unique<protocol::EvidencePayload>();
            context.evidence.snapshot(*evidence,
                                      context.process_id,
                                      context.ui_thread_id);
            const auto result = write_frame(response_handle,
                                            context.session,
                                            protocol::FrameKind::Evidence,
                                            frame.header.request_id,
                                            *evidence);
            if (result.status != IoStatus::Succeeded) {
                return 22;
            }
            break;
        }
        case protocol::FrameKind::Shutdown: {
            if (frame.header.payload_size != 0U) {
                const auto result = send_command_result(
                    response_handle,
                    context,
                    protocol::FrameKind::CommandResult,
                    frame.header.request_id,
                    frame.header.kind,
                    protocol::CommandStatus::InvalidPayload,
                    ERROR_INVALID_PARAMETER);
                if (result.status != IoStatus::Succeeded) {
                    return 22;
                }
                break;
            }
            UiRequest request;
            request.kind = UiRequestKind::Shutdown;
            static_cast<void>(invoke_ui(context, request));
            const auto result = send_command_result(
                response_handle,
                context,
                protocol::FrameKind::CommandResult,
                frame.header.request_id,
                frame.header.kind,
                request.status,
                request.native_error);
            if (result.status != IoStatus::Succeeded) {
                return 22;
            }
            shutdown_requested = true;
            break;
        }
        default: {
            const auto result = send_command_result(
                response_handle,
                context,
                protocol::FrameKind::CommandResult,
                frame.header.request_id,
                frame.header.kind,
                protocol::CommandStatus::UnsupportedCommand,
                ERROR_NOT_SUPPORTED);
            if (result.status != IoStatus::Succeeded) {
                return 22;
            }
            break;
        }
        }
    }
    return 0;
}

[[nodiscard]] int run_target(const TargetOptions& options) {
    UniqueHandle command_handle{options.command_read_handle};
    UniqueHandle response_handle{options.response_write_handle};
    if (GetFileType(command_handle.get()) != FILE_TYPE_PIPE ||
        GetFileType(response_handle.get()) != FILE_TYPE_PIPE) {
        return 10;
    }
    if (SetHandleInformation(command_handle.get(), HANDLE_FLAG_INHERIT, 0U) ==
            FALSE ||
        SetHandleInformation(response_handle.get(), HANDLE_FLAG_INHERIT, 0U) ==
            FALSE) {
        return 10;
    }

    UniqueHandle ready_event{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    UniqueHandle exit_event{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    if (!ready_event.valid() || !exit_event.valid()) {
        return 11;
    }

    TargetContext context;
    context.session = options.session;
    context.property_name = protocol::session_property_name(options.session);
    context.instance = GetModuleHandleW(nullptr);
    context.process_id = GetCurrentProcessId();
    context.ready_event = ready_event.get();
    context.exit_event = exit_event.get();

    std::thread ui_thread{[&context] { ui_thread_main(context); }};
    const DWORD ready_result = WaitForSingleObject(ready_event.get(), INFINITE);
    if (ready_result != WAIT_OBJECT_0 || !context.startup_succeeded) {
        ui_thread.join();
        return 12;
    }

    const auto handshake = make_handshake(context);
    const auto handshake_result = write_frame(response_handle.get(),
                                              context.session,
                                              protocol::FrameKind::Handshake,
                                              0U,
                                              handshake);
    int result = 13;
    if (handshake_result.status == IoStatus::Succeeded) {
        result = run_command_loop(command_handle.get(),
                                  response_handle.get(),
                                  context);
    }

    if (context.ui_running.load(std::memory_order_acquire)) {
        UiRequest request;
        request.kind = UiRequestKind::Shutdown;
        static_cast<void>(invoke_ui(context, request));
    }
    ui_thread.join();
    return result;
}

} // namespace

int wmain(const int argc, wchar_t* argv[]) {
    const auto options = parse_options(argc, argv);
    if (!options.has_value()) {
        return 2;
    }
    try {
        return run_target(*options);
    } catch (...) {
        return 3;
    }
}
