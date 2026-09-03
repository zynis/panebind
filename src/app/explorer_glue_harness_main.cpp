#include "platform/windows/explorer/explorer_glue_session.h"
#include "platform/windows/explorer/explorer_glue_session_internal.h"
#include "platform/windows/explorer/explorer_session.h"
#include "platform/windows/explorer/explorer_session_internal.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

namespace behavior = panebind::core::behavior;
namespace explorer = panebind::platform::windows::explorer;
namespace detail = panebind::platform::windows::explorer::detail;
namespace geometry = panebind::core::geometry;

constexpr std::uint32_t kDefaultTimeoutSeconds = 120U;
constexpr std::uint32_t kMinimumTimeoutSeconds = 30U;
constexpr std::uint32_t kMaximumTimeoutSeconds = 300U;
constexpr std::size_t kNonceAttempts = 8U;

struct Options final {
    bool interactive_consent_test{};
    bool help{};
    std::optional<std::filesystem::path> evidence_log;
    std::uint32_t timeout_seconds{kDefaultTimeoutSeconds};
};

enum class ConsoleLineStatus : std::uint8_t {
    Read,
    EndOfInput,
    NotInteractiveConsole,
    Failed,
    TooLong,
};

struct ConsoleLineResult final {
    ConsoleLineStatus status{ConsoleLineStatus::Failed};
    std::wstring line;
};

[[nodiscard]] bool is_interactive_console_handle(const HANDLE handle) noexcept {
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE ||
        GetFileType(handle) != FILE_TYPE_CHAR) {
        return false;
    }
    DWORD mode = 0U;
    return GetConsoleMode(handle, &mode) != FALSE;
}

[[nodiscard]] bool write_console_text(const std::wstring_view text) noexcept {
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!is_interactive_console_handle(output) ||
        text.size() > std::numeric_limits<DWORD>::max()) {
        return false;
    }
    DWORD written = 0U;
    return WriteConsoleW(output,
                         text.data(),
                         static_cast<DWORD>(text.size()),
                         &written,
                         nullptr) != FALSE &&
           static_cast<std::size_t>(written) == text.size();
}

[[nodiscard]] ConsoleLineResult read_console_line() {
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (!is_interactive_console_handle(input)) {
        return {ConsoleLineStatus::NotInteractiveConsole, {}};
    }

    std::array<wchar_t, 128U> buffer{};
    DWORD read = 0U;
    if (ReadConsoleW(input,
                     buffer.data(),
                     static_cast<DWORD>(buffer.size()),
                     &read,
                     nullptr) == FALSE) {
        return {ConsoleLineStatus::Failed, {}};
    }
    if (read == 0U) {
        return {ConsoleLineStatus::EndOfInput, {}};
    }
    if (read == static_cast<DWORD>(buffer.size()) &&
        buffer[read - 1U] != L'\n') {
        return {ConsoleLineStatus::TooLong, {}};
    }

    std::wstring line{buffer.data(), read};
    while (!line.empty() &&
           (line.back() == L'\r' || line.back() == L'\n')) {
        line.pop_back();
    }
    return {ConsoleLineStatus::Read, std::move(line)};
}

[[nodiscard]] std::string utf8(const std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {};
    }
    const int required = WideCharToMultiByte(CP_UTF8,
                                              WC_ERR_INVALID_CHARS,
                                              value.data(),
                                              static_cast<int>(value.size()),
                                              nullptr,
                                              0,
                                              nullptr,
                                              nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8,
                            WC_ERR_INVALID_CHARS,
                            value.data(),
                            static_cast<int>(value.size()),
                            result.data(),
                            required,
                            nullptr,
                            nullptr) != required) {
        return {};
    }
    return result;
}

[[nodiscard]] std::string json_quote(const std::string_view value) {
    std::ostringstream output;
    output << '"';
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20U) {
                output << "\\u00" << hex[(character >> 4U) & 0x0fU]
                       << hex[character & 0x0fU];
            } else {
                output << static_cast<char>(character);
            }
            break;
        }
    }
    output << '"';
    return output.str();
}

[[nodiscard]] constexpr std::string_view json_bool(const bool value) noexcept {
    return value ? "true" : "false";
}

[[nodiscard]] std::string utc_timestamp() {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << time.wYear << '-'
           << std::setw(2) << time.wMonth << '-' << std::setw(2) << time.wDay
           << 'T' << std::setw(2) << time.wHour << ':' << std::setw(2)
           << time.wMinute << ':' << std::setw(2) << time.wSecond << '.'
           << std::setw(3) << time.wMilliseconds << 'Z';
    return output.str();
}

class EvidenceLog final {
public:
    EvidenceLog() noexcept = default;
    ~EvidenceLog() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            static_cast<void>(CloseHandle(handle_));
        }
    }

    EvidenceLog(const EvidenceLog&) = delete;
    EvidenceLog& operator=(const EvidenceLog&) = delete;

    [[nodiscard]] bool open_new(const std::filesystem::path& path) noexcept {
        if (handle_ != INVALID_HANDLE_VALUE) {
            return false;
        }
        handle_ = CreateFileW(path.c_str(),
                              GENERIC_WRITE,
                              FILE_SHARE_READ,
                              nullptr,
                              CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
        return handle_ != INVALID_HANDLE_VALUE;
    }

    [[nodiscard]] bool record(const std::string_view kind,
                              const std::string_view fields = {}) {
        if (!healthy_ || handle_ == INVALID_HANDLE_VALUE ||
            next_sequence_ == 0U) {
            healthy_ = false;
            return false;
        }
        std::ostringstream output;
        output << "{\"schema_version\":1,\"schema_name\":"
               << json_quote("panebind.r1c2b.explorer_glue")
               << ",\"harness_sequence\":" << next_sequence_++
               << ",\"record_kind\":" << json_quote(kind)
               << ",\"recorded_at\":" << json_quote(utc_timestamp())
               << fields << "}\n";
        const std::string line = output.str();
        if (line.size() > std::numeric_limits<DWORD>::max()) {
            healthy_ = false;
            return false;
        }
        DWORD written = 0U;
        healthy_ = WriteFile(handle_,
                             line.data(),
                             static_cast<DWORD>(line.size()),
                             &written,
                             nullptr) != FALSE &&
                   static_cast<std::size_t>(written) == line.size() &&
                   FlushFileBuffers(handle_) != FALSE;
        return healthy_;
    }

    [[nodiscard]] bool healthy() const noexcept { return healthy_; }

private:
    HANDLE handle_{INVALID_HANDLE_VALUE};
    std::uint64_t next_sequence_{1U};
    bool healthy_{true};
};

[[nodiscard]] bool parse_timeout(const std::wstring_view text,
                                 std::uint32_t& value) noexcept {
    if (text.empty()) {
        return false;
    }
    std::uint64_t parsed = 0U;
    for (const wchar_t character : text) {
        if (character < L'0' || character > L'9') {
            return false;
        }
        parsed = parsed * 10U + static_cast<unsigned>(character - L'0');
        if (parsed > kMaximumTimeoutSeconds) {
            return false;
        }
    }
    if (parsed < kMinimumTimeoutSeconds || parsed > kMaximumTimeoutSeconds) {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] std::optional<Options> parse_options(const int argc,
                                                   wchar_t* argv[]) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::wstring_view argument{argv[index]};
        if (argument == L"--interactive-consent-test") {
            if (options.interactive_consent_test) {
                return std::nullopt;
            }
            options.interactive_consent_test = true;
        } else if (argument == L"--evidence-log") {
            if (options.evidence_log.has_value() || index + 1 >= argc) {
                return std::nullopt;
            }
            options.evidence_log = std::filesystem::path{argv[++index]};
        } else if (argument == L"--timeout-seconds") {
            if (index + 1 >= argc ||
                !parse_timeout(argv[++index], options.timeout_seconds)) {
                return std::nullopt;
            }
        } else if (argument == L"--help" || argument == L"-h") {
            options.help = true;
        } else {
            return std::nullopt;
        }
    }
    if (options.help) {
        return options;
    }
    if (!options.interactive_consent_test ||
        !options.evidence_log.has_value() || options.evidence_log->empty()) {
        return std::nullopt;
    }
    return options;
}

