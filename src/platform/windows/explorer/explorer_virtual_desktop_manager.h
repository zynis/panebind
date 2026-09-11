#pragma once

#include "platform/windows/explorer/explorer_consent_validation.h"
#include "platform/windows/explorer/explorer_glue_profile.h"
#include <objbase.h>
#include <shobjidl_core.h>
#include <ShObjIdl.h>
#include <utility>

namespace panebind::platform::windows::explorer {

struct VirtualDesktopQueryResult {
    HRESULT result{E_FAIL};
    BOOL current{FALSE};
    [[nodiscard]] bool eligible() const noexcept { return result == S_OK && current != FALSE; }
};

// A read-only service, NOT a window capability. Owner-STA lifetime is strictly
// narrower than the private Glue session. No static COM pointer or BOOL cache.
class ExplorerVirtualDesktopManager final {
public:
    using Factory = HRESULT (*)(IVirtualDesktopManager**) noexcept;
    explicit ExplorerVirtualDesktopManager(Factory factory = &create_native) noexcept
        : owner_thread_(GetCurrentThreadId()) {
        creation_result_ = owner_apartment();
        if (creation_result_ != S_OK) return;
        const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (initialized != S_OK && initialized != S_FALSE) { creation_result_ = initialized; return; }
        com_reference_ = true;
        audit_vdm_create(true);
        ++create_count_;
        creation_result_ = factory ? factory(&manager_) : E_INVALIDARG;
        if (creation_result_ != S_OK || manager_ == nullptr) {
            if (manager_) { audit_vdm_release(true); ++release_count_; std::exchange(manager_, nullptr)->Release(); }
            if (creation_result_ == S_OK) creation_result_ = E_POINTER;
        }
    }
    ~ExplorerVirtualDesktopManager() noexcept { static_cast<void>(close()); }
    ExplorerVirtualDesktopManager(const ExplorerVirtualDesktopManager&) = delete;
    ExplorerVirtualDesktopManager& operator=(const ExplorerVirtualDesktopManager&) = delete;
    ExplorerVirtualDesktopManager(ExplorerVirtualDesktopManager&&) = delete;
    ExplorerVirtualDesktopManager& operator=(ExplorerVirtualDesktopManager&&) = delete;

    [[nodiscard]] VirtualDesktopQueryResult query(HWND authorized_frame, ExplorerGlueProfiler* profile = nullptr) noexcept {
        const auto apartment = owner_apartment();
        if (apartment != S_OK) return {apartment, FALSE};
        if (closed_ || creation_result_ != S_OK || manager_ == nullptr)
            return {closed_ ? E_UNEXPECTED : creation_result_, FALSE};
        GlueProfileScope measured(profile, GlueProfileStage::VirtualDesktopQuery);
        audit_vdm_query();
        ++query_count_;
        BOOL current = FALSE; // new local output for EVERY call, never reused
        const auto result = manager_->IsWindowOnCurrentVirtualDesktop(authorized_frame, &current);
        return {result, current};
    }
    [[nodiscard]] bool close() noexcept {
        if (GetCurrentThreadId() != owner_thread_) return false;
        if (closed_) return true;
        if (com_reference_ && owner_apartment() != S_OK) return false;
        closed_ = true;
        if (manager_) { audit_vdm_release(true); ++release_count_; std::exchange(manager_, nullptr)->Release(); }
        if (std::exchange(com_reference_, false)) CoUninitialize();
        return true;
    }
    [[nodiscard]] DWORD owner_thread() const noexcept { return owner_thread_; }
    [[nodiscard]] HRESULT creation_result() const noexcept { return creation_result_; }
    [[nodiscard]] bool closed() const noexcept { return closed_; }
    [[nodiscard]] std::uint64_t create_count() const noexcept { return create_count_; }
    [[nodiscard]] std::uint64_t query_count() const noexcept { return query_count_; }
    [[nodiscard]] std::uint64_t release_count() const noexcept { return release_count_; }
private:
    static HRESULT create_native(IVirtualDesktopManager** value) noexcept {
        return CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(value));
    }
    [[nodiscard]] HRESULT owner_apartment() const noexcept {
        if (GetCurrentThreadId() != owner_thread_) return RPC_E_WRONG_THREAD;
        APTTYPE type{}; APTTYPEQUALIFIER qualifier{};
        const auto result = CoGetApartmentType(&type, &qualifier);
        if (result != S_OK) return result;
        return type == APTTYPE_STA || type == APTTYPE_MAINSTA ? S_OK : RPC_E_CHANGED_MODE;
    }
    DWORD owner_thread_{};
    IVirtualDesktopManager* manager_{};
    HRESULT creation_result_{E_FAIL};
    bool com_reference_{}, closed_{};
    std::uint64_t create_count_{}, query_count_{}, release_count_{};
};
} // namespace panebind::platform::windows::explorer
