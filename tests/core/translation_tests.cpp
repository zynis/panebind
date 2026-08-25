#include "core/movement/move_plan.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace geometry = panebind::core::geometry;
namespace model = panebind::core::model;
namespace movement = panebind::core::movement;
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

void expect_change(const movement::GeometryChange& change,
                   movement::GeometryChangeKind kind,
                   std::optional<movement::TranslationDelta> delta,
                   std::string_view description) {
    expect(change.kind == kind && change.delta == delta, description);
}

void expect_plan(const std::optional<std::vector<movement::PlannedTranslation>>& plan,
                 const std::vector<movement::PlannedTranslation>& expected,
                 std::string_view description) {
    expect(plan.has_value() && *plan == expected, description);
}

void test_geometry_change_classification() {
    const geometry::Rect original{100, 100, 500, 400};

    expect_change(movement::classify_geometry_change(original, original),
                  movement::GeometryChangeKind::Unchanged,
                  std::nullopt,
                  "identical geometry is unchanged");
    expect_change(movement::classify_geometry_change(
                      original, geometry::Rect{140, 100, 540, 400}),
                  movement::GeometryChangeKind::Translation,
                  movement::TranslationDelta{40, 0},
                  "pure x movement is translation");
    expect_change(movement::classify_geometry_change(
                      original, geometry::Rect{100, 70, 500, 370}),
                  movement::GeometryChangeKind::Translation,
                  movement::TranslationDelta{0, -30},
                  "pure y movement is translation");
    expect_change(movement::classify_geometry_change(
                      original, geometry::Rect{140, 80, 540, 380}),
                  movement::GeometryChangeKind::Translation,
                  movement::TranslationDelta{40, -20},
                  "diagonal movement reports both signed deltas");

    expect_change(movement::classify_geometry_change(
                      original, geometry::Rect{100, 100, 520, 400}),
                  movement::GeometryChangeKind::ResizeOrMixed,
                  std::nullopt,
                  "width change is resize-or-mixed");
    expect_change(movement::classify_geometry_change(
                      original, geometry::Rect{100, 100, 500, 420}),
                  movement::GeometryChangeKind::ResizeOrMixed,
                  std::nullopt,
                  "height change is resize-or-mixed");
    expect_change(movement::classify_geometry_change(
                      original, geometry::Rect{120, 70, 560, 390}),
                  movement::GeometryChangeKind::ResizeOrMixed,
                  std::nullopt,
                  "position and size change is resize-or-mixed");
    expect_change(movement::classify_geometry_change(
                      geometry::Rect{955, 660, 2816, 1745},
                      geometry::Rect{-13, -13, 3085, 1837}),
                  movement::GeometryChangeKind::ResizeOrMixed,
                  std::nullopt,
                  "maximize-like geometry is not misclassified as translation");
}

void test_translate_rect_and_arithmetic_boundaries() {
    const geometry::Rect original{100, 100, 500, 500};
    const movement::TranslationDelta delta{40, -20};
    const auto translated = movement::translate_rect(original, delta);
    expect(translated == geometry::Rect{140, 80, 540, 480},
           "translation delta reproduces all four target edges");
    expect(translated.size() == original.size(),
           "translation preserves width and height");
    expect(movement::translate_rect(geometry::Rect{-100, -50, -20, 30},
                                    movement::TranslationDelta{-25, 40}) ==
               geometry::Rect{-125, -10, -45, 70},
           "translation supports negative signed coordinates and deltas");

    constexpr auto maximum = std::numeric_limits<geometry::Coordinate>::max();
    constexpr auto minimum = std::numeric_limits<geometry::Coordinate>::min();

    expect_throws<std::overflow_error>(
        [&] {
            [[maybe_unused]] const auto result = movement::translate_rect(
                geometry::Rect{maximum - 20, 0, maximum - 10, 10},
                movement::TranslationDelta{11, 0});
        },
        "translation fails when any target coordinate would overflow");
    expect_throws<std::overflow_error>(
        [&] {
            [[maybe_unused]] const auto result = movement::translate_rect(
                geometry::Rect{minimum + 10, 0, minimum + 20, 10},
                movement::TranslationDelta{-11, 0});
        },
        "negative translation fails when any target coordinate would underflow");
    expect_throws<std::overflow_error>(
        [&] {
            [[maybe_unused]] const auto result = movement::classify_geometry_change(
                geometry::Rect{minimum, 0, minimum + 10, 10},
                geometry::Rect{maximum - 10, 0, maximum, 10});
        },
        "unrepresentable equal-size translation delta reports overflow");
    expect_throws<std::overflow_error>(
        [&] {
            [[maybe_unused]] const auto result = movement::classify_geometry_change(
                geometry::Rect{minimum, 0, maximum, 10},
                geometry::Rect{0, 0, 10, 10});
        },
        "unrepresentable rectangle extent reports overflow");
}

