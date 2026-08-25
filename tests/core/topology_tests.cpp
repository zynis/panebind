#include "core/topology/window_adjacency.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace geometry = panebind::core::geometry;
namespace model = panebind::core::model;
namespace topology = panebind::core::topology;

namespace {

int failures = 0;

void expect(bool condition, std::string_view description) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

template <typename Exception, typename Action>
void expect_throws(Action action, std::string_view description) {
    try {
        action();
        expect(false, description);
    } catch (const Exception&) {
        expect(true, description);
    } catch (...) {
        expect(false, description);
    }
}

[[nodiscard]] topology::WindowGeometry window(std::string id,
                                              geometry::Rect visible_rect) {
    return {model::WindowId{std::move(id)}, visible_rect};
}

[[nodiscard]] topology::WindowAdjacencyGraph
build_graph(const std::vector<topology::WindowGeometry>& windows,
            geometry::Distance tolerance = 0) {
    return topology::WindowAdjacencyGraph::build(
        std::span<const topology::WindowGeometry>{windows},
        topology::AdjacencyOptions{tolerance});
}

void expect_relation(const std::optional<topology::AdjacencyRelation>& relation,
                     std::string_view first,
                     std::string_view second,
                     geometry::Edge first_edge,
                     geometry::Edge second_edge,
                     geometry::Distance signed_gap,
                     geometry::Distance overlap,
                     std::string_view description) {
    if (!relation.has_value()) {
        expect(false, description);
        return;
    }

    expect(relation->first.value() == first && relation->second.value() == second &&
               relation->first_edge == first_edge &&
               relation->second_edge == second_edge &&
               relation->signed_gap == signed_gap &&
               relation->orthogonal_overlap == overlap,
           description);
}

[[nodiscard]] std::vector<std::string>
window_ids(std::span<const topology::WindowGeometry> windows) {
    std::vector<std::string> result;
    result.reserve(windows.size());
    for (const auto& item : windows) {
        result.push_back(item.id.value());
    }
    return result;
}

[[nodiscard]] std::vector<std::string>
window_ids(const std::vector<model::WindowId>& windows) {
    std::vector<std::string> result;
    result.reserve(windows.size());
    for (const auto& id : windows) {
        result.push_back(id.value());
    }
    return result;
}

[[nodiscard]] std::vector<topology::AdjacencyRelation>
relations(const topology::WindowAdjacencyGraph& graph) {
    return {graph.relations().begin(), graph.relations().end()};
}

void test_all_opposing_edge_orientations() {
    const topology::AdjacencyOptions exact{};

    const auto right_left = topology::find_adjacency(
        window("A", {0, 0, 100, 100}), window("B", {100, 20, 200, 80}), exact);
    expect_relation(right_left,
                    "A",
                    "B",
                    geometry::Edge::Right,
                    geometry::Edge::Left,
                    0,
                    60,
                    "right-to-left exact contact reports its shared edge");

    const auto left_right = topology::find_adjacency(
        window("A", {100, 10, 200, 90}), window("B", {0, 0, 100, 100}), exact);
    expect_relation(left_right,
                    "A",
                    "B",
                    geometry::Edge::Left,
                    geometry::Edge::Right,
                    0,
                    80,
                    "left-to-right exact contact reports its shared edge");

    const auto bottom_top = topology::find_adjacency(
        window("A", {10, 0, 90, 100}), window("B", {0, 100, 100, 200}), exact);
    expect_relation(bottom_top,
                    "A",
                    "B",
                    geometry::Edge::Bottom,
                    geometry::Edge::Top,
                    0,
                    80,
                    "bottom-to-top exact contact reports its shared edge");

    const auto top_bottom = topology::find_adjacency(
        window("A", {0, 100, 100, 200}), window("B", {10, 0, 90, 100}), exact);
    expect_relation(top_bottom,
                    "A",
                    "B",
                    geometry::Edge::Top,
                    geometry::Edge::Bottom,
                    0,
                    80,
                    "top-to-bottom exact contact reports its shared edge");

    const auto reversed = topology::find_adjacency(
        window("B", {100, 20, 200, 80}), window("A", {0, 0, 100, 100}), exact);
    expect(reversed == right_left,
           "relation identity and orientation are independent of argument order");
}

void test_tolerance_corner_and_overlap_semantics() {
    const auto first = window("A", {0, 0, 100, 100});
    const topology::AdjacencyOptions tolerance{5};

    expect_relation(topology::find_adjacency(
                        first, window("B", {105, 20, 205, 80}), tolerance),
                    "A",
                    "B",
                    geometry::Edge::Right,
                    geometry::Edge::Left,
                    5,
                    60,
                    "positive gap equal to tolerance is adjacent");
    expect(!topology::find_adjacency(
                first, window("B", {106, 20, 206, 80}), tolerance)
                .has_value(),
           "positive gap outside tolerance is not adjacent");

    expect_relation(topology::find_adjacency(
                        first, window("B", {95, 20, 195, 80}), tolerance),
                    "A",
                    "B",
                    geometry::Edge::Right,
                    geometry::Edge::Left,
                    -5,
                    60,
                    "small edge intrusion retains a negative signed gap");
    expect(!topology::find_adjacency(
                first, window("B", {94, 20, 194, 80}), tolerance)
                .has_value(),
           "intrusion beyond tolerance is not adjacent");

    expect(!topology::find_adjacency(
                first, window("B", {100, 100, 200, 200}), tolerance)
                .has_value(),
           "corner-only contact is not adjacency");
    expect_relation(topology::find_adjacency(
                        first, window("B", {100, 99, 200, 199}), tolerance),
                    "A",
                    "B",
                    geometry::Edge::Right,
                    geometry::Edge::Left,
                    0,
                    1,
                    "one unit of real orthogonal overlap is adjacency");
    expect(!topology::find_adjacency(
                first, window("B", {100, 101, 200, 201}), tolerance)
                .has_value(),
           "perpendicular near-touch is not inflated by edge tolerance");

    expect_relation(topology::find_adjacency(
                        first, window("B", {95, 10, 140, 90}), tolerance),
                    "A",
                    "B",
                    geometry::Edge::Right,
                    geometry::Edge::Left,
                    -5,
                    80,
                    "near-edge overlap with one qualifying contact is adjacent");
    expect_relation(topology::find_adjacency(
                        first, window("B", {0, 20, 5, 80}), tolerance),
                    "A",
                    "B",
                    geometry::Edge::Left,
                    geometry::Edge::Right,
                    -5,
                    60,
                    "near-edge containment with one contact is shallow intrusion");
    expect(!topology::find_adjacency(
                first, window("B", {50, 20, 150, 80}), tolerance)
                .has_value(),
           "deep positive-area overlap is not adjacency");
    expect(!topology::find_adjacency(
                first, window("B", {20, 20, 80, 80}), tolerance)
                .has_value(),
           "deep containment is not adjacency");

    expect(!topology::find_adjacency(
                first, window("B", {95, 95, 195, 195}), tolerance)
                .has_value(),
           "two-axis small intrusion is rejected as an ambiguous pair");
    expect(!topology::find_adjacency(
                first, window("B", {40, 40, 60, 60}),
                topology::AdjacencyOptions{60})
                .has_value(),
           "large tolerance does not silently choose among multiple contacts");
    expect(!topology::find_adjacency(
                first, window("B", {40, 20, 60, 80}),
                topology::AdjacencyOptions{60})
                .has_value(),
           "two same-axis contacts are rejected as ambiguous");
}

void test_invalid_and_empty_inputs() {
    const auto empty = window("empty", {10, 10, 10, 80});
    const auto empty_height = window("empty-height", {10, 10, 80, 10});
    const auto normal = window("normal", {10, 10, 80, 80});
    expect(!topology::find_adjacency(empty, normal, {}).has_value(),
           "empty rectangle has no adjacency relation");
    expect(!topology::find_adjacency(empty_height, normal, {}).has_value(),
           "zero-height rectangle has no adjacency relation");

    const auto graph = build_graph({empty, empty_height, normal});
    expect(graph.windows().size() == 3 && graph.relations().empty(),
           "empty rectangles remain isolated graph nodes");
    expect(window_ids(graph.connected_component(model::WindowId{"empty"})) ==
               std::vector<std::string>{"empty"},
           "empty rectangle's connected component contains only itself");

    expect_throws<std::invalid_argument>(
        [&] { [[maybe_unused]] const auto relation = topology::find_adjacency(
                  normal, window("normal", {80, 10, 100, 80}), {}); },
        "find_adjacency rejects identical WindowId values");
    expect_throws<std::invalid_argument>(
        [&] { [[maybe_unused]] const auto relation = topology::find_adjacency(
                  normal, window("other", {80, 10, 100, 80}),
                  topology::AdjacencyOptions{-1}); },
        "find_adjacency rejects negative tolerance");
    expect_throws<std::invalid_argument>(
        [&] {
            [[maybe_unused]] const auto duplicate_graph = build_graph(
                {normal, window("normal", {80, 10, 100, 80})});
        },
        "graph rejects duplicate WindowId values");
    expect_throws<std::invalid_argument>(
        [&] {
            [[maybe_unused]] const auto duplicate_graph = build_graph(
                {normal, window("normal", normal.visible_rect)});
        },
        "graph rejects duplicate WindowId values even when geometry is identical");
    expect_throws<std::invalid_argument>(
        [&] {
            [[maybe_unused]] const auto invalid_graph = build_graph(
                {normal, window("other", {80, 10, 100, 80})}, -1);
        },
        "graph rejects negative tolerance");
    expect_throws<std::invalid_argument>(
        [&] {
            [[maybe_unused]] const auto component =
                graph.connected_component(model::WindowId{"unknown"});
        },
        "connected_component rejects an unknown start ID");
}

void test_signed_and_overflow_boundaries() {
    constexpr auto maximum = std::numeric_limits<geometry::Coordinate>::max();
    constexpr auto minimum = std::numeric_limits<geometry::Coordinate>::min();

    expect(!geometry::magnitude_within(minimum, maximum),
           "INT64_MIN magnitude is outside maximum representable tolerance");

    expect_relation(topology::find_adjacency(
                        window("A", {maximum - 300, -100, maximum - 200, 100}),
                        window("B", {maximum - 195, -50, maximum - 95, 50}),
                        topology::AdjacencyOptions{5}),
                    "A",
                    "B",
                    geometry::Edge::Right,
                    geometry::Edge::Left,
                    5,
                    100,
                    "large positive coordinates preserve signed-gap arithmetic");
    expect_relation(topology::find_adjacency(
                        window("A", {minimum + 95, -100, minimum + 195, 100}),
                        window("B", {minimum, -50, minimum + 90, 50}),
                        topology::AdjacencyOptions{5}),
                    "A",
                    "B",
                    geometry::Edge::Left,
                    geometry::Edge::Right,
                    5,
                    100,
                    "large negative coordinates preserve signed-gap arithmetic");

    expect(!topology::find_adjacency(
                window("A", {minimum, 0, minimum + 10, 100}),
                window("B", {maximum - 10, 0, maximum, 100}),
                topology::AdjacencyOptions{maximum})
                .has_value(),
           "unrepresentable far edge gap is safely outside every tolerance");

    expect_throws<std::overflow_error>(
        [&] {
            [[maybe_unused]] const auto relation = topology::find_adjacency(
                window("A", {0, minimum, 100, maximum}),
                window("B", {100, minimum, 200, maximum}),
                {});
        },
        "unrepresentable orthogonal overlap reports overflow");
}

void test_graph_shapes_and_components() {
    const auto pair = build_graph(
        {window("B", {10, 0, 20, 10}), window("A", {0, 0, 10, 10})});
    expect(window_ids(pair.windows()) == std::vector<std::string>({"A", "B"}),
           "two-node graph canonicalizes node order");
    expect(pair.relations().size() == 1,
           "two adjacent nodes produce one undirected relation");

    const auto horizontal_chain = build_graph(
        {window("C", {20, 0, 30, 10}),
         window("A", {0, 0, 10, 10}),
         window("B", {10, 0, 20, 10})});
    expect(horizontal_chain.relations().size() == 2,
           "three-node horizontal chain has two direct relations");
    expect(window_ids(horizontal_chain.connected_component(model::WindowId{"A"})) ==
               std::vector<std::string>({"A", "B", "C"}),
           "chain connected component includes transitive neighbors");

    const auto vertical_chain = build_graph(
        {window("A", {0, 0, 10, 10}),
         window("B", {0, 10, 10, 20}),
         window("C", {0, 20, 10, 30})});
    expect(vertical_chain.relations().size() == 2,
           "three-node vertical chain has two direct relations");

    const std::vector grid_windows{
        window("D", {10, 10, 20, 20}),
        window("B", {10, 0, 20, 10}),
        window("C", {0, 10, 10, 20}),
        window("A", {0, 0, 10, 10}),
    };
    const auto grid = build_graph(grid_windows);
    expect(grid.relations().size() == 4,
           "2x2 topology has four shared-side relations and no corner diagonals");
    expect(window_ids(grid.connected_component(model::WindowId{"D"})) ==
               std::vector<std::string>({"A", "B", "C", "D"}),
           "2x2 topology is one connected component from every corner");

    const auto l_shape = build_graph(
        {window("A", {0, 0, 10, 20}),
         window("B", {10, 0, 20, 10}),
         window("C", {0, 20, 10, 30})});
    expect(l_shape.relations().size() == 2 &&
               window_ids(l_shape.connected_component(model::WindowId{"B"})) ==
                   std::vector<std::string>({"A", "B", "C"}),
           "L topology connects through its elbow without corner-only edges");

    const auto separated = build_graph(
        {window("F", {100, 100, 110, 110}),
         window("E", {50, 10, 60, 20}),
         window("A", {0, 0, 10, 10}),
         window("D", {50, 0, 60, 10}),
         window("C", {20, 0, 30, 10}),
         window("B", {10, 0, 20, 10})});
    expect(window_ids(separated.connected_component(model::WindowId{"A"})) ==
               std::vector<std::string>({"A", "B", "C"}),
           "first of multiple components is isolated from the second");
    expect(window_ids(separated.connected_component(model::WindowId{"D"})) ==
               std::vector<std::string>({"D", "E"}),
           "second independent component is solved separately");
    expect(window_ids(separated.connected_component(model::WindowId{"F"})) ==
               std::vector<std::string>{"F"},
           "isolated node forms a singleton component");

    std::set<std::string> unordered_pairs;
    for (const auto& relation : grid.relations()) {
        expect(relation.first.value() < relation.second.value(),
               "graph relation uses canonical ID orientation");
        unordered_pairs.insert(relation.first.value() + "\n" + relation.second.value());
    }
    expect(unordered_pairs.size() == grid.relations().size(),
           "graph contains no duplicate pair relation");
}

void test_permutation_and_generated_properties() {
    std::vector source{
        window("A", {0, 0, 10, 10}),
        window("B", {10, 0, 20, 10}),
        window("C", {0, 10, 10, 20}),
        window("D", {10, 10, 20, 20}),
        window("E", {50, 50, 60, 60}),
    };
    const auto baseline = build_graph(source);
    const auto baseline_windows = window_ids(baseline.windows());
    const auto baseline_relations = relations(baseline);

    std::sort(source.begin(), source.end(), [](const auto& first, const auto& second) {
        return first.id.value() < second.id.value();
    });
    std::size_t permutation_count = 0;
    do {
        const auto graph = build_graph(source);
        expect(window_ids(graph.windows()) == baseline_windows &&
                   relations(graph) == baseline_relations,
               "every input permutation produces the same graph");
        ++permutation_count;
    } while (std::next_permutation(
        source.begin(), source.end(), [](const auto& first, const auto& second) {
            return first.id.value() < second.id.value();
        }));
    expect(permutation_count == 120,
           "deterministic permutation check covers every ordering of five windows");

    for (geometry::Coordinate gap = -4; gap <= 4; ++gap) {
        const auto first = window("A", {0, -20, 100, 20});
        const auto second = window("B", {100 + gap, -10, 180 + gap, 10});
        const topology::AdjacencyOptions options{4};
        const auto forward = topology::find_adjacency(first, second, options);
        const auto reverse = topology::find_adjacency(second, first, options);
        expect(forward == reverse && forward.has_value(),
               "generated signed-gap relation is symmetric under input reversal");
    }

    const std::array<std::pair<geometry::Coordinate, geometry::Coordinate>, 5>
        offsets{{{-100, -75}, {-31, 47}, {0, 0}, {29, -53}, {110, 95}}};
    for (const auto [dx, dy] : offsets) {
        std::vector<topology::WindowGeometry> translated;
        translated.reserve(source.size());
        for (const auto& item : source) {
            const auto& rect = item.visible_rect;
            translated.push_back(window(item.id.value(),
                                        {rect.left() + dx,
                                         rect.top() + dy,
                                         rect.right() + dx,
                                         rect.bottom() + dy}));
        }
        expect(relations(build_graph(translated)) == baseline_relations,
               "translating every node preserves adjacency topology");
    }
}

} // namespace

int main() {
    test_all_opposing_edge_orientations();
    test_tolerance_corner_and_overlap_semantics();
    test_invalid_and_empty_inputs();
    test_signed_and_overflow_boundaries();
    test_graph_shapes_and_components();
    test_permutation_and_generated_properties();

    if (failures != 0) {
        std::cerr << failures << " topology test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All topology tests passed\n";
    return EXIT_SUCCESS;
}
