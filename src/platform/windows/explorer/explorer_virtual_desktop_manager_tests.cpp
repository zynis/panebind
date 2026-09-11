#include "platform/windows/explorer/explorer_virtual_desktop_manager.h"
#include <array>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace e = panebind::platform::windows::explorer;
namespace {
int failures{};
void check(bool ok, const char* message) { if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; } }
struct State {
    unsigned creates{}, queries{}, releases{};
    HRESULT create_result{S_OK}, query_result{S_OK};
    BOOL current{TRUE};
    HWND last_frame{};
    bool release_on_sta{};
};
State* fixture_state{}; // test factory state, never a global COM service
class Fake final : public IVirtualDesktopManager {
public:
    explicit Fake(State& state) : state_(state) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** value) override { *value = nullptr; return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto count = --refs_;
        if (!count) {
            ++state_.releases;
            APTTYPE type{}; APTTYPEQUALIFIER q{};
            state_.release_on_sta = CoGetApartmentType(&type, &q) == S_OK && (type == APTTYPE_STA || type == APTTYPE_MAINSTA);
            delete this;
        }
        return count;
    }
    HRESULT STDMETHODCALLTYPE IsWindowOnCurrentVirtualDesktop(HWND frame, BOOL* value) override {
        ++state_.queries; state_.last_frame = frame; *value = state_.current; return state_.query_result;
    }
    HRESULT STDMETHODCALLTYPE GetWindowDesktopId(HWND, GUID*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE MoveWindowToDesktop(HWND, REFGUID) override { return E_NOTIMPL; } // never control a desktop
private:
    State& state_; ULONG refs_{1};
};
HRESULT create_fake(IVirtualDesktopManager** manager) noexcept {
    ++fixture_state->creates; *manager = nullptr;
    if (fixture_state->create_result == S_OK) *manager = new Fake(*fixture_state);
    return fixture_state->create_result;
}
bool apartment_present() { APTTYPE t{}; APTTYPEQUALIFIER q{}; return CoGetApartmentType(&t, &q) == S_OK; }
void count_and_lifetime(bool profile_on) {
    State state; fixture_state = &state;
    check(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)), "owner STA initialization");
    e::ConsentValidationAudit audit; audit.vdm_enabled = true;
    e::ConsentValidationAuditScope audit_scope(&audit);
    {
        e::ExplorerVirtualDesktopManager manager(create_fake);
        auto profile = profile_on ? std::make_unique<e::ExplorerGlueProfiler>() : nullptr;
        e::ConsentValidationPhaseScope active(e::ConsentValidationPhase::Active);
        for (int operation = 0; operation < 60; ++operation) for (int point = 0; point < 6; ++point) {
            const auto frame = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(point % 2 + 1));
            check(manager.query(frame, profile.get()).eligible() && state.last_frame == frame, "each validation queries its current exact frame");
        }
        check(state.creates == 1 && state.queries == 360 && manager.query_count() == 360, "60-op-equivalent: one manager, every required query");
        check(audit.manager_creates[2] == 0 && audit.desktop_queries[2] == 360, "active creates zero, actual fresh query count preserved");
        check(!profile || (profile->valid() && profile->spans().size() == 360), "profiling ON/OFF preserves outcomes and queries");
        check(manager.close() && manager.close() && manager.closed() && state.releases == 1 && state.release_on_sta, "idempotent close releases once while STA alive");
        check(!manager.query(reinterpret_cast<HWND>(1)).eligible() && state.queries == 360, "closed manager never queries/recreates");
    }
    check(apartment_present() && state.releases == 1, "manager balanced only its own COM ref, no double release");
    CoUninitialize();
    check(!apartment_present(), "all references balanced; no release after COM shutdown");
}
void failures_and_freshness() {
    State state; fixture_state = &state;
    { e::ExplorerVirtualDesktopManager no_apartment(create_fake); check(no_apartment.creation_result() == CO_E_NOTINITIALIZED && state.creates == 0, "uninitialized apartment fails before activation"); }
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    { e::ExplorerVirtualDesktopManager mta(create_fake); check(mta.creation_result() == RPC_E_CHANGED_MODE && state.creates == 0, "MTA rejected without changing its apartment"); }
    CoUninitialize();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    {
        e::ExplorerVirtualDesktopManager manager(create_fake);
        unsigned writes = 0;
        if (manager.query(reinterpret_cast<HWND>(1)).eligible()) ++writes;
        state.current = FALSE;
        if (manager.query(reinterpret_cast<HWND>(1)).eligible()) ++writes;
        check(writes == 1 && state.queries == 2, "desktop true->false is fresh and blocks next modeled native write");
        state.current = TRUE; state.query_result = E_FAIL;
        check(!manager.query(reinterpret_cast<HWND>(1)).eligible() && state.creates == 1 && state.queries == 3, "query failure does not reuse true or recreate/retry");
        state.query_result = S_FALSE;
        check(!manager.query(reinterpret_cast<HWND>(1)).eligible(), "exact S_OK semantics, not generic SUCCEEDED");
        const auto before = state.queries;
        std::thread foreign([&] {
            check(manager.query(reinterpret_cast<HWND>(1)).result == RPC_E_WRONG_THREAD, "foreign thread cannot invoke service");
            check(!manager.close(), "foreign thread cannot Release or CoUninitialize owner");
        }); foreign.join();
        check(state.queries == before && manager.close() && state.releases == 1, "owner safely cleans up after wrong-thread/query errors");
    }
    state = {}; state.create_result = E_FAIL;
    { e::ExplorerVirtualDesktopManager failed(create_fake);
      for (int i = 0; i < 3; ++i) check(!failed.query(reinterpret_cast<HWND>(1)).eligible(), "create failure stays unavailable");
      check(state.creates == 1 && state.queries == 0, "no lazy reacquisition after create failure"); }
    state = {};
    try { e::ExplorerVirtualDesktopManager unwind(create_fake); throw std::runtime_error("fixture"); } catch (const std::runtime_error&) {}
    check(state.releases == 1 && state.release_on_sta, "exception cleanup releases before apartment shutdown");
    CoUninitialize(); check(!apartment_present(), "failure paths balance own COM references");
}
void native_read_only_probe() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const auto frame = CreateWindowExW(0, L"STATIC", L"PaneBind VDM read-only fixture", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    check(frame != nullptr, "owned probe frame created, not user Explorer");
    {
        e::ExplorerVirtualDesktopManager manager;
        check(manager.creation_result() == S_OK, "real VDM acquired on owner STA");
        const auto answer = manager.query(frame);
        check(answer.result == S_OK && manager.query_count() == 1, "real read-only owned-frame desktop query completed");
        check(manager.close() && manager.release_count() == 1, "real interface released before COM shutdown");
    }
    if (frame) DestroyWindow(frame);
    CoUninitialize();
}
}
int main() {
    static_assert(!std::is_copy_constructible_v<e::ExplorerVirtualDesktopManager> && !std::is_move_constructible_v<e::ExplorerVirtualDesktopManager>);
    count_and_lifetime(false); count_and_lifetime(true); failures_and_freshness(); native_read_only_probe();
    std::cout << "VDM lifetime/freshness tests " << (failures ? "FAIL" : "PASS") << "; modeled operations=60, manager=1, queries=360, active creates=0\n";
    return failures ? 1 : 0;
}
