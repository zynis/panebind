#include "platform/windows/explorer/explorer_session.h"
#include "platform/windows/explorer/explorer_session_internal.h"
#include "platform/windows/explorer/explorer_shell_inventory.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace {

namespace explorer = panebind::platform::windows::explorer;
namespace detail = panebind::platform::windows::explorer::detail;
namespace geometry = panebind::core::geometry;

constexpr std::uint32_t kMaximumHoldSeconds = 86'400U;
constexpr std::size_t kNonceAttempts = 8U;

enum class HarnessMode {
    SelfTest,
    ProvisionOnly,
};

struct Options {
    HarnessMode mode{HarnessMode::SelfTest};
    std::uint32_t hold_seconds{2U};
    std::size_t attempt_index{};
    bool help{};
};

struct TargetDirectory {
    std::filesystem::path path;
    std::filesystem::path evidence_root;
    explorer::FilesystemLocationIdentity identity;
};

struct DirectoryCleanupResult {
    bool inventory_complete{};
    bool inventory_proved_unused{};
    bool containment_valid{};
    bool plain_directory{};
    bool identity_stable{};
    bool directory_empty{};
    bool removed{};
    int filesystem_error{};
};

class ComApartment final {
public:
    ComApartment() noexcept
        : result_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED |
                                              COINIT_DISABLE_OLE1DDE)) {}

    ~ComApartment() {
        if (succeeded()) {
            CoUninitialize();
        }
    }

    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;

    [[nodiscard]] bool succeeded() const noexcept {
        return result_ == S_OK || result_ == S_FALSE;
    }

    [[nodiscard]] HRESULT result() const noexcept { return result_; }

private:
    HRESULT result_{};
};

[[nodiscard]] std::string_view reason_name(
    const explorer::ExplorerEligibilityReason reason) noexcept {
    using enum explorer::ExplorerEligibilityReason;
    switch (reason) {
    case Eligible:
        return "eligible";
    case InvalidTargetDirectory:
        return "invalid_target_directory";
    case TargetDirectoryNotEmpty:
        return "target_directory_not_empty";
    case ComApartmentUnavailable:
        return "com_apartment_unavailable";
    case InventoryUnavailable:
        return "inventory_unavailable";
    case InventoryUnstable:
        return "inventory_unstable";
    case BaselineWindowIdentityUnavailable:
        return "baseline_window_identity_unavailable";
    case ShellEventSubscriptionUnavailable:
        return "shell_event_subscription_unavailable";
    case BrowserEventSubscriptionUnavailable:
        return "browser_event_subscription_unavailable";
    case ShellEventStreamInvalid:
        return "shell_event_stream_invalid";
    case RegistrationNotObserved:
        return "registration_not_observed";
    case RegistrationResolutionFailed:
        return "registration_resolution_failed";
    case RegistrationRevoked:
        return "registration_revoked";
    case CanonicalIdentityMismatch:
        return "canonical_identity_mismatch";
    case SubscriptionGenerationMismatch:
        return "subscription_generation_mismatch";
    case ShellWindowCreationFailed:
        return "shell_window_creation_failed";
    case ShellWindowHandleMissing:
        return "shell_window_handle_missing";
    case PreexistingWindow:
        return "preexisting_window";
    case ReusedExistingWindow:
        return "reused_existing_window";
    case AmbiguousCandidate:
        return "ambiguous_candidate";
    case BaselineChanged:
        return "baseline_changed";
    case LocationNotReady:
        return "location_not_ready";
    case LocationMismatch:
        return "location_mismatch";
    case WindowDestroyed:
        return "window_destroyed";
    case ProcessOpenFailed:
        return "process_open_failed";
    case ProcessExited:
        return "process_exited";
    case WrongProcess:
        return "wrong_process";
    case WrongThread:
        return "wrong_thread";
    case WrongImage:
        return "wrong_image";
    case WrongClass:
        return "wrong_class";
    case NotTopLevel:
        return "not_top_level";
    case ChildWindow:
        return "child_window";
    case OwnedWindow:
        return "owned_window";
    case Invisible:
        return "invisible";
    case Cloaked:
        return "cloaked";
    case Minimized:
        return "minimized";
    case Maximized:
        return "maximized";
    case WrongVirtualDesktop:
        return "wrong_virtual_desktop";
    case SecurityQueryFailed:
        return "security_query_failed";
    case UserMismatch:
        return "user_mismatch";
    case SessionMismatch:
        return "session_mismatch";
    case IntegrityMismatch:
        return "integrity_mismatch";
    case Elevated:
        return "elevated";
    case UiAccess:
        return "ui_access";
    case AppContainer:
        return "app_container";
    case GeometryCaptureFailed:
        return "geometry_capture_failed";
    case DpiContextMismatch:
        return "dpi_context_mismatch";
    case MonitorUnavailable:
        return "monitor_unavailable";
    case MonitorChanged:
        return "monitor_changed";
    case DpiChanged:
        return "dpi_changed";
    case UnsafeDelta:
        return "unsafe_delta";
    case OperationLimitReached:
        return "operation_limit_reached";
    case OperationSequenceViolation:
        return "operation_sequence_violation";
    case StaleToken:
        return "stale_token";
    case AuthorityExhausted:
        return "authority_exhausted";
    case GenerationExhausted:
        return "generation_exhausted";
    case EmptyGeometry:
        return "empty_geometry";
    case ResizeRejected:
        return "resize_rejected";
    case ArithmeticOverflow:
        return "arithmetic_overflow";
    case NativeCoordinateOutOfRange:
        return "native_coordinate_out_of_range";
    case NativeApplyFailed:
        return "native_apply_failed";
    case PostVerificationFailed:
        return "post_verification_failed";
    case TargetInvalidated:
        return "target_invalidated";
    case SafeCleanupNotPerformed:
        return "safe_cleanup_not_performed";
    }
    return "unknown";
}

[[nodiscard]] std::string_view stage_name(
    const explorer::ExplorerOperationStage stage) noexcept {
    using enum explorer::ExplorerOperationStage;
    switch (stage) {
    case BaselineInventory:
        return "baseline_inventory";
    case ShellWindowCreation:
        return "shell_window_creation";
    case Navigation:
        return "navigation";
    case CandidateResolution:
        return "candidate_resolution";
    case Eligibility:
        return "eligibility";
    case Preflight:
        return "preflight";
    case NativeApply:
        return "native_apply";
    case PostVerification:
        return "post_verification";
    case Restore:
        return "restore";
    case Cleanup:
        return "cleanup";
    }
    return "unknown";
}

void append_rect(std::ostream& output, const geometry::Rect& rect) {
    output << "{\"left\":" << rect.left() << ",\"top\":" << rect.top()
           << ",\"right\":" << rect.right() << ",\"bottom\":" << rect.bottom()
           << '}';
}

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
            result.push_back(character < 0x20U ? '?'
                                               : static_cast<char>(character));
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
        result.push_back(character <= 0x7f ? static_cast<char>(character)
                                           : '?');
    }
    return result;
}

