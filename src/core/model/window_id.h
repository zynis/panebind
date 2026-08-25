#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace panebind::core::model {

class WindowId final {
public:
    WindowId() = delete;

    explicit WindowId(std::string identifier) : value_(std::move(identifier)) {
        if (value_.empty()) {
            throw std::invalid_argument{"WindowId must not be empty"};
        }
    }

    [[nodiscard]] const std::string& value() const noexcept { return value_; }

    friend bool operator==(const WindowId&, const WindowId&) = default;

private:
    std::string value_;
};

} // namespace panebind::core::model
