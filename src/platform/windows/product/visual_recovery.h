#pragma once

#include "window_catalog.h"
#include "core/v1/visual_ownership.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace panebind::platform::windows::product {

// Owner-STA client for a same-executable/same-Medium helper. No helper exists
// until an actual pending owned visual change needs a before-write receipt.
// This is cleanup authority only: no arbitrary command, geometry or input API.
class VisualRecovery final {
public:
    VisualRecovery();
    ~VisualRecovery();
    VisualRecovery(const VisualRecovery&) = delete;
    VisualRecovery& operator=(const VisualRecovery&) = delete;
    // original + intended applied tuple must be recorded BEFORE native mutation.
    // A false receipt forbids that mutation; it must not be treated as optional.
    bool remember(const Snapshot&,const core::v1::VisualOwnership&);
    // Collapse a prepared transition only after actual native postverification.
    // Until this receipt, helper retains both prior and planned owned tuples.
    bool commit(const Snapshot&,const core::v1::VisualOwnership& actual);
    // Call only once actual restore/external replacement revoked our ownership.
    // Last removal causes helper exit; next actual mutation starts a new helper.
    bool forget(std::uint64_t parent_key);
    // Ask helper to restore its outstanding records, wait bounded, never hold
    // up low-level input cleanup. False is a real unconfirmed cleanup result.
    bool stop();
    [[nodiscard]] std::string error() const;
    [[nodiscard]] DWORD helper_pid() const;
    // argv excludes argv[0]/the executable, as in the production argument parser.
    // Unrelated commands return nullopt; malformed internal calls fail closed.
    static std::optional<int> helper_entry(const std::vector<std::wstring>& argv);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace panebind::platform::windows::product