void print_usage() {
    std::cout
        << "Usage:\n"
        << "  panebind-explorer-glue-harness.exe --interactive-consent-test "
           "--evidence-log PATH [--timeout-seconds 30..300]\n";
}

[[nodiscard]] std::optional<std::filesystem::path>
validated_repository_root() {
    std::error_code error;
    auto root = std::filesystem::current_path(error);
    if (error) {
        return std::nullopt;
    }
    root = std::filesystem::absolute(root, error).lexically_normal();
    if (error ||
        !std::filesystem::is_regular_file(root / "CMakeLists.txt", error) ||
        error || !std::filesystem::is_directory(root / "src", error) || error) {
        return std::nullopt;
    }
    return root;
}

[[nodiscard]] std::string guid_nonce() {
    GUID guid{};
    if (FAILED(CoCreateGuid(&guid))) {
        return {};
    }
    const auto* const bytes = reinterpret_cast<const unsigned char*>(&guid);
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(sizeof(guid) * 2U);
    for (std::size_t index = 0U; index < sizeof(guid); ++index) {
        result.push_back(digits[(bytes[index] >> 4U) & 0x0fU]);
        result.push_back(digits[bytes[index] & 0x0fU]);
    }
    return result;
}

[[nodiscard]] std::optional<std::filesystem::path> create_target_directory(
    const std::filesystem::path& repository_root,
    const std::string_view role) {
    std::error_code error;
    const auto evidence_root = repository_root / "uat" / "r1c2b";
    std::filesystem::create_directories(evidence_root, error);
    if (error) {
        return std::nullopt;
    }
    for (std::size_t attempt = 0U; attempt < kNonceAttempts; ++attempt) {
        const std::string nonce = guid_nonce();
        if (nonce.empty()) {
            return std::nullopt;
        }
        const auto candidate = evidence_root /
                               (std::string{role} + "-" + nonce);
        if (std::filesystem::create_directory(candidate, error)) {
            auto absolute = std::filesystem::absolute(candidate, error);
            if (error) {
                return std::nullopt;
            }
            return absolute.lexically_normal();
        }
        if (error && error != std::errc::file_exists) {
            return std::nullopt;
        }
        error.clear();
    }
    return std::nullopt;
}

[[nodiscard]] constexpr std::string_view role_name(
    const explorer::ExplorerGlueWindowRole role) noexcept {
    return role == explorer::ExplorerGlueWindowRole::Leader ? "leader"
                                                             : "follower";
}

