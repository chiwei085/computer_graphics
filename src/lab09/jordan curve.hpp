#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace demo
{

struct Cell
{
    int x = 0;
    int y = 0;
};

struct WindingQueryResult
{
    int total = 0;
    bool on_boundary = false;
    bool inside = false;
};

struct OddEvenQueryResult
{
    int crossings = 0;
    bool on_boundary = false;
    bool inside = false;
};

/**
 * @brief Fill a polygon using the non-zero winding Jordan-curve test.
 *
 * Scans the polygon bounding box and runs a point-in-polygon query at each
 * grid cell center. The query first checks whether the point lies on any edge;
 * otherwise it accumulates upward and downward crossings into a winding number.
 * A non-zero winding number marks the cell as interior.
 *
 * Unlike the Lab07 half-space test, this method does not require a convexity
 * check and does not depend on a specific input winding order.
 *
 * @param vertices Polygon vertices in click order.
 * @return All grid cells covered by the polygon interior and boundary.
 */
inline std::vector<Cell> fill_polygon_non_zero(
    const std::vector<Cell>& vertices);

/**
 * @brief Fill a polygon using the odd-even Jordan-curve test.
 *
 * Scans the polygon bounding box and runs an even-odd point-in-polygon query
 * at each grid cell center. Boundary points are treated as interior.
 *
 * @param vertices Polygon vertices in click order.
 * @return All grid cells covered by the polygon interior and boundary.
 */
inline std::vector<Cell> fill_polygon_odd_even(
    const std::vector<Cell>& vertices);

/**
 * @brief Query one point with the non-zero winding rule and return details.
 *
 * @param verts Ordered polygon vertices.
 * @param px    Query point x coordinate.
 * @param py    Query point y coordinate.
 * @return Boundary flag, winding total, and inside/outside classification.
 */
inline WindingQueryResult query_point_non_zero(const std::vector<Cell>& verts,
                                               double px, double py);
inline OddEvenQueryResult query_point_odd_even(const std::vector<Cell>& verts,
                                               double px, double py);

/**
 * @brief Compute twice the signed area of a polygon (shoelace formula).
 *
 * A positive result indicates CCW winding in mathematical coordinates (y up).
 * Zero indicates a degenerate or collinear vertex set.
 *
 * @param verts Ordered polygon vertices.
 * @return 2 * signed area; positive = CCW.
 */
inline int signed_area2(const std::vector<Cell>& verts) {
    const std::size_t n = verts.size();
    int area2 = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const Cell& p = verts[i];
        const Cell& q = verts[(i + 1) % n];
        area2 += p.x * q.y - q.x * p.y;
    }
    return area2;
}

struct BoundingBox
{
    int x_min, x_max, y_min, y_max;
};

inline BoundingBox compute_bounding_box(const std::vector<Cell>& verts) {
    BoundingBox bb{verts[0].x, verts[0].x, verts[0].y, verts[0].y};
    for (const Cell& v : verts) {
        bb.x_min = std::min(bb.x_min, v.x);
        bb.x_max = std::max(bb.x_max, v.x);
        bb.y_min = std::min(bb.y_min, v.y);
        bb.y_max = std::max(bb.y_max, v.y);
    }
    return bb;
}

