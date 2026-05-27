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

#include "animation.hpp"
#include "jordan curve.hpp"

namespace demo
{

enum class StepType
{
    E,
    NE,
};

struct RasterPixel
{
    int x = 0;
    int y = 0;
    StepType step = StepType::E;
};

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
    std::vector<Cell> vertices;
    std::vector<LineSegment> edges;
    std::vector<AnimationStep> animation_steps;
    AnimationPlaybackState playback;

    enum class State
    {
        kNormal,
        kCustomInput,
    } state = State::kNormal;

    std::string custom_buf;

    static void reshape_cb(int w, int h);
    static void display_cb();
    static void mouse_cb(int button, int state, int x, int y);
    static void keyboard_cb(unsigned char key, int x, int y);
    static void menu_cb(int value);
    static void timer_cb(int generation);
};

namespace
{

constexpr float kBackgroundColor[3] = {0.15f, 0.15f, 0.17f};
constexpr float kGridColor[3] = {0.42f, 0.42f, 0.44f};
constexpr float kEdgeColor[3] = {0.0f, 0.0f, 0.0f};
constexpr float kInsideColor[3] = {0.96f, 0.96f, 0.96f};
constexpr float kOutsideColor[3] = {0.93f, 0.42f, 0.30f};
constexpr float kCurrentStepColor[3] = {0.18f, 0.72f, 1.00f};
constexpr float kOverlayColor[3] = {0.85f, 0.85f, 0.85f};
constexpr float kOverlayBandColor[3] = {0.22f, 0.22f, 0.24f};
constexpr float kPanelFillColor[3] = {0.24f, 0.24f, 0.24f};
constexpr float kPanelBorderColor[3] = {0.60f, 0.60f, 0.60f};

constexpr unsigned char kKeyBackspace = '\b';
constexpr unsigned char kKeyDelete = 127;
constexpr unsigned char kKeyEnter = '\r';
constexpr unsigned char kKeyEscape = '\x1b';
constexpr std::size_t kMaxCustomDigits = 6;
constexpr int kMinPlaybackDelayMs = 30;
constexpr int kMaxPlaybackDelayMs = 1000;
constexpr int kPlaybackDelayStepMs = 30;

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
    constexpr int kBottomOverlayBand = 100;

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

void reset_animation(App& state) {
    reset_animation(state.animation_steps, state.playback);
}

void apply_dimension(App& state, int new_dimension) {
    if (new_dimension <= 0) return;
    state.dimension = new_dimension;
    state.vertices.clear();
    state.edges.clear();
    reset_animation(state);
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

void draw_revealed_fill(const App& state) {
    glBegin(GL_QUADS);
    for (std::size_t i = 0; i < state.playback.revealed_step_count; ++i) {
        const AnimationStep& step = state.animation_steps[i];
        if (step.query.on_boundary) continue;
        glColor3fv(step.query.inside ? kInsideColor : kOutsideColor);
        emit_cell_quad_vertices(step.cell.x, step.cell.y, 0.46f);
    }
    glEnd();
}

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

void draw_current_step_cursor(const App& state) {
    const AnimationStep* step =
        current_animation_step(state.animation_steps, state.playback);
    if (step == nullptr) return;

    glColor3fv(kCurrentStepColor);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(static_cast<float>(step->cell.x) - 0.5f,
               static_cast<float>(step->cell.y) - 0.5f);
    glVertex2f(static_cast<float>(step->cell.x) + 0.5f,
               static_cast<float>(step->cell.y) - 0.5f);
    glVertex2f(static_cast<float>(step->cell.x) + 0.5f,
               static_cast<float>(step->cell.y) + 0.5f);
    glVertex2f(static_cast<float>(step->cell.x) - 0.5f,
               static_cast<float>(step->cell.y) + 0.5f);
    glEnd();
}

struct RegionTransform
{
    bool swap_xy = false;
    bool flip_x = false;
    bool flip_y = false;
};

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

Cell transform_point(Cell p, const RegionTransform& transform) {
    if (transform.swap_xy) std::swap(p.x, p.y);
    if (transform.flip_x) p.x = -p.x;
    if (transform.flip_y) p.y = -p.y;
    return p;
}

Cell inverse_transform_point(Cell p, const RegionTransform& transform) {
    if (transform.flip_y) p.y = -p.y;
    if (transform.flip_x) p.x = -p.x;
    if (transform.swap_xy) std::swap(p.x, p.y);
    return p;
}

LineSegment rasterize_line(const Cell& from, const Cell& to) {
    LineSegment segment{};
    segment.from = from;
    segment.to = to;

    const RegionTransform transform = classify_region_transform(from, to);
    if (from.x == to.x && from.y == to.y) return segment;
    segment.region = region_from_transform(transform);

    const Cell start = transform_point(from, transform);
    const Cell end = transform_point(to, transform);

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

    int y = start.y;
    int decision = 2 * transformed_dy - transformed_dx;
    for (int x = start.x + 1; x <= end.x; ++x) {
        StepType step = StepType::E;
        if (decision < 0) {
            decision += 2 * transformed_dy;
        }
        else {
            ++y;
            decision += 2 * (transformed_dy - transformed_dx);
            step = StepType::NE;
        }

        const Cell world = inverse_transform_point(Cell{x, y}, transform);
        segment.pixels.push_back(RasterPixel{world.x, world.y, step});
    }

    return segment;
}

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

    std::printf("Animation steps: %zu  revealed=%zu\n\n",
                state.animation_steps.size(),
                state.playback.revealed_step_count);

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

CellSet collect_boundary_cells(const App& state) {
    CellSet boundary_cells;
    for (const Cell& vertex : state.vertices) {
        boundary_cells.insert(std::make_pair(vertex.x, vertex.y));
    }
    for (const LineSegment& edge : state.edges) {
        boundary_cells.insert(std::make_pair(edge.from.x, edge.from.y));
        boundary_cells.insert(std::make_pair(edge.to.x, edge.to.y));
        for (const RasterPixel& pixel : edge.pixels) {
            boundary_cells.insert(std::make_pair(pixel.x, pixel.y));
        }
    }
    return boundary_cells;
}

void build_animation_steps(App& state) {
    state.animation_steps =
        build_animation_steps(state.vertices, collect_boundary_cells(state));
    state.playback.revealed_step_count = 0;
}

void rebuild_edges(App& state) {
    state.edges.clear();
    reset_animation(state);

    const std::size_t n = state.vertices.size();
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

    build_animation_steps(state);
    print_polygon_report(state);
}

void schedule_next_frame(App& state) {
    if (state.playback.mode == PlaybackMode::Stopped) return;
    glutTimerFunc(static_cast<unsigned int>(state.playback.playback_delay_ms),
                  &App::timer_cb, state.playback.playback_generation);
}

std::string build_overlay_summary(const App& state) {
    const std::size_t n = state.vertices.size();
    std::string summary = "Vertices: " + std::to_string(n);
    if (n > 0 && n <= 4) {
        for (std::size_t i = 0; i < n; ++i) {
            char vertex_buf[32];
            std::snprintf(vertex_buf, sizeof(vertex_buf), "  v%zu=(%d,%d)",
                          i + 1, state.vertices[i].x, state.vertices[i].y);
            summary += vertex_buf;
        }
    }

    char suffix[32];
    std::snprintf(suffix, sizeof(suffix), "   [N = %d]", state.dimension);
    summary += suffix;
    return summary;
}

std::string build_colormap_summary() {
    return "Vertices: rainbow  Edge: black  Inside(A): white  Outside(B): red";
}

std::string build_playback_summary(const App& state) {
    std::string status = "ready";
    switch (state.playback.mode) {
        case PlaybackMode::Reversing:
            status = "reversing";
            break;
        case PlaybackMode::Playing:
            status = "playing";
            break;
        case PlaybackMode::Stopped:
            if (!state.animation_steps.empty()) {
                if (state.playback.revealed_step_count >=
                    state.animation_steps.size())
                    status = "finished";
                else if (state.playback.revealed_step_count == 0)
                    status = "reversed";
                else
                    status = "stepping";
            }
            break;
    }

    return "Playback: " + status + "  step " +
           std::to_string(state.playback.revealed_step_count) + "/" +
           std::to_string(state.animation_steps.size()) +
           "  delay=" + std::to_string(state.playback.playback_delay_ms) +
           "ms  controls: P play/pause, V reverse, N/Space next, R replay, +/- "
           "speed";
}

std::string build_crossing_summary(const App& state) {
    if (state.vertices.size() < 3) {
        return "Add at least 3 vertices to build the Jordan Curve animation";
    }
    if (state.animation_steps.empty()) {
        return "Polygon is degenerate, so no animation steps are available";
    }

    const AnimationStep* step =
        current_animation_step(state.animation_steps, state.playback);
    if (step == nullptr) {
        return "Current pixel: none  cross number = -  classification = "
               "waiting";
    }

    std::string classification = "outside";
    if (step->query.on_boundary) {
        classification = "boundary";
    }
    else if (step->query.inside) {
        classification = "inside";
    }
    if (step->visual_boundary && !step->query.on_boundary) {
        classification += " [edge pixel]";
    }

    return "Current pixel: (" + std::to_string(step->cell.x) + "," +
           std::to_string(step->cell.y) +
           ")  cross number = " + std::to_string(step->query.crossings) +
           "  classification = " + classification;
}

void draw_overlay(const App& state) {
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);
    constexpr float kOverlayMarginX = 20.0f;
    constexpr float kOverlayMarginY = 18.0f;
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
        const std::string playback_text = build_playback_summary(state);
        const std::string crossing_text = build_crossing_summary(state);
        draw_bitmap_text(kOverlayMarginX, kOverlayMarginY,
                         overlay_text.c_str());
        draw_bitmap_text(kOverlayMarginX, kOverlayMarginY + 22.0f,
                         colormap_text.c_str());
        draw_bitmap_text(kOverlayMarginX, kOverlayMarginY + 44.0f,
                         playback_text.c_str());
        draw_bitmap_text(kOverlayMarginX, kOverlayMarginY + 66.0f,
                         crossing_text.c_str());
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

void restart_animation(App& state) {
    stop_animation(state.playback);
    state.playback.revealed_step_count = 0;
    if (!state.animation_steps.empty()) {
        state.playback.mode = PlaybackMode::Playing;
        schedule_next_frame(state);
    }
    glutPostRedisplay();
}

void reschedule_animation_if_playing(App& state) {
    if (state.playback.mode == PlaybackMode::Stopped) return;
    ++state.playback.playback_generation;
    schedule_next_frame(state);
}

}  // namespace

void App::reshape_cb(int w, int h) {
    apply_projection(w, h, app().dimension);
}

void App::display_cb() {
    App& state = app();
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);