[[nodiscard]] std::string utc_timestamp() {
    SYSTEMTIME timestamp{};
    GetSystemTime(&timestamp);
    std::array<char, 32U> buffer{};
    const int length = sprintf_s(
        buffer.data(), buffer.size(),
        "%04hu-%02hu-%02huT%02hu:%02hu:%02hu.%03huZ", timestamp.wYear,
        timestamp.wMonth, timestamp.wDay, timestamp.wHour, timestamp.wMinute,
        timestamp.wSecond, timestamp.wMilliseconds);
    return length > 0
               ? std::string{buffer.data(), static_cast<std::size_t>(length)}
               : std::string{"timestamp_unavailable"};
}

void emit_test(const std::string_view name, const std::string_view result,
               const std::optional<explorer::ExplorerEligibilityReason> reason =
                   std::nullopt) {
    std::cout << "{\"schema_version\":1,\"record_kind\":\"test\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"name\":\"" << name << '"' << ",\"result\":\"" << result
              << '"';
    if (reason.has_value()) {
        std::cout << ",\"reason\":\"" << reason_name(*reason) << '"';
    }
    std::cout << "}\n";
}

void emit_diagnostic(
    const std::string_view phase,
    const std::optional<explorer::ExplorerDiagnostic>& diagnostic) {
    if (!diagnostic.has_value()) {
        return;
    }
    // Deliberately omit free-form detail and all paths/titles. Shell-origin
    // diagnostics retain only fixed stage/API labels plus the numeric code.
    std::cout << "{\"schema_version\":1,\"record_kind\":\"diagnostic\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"phase\":\"" << phase << '"'
              << ",\"code\":" << diagnostic->code;
    if (!diagnostic->shell_stage.empty()) {
        std::cout << ",\"stage\":" << json_quote(diagnostic->shell_stage)
                  << ",\"api\":" << json_quote(diagnostic->api);
    }
    std::cout << "}\n";
}

void emit_snapshot(const std::string_view phase,
                   const explorer::ExplorerWindowSnapshot& snapshot) {
    std::cout << "{\"schema_version\":1,\"record_kind\":\"snapshot\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"phase\":\"" << phase << '"' << ",\"positioning_rect\":";
    append_rect(std::cout, snapshot.positioning_rect);
    std::cout << ",\"visible_rect\":";
    append_rect(std::cout, snapshot.visible_rect);
    std::cout << ",\"monitor_rect\":";
    append_rect(std::cout, snapshot.monitor_rect);
    std::cout << ",\"monitor_work_area\":";
    append_rect(std::cout, snapshot.monitor_work_area);
    std::cout << ",\"pid\":" << snapshot.process_id
              << ",\"tid\":" << snapshot.thread_id
              << ",\"dpi\":" << snapshot.dpi << ",\"monitor_device\":"
              << json_quote(narrow_ascii(snapshot.monitor_device_name))
              << ",\"window_class\":"
              << json_quote(narrow_ascii(snapshot.window_class))
              << ",\"process_image_identity_validated\":true"
              << ",\"same_user_validated\":true"
              << ",\"same_session_validated\":"
              << (snapshot.controller_security.session_id ==
                          snapshot.target_security.session_id
                      ? "true"
                      : "false")
              << ",\"controller_integrity\":"
              << snapshot.controller_security.integrity_rid
              << ",\"controller_elevated\":"
              << (snapshot.controller_security.elevated ? "true" : "false")
              << ",\"controller_ui_access\":"
              << (snapshot.controller_security.ui_access ? "true" : "false")
              << ",\"controller_app_container\":"
              << (snapshot.controller_security.app_container ? "true" : "false")
              << ",\"target_integrity\":"
              << snapshot.target_security.integrity_rid
              << ",\"target_elevated\":"
              << (snapshot.target_security.elevated ? "true" : "false")
              << ",\"target_ui_access\":"
              << (snapshot.target_security.ui_access ? "true" : "false")
              << ",\"target_app_container\":"
              << (snapshot.target_security.app_container ? "true" : "false")
              << ",\"root_top_level\":"
              << (snapshot.root_top_level ? "true" : "false")
              << ",\"visible\":" << (snapshot.visible ? "true" : "false")
              << ",\"cloaked\":" << (snapshot.cloaked ? "true" : "false")
              << ",\"minimized\":" << (snapshot.minimized ? "true" : "false")
              << ",\"maximized\":" << (snapshot.maximized ? "true" : "false")
              << ",\"current_virtual_desktop\":"
              << (snapshot.on_current_virtual_desktop ? "true" : "false")
              << ",\"exact_test_location\":"
              << (snapshot.exact_test_location ? "true" : "false") << "}\n";
}

void emit_operation(const std::string_view phase,
                    const explorer::ExplorerOperationResult& operation) {
    std::cout << "{\"schema_version\":1,\"record_kind\":\"operation\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"phase\":\"" << phase << '"'
              << ",\"operation_id\":" << operation.operation_id
              << ",\"reason\":\"" << reason_name(operation.reason) << '"'
              << ",\"stage\":\"" << stage_name(operation.stage) << '"'
              << ",\"native_apply_attempted\":"
              << (operation.native_apply_attempted ? "true" : "false")
              << ",\"native_outcome_known\":"
              << (operation.native_outcome_known ? "true" : "false")
              << ",\"cleanup_operation\":"
              << (operation.cleanup_operation ? "true" : "false");
    if (operation.receipt.has_value()) {
        const auto& receipt = *operation.receipt;
        std::cout << ",\"logical_id\":" << receipt.token.logical_id()
                  << ",\"generation\":" << receipt.token.generation()
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
        std::cout << ",\"visible_target_verified\":"
                  << (receipt.visible_target_verified ? "true" : "false")
                  << ",\"positioning_target_verified\":"
                  << (receipt.positioning_target_verified ? "true" : "false")
                  << ",\"size_preserved\":"
                  << (receipt.size_preserved ? "true" : "false")
                  << ",\"identity_stable\":"
                  << (receipt.identity_stable ? "true" : "false")
                  << ",\"location_stable\":"
                  << (receipt.location_stable ? "true" : "false")
                  << ",\"monitor_and_dpi_stable\":"
                  << (receipt.monitor_and_dpi_stable ? "true" : "false");
    }
    std::cout << "}\n";
    emit_diagnostic(phase, operation.diagnostic);
}