namespace
{

/**
 * @brief Evaluate the signed cross product for one directed edge and query
 * point.
 *
 * Positive means `(px, py)` lies to the left of edge `from -> to`, negative
 * means right, and zero means collinear. The winding-rule test uses this sign
 * to decide whether a vertical crossing contributes `+1` or `-1`.
 *
 * @param from Edge start.
 * @param to   Edge end.
 * @param px   Query point x coordinate.
 * @param py   Query point y coordinate.
 * @return Signed cross-product value.
 */
double is_left(const Cell& from, const Cell& to, double px, double py) {
    return static_cast<double>(to.x - from.x) * (py - from.y) -
           (px - from.x) * static_cast<double>(to.y - from.y);
}

/**
 * @brief Check whether a query point lies on the closed segment `from -> to`.
 *
 * The point must be nearly collinear with the segment and also fall inside the
 * segment's axis-aligned bounding box. This boundary test is done before the
 * winding accumulation so edge pixels can be treated as filled directly.
 *
 * @param from Edge start.
 * @param to   Edge end.
 * @param px   Query point x coordinate.
 * @param py   Query point y coordinate.
 * @return True if the point lies on the segment, else false.
 */
bool point_on_segment(const Cell& from, const Cell& to, double px, double py) {
    constexpr double kEpsilon = 1e-9;
    const double cross = is_left(from, to, px, py);
    if (std::abs(cross) > kEpsilon) return false;

    const double min_x = std::min(from.x, to.x) - kEpsilon;
    const double max_x = std::max(from.x, to.x) + kEpsilon;
    const double min_y = std::min(from.y, to.y) - kEpsilon;
    const double max_y = std::max(from.y, to.y) + kEpsilon;
    return px >= min_x && px <= max_x && py >= min_y && py <= max_y;
}

/**
 * @brief Classify one query point with the non-zero winding rule.
 *
 * The test first handles boundary points explicitly. For all remaining points,
 * each polygon edge is checked for an upward or downward crossing of the
 * horizontal line through `(px, py)`. A left-of-edge upward crossing adds `1`
 * to the winding number; a right-of-edge downward crossing subtracts `1`.
 * Any non-zero final winding number means the point is inside the polygon.
 *
 * @param verts Ordered polygon vertices.
 * @param px    Query point x coordinate.
 * @param py    Query point y coordinate.
 * @return True if the point is inside or on the boundary, else false.
 */
WindingQueryResult query_point_non_zero_impl(const std::vector<Cell>& verts,
                                             double px, double py) {
    const std::size_t n = verts.size();
    int winding = 0;

    for (std::size_t i = 0; i < n; ++i) {
        const Cell& from = verts[i];
        const Cell& to = verts[(i + 1) % n];

        if (point_on_segment(from, to, px, py)) {
            return WindingQueryResult{winding, true, true};
        }

        if (from.y <= py) {
            if (to.y > py && is_left(from, to, px, py) > 0.0) {
                ++winding;
            }
        }
        else {
            if (to.y <= py && is_left(from, to, px, py) < 0.0) {
                --winding;
            }
        }
    }

    return WindingQueryResult{winding, false, winding != 0};
}

/**
 * @brief Classify one query point with the non-zero winding rule.
 *
 * The test first handles boundary points explicitly. For all remaining points,
 * each polygon edge is checked for an upward or downward crossing of the
 * horizontal line through `(px, py)`. A left-of-edge upward crossing adds `1`
 * to the winding number; a right-of-edge downward crossing subtracts `1`.
 * Any non-zero final winding number means the point is inside the polygon.
 *
 * @param verts Ordered polygon vertices.
 * @param px    Query point x coordinate.
 * @param py    Query point y coordinate.
 * @return True if the point is inside or on the boundary, else false.
 */
bool point_in_polygon_non_zero(const std::vector<Cell>& verts, double px,
                               double py) {
    return query_point_non_zero_impl(verts, px, py).inside;
}

/**
 * @brief Classify one query point with the odd-even crossing rule.
 *
 * The test first handles boundary points explicitly. For the remaining points,
 * it counts how many polygon edges cross the horizontal ray starting at
 * `(px, py)` and extending to the right. An odd number of crossings means the
 * point is inside; an even number means outside.
 *
 * @param verts Ordered polygon vertices.
 * @param px    Query point x coordinate.
 * @param py    Query point y coordinate.
 * @return True if the point is inside or on the boundary, else false.
 */
OddEvenQueryResult query_point_odd_even_impl(const std::vector<Cell>& verts,
                                             double px, double py) {
    const std::size_t n = verts.size();
    int crossings = 0;

    for (std::size_t i = 0; i < n; ++i) {
        const Cell& from = verts[i];
        const Cell& to = verts[(i + 1) % n];

        if (point_on_segment(from, to, px, py)) {
            return OddEvenQueryResult{crossings, true, true};
        }

        const bool crosses_scanline =
            (from.y <= py && py < to.y) || (to.y <= py && py < from.y);
        if (!crosses_scanline) continue;

        const double x_intersection =
            from.x + (py - from.y) * static_cast<double>(to.x - from.x) /
                         static_cast<double>(to.y - from.y);
        if (x_intersection > px) ++crossings;
    }

    return OddEvenQueryResult{crossings, false, (crossings & 1) != 0};
}

bool point_in_polygon_odd_even(const std::vector<Cell>& verts, double px,
                               double py) {
    return query_point_odd_even_impl(verts, px, py).inside;
}

void print_fill_report(const char* label, const std::vector<Cell>& vertices,
                       const std::vector<Cell>& filled) {
    std::printf("=== %s ===\n", label);
    const std::size_t n = vertices.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Cell& from = vertices[i];
        const Cell& to = vertices[(i + 1) % n];
        std::printf("  Edge %zu: (%d,%d) -> (%d,%d)\n", i, from.x, from.y, to.x,
                    to.y);
    }
    std::printf("Filled cells (%zu):\n", filled.size());
    for (std::size_t i = 0; i < filled.size(); ++i) {
        if (i % 6 == 0) std::printf("  ");
        std::printf("(%d,%d)", filled[i].x, filled[i].y);
        if (i % 6 == 5 || i + 1 == filled.size()) {
            std::printf("\n");
        }
        else {
            std::printf("  ");
        }
    }
    std::fflush(stdout);
}

}  // namespace

