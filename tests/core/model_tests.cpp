#include "core/events/window_event.h"
#include "core/model/window_snapshot.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace events = panebind::core::events;
namespace model = panebind::core::model;

namespace {

int failures = 0;

void expect(bool condition, std::string_view description) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

template <typename Action>
void expect_invalid_argument(Action action, std::string_view description) {
    try {
        action();
        expect(false, description);
    } catch (const std::invalid_argument&) {
        expect(true, description);
    } catch (...) {
        expect(false, description);
    }
}

void test_required_identity() {
    expect_invalid_argument([] { [[maybe_unused]] events::WindowId id{""}; },
                            "empty window identifier is rejected");
    expect_invalid_argument([] { [[maybe_unused]] model::ProcessId id{0}; },
                            "zero process identifier is rejected");

    const events::WindowEvent event{events::WindowEventType::GeometryChanged,
                                    events::WindowId{"win32:0x1"}};
    expect(event.window_id.value() == "win32:0x1", "event retains opaque identity");

    const model::NormalizedWindowSnapshot snapshot{events::WindowId{"win32:0x2"},
                                                   model::ProcessId{42}};
    expect(snapshot.id.value() == "win32:0x2", "snapshot requires window identity");
    expect(snapshot.process_id.value() == 42, "snapshot requires process identity");
}

void test_utf8_contract() {
    model::NormalizedWindowSnapshot snapshot{events::WindowId{"win32:0x3"},
                                             model::ProcessId{7}};
    const std::string utf8_title{"\xE7\xAA\x97\xE5\x8F\xA3"};
    snapshot.title = utf8_title;
    expect(snapshot.title == utf8_title, "normalized text retains UTF-8 bytes");
}

} // namespace

int main() {
    test_required_identity();
    test_utf8_contract();

    if (failures != 0) {
        std::cerr << failures << " core model test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All core model tests passed\n";
    return EXIT_SUCCESS;
}

