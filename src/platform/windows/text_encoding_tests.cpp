#include "platform/windows/text_encoding.h"

#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void expect_equal(const std::string_view actual,
                  const std::string_view expected,
                  const std::string_view description) {
    if (actual != expected) {
        std::cerr << description << " failed\nexpected: " << expected
                  << "\nactual:   " << actual << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    using panebind::platform::windows::json_quote;
    using panebind::platform::windows::utf16_to_utf8;

    const auto unicode = utf16_to_utf8(L"\u7a97\u53e3 \U0001F642");
    if (!unicode.value.has_value()) {
        std::cerr << "valid UTF-16 conversion failed with " << unicode.error_code << '\n';
        return 1;
    }
    expect_equal(*unicode.value,
                 "\xe7\xaa\x97\xe5\x8f\xa3 \xf0\x9f\x99\x82",
                 "non-ASCII UTF-8 conversion");
    expect_equal(json_quote(*unicode.value),
                 "\"\xe7\xaa\x97\xe5\x8f\xa3 \xf0\x9f\x99\x82\"",
                 "non-ASCII JSON preservation");

    const std::string controls{"quote=\" slash=\\ line=\n tab=\t byte=\x01"};
    expect_equal(json_quote(controls),
                 "\"quote=\\\" slash=\\\\ line=\\n tab=\\t byte=\\u0001\"",
                 "JSON escaping");

    const wchar_t invalid_surrogate[] = {static_cast<wchar_t>(0xd800), L'\0'};
    const auto invalid = utf16_to_utf8(invalid_surrogate);
    if (invalid.value.has_value()) {
        std::cerr << "invalid UTF-16 surrogate was accepted\n";
        ++failures;
    }

    return failures == 0 ? 0 : 1;
}
