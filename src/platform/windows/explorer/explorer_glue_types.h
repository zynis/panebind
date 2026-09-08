#pragma once

#include <cstdint>

namespace panebind::platform::windows::explorer {

enum class ExplorerGlueWindowRole : std::uint8_t {
    Leader,
    Follower,
};

} // namespace panebind::platform::windows::explorer
