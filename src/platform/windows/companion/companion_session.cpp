#include "platform/windows/companion/companion_session.h"

#include "core/geometry/checked_arithmetic.h"
#include "platform/windows/companion/companion_session_internal.h"
#include "platform/windows/operations/window_translation.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <bcrypt.h>
#include <dwmapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>

namespace panebind::platform::windows::companion {
namespace {

constexpr UINT kTranslationFlags =
    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;
constexpr DWORD kPipeBufferBytes = 64U * 1024U;
constexpr DWORD kFixtureFallbackExitCode = 0xC1F10001U;
constexpr wchar_t kCompanionTargetExecutableName[] =
    L"panebind-companion-target.exe";

std::atomic<std::uint64_t> next_controller_authority{1U};
std::atomic<std::uint64_t> next_request_id{1U};
std::atomic<std::uint64_t> next_operation_id{1U};

[[nodiscard]] std::uint64_t issue_monotonic(
    std::atomic<std::uint64_t>& source) noexcept {
    auto current = source.load(std::memory_order_relaxed);
    for (;;) {
        if (current == 0U || current == std::numeric_limits<std::uint64_t>::max()) {
            return 0U;
        }
        if (source.compare_exchange_weak(current,
                                         current + 1U,
                                         std::memory_order_relaxed,
                                         std::memory_order_relaxed)) {
            return current;
        }
    }
}

[[nodiscard]] CompanionDiagnostic adapter_diagnostic(std::uint64_t code,
                                                     std::string api,
                                                     std::string detail) {
    return {CompanionDiagnosticDomain::Adapter,
            code,
            std::move(api),
            std::move(detail)};
}

[[nodiscard]] CompanionDiagnostic protocol_diagnostic(std::uint64_t code,
                                                      std::string api,
                                                      std::string detail) {
    return {CompanionDiagnosticDomain::Protocol,
            code,
            std::move(api),
            std::move(detail)};
}

[[nodiscard]] CompanionDiagnostic win32_diagnostic(std::string api,
                                                   DWORD error,
                                                   std::string detail) {
    return {CompanionDiagnosticDomain::Win32,
            static_cast<std::uint64_t>(error),
            std::move(api),
            std::move(detail)};
}

[[nodiscard]] CompanionDiagnostic hresult_diagnostic(std::string api,
                                                     HRESULT error,
                                                     std::string detail) {
    return {CompanionDiagnosticDomain::HResult,
            static_cast<std::uint32_t>(error),
            std::move(api),
            std::move(detail)};
}

class UniqueHandle final {
public:
    UniqueHandle() noexcept = default;
    explicit UniqueHandle(HANDLE handle) noexcept : handle_(handle) {}
    ~UniqueHandle() {
        reset();
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] HANDLE get() const noexcept {
        return handle_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }

    [[nodiscard]] HANDLE release() noexcept {
        return std::exchange(handle_, nullptr);
    }

    void reset(HANDLE handle = nullptr) noexcept {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_{};
};

class AttributeList final {
public:
    AttributeList() = default;
    ~AttributeList() {
        if (list_ != nullptr) {
            DeleteProcThreadAttributeList(list_);
        }
    }

    AttributeList(const AttributeList&) = delete;
    AttributeList& operator=(const AttributeList&) = delete;

    [[nodiscard]] bool initialize() {
        SIZE_T bytes = 0U;
        SetLastError(ERROR_SUCCESS);
        static_cast<void>(InitializeProcThreadAttributeList(
            nullptr, 1U, 0U, &bytes));
        if (bytes == 0U || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return false;
        }
        storage_.resize(bytes);
        list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
        return InitializeProcThreadAttributeList(list_, 1U, 0U, &bytes) != FALSE;
    }

    [[nodiscard]] bool restrict_handles(std::array<HANDLE, 2U>& handles) {
        return UpdateProcThreadAttribute(list_,
                                         0U,
                                         PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                         handles.data(),
                                         sizeof(handles),
                                         nullptr,
                                         nullptr) != FALSE;
    }

    [[nodiscard]] LPPROC_THREAD_ATTRIBUTE_LIST get() const noexcept {
        return list_;
    }

private:
    std::vector<std::byte> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_{};
};

[[nodiscard]] DWORD bounded_wait_milliseconds(
    const std::chrono::milliseconds timeout) noexcept {
    if (timeout.count() <= 0) {
        return 0U;
    }
    constexpr auto maximum =
        static_cast<std::chrono::milliseconds::rep>(INFINITE - 1U);
    if (timeout.count() > maximum) {
        return INFINITE - 1U;
    }
    return static_cast<DWORD>(timeout.count());
}

[[nodiscard]] bool write_exact(HANDLE pipe,
                               const void* source,
                               std::size_t byte_count,
                               DWORD& error) noexcept {
    const auto* cursor = static_cast<const std::byte*>(source);
    std::size_t remaining = byte_count;
    while (remaining != 0U) {
        const auto chunk = static_cast<DWORD>(std::min<std::size_t>(
            remaining, std::numeric_limits<DWORD>::max()));
        DWORD written = 0U;
        if (!WriteFile(pipe, cursor, chunk, &written, nullptr)) {
            error = GetLastError();
            return false;
        }
        if (written == 0U) {
            error = ERROR_WRITE_FAULT;
            return false;
        }
        cursor += written;
        remaining -= written;
    }
    return true;
}

[[nodiscard]] bool read_exact(HANDLE pipe,
                              void* destination,
                              std::size_t byte_count,
                              DWORD& error) noexcept {
    auto* cursor = static_cast<std::byte*>(destination);
    std::size_t remaining = byte_count;
    while (remaining != 0U) {
        const auto chunk = static_cast<DWORD>(std::min<std::size_t>(
            remaining, std::numeric_limits<DWORD>::max()));
        DWORD read = 0U;
        if (!ReadFile(pipe, cursor, chunk, &read, nullptr)) {
            error = GetLastError();
            return false;
        }
        if (read == 0U) {
            error = ERROR_BROKEN_PIPE;
            return false;
        }
        cursor += read;
        remaining -= read;
    }
    return true;
}

enum class FrameReadStatus {
    Succeeded,
    Timeout,
    ProcessExited,
    ReadFailed,
    PayloadTooLarge,
    ThreadFailure,
};

struct FrameReadResult {
    FrameReadStatus status{FrameReadStatus::ReadFailed};
    protocol::FrameHeader header{};
    std::vector<std::byte> payload;
    DWORD error{};
};

[[nodiscard]] FrameReadResult read_frame_with_timeout(
    HANDLE pipe,
    HANDLE process,
    std::chrono::milliseconds timeout) {
    struct Job {
        FrameReadResult result;
    } job;

    std::thread reader;
    try {
        reader = std::thread([&job, pipe]() {
            DWORD error = ERROR_SUCCESS;
            if (!read_exact(pipe,
                            &job.result.header,
                            sizeof(job.result.header),
                            error)) {
                job.result.status = FrameReadStatus::ReadFailed;
                job.result.error = error;
                return;
            }
            if (job.result.header.payload_size > protocol::kMaximumPayloadBytes) {
                job.result.status = FrameReadStatus::PayloadTooLarge;
                return;
            }
            job.result.payload.resize(job.result.header.payload_size);
            if (!job.result.payload.empty() &&
                !read_exact(pipe,
                            job.result.payload.data(),
                            job.result.payload.size(),
                            error)) {
                job.result.status = FrameReadStatus::ReadFailed;
                job.result.error = error;
                return;
            }
            job.result.status = FrameReadStatus::Succeeded;
        });
    } catch (...) {
        return {FrameReadStatus::ThreadFailure, {}, {}, ERROR_NOT_ENOUGH_MEMORY};
    }

    const HANDLE reader_handle =
        reinterpret_cast<HANDLE>(reader.native_handle());
    const std::array waits{reader_handle, process};
    DWORD wait = WaitForMultipleObjects(static_cast<DWORD>(waits.size()),
                                        waits.data(),
                                        FALSE,
                                        bounded_wait_milliseconds(timeout));
    if (wait == WAIT_OBJECT_0 + 1U) {
        // A final frame may already be buffered while process teardown wins the
        // wait race. Give the reader one small bounded drain opportunity.
        wait = WaitForSingleObject(reader_handle, 50U);
        if (wait != WAIT_OBJECT_0) {
            static_cast<void>(CancelSynchronousIo(reader_handle));
            reader.join();
            return {FrameReadStatus::ProcessExited, {}, {}, ERROR_BROKEN_PIPE};
        }
    } else if (wait == WAIT_TIMEOUT) {
        static_cast<void>(CancelSynchronousIo(reader_handle));
        reader.join();
        return {FrameReadStatus::Timeout, {}, {}, WAIT_TIMEOUT};
    } else if (wait != WAIT_OBJECT_0) {
        const DWORD error = GetLastError();
        static_cast<void>(CancelSynchronousIo(reader_handle));
        reader.join();
        return {FrameReadStatus::ThreadFailure, {}, {}, error};
    }

    reader.join();
    return std::move(job.result);
}

[[nodiscard]] bool frame_envelope_matches(
    const protocol::FrameHeader& header,
    protocol::SessionMarker session,
    protocol::FrameKind expected_kind,
    std::uint64_t expected_request_id) noexcept {
    return header.magic == protocol::kFrameMagic &&
           header.version == protocol::kProtocolVersion &&
           header.kind == expected_kind && header.session == session &&
           header.request_id == expected_request_id;
}

template <typename Payload>
[[nodiscard]] std::optional<Payload> decode_exact_payload(
    const std::vector<std::byte>& bytes) noexcept {
    static_assert(std::is_trivially_copyable_v<Payload>);
    if (bytes.size() != sizeof(Payload)) {
        return std::nullopt;
    }
    Payload payload{};
    std::memcpy(&payload, bytes.data(), sizeof(payload));
    return payload;
}

[[nodiscard]] std::optional<std::filesystem::path> fully_qualified_file_path(
    const std::filesystem::path& requested,
    DWORD& error) {
    if (requested.empty() || !requested.is_absolute()) {
        error = ERROR_BAD_PATHNAME;
        return std::nullopt;
    }
    const std::wstring requested_text = requested.native();
    const DWORD required =
        GetFullPathNameW(requested_text.c_str(), 0U, nullptr, nullptr);
    if (required == 0U) {
        error = GetLastError();
        return std::nullopt;
    }
    std::vector<wchar_t> buffer(required);
    const DWORD length = GetFullPathNameW(requested_text.c_str(),
                                          required,
                                          buffer.data(),
                                          nullptr);
    if (length == 0U || length >= required) {
        error = length == 0U ? GetLastError() : ERROR_INSUFFICIENT_BUFFER;
        return std::nullopt;
    }
    std::filesystem::path result{std::wstring{buffer.data(), length}};
    const DWORD attributes = GetFileAttributesW(result.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
        error = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError()
                                                       : ERROR_DIRECTORY;
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] bool issue_session_marker(protocol::SessionMarker& marker,
                                        DWORD& error) noexcept {
    const NTSTATUS status = BCryptGenRandom(
        nullptr,
        reinterpret_cast<PUCHAR>(&marker),
        static_cast<ULONG>(sizeof(marker)),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status < 0) {
        error = static_cast<DWORD>(status);
        return false;
    }
    if (!marker.valid()) {
        marker.low = 1U;
    }
    return true;
}

[[nodiscard]] std::optional<ProcessSecurityFacts> query_security_facts(
    HANDLE process,
    CompanionDiagnostic& diagnostic) {
    HANDLE raw_token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &raw_token)) {
        diagnostic = win32_diagnostic("OpenProcessToken",
                                      GetLastError(),
                                      "process token query failed");
        return std::nullopt;
    }
    UniqueHandle token{raw_token};

