#include <GL/freeglut.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "jordan curve.hpp"

namespace demo
{

/**
 * @brief Step classification in the normalized Region 1 frame.
 *
 * `E` advances only along the major axis, while `NE` advances along both the
 * major and minor axes during one Bresenham iteration.
 */
enum class StepType
{
    E,
    NE,
};

/**
 * @brief One pixel emitted by line rasterization.
 *
 * The coordinate is stored in original grid space after inverse transform, and
 * `step` records which Region 1 decision produced the pixel.
 */
struct RasterPixel
{
    int x = 0;
    int y = 0;
    StepType step = StepType::E;
};

/**
 * @brief Cached rasterization result for one polygon edge.
 *
 * `pixels` excludes the starting endpoint itself. The vertex indices are kept
 * explicitly so console output remains correct even when degenerate edges are
 * omitted from the drawable edge list.
 */
struct LineSegment
{
    Cell from;
    Cell to;
    int from_idx = -1;
    int to_idx = -1;
    int region = 0;
    std::vector<RasterPixel> pixels;
};

struct App
{
    int dimension = 10;
    std::vector<Cell> vertices;      ///< Polygon vertices in click order.
    std::vector<LineSegment> edges;  ///< Rasterized polygon edges.
    std::vector<Cell> filled_cells;  ///< Non-zero winding fill result.
    bool has_query_point = false;
    Cell query_point;
    WindingQueryResult query_result;
    OddEvenQueryResult odd_even_query_result;

    enum class State
    {
        kNormal,
        kCustomInput,
        kPickQueryPoint,
    } state = State::kNormal;

    std::string custom_buf;

