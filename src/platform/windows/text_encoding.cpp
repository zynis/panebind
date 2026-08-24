#include "platform/windows/text_encoding.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <limits>
#include <utility>

namespace snapweave::platform::windows {

Utf8ConversionResult utf16_to_utf8(const std::wstring_view value) {
    if (value.empty()) {
        return {std::string{}, ERROR_SUCCESS};
    }

    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return {std::nullopt, ERROR_INSUFFICIENT_BUFFER};
    }

    const auto input_size = static_cast<int>(value.size());
    const int output_size = WideCharToMultiByte(CP_UTF8,
                                                WC_ERR_INVALID_CHARS,
                                                value.data(),
                                                input_size,
                                                nullptr,
                                                0,
                                                nullptr,
                                                nullptr);
    if (output_size == 0) {
        return {std::nullopt, GetLastError()};
    }

    std::string output(static_cast<std::size_t>(output_size), '\0');
    const int converted = WideCharToMultiByte(CP_UTF8,
                                              WC_ERR_INVALID_CHARS,
                                              value.data(),
                                              input_size,
                                              output.data(),
                                              output_size,
                                              nullptr,
                                              nullptr);
    if (converted == 0) {
        return {std::nullopt, GetLastError()};
    }

    return {std::move(output), ERROR_SUCCESS};
}

std::string json_quote(const std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";

    std::string output;
    output.reserve(value.size() + 2U);
    output.push_back('"');

    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (character < 0x20U) {
                output += "\\u00";
                output.push_back(hex[(character >> 4U) & 0x0fU]);
                output.push_back(hex[character & 0x0fU]);
            } else {
                output.push_back(static_cast<char>(character));
            }
            break;
        }
    }

    output.push_back('"');
    return output;
}

} // namespace snapweave::platform::windows