    DWORD integrity_bytes = 0U;
    static_cast<void>(GetTokenInformation(token.get(),
                                          TokenIntegrityLevel,
                                          nullptr,
                                          0U,
                                          &integrity_bytes));
    if (integrity_bytes == 0U || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        diagnostic = win32_diagnostic("GetTokenInformation",
                                      GetLastError(),
                                      "integrity buffer sizing failed");
        return std::nullopt;
    }
    std::vector<std::byte> integrity_storage(integrity_bytes);
    if (!GetTokenInformation(token.get(),
                             TokenIntegrityLevel,
                             integrity_storage.data(),
                             integrity_bytes,
                             &integrity_bytes)) {
        diagnostic = win32_diagnostic("GetTokenInformation",
                                      GetLastError(),
                                      "integrity query failed");
        return std::nullopt;
    }
    const auto* label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(
        integrity_storage.data());
    if (!IsValidSid(label->Label.Sid)) {
        diagnostic = adapter_diagnostic(1U,
                                        "IsValidSid",
                                        "process integrity SID is invalid");
        return std::nullopt;
    }
    const PUCHAR count = GetSidSubAuthorityCount(label->Label.Sid);
    if (count == nullptr || *count == 0U) {
        diagnostic = adapter_diagnostic(2U,
                                        "GetSidSubAuthorityCount",
                                        "process integrity SID has no RID");
        return std::nullopt;
    }
    const PDWORD rid = GetSidSubAuthority(label->Label.Sid, *count - 1U);
    if (rid == nullptr) {
        diagnostic = adapter_diagnostic(3U,
                                        "GetSidSubAuthority",
                                        "process integrity RID query failed");
        return std::nullopt;
    }

    DWORD ui_access = 0U;
    DWORD returned = 0U;
    if (!GetTokenInformation(token.get(),
                             TokenUIAccess,
                             &ui_access,
                             sizeof(ui_access),
                             &returned)) {
        diagnostic = win32_diagnostic("GetTokenInformation",
                                      GetLastError(),
                                      "TokenUIAccess query failed");
        return std::nullopt;
    }
    DWORD app_container = 0U;
    if (!GetTokenInformation(token.get(),
                             TokenIsAppContainer,
                             &app_container,
                             sizeof(app_container),
                             &returned)) {
        diagnostic = win32_diagnostic("GetTokenInformation",
                                      GetLastError(),
                                      "TokenIsAppContainer query failed");
        return std::nullopt;
    }