    static void reshape_cb(int w, int h);
    static void display_cb();
    static void mouse_cb(int button, int state, int x, int y);
    static void keyboard_cb(unsigned char key, int x, int y);
    static void menu_cb(int value);
};

namespace
{

constexpr float kBackgroundColor[3] = {0.15f, 0.15f, 0.17f};
constexpr float kGridColor[3] = {0.42f, 0.42f, 0.44f};
constexpr float kEdgeColor[3] = {0.0f, 0.0f, 0.0f};
constexpr float kFillColor[3] = {0.78f, 0.78f, 0.80f};
constexpr float kQueryPointColor[3] = {1.0f, 1.0f, 1.0f};
constexpr float kOverlayColor[3] = {0.85f, 0.85f, 0.85f};
constexpr float kOverlayBandColor[3] = {0.22f, 0.22f, 0.24f};
constexpr float kPanelFillColor[3] = {0.24f, 0.24f, 0.24f};
constexpr float kPanelBorderColor[3] = {0.60f, 0.60f, 0.60f};

constexpr unsigned char kKeyBackspace = '\b';
constexpr unsigned char kKeyDelete = 127;
constexpr unsigned char kKeyEnter = '\r';
constexpr unsigned char kKeyEscape = '\x1b';
constexpr std::size_t kMaxCustomDigits = 6;

enum MenuValue
{
    kMenuDim10 = 10,
    kMenuDim15 = 15,
    kMenuDim20 = 20,
    kMenuCustom = 1001,
};

App& app() {
    return *static_cast<App*>(glutGetWindowData());
}

using Color3 = std::array<float, 3>;

struct NamedColor
{
    const char* name = "";
    Color3 rgb{};
};

constexpr std::array<NamedColor, 6> kVertexColorMap = {{
    NamedColor{"Red", {1.00f, 0.22f, 0.22f}},
    NamedColor{"Orange", {1.00f, 0.58f, 0.18f}},
    NamedColor{"Yellow", {0.96f, 0.86f, 0.22f}},
    NamedColor{"Green", {0.30f, 0.82f, 0.34f}},
    NamedColor{"Blue", {0.26f, 0.55f, 1.00f}},
    NamedColor{"Purple", {0.72f, 0.42f, 0.96f}},
}};

constexpr int kRegionTable[2][2][2] = {
    {{1, 8}, {4, 5}},
    {{2, 3}, {7, 6}},
};

struct GridLayout
{
    int x = 0;
    int y = 0;
    int width = 1;
    int height = 1;
};

GridLayout compute_grid_layout(int window_width, int window_height) {
    constexpr int kOuterMargin = 32;
    constexpr int kBottomOverlayBand = 72;

    GridLayout layout{};
    layout.x = kOuterMargin;
    layout.y = kOuterMargin + kBottomOverlayBand;
    layout.width = window_width - 2 * kOuterMargin;
    layout.height = window_height - (3 * kOuterMargin + kBottomOverlayBand);

    if (layout.width < 1) layout.width = 1;
    if (layout.height < 1) layout.height = 1;
    return layout;
}

void apply_projection(int width, int height, int dimension) {
    if (height <= 0) height = 1;
    const GridLayout layout = compute_grid_layout(width, height);
    glViewport(layout.x, layout.y, layout.width, layout.height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const double n = static_cast<double>(dimension);
    glOrtho(-n - 0.5, n + 0.5, -n - 0.5, n + 0.5, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
}

void initialize_gl_state() {
    glClearColor(kBackgroundColor[0], kBackgroundColor[1], kBackgroundColor[2],
                 1.0f);
    glDisable(GL_DEPTH_TEST);
}

/**
 * @brief Reset polygon data after changing the visible grid dimension.
 *
 * Like Lab07, the app maintains ordered vertices, a derived edge cache, and a
 * cached fill result, so all three are cleared when `N` changes.
 *
 * @param state         Global application state.
 * @param new_dimension New grid half-range.
 */
void apply_dimension(App& state, int new_dimension) {
    if (new_dimension <= 0) return;
    state.dimension = new_dimension;
    state.vertices.clear();
    state.edges.clear();
    state.filled_cells.clear();
    state.has_query_point = false;
    state.query_result = {};
    state.odd_even_query_result = {};
    state.state = App::State::kNormal;
    state.custom_buf.clear();
    apply_projection(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT),
                     state.dimension);
    glutPostRedisplay();
}

int bitmap_text_width(void* font, const char* text) {
    return glutBitmapLength(font, reinterpret_cast<const unsigned char*>(text));
}

void draw_bitmap_text(float x, float y, const char* text) {
    glRasterPos2f(x, y);
    for (const char* p = text; *p != '\0'; ++p) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *p);
    }
}

bool window_to_grid(int win_x, int win_y, int width, int height, int n, int* gx,
                    int* gy) {
    if (gx == nullptr || gy == nullptr || width <= 0 || height <= 0 || n <= 0) {
        return false;
    }

    const double u = static_cast<double>(win_x) / static_cast<double>(width);
    const double v = static_cast<double>(win_y) / static_cast<double>(height);
    const double span = static_cast<double>(2 * n + 1);
    const double wx = (-static_cast<double>(n) - 0.5) + u * span;
    const double wy = (static_cast<double>(n) + 0.5) - v * span;

    const int cell_x = static_cast<int>(std::floor(wx + 0.5));
    const int cell_y = static_cast<int>(std::floor(wy + 0.5));
    if (cell_x < -n || cell_x > n || cell_y < -n || cell_y > n) {
        return false;
    }

    *gx = cell_x;
    *gy = cell_y;
    return true;
}

void draw_grid(int n) {
    glColor3fv(kGridColor);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int x = -n - 1; x <= n; ++x) {
        const float line_x = static_cast<float>(x) + 0.5f;
        glVertex2f(line_x, -static_cast<float>(n) - 0.5f);
        glVertex2f(line_x, static_cast<float>(n) + 0.5f);
    }
    for (int y = -n - 1; y <= n; ++y) {
        const float line_y = static_cast<float>(y) + 0.5f;
        glVertex2f(-static_cast<float>(n) - 0.5f, line_y);
        glVertex2f(static_cast<float>(n) + 0.5f, line_y);
    }
    glEnd();
}

/**
 * @brief Emit the four vertices of a quad centered on one grid cell.
 *
 * This helper only submits geometry; the caller must already be inside
 * `glBegin(GL_QUADS)` and have selected the intended color.
 *
 * @param x           Grid-space x coordinate of the cell center.
 * @param y           Grid-space y coordinate of the cell center.
 * @param half_extent Half the width/height of the rendered quad.
 */
