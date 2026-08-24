#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace snapweave::platform::windows {

struct Utf8ConversionResult {
    std::optional<std::string> value;
    unsigned long error_code{};
};

// Converts UTF-16 to strict UTF-8. Invalid surrogate sequences are rejected
// instead of being silently replaced so observer metadata remains auditable.
[[nodiscard]] Utf8ConversionResult utf16_to_utf8(std::wstring_view value);

// Returns a complete JSON string token, including its surrounding quotes.
// The input is expected to be valid UTF-8; non-ASCII bytes are retained.
[[nodiscard]] std::string json_quote(std::string_view value);

} // namespace snapweave::platform::windows