    return ProcessSecurityFacts{*rid,
                                ui_access != 0U,
                                app_container != 0U};
}

[[nodiscard]] std::uintptr_t native_key(HWND window) noexcept {
    return reinterpret_cast<std::uintptr_t>(window);
}

[[nodiscard]] HWND native_window(std::uintptr_t key) noexcept {
    return reinterpret_cast<HWND>(key);
}

[[nodiscard]] HANDLE marker_value(protocol::LogicalWindowId logical_id,
                                  std::uint64_t generation) noexcept {
    const std::uint64_t value =
        protocol::marker_property_value(logical_id, generation);
    if (value == 0U || value > std::numeric_limits<std::uintptr_t>::max()) {
        return nullptr;
    }
    return reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(value));
}

[[nodiscard]] bool has_expected_class(HWND window,
                                      CompanionDiagnostic& diagnostic) {
    std::array<wchar_t, 256U> class_name{};
    const int length = GetClassNameW(window,
                                     class_name.data(),
                                     static_cast<int>(class_name.size()));
    if (length == 0) {
        diagnostic = win32_diagnostic("GetClassNameW",
                                      GetLastError(),
                                      "companion window class query failed");
        return false;
    }
    return std::wstring_view{class_name.data(), static_cast<std::size_t>(length)} ==
           protocol::kWindowClassName;
}

[[nodiscard]] bool has_top_level_shape(HWND window,
                                       CompanionDiagnostic& diagnostic) {
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    const DWORD style_error = GetLastError();
    if (style == 0 && style_error != ERROR_SUCCESS) {
        diagnostic = win32_diagnostic("GetWindowLongPtrW",
                                      style_error,
                                      "companion window style query failed");
        return false;
    }
    const bool child =
        (static_cast<ULONG_PTR>(style) & static_cast<ULONG_PTR>(WS_CHILD)) != 0U;
    if (!detail::is_companion_top_level_provenance(
            GetAncestor(window, GA_ROOT) == window,
            child,
            GetWindow(window, GW_OWNER) != nullptr)) {
        diagnostic = adapter_diagnostic(
            4U,
            "GetAncestor/GetWindowLongPtrW/GetWindow",
            "companion capability requires an unowned independent top-level window");
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<core::geometry::Rect> checked_native_rect(
    const RECT& rect) noexcept {
    if (rect.right < rect.left || rect.bottom < rect.top) {
        return std::nullopt;
    }
    return core::geometry::Rect{rect.left, rect.top, rect.right, rect.bottom};
}

[[nodiscard]] bool same_rect_size(const core::geometry::Rect& first,
                                  const core::geometry::Rect& second) noexcept {
    const auto first_width = core::geometry::checked_difference(first.right(),
                                                                first.left());
    const auto first_height = core::geometry::checked_difference(first.bottom(),
                                                                 first.top());
    const auto second_width = core::geometry::checked_difference(second.right(),
                                                                 second.left());
    const auto second_height = core::geometry::checked_difference(second.bottom(),
                                                                  second.top());
    return first_width.has_value() && first_height.has_value() &&
           second_width.has_value() && second_height.has_value() &&
           *first_width == *second_width && *first_height == *second_height;
}

} // namespace

namespace detail {

CompanionTokenLedger::CompanionTokenLedger(
    const protocol::SessionMarker session_authority) noexcept
    : CompanionTokenLedger(session_authority,
                           issue_monotonic(next_controller_authority)) {}

CompanionTokenLedger::CompanionTokenLedger(
    const protocol::SessionMarker session_authority,
    const std::uint64_t controller_authority) noexcept
    : controller_authority_(controller_authority),
      session_authority_(session_authority),
      session_active_(session_authority.valid() && controller_authority != 0U) {}

CompanionLedgerIssueResult CompanionTokenLedger::issue(
    const protocol::LogicalWindowId logical_id,
    const std::uint64_t generation,
    const std::uintptr_t native_key_value,
    const std::uint32_t process_id,
    const std::uint32_t creator_thread_id) {
    if (!session_active_) {
        return {CompanionLedgerIssueStatus::SessionRetired, std::nullopt};
    }
    if (controller_authority_ == 0U) {
        return {CompanionLedgerIssueStatus::AuthorityExhausted, std::nullopt};
    }
    if (!protocol::is_valid_logical_window_id(
            static_cast<std::uint32_t>(logical_id)) ||
        generation == 0U || native_key_value == 0U || process_id == 0U ||
        creator_thread_id == 0U) {
        return {CompanionLedgerIssueStatus::InvalidIdentity, std::nullopt};
    }
    if (active_.contains(logical_id)) {
        return {CompanionLedgerIssueStatus::DuplicateLogicalId, std::nullopt};
    }
    if (logical_by_native_.contains(native_key_value)) {
        return {CompanionLedgerIssueStatus::DuplicateNativeKey, std::nullopt};
    }
    const auto prior = highest_generation_.find(logical_id);
    if (prior != highest_generation_.end() && generation <= prior->second) {
        return {CompanionLedgerIssueStatus::NonMonotonicGeneration,
                std::nullopt};
    }

    const CompanionLedgerEntry entry{session_authority_,
                                     logical_id,
                                     generation,
                                     native_key_value,
                                     process_id,
                                     creator_thread_id};
    active_.emplace(logical_id, entry);
    logical_by_native_.emplace(native_key_value, logical_id);
    highest_generation_.insert_or_assign(logical_id, generation);
    return {CompanionLedgerIssueStatus::Succeeded,
            CompanionWindowToken{controller_authority_,
                                 session_authority_,
                                 logical_id,
                                 generation}};
}

bool CompanionTokenLedger::retire(
    const protocol::LogicalWindowId logical_id) noexcept {
    const auto found = active_.find(logical_id);
    if (found == active_.end()) {
        return false;
    }
    logical_by_native_.erase(found->second.native_key);
    active_.erase(found);
    return true;
}

void CompanionTokenLedger::retire_all() noexcept {
    active_.clear();
    logical_by_native_.clear();
    session_active_ = false;
}

bool CompanionTokenLedger::session_active() const noexcept {
    return session_active_;
}

std::size_t CompanionTokenLedger::active_count() const noexcept {
    return active_.size();
}

std::optional<CompanionLedgerEntry> CompanionTokenLedger::resolve(
    const CompanionWindowToken& token) const noexcept {
    if (!session_active_ ||
        token.controller_authority_ != controller_authority_ ||
        token.session_authority_ != session_authority_) {
        return std::nullopt;
    }
    const auto found = active_.find(token.logical_id_);
    if (found == active_.end() || found->second.generation != token.generation_) {
        return std::nullopt;
    }
    return found->second;
}

std::vector<CompanionWindowToken> CompanionTokenLedger::active_tokens() const {
    std::vector<CompanionWindowToken> tokens;
    if (!session_active_) {
        return tokens;
    }
    tokens.reserve(active_.size());
    for (const auto& [logical_id, entry] : active_) {
        tokens.push_back(CompanionWindowToken{controller_authority_,
                                             session_authority_,
                                             logical_id,
                                             entry.generation});
    }
    return tokens;
}

std::vector<CompanionLedgerEntry> CompanionTokenLedger::active_entries() const {
    std::vector<CompanionLedgerEntry> entries;
    entries.reserve(active_.size());
    for (const auto& [logical_id, entry] : active_) {
        static_cast<void>(logical_id);
        entries.push_back(entry);
    }
    return entries;
}

CompanionBatchTokenValidation validate_companion_batch_tokens(
    const std::span<const CompanionWindowToken> tokens,
    const CompanionTokenLedger& ledger) noexcept {
    if (tokens.empty()) {
        return CompanionBatchTokenValidation::Empty;
    }
    for (std::size_t first = 0U; first < tokens.size(); ++first) {
        for (std::size_t second = first + 1U; second < tokens.size(); ++second) {
            if (tokens[first] == tokens[second]) {
                return CompanionBatchTokenValidation::Duplicate;
            }
        }
    }
    for (const auto& token : tokens) {
        if (!ledger.resolve(token).has_value()) {
            return CompanionBatchTokenValidation::Unknown;
        }
    }
    return CompanionBatchTokenValidation::Succeeded;
}

bool is_companion_top_level_provenance(const bool root_is_self,
                                       const bool has_child_style,
                                       const bool has_owner) noexcept {
    return root_is_self && !has_child_style && !has_owner;
}

bool is_valid_uncooperative_selection(
    const std::uint32_t window_mask,
    const std::int32_t offset_x,
    const std::uint32_t requested_window_mask) noexcept {
    const std::uint32_t d_bit =
        protocol::logical_window_bit(protocol::LogicalWindowId::D);
    return (window_mask & ~d_bit) == 0U &&
           (window_mask & ~requested_window_mask) == 0U &&
           offset_x >= -protocol::kMaximumTestOffset &&
           offset_x <= protocol::kMaximumTestOffset &&
           (offset_x == 0 || window_mask == d_bit);
}

} // namespace detail

namespace {

struct ResolvedCompanionWindow {
    HWND window{};
    detail::CompanionLedgerEntry entry;
};

struct ResolveResult {
    CompanionOperationStatus status{CompanionOperationStatus::StaleToken};
    std::optional<ResolvedCompanionWindow> value;
    std::optional<CompanionDiagnostic> diagnostic;
};

struct CommandExchangeResult {
    CompanionCommandStatus status{CompanionCommandStatus::ProtocolFailure};
    std::uint64_t request_id{};
    protocol::FrameHeader header{};
    std::vector<std::byte> payload;
    std::optional<CompanionDiagnostic> diagnostic;
};

} // namespace

struct CompanionSession::Impl final {
    Impl(HANDLE process_handle,
         HANDLE command_write_handle,
         HANDLE response_read_handle,
         DWORD process_id,
         protocol::SessionMarker marker) noexcept
        : process(process_handle),
          command_write(command_write_handle),
          response_read(response_read_handle),
          ledger(marker) {
        facts.controller_process_id = GetCurrentProcessId();
        facts.controller_thread_id = GetCurrentThreadId();
        facts.target_process_id = process_id;
        facts.session_authority = marker;
    }

    ~Impl() {
        std::lock_guard lock{mutex};
        ledger.retire_all();
        if (command_write != nullptr) {
            CloseHandle(command_write);
            command_write = nullptr;
        }
        if (process != nullptr &&
            WaitForSingleObject(process, 0U) == WAIT_TIMEOUT) {
            // This is only the exact child process created for this fixture.
            static_cast<void>(TerminateProcess(process,
                                               kFixtureFallbackExitCode));
            static_cast<void>(WaitForSingleObject(process, 2'000U));
        }
        if (response_read != nullptr) {
            CloseHandle(response_read);
            response_read = nullptr;
        }
        if (process != nullptr) {
            CloseHandle(process);
            process = nullptr;
        }
    }

    [[nodiscard]] bool refresh_process_state_locked() noexcept {
        if (!process_alive || process == nullptr) {
            return false;
        }
        const DWORD wait = WaitForSingleObject(process, 0U);
        if (wait != WAIT_TIMEOUT ||
            GetProcessId(process) != facts.target_process_id) {
            process_alive = false;
            ledger.retire_all();
            return false;
        }
        return true;
    }

    [[nodiscard]] bool write_frame_locked(protocol::FrameKind kind,
                                          std::uint64_t request_id,
                                          const void* payload,
                                          std::size_t payload_size,
                                          CompanionDiagnostic& diagnostic) {
        if (payload_size > protocol::kMaximumPayloadBytes ||
            payload_size > std::numeric_limits<std::uint32_t>::max()) {
            diagnostic = protocol_diagnostic(1U,
                                             "write_frame",
                                             "protocol payload exceeds bound");
            return false;
        }
        protocol::FrameHeader header{};
        header.kind = kind;
        header.payload_size = static_cast<std::uint32_t>(payload_size);
        header.session = facts.session_authority;
        header.request_id = request_id;

        DWORD error = ERROR_SUCCESS;
        if (!write_exact(command_write, &header, sizeof(header), error) ||
            (payload_size != 0U &&
             !write_exact(command_write, payload, payload_size, error))) {
            diagnostic = win32_diagnostic("WriteFile",
                                          error,
                                          "companion command pipe write failed");
            static_cast<void>(refresh_process_state_locked());
            return false;
        }
        return true;
    }

    [[nodiscard]] CommandExchangeResult exchange_locked(
        protocol::FrameKind command,
        const void* payload,
        std::size_t payload_size,
        protocol::FrameKind expected_response,
        std::chrono::milliseconds timeout = std::chrono::seconds{2}) {
        CommandExchangeResult result;
        result.request_id = issue_monotonic(next_request_id);
        if (result.request_id == 0U) {
            result.diagnostic = adapter_diagnostic(
                5U, "CompanionSession", "request identifier space exhausted");
            return result;
        }
        if (!refresh_process_state_locked()) {
            result.status = CompanionCommandStatus::SessionExited;
            result.diagnostic = adapter_diagnostic(
                6U, "WaitForSingleObject", "companion process has exited");
            return result;
        }
        CompanionDiagnostic write_diagnostic;
        if (!write_frame_locked(command,
                                result.request_id,
                                payload,
                                payload_size,
                                write_diagnostic)) {
            result.status = refresh_process_state_locked()
                                ? CompanionCommandStatus::ProtocolFailure
                                : CompanionCommandStatus::SessionExited;
            result.diagnostic = std::move(write_diagnostic);
            return result;
        }

        auto frame = read_frame_with_timeout(response_read, process, timeout);
        switch (frame.status) {
        case FrameReadStatus::Timeout:
            result.status = CompanionCommandStatus::Timeout;
            result.diagnostic = adapter_diagnostic(
                7U, "ReadFile", "companion response timed out");
            return result;
        case FrameReadStatus::ProcessExited:
            static_cast<void>(refresh_process_state_locked());
            result.status = CompanionCommandStatus::SessionExited;
            result.diagnostic = adapter_diagnostic(
                8U, "WaitForMultipleObjects", "companion exited before response");
            return result;
        case FrameReadStatus::ReadFailed:
        case FrameReadStatus::ThreadFailure:
            static_cast<void>(refresh_process_state_locked());
            result.status = process_alive
                                ? CompanionCommandStatus::ProtocolFailure
                                : CompanionCommandStatus::SessionExited;
            result.diagnostic = win32_diagnostic(
                "ReadFile", frame.error, "companion response read failed");
            return result;
        case FrameReadStatus::PayloadTooLarge:
            result.status = CompanionCommandStatus::ProtocolFailure;
            result.diagnostic = protocol_diagnostic(
                9U, "FrameHeader", "companion response payload exceeds bound");
            return result;
        case FrameReadStatus::Succeeded:
            break;
        }
        if (!frame_envelope_matches(frame.header,
                                    facts.session_authority,
                                    expected_response,
                                    result.request_id)) {
            result.status = CompanionCommandStatus::ProtocolFailure;
            result.diagnostic = protocol_diagnostic(
                10U, "FrameHeader", "companion response envelope mismatch");
            return result;
        }
        result.status = CompanionCommandStatus::Succeeded;
        result.header = frame.header;
        result.payload = std::move(frame.payload);
        return result;
    }

    [[nodiscard]] CompanionCommandResult command_locked(
        protocol::FrameKind command,
        const void* payload,
        std::size_t payload_size,
        std::chrono::milliseconds timeout = std::chrono::seconds{2},
        std::optional<protocol::LogicalWindowId> expected_logical_id =
            std::nullopt,
        std::optional<std::uint64_t> expected_step_id = std::nullopt) {
        auto exchange = exchange_locked(command,
                                        payload,
                                        payload_size,
                                        protocol::FrameKind::CommandResult,
                                        timeout);
        CompanionCommandResult result;
        result.status = exchange.status;
        result.request_id = exchange.request_id;
        result.diagnostic = std::move(exchange.diagnostic);
        if (exchange.status != CompanionCommandStatus::Succeeded) {
            return result;
        }
        const auto response =
            decode_exact_payload<protocol::CommandResultPayload>(exchange.payload);
        if (!response.has_value() || response->command != command ||
            response->target_process_id != facts.target_process_id ||
            response->target_ui_thread_id != facts.target_ui_thread_id ||
            (expected_logical_id.has_value() &&
             response->logical_id != *expected_logical_id) ||
            (expected_step_id.has_value() &&
             response->step_id != *expected_step_id)) {
            result.status = CompanionCommandStatus::ProtocolFailure;
            result.diagnostic = protocol_diagnostic(
                11U, "CommandResult", "companion command response mismatch");
            return result;
        }
        result.target_status = response->status;
        if (response->status != protocol::CommandStatus::Succeeded) {
            result.status = response->status == protocol::CommandStatus::NativeFailure
                                ? CompanionCommandStatus::NativeFailure
                                : CompanionCommandStatus::TargetRejected;
            result.diagnostic = protocol_diagnostic(
                response->native_error,
                "CommandResult",
                "companion target rejected the fixture command");
        }
        return result;
    }

    [[nodiscard]] ResolveResult resolve_locked(
        const CompanionWindowToken& token) {
        if (!refresh_process_state_locked()) {
            return {CompanionOperationStatus::SessionExited,
                    std::nullopt,
                    adapter_diagnostic(12U,
                                       "WaitForSingleObject",
                                       "companion process is not alive")};
        }
        const auto entry = ledger.resolve(token);
        if (!entry.has_value()) {
            return {CompanionOperationStatus::StaleToken,
                    std::nullopt,
                    adapter_diagnostic(13U,
                                       "CompanionTokenLedger",
                                       "token is stale or belongs to another session")};
        }
        if (GetProcessId(process) != entry->process_id ||
            entry->process_id != facts.target_process_id) {
            ledger.retire_all();
            process_alive = false;
            return {CompanionOperationStatus::WrongProcess,
                    std::nullopt,
                    adapter_diagnostic(14U,
                                       "GetProcessId",
                                       "launch process handle identity changed")};
        }

        HWND window = native_window(entry->native_key);
        if (!IsWindow(window)) {
            static_cast<void>(ledger.retire(entry->logical_id));
            return {CompanionOperationStatus::InvalidWindow,
                    std::nullopt,
                    adapter_diagnostic(15U,
                                       "IsWindow",
                                       "registered companion HWND is no longer valid")};
        }
        DWORD process_id = 0U;
        SetLastError(ERROR_SUCCESS);
        const DWORD thread_id = GetWindowThreadProcessId(window, &process_id);
        if (thread_id == 0U) {
            const DWORD error = GetLastError();
            static_cast<void>(ledger.retire(entry->logical_id));
            return {CompanionOperationStatus::InvalidWindow,
                    std::nullopt,
                    win32_diagnostic("GetWindowThreadProcessId",
                                     error,
                                     "companion HWND identity query failed")};
        }
        if (process_id != entry->process_id ||
            process_id != facts.target_process_id) {
            static_cast<void>(ledger.retire(entry->logical_id));
            return {CompanionOperationStatus::WrongProcess,
                    std::nullopt,
                    adapter_diagnostic(16U,
                                       "GetWindowThreadProcessId",
                                       "companion HWND belongs to a different process")};
        }
        if (thread_id != entry->creator_thread_id ||
            thread_id != facts.target_ui_thread_id) {
            static_cast<void>(ledger.retire(entry->logical_id));
            return {CompanionOperationStatus::WrongThread,
                    std::nullopt,
                    adapter_diagnostic(17U,
                                       "GetWindowThreadProcessId",
                                       "companion HWND creator thread changed")};
        }

        CompanionDiagnostic diagnostic;
        if (!has_expected_class(window, diagnostic)) {
            static_cast<void>(ledger.retire(entry->logical_id));
            if (diagnostic.api.empty()) {
                diagnostic = adapter_diagnostic(
                    18U, "GetClassNameW", "unexpected companion window class");
            }
            return {CompanionOperationStatus::WrongWindowClass,
                    std::nullopt,
                    std::move(diagnostic)};
        }
        if (!has_top_level_shape(window, diagnostic)) {
            static_cast<void>(ledger.retire(entry->logical_id));
            return {CompanionOperationStatus::NotIndependentTopLevel,
                    std::nullopt,
                    std::move(diagnostic)};
        }
        const std::wstring property_name =
            protocol::session_property_name(entry->session_authority);
        if (GetPropW(window, property_name.c_str()) !=
            marker_value(entry->logical_id, entry->generation)) {
            static_cast<void>(ledger.retire(entry->logical_id));
            return {CompanionOperationStatus::MarkerFailure,
                    std::nullopt,
                    adapter_diagnostic(
                        19U,
                        "GetPropW",
                        "companion launch/session marker is missing or changed")};
        }
        return {CompanionOperationStatus::Succeeded,
                ResolvedCompanionWindow{window, *entry},
                std::nullopt};
    }

    [[nodiscard]] CompanionCaptureResult capture_locked(
        const CompanionWindowToken& token);
    [[nodiscard]] bool accept_handshake_locked(
        const protocol::HandshakePayload& handshake,
        CompanionDiagnostic& diagnostic);
    void recapture_locked(CompanionOperationResult& result,
                          bool after_native_failure);

    std::mutex mutex;
    HANDLE process{};
    HANDLE command_write{};
    HANDLE response_read{};
    bool process_alive{true};
    bool shutdown_started{};
    CompanionLaunchFacts facts;
    detail::CompanionTokenLedger ledger;
};

namespace {

[[nodiscard]] CompanionCaptureResult capture_native_window(
    HWND window,
    std::uint32_t process_id,
    std::uint32_t thread_id) {
    CompanionCaptureResult result;

    RECT positioning{};
    if (!GetWindowRect(window, &positioning)) {
        result.status = CompanionOperationStatus::GeometryCaptureFailed;
        result.diagnostic = win32_diagnostic(
            "GetWindowRect", GetLastError(), "positioning rectangle query failed");
        return result;
    }
    RECT visible{};
    const HRESULT visible_result = DwmGetWindowAttribute(
        window, DWMWA_EXTENDED_FRAME_BOUNDS, &visible, sizeof(visible));
    if (FAILED(visible_result)) {
        result.status = CompanionOperationStatus::GeometryCaptureFailed;
        result.diagnostic = hresult_diagnostic(
            "DwmGetWindowAttribute",
            visible_result,
            "extended visible frame query failed");
        return result;
    }
    const auto positioning_rect = checked_native_rect(positioning);
    const auto visible_rect = checked_native_rect(visible);
    if (!positioning_rect.has_value() || !visible_rect.has_value()) {
        result.status = CompanionOperationStatus::GeometryCaptureFailed;
        result.diagnostic = adapter_diagnostic(
            20U, "window geometry", "native API returned an inverted rectangle");
        return result;
    }

    const DPI_AWARENESS_CONTEXT dpi_context =
        GetWindowDpiAwarenessContext(window);
    if (dpi_context == nullptr ||
        !AreDpiAwarenessContextsEqual(
            dpi_context, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        result.status = CompanionOperationStatus::DpiContextMismatch;
        result.diagnostic = adapter_diagnostic(
            21U,
            "GetWindowDpiAwarenessContext",
            "companion target window is not PerMonitorV2");
        return result;
    }
    const UINT dpi = GetDpiForWindow(window);
    if (dpi == 0U) {
        result.status = CompanionOperationStatus::GeometryCaptureFailed;
        result.diagnostic = adapter_diagnostic(
            22U, "GetDpiForWindow", "window DPI query returned zero");
        return result;
    }
    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr) {
        result.status = CompanionOperationStatus::GeometryCaptureFailed;
        result.diagnostic = adapter_diagnostic(
            23U, "MonitorFromWindow", "window monitor query returned null");
        return result;
    }
    MONITORINFOEXW monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfoW(monitor, &monitor_info)) {
        result.status = CompanionOperationStatus::GeometryCaptureFailed;
        result.diagnostic = win32_diagnostic(
            "GetMonitorInfoW", GetLastError(), "monitor geometry query failed");
        return result;
    }
    const auto monitor_rect = checked_native_rect(monitor_info.rcMonitor);
    const auto work_area = checked_native_rect(monitor_info.rcWork);
    if (!monitor_rect.has_value() || !work_area.has_value()) {
        result.status = CompanionOperationStatus::GeometryCaptureFailed;
        result.diagnostic = adapter_diagnostic(
            24U, "GetMonitorInfoW", "monitor API returned an inverted rectangle");
        return result;
    }

    result.status = CompanionOperationStatus::Succeeded;
    result.snapshot = CompanionWindowSnapshot{*positioning_rect,
                                              *visible_rect,
                                              dpi,
                                              monitor_info.szDevice,
                                              *monitor_rect,
                                              *work_area,
                                              process_id,
                                              thread_id};
    return result;
}

} // namespace

CompanionCaptureResult CompanionSession::Impl::capture_locked(
    const CompanionWindowToken& token) {
    auto resolved = resolve_locked(token);
    if (!resolved.value.has_value()) {
        return {resolved.status,
                CompanionOperationStage::Preflight,
                std::nullopt,
                std::move(resolved.diagnostic)};
    }
    auto result = capture_native_window(resolved.value->window,
                                        resolved.value->entry.process_id,
                                        resolved.value->entry.creator_thread_id);
    if (!result.succeeded()) {
        return result;
    }
    auto confirmed = resolve_locked(token);
    if (!confirmed.value.has_value() ||
        confirmed.value->window != resolved.value->window) {
        return {CompanionOperationStatus::WindowInvalidatedDuringApply,
                CompanionOperationStage::Preflight,
                std::nullopt,
                confirmed.diagnostic.has_value()
                    ? std::move(confirmed.diagnostic)
                    : std::optional<CompanionDiagnostic>{adapter_diagnostic(
                          25U,
                          "CompanionTokenLedger",
                          "companion HWND changed during capture")}};
    }
    return result;
}

bool CompanionSession::Impl::accept_handshake_locked(
    const protocol::HandshakePayload& handshake,
    CompanionDiagnostic& diagnostic) {
    if (!refresh_process_state_locked()) {
        diagnostic = adapter_diagnostic(
            26U, "WaitForSingleObject", "companion exited during handshake");
        return false;
    }
    if (handshake.target_process_id != facts.target_process_id ||
        handshake.target_ui_thread_id == 0U ||
        handshake.window_count != protocol::kWindowCount ||
        GetProcessId(process) != facts.target_process_id) {
        diagnostic = protocol_diagnostic(
            27U, "HandshakePayload", "target process/thread/count mismatch");
        return false;
    }

    std::uint32_t logical_mask = 0U;
    std::array<std::uint64_t, protocol::kWindowCount> native_windows{};
    for (std::size_t index = 0U; index < handshake.windows.size(); ++index) {
        const auto& registration = handshake.windows[index];
        const auto logical_value =
            static_cast<std::uint32_t>(registration.logical_id);
        if (!protocol::is_valid_logical_window_id(logical_value) ||
            registration.generation == 0U ||
            registration.native_window == 0U ||
            registration.process_id != facts.target_process_id ||
            registration.ui_thread_id != handshake.target_ui_thread_id ||
            registration.marker_value != protocol::marker_property_value(
                                             registration.logical_id,
                                             registration.generation)) {
            diagnostic = protocol_diagnostic(
                28U, "WindowRegistrationFact", "invalid registration identity");
            return false;
        }
        const std::uint32_t bit =
            protocol::logical_window_bit(registration.logical_id);
        if ((logical_mask & bit) != 0U) {
            diagnostic = protocol_diagnostic(
                29U, "WindowRegistrationFact", "duplicate logical window ID");
            return false;
        }
        logical_mask |= bit;
        native_windows[index] = registration.native_window;
    }
    if (logical_mask != protocol::kAllWindowMask) {
        diagnostic = protocol_diagnostic(
            30U, "HandshakePayload", "handshake does not contain exactly A/B/C/D");
        return false;
    }
    for (std::size_t first = 0U; first < native_windows.size(); ++first) {
        for (std::size_t second = first + 1U;
             second < native_windows.size();
             ++second) {
            if (native_windows[first] == native_windows[second]) {
                diagnostic = protocol_diagnostic(
                    31U, "WindowRegistrationFact", "duplicate native HWND fact");
                return false;
            }
        }
    }

    const std::wstring property_name =
        protocol::session_property_name(facts.session_authority);
    for (const auto& registration : handshake.windows) {
        if (registration.native_window >
            std::numeric_limits<std::uintptr_t>::max()) {
            diagnostic = protocol_diagnostic(
                32U, "WindowRegistrationFact", "HWND fact is not representable");
            return false;
        }
        HWND window = native_window(
            static_cast<std::uintptr_t>(registration.native_window));
        if (!IsWindow(window)) {
            diagnostic = adapter_diagnostic(
                33U, "IsWindow", "handshake HWND is not currently valid");
            return false;
        }
        DWORD process_id = 0U;
        SetLastError(ERROR_SUCCESS);
        const DWORD thread_id = GetWindowThreadProcessId(window, &process_id);
        if (thread_id == 0U || process_id != facts.target_process_id ||
            thread_id != handshake.target_ui_thread_id) {
            diagnostic = adapter_diagnostic(
                34U,
                "GetWindowThreadProcessId",
                "handshake HWND does not belong to the launched UI thread");
            return false;
        }
        if (!has_expected_class(window, diagnostic)) {
            if (diagnostic.api.empty()) {
                diagnostic = adapter_diagnostic(
                    35U, "GetClassNameW", "unexpected handshake window class");
            }
            return false;
        }
        if (!has_top_level_shape(window, diagnostic)) {
            return false;
        }
        if (GetPropW(window, property_name.c_str()) !=
            marker_value(registration.logical_id, registration.generation)) {
            diagnostic = adapter_diagnostic(
                36U,
                "GetPropW",
                "handshake window lacks the per-launch marker");
            return false;
        }
    }

    facts.target_ui_thread_id = handshake.target_ui_thread_id;
    for (const auto& registration : handshake.windows) {
        const auto issue = ledger.issue(
            registration.logical_id,
            registration.generation,
            static_cast<std::uintptr_t>(registration.native_window),
            registration.process_id,
            registration.ui_thread_id);
        if (issue.status != detail::CompanionLedgerIssueStatus::Succeeded) {
            ledger.retire_all();
            diagnostic = adapter_diagnostic(
                37U,
                "CompanionTokenLedger",
                "validated handshake registration could not be issued");
            return false;
        }
    }
    return ledger.active_count() == protocol::kWindowCount;
}

CompanionSession::CompanionSession(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

CompanionSession::~CompanionSession() {
    if (impl_ != nullptr) {
        static_cast<void>(shutdown());
    }
}

CompanionLaunchResult CompanionSession::launch(
    const std::filesystem::path& fully_qualified_target_path,
    const std::chrono::milliseconds handshake_timeout) {
    DWORD path_error = ERROR_SUCCESS;
    const auto target_path =
        fully_qualified_file_path(fully_qualified_target_path, path_error);
    if (!target_path.has_value()) {
        return {CompanionLaunchStatus::InvalidTargetPath,
                nullptr,
                win32_diagnostic("GetFullPathNameW/GetFileAttributesW",
                                 path_error,
                                 "target executable path must be an existing fully qualified file")};
    }
    const std::wstring target_filename = target_path->filename().native();
    if (CompareStringOrdinal(target_filename.c_str(),
                             -1,
                             kCompanionTargetExecutableName,
                             -1,
                             TRUE) != CSTR_EQUAL) {
        return {CompanionLaunchStatus::InvalidTargetPath,
                nullptr,
                adapter_diagnostic(
                    63U,
                    "CompanionSession::launch",
                    "R1-C1 only launches panebind-companion-target.exe")};
    }

    protocol::SessionMarker marker{};
    DWORD marker_error = ERROR_SUCCESS;
    if (!issue_session_marker(marker, marker_error)) {
        return {CompanionLaunchStatus::AuthorityGenerationFailed,
                nullptr,
                win32_diagnostic("BCryptGenRandom",
                                 marker_error,
                                 "per-launch session authority generation failed")};
    }

    SECURITY_ATTRIBUTES inheritable{};
    inheritable.nLength = sizeof(inheritable);
    inheritable.bInheritHandle = TRUE;

    HANDLE raw_child_command_read = nullptr;
    HANDLE raw_parent_command_write = nullptr;
    if (!CreatePipe(&raw_child_command_read,
                    &raw_parent_command_write,
                    &inheritable,
                    kPipeBufferBytes)) {
        return {CompanionLaunchStatus::PipeCreationFailed,
                nullptr,
                win32_diagnostic("CreatePipe",
                                 GetLastError(),
                                 "controller-to-child pipe creation failed")};
    }
    UniqueHandle child_command_read{raw_child_command_read};
    UniqueHandle parent_command_write{raw_parent_command_write};

    HANDLE raw_parent_response_read = nullptr;
    HANDLE raw_child_response_write = nullptr;
    if (!CreatePipe(&raw_parent_response_read,
                    &raw_child_response_write,
                    &inheritable,
                    kPipeBufferBytes)) {
        return {CompanionLaunchStatus::PipeCreationFailed,
                nullptr,
                win32_diagnostic("CreatePipe",
                                 GetLastError(),
                                 "child-to-controller pipe creation failed")};
    }
    UniqueHandle parent_response_read{raw_parent_response_read};
    UniqueHandle child_response_write{raw_child_response_write};

    if (!SetHandleInformation(parent_command_write.get(),
                              HANDLE_FLAG_INHERIT,
                              0U) ||
        !SetHandleInformation(parent_response_read.get(),
                              HANDLE_FLAG_INHERIT,
                              0U)) {
        return {CompanionLaunchStatus::HandleRestrictionFailed,
                nullptr,
                win32_diagnostic("SetHandleInformation",
                                 GetLastError(),
                                 "parent pipe endpoints could not be made non-inheritable")};
    }

    AttributeList attributes;
    if (!attributes.initialize()) {
        return {CompanionLaunchStatus::HandleRestrictionFailed,
                nullptr,
                win32_diagnostic("InitializeProcThreadAttributeList",
                                 GetLastError(),
                                 "restricted child handle list initialization failed")};
    }
    std::array child_handles{child_command_read.get(),
                             child_response_write.get()};
    if (!attributes.restrict_handles(child_handles)) {
        return {CompanionLaunchStatus::HandleRestrictionFailed,
                nullptr,
                win32_diagnostic("UpdateProcThreadAttribute",
                                 GetLastError(),
                                 "restricted child handle list update failed")};
    }

    const auto handle_number = [](HANDLE handle) {
        return std::to_wstring(reinterpret_cast<std::uintptr_t>(handle));
    };
    std::wstring command_line = L"\"" + target_path->native() +
                                L"\" --session-high " +
                                std::to_wstring(marker.high) +
                                L" --session-low " +
                                std::to_wstring(marker.low) +
                                L" --command-read-handle " +
                                handle_number(child_command_read.get()) +
                                L" --response-write-handle " +
                                handle_number(child_response_write.get());
    std::vector<wchar_t> writable_command(command_line.begin(),
                                          command_line.end());
    writable_command.push_back(L'\0');

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = attributes.get();
    PROCESS_INFORMATION process_information{};
    const DWORD creation_flags =
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;
    if (!CreateProcessW(target_path->c_str(),
                        writable_command.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        creation_flags,
                        nullptr,
                        nullptr,
                        &startup.StartupInfo,
                        &process_information)) {
        return {CompanionLaunchStatus::ProcessCreationFailed,
                nullptr,
                win32_diagnostic("CreateProcessW",
                                 GetLastError(),
                                 "companion fixture process creation failed")};
    }

    UniqueHandle process{process_information.hProcess};
    UniqueHandle primary_thread{process_information.hThread};
    // Parent copies of child-only endpoints must close immediately so EOF and
    // broken-pipe semantics remain truthful.
    child_command_read.reset();
    child_response_write.reset();
    primary_thread.reset();

    auto impl = std::make_unique<Impl>(process.release(),
                                      parent_command_write.release(),
                                      parent_response_read.release(),
                                      process_information.dwProcessId,
                                      marker);

    CompanionDiagnostic security_diagnostic;
    const auto controller_security =
        query_security_facts(GetCurrentProcess(), security_diagnostic);
    if (!controller_security.has_value()) {
        return {CompanionLaunchStatus::SecurityQueryFailed,
                nullptr,
                std::move(security_diagnostic)};
    }
    const auto target_security =
        query_security_facts(impl->process, security_diagnostic);
    if (!target_security.has_value()) {
        return {CompanionLaunchStatus::SecurityQueryFailed,
                nullptr,
                std::move(security_diagnostic)};
    }
    impl->facts.controller_security = *controller_security;
    impl->facts.target_security = *target_security;
    if (*controller_security != *target_security) {
        return {CompanionLaunchStatus::SecurityMismatch,
                nullptr,
                adapter_diagnostic(
                    38U,
                    "GetTokenInformation",
                    "R1-C1 requires matching integrity, UIAccess, and AppContainer facts")};
    }

    auto handshake_frame = read_frame_with_timeout(impl->response_read,
                                                   impl->process,
                                                   handshake_timeout);
    if (handshake_frame.status == FrameReadStatus::Timeout) {
        return {CompanionLaunchStatus::HandshakeTimeout,
                nullptr,
                adapter_diagnostic(39U,
                                   "ReadFile",
                                   "companion handshake timed out")};
    }
    if (handshake_frame.status == FrameReadStatus::ProcessExited) {
        return {CompanionLaunchStatus::TargetExited,
                nullptr,
                adapter_diagnostic(40U,
                                   "WaitForMultipleObjects",
                                   "companion exited before handshake")};
    }
    if (handshake_frame.status != FrameReadStatus::Succeeded) {
        return {CompanionLaunchStatus::HandshakeProtocolFailure,
                nullptr,
                win32_diagnostic("ReadFile",
                                 handshake_frame.error,
                                 "companion handshake read failed")};
    }
    if (!frame_envelope_matches(handshake_frame.header,
                                marker,
                                protocol::FrameKind::Handshake,
                                0U)) {
        return {CompanionLaunchStatus::HandshakeProtocolFailure,
                nullptr,
                protocol_diagnostic(
                    41U, "FrameHeader", "companion handshake envelope mismatch")};
    }
    const auto handshake =
        decode_exact_payload<protocol::HandshakePayload>(handshake_frame.payload);
    if (!handshake.has_value()) {
        return {CompanionLaunchStatus::HandshakeProtocolFailure,
                nullptr,
                protocol_diagnostic(
                    42U, "HandshakePayload", "companion handshake size mismatch")};
    }
    CompanionDiagnostic handshake_diagnostic;
    {
        std::lock_guard lock{impl->mutex};
        if (!impl->accept_handshake_locked(*handshake, handshake_diagnostic)) {
            return {CompanionLaunchStatus::HandshakeRejected,
                    nullptr,
                    std::move(handshake_diagnostic)};
        }
    }

    auto session = std::unique_ptr<CompanionSession>{
        new CompanionSession(std::move(impl))};
    return {CompanionLaunchStatus::Succeeded, std::move(session), std::nullopt};
}

const CompanionLaunchFacts& CompanionSession::launch_facts() const noexcept {
    return impl_->facts;
}

bool CompanionSession::is_alive() noexcept {
    std::lock_guard lock{impl_->mutex};
    return impl_->refresh_process_state_locked();
}

std::vector<CompanionWindowToken> CompanionSession::tokens() const {
    std::lock_guard lock{impl_->mutex};
    if (!impl_->refresh_process_state_locked()) {
        return {};
    }
    const auto candidates = impl_->ledger.active_tokens();
    for (const auto& candidate : candidates) {
        static_cast<void>(impl_->resolve_locked(candidate));
    }
    return impl_->ledger.active_tokens();
}

std::optional<CompanionWindowToken> CompanionSession::token(
    const protocol::LogicalWindowId logical_id) const {
    const auto active = tokens();
    const auto found = std::find_if(active.begin(), active.end(), [&](const auto& value) {
        return value.logical_id() == logical_id;
    });
    if (found == active.end()) {
        return std::nullopt;
    }
    return *found;
}

bool CompanionSession::contains(const CompanionWindowToken& token_value) noexcept {
    std::lock_guard lock{impl_->mutex};
    return impl_->resolve_locked(token_value).value.has_value();
}

CompanionCommandResult CompanionSession::set_step(
    const std::uint64_t step_id,
    const std::uint32_t uncooperative_window_mask,
    const std::int32_t uncooperative_offset_x) {
    if (step_id == 0U ||
        !detail::is_valid_uncooperative_selection(
            uncooperative_window_mask,
            uncooperative_offset_x,
            protocol::kAllWindowMask)) {
        return {CompanionCommandStatus::InvalidRequest,
                protocol::CommandStatus::InvalidPayload,
                0U,
                adapter_diagnostic(
                    43U, "SetStep", "invalid step ID, mask, or test offset")};
    }
    const protocol::SetStepPayload payload{uncooperative_window_mask,
                                           uncooperative_offset_x,
                                           step_id};
    std::lock_guard lock{impl_->mutex};
    return impl_->command_locked(protocol::FrameKind::SetStep,
                                 &payload,
                                 sizeof(payload),
                                 std::chrono::seconds{2},
                                 std::nullopt,
                                 step_id);
}

CompanionCommandResult CompanionSession::destroy_window(
    const CompanionWindowToken& token_value) {
    std::lock_guard lock{impl_->mutex};
    auto resolved = impl_->resolve_locked(token_value);
    if (!resolved.value.has_value()) {
        const auto command_status =
            resolved.status == CompanionOperationStatus::SessionExited
                ? CompanionCommandStatus::SessionExited
                : CompanionCommandStatus::StaleToken;
        return {command_status,
                protocol::CommandStatus::InvalidWindow,
                0U,
                std::move(resolved.diagnostic)};
    }
    const protocol::DestroyWindowPayload payload{token_value.logical_id(), 0U};
    auto result = impl_->command_locked(protocol::FrameKind::DestroyWindow,
                                        &payload,
                                        sizeof(payload),
                                        std::chrono::seconds{2},
                                        token_value.logical_id());
    if (result.succeeded()) {
        static_cast<void>(impl_->ledger.retire(token_value.logical_id()));
    }
    return result;
}

CompanionEvidenceResult CompanionSession::query_evidence() {
    std::lock_guard lock{impl_->mutex};
    auto exchange = impl_->exchange_locked(protocol::FrameKind::QueryEvidence,
                                           nullptr,
                                           0U,
                                           protocol::FrameKind::Evidence);
    CompanionEvidenceResult result;
    result.status = exchange.status;
    result.request_id = exchange.request_id;
    result.diagnostic = std::move(exchange.diagnostic);
    if (exchange.status != CompanionCommandStatus::Succeeded) {
        return result;
    }
    auto evidence =
        decode_exact_payload<protocol::EvidencePayload>(exchange.payload);
    if (!evidence.has_value() ||
        evidence->record_count > protocol::kMaximumEvidenceRecords ||
        evidence->target_process_id != impl_->facts.target_process_id ||
        evidence->target_ui_thread_id != impl_->facts.target_ui_thread_id) {
        result.status = CompanionCommandStatus::ProtocolFailure;
        result.diagnostic = protocol_diagnostic(
            44U, "EvidencePayload", "companion evidence payload mismatch");
        return result;
    }
    for (std::size_t index = 0U; index < evidence->record_count; ++index) {
        const auto& record = evidence->records[index];
        if (!protocol::is_valid_logical_window_id(
                static_cast<std::uint32_t>(record.logical_id)) ||
            record.target_process_id != impl_->facts.target_process_id ||
            record.target_ui_thread_id != impl_->facts.target_ui_thread_id) {
            result.status = CompanionCommandStatus::ProtocolFailure;
            result.diagnostic = protocol_diagnostic(
                45U, "EvidenceRecord", "companion evidence identity mismatch");
            return result;
        }
    }
    result.evidence = std::move(evidence);
    return result;
}

CompanionShutdownResult CompanionSession::shutdown(
    const std::chrono::milliseconds graceful_timeout,
    const std::chrono::milliseconds terminate_timeout) {
    std::lock_guard lock{impl_->mutex};
    CompanionShutdownResult result;

    if (!impl_->refresh_process_state_locked()) {
        result.status = CompanionCommandStatus::Succeeded;
        result.process_signaled = true;
    } else {
        impl_->shutdown_started = true;
        const auto command = impl_->command_locked(protocol::FrameKind::Shutdown,
                                                   nullptr,
                                                   0U,
                                                   graceful_timeout);
        result.graceful_request_acknowledged = command.succeeded();
        if (!command.succeeded()) {
            result.diagnostic = command.diagnostic;
        }

        DWORD wait = WaitForSingleObject(
            impl_->process, bounded_wait_milliseconds(graceful_timeout));
        if (wait != WAIT_OBJECT_0) {
            result.terminate_fallback_used = true;
            if (!TerminateProcess(impl_->process, kFixtureFallbackExitCode)) {
                const DWORD terminate_error = GetLastError();
                // A graceful exit can win the narrow race between the first
                // wait and TerminateProcess. Treat only an actually signaled
                // process as successful; never hide another termination error.
                wait = WaitForSingleObject(impl_->process, 0U);
                if (wait != WAIT_OBJECT_0) {
                    result.status = CompanionCommandStatus::NativeFailure;
                    result.diagnostic = win32_diagnostic(
                        "TerminateProcess",
                        terminate_error,
                        "exact companion fixture cleanup fallback failed");
                    impl_->ledger.retire_all();
                    impl_->process_alive = false;
                    return result;
                }
            } else {
                wait = WaitForSingleObject(
                    impl_->process,
                    bounded_wait_milliseconds(terminate_timeout));
            }
        }
        result.process_signaled = wait == WAIT_OBJECT_0;
        result.status = result.process_signaled
                            ? CompanionCommandStatus::Succeeded
                            : CompanionCommandStatus::Timeout;
        if (!result.process_signaled) {
            result.diagnostic = adapter_diagnostic(
                46U,
                "WaitForSingleObject",
                "companion process did not signal after bounded cleanup");
        }
        impl_->process_alive = false;
        impl_->ledger.retire_all();
    }

    DWORD exit_code = 0U;
    if (impl_->process != nullptr &&
        GetExitCodeProcess(impl_->process, &exit_code)) {
        result.exit_code = exit_code;
    }
    return result;
}

namespace {

[[nodiscard]] CompanionOperationStatus translation_status(
    const operations::window_translation::TranslationPreparationStatus status) noexcept {
    using PreparationStatus =
        operations::window_translation::TranslationPreparationStatus;
    switch (status) {
    case PreparationStatus::Succeeded:
        return CompanionOperationStatus::Succeeded;
    case PreparationStatus::EmptyGeometry:
        return CompanionOperationStatus::EmptyGeometry;
    case PreparationStatus::ResizeRejected:
        return CompanionOperationStatus::ResizeRejected;
    case PreparationStatus::ArithmeticOverflow:
        return CompanionOperationStatus::ArithmeticOverflow;
    case PreparationStatus::NativeCoordinateOutOfRange:
        return CompanionOperationStatus::NativeCoordinateOutOfRange;
    }
    return CompanionOperationStatus::ArithmeticOverflow;
}

void verify_receipt(CompanionWindowOperationReceipt& receipt,
                    const CompanionWindowSnapshot& actual) noexcept {
    receipt.actual = actual;
    receipt.visible_target_verified =
        actual.visible_rect == receipt.requested_visible_rect;
    receipt.positioning_target_verified =
        receipt.requested_positioning_rect.has_value() &&
        actual.positioning_rect == *receipt.requested_positioning_rect;
    receipt.size_preserved =
        receipt.before.has_value() &&
        same_rect_size(receipt.before->visible_rect, actual.visible_rect) &&
        same_rect_size(receipt.before->positioning_rect, actual.positioning_rect);
    receipt.dpi_and_monitor_stable =
        receipt.before.has_value() && receipt.before->dpi == actual.dpi &&
        receipt.before->monitor_device_name == actual.monitor_device_name &&
        receipt.before->target_process_id == actual.target_process_id &&
        receipt.before->target_thread_id == actual.target_thread_id;
}

struct PreparedCompanionOperation {
    CompanionWindowToken token;
    HWND window{};
    core::geometry::Rect target_positioning_rect;
};

[[nodiscard]] CompanionDiagnostic geometry_diagnostic() {
    return adapter_diagnostic(
        47U,
        "prepare_visible_translation",
        "visible target is not a representable pure translation");
}

} // namespace

void CompanionSession::Impl::recapture_locked(
    CompanionOperationResult& result,
    const bool after_native_failure) {
    result.verified_count = 0U;
    result.recaptured_after_native_failure = after_native_failure;
    for (auto& receipt : result.windows) {
        const auto capture = capture_locked(receipt.token);
        if (!capture.succeeded()) {
            continue;
        }
        verify_receipt(receipt, *capture.snapshot);
        if (receipt.visible_target_verified &&
            receipt.positioning_target_verified && receipt.size_preserved &&
            receipt.dpi_and_monitor_stable) {
            ++result.verified_count;
        }
    }
}

CompanionWindowOperations::CompanionWindowOperations(
    CompanionSession& session) noexcept
    : session_(&session) {}

CompanionCaptureResult CompanionWindowOperations::capture(
    const CompanionWindowToken& token) {
    std::lock_guard lock{session_->impl_->mutex};
    return session_->impl_->capture_locked(token);
}

CompanionOperationResult CompanionWindowOperations::apply_one(
    const CompanionWindowTranslationRequest& request,
    const CompanionApplyOptions options) {
    CompanionOperationResult result;
    result.requested_count = 1U;
    result.operation_batch_id = issue_monotonic(next_operation_id);
    result.windows.push_back(CompanionWindowOperationReceipt{
        request.token,
        request.token.session_authority(),
        0U,
        0U,
        request.target_visible_rect});

    const std::uint32_t requested_mask =
        protocol::logical_window_bit(request.token.logical_id());
    if (result.operation_batch_id == 0U ||
        !detail::is_valid_uncooperative_selection(
            options.uncooperative_window_mask,
            options.uncooperative_offset_x,
            requested_mask)) {
        result.status = CompanionOperationStatus::InvalidRequest;
        result.diagnostic = adapter_diagnostic(
            48U,
            "CompanionWindowOperations",
            "operation ID exhausted or invalid fixture apply options");
        return result;
    }
    std::lock_guard lock{session_->impl_->mutex};
    const std::array tokens{request.token};
    const auto token_validation = detail::validate_companion_batch_tokens(
        tokens, session_->impl_->ledger);
    if (token_validation != detail::CompanionBatchTokenValidation::Succeeded) {
        result.status = session_->impl_->refresh_process_state_locked()
                            ? CompanionOperationStatus::StaleToken
                            : CompanionOperationStatus::SessionExited;
        result.diagnostic = adapter_diagnostic(
            50U,
            "CompanionTokenLedger",
            "single companion translation token is stale");
        return result;
    }

    auto resolved = session_->impl_->resolve_locked(request.token);
    if (!resolved.value.has_value()) {
        result.status = resolved.status;
        result.diagnostic = std::move(resolved.diagnostic);
        return result;
    }
    auto captured = capture_native_window(resolved.value->window,
                                          resolved.value->entry.process_id,
                                          resolved.value->entry.creator_thread_id);
    if (!captured.succeeded()) {
        result.status = captured.status;
        result.diagnostic = std::move(captured.diagnostic);
        return result;
    }
    const auto preparation =
        operations::window_translation::prepare_visible_translation(
            captured.snapshot->positioning_rect,
            captured.snapshot->visible_rect,
            request.target_visible_rect);
    if (preparation.status != operations::window_translation::
                                  TranslationPreparationStatus::Succeeded) {
        result.status = translation_status(preparation.status);
        result.diagnostic = geometry_diagnostic();
        return result;
    }
    auto confirmed = session_->impl_->resolve_locked(request.token);
    if (!confirmed.value.has_value() ||
        confirmed.value->window != resolved.value->window) {
        result.status = CompanionOperationStatus::WindowInvalidatedDuringApply;
        result.diagnostic = confirmed.diagnostic.has_value()
                                ? std::move(confirmed.diagnostic)
                                : std::optional<CompanionDiagnostic>{
                                      adapter_diagnostic(
                                          51U,
                                          "CompanionTokenLedger",
                                          "companion HWND changed during single preflight")};
        return result;
    }

    auto& receipt = result.windows.front();
    receipt.before = *captured.snapshot;
    receipt.requested_positioning_rect = *preparation.target_positioning_rect;
    receipt.target_process_id = resolved.value->entry.process_id;
    receipt.target_thread_id = resolved.value->entry.creator_thread_id;

    result.stage = CompanionOperationStage::TargetArm;
    const protocol::SetStepPayload step_payload{
        options.uncooperative_window_mask,
        options.uncooperative_offset_x,
        result.operation_batch_id};
    const auto armed = session_->impl_->command_locked(
        protocol::FrameKind::SetStep,
        &step_payload,
        sizeof(step_payload),
        std::chrono::seconds{2},
        std::nullopt,
        result.operation_batch_id);
    if (!armed.succeeded()) {
        result.status = CompanionOperationStatus::TargetArmFailed;
        result.diagnostic = armed.diagnostic;
        return result;
    }
    confirmed = session_->impl_->resolve_locked(request.token);
    if (!confirmed.value.has_value() ||
        confirmed.value->window != resolved.value->window) {
        result.status = CompanionOperationStatus::WindowInvalidatedDuringApply;
        result.diagnostic = confirmed.diagnostic;
        return result;
    }

    const auto& target = *preparation.target_positioning_rect;
    result.stage = CompanionOperationStage::SingleApply;
    result.native_apply_attempted = true;
    if (!SetWindowPos(resolved.value->window,
                      nullptr,
                      static_cast<int>(target.left()),
                      static_cast<int>(target.top()),
                      0,
                      0,
                      kTranslationFlags)) {
        result.status = CompanionOperationStatus::SingleApplyFailed;
        result.native_outcome_known = false;
        result.diagnostic = win32_diagnostic(
            "SetWindowPos",
            GetLastError(),
            "cross-process single apply failed; no rollback is assumed");
        session_->impl_->recapture_locked(result, true);
        return result;
    }

    result.stage = CompanionOperationStage::PostVerification;
    session_->impl_->recapture_locked(result, false);
    if (!result.windows.front().actual.has_value()) {
        result.status = CompanionOperationStatus::WindowInvalidatedDuringApply;
        result.diagnostic = adapter_diagnostic(
            52U,
            "CompanionWindowOperations",
            "companion window could not be captured after single apply");
        return result;
    }
    if (result.verified_count != 1U) {
        result.status = CompanionOperationStatus::PostVerificationFailed;
        result.diagnostic = adapter_diagnostic(
            53U,
            "CompanionWindowOperations",
            "requested and actual geometry or context differ");
        return result;
    }
    result.status = CompanionOperationStatus::Succeeded;
    return result;
}

CompanionOperationResult CompanionWindowOperations::apply(
    const std::span<const CompanionWindowTranslationRequest> requests,
    const CompanionApplyOptions options) {
    CompanionOperationResult result;
    result.requested_count = requests.size();
    result.operation_batch_id = issue_monotonic(next_operation_id);
    result.windows.reserve(requests.size());

    std::vector<CompanionWindowToken> tokens;
    tokens.reserve(requests.size());
    std::uint32_t requested_mask = 0U;
    for (const auto& request : requests) {
        result.windows.push_back(CompanionWindowOperationReceipt{
            request.token,
            request.token.session_authority(),
            0U,
            0U,
            request.target_visible_rect});
        tokens.push_back(request.token);
        requested_mask |= protocol::logical_window_bit(request.token.logical_id());
    }

    if (result.operation_batch_id == 0U ||
        !detail::is_valid_uncooperative_selection(
            options.uncooperative_window_mask,
            options.uncooperative_offset_x,
            requested_mask)) {
        result.status = CompanionOperationStatus::InvalidRequest;
        result.diagnostic = adapter_diagnostic(
            54U,
            "CompanionWindowOperations",
            "operation ID exhausted or invalid fixture apply options");
        return result;
    }

    std::lock_guard lock{session_->impl_->mutex};
    const auto token_validation = detail::validate_companion_batch_tokens(
        tokens, session_->impl_->ledger);
    switch (token_validation) {
    case detail::CompanionBatchTokenValidation::Empty:
        result.status = CompanionOperationStatus::InvalidRequest;
        result.diagnostic = adapter_diagnostic(
            55U, "CompanionWindowOperations", "translation batch is empty");
        return result;
    case detail::CompanionBatchTokenValidation::Duplicate:
        result.status = CompanionOperationStatus::DuplicateToken;
        result.diagnostic = adapter_diagnostic(
            56U, "CompanionWindowOperations", "translation batch has a duplicate token");
        return result;
    case detail::CompanionBatchTokenValidation::Unknown:
        result.status = session_->impl_->refresh_process_state_locked()
                            ? CompanionOperationStatus::StaleToken
                            : CompanionOperationStatus::SessionExited;
        result.diagnostic = adapter_diagnostic(
            57U, "CompanionTokenLedger", "translation batch has a stale token");
        return result;
    case detail::CompanionBatchTokenValidation::Succeeded:
        break;
    }
    if (requests.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        result.status = CompanionOperationStatus::InvalidRequest;
        result.diagnostic = adapter_diagnostic(
            58U, "BeginDeferWindowPos", "batch count is not representable as int");
        return result;
    }

    std::vector<PreparedCompanionOperation> prepared;
    prepared.reserve(requests.size());
    for (std::size_t index = 0U; index < requests.size(); ++index) {
        const auto& request = requests[index];
        auto resolved = session_->impl_->resolve_locked(request.token);
        if (!resolved.value.has_value()) {
            result.status = resolved.status;
            result.diagnostic = std::move(resolved.diagnostic);
            return result;
        }
        auto captured = capture_native_window(resolved.value->window,
                                              resolved.value->entry.process_id,
                                              resolved.value->entry.creator_thread_id);
        if (!captured.succeeded()) {
            result.status = captured.status;
            result.diagnostic = std::move(captured.diagnostic);
            return result;
        }
        const auto preparation =
            operations::window_translation::prepare_visible_translation(
                captured.snapshot->positioning_rect,
                captured.snapshot->visible_rect,
                request.target_visible_rect);
        if (preparation.status != operations::window_translation::
                                      TranslationPreparationStatus::Succeeded) {
            result.status = translation_status(preparation.status);
            result.diagnostic = geometry_diagnostic();
            return result;
        }
        auto confirmed = session_->impl_->resolve_locked(request.token);
        if (!confirmed.value.has_value() ||
            confirmed.value->window != resolved.value->window) {
            result.status = CompanionOperationStatus::WindowInvalidatedDuringApply;
            result.diagnostic = confirmed.diagnostic.has_value()
                                    ? std::move(confirmed.diagnostic)
                                    : std::optional<CompanionDiagnostic>{
                                          adapter_diagnostic(
                                              59U,
                                              "CompanionTokenLedger",
                                              "companion HWND changed during batch preflight")};
            return result;
        }

        auto& receipt = result.windows[index];
        receipt.before = *captured.snapshot;
        receipt.requested_positioning_rect = *preparation.target_positioning_rect;
        receipt.target_process_id = resolved.value->entry.process_id;
        receipt.target_thread_id = resolved.value->entry.creator_thread_id;
        prepared.push_back(PreparedCompanionOperation{
            request.token,
            resolved.value->window,
            *preparation.target_positioning_rect});
    }

    const HWND shared_parent = GetAncestor(prepared.front().window, GA_PARENT);
    if (std::any_of(prepared.begin(), prepared.end(), [&](const auto& operation) {
            return GetAncestor(operation.window, GA_PARENT) != shared_parent;
        })) {
        result.status = CompanionOperationStatus::InvalidRequest;
        result.diagnostic = adapter_diagnostic(
            60U,
            "GetAncestor",
            "DeferWindowPos batch members must share one native parent");
        return result;
    }
    for (const auto& operation : prepared) {
        auto resolved = session_->impl_->resolve_locked(operation.token);
        if (!resolved.value.has_value() ||
            resolved.value->window != operation.window) {
            result.status = CompanionOperationStatus::WindowInvalidatedDuringApply;
            result.diagnostic = resolved.diagnostic;
            return result;
        }
    }

    result.stage = CompanionOperationStage::TargetArm;
    const protocol::SetStepPayload step_payload{
        options.uncooperative_window_mask,
        options.uncooperative_offset_x,
        result.operation_batch_id};
    const auto armed = session_->impl_->command_locked(
        protocol::FrameKind::SetStep,
        &step_payload,
        sizeof(step_payload),
        std::chrono::seconds{2},
        std::nullopt,
        result.operation_batch_id);
    if (!armed.succeeded()) {
        result.status = CompanionOperationStatus::TargetArmFailed;
        result.diagnostic = armed.diagnostic;
        return result;
    }
    for (const auto& operation : prepared) {
        auto resolved = session_->impl_->resolve_locked(operation.token);
        if (!resolved.value.has_value() ||
            resolved.value->window != operation.window) {
            result.status = CompanionOperationStatus::WindowInvalidatedDuringApply;
            result.diagnostic = resolved.diagnostic;
            return result;
        }
    }

    result.stage = CompanionOperationStage::BeginBatch;
    HDWP deferred = BeginDeferWindowPos(static_cast<int>(prepared.size()));
    if (deferred == nullptr) {
        result.status = CompanionOperationStatus::BeginBatchFailed;
        result.diagnostic = win32_diagnostic(
            "BeginDeferWindowPos",
            GetLastError(),
            "cross-process native batch allocation failed");
        session_->impl_->recapture_locked(result, true);
        return result;
    }
    result.stage = CompanionOperationStage::DeferBatch;
    for (const auto& operation : prepared) {
        const auto& target = operation.target_positioning_rect;
        HDWP updated = DeferWindowPos(deferred,
                                     operation.window,
                                     nullptr,
                                     static_cast<int>(target.left()),
                                     static_cast<int>(target.top()),
                                     0,
                                     0,
                                     kTranslationFlags);
        if (updated == nullptr) {
            const DWORD error = GetLastError();
            deferred = nullptr;
            result.status = CompanionOperationStatus::DeferBatchFailed;
            result.diagnostic = win32_diagnostic(
                "DeferWindowPos",
                error,
                "native defer chain failed and was abandoned without End");
            session_->impl_->recapture_locked(result, true);
            return result;
        }
        deferred = updated;
        ++result.deferred_count;
    }

    result.stage = CompanionOperationStage::EndBatch;
    result.native_apply_attempted = true;
    if (!EndDeferWindowPos(deferred)) {
        result.status = CompanionOperationStatus::EndBatchFailed;
        result.native_outcome_known = false;
        result.diagnostic = win32_diagnostic(
            "EndDeferWindowPos",
            GetLastError(),
            "cross-process native batch failed; no rollback or atomicity is assumed");
        session_->impl_->recapture_locked(result, true);
        return result;
    }

    result.stage = CompanionOperationStage::PostVerification;
    session_->impl_->recapture_locked(result, false);
    const bool all_captured = std::all_of(
        result.windows.begin(), result.windows.end(), [](const auto& receipt) {
            return receipt.actual.has_value();
        });
    if (!all_captured) {
        result.status = CompanionOperationStatus::WindowInvalidatedDuringApply;
        result.diagnostic = adapter_diagnostic(
            61U,
            "CompanionWindowOperations",
            "one or more companion windows could not be captured after apply");
        return result;
    }
    if (result.verified_count != result.requested_count) {
        result.status = CompanionOperationStatus::PostVerificationFailed;
        result.diagnostic = adapter_diagnostic(
            62U,
            "CompanionWindowOperations",
            "requested and actual geometry or DPI/monitor context differ");
        return result;
    }
    result.status = CompanionOperationStatus::Succeeded;
    return result;
}

} // namespace panebind::platform::windows::companion