void emit_cell_quad_vertices(int x, int y, float half_extent) {
    glVertex2f(static_cast<float>(x) - half_extent,
               static_cast<float>(y) - half_extent);
    glVertex2f(static_cast<float>(x) + half_extent,
               static_cast<float>(y) - half_extent);
    glVertex2f(static_cast<float>(x) + half_extent,
               static_cast<float>(y) + half_extent);
    glVertex2f(static_cast<float>(x) - half_extent,
               static_cast<float>(y) + half_extent);
}

/**
 * @brief Draw the cached winding-fill cells for the current polygon.
 *
 * Fill quads are inset slightly so the grid remains visible. Edges and
 * vertices are rendered later on top of this interior pass.
 *
 * @param state Current polygon state.
 */
void draw_filled_cells(const App& state) {
    glColor3fv(kFillColor);
    glBegin(GL_QUADS);
    for (const Cell& cell : state.filled_cells) {
        emit_cell_quad_vertices(cell.x, cell.y, 0.46f);
    }
    glEnd();
}

/**
 * @brief Draw all cached line pixels with one high-contrast edge color.
 *
 * Pixels are inset slightly from the cell boundaries so the grid remains
 * visible beneath the rasterized polygon edges.
 *
 * @param state Current polygon state.
 */
void draw_line_pixels(const App& state) {
    glColor3fv(kEdgeColor);
    glBegin(GL_QUADS);
    for (const LineSegment& edge : state.edges) {
        for (const RasterPixel& pixel : edge.pixels) {
            emit_cell_quad_vertices(pixel.x, pixel.y, 0.4f);
        }
    }
    glEnd();
}

/**
 * @brief Draw all clicked vertices as red endpoint markers.
 *
 * Vertices are rendered after line pixels so the endpoints remain visible even
 * when the polygon edge passes through the same cell. Each vertex uses the
 * assignment color map order: red, orange, yellow, green, blue, purple, then
 * repeats cyclically.
 *
 * @param state Current polygon state.
 */
void draw_vertices(const App& state) {
    glBegin(GL_QUADS);
    for (std::size_t i = 0; i < state.vertices.size(); ++i) {
        const Cell& vertex = state.vertices[i];
        const NamedColor& color = kVertexColorMap[i % kVertexColorMap.size()];
        glColor3fv(color.rgb.data());
        emit_cell_quad_vertices(vertex.x, vertex.y, 0.5f);
    }
    glEnd();
}

void draw_query_point(const App& state) {
    if (!state.has_query_point) return;
    glColor3fv(kQueryPointColor);
    glBegin(GL_QUADS);
    emit_cell_quad_vertices(state.query_point.x, state.query_point.y, 0.24f);
    glEnd();
}

struct RegionTransform
{
    bool swap_xy = false;
    bool flip_x = false;
    bool flip_y = false;
};

/**
 * @brief Classify a segment and compute the transform into Region 1.
 *
 * The resulting flags normalize any non-degenerate segment into the canonical
 * case `dx' > 0`, `dy' >= 0`, `dx' >= dy'`, which allows one Bresenham core to
 * handle every direction.
 *
 * @param from Segment start in grid space.
 * @param to   Segment end in grid space.
 * @return Swap/flip flags for normalization.
 */
RegionTransform classify_region_transform(const Cell& from, const Cell& to) {
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;

    RegionTransform result{};
    if (dx == 0 && dy == 0) return result;

    result.swap_xy = std::abs(dy) > std::abs(dx);
    result.flip_x = (result.swap_xy ? dy : dx) < 0;
    result.flip_y = (result.swap_xy ? dx : dy) < 0;

    return result;
}

int region_from_transform(const RegionTransform& transform) {
    return kRegionTable[transform.swap_xy][transform.flip_x][transform.flip_y];
}

/**
 * @brief Apply the Region 1 normalization transform to one point.
 *
 * The transform order follows the implementation plan exactly: swap axes,
 * mirror x, then mirror y.
 *
 * @param p         Point in original grid space.
 * @param transform Transform flags returned by region classification.
 * @return Point expressed in the normalized Region 1 frame.
 */
