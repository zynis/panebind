#include "core/geometry/geometry.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

namespace geometry = snapweave::core::geometry;

namespace {

int failures = 0;

void expect(bool condition, std::string_view description) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

void test_size_and_normalization() {
    const geometry::Rect rect{30, 50, 10, 20};
    expect(rect.left() == 10 && rect.top() == 20, "coordinates normalize minima");
    expect(rect.right() == 30 && rect.bottom() == 50, "coordinates normalize maxima");
    expect(rect.size() == geometry::Size{20, 30}, "width and height use normalized edges");
    expect(!rect.empty(), "positive-area rectangle is not empty");
    expect(geometry::Rect{1, 1, 1, 9}.empty(), "zero-width rectangle is empty");
}

void test_intersection_and_overlap() {
    const geometry::Rect first{0, 0, 100, 100};
    const geometry::Rect second{60, 25, 140, 80};
    const auto overlap = geometry::intersection(first, second);

    expect(overlap == std::optional{geometry::Rect{60, 25, 100, 80}},
           "intersection returns shared positive area");
    expect(geometry::overlaps(first, second), "positive-area rectangles overlap");
    expect(geometry::horizontal_overlap(first, second) == 40,
           "horizontal overlap is calculated independently");
    expect(geometry::vertical_overlap(first, second) == 55,
           "vertical overlap is calculated independently");

    const geometry::Rect edge_touching{100, 10, 120, 90};
    expect(!geometry::intersection(first, edge_touching).has_value(),
           "edge contact has no positive-area intersection");
    expect(!geometry::overlaps(first, edge_touching),
           "edge contact is not rectangle overlap");
}

void test_edge_distance() {
    const geometry::Rect first{10, 20, 50, 70};
    const geometry::Rect second{57, 18, 90, 75};

    expect(geometry::edge_distance(first,
                                   geometry::Edge::Right,
                                   second,
                                   geometry::Edge::Left) == 7,
           "distance between opposing vertical edges");
    expect(geometry::edge_distance(first,
                                   geometry::Edge::Top,
                                   second,
                                   geometry::Edge::Bottom) == 55,
           "distance between horizontal edges");
}

void test_tolerance() {
    expect(geometry::within_tolerance(0, 0), "zero distance meets zero tolerance");
    expect(geometry::within_tolerance(8, 8), "distance equal to tolerance is accepted");
    expect(!geometry::within_tolerance(9, 8), "distance above tolerance is rejected");
    expect(!geometry::within_tolerance(1, -1), "negative tolerance is rejected");
    expect(!geometry::within_tolerance(-1, 4), "negative distance is rejected");
}

} // namespace

int main() {
    test_size_and_normalization();
    test_intersection_and_overlap();
    test_edge_distance();
    test_tolerance();

    if (failures != 0) {
        std::cerr << failures << " geometry test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All geometry tests passed\n";
    return EXIT_SUCCESS;
}