[[nodiscard]] constexpr std::string_view glue_reason_name(
    const explorer::ExplorerGlueReason reason) noexcept {
    using enum explorer::ExplorerGlueReason;
    switch (reason) {
    case Eligible:
        return "eligible";
    case InvalidArgument:
        return "invalid_argument";
    case WrongOwnerThread:
        return "wrong_owner_thread";
    case TargetConsentIncomplete:
        return "target_consent_incomplete";
    case PairNotDistinct:
        return "pair_not_distinct";
    case FollowerBaselineMissingLeader:
        return "follower_baseline_missing_leader";
    case TargetChanged:
        return "target_changed";
    case MonitorOrDpiMismatch:
        return "monitor_or_dpi_mismatch";
    case GlueConsentRequired:
        return "glue_consent_required";
    case ConsentGenerationMismatch:
        return "consent_generation_mismatch";
    case AuthorityConsumed:
        return "authority_consumed";
    case UnsafeLayout:
        return "unsafe_layout";
    case LayoutOperationFailed:
        return "layout_operation_failed";
    case TopologyInvalid:
        return "topology_invalid";
    case EventSourceFailed:
        return "event_source_failed";
    case EventQueueOverflow:
        return "event_queue_overflow";
    case BehaviorAborted:
        return "behavior_aborted";
    case TimedOut:
        return "timed_out";
    case RestoreFailed:
        return "restore_failed";
    case CleanupLifecycleFailed:
        return "cleanup_lifecycle_failed";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view glue_stage_name(
    const explorer::ExplorerGlueStage stage) noexcept {
    using enum explorer::ExplorerGlueStage;
    switch (stage) {
    case PairValidation:
        return "pair_validation";
    case Consent:
        return "consent";
    case Layout:
        return "layout";
    case Topology:
        return "topology";
    case EventSource:
        return "event_source";
    case ActiveSession:
        return "active_session";
    case Completion:
        return "completion";
    case Restore:
        return "restore";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view behavior_state_name(
    const behavior::GlueMoveState state) noexcept {
    using enum behavior::GlueMoveState;
    switch (state) {
    case Idle:
        return "idle";
    case Armed:
        return "armed";
    case Active:
        return "active";
    case Completing:
        return "completing";
    case Completed:
        return "completed";
    case Aborted:
        return "aborted";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view behavior_event_name(
    const behavior::GlueEventKind kind) noexcept {
    using enum behavior::GlueEventKind;
    switch (kind) {
    case MoveResizeStarted:
        return "move_resize_started";
    case GeometryChanged:
        return "geometry_changed";
    case MoveResizeEnded:
        return "move_resize_ended";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view decision_name(
    const behavior::GlueDecisionKind decision) noexcept {
    using enum behavior::GlueDecisionKind;
    switch (decision) {
    case None:
        return "none";
    case Armed:
        return "armed";
    case Activated:
        return "activated";
    case FollowerMoveRequested:
        return "follower_move_requested";
    case LeaderNoOp:
        return "leader_noop";
    case OperationRecorded:
        return "operation_recorded";
    case FeedbackObservedPendingResult:
        return "feedback_observed_pending_result";
    case FeedbackAcknowledged:
        return "feedback_acknowledged";
    case DuplicateFeedbackSuppressed:
        return "duplicate_feedback_suppressed";
    case UnrelatedEventIgnored:
        return "unrelated_event_ignored";
    case TerminalInputIgnored:
        return "terminal_input_ignored";
    case Completing:
        return "completing";
    case Completed:
        return "completed";
    case Aborted:
        return "aborted";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view abort_reason_name(
    const behavior::GlueAbortReason reason) noexcept {
    using enum behavior::GlueAbortReason;
    switch (reason) {
    case IllegalTransition:
        return "illegal_transition";
    case InvalidTopology:
        return "invalid_topology";
    case InvalidEvent:
        return "invalid_event";
    case SessionGenerationMismatch:
        return "session_generation_mismatch";
    case EventSequenceNotMonotonic:
        return "event_sequence_not_monotonic";
    case OperationGenerationMismatch:
        return "operation_generation_mismatch";
    case GenerationExhausted:
        return "generation_exhausted";
    case ResizeOrMixed:
        return "resize_or_mixed";
    case LeaderDidNotTranslate:
        return "leader_did_not_translate";
    case PendingLedgerOverflow:
        return "pending_ledger_overflow";
    case EventQueueOverflow:
        return "event_queue_overflow";
    case EventSourceFailure:
        return "event_source_failure";
    case TimedOut:
        return "timed_out";
    case OperationResultMissing:
        return "operation_result_missing";
    case NativeApplyFailed:
        return "native_apply_failed";
    case PostVerificationFailed:
        return "post_verification_failed";
    case TargetInvalidated:
        return "target_invalidated";
    case UnexpectedFollowerLifecycle:
        return "unexpected_follower_lifecycle";
    case UnexpectedFollowerGeometry:
        return "unexpected_follower_geometry";
    case AmbiguousFollowerFeedback:
        return "ambiguous_follower_feedback";
    case FinalLeaderMismatch:
        return "final_leader_mismatch";
    case FinalFollowerMismatch:
        return "final_follower_mismatch";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::string_view operation_phase_name(
    const explorer::ExplorerGlueOperationPhase phase) noexcept {
    using enum explorer::ExplorerGlueOperationPhase;
    switch (phase) {
    case Setup:
        return "setup";
    case ActiveFollower:
        return "active_follower";
    case Restore:
        return "restore";
    }
    return "unknown";
}

void append_rect(std::ostringstream& output, const geometry::Rect& rect) {
    output << "{\"left\":" << rect.left() << ",\"top\":" << rect.top()
           << ",\"right\":" << rect.right() << ",\"bottom\":"
           << rect.bottom() << '}';
}

void append_snapshot(std::ostringstream& output,
                     const explorer::ExplorerWindowSnapshot& snapshot) {
    output << "{\"visible\":";
    append_rect(output, snapshot.visible_rect);
    output << ",\"positioning\":";
    append_rect(output, snapshot.positioning_rect);
    output << ",\"pid\":" << snapshot.process_id << ",\"tid\":"
           << snapshot.thread_id << ",\"class\":"
           << json_quote(utf8(snapshot.window_class)) << ",\"dpi\":"
           << snapshot.dpi << ",\"monitor\":"
           << json_quote(utf8(snapshot.monitor_device_name))
           << ",\"monitor_rect\":";
    append_rect(output, snapshot.monitor_rect);
    output << ",\"work_area\":";
    append_rect(output, snapshot.monitor_work_area);
    output << ",\"root_top_level\":" << json_bool(snapshot.root_top_level)
           << ",\"visible_state\":" << json_bool(snapshot.visible)
           << ",\"cloaked\":" << json_bool(snapshot.cloaked)
           << ",\"minimized\":" << json_bool(snapshot.minimized)
           << ",\"maximized\":" << json_bool(snapshot.maximized)
           << ",\"current_virtual_desktop\":"
           << json_bool(snapshot.on_current_virtual_desktop)
           << ",\"exact_test_location\":"
           << json_bool(snapshot.exact_test_location)
           << ",\"target_integrity_rid\":"
           << snapshot.target_security.integrity_rid
           << ",\"target_session_id\":"
           << snapshot.target_security.session_id
           << ",\"target_elevated\":"
           << json_bool(snapshot.target_security.elevated)
           << ",\"target_ui_access\":"
           << json_bool(snapshot.target_security.ui_access)
           << ",\"target_app_container\":"
           << json_bool(snapshot.target_security.app_container) << '}';
}

[[nodiscard]] bool exact_operation(
    const explorer::ExplorerOperationResult& operation) noexcept {
    if (!operation.succeeded() || !operation.receipt.has_value() ||
        !operation.receipt->actual.has_value()) {
        return false;
    }
    const auto& receipt = *operation.receipt;
    return receipt.visible_target_verified &&
           receipt.positioning_target_verified && receipt.size_preserved &&
           receipt.identity_stable && receipt.location_stable &&
           receipt.monitor_and_dpi_stable &&
           receipt.actual->visible_rect == receipt.requested_visible_rect &&
           receipt.requested_positioning_rect.has_value() &&
           receipt.actual->positioning_rect ==
               *receipt.requested_positioning_rect;
}

void record_diagnostic(EvidenceLog& evidence,
                       const std::string_view context,
                       const std::optional<explorer::ExplorerDiagnostic>& diagnostic) {
    if (!diagnostic.has_value()) {
        return;
    }
    std::ostringstream fields;
    fields << ",\"context\":" << json_quote(context)
           << ",\"domain\":"
           << static_cast<unsigned>(diagnostic->domain)
           << ",\"code\":" << diagnostic->code
           << ",\"api\":" << json_quote(diagnostic->api)
           << ",\"shell_stage\":" << json_quote(diagnostic->shell_stage)
           << ",\"detail_redacted\":true";
    static_cast<void>(evidence.record("diagnostic", fields.str()));
}

[[nodiscard]] bool emit_target_prompt(
    const explorer::ExplorerGlueWindowRole role,
    const std::filesystem::path& path) {
    std::wstring prompt = role == explorer::ExplorerGlueWindowRole::Leader
                              ? L"\n第 1 步：创建 Leader Explorer\n\n"
                              : L"\n第 2 步：创建 Follower Explorer\n\n";
    prompt += L"请亲自新建一个文件资源管理器顶层窗口，并进入这个空测试目录：\n\n";
    prompt += path.native();
    prompt +=
        L"\n\n请勿改用任何既有 Explorer 窗口。\n"
        L"请先把这个测试窗口保持为普通状态（不要最大化或最小化），并手动缩小到工作区能够同时容纳两个当前尺寸窗口；PaneBind 不会 resize。\n"
        L"完成后回到此控制台，直接按 ENTER。\n\n";
    return write_console_text(prompt);
}

[[nodiscard]] bool emit_glue_prompt() noexcept {
    return write_console_text(
        L"\n第 3 步：授权一次临时 Glue Move session\n\n"
        L"Leader Explorer = 已确认\n"
        L"Follower Explorer = 已确认\n"
        L"两个窗口均为本轮 baseline 后新建的测试窗口。\n"
        L"任何既有 Explorer 都不会被控制。\n\n"
        L"授权后 PaneBind 将：\n"
        L"1. 仅用纯平移把两个测试窗口排列为零间隙相邻；\n"
        L"2. 等待你正常拖动 Leader 一次；\n"
        L"3. 让 Follower 实时跟随；\n"
        L"4. 在你松开 Leader 后结束 session；\n"
        L"5. 停止 WinEvent source 后，把两个窗口恢复到测试前位置。\n\n"
        L"输入 Y 后按 ENTER 授权；其他输入将取消，不移动窗口。\n\n");
}

[[nodiscard]] bool emit_drag_prompt(const std::uint32_t timeout_seconds) {
    std::wostringstream prompt;
    prompt << L"\n第 4 步：现在只拖动 Leader 窗口一次\n\n"
           << L"请只用普通资源管理器标题栏拖动 Leader 一段明显距离，然后松开鼠标。\n"
           << L"不要按 Alt、Ctrl 或 Shift；不要 Resize、移动 Follower、最大化、最小化、切换目录，或拖到屏幕边缘触发 Windows Snap。\n"
           << L"无需再按键确认。Harness 将通过 WinEvent 自动观察 START / LOCATION / END。\n"
           << L"等待上限：" << timeout_seconds << L" 秒。\n\n";
    return write_console_text(prompt.str());
}

struct ProvisionedTarget final {
    std::unique_ptr<explorer::ExplorerTestSession> session;
    explorer::ExplorerConsentFacts consent_facts;
    detail::ExplorerDiagnosticNativeIdentity native_identity;
};

struct TargetAttempt final {
    std::optional<ProvisionedTarget> target;
    std::string failure_reason;
};

[[nodiscard]] TargetAttempt provision_target(
    EvidenceLog& evidence,
    const explorer::ExplorerGlueWindowRole role,
    const std::filesystem::path& path) {
    const auto role_text = role_name(role);
    auto begin = explorer::ExplorerConsentProvisioning::begin(path);
    {
        std::ostringstream fields;
        fields << ",\"role\":" << json_quote(role_text)
               << ",\"result\":"
               << json_quote(begin.succeeded() ? "PASS" : "BLOCKED")
               << ",\"reason_code\":"
               << static_cast<unsigned>(begin.reason)
               << ",\"baseline_generation\":"
               << begin.facts.generations.baseline_generation
               << ",\"total_shell_entries\":"
               << begin.facts.baseline_total_shell_entries
               << ",\"reliable_shell_entries\":"
               << begin.facts.baseline_reliable_shell_entries
               << ",\"forbidden_preexisting_hwnd_count\":"
               << begin.facts.forbidden_preexisting_hwnd_count
               << ",\"baseline_exclusion_complete\":"
               << json_bool(begin.facts.baseline_exclusion_complete)
               << ",\"target_directory_contract_verified\":"
               << json_bool(begin.succeeded());
        if (!evidence.record("baseline", fields.str())) {
            return {std::nullopt, "evidence_write_failed"};
        }
    }
    record_diagnostic(evidence, "target_baseline", begin.diagnostic);
    if (!evidence.healthy()) {
        return {std::nullopt, "evidence_write_failed"};
    }
    if (!begin.succeeded()) {
        return {std::nullopt, "target_baseline_blocked"};
    }

    auto provisioning = std::move(begin.provisioning);
    const auto prompt = provisioning->record_target_prompt();
    {
        std::ostringstream fields;
        fields << ",\"role\":" << json_quote(role_text)
               << ",\"result\":"
               << json_quote(prompt.succeeded() ? "READY" : "BLOCKED")
               << ",\"generation\":" << prompt.generation
               << ",\"input_source\":\"interactive_console\"";
        if (!evidence.record("target_consent_prompt", fields.str())) {
            return {std::nullopt, "evidence_write_failed"};
        }
    }
    record_diagnostic(evidence, "target_prompt", prompt.diagnostic);
    if (!prompt.succeeded()) {
        return {std::nullopt, "target_prompt_blocked"};
    }
    if (!emit_target_prompt(role, path)) {
        return {std::nullopt, "console_output_failed"};
    }

    const auto line = read_console_line();
    const bool confirmed = line.status == ConsoleLineStatus::Read &&
                           line.line.empty();
    {
        std::ostringstream fields;
        fields << ",\"role\":" << json_quote(role_text)
               << ",\"result\":"
               << json_quote(confirmed ? "CONFIRMED" : "DECLINED")
               << ",\"input_source\":\"interactive_console\""
               << ",\"native_apply_attempted\":false";
        if (!evidence.record("target_consent_confirmation", fields.str())) {
            return {std::nullopt, "evidence_write_failed"};
        }
    }
    if (!confirmed) {
        return {std::nullopt, "target_consent_declined"};
    }

    auto candidate = provisioning->confirm_user_target();
    {
        std::ostringstream fields;
        fields << ",\"role\":" << json_quote(role_text)
               << ",\"result\":"
               << json_quote(candidate.succeeded() ? "PASS" : "BLOCKED")
               << ",\"reason_code\":"
               << static_cast<unsigned>(candidate.reason)
               << ",\"authority_kind\":\"user_consent\""
               << ",\"exact_new_candidate_count\":"
               << candidate.facts.exact_new_candidate_count
               << ",\"preexisting_exact_location_detected\":"
               << json_bool(candidate.facts.preexisting_exact_location_detected)
               << ",\"unique_new_target\":"
               << json_bool(candidate.facts.unique_new_target)
               << ",\"exact_target_location\":"
               << json_bool(candidate.facts.exact_target_location)
               << ",\"baseline_generation\":"
               << candidate.facts.generations.baseline_generation
               << ",\"target_prompt_generation\":"
               << candidate.facts.generations.target_prompt_generation
               << ",\"target_confirmation_generation\":"
               << candidate.facts.generations.target_confirmation_generation
               << ",\"eligibility_generation\":"
               << candidate.facts.generations.eligibility_generation
               << ",\"token_generation\":"
               << candidate.facts.generations.token_generation;
        if (!evidence.record("candidate_selection", fields.str())) {
            return {std::nullopt, "evidence_write_failed"};
        }
    }
    record_diagnostic(evidence, "candidate_selection", candidate.diagnostic);
    if (!evidence.healthy()) {
        return {std::nullopt, "evidence_write_failed"};
    }
    if (!candidate.succeeded()) {
        return {std::nullopt, "candidate_selection_blocked"};
    }

    auto session = std::move(candidate.session);
    const auto native_identity = detail::ExplorerSessionDiagnostics::read(*session);
    {
        std::ostringstream fields;
        fields << ",\"role\":" << json_quote(role_text)
               << ",\"process\":\"explorer.exe\""
               << ",\"native_key\":" << native_identity.native_key
               << ",\"pid\":" << native_identity.process_id
               << ",\"tid\":" << native_identity.thread_id
               << ",\"logical_id\":" << session->token().logical_id()
               << ",\"capability_generation\":"
               << session->token().generation()
               << ",\"consent_generation\":"
               << session->token().consent_generation()
               << ",\"raw_ignored_evidence_only\":true";
        if (!evidence.record("native_target_identity", fields.str())) {
            return {std::nullopt, "evidence_write_failed"};
        }
    }
    ProvisionedTarget result{std::move(session),
                             candidate.facts,
                             native_identity};
    return {std::move(result), {}};
}

void append_optional_snapshot(
    std::ostringstream& output,
    const std::optional<explorer::ExplorerWindowSnapshot>& snapshot) {
    if (snapshot.has_value()) {
        append_snapshot(output, *snapshot);
    } else {
        output << "null";
    }
}

[[nodiscard]] bool record_step(
    EvidenceLog& evidence,
    const std::string_view name,
    const explorer::ExplorerGlueStepResult& step) {
    std::ostringstream fields;
    fields << ",\"step\":" << json_quote(name)
           << ",\"result\":"
           << json_quote(step.succeeded() ? "PASS" : "BLOCKED")
           << ",\"reason\":" << json_quote(glue_reason_name(step.reason))
           << ",\"stage\":" << json_quote(glue_stage_name(step.stage));
    const bool recorded = evidence.record("glue_step", fields.str());
    record_diagnostic(evidence, name, step.diagnostic);
    return recorded && evidence.healthy();
}

[[nodiscard]] bool record_trace(
    EvidenceLog& evidence,
    const std::vector<explorer::ExplorerGlueTraceRecord>& trace) {
    for (const auto& item : trace) {
        std::ostringstream fields;
        fields << ",\"trace_sequence\":" << item.trace_sequence
               << ",\"glue_session_generation\":"
               << item.glue_session_generation
               << ",\"event_sequence\":" << item.event_sequence
               << ",\"role\":" << json_quote(role_name(item.role))
               << ",\"event_kind\":";
        if (item.event_kind.has_value()) {
            fields << json_quote(behavior_event_name(*item.event_kind));
        } else {
            fields << "null";
        }
        fields << ",\"decision\":";
        if (item.decision_kind.has_value()) {
            fields << json_quote(decision_name(*item.decision_kind));
        } else {
            fields << "null";
        }
        fields << ",\"abort_reason\":";
        if (item.abort_reason.has_value()) {
            fields << json_quote(abort_reason_name(*item.abort_reason));
        } else {
            fields << "null";
        }
        fields << ",\"behavior_operation_generation\":"
               << item.behavior_operation_generation
               << ",\"visible\":";
        if (item.visible_rect.has_value()) {
            append_rect(fields, *item.visible_rect);
        } else {
            fields << "null";
        }
        if (!evidence.record("internal_trace", fields.str())) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool record_operations(
    EvidenceLog& evidence,
    const std::vector<explorer::ExplorerGlueOperationRecord>& operations) {
    for (const auto& item : operations) {
        const auto& operation = item.result;
        std::ostringstream fields;
        fields << ",\"phase\":" << json_quote(operation_phase_name(item.phase))
               << ",\"role\":" << json_quote(role_name(item.role))
               << ",\"behavior_operation_generation\":"
               << item.behavior_operation_generation
               << ",\"source_leader_sequence\":"
               << item.source_leader_sequence
               << ",\"operation_id\":" << operation.operation_id
               << ",\"reason_code\":"
               << static_cast<unsigned>(operation.reason)
               << ",\"stage_code\":"
               << static_cast<unsigned>(operation.stage)
               << ",\"native_apply_attempted\":"
               << json_bool(operation.native_apply_attempted)
               << ",\"native_outcome_known\":"
               << json_bool(operation.native_outcome_known)
               << ",\"cleanup_operation\":"
               << json_bool(operation.cleanup_operation)
               << ",\"exact_receipt\":" << json_bool(exact_operation(operation));
        if (operation.receipt.has_value()) {
            const auto& receipt = *operation.receipt;
            fields << ",\"before\":";
            append_optional_snapshot(fields, receipt.before);
            fields << ",\"requested_visible\":";
            append_rect(fields, receipt.requested_visible_rect);
            fields << ",\"requested_positioning\":";
            if (receipt.requested_positioning_rect.has_value()) {
                append_rect(fields, *receipt.requested_positioning_rect);
            } else {
                fields << "null";
            }
            fields << ",\"actual\":";
            append_optional_snapshot(fields, receipt.actual);
            fields << ",\"size_preserved\":"
                   << json_bool(receipt.size_preserved)
                   << ",\"identity_stable\":"
                   << json_bool(receipt.identity_stable)
                   << ",\"location_stable\":"
                   << json_bool(receipt.location_stable)
                   << ",\"monitor_and_dpi_stable\":"
                   << json_bool(receipt.monitor_and_dpi_stable);
        }
        if (!evidence.record("operation", fields.str())) {
            return false;
        }
        record_diagnostic(evidence, "operation", operation.diagnostic);
        if (!evidence.healthy()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool record_facts(EvidenceLog& evidence,
                                const explorer::ExplorerGlueFacts& facts) {
    std::ostringstream fields;
    fields << ",\"glue_authority_id\":" << facts.glue_authority_id
           << ",\"glue_session_generation\":"
           << facts.glue_session_generation
           << ",\"pair_preview_generation\":"
           << facts.consent_generations.pair_preview_generation
           << ",\"prompt_generation\":"
           << facts.consent_generations.prompt_generation
           << ",\"confirmation_generation\":"
           << facts.consent_generations.confirmation_generation
           << ",\"authority_generation\":"
           << facts.consent_generations.authority_generation
           << ",\"leader_target_consent_prefix_valid\":"
           << json_bool(facts.leader_target_consent_prefix_valid)
           << ",\"follower_target_consent_prefix_valid\":"
           << json_bool(facts.follower_target_consent_prefix_valid)
           << ",\"follower_baseline_excluded_leader\":"
           << json_bool(facts.follower_baseline_excluded_leader)
           << ",\"pair_distinct\":" << json_bool(facts.pair_distinct)
           << ",\"same_monitor_and_dpi\":"
           << json_bool(facts.same_monitor_and_dpi)
           << ",\"glue_consent_confirmed\":"
           << json_bool(facts.glue_consent_confirmed)
           << ",\"test_layout_planned\":"
           << json_bool(facts.test_layout_planned)
           << ",\"test_layout_exact\":"
           << json_bool(facts.test_layout_exact)
           << ",\"topology_exact_two_window_component\":"
           << json_bool(facts.topology_exact_two_window_component)
           << ",\"event_source_armed\":"
           << json_bool(facts.event_source_armed)
           << ",\"event_source_stopped\":"
           << json_bool(facts.event_source_stopped)
           << ",\"event_source_lifecycle_clean\":"
           << json_bool(facts.event_source_lifecycle_clean)
           << ",\"leader_restore_attempted\":"
           << json_bool(facts.leader_restore_attempted)
           << ",\"follower_restore_attempted\":"
           << json_bool(facts.follower_restore_attempted)
           << ",\"leader_restored_exact\":"
           << json_bool(facts.leader_restored_exact)
           << ",\"follower_restored_exact\":"
           << json_bool(facts.follower_restored_exact)
           << ",\"user_windows_close_attempted\":"
           << json_bool(facts.user_windows_close_attempted)
           << ",\"follower_native_apply_count\":"
           << facts.follower_native_apply_count
           << ",\"follower_noop_count\":" << facts.follower_noop_count
           << ",\"pending_capacity\":" << facts.pending_capacity
           << ",\"max_pending_depth\":" << facts.max_pending_depth
           << ",\"event_queue_capacity\":" << facts.event_queue_capacity
           << ",\"max_event_queue_depth\":"
           << facts.max_event_queue_depth
           << ",\"accepted_event_count\":" << facts.accepted_event_count
           << ",\"ignored_event_count\":" << facts.ignored_event_count
           << ",\"suppressed_feedback_count\":"
           << facts.suppressed_feedback_count
           << ",\"duplicate_feedback_count\":"
           << facts.duplicate_feedback_count
           << ",\"missing_feedback_count\":"
           << facts.missing_feedback_count
           << ",\"reconciled_feedback_count\":"
           << facts.reconciled_feedback_count
           << ",\"unexpected_feedback_count\":"
           << facts.unexpected_feedback_count
           << ",\"behavior_state\":"
           << json_quote(behavior_state_name(facts.behavior_state))
           << ",\"behavior_abort_reason\":";
    if (facts.behavior_abort_reason.has_value()) {
        fields << json_quote(abort_reason_name(*facts.behavior_abort_reason));
    } else {
        fields << "null";
    }
    fields << ",\"leader_original\":";
    append_optional_snapshot(fields, facts.leader_original);
    fields << ",\"follower_original\":";
    append_optional_snapshot(fields, facts.follower_original);
    fields << ",\"leader_layout\":";
    append_optional_snapshot(fields, facts.leader_layout);
    fields << ",\"follower_layout\":";
    append_optional_snapshot(fields, facts.follower_layout);
    fields << ",\"leader_final\":";
    append_optional_snapshot(fields, facts.leader_final);
    fields << ",\"follower_final\":";
    append_optional_snapshot(fields, facts.follower_final);
    fields << ",\"leader_restored\":";
    append_optional_snapshot(fields, facts.leader_restored);
    fields << ",\"follower_restored\":";
    append_optional_snapshot(fields, facts.follower_restored);
    return evidence.record("facts", fields.str());
}

struct RunOutcome final {
    bool runtime_pass{};
    std::string reason{"not_started"};
    explorer::ExplorerGlueReason glue_reason{
        explorer::ExplorerGlueReason::InvalidArgument};
    explorer::ExplorerGlueStage glue_stage{
        explorer::ExplorerGlueStage::PairValidation};
    explorer::ExplorerGlueFacts facts;
    bool facts_available{};
    bool all_active_follower_operations_exact{};
    std::size_t active_follower_operation_count{};
    std::size_t leader_start_count{};
    std::size_t leader_location_count{};
    std::size_t leader_end_count{};
    std::size_t follower_feedback_count{};
    std::size_t recursive_follower_operation_count{};
    std::size_t acknowledged_operation_count{};
    std::size_t reconciled_operation_count{};
    std::size_t duplicate_feedback_trace_count{};
    bool feedback_operation_correlation_valid{};
    bool trace_generation_valid{};
};

struct FeedbackCorrelationSummary final {
    bool valid{true};
    std::size_t acknowledged_count{};
    std::size_t reconciled_count{};
};

[[nodiscard]] FeedbackCorrelationSummary record_feedback_correlations(
    EvidenceLog& evidence,
    const std::vector<explorer::ExplorerGlueTraceRecord>& trace,
    const std::vector<explorer::ExplorerGlueOperationRecord>& operations,
    const explorer::ExplorerGlueFacts& facts) {
    FeedbackCorrelationSummary summary;
    std::vector<bool> feedback_used(trace.size(), false);
    for (const auto& operation_record : operations) {
        if (operation_record.phase !=
            explorer::ExplorerGlueOperationPhase::ActiveFollower) {
            continue;
        }
        const auto& operation = operation_record.result;
        const bool receipt_exact = exact_operation(operation);
        std::optional<geometry::Rect> expected;
        std::optional<geometry::Rect> actual;
        if (operation.receipt.has_value()) {
            expected = operation.receipt->requested_visible_rect;
            if (operation.receipt->actual.has_value()) {
                actual = operation.receipt->actual->visible_rect;
            }
        }

        std::size_t command_matches = 0U;
        if (expected.has_value()) {
            for (const auto& item : trace) {
                if (item.role == explorer::ExplorerGlueWindowRole::Leader &&
                    item.event_sequence ==
                        operation_record.source_leader_sequence &&
                    item.decision_kind ==
                        behavior::GlueDecisionKind::FollowerMoveRequested &&
                    item.behavior_operation_generation ==
                        operation_record.behavior_operation_generation &&
                    item.visible_rect == expected) {
                    ++command_matches;
                }
            }
        }

        std::optional<std::uint64_t> feedback_sequence;
        if (expected.has_value()) {
            for (std::size_t index = 0U; index < trace.size(); ++index) {
                const auto& item = trace[index];
                if (!feedback_used[index] && item.event_sequence != 0U &&
                    item.event_sequence >
                        operation_record.source_leader_sequence &&
                    item.role == explorer::ExplorerGlueWindowRole::Follower &&
                    item.event_kind == behavior::GlueEventKind::GeometryChanged &&
                    item.visible_rect == expected &&
                    (item.decision_kind ==
                         behavior::GlueDecisionKind::FeedbackAcknowledged ||
                     item.decision_kind == behavior::GlueDecisionKind::
                                               FeedbackObservedPendingResult)) {
                    feedback_used[index] = true;
                    feedback_sequence = item.event_sequence;
                    break;
                }
            }
        }

        const bool acknowledged = feedback_sequence.has_value();
        const bool reconciliation_supported =
            acknowledged ||
            (receipt_exact && facts.leader_final.has_value() &&
             facts.follower_final.has_value());
        summary.valid = summary.valid && receipt_exact &&
                        command_matches == 1U && reconciliation_supported &&
                        operation_record.behavior_operation_generation != 0U &&
                        operation_record.source_leader_sequence != 0U;
        if (acknowledged) {
            ++summary.acknowledged_count;
        } else {
            ++summary.reconciled_count;
        }

        std::ostringstream fields;
        fields << ",\"operation_generation\":"
               << operation_record.behavior_operation_generation
               << ",\"source_leader_sequence\":"
               << operation_record.source_leader_sequence
               << ",\"command_trace_match_count\":" << command_matches
               << ",\"exact_operation_receipt\":"
               << json_bool(receipt_exact)
               << ",\"disposition\":"
               << json_quote(acknowledged
                                 ? "acknowledged_self_feedback"
                                 : "reconciled_by_operation_receipt_and_final_snapshot")
               << ",\"feedback_event_sequence\":";
        if (feedback_sequence.has_value()) {
            fields << *feedback_sequence;
        } else {
            fields << "null";
        }
        fields << ",\"expected_visible\":";
        if (expected.has_value()) {
            append_rect(fields, *expected);
        } else {
            fields << "null";
        }
        fields << ",\"actual_visible\":";
        if (actual.has_value()) {
            append_rect(fields, *actual);
        } else {
            fields << "null";
        }
        if (!evidence.record("feedback_reconciliation", fields.str())) {
            summary.valid = false;
            return summary;
        }
    }
    return summary;
}

[[nodiscard]] RunOutcome run_interactive(const Options& options,
                                         EvidenceLog& evidence) {
    RunOutcome outcome;
    const auto repository_root = validated_repository_root();
    if (!repository_root.has_value()) {
        outcome.reason = "repository_root_unavailable";
        return outcome;
    }
    const auto leader_path =
        create_target_directory(*repository_root, "leader");
    const auto follower_path =
        create_target_directory(*repository_root, "follower");
    if (!leader_path.has_value() || !follower_path.has_value()) {
        outcome.reason = "nonce_directory_unavailable";
        return outcome;
    }
    {
        std::ostringstream fields;
        fields << ",\"role\":\"leader\",\"target_id\":"
               << json_quote(utf8(leader_path->filename().native()))
               << ",\"created_empty\":true,\"full_path_redacted\":true";
        if (!evidence.record("nonce_target", fields.str())) {
            outcome.reason = "evidence_write_failed";
            return outcome;
        }
    }
    {
        std::ostringstream fields;
        fields << ",\"role\":\"follower\",\"target_id\":"
               << json_quote(utf8(follower_path->filename().native()))
               << ",\"created_empty\":true,\"full_path_redacted\":true";
        if (!evidence.record("nonce_target", fields.str())) {
            outcome.reason = "evidence_write_failed";
            return outcome;
        }
    }

    auto leader_attempt = provision_target(
        evidence, explorer::ExplorerGlueWindowRole::Leader, *leader_path);
    if (!leader_attempt.target.has_value()) {
        outcome.reason = std::move(leader_attempt.failure_reason);
        static_cast<void>(write_console_text(
            L"Leader 未能通过唯一新窗口授权；本次运行停止，未控制任何窗口。\n"));
        return outcome;
    }
    ProvisionedTarget leader = std::move(*leader_attempt.target);

    // This begin occurs only while the Leader session remains alive. The
    // resulting Follower baseline therefore permanently forbids the Leader
    // HWND as a preexisting candidate.
    auto follower_attempt = provision_target(
        evidence, explorer::ExplorerGlueWindowRole::Follower, *follower_path);
    if (!follower_attempt.target.has_value()) {
        outcome.reason = std::move(follower_attempt.failure_reason);
        static_cast<void>(write_console_text(
            L"Follower 未能通过唯一新窗口授权；本次运行停止，未移动窗口。\n"
            L"请自行关闭两个测试 Explorer。\n"));
        return outcome;
    }
    ProvisionedTarget follower = std::move(*follower_attempt.target);

    const auto leader_native = leader.native_identity;
    const auto follower_native = follower.native_identity;
    auto glue_begin = explorer::ExplorerGlueConsent::begin(
        std::move(leader.session), std::move(follower.session));
    outcome.facts = glue_begin.facts;
    outcome.facts_available = true;
    outcome.glue_reason = glue_begin.reason;
    {
        std::ostringstream fields;
        fields << ",\"result\":"
               << json_quote(glue_begin.succeeded() ? "PASS" : "BLOCKED")
               << ",\"reason\":"
               << json_quote(glue_reason_name(glue_begin.reason))
               << ",\"leader_target_consent_prefix_valid\":"
               << json_bool(
                      glue_begin.facts.leader_target_consent_prefix_valid)
               << ",\"follower_target_consent_prefix_valid\":"
               << json_bool(
                      glue_begin.facts.follower_target_consent_prefix_valid)
               << ",\"follower_baseline_excluded_leader\":"
               << json_bool(glue_begin.facts.follower_baseline_excluded_leader)
               << ",\"pair_distinct\":"
               << json_bool(glue_begin.facts.pair_distinct)
               << ",\"same_monitor_and_dpi\":"
               << json_bool(glue_begin.facts.same_monitor_and_dpi)
               << ",\"test_layout_planned\":"
               << json_bool(glue_begin.facts.test_layout_planned)
               << ",\"leader_original\":";
        append_optional_snapshot(fields, glue_begin.facts.leader_original);
        fields << ",\"follower_original\":";
        append_optional_snapshot(fields, glue_begin.facts.follower_original);
        if (!evidence.record("pair_validation", fields.str())) {
            outcome.reason = "evidence_write_failed";
            return outcome;
        }
    }
    record_diagnostic(evidence, "pair_validation", glue_begin.diagnostic);
    if (!evidence.healthy()) {
        outcome.reason = "evidence_write_failed";
        return outcome;
    }
    if (!glue_begin.succeeded()) {
        outcome.reason = "pair_validation_blocked";
        static_cast<void>(write_console_text(
            L"两个目标无法形成安全的同 monitor/DPI 零间隙测试布局。\n"
            L"请自行关闭测试 Explorer；本次没有移动窗口。\n"));
        return outcome;
    }

    auto consent = std::move(glue_begin.consent);
    const auto glue_prompt = consent->record_glue_prompt();
    if (!record_step(evidence, "glue_consent_prompt", glue_prompt)) {
        outcome.reason = "evidence_write_failed";
        return outcome;
    }
    if (!glue_prompt.succeeded()) {
        outcome.reason = "glue_prompt_blocked";
        return outcome;
    }
    {
        std::ostringstream fields;
        fields << ",\"result\":\"READY\",\"generation\":"
               << consent->facts().consent_generations.prompt_generation
               << ",\"input_source\":\"interactive_console\"";
        if (!evidence.record("glue_consent_prompt", fields.str())) {
            outcome.reason = "evidence_write_failed";
            return outcome;
        }
    }
    if (!emit_glue_prompt()) {
        outcome.reason = "console_output_failed";
        return outcome;
    }
    const auto glue_line = read_console_line();
    const bool glue_confirmed =
        glue_line.status == ConsoleLineStatus::Read &&
        (glue_line.line == L"Y" || glue_line.line == L"y");
    {
        std::ostringstream fields;
        fields << ",\"result\":"
               << json_quote(glue_confirmed ? "CONFIRMED" : "DECLINED")
               << ",\"input_source\":\"interactive_console\""
               << ",\"native_apply_attempted\":false";
        if (!evidence.record("glue_consent_confirmation", fields.str())) {
            outcome.reason = "evidence_write_failed";
            return outcome;
        }
    }
    if (!glue_confirmed) {
        outcome.reason = "glue_consent_declined";
        static_cast<void>(write_console_text(
            L"未收到 Y + ENTER 授权；窗口没有被移动。请自行关闭测试 Explorer。\n"));
        return outcome;
    }

    auto authorized = consent->confirm_user_glue();
    outcome.facts = authorized.facts;
    outcome.glue_reason = authorized.reason;
    {
        std::ostringstream fields;
        fields << ",\"result\":"
               << json_quote(authorized.succeeded() ? "PASS" : "BLOCKED")
               << ",\"reason\":"
               << json_quote(glue_reason_name(authorized.reason))
               << ",\"pair_preview_generation\":"
               << authorized.facts.consent_generations.pair_preview_generation
               << ",\"prompt_generation\":"
               << authorized.facts.consent_generations.prompt_generation
               << ",\"confirmation_generation\":"
               << authorized.facts.consent_generations.confirmation_generation
               << ",\"authority_generation\":"
               << authorized.facts.consent_generations.authority_generation
               << ",\"glue_authority_id\":"
               << authorized.facts.glue_authority_id;
        if (!evidence.record("glue_authority", fields.str())) {
            outcome.reason = "evidence_write_failed";
            return outcome;
        }
    }
    record_diagnostic(evidence, "glue_authority", authorized.diagnostic);
    if (!evidence.healthy()) {
        outcome.reason = "evidence_write_failed";
        return outcome;
    }
    if (!authorized.succeeded()) {
        outcome.reason = "glue_authority_blocked";
        return outcome;
    }

    auto session = std::move(authorized.session);
    const auto glue_native = detail::ExplorerGlueSessionDiagnostics::read(*session);
    {
        std::ostringstream fields;
        fields << ",\"leader_native_key\":" << leader_native.native_key
               << ",\"leader_pid\":" << leader_native.process_id
               << ",\"leader_tid\":" << leader_native.thread_id
               << ",\"follower_native_key\":" << follower_native.native_key
               << ",\"follower_pid\":" << follower_native.process_id
               << ",\"follower_tid\":" << follower_native.thread_id
               << ",\"session_bindings_present\":"
               << json_bool(glue_native.leader.has_value() &&
                            glue_native.follower.has_value())
               << ",\"raw_ignored_evidence_only\":true";
        if (!evidence.record("glue_native_bindings", fields.str())) {
            static_cast<void>(session->cancel_and_restore());
            outcome.reason = "evidence_write_failed";
            return outcome;
        }
    }

    const auto setup = session->setup_test_layout();
    if (!record_step(evidence, "setup_test_layout", setup)) {
        static_cast<void>(session->cancel_and_restore());
        outcome.reason = "evidence_write_failed";
        outcome.facts = session->facts();
        return outcome;
    }
    if (!setup.succeeded()) {
        outcome.reason = "setup_test_layout_blocked";
        outcome.glue_reason = setup.reason;
        outcome.glue_stage = setup.stage;
        outcome.facts = session->facts();
        static_cast<void>(record_operations(evidence, session->operations()));
        static_cast<void>(record_facts(evidence, outcome.facts));
        return outcome;
    }

    const auto armed = session->arm();
    if (!record_step(evidence, "arm_event_source", armed)) {
        static_cast<void>(session->cancel_and_restore());
        outcome.reason = "evidence_write_failed";
        outcome.facts = session->facts();
        return outcome;
    }
    if (!armed.succeeded()) {
        outcome.reason = "event_source_arm_blocked";
        outcome.glue_reason = armed.reason;
        outcome.glue_stage = armed.stage;
        outcome.facts = session->facts();
        static_cast<void>(record_operations(evidence, session->operations()));
        static_cast<void>(record_facts(evidence, outcome.facts));
        return outcome;
    }
    {
        std::ostringstream fields;
        fields << ",\"role\":\"leader\",\"timeout_seconds\":"
               << options.timeout_seconds
               << ",\"console_input_after_arm\":false";
        if (!evidence.record("drag_prompt", fields.str()) ||
            !emit_drag_prompt(options.timeout_seconds)) {
            static_cast<void>(session->cancel_and_restore());
            outcome.reason = "drag_prompt_failed";
            outcome.facts = session->facts();
            return outcome;
        }
    }

    const auto terminal = session->run_until_terminal(
        std::chrono::seconds{options.timeout_seconds});
    outcome.glue_reason = terminal.reason;
    outcome.glue_stage = terminal.stage;
    outcome.facts = session->facts();
    if (!record_step(evidence, "run_until_terminal", terminal) ||
        !record_trace(evidence, session->trace()) ||
        !record_operations(evidence, session->operations())) {
        outcome.reason = "evidence_write_failed";
        return outcome;
    }

    bool all_active_exact = true;
    for (const auto& operation : session->operations()) {
        if (operation.phase == explorer::ExplorerGlueOperationPhase::ActiveFollower) {
            ++outcome.active_follower_operation_count;
            all_active_exact = all_active_exact &&
                               operation.result.native_apply_attempted &&
                               exact_operation(operation.result);
        }
    }
    outcome.all_active_follower_operations_exact = all_active_exact;
    outcome.trace_generation_valid =
        outcome.facts.glue_session_generation != 0U;
    for (const auto& item : session->trace()) {
        outcome.trace_generation_valid =
            outcome.trace_generation_valid &&
            item.glue_session_generation ==
                outcome.facts.glue_session_generation;
        if (item.decision_kind ==
            behavior::GlueDecisionKind::DuplicateFeedbackSuppressed) {
            ++outcome.duplicate_feedback_trace_count;
        }
        if (item.event_sequence == 0U || !item.event_kind.has_value()) {
            continue;
        }
        if (item.role == explorer::ExplorerGlueWindowRole::Leader) {
            switch (*item.event_kind) {
            case behavior::GlueEventKind::MoveResizeStarted:
                ++outcome.leader_start_count;
                break;
            case behavior::GlueEventKind::GeometryChanged:
                ++outcome.leader_location_count;
                break;
            case behavior::GlueEventKind::MoveResizeEnded:
                ++outcome.leader_end_count;
                break;
            }
        } else if (*item.event_kind ==
                   behavior::GlueEventKind::GeometryChanged) {
            ++outcome.follower_feedback_count;
            if (item.decision_kind ==
                behavior::GlueDecisionKind::FollowerMoveRequested) {
                ++outcome.recursive_follower_operation_count;
            }
        }
    }

    const auto correlation = record_feedback_correlations(evidence,
                                                          session->trace(),
                                                          session->operations(),
                                                          outcome.facts);
    outcome.acknowledged_operation_count = correlation.acknowledged_count;
    outcome.reconciled_operation_count = correlation.reconciled_count;
    outcome.feedback_operation_correlation_valid = correlation.valid;
    if (!correlation.valid || !evidence.healthy() ||
        !record_facts(evidence, outcome.facts)) {
        outcome.reason = "feedback_correlation_or_evidence_failed";
        return outcome;
    }

    const auto& facts = outcome.facts;
    outcome.runtime_pass =
        terminal.succeeded() &&
        facts.behavior_state == behavior::GlueMoveState::Completed &&
        !facts.behavior_abort_reason.has_value() &&
        facts.leader_target_consent_prefix_valid &&
        facts.follower_target_consent_prefix_valid &&
        facts.follower_baseline_excluded_leader && facts.pair_distinct &&
        facts.same_monitor_and_dpi && facts.glue_consent_confirmed &&
        facts.test_layout_exact &&
        facts.topology_exact_two_window_component &&
        facts.event_source_armed && facts.event_source_stopped &&
        facts.event_source_lifecycle_clean && facts.leader_restored_exact &&
        facts.follower_restored_exact && !facts.user_windows_close_attempted &&
        outcome.leader_start_count == 1U &&
        outcome.leader_location_count >= 1U &&
        outcome.leader_end_count == 1U &&
        outcome.active_follower_operation_count >= 1U && all_active_exact &&
        facts.follower_native_apply_count ==
            outcome.active_follower_operation_count &&
        outcome.trace_generation_valid &&
        outcome.feedback_operation_correlation_valid &&
        outcome.acknowledged_operation_count +
                outcome.reconciled_operation_count ==
            outcome.active_follower_operation_count &&
        facts.suppressed_feedback_count == outcome.follower_feedback_count &&
        facts.duplicate_feedback_count ==
            outcome.duplicate_feedback_trace_count &&
        facts.missing_feedback_count == outcome.reconciled_operation_count &&
        facts.reconciled_feedback_count ==
            outcome.reconciled_operation_count &&
        outcome.recursive_follower_operation_count == 0U &&
        facts.unexpected_feedback_count == 0U;
    outcome.reason = outcome.runtime_pass ? "pass" : "runtime_gate_failed";
    return outcome;
}

[[nodiscard]] bool record_summary(EvidenceLog& evidence,
                                  const RunOutcome& outcome) {
    const bool queue_overflow =
        outcome.glue_reason == explorer::ExplorerGlueReason::EventQueueOverflow ||
        outcome.facts.behavior_abort_reason ==
            behavior::GlueAbortReason::EventQueueOverflow;
    std::ostringstream fields;
    fields << ",\"result\":"
           << json_quote(outcome.runtime_pass ? "PASS" : "BLOCKED")
           << ",\"reason\":" << json_quote(outcome.reason)
           << ",\"glue_reason\":"
           << json_quote(glue_reason_name(outcome.glue_reason))
           << ",\"glue_stage\":"
           << json_quote(glue_stage_name(outcome.glue_stage))
           << ",\"implementation_ready\":true"
           << ",\"runtime_gate\":"
           << json_quote(outcome.runtime_pass ? "PASS" : "BLOCKED")
           << ",\"behavior_state\":"
           << json_quote(behavior_state_name(outcome.facts.behavior_state))
           << ",\"leader_start_count\":" << outcome.leader_start_count
           << ",\"leader_location_count\":"
           << outcome.leader_location_count
           << ",\"leader_end_count\":" << outcome.leader_end_count
           << ",\"follower_feedback_count\":"
           << outcome.follower_feedback_count
           << ",\"follower_native_apply_count\":"
           << outcome.facts.follower_native_apply_count
           << ",\"active_follower_operation_count\":"
           << outcome.active_follower_operation_count
           << ",\"follower_noop_count\":"
           << outcome.facts.follower_noop_count
           << ",\"suppressed_feedback_count\":"
           << outcome.facts.suppressed_feedback_count
           << ",\"duplicate_feedback_count\":"
           << outcome.facts.duplicate_feedback_count
           << ",\"missing_feedback_count\":"
           << outcome.facts.missing_feedback_count
           << ",\"reconciled_feedback_count\":"
           << outcome.facts.reconciled_feedback_count
           << ",\"acknowledged_operation_count\":"
           << outcome.acknowledged_operation_count
           << ",\"reconciled_operation_count\":"
           << outcome.reconciled_operation_count
           << ",\"feedback_operation_correlation_valid\":"
           << json_bool(outcome.feedback_operation_correlation_valid)
           << ",\"trace_generation_valid\":"
           << json_bool(outcome.trace_generation_valid)
           << ",\"feedback_suppression_evidence\":"
           << json_quote(outcome.follower_feedback_count != 0U
                             ? "observed_and_suppressed"
                             : "no_feedback_event_reconciled")
           << ",\"unexpected_feedback_count\":"
           << outcome.facts.unexpected_feedback_count
           << ",\"recursive_follower_operation_count\":"
           << outcome.recursive_follower_operation_count
           << ",\"all_active_follower_operations_exact\":"
           << json_bool(outcome.all_active_follower_operations_exact)
           << ",\"queue_overflow\":" << json_bool(queue_overflow)
           << ",\"max_event_queue_depth\":"
           << outcome.facts.max_event_queue_depth
           << ",\"max_pending_depth\":"
           << outcome.facts.max_pending_depth
           << ",\"event_source_armed\":"
           << json_bool(outcome.facts.event_source_armed)
           << ",\"event_source_stopped\":"
           << json_bool(outcome.facts.event_source_stopped)
           << ",\"event_source_lifecycle_clean\":"
           << json_bool(outcome.facts.event_source_lifecycle_clean)
           << ",\"topology_frozen_exact_pair\":"
           << json_bool(outcome.facts.topology_exact_two_window_component)
           << ",\"leader_restored_exact\":"
           << json_bool(outcome.facts.leader_restored_exact)
           << ",\"follower_restored_exact\":"
           << json_bool(outcome.facts.follower_restored_exact)
           << ",\"user_preexisting_windows_touched\":false"
           << ",\"other_third_party_control\":false"
           << ",\"global_input_control\":false"
           << ",\"user_windows_close_attempted\":"
           << json_bool(outcome.facts.user_windows_close_attempted)
           << ",\"r0_observer_runtime_dependency\":false"
           << ",\"r0_observer_semantics_changed\":false";
    return evidence.record("summary", fields.str());
}

} // namespace

int wmain(const int argc, wchar_t* argv[]) {
    const auto options = parse_options(argc, argv);
    if (!options.has_value()) {
        print_usage();
        return EXIT_FAILURE;
    }
    if (options->help) {
        print_usage();
        return EXIT_SUCCESS;
    }
    if (!is_interactive_console_handle(GetStdHandle(STD_INPUT_HANDLE)) ||
        !is_interactive_console_handle(GetStdHandle(STD_OUTPUT_HANDLE))) {
        std::wcerr
            << L"交互授权必须使用继承的真实控制台输入/输出；不接受管道或重定向。\n";
        return EXIT_FAILURE;
    }

    EvidenceLog evidence;
    if (!evidence.open_new(*options->evidence_log)) {
        static_cast<void>(write_console_text(
            L"无法用 CREATE_NEW 创建独立 evidence JSONL；测试不会继续。\n"));
        return EXIT_FAILURE;
    }
    {
        std::ostringstream fields;
        fields << ",\"mode\":\"interactive_consent_test\""
               << ",\"input_source\":\"interactive_console\""
               << ",\"timeout_seconds\":" << options->timeout_seconds
               << ",\"target_window_count\":2"
               << ",\"r0_observer_runtime_dependency\":false"
               << ",\"synthetic_input\":false";
        if (!evidence.record("startup", fields.str())) {
            return EXIT_FAILURE;
        }
    }

    RunOutcome outcome;
    try {
        outcome = run_interactive(*options, evidence);
    } catch (const std::exception&) {
        outcome.runtime_pass = false;
        outcome.reason = "unhandled_exception";
    } catch (...) {
        outcome.runtime_pass = false;
        outcome.reason = "unknown_exception";
    }

    if (!evidence.healthy() || !record_summary(evidence, outcome) ||
        !evidence.record("shutdown", ",\"disposition\":\"complete\"")) {
        static_cast<void>(write_console_text(
            L"Evidence 写入失败；本次结果不能作为验证证据。\n"));
        return EXIT_FAILURE;
    }

    if (outcome.runtime_pass) {
        static_cast<void>(write_console_text(
            L"\nGlue Move session 已完成，两个测试 Explorer 已精确恢复原位置。\n"
            L"PaneBind 没有关闭窗口。若本程序由 evidence runner 启动，请先等待 runner 完成外部 Observer 校验，再自行关闭 Leader 和 Follower。\n"));
        return EXIT_SUCCESS;
    }

    if (outcome.facts_available && outcome.facts.leader_restored_exact &&
        outcome.facts.follower_restored_exact) {
        static_cast<void>(write_console_text(
            L"\nGlue session 未通过，但两个测试 Explorer 已恢复原位置。若由 evidence runner 启动，请等待其完成审计后再自行关闭。\n"));
    } else {
        static_cast<void>(write_console_text(
            L"\n本次 Glue 验证被阻断或已安全中止。请检查 evidence，并自行处理测试 Explorer；PaneBind 不会关闭它们。\n"));
    }
    return EXIT_FAILURE;
}