Cell transform_point(Cell p, const RegionTransform& transform) {
    if (transform.swap_xy) std::swap(p.x, p.y);
    if (transform.flip_x) p.x = -p.x;
    if (transform.flip_y) p.y = -p.y;
    return p;
}

/**
 * @brief Undo a Region 1 normalization transform.
 *
 * This reverses `transform_point` so pixels generated in the normalized frame
 * can be mapped back into the original grid coordinates.
 *
 * @param p         Point in Region 1 coordinates.
 * @param transform Transform flags previously used for normalization.
 * @return Point mapped back to original grid space.
 */
Cell inverse_transform_point(Cell p, const RegionTransform& transform) {
    if (transform.flip_y) p.y = -p.y;
    if (transform.flip_x) p.x = -p.x;
    if (transform.swap_xy) std::swap(p.x, p.y);
    return p;
}

/**
 * @brief Rasterize one segment with a Region 1 Bresenham core.
 *
 * The input segment is normalized into Region 1, stepped with integer midpoint
 * updates, and each emitted pixel is mapped back to the original region while
 * preserving its `E/NE` classification.
 *
 * @param from Segment start in grid space.
 * @param to   Segment end in grid space.
 * @return Cached line segment metadata and emitted raster pixels.
 */
LineSegment rasterize_line(const Cell& from, const Cell& to) {
    LineSegment segment{};
    segment.from = from;
    segment.to = to;

    // Compute the region and the symmetry transform that maps the
    // original segment into the canonical Region 1 case.
    const RegionTransform transform = classify_region_transform(from, to);
    if (from.x == to.x && from.y == to.y) return segment;
    segment.region = region_from_transform(transform);

    // After normalization, start=(x0', y0') and end=(x1', y1') satisfy
    // dx' = x1' - x0' > 0, dy' = y1' - y0' >= 0, and dx' >= dy'.
    const Cell start = transform_point(from, transform);
    const Cell end = transform_point(to, transform);

    // These are the Region 1 major/minor axis lengths used in the implicit
    // line function F(x, y) = dy'(x - x0') - dx'(y - y0').
    const int transformed_dx = end.x - start.x;
    const int transformed_dy = end.y - start.y;
    if (transformed_dx <= 0 || transformed_dy < 0 ||
        transformed_dx < transformed_dy) {
        std::fprintf(stderr,
                     "Unexpected transform for line (%d,%d)->(%d,%d): "
                     "region=%d dx'=%d dy'=%d\n",
                     from.x, from.y, to.x, to.y, segment.region, transformed_dx,
                     transformed_dy);
        return segment;
    }

    // Begin from the start pixel (x0', y0'). The loop will emit the later
    // pixels only, because the start endpoint is drawn separately as a vertex.
    int y = start.y;

    // Integer midpoint decision variable for the first step:
    // d0 = 2 * F(x0' + 1, y0' + 1/2) = 2*dy' - dx'.
    int decision = 2 * transformed_dy - transformed_dx;
    for (int x = start.x + 1; x <= end.x; ++x) {
        // Default candidate is E = (x, y), meaning "advance only on major
        // axis".
        StepType step = StepType::E;
        if (decision < 0) {
            // The midpoint lies on the E side of the line, so choose E.
            // Recurrence: d <- d + 2*dy'.
            decision += 2 * transformed_dy;
        }
        else {
            // The midpoint lies on the NE side or exactly on the line, so
            // choose NE = (x, y + 1).
            ++y;
            // Recurrence: d <- d + 2*(dy' - dx').
            decision += 2 * (transformed_dy - transformed_dx);
            step = StepType::NE;
        }

        // Map the Region 1 pixel back to the original octant while preserving
        // the E/NE decision label for reporting and coloring.
        const Cell world = inverse_transform_point(Cell{x, y}, transform);
        segment.pixels.push_back(RasterPixel{world.x, world.y, step});
    }

    return segment;
}

/**
 * @brief Print the current polygon and all rasterized edges to stdout.
 *
 * Each edge line includes the original vertex labels, region id, and emitted
 * pixel sequence so the Lab06 rasterization steps can be inspected directly.
 *
 * @param state Current polygon state to report.
 */
