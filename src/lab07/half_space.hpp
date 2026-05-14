#pragma once

#include <algorithm>
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

/**
 * @brief Fill a convex polygon using the half-space test (Pineda 1988).
 *
 * Evaluates one implicit edge equation per polygon edge and collects every
 * grid cell whose center lies on the interior side of all edges simultaneously.
 * Vertices may be supplied in either winding order; CW input is reversed to CCW
 * before processing. All boundary pixels (E = 0) are treated as interior, so
 * every edge of a standalone polygon is fully drawn including corner vertices.
 *
 * The per-pixel cost is O(N) comparisons and additions, where N is the vertex
 * count. Row-to-row and column-to-column transitions reuse a running edge value
 * rather than recomputing from scratch.
 *
 * @param vertices Polygon vertices in click order; must form a convex polygon.
 * @return All grid cells covered by the polygon interior and boundary.
 */
inline std::vector<Cell> fill_polygon_half_space(
    const std::vector<Cell>& vertices);

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

namespace
{

/**
 * @brief Coefficients of one directed-edge implicit line function.
 *
 * For the directed edge from `from` to `to`, the half-space function is:
 *
 *   E(x, y) = a*x + b*y + c
 *
 * where
 *   a = to.y - from.y          (equals dy; right-step increment)
 *   b = -(to.x - from.x)       (equals -dx; up-step increment)
 *   c = -(a * from.x + b * from.y)
 *
 * Incremental updates keep the per-pixel cost to pure addition:
 *   E(x + 1, y) = E(x, y) + a
 *   E(x, y + 1) = E(x, y) + b
 *
 * Interior condition is `E(x, y) <= 0`, which includes the polygon boundary.
 */
struct EdgeEquation
{
    int a = 0;
    int b = 0;
    int c = 0;
};

/**
 * @brief Build the edge equation for the directed edge from `from` to `to`.
 *
 * Standalone polygon rendering uses inclusive boundaries, so every edge shares
 * the same interior test `E(x, y) <= 0`.
 *
 * @param from Directed edge start in grid space.
 * @param to   Directed edge end in grid space.
 * @return Fully initialized edge equation.
 */
EdgeEquation make_edge_equation(const Cell& from, const Cell& to) {
    const int a = to.y - from.y;
    const int b = -(to.x - from.x);
    const int c = -(a * from.x + b * from.y);
    return EdgeEquation{a, b, c};
}

/**
 * @brief Print the half-space fill result to stdout.
 *
 * Reports edge coefficients and the list of filled cells so the algorithm
 * can be inspected step by step, mirroring the polygon report in poly_fill.cpp.
 *
 * @param edges   Edge equations used during fill.
 * @param filled  Cells produced by the fill pass.
 */
void print_fill_report(const std::vector<EdgeEquation>& edges,
                       const std::vector<Cell>& filled) {
    std::printf("=== Half-space fill ===\n");
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const EdgeEquation& e = edges[i];
        std::printf("  Edge %zu: E(x,y) = %d*x + %d*y + %d\n", i, e.a, e.b,
                    e.c);
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

inline std::vector<Cell> fill_polygon_half_space(
    const std::vector<Cell>& vertices) {
    const std::size_t n = vertices.size();
    if (n < 3) return {};

    // Normalize to CCW winding (positive signed area) so the edge equations
    // use one consistent interior convention regardless of click order.
    std::vector<Cell> verts = vertices;
    const int area2 = signed_area2(verts);
    if (area2 == 0) return {};
    if (area2 < 0) {
        std::reverse(verts.begin(), verts.end());
    }

    // Build one edge equation per polygon edge.
    std::vector<EdgeEquation> edges;
    edges.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        edges.push_back(make_edge_equation(verts[i], verts[(i + 1) % n]));
    }

    // -----------------------------------------------------------------------
    // Bounding box: defines the pixel region that the scan will traverse.
    // Every candidate pixel is drawn from this rectangle; the half-space
    // tests then discard those outside the polygon.
    // -----------------------------------------------------------------------
    int x_min = verts[0].x, x_max = verts[0].x;
    int y_min = verts[0].y, y_max = verts[0].y;
    for (const Cell& v : verts) {
        x_min = std::min(x_min, v.x);
        x_max = std::max(x_max, v.x);
        y_min = std::min(y_min, v.y);
        y_max = std::max(y_max, v.y);
    }

    // -----------------------------------------------------------------------
    // Seed all edge values once at the bounding-box origin (x_min, y_min).
    // From here, every pixel is reached by pure addition — no multiplications
    // inside the scan loops.
    //
    //   e[k] = E_k(x, y)
    //
    // Right step:  E_k(x+1, y)       = e[k] + a_k
    // Up step:     E_k(x,   y+1)     = e[k] + b_k
    // Rewind+up:   E_k(x_min, y+1)   = e[k] - x_span * a_k + b_k
    //              (used after each row to reset x and advance y)
    // -----------------------------------------------------------------------
    const int x_span = x_max - x_min + 1;

    std::vector<int> e(n);
    for (std::size_t k = 0; k < n; ++k) {
        e[k] = edges[k].a * x_min + edges[k].b * y_min + edges[k].c;
    }

    std::vector<Cell> result;

    for (int y = y_min; y <= y_max; ++y) {
        for (int x = x_min; x <= x_max; ++x) {
            // Half-space test: pixel is inside when all edge values pass.
            bool inside = true;
            for (std::size_t k = 0; k < n; ++k) {
                if (e[k] > 0) {
                    inside = false;
                    break;
                }
            }
            if (inside) result.push_back(Cell{x, y});

            for (std::size_t k = 0; k < n; ++k) e[k] += edges[k].a;
        }
        // Rewind x_span steps left and step up one row.
        // Mirrors the professor's:  e += -xDim * a + b
        for (std::size_t k = 0; k < n; ++k) {
            e[k] += -x_span * edges[k].a + edges[k].b;
        }
    }

    print_fill_report(edges, result);
    return result;
}

}  // namespace demo