void test_two_window_and_unchanged_plans() {
    const auto graph = build_graph(
        {window("B", {10, 0, 20, 10}), window("A", {0, 0, 10, 10})});
    const movement::TranslationSession session{graph, model::WindowId{"A"}};

    expect_plan(session.plan(geometry::Rect{100, 50, 110, 60}),
                {{model::WindowId{"B"}, geometry::Rect{110, 50, 120, 60}}},
                "two-window plan applies leader total delta to its follower");
    expect_plan(session.plan(geometry::Rect{0, 0, 10, 10}),
                {{model::WindowId{"B"}, geometry::Rect{10, 0, 20, 10}}},
                "unchanged leader yields an unchanged follower target");
    expect(!session.plan(geometry::Rect{5, 5, 20, 20}).has_value(),
           "resize-or-mixed leader geometry produces no move plan");

    const auto isolated_graph = build_graph({window("solo", {0, 0, 10, 10})});
    const movement::TranslationSession isolated{isolated_graph,
                                                model::WindowId{"solo"}};
    expect_plan(isolated.plan(geometry::Rect{-5, 7, 5, 17}),
                {},
                "isolated leader has a successful empty follower plan");

    expect_throws<std::invalid_argument>(
        [&] {
            [[maybe_unused]] const movement::TranslationSession unknown{
                graph, model::WindowId{"unknown"}};
        },
        "translation session rejects a leader outside the graph");
}

void test_four_window_plan_and_negative_delta() {
    const std::vector windows{
        window("D", {10, 10, 20, 20}),
        window("B", {10, 0, 20, 10}),
        window("C", {0, 10, 10, 20}),
        window("A", {0, 0, 10, 10}),
        window("outside", {100, 100, 110, 110}),
    };
    const auto graph = build_graph(windows);
    const movement::TranslationSession session{graph, model::WindowId{"A"}};

    expect_plan(
        session.plan(geometry::Rect{-7, 13, 3, 23}),
        {{model::WindowId{"B"}, geometry::Rect{3, 13, 13, 23}},
         {model::WindowId{"C"}, geometry::Rect{-7, 23, 3, 33}},
         {model::WindowId{"D"}, geometry::Rect{3, 23, 13, 33}}},
        "2x2 plan translates the complete component by one negative/positive delta");

    const auto plan = session.plan(geometry::Rect{-7, 13, 3, 23});
    expect(plan.has_value() &&
               std::none_of(plan->begin(), plan->end(), [](const auto& item) {
                   return item.id.value() == "A" || item.id.value() == "outside";
               }),
           "plan excludes both the leader and windows outside its initial component");
}