void print_polygon_report(const App& state) {
    std::printf("=== Polygon vertices:");
    if (state.vertices.empty()) {
        std::printf(" (none)");
    }
    else {
        for (std::size_t i = 0; i < state.vertices.size(); ++i) {
            const Cell& vertex = state.vertices[i];
            std::printf(" v%zu=(%d,%d)", i + 1, vertex.x, vertex.y);
        }
    }
    std::printf(" ===\n\n");
    std::printf("Color map:");
    for (std::size_t i = 0; i < kVertexColorMap.size(); ++i) {
        std::printf(" v%zu=%s", i + 1, kVertexColorMap[i].name);
    }
    std::printf(" (then repeat)\n\n");

    if (state.has_query_point) {
        const char* direction = "none";
        if (state.query_result.on_boundary) {
            direction = "boundary";
        }
        else if (state.query_result.total > 0) {
            direction = "counterclockwise";
        }
        else if (state.query_result.total < 0) {
            direction = "clockwise";
        }
        std::printf("P=(%d,%d): cross=%s  total=%d  %s\n\n",
                    state.query_point.x, state.query_point.y, direction,
                    state.odd_even_query_result.crossings,
                    state.query_result.inside ? "inside" : "outside");
    }

    for (const LineSegment& edge : state.edges) {
        assert(edge.from_idx >= 0 && edge.to_idx >= 0);
        std::printf("Line v%d-v%d: region %d\n", edge.from_idx + 1,
                    edge.to_idx + 1, edge.region);
        std::printf("  (%d,%d) START\n", edge.from.x, edge.from.y);
        for (std::size_t i = 0; i < edge.pixels.size(); ++i) {
            if (i % 4 == 0) std::printf("  ");
            const RasterPixel& pixel = edge.pixels[i];
            std::printf("(%d,%d) %-2s", pixel.x, pixel.y,
                        pixel.step == StepType::E ? "E" : "NE");
            if (i % 4 == 3 || i + 1 == edge.pixels.size()) {
                std::printf("\n");
            }
            else {
                std::printf("  ");
            }
        }
        std::printf("\n");
    }
    std::fflush(stdout);
}

/**
 * @brief Recompute the edge cache from the current ordered vertex list.
 *
 * With two vertices, only one segment is generated. With three or more
 * vertices, the polygon is closed by connecting the last vertex back to the
 * first. Degenerate edges are skipped from the drawable cache.
 *
 * @param state Application state whose edge cache should be refreshed.
 */
void rebuild_edges(App& state) {
    state.edges.clear();
    state.filled_cells.clear();
    const std::size_t n = state.vertices.size();
    if (state.has_query_point) {
        state.query_result = {};
        state.odd_even_query_result = {};
    }
    if (n < 2) {
        print_polygon_report(state);
        return;
    }

    const std::size_t edge_count = (n >= 3) ? n : 1;
    state.edges.reserve(edge_count);
    for (std::size_t i = 0; i < edge_count; ++i) {
        const std::size_t next = (i + 1) % n;
        const Cell& from = state.vertices[i];
        const Cell& to = state.vertices[next];
        LineSegment edge = rasterize_line(from, to);
        edge.from_idx = static_cast<int>(i);
        edge.to_idx = static_cast<int>(next);
        if (edge.region != 0) {
            state.edges.push_back(std::move(edge));
        }
    }

    if (n >= 3) {
        // state.filled_cells = fill_polygon_odd_even(state.vertices);
        state.filled_cells = fill_polygon_non_zero(state.vertices);
        if (state.has_query_point && signed_area2(state.vertices) != 0) {
            state.query_result = query_point_non_zero(
                state.vertices, static_cast<double>(state.query_point.x),
                static_cast<double>(state.query_point.y));
            state.odd_even_query_result = query_point_odd_even(
                state.vertices, static_cast<double>(state.query_point.x),
                static_cast<double>(state.query_point.y));
        }
    }

    print_polygon_report(state);
}

