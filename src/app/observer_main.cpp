#include "platform/windows/windows_observer.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <system_error>

namespace {

void print_usage(std::ostream& output) {
    output << "Usage:\n"
              "  snapweave-observer --enumerate-only\n"
              "  snapweave-observer --observe-seconds N\n"
              "  snapweave-observer\n"
              "  snapweave-observer --help\n";
}

[[nodiscard]] std::optional<std::chrono::seconds> parse_duration(
    const std::string_view value) {
    std::int64_t seconds = 0;
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto [position, error] = std::from_chars(begin, end, seconds);
    constexpr auto maximum_seconds =
        static_cast<std::int64_t>(USER_TIMER_MAXIMUM / 1000U);
    if (error != std::errc{} || position != end || seconds <= 0 ||
        seconds > maximum_seconds) {
        return std::nullopt;
    }
    return std::chrono::seconds{seconds};
}

} // namespace

int main(const int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    static_cast<void>(SetConsoleOutputCP(CP_UTF8));

    bool enumerate_only = false;
    std::optional<std::chrono::seconds> observation_duration;

    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        print_usage(std::cout);
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--enumerate-only") {
        enumerate_only = true;
    } else if (argc == 3 && std::string_view{argv[1]} == "--observe-seconds") {
        observation_duration = parse_duration(argv[2]);
        if (!observation_duration.has_value()) {
            std::cerr << "--observe-seconds requires a supported positive integer.\n";
            print_usage(std::cerr);
            return 2;
        }
    } else if (argc != 1) {
        std::cerr << "Unknown or incompatible command-line arguments.\n";
        print_usage(std::cerr);
        return 2;
    }

    snapweave::platform::windows::WindowsObserver observer{std::cout};
    return enumerate_only ? observer.enumerate_only()
                          : observer.observe(observation_duration);
}