inline std::vector<Cell> fill_polygon_non_zero(
    const std::vector<Cell>& vertices) {
    const std::size_t n = vertices.size();
    if (n < 3) return {};

    const int area2 = signed_area2(vertices);
    if (area2 == 0) return {};

    const BoundingBox bb = compute_bounding_box(vertices);

    std::vector<Cell> result;
    result.reserve(static_cast<std::size_t>(bb.x_max - bb.x_min + 1) *
                   static_cast<std::size_t>(bb.y_max - bb.y_min + 1));

    for (int y = bb.y_min; y <= bb.y_max; ++y) {
        for (int x = bb.x_min; x <= bb.x_max; ++x) {
            if (point_in_polygon_non_zero(vertices, static_cast<double>(x),
                                          static_cast<double>(y))) {
                result.push_back(Cell{x, y});
            }
        }
    }

    print_fill_report("Non-zero winding fill", vertices, result);
    return result;
}

inline WindingQueryResult query_point_non_zero(const std::vector<Cell>& verts,
                                               double px, double py) {
    return query_point_non_zero_impl(verts, px, py);
}

inline OddEvenQueryResult query_point_odd_even(const std::vector<Cell>& verts,
                                               double px, double py) {
    return query_point_odd_even_impl(verts, px, py);
}

inline std::vector<Cell> fill_polygon_odd_even(
    const std::vector<Cell>& vertices) {
    const std::size_t n = vertices.size();
    if (n < 3) return {};

    const int area2 = signed_area2(vertices);
    if (area2 == 0) return {};

    const BoundingBox bb = compute_bounding_box(vertices);

    std::vector<Cell> result;
    result.reserve(static_cast<std::size_t>(bb.x_max - bb.x_min + 1) *
                   static_cast<std::size_t>(bb.y_max - bb.y_min + 1));

    for (int y = bb.y_min; y <= bb.y_max; ++y) {
        for (int x = bb.x_min; x <= bb.x_max; ++x) {
            if (point_in_polygon_odd_even(vertices, static_cast<double>(x),
                                          static_cast<double>(y))) {
                result.push_back(Cell{x, y});
            }
        }
    }

    print_fill_report("Odd-even fill", vertices, result);
    return result;
}

}  // namespace demo