/**
 * @brief Build the Lab08 overlay status string.
 *
 * Short polygons show explicit vertex coordinates; longer ones fall back to a
 * compact count plus the current grid dimension.
 *
 * @param state Current polygon state.
 * @return Overlay status string ready for bitmap text rendering.
 */
std::string build_overlay_summary(const App& state) {
    const std::size_t n = state.vertices.size();
    std::string summary = "Vertices: " + std::to_string(n);
    if (n >= 3) {
        summary += "  Fill: " + std::to_string(state.filled_cells.size());
    }
    if (n > 0 && n <= 4) {
        for (std::size_t i = 0; i < n; ++i) {
            char vertex_buf[32];
            std::snprintf(vertex_buf, sizeof(vertex_buf), "  v%zu=(%d,%d)",
                          i + 1, state.vertices[i].x, state.vertices[i].y);
            summary += vertex_buf;
        }
    }

    char suffix[24];
    std::snprintf(suffix, sizeof(suffix), "   [N = %d]", state.dimension);
    summary += suffix;
    return summary;
}

std::string build_colormap_summary() {
    std::string summary = "Color map:";
    for (std::size_t i = 0; i < kVertexColorMap.size(); ++i) {
        summary += " v" + std::to_string(i + 1) + "=" + kVertexColorMap[i].name;
    }
    summary += " ...";
    return summary;
}

std::string build_query_summary(const App& state) {
    if (!state.has_query_point) {
        return state.state == App::State::kPickQueryPoint
                   ? "P mode: click one grid cell to set P"
                   : "Press P, then click one grid cell to query P";
    }

    std::string direction = "none";
    if (state.query_result.on_boundary) {
        direction = "boundary";
    }
    else if (state.query_result.total > 0) {
        direction = "counterclockwise";
    }
    else if (state.query_result.total < 0) {
        direction = "clockwise";
    }

    return "P=(" + std::to_string(state.query_point.x) + "," +
           std::to_string(state.query_point.y) + ") cross=" + direction +
           " total=" + std::to_string(state.odd_even_query_result.crossings) +
           " " +
           (state.query_result.inside ? "inside" : "outside");
}