[[nodiscard]] std::optional<Options> parse_options(const int argc,
                                                   char* const argv[]) {
    Options options;
    if (argc == 2 && (std::string_view{argv[1]} == "--help" ||
                      std::string_view{argv[1]} == "-h")) {
        options.help = true;
        return options;
    }
    bool self_test = false;
    bool provision_only = false;
    bool hold_seen = false;
    bool attempt_seen = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--self-test" && !self_test) {
            self_test = true;
            continue;
        }
        if (argument == "--hold-seconds" && !hold_seen && index + 1 < argc) {
            const std::string_view value{argv[++index]};
            std::uint32_t parsed{};
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(), parsed);
            if (result.ec != std::errc{} ||
                result.ptr != value.data() + value.size() ||
                parsed > kMaximumHoldSeconds) {
                return std::nullopt;
            }
            options.hold_seconds = parsed;
            hold_seen = true;
            continue;
        }
        if (argument == "--provision-only" && !provision_only) {
            provision_only = true;
            continue;
        }
        if (argument == "--attempt-index" && !attempt_seen &&
            index + 1 < argc) {
            const std::string_view value{argv[++index]};
            std::size_t parsed{};
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(), parsed);
            if (result.ec != std::errc{} ||
                result.ptr != value.data() + value.size() || parsed < 1U ||
                parsed > 3U) {
                return std::nullopt;
            }
            options.attempt_index = parsed;
            attempt_seen = true;
            continue;
        }
        return std::nullopt;
    }
    if (self_test == provision_only) {
        return std::nullopt;
    }
    if (self_test) {
        if (attempt_seen) {
            return std::nullopt;
        }
        options.mode = HarnessMode::SelfTest;
        return options;
    }
    if (hold_seen || !attempt_seen) {
        return std::nullopt;
    }
    options.mode = HarnessMode::ProvisionOnly;
    return options;
}

[[nodiscard]] bool is_plain_directory(
    const std::filesystem::path& path) noexcept {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0U;
}

[[nodiscard]] bool ensure_plain_directory(
    const std::filesystem::path& path) noexcept {
    const DWORD existing = GetFileAttributesW(path.c_str());
    if (existing != INVALID_FILE_ATTRIBUTES) {
        return is_plain_directory(path);
    }
    std::error_code error;
    const bool created = std::filesystem::create_directory(path, error);
    return created && !error && is_plain_directory(path);
}

[[nodiscard]] std::optional<std::filesystem::path>
validated_repository_root() noexcept {
    std::error_code error;
    const auto current = std::filesystem::current_path(error);
    if (error) {
        return std::nullopt;
    }
    auto root = std::filesystem::canonical(current, error);
    if (error) {
        return std::nullopt;
    }
    root = root.lexically_normal();
    if (!std::filesystem::exists(root / L".git", error) || error ||
        !std::filesystem::is_regular_file(root / L"CMakeLists.txt", error) ||
        error || !is_plain_directory(root / L"src")) {
        return std::nullopt;
    }
    return root;
}

[[nodiscard]] std::wstring guid_nonce() noexcept {
    GUID guid{};
    if (CoCreateGuid(&guid) != S_OK) {
        return {};
    }
    constexpr std::array<wchar_t, 16U> hexadecimal{
        L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7',
        L'8', L'9', L'a', L'b', L'c', L'd', L'e', L'f'};
    const auto* bytes = reinterpret_cast<const unsigned char*>(&guid);
    std::wstring nonce;
    nonce.reserve(sizeof(guid) * 2U);
    for (std::size_t index = 0U; index < sizeof(guid); ++index) {
        nonce.push_back(hexadecimal[(bytes[index] >> 4U) & 0x0fU]);
        nonce.push_back(hexadecimal[bytes[index] & 0x0fU]);
    }
    return nonce;
}

[[nodiscard]] std::optional<TargetDirectory> create_target_directory(
    const std::filesystem::path& repository_root) noexcept {
    const auto uat_root = repository_root / L"uat";
    const auto evidence_root = uat_root / L"r1c2a";
    if (!ensure_plain_directory(uat_root) ||
        !ensure_plain_directory(evidence_root)) {
        return std::nullopt;
    }

    for (std::size_t attempt = 0U; attempt < kNonceAttempts; ++attempt) {
        const auto nonce = guid_nonce();
        if (nonce.empty()) {
            return std::nullopt;
        }
        const auto candidate = evidence_root / (L"target-" + nonce);
        if (candidate.parent_path() != evidence_root) {
            return std::nullopt;
        }
        std::error_code error;
        if (!std::filesystem::create_directory(candidate, error)) {
            if (error) {
                return std::nullopt;
            }
            continue;
        }
        if (!is_plain_directory(candidate) ||
            !std::filesystem::is_empty(candidate, error) || error) {
            return std::nullopt;
        }
        const auto location = explorer::filesystem_location_identity(candidate);
        if (!location.filesystem()) {
            return std::nullopt;
        }
        return TargetDirectory{candidate, evidence_root, *location.identity};
    }
    return std::nullopt;
}