    glClear(GL_COLOR_BUFFER_BIT);
    apply_projection(width, height, state.dimension);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    draw_grid(state.dimension);
    draw_revealed_fill(state);
    draw_line_pixels(state);
    draw_vertices(state);
    draw_current_step_cursor(state);
    draw_overlay(state);
    glutSwapBuffers();
}

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
    glutPostRedisplay();
}

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

    const auto start_playback = [&](PlaybackMode direction) {
        stop_animation(self.playback);
        if (direction == PlaybackMode::Playing) {
            if (self.playback.revealed_step_count >=
                self.animation_steps.size())
                self.playback.revealed_step_count = 0;
        }
        else {
            if (self.playback.revealed_step_count == 0)
                self.playback.revealed_step_count = self.animation_steps.size();
        }
        self.playback.mode = direction;
        schedule_next_frame(self);
        glutPostRedisplay();
    };

    if (key == 'p' || key == 'P') {
        if (self.animation_steps.empty()) return;
        if (self.playback.mode != PlaybackMode::Stopped) {
            stop_animation(self.playback);
            glutPostRedisplay();
            return;
        }
        start_playback(PlaybackMode::Playing);
        return;
    }

    if (key == 'v' || key == 'V') {
        if (self.animation_steps.empty()) return;
        if (self.playback.mode == PlaybackMode::Reversing) {
            stop_animation(self.playback);
            glutPostRedisplay();
            return;
        }
        start_playback(PlaybackMode::Reversing);
        return;
    }

    if (key == ' ' || key == 'n' || key == 'N') {
        if (self.animation_steps.empty()) return;
        stop_animation(self.playback);
        if (self.playback.revealed_step_count >= self.animation_steps.size()) {
            glutPostRedisplay();
            return;
        }
        ++self.playback.revealed_step_count;
        glutPostRedisplay();
        return;
    }

    if (key == 'r' || key == 'R') {
        restart_animation(self);
        return;
    }

    if (key == '+' || key == '=') {
        self.playback.playback_delay_ms =
            std::max(kMinPlaybackDelayMs,
                     self.playback.playback_delay_ms - kPlaybackDelayStepMs);
        reschedule_animation_if_playing(self);
        glutPostRedisplay();
        return;
    }

    if (key == '-' || key == '_') {
        self.playback.playback_delay_ms =
            std::min(kMaxPlaybackDelayMs,
                     self.playback.playback_delay_ms + kPlaybackDelayStepMs);
        reschedule_animation_if_playing(self);
        glutPostRedisplay();
        return;
    }

    if (key == kKeyEscape) {
        self.vertices.clear();
        rebuild_edges(self);
        glutPostRedisplay();
        return;
    }
}

void App::timer_cb(int generation) {
    App& state = app();
    if (state.playback.mode == PlaybackMode::Stopped ||
        generation != state.playback.playback_generation) {
        return;
    }

    if (state.playback.mode == PlaybackMode::Reversing) {
        if (state.playback.revealed_step_count > 0)
            --state.playback.revealed_step_count;
        if (state.playback.revealed_step_count == 0) {
            state.playback.mode = PlaybackMode::Stopped;
            glutPostRedisplay();
            return;
        }
    }
    else {
        if (state.playback.revealed_step_count < state.animation_steps.size())
            ++state.playback.revealed_step_count;
        if (state.playback.revealed_step_count >=
            state.animation_steps.size()) {
            state.playback.revealed_step_count = state.animation_steps.size();
            state.playback.mode = PlaybackMode::Stopped;
            glutPostRedisplay();
            return;
        }
    }

    schedule_next_frame(state);
    glutPostRedisplay();
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
    const int win = glutCreateWindow("Jordan Curve Animation");
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