void draw_overlay(const App& state) {
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);
    constexpr float kOverlayMarginX = 20.0f;
    constexpr float kOverlayMarginY = 28.0f;
    const GridLayout layout = compute_grid_layout(width, height);

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_VIEWPORT_BIT |
                 GL_LINE_BIT);
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, width, 0.0, height, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3fv(kOverlayBandColor);
    glBegin(GL_QUADS);
    glVertex2i(0, 0);
    glVertex2i(width, 0);
    glVertex2i(width, layout.y);
    glVertex2i(0, layout.y);
    glEnd();

    glColor3fv(kOverlayColor);
    glBegin(GL_LINE_LOOP);
    glVertex2i(layout.x, layout.y);
    glVertex2i(layout.x + layout.width, layout.y);
    glVertex2i(layout.x + layout.width, layout.y + layout.height);
    glVertex2i(layout.x, layout.y + layout.height);
    glEnd();

    if (state.state == App::State::kCustomInput) {
        char prompt[128];
        std::snprintf(prompt, sizeof(prompt), "Enter dimension: [%s]_",
                      state.custom_buf.c_str());
        constexpr const char* hint = "(Enter to confirm, Esc to cancel)";
        constexpr float kPanelPaddingX = 20.0f;
        constexpr int kPanelPaddingTop = 16;
        constexpr int kPanelPaddingBottom = 14;
        constexpr int kLineGap = 12;

        const int prompt_width =
            bitmap_text_width(GLUT_BITMAP_HELVETICA_18, prompt);
        const int hint_width =
            bitmap_text_width(GLUT_BITMAP_HELVETICA_18, hint);
        constexpr int kTextHeight = 18;
        const int panel_width =
            (prompt_width > hint_width ? prompt_width : hint_width) +
            static_cast<int>(kPanelPaddingX * 2.0f);
        const int panel_height = kPanelPaddingTop + kTextHeight + kLineGap +
                                 kTextHeight + kPanelPaddingBottom;
        const int panel_x = (width - panel_width) / 2;
        const int panel_y = (height - panel_height) / 2;
        const int prompt_x = (width - prompt_width) / 2;
        const int hint_x = (width - hint_width) / 2;
        const int prompt_y =
            panel_y + panel_height - kPanelPaddingTop - kTextHeight;
        const int hint_y = panel_y + kPanelPaddingBottom;

        glColor3fv(kPanelFillColor);
        glBegin(GL_QUADS);
        glVertex2f(static_cast<float>(panel_x), static_cast<float>(panel_y));
        glVertex2f(static_cast<float>(panel_x + panel_width),
                   static_cast<float>(panel_y));
        glVertex2f(static_cast<float>(panel_x + panel_width),
                   static_cast<float>(panel_y + panel_height));
        glVertex2f(static_cast<float>(panel_x),
                   static_cast<float>(panel_y + panel_height));
        glEnd();

        glColor3fv(kPanelBorderColor);
        glBegin(GL_LINE_LOOP);
        glVertex2f(static_cast<float>(panel_x), static_cast<float>(panel_y));
        glVertex2f(static_cast<float>(panel_x + panel_width),
                   static_cast<float>(panel_y));
        glVertex2f(static_cast<float>(panel_x + panel_width),
                   static_cast<float>(panel_y + panel_height));
        glVertex2f(static_cast<float>(panel_x),
                   static_cast<float>(panel_y + panel_height));
        glEnd();

        glColor3fv(kOverlayColor);
        draw_bitmap_text(static_cast<float>(prompt_x),
                         static_cast<float>(prompt_y), prompt);
        draw_bitmap_text(static_cast<float>(hint_x), static_cast<float>(hint_y),
                         hint);
    }
    else {
        const std::string overlay_text = build_overlay_summary(state);
        const std::string colormap_text = build_colormap_summary();
        const std::string query_text = build_query_summary(state);
        draw_bitmap_text(kOverlayMarginX, kOverlayMarginY,
                         overlay_text.c_str());
        draw_bitmap_text(kOverlayMarginX, kOverlayMarginY + 24.0f,
                         colormap_text.c_str());
        draw_bitmap_text(kOverlayMarginX, kOverlayMarginY + 48.0f,
                         query_text.c_str());
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

bool try_apply_custom_dimension(App& state) {
    if (state.custom_buf.empty()) return false;

    const int value = std::atoi(state.custom_buf.c_str());
    if (value <= 0) return false;
    apply_dimension(state, value);
    return true;
}

}  // namespace

void App::reshape_cb(int w, int h) {
    apply_projection(w, h, app().dimension);
}

/**
 * @brief Render the Lab08 scene in grid, fill, line-pixel, vertex order.
 *
 * The winding-rule interior is drawn first, then the Lab06 edge rasterization
 * is layered on top, and finally the clicked vertices remain visible above
 * both.
 */
void App::display_cb() {
    App& state = app();
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);

    glClear(GL_COLOR_BUFFER_BIT);
    apply_projection(width, height, state.dimension);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    draw_grid(state.dimension);
    draw_filled_cells(state);
    draw_line_pixels(state);
    draw_vertices(state);
    draw_query_point(state);
    draw_overlay(state);
    glutSwapBuffers();
}

/**
 * @brief Toggle one polygon vertex from a left-clicked grid cell.
 *
 * Clicking an empty cell appends a new vertex at the end of the polygon list.
 * Clicking a cell that already exists in the vertex list removes that vertex,
 * treating the click as if the vertex had never been added.
 */