[[nodiscard]] bool inventory_proves_location_unused(
    const explorer::ShellWindowInventory& inventory,
    const explorer::FilesystemLocationIdentity& identity) noexcept {
    if (!inventory.complete) {
        return false;
    }
    for (const auto& window : inventory.windows) {
        if (window.shell_entry_count != window.locations.size()) {
            return false;
        }
        for (const auto& location : window.locations) {
            // Fail closed for any location whose directory identity cannot be
            // established. A filesystem identity unequal to the nonce target
            // is the only affirmative proof accepted here.
            if (!location.filesystem() || *location.identity == identity) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] DirectoryCleanupResult cleanup_target_directory(
    const TargetDirectory& target) noexcept {
    DirectoryCleanupResult result;
    const auto inventory = explorer::capture_shell_window_inventory();
    result.inventory_complete = inventory.complete;
    result.inventory_proved_unused =
        inventory_proves_location_unused(inventory, target.identity);
    if (!result.inventory_proved_unused) {
        return result;
    }

    std::error_code error;
    const auto canonical_parent =
        std::filesystem::canonical(target.path.parent_path(), error);
    const auto leaf = target.path.filename().native();
    const bool leaf_valid =
        leaf.size() == 39U && leaf.starts_with(L"target-") &&
        std::all_of(leaf.begin() + 7, leaf.end(), [](const wchar_t character) {
            return (character >= L'0' && character <= L'9') ||
                   (character >= L'a' && character <= L'f');
        });
    result.containment_valid =
        !error && target.path.parent_path() == target.evidence_root &&
        canonical_parent == target.evidence_root && leaf_valid &&
        is_plain_directory(target.evidence_root.parent_path()) &&
        is_plain_directory(target.evidence_root);
    if (!result.containment_valid) {
        result.filesystem_error = error.value();
        return result;
    }

    result.plain_directory = is_plain_directory(target.path);
    if (!result.plain_directory) {
        return result;
    }
    const auto current_identity =
        explorer::filesystem_location_identity(target.path);
    result.identity_stable = current_identity.filesystem() &&
                             *current_identity.identity == target.identity;
    if (!result.identity_stable) {
        return result;
    }

    result.directory_empty = std::filesystem::is_empty(target.path, error);
    if (error || !result.directory_empty) {
        result.filesystem_error = error.value();
        return result;
    }
    // Recheck immediately before the single non-recursive remove. This does
    // not follow a replacement reparse point or delete an identity different
    // from the nonce directory created by this process.
    const auto final_identity =
        explorer::filesystem_location_identity(target.path);
    if (!is_plain_directory(target.path) || !final_identity.filesystem() ||
        *final_identity.identity != target.identity) {
        result.identity_stable = false;
        return result;
    }
    result.removed = std::filesystem::remove(target.path, error);
    result.filesystem_error = error.value();
    return result;
}

void emit_directory_cleanup(const DirectoryCleanupResult& cleanup,
                            const unsigned int schema_version = 1U) {
    std::cout << "{\"schema_version\":" << schema_version
              << ",\"record_kind\":\"directory_cleanup\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"inventory_complete\":"
              << (cleanup.inventory_complete ? "true" : "false")
              << ",\"inventory_proved_unused\":"
              << (cleanup.inventory_proved_unused ? "true" : "false")
              << ",\"containment_valid\":"
              << (cleanup.containment_valid ? "true" : "false")
              << ",\"plain_directory\":"
              << (cleanup.plain_directory ? "true" : "false")
              << ",\"identity_stable\":"
              << (cleanup.identity_stable ? "true" : "false")
              << ",\"directory_empty\":"
              << (cleanup.directory_empty ? "true" : "false")
              << ",\"directory_removed\":"
              << (cleanup.removed ? "true" : "false")
              << ",\"filesystem_error\":" << cleanup.filesystem_error << "}\n";
}

[[nodiscard]] constexpr const char* json_bool(const bool value) noexcept {
    return value ? "true" : "false";
}

void emit_provision_diagnostic(
    const std::string_view phase,
    const std::optional<explorer::ExplorerDiagnostic>& diagnostic) {
    if (!diagnostic.has_value()) {
        return;
    }
    // Do not serialize free-form detail: it can contain the nonce path or
    // localized Shell data. Numeric status plus fixed adapter labels are
    // sufficient to correlate this record with the implementation.
    std::cout << "{\"schema_version\":2,\"record_kind\":\"diagnostic\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"phase\":" << json_quote(phase)
              << ",\"code\":" << diagnostic->code;
    if (!diagnostic->shell_stage.empty()) {
        std::cout << ",\"stage\":" << json_quote(diagnostic->shell_stage)
                  << ",\"api\":" << json_quote(diagnostic->api);
    }
    std::cout << "}\n";
}

void emit_provisioning_facts(
    const std::size_t attempt_index,
    const explorer::ExplorerProvisioningFacts& facts,
    const std::optional<detail::ExplorerDiagnosticNativeIdentity>&
        native_identity = std::nullopt) {
    std::cout << "{\"schema_version\":2,\"record_kind\":\"baseline\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"attempt_index\":" << attempt_index
              << ",\"total_shell_entries\":"
              << facts.baseline_total_shell_entries
              << ",\"reliable_shell_entries\":"
              << facts.baseline_reliable_shell_entries
              << ",\"reliable_unique_hwnd_count\":"
              << facts.baseline_reliable_unique_hwnd_count
              << ",\"forbidden_preexisting_hwnd_count\":"
              << facts.forbidden_preexisting_hwnd_count
              << ",\"valid_location_count\":"
              << facts.baseline_valid_location_count
              << ",\"empty_location_opaque_count\":"
              << facts.baseline_empty_location_count
              << ",\"inaccessible_location_opaque_count\":"
              << facts.baseline_inaccessible_location_count
              << ",\"exclusion_complete\":"
              << json_bool(facts.baseline_exclusion_complete) << "}\n";

    std::cout
        << "{\"schema_version\":2,\"record_kind\":\"subscription\""
        << ",\"recorded_at\":\"" << utc_timestamp() << '"'
        << ",\"attempt_index\":" << attempt_index
        << ",\"shell_advised\":" << json_bool(facts.shell_subscription_advised)
        << ",\"shell_generation\":" << facts.shell_subscription_generation
        << ",\"shell_callback_count\":" << facts.shell_callback_count
        << ",\"shell_registered_count\":" << facts.shell_registered_event_count
        << ",\"shell_revoked_count\":" << facts.shell_revoked_event_count
        << ",\"shell_malformed_count\":" << facts.shell_malformed_event_count
        << ",\"shell_overflow_count\":" << facts.shell_overflow_event_count
        << ",\"shell_wrong_thread_count\":"
        << facts.shell_wrong_thread_event_count
        << ",\"shell_post_retirement_count\":"
        << facts.shell_post_retirement_event_count
        << ",\"shell_generation_mismatch\":"
        << json_bool(facts.shell_subscription_generation_mismatch)
        << ",\"shell_cookie_lifecycle_ambiguous\":"
        << json_bool(facts.shell_cookie_lifecycle_ambiguous)
        << ",\"browser_advised\":"
        << json_bool(facts.browser_subscription_advised)
        << ",\"browser_navigate_complete_count\":"
        << facts.browser_navigate_complete_count
              << ",\"browser_matching_navigate_complete_count\":"
              << facts.browser_matching_navigate_complete_count
              << ",\"issued_matching_navigate_complete_count\":"
              << facts.issued_matching_navigate_complete_count
              << ",\"browser_navigation_history_ambiguous\":"
              << json_bool(facts.browser_navigation_history_ambiguous)
        << ",\"browser_unrelated_navigate_complete_count\":"
        << facts.browser_unrelated_navigate_complete_count
        << ",\"browser_identity_query_failure_count\":"
        << facts.browser_identity_query_failure_count
        << ",\"browser_quit_count\":" << facts.browser_quit_event_count
        << ",\"browser_malformed_count\":"
        << facts.browser_malformed_event_count
        << ",\"browser_overflow_count\":" << facts.browser_overflow_event_count
        << ",\"browser_wrong_thread_count\":"
        << facts.browser_wrong_thread_event_count
        << ",\"browser_post_retirement_count\":"
        << facts.browser_post_retirement_event_count << "}\n";

    std::cout << "{\"schema_version\":2,\"record_kind\":\"registration\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"attempt_index\":" << attempt_index
              << ",\"registered_cookie_count\":"
              << facts.registered_cookie_count
              << ",\"revoked_cookie_count\":" << facts.revoked_cookie_count
              << ",\"unrelated_registration_count\":"
              << facts.unrelated_registration_count
              << ",\"unresolved_registration_count\":"
              << facts.unresolved_registration_count
              << ",\"shared_window_identity_conflict_count\":"
              << facts.shared_window_identity_conflict_count
              << ",\"matching_registration_count\":"
              << facts.matching_registration_count << ",\"matching_cookie\":";
    if (facts.matching_cookie.has_value()) {
        std::cout << *facts.matching_cookie;
    } else {
        std::cout << "null";
    }
    std::cout << ",\"find_window_resolved\":"
              << json_bool(facts.matching_cookie_find_window_resolved) << "}\n";

    std::cout << "{\"schema_version\":2,\"record_kind\":\"attribution\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"attempt_index\":" << attempt_index
              << ",\"canonical_iunknown_identity_matches\":"
              << json_bool(facts.canonical_iunknown_identity_matches)
              << ",\"lease_hwnd_resolved\":"
              << json_bool(facts.lease_hwnd_resolved)
              << ",\"cookie_hwnd_resolved\":"
              << json_bool(facts.cookie_hwnd_resolved)
              << ",\"live_eligibility_hwnd_resolved\":"
              << json_bool(facts.live_eligibility_hwnd_resolved)
              << ",\"lease_cookie_live_hwnd_match\":"
              << json_bool(facts.lease_cookie_live_hwnd_match)
              << ",\"target_hwnd_preexisting\":"
              << json_bool(facts.target_hwnd_preexisting)
              << ",\"exact_target_location\":"
              << json_bool(facts.exact_target_location)
              << ",\"token_issued\":" << json_bool(facts.token_issued);
    if (native_identity.has_value()) {
        // This numeric HWND is fixture-internal diagnostic evidence. No raw
        // handle enters the public token or an operation API.
        std::cout << ",\"native_hwnd\":" << native_identity->native_key
                  << ",\"target_pid\":" << native_identity->process_id
                  << ",\"target_tid\":" << native_identity->thread_id;
    }
    std::cout << "}\n";
}

void emit_provisioning_cleanup(
    const std::size_t attempt_index,
    const explorer::ExplorerProvisioningCleanupFacts& cleanup) {
    std::cout << "{\"schema_version\":2,\"record_kind\":\"window_cleanup\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"attempt_index\":" << attempt_index
              << ",\"cleanup_authorized\":"
              << json_bool(cleanup.cleanup_authorized)
              << ",\"native_quit_attempted\":"
              << json_bool(cleanup.native_quit_attempted)
              << ",\"native_quit_succeeded\":"
              << json_bool(cleanup.native_quit_succeeded)
              << ",\"matching_registration_revoked\":"
              << json_bool(cleanup.matching_registration_revoked)
              << ",\"exact_hwnd_invalidated\":"
              << json_bool(cleanup.exact_hwnd_invalidated)
              << ",\"orphan_attribution_known\":"
              << json_bool(cleanup.orphan_attribution_known)
              << ",\"attributable_orphan\":"
              << json_bool(cleanup.attributable_orphan)
              << ",\"browser_events_unadvised\":"
              << json_bool(cleanup.browser_events_unadvised)
              << ",\"shell_events_unadvised\":"
              << json_bool(cleanup.shell_events_unadvised)
              << ",\"browser_event_lifecycle_clean\":"
              << json_bool(cleanup.browser_event_lifecycle_clean)
              << ",\"shell_event_lifecycle_clean\":"
              << json_bool(cleanup.shell_event_lifecycle_clean)
              << ",\"completed\":" << json_bool(cleanup.completed()) << "}\n";
}

void emit_provision_summary(
    const std::size_t attempt_index,
    const explorer::ExplorerEligibilityReason reason,
    const explorer::ExplorerProvisioningFacts& facts,
    const explorer::ExplorerProvisioningCleanupFacts& cleanup,
    const bool capture_read_only_passed,
    const bool stale_token_preflight_tested,
    const bool stale_token_preflight_passed,
    const bool directory_removed,
    const std::size_t native_translation_count,
    const bool passed) {
    std::cout << "{\"schema_version\":2,\"record_kind\":\"summary\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"mode\":\"provision_only\""
              << ",\"attempt_index\":" << attempt_index
              << ",\"reason\":" << json_quote(reason_name(reason))
              << ",\"baseline_exclusion_complete\":"
              << json_bool(facts.baseline_exclusion_complete)
              << ",\"registration_subscription\":\""
              << (facts.shell_subscription_advised ? "PASS" : "BLOCKED") << '"'
              << ",\"browser_subscription\":\""
              << (facts.browser_subscription_advised ? "PASS" : "BLOCKED")
              << '"' << ",\"matching_registration_count\":"
              << facts.matching_registration_count
              << ",\"unresolved_registration_count\":"
              << facts.unresolved_registration_count
              << ",\"shared_window_identity_conflict_count\":"
              << facts.shared_window_identity_conflict_count
              << ",\"browser_identity_query_failure_count\":"
              << facts.browser_identity_query_failure_count
              << ",\"browser_navigation_history_ambiguous\":"
              << json_bool(facts.browser_navigation_history_ambiguous)
              << ",\"canonical_iunknown_identity_matches\":"
              << json_bool(facts.canonical_iunknown_identity_matches)
              << ",\"hwnd_three_way_match\":"
              << json_bool(facts.lease_cookie_live_hwnd_match)
              << ",\"target_hwnd_preexisting\":"
              << json_bool(facts.target_hwnd_preexisting)
              << ",\"exact_target_location\":"
              << json_bool(facts.exact_target_location)
              << ",\"token_issued\":" << json_bool(facts.token_issued)
              << ",\"capture_read_only\":"
              << (capture_read_only_passed ? "\"PASS\"" : "\"BLOCKED\"")
              << ",\"safe_cleanup\":\""
              << (cleanup.completed() ? "PASS" : "SAFE_CLEANUP_NOT_PERFORMED")
              << '"' << ",\"matching_registration_revoked\":"
              << json_bool(cleanup.matching_registration_revoked)
              << ",\"exact_hwnd_invalidated\":"
              << json_bool(cleanup.exact_hwnd_invalidated)
              << ",\"attributable_orphan_count\":";
    if (cleanup.orphan_attribution_known) {
        std::cout << (cleanup.attributable_orphan ? 1 : 0);
    } else {
        std::cout << "null";
    }
    std::cout
              << ",\"stale_token_preflight\":\""
              << (stale_token_preflight_passed
                      ? "PASS"
                      : (stale_token_preflight_tested ? "FAIL" : "NOT_TESTED"))
              << '"'
              << ",\"directory_removed\":" << json_bool(directory_removed)
              << ",\"native_translation_count\":"
              << native_translation_count
              << ",\"user_existing_windows_touched\":false"
              << ",\"other_third_party_control\":false"
              << ",\"result\":\"" << (passed ? "PASS" : "BLOCKED") << "\"}\n";
}

[[nodiscard]] bool exact_operation_receipt(
    const explorer::ExplorerOperationResult& operation) noexcept {
    return operation.succeeded() && operation.receipt.has_value() &&
           operation.receipt->actual.has_value() &&
           operation.receipt->visible_target_verified &&
           operation.receipt->positioning_target_verified &&
           operation.receipt->size_preserved &&
           operation.receipt->identity_stable &&
           operation.receipt->location_stable &&
           operation.receipt->monitor_and_dpi_stable;
}

int run_provision_only(const Options& options) {
    std::cout << std::unitbuf;
    std::cout << "{\"schema_version\":2,\"record_kind\":\"harness_start\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"mode\":\"provision_only\""
              << ",\"attempt_index\":" << options.attempt_index
              << ",\"fixed_attempt_count\":3}\n";

    explorer::ExplorerProvisioningFacts empty_facts;
    explorer::ExplorerProvisioningCleanupFacts empty_cleanup;
    const ComApartment apartment;
    if (!apartment.succeeded()) {
        std::cout << "{\"schema_version\":2,\"record_kind\":\"com_apartment\""
                  << ",\"recorded_at\":\"" << utc_timestamp() << '"'
                  << ",\"attempt_index\":" << options.attempt_index
                  << ",\"result\":\"BLOCKED\",\"hresult\":"
                  << static_cast<std::uint32_t>(apartment.result()) << "}\n";
        emit_provision_summary(
            options.attempt_index,
            explorer::ExplorerEligibilityReason::ComApartmentUnavailable,
            empty_facts, empty_cleanup, false, false, false, false, 0U, false);
        return EXIT_FAILURE;
    }

    const auto repository_root = validated_repository_root();
    if (!repository_root.has_value()) {
        emit_provision_summary(
            options.attempt_index,
            explorer::ExplorerEligibilityReason::InvalidTargetDirectory,
            empty_facts, empty_cleanup, false, false, false, false, 0U, false);
        return EXIT_FAILURE;
    }
    const auto target = create_target_directory(*repository_root);
    if (!target.has_value()) {
        emit_provision_summary(
            options.attempt_index,
            explorer::ExplorerEligibilityReason::InvalidTargetDirectory,
            empty_facts, empty_cleanup, false, false, false, false, 0U, false);
        return EXIT_FAILURE;
    }

    auto provision = explorer::ExplorerTestSession::provision(
        target->path, std::chrono::seconds{8});
    if (!provision.succeeded()) {
        emit_provisioning_facts(options.attempt_index, provision.facts);
        emit_provision_diagnostic("provision", provision.diagnostic);
        emit_provisioning_cleanup(options.attempt_index, provision.cleanup);
        const auto directory_cleanup = cleanup_target_directory(*target);
        emit_directory_cleanup(directory_cleanup, 2U);
        emit_provision_summary(options.attempt_index, provision.reason,
                               provision.facts, provision.cleanup, false, false,
                               false, directory_cleanup.removed, 0U, false);
        return EXIT_FAILURE;
    }

    std::unique_ptr<explorer::ExplorerTestSession> session =
        std::move(provision.session);
    const auto token = session->token();
    const auto native_identity =
        detail::ExplorerSessionDiagnostics::read(*session);

    // Provision-only may read the issued capability once. It never selects a
    // delta and never invokes a live-token apply or restore operation.
    explorer::ExplorerWindowOperations operations{*session};
    const auto capture = operations.capture(token);
    const bool capture_read_only_passed =
        capture.succeeded() && capture.snapshot.has_value();
    std::cout << "{\"schema_version\":2,\"record_kind\":\"capture\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"attempt_index\":" << options.attempt_index
              << ",\"reason\":" << json_quote(reason_name(capture.reason))
              << ",\"stage\":" << json_quote(stage_name(capture.stage))
              << ",\"snapshot_available\":"
              << json_bool(capture.snapshot.has_value())
              << ",\"read_only\":true,\"native_apply_attempted\":false}\n";
    emit_provision_diagnostic("capture", capture.diagnostic);

    const auto close =
        session->close_test_window(token, std::chrono::seconds{3});
    std::cout << "{\"schema_version\":2,\"record_kind\":\"session_close\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"attempt_index\":" << options.attempt_index
              << ",\"reason\":" << json_quote(reason_name(close.reason))
              << ",\"native_close_attempted\":"
              << json_bool(close.native_close_attempted)
              << ",\"native_close_succeeded\":"
              << json_bool(close.native_close_succeeded)
              << ",\"window_disappeared\":"
              << json_bool(close.window_disappeared)
              << ",\"token_retired\":" << json_bool(close.token_retired)
              << ",\"matching_registration_revoked\":"
              << json_bool(close.matching_registration_revoked)
              << ",\"exact_hwnd_invalidated\":"
              << json_bool(close.exact_hwnd_invalidated)
              << ",\"orphan_attribution_known\":"
              << json_bool(close.orphan_attribution_known)
              << ",\"attributable_orphan\":"
              << json_bool(close.attributable_orphan)
              << ",\"browser_events_unadvised\":"
              << json_bool(close.browser_events_unadvised)
              << ",\"shell_events_unadvised\":"
              << json_bool(close.shell_events_unadvised)
              << ",\"browser_event_lifecycle_clean\":"
              << json_bool(close.browser_event_lifecycle_clean)
              << ",\"shell_event_lifecycle_clean\":"
              << json_bool(close.shell_event_lifecycle_clean) << "}\n";
    emit_provision_diagnostic("window_cleanup", close.diagnostic);
    const auto facts_after_cleanup = session->provisioning_facts();
    const auto cleanup = facts_after_cleanup.cleanup;

    bool stale_token_preflight_passed = false;
    std::size_t native_translation_count = 0U;
    const bool stale_token_preflight_tested =
        close.succeeded() && cleanup.completed();
    if (stale_token_preflight_tested) {
        const auto stale = operations.apply_single(
            token, facts_after_cleanup.initial_snapshot.visible_rect);
        native_translation_count = stale.native_apply_attempted ? 1U : 0U;
        stale_token_preflight_passed =
            stale.reason == explorer::ExplorerEligibilityReason::StaleToken &&
            stale.stage == explorer::ExplorerOperationStage::Preflight &&
            !stale.native_apply_attempted;
        std::cout << "{\"schema_version\":2,\"record_kind\":\"stale_preflight\""
                  << ",\"recorded_at\":\"" << utc_timestamp() << '"'
                  << ",\"attempt_index\":" << options.attempt_index
                  << ",\"reason\":" << json_quote(reason_name(stale.reason))
                  << ",\"stage\":" << json_quote(stage_name(stale.stage))
                  << ",\"native_apply_attempted\":"
                  << json_bool(stale.native_apply_attempted) << ",\"result\":\""
                  << (stale_token_preflight_passed ? "PASS" : "FAIL")
                  << "\"}\n";
        emit_provision_diagnostic("stale_preflight", stale.diagnostic);
    } else {
        std::cout << "{\"schema_version\":2,\"record_kind\":\"stale_preflight\""
                  << ",\"recorded_at\":\"" << utc_timestamp() << '"'
                  << ",\"attempt_index\":" << options.attempt_index
                  << ",\"native_apply_attempted\":false"
                  << ",\"result\":\"NOT_TESTED\"}\n";
    }

    emit_provisioning_facts(options.attempt_index, facts_after_cleanup,
                            native_identity);
    emit_provisioning_cleanup(options.attempt_index, cleanup);
    const auto directory_cleanup = cleanup_target_directory(*target);
    emit_directory_cleanup(directory_cleanup, 2U);

    const bool event_streams_clean =
        facts_after_cleanup.shell_malformed_event_count == 0U &&
        facts_after_cleanup.shell_overflow_event_count == 0U &&
        facts_after_cleanup.shell_wrong_thread_event_count == 0U &&
        facts_after_cleanup.shell_post_retirement_event_count == 0U &&
        !facts_after_cleanup.shell_subscription_generation_mismatch &&
        !facts_after_cleanup.shell_cookie_lifecycle_ambiguous &&
        facts_after_cleanup.browser_malformed_event_count == 0U &&
        facts_after_cleanup.browser_overflow_event_count == 0U &&
        facts_after_cleanup.browser_wrong_thread_event_count == 0U &&
        facts_after_cleanup.browser_post_retirement_event_count == 0U;
    const bool passed =
        facts_after_cleanup.baseline_exclusion_complete &&
        facts_after_cleanup.shell_subscription_advised &&
        facts_after_cleanup.browser_subscription_advised &&
        event_streams_clean &&
        facts_after_cleanup.matching_registration_count == 1U &&
        facts_after_cleanup.unresolved_registration_count == 0U &&
        facts_after_cleanup.shared_window_identity_conflict_count == 0U &&
        facts_after_cleanup.browser_identity_query_failure_count == 0U &&
        !facts_after_cleanup.browser_navigation_history_ambiguous &&
        facts_after_cleanup.matching_cookie.has_value() &&
        facts_after_cleanup.matching_cookie_find_window_resolved &&
        facts_after_cleanup.canonical_iunknown_identity_matches &&
        facts_after_cleanup.lease_hwnd_resolved &&
        facts_after_cleanup.cookie_hwnd_resolved &&
        facts_after_cleanup.live_eligibility_hwnd_resolved &&
        facts_after_cleanup.lease_cookie_live_hwnd_match &&
        !facts_after_cleanup.target_hwnd_preexisting &&
        facts_after_cleanup.exact_target_location &&
        facts_after_cleanup.token_issued && capture_read_only_passed &&
        close.succeeded() && cleanup.completed() &&
        stale_token_preflight_passed && native_translation_count == 0U;

    emit_provision_summary(
        options.attempt_index, explorer::ExplorerEligibilityReason::Eligible,
        facts_after_cleanup, cleanup, capture_read_only_passed,
        stale_token_preflight_tested, stale_token_preflight_passed,
        directory_cleanup.removed, native_translation_count, passed);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

int run_self_test(const Options& options) {
    std::cout << std::unitbuf;
    std::cout << "{\"schema_version\":1,\"record_kind\":\"harness_start\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"mode\":\"self_test\",\"hold_seconds\":"
              << options.hold_seconds << "}\n";

    const ComApartment apartment;
    if (!apartment.succeeded()) {
        std::cout << "{\"schema_version\":1,\"record_kind\":\"com_apartment\""
                  << ",\"recorded_at\":\"" << utc_timestamp() << '"'
                  << ",\"result\":\"BLOCKED\",\"hresult\":"
                  << static_cast<std::uint32_t>(apartment.result()) << "}\n";
        emit_test("COM_STA", "FAIL",
                  explorer::ExplorerEligibilityReason::ComApartmentUnavailable);
        return EXIT_FAILURE;
    }
    emit_test("COM_STA", "PASS");

    const auto repository_root = validated_repository_root();
    if (!repository_root.has_value()) {
        emit_test("REPOSITORY_ROOT", "FAIL");
        return EXIT_FAILURE;
    }
    const auto target = create_target_directory(*repository_root);
    if (!target.has_value()) {
        emit_test("UNIQUE_EMPTY_TARGET_DIRECTORY", "FAIL");
        return EXIT_FAILURE;
    }
    emit_test("UNIQUE_EMPTY_TARGET_DIRECTORY", "PASS");

    bool runtime_passed = false;
    bool safe_close_succeeded = false;
    bool stale_preflight_passed = false;
    bool lifetime_tested = false;

    auto provision = explorer::ExplorerTestSession::provision(
        target->path, std::chrono::seconds{8});
    if (!provision.succeeded()) {
        std::cout << "{\"schema_version\":1,\"record_kind\":\"provision\""
                  << ",\"recorded_at\":\"" << utc_timestamp() << '"'
                  << ",\"result\":\"BLOCKED\",\"reason\":\""
                  << reason_name(provision.reason) << "\"}\n";
        emit_diagnostic("provision", provision.diagnostic);
        emit_test("EXPLORER_TEST_TARGET_ISOLATION", "FAIL", provision.reason);
        const auto directory_cleanup = cleanup_target_directory(*target);
        emit_directory_cleanup(directory_cleanup);
        std::cout << "{\"schema_version\":1,\"record_kind\":\"summary\""
                  << ",\"recorded_at\":\"" << utc_timestamp() << '"'
                  << ",\"result\":\"BLOCKED\""
                  << ",\"explorer_test_target_isolation\":\"BLOCKED\""
                  << ",\"native_translation_count\":0}\n";
        return EXIT_FAILURE;
    }

    std::unique_ptr<explorer::ExplorerTestSession> session =
        std::move(provision.session);
    const auto& facts = session->provisioning_facts();
    const auto token = session->token();
    const auto native_identity =
        detail::ExplorerSessionDiagnostics::read(*session);
    std::cout << "{\"schema_version\":1,\"record_kind\":\"provision\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"result\":\"PASS\""
              << ",\"preexisting_window_count\":"
              << facts.preexisting_window_count
              << ",\"post_navigation_window_count\":"
              << facts.post_navigation_window_count
              << ",\"new_candidate_count\":" << facts.new_candidate_count
              << ",\"retained_window_was_new_before_navigation\":"
              << (facts.retained_window_was_new_before_navigation ? "true"
                                                                  : "false")
              << ",\"baseline_facts_unchanged\":"
              << (facts.baseline_facts_unchanged ? "true" : "false")
              << ",\"exact_unique_test_location\":"
              << (facts.exact_unique_test_location ? "true" : "false")
              << ",\"logical_id\":" << token.logical_id()
              << ",\"generation\":" << token.generation()
              << ",\"native_hwnd\":" << native_identity.native_key
              << ",\"target_pid\":" << native_identity.process_id
              << ",\"target_tid\":" << native_identity.thread_id << "}\n";
    emit_test("EXPLORER_TEST_TARGET_ISOLATION", "PASS");
    emit_snapshot("initial", facts.initial_snapshot);

    explorer::ExplorerWindowOperations operations{*session};
    const auto capture = operations.capture(token);
    if (capture.succeeded()) {
        emit_snapshot("preflight", *capture.snapshot);
    }
    emit_test("LIVE_PREFLIGHT", capture.succeeded() ? "PASS" : "FAIL",
              capture.reason);

    explorer::ExplorerSafeDeltaResult safe_delta;
    if (capture.succeeded()) {
        safe_delta = explorer::select_safe_test_delta(*capture.snapshot);
    }
    std::cout << "{\"schema_version\":1,\"record_kind\":\"safe_delta\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"reason\":\"" << reason_name(safe_delta.reason) << '"';
    if (safe_delta.succeeded()) {
        std::cout << ",\"dx\":" << safe_delta.delta->dx
                  << ",\"dy\":" << safe_delta.delta->dy
                  << ",\"target_visible\":";
        append_rect(std::cout, *safe_delta.target_visible_rect);
    }
    std::cout << "}\n";
    emit_test("SAFE_SINGLE_MONITOR_DELTA",
              safe_delta.succeeded() ? "PASS" : "FAIL", safe_delta.reason);

    std::optional<explorer::ExplorerOperationResult> apply;
    std::optional<explorer::ExplorerOperationResult> restore;
    if (capture.succeeded() && safe_delta.succeeded()) {
        apply = operations.apply_single(token, *safe_delta.target_visible_rect);
        emit_operation("single_translation", *apply);
        emit_test("SINGLE_TRANSLATION_EXACT",
                  exact_operation_receipt(*apply) ? "PASS" : "FAIL",
                  apply->reason);

        if (apply->succeeded()) {
            std::cout << "{\"schema_version\":1,\"record_kind\":\"hold\""
                      << ",\"recorded_at\":\"" << utc_timestamp() << '"'
                      << ",\"seconds\":" << options.hold_seconds << "}\n";
            std::this_thread::sleep_for(
                std::chrono::seconds{options.hold_seconds});
        }

        if (apply->native_apply_attempted) {
            restore = operations.restore(token);
            emit_operation("restore", *restore);
            emit_test("RESTORE_EXACT",
                      exact_operation_receipt(*restore) ? "PASS" : "FAIL",
                      restore->reason);
        }
    }

    runtime_passed = capture.succeeded() && safe_delta.succeeded() &&
                     apply.has_value() && exact_operation_receipt(*apply) &&
                     restore.has_value() && exact_operation_receipt(*restore);

    const auto cleanup =
        session->close_test_window(token, std::chrono::seconds{3});
    safe_close_succeeded = cleanup.succeeded();
    std::cout << "{\"schema_version\":1,\"record_kind\":\"window_cleanup\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"reason\":\"" << reason_name(cleanup.reason) << '"'
              << ",\"native_close_attempted\":"
              << (cleanup.native_close_attempted ? "true" : "false")
              << ",\"window_disappeared\":"
              << (cleanup.window_disappeared ? "true" : "false")
              << ",\"token_retired\":"
              << (cleanup.token_retired ? "true" : "false") << "}\n";
    emit_diagnostic("window_cleanup", cleanup.diagnostic);
    emit_test("SAFE_EXACT_WINDOW_CLOSE",
              safe_close_succeeded ? "PASS" : "NOT_TESTED", cleanup.reason);

    if (safe_close_succeeded) {
        lifetime_tested = true;
        const auto stale =
            operations.apply_single(token, facts.initial_snapshot.visible_rect);
        emit_operation("stale_token_preflight", stale);
        stale_preflight_passed =
            stale.reason == explorer::ExplorerEligibilityReason::StaleToken &&
            stale.stage == explorer::ExplorerOperationStage::Preflight &&
            !stale.native_apply_attempted;
        emit_test("WINDOW_DESTROY_LIFETIME",
                  stale_preflight_passed ? "PASS" : "FAIL", stale.reason);
        runtime_passed = runtime_passed && stale_preflight_passed;
    } else {
        emit_test("WINDOW_DESTROY_LIFETIME", "NOT_TESTED",
                  explorer::ExplorerEligibilityReason::SafeCleanupNotPerformed);
    }

    const auto directory_cleanup = cleanup_target_directory(*target);
    emit_directory_cleanup(directory_cleanup);

    // Recovery requires exact graceful cleanup, real stale-token evidence,
    // and removal of the empty nonce directory before a full runtime can PASS
    // or authorize the Release run. Translation/restore success alone is not
    // sufficient.
    runtime_passed = runtime_passed && safe_close_succeeded &&
                     stale_preflight_passed && directory_cleanup.removed;

    std::cout << "{\"schema_version\":1,\"record_kind\":\"summary\""
              << ",\"recorded_at\":\"" << utc_timestamp() << '"'
              << ",\"result\":\"" << (runtime_passed ? "PASS" : "FAIL") << '"'
              << ",\"explorer_test_target_isolation\":\"PASS\""
              << ",\"native_translation_count\":"
              << (apply.has_value() && apply->native_apply_attempted ? 1 : 0)
              << ",\"restore_native_apply_count\":"
              << (restore.has_value() && restore->native_apply_attempted ? 1
                                                                         : 0)
              << ",\"safe_close\":\""
              << (safe_close_succeeded ? "PASS" : "SAFE_CLEANUP_NOT_PERFORMED")
              << '"' << ",\"window_destroy_lifetime\":\""
              << (lifetime_tested ? (stale_preflight_passed ? "PASS" : "FAIL")
                                  : "NOT_TESTED")
              << '"' << ",\"directory_removed\":"
              << (directory_cleanup.removed ? "true" : "false") << "}\n";
    return runtime_passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace

int main(const int argc, char* argv[]) {
    constexpr std::string_view usage =
        "Usage:\n"
        "  panebind-explorer-harness --self-test "
        "[--hold-seconds <0..86400>]\n"
        "  panebind-explorer-harness --provision-only "
        "--attempt-index <1|2|3>\n";
    const auto options = parse_options(argc, argv);
    if (!options.has_value()) {
        std::cerr << usage;
        return EXIT_FAILURE;
    }
    if (options->help) {
        std::cout << usage;
        return EXIT_SUCCESS;
    }
    try {
        return options->mode == HarnessMode::ProvisionOnly
                   ? run_provision_only(*options)
                   : run_self_test(*options);
    } catch (...) {
        // Do not serialize exception text: filesystem and Shell exceptions can
        // contain user paths or localized window data.
        if (options->mode == HarnessMode::ProvisionOnly) {
            std::cout << "{\"schema_version\":2,\"record_kind\":\"summary\""
                      << ",\"recorded_at\":\"" << utc_timestamp() << '"'
                      << ",\"mode\":\"provision_only\""
                      << ",\"attempt_index\":" << options->attempt_index
                      << ",\"reason\":\"unexpected_exception\""
                      << ",\"baseline_exclusion_complete\":false"
                      << ",\"registration_subscription\":\"BLOCKED\""
                      << ",\"browser_subscription\":\"BLOCKED\""
                      << ",\"matching_registration_count\":0"
                      << ",\"unresolved_registration_count\":null"
                      << ",\"shared_window_identity_conflict_count\":null"
                      << ",\"browser_identity_query_failure_count\":null"
                      << ",\"canonical_iunknown_identity_matches\":false"
                      << ",\"hwnd_three_way_match\":false"
                      << ",\"target_hwnd_preexisting\":null"
                      << ",\"exact_target_location\":false"
                      << ",\"token_issued\":false"
                      << ",\"capture_read_only\":\"BLOCKED\""
                      << ",\"safe_cleanup\":\"SAFE_CLEANUP_NOT_PERFORMED\""
                      << ",\"matching_registration_revoked\":null"
                      << ",\"exact_hwnd_invalidated\":null"
                      << ",\"attributable_orphan_count\":null"
                      << ",\"stale_token_preflight\":\"NOT_TESTED\""
                      << ",\"directory_removed\":null"
                      << ",\"native_translation_count\":null"
                      << ",\"user_existing_windows_touched\":false"
                      << ",\"other_third_party_control\":false"
                      << ",\"result\":\"FAIL\"}\n";
        } else {
            std::cout << "{\"schema_version\":1,\"record_kind\":\"summary\""
                      << ",\"recorded_at\":\"" << utc_timestamp() << '"'
                      << ",\"result\":\"FAIL\",\"reason\":\"unexpected_"
                         "exception\"}\n";
        }
        return EXIT_FAILURE;
    }
}
