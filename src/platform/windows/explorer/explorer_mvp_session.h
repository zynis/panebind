#pragma once

#include "platform/windows/explorer/explorer_group_session.h"

#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace panebind::platform::windows::explorer {

// One consent-bound product entry: ordinary Move may be taken over only after
// real Raw/WinEvent attribution, an observed shield, native END and a fresh
// three-frame capture. Ctrl Move stays in the native leader/Glue route; Resize
// is native. The owner is the provisioning STA, including every Shell capture
// and source SetWindowPos. No arbitrary HWND admission is exposed.
class ExplorerMvpSession final {
public:
    using OwnedMembers = ExplorerGroupSession::OwnedMembers;

    [[nodiscard]] static std::unique_ptr<ExplorerMvpSession>
    create_after_explicit_consent(OwnedMembers members,
                                  std::function<bool()> environment_allowed);
    ~ExplorerMvpSession();
    ExplorerMvpSession(const ExplorerMvpSession&) = delete;
    ExplorerMvpSession& operator=(const ExplorerMvpSession&) = delete;

    // Call from the provisioning STA's normal message/console pump. Neither
    // method creates synthetic input or blocks waiting for a drag to finish.
    [[nodiscard]] bool pump();
    [[nodiscard]] std::optional<GroupProductStatus> status();
    [[nodiscard]] bool stop();
    [[nodiscard]] bool healthy() const noexcept;
    [[nodiscard]] std::string_view reason() const noexcept;
    [[nodiscard]] const std::array<detail::GroupMemberBinding, 3>& bindings() const noexcept;

private:
    struct Impl;
    explicit ExplorerMvpSession(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;
};

} // namespace panebind::platform::windows::explorer