void App::mouse_cb(int button, int state, int x, int y) {
    App& self = app();
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN ||
        self.state == State::kCustomInput) {
        return;
    }

    int gx = 0;
    int gy = 0;
    const int window_width = glutGet(GLUT_WINDOW_WIDTH);
    const int window_height = glutGet(GLUT_WINDOW_HEIGHT);
    const GridLayout layout = compute_grid_layout(window_width, window_height);
    const int local_x = x - layout.x;
    const int local_y = y - (window_height - (layout.y + layout.height));
    if (local_x < 0 || local_x >= layout.width || local_y < 0 ||
        local_y >= layout.height) {
        return;
    }
    if (!window_to_grid(local_x, local_y, layout.width, layout.height,
                        self.dimension, &gx, &gy)) {
        return;
    }

    if (self.state == State::kPickQueryPoint) {
        self.query_point = Cell{gx, gy};
        self.has_query_point = true;
        self.query_result = {};
        self.odd_even_query_result = {};
        if (self.vertices.size() >= 3 && signed_area2(self.vertices) != 0) {
            self.query_result =
                query_point_non_zero(self.vertices, static_cast<double>(gx),
                                     static_cast<double>(gy));
            self.odd_even_query_result =
                query_point_odd_even(self.vertices, static_cast<double>(gx),
                                     static_cast<double>(gy));
        }
        self.state = State::kNormal;
        print_polygon_report(self);
    }
    else {
        const auto existing =
            std::find_if(self.vertices.begin(), self.vertices.end(),
                         [gx, gy](const Cell& vertex) {
                             return vertex.x == gx && vertex.y == gy;
                         });
        if (existing != self.vertices.end()) {
            self.vertices.erase(existing);
        }
        else {
            self.vertices.push_back(Cell{gx, gy});
        }

        rebuild_edges(self);
    }
    glutPostRedisplay();
}

/**
 * @brief Handle Lab08 keyboard actions.
 *
 * In normal mode, `Esc` clears all vertices and rasterized edges. In custom
 * input mode, the handler reuses Lab05's dimension text-entry behavior.
 */
void App::keyboard_cb(unsigned char key, int, int) {
    App& self = app();

    if (self.state == State::kCustomInput) {
        if (key >= '0' && key <= '9' &&
            self.custom_buf.size() < kMaxCustomDigits) {
            self.custom_buf.push_back(static_cast<char>(key));
            glutPostRedisplay();
            return;
        }
        if ((key == kKeyBackspace || key == kKeyDelete) &&
            !self.custom_buf.empty()) {
            self.custom_buf.pop_back();
            glutPostRedisplay();
            return;
        }
        if (key == kKeyEnter) {
            if (try_apply_custom_dimension(self)) return;
            glutPostRedisplay();
            return;
        }
        if (key == kKeyEscape) {
            self.state = State::kNormal;
            self.custom_buf.clear();
            glutPostRedisplay();
            return;
        }
        return;
    }

    // Select P1 (user)
    if (key == 'p' || key == 'P') {
        self.state = State::kPickQueryPoint;
        glutPostRedisplay();
        return;
    }

    if (key == kKeyEscape) {
        if (self.state == State::kPickQueryPoint) {
            self.state = State::kNormal;
            glutPostRedisplay();
            return;
        }
        self.vertices.clear();
        self.has_query_point = false;
        self.query_result = {};
        self.odd_even_query_result = {};
        rebuild_edges(self);
        glutPostRedisplay();
    }
}

void App::menu_cb(int value) {
    App& self = app();
    switch (value) {
        case kMenuDim10:
        case kMenuDim15:
        case kMenuDim20:
            apply_dimension(self, value);
            return;
        case kMenuCustom:
            self.state = State::kCustomInput;
            self.custom_buf.clear();
            glutPostRedisplay();
            return;
        default:
            return;
    }
}

}  // namespace demo

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(700, 700);
    glutInitWindowPosition(600, 80);
    const int win = glutCreateWindow("Polygon Fill");
    if (win <= 0) return 1;

    demo::initialize_gl_state();

    static demo::App app{};
    glutSetWindowData(&app);

    glutReshapeFunc(&demo::App::reshape_cb);
    glutDisplayFunc(&demo::App::display_cb);
    glutMouseFunc(&demo::App::mouse_cb);
    glutKeyboardFunc(&demo::App::keyboard_cb);

    glutCreateMenu(&demo::App::menu_cb);
    glutAddMenuEntry("10", demo::kMenuDim10);
    glutAddMenuEntry("15", demo::kMenuDim15);
    glutAddMenuEntry("20", demo::kMenuDim20);
    glutAddMenuEntry("Custom...", demo::kMenuCustom);
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    glutMainLoop();
    return 0;
}