void test_initial_relative_repeated_planning() {
    const auto graph = build_graph(
        {window("A", {0, 0, 100, 100}), window("B", {100, 0, 200, 100})});
    const movement::TranslationSession session{graph, model::WindowId{"A"}};
    const geometry::Rect follower_initial{100, 0, 200, 100};

    const std::array totals{
        movement::TranslationDelta{1, 1},
        movement::TranslationDelta{2, -3},
        movement::TranslationDelta{1, 1},
        movement::TranslationDelta{100, 50},
        movement::TranslationDelta{-5, -8},
        movement::TranslationDelta{0, 0},
    };
    for (const auto& total : totals) {
        const auto leader_current =
            movement::translate_rect(geometry::Rect{0, 0, 100, 100}, total);
        const auto expected_follower = movement::translate_rect(follower_initial, total);
        expect_plan(session.plan(leader_current),
                    {{model::WindowId{"B"}, expected_follower}},
                    "each repeated plan uses session initial geometry plus total delta");
    }

    for (geometry::Distance dx = -40; dx <= 40; dx += 5) {
        for (geometry::Distance dy = -30; dy <= 30; dy += 6) {
            const movement::TranslationDelta total{dx, dy};
            const auto leader_current =
                movement::translate_rect(geometry::Rect{0, 0, 100, 100}, total);
            const auto plan = session.plan(leader_current);
            const auto expected = movement::translate_rect(follower_initial, total);
            expect(plan.has_value() && plan->size() == 1 &&
                       plan->front().target_visible_rect == expected &&
                       plan->front().target_visible_rect.size() == follower_initial.size(),
                   "generated plans preserve follower size without cumulative drift");
        }
    }
}

void test_plan_determinism_under_graph_permutation() {
    std::vector windows{
        window("A", {0, 0, 10, 10}),
        window("B", {10, 0, 20, 10}),
        window("C", {0, 10, 10, 20}),
        window("D", {10, 10, 20, 20}),
    };
    const auto baseline_graph = build_graph(windows);
    const movement::TranslationSession baseline_session{baseline_graph,
                                                        model::WindowId{"A"}};
    const auto baseline_plan = baseline_session.plan(geometry::Rect{25, -15, 35, -5});

    std::sort(windows.begin(), windows.end(), [](const auto& first, const auto& second) {
        return first.id.value() < second.id.value();
    });
    std::size_t permutation_count = 0;
    do {
        const auto graph = build_graph(windows);
        const movement::TranslationSession session{graph, model::WindowId{"A"}};
        expect(session.plan(geometry::Rect{25, -15, 35, -5}) == baseline_plan,
               "move plan is independent of graph input order");
        ++permutation_count;
    } while (std::next_permutation(
        windows.begin(), windows.end(), [](const auto& first, const auto& second) {
            return first.id.value() < second.id.value();
        }));
    expect(permutation_count == 24,
           "move-plan permutation check covers every ordering of four windows");
}

void test_session_owns_initial_snapshot() {
    const auto session = [] {
        const auto graph = build_graph(
            {window("A", {0, 0, 10, 10}), window("B", {10, 0, 20, 10})});
        return movement::TranslationSession{graph, model::WindowId{"A"}};
    }();

    expect_plan(session.plan(geometry::Rect{25, -5, 35, 5}),
                {{model::WindowId{"B"}, geometry::Rect{35, -5, 45, 5}}},
                "translation session owns its initial snapshot after graph destruction");
}

void test_plan_overflow_is_all_or_nothing() {
    constexpr auto maximum = std::numeric_limits<geometry::Coordinate>::max();
    const auto graph = build_graph(
        {window("A", {maximum - 40, 0, maximum - 30, 10}),
         window("B", {maximum - 30, 0, maximum - 20, 10}),
         window("C", {maximum - 20, 0, maximum - 10, 10})});
    const movement::TranslationSession session{graph, model::WindowId{"A"}};

    expect_throws<std::overflow_error>(
        [&] {
            [[maybe_unused]] const auto plan =
                session.plan(geometry::Rect{maximum - 29, 0, maximum - 19, 10});
        },
        "one overflowing follower fails the whole plan instead of returning partial targets");
}

} // namespace

int main() {
    test_geometry_change_classification();
    test_translate_rect_and_arithmetic_boundaries();
    test_two_window_and_unchanged_plans();
    test_four_window_plan_and_negative_delta();
    test_initial_relative_repeated_planning();
    test_plan_determinism_under_graph_permutation();
    test_session_owns_initial_snapshot();
    test_plan_overflow_is_all_or_nothing();

    if (failures != 0) {
        std::cerr << failures << " translation test(s) failed\n";
        return EXIT_FAILURE;
    }

    std::cout << "All translation tests passed\n";
    return EXIT_SUCCESS;
}
