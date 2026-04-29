#include <GL/freeglut.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

namespace demo
{

struct Cell
{
    int gx = 0;
    int gy = 0;

    bool operator<(const Cell& other) const {
        if (gx != other.gx) return gx < other.gx;
        return gy < other.gy;
    }
};

struct App
{
    int dimension = 10;
    std::set<Cell> filled_cells;
    bool has_last_cell = false;
    int last_gx = 0;
    int last_gy = 0;

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
};

namespace
{

constexpr float kBackgroundColor[3] = {0.15f, 0.15f, 0.17f};
constexpr float kGridColor[3] = {0.42f, 0.42f, 0.44f};
constexpr float kFillColorA[3] = {0.95f, 0.55f, 0.10f};
constexpr float kFillColorB[3] = {0.10f, 0.70f, 0.90f};
constexpr float kOverlayColor[3] = {0.85f, 0.85f, 0.85f};
constexpr float kOverlayBandColor[3] = {0.22f, 0.22f, 0.24f};
constexpr float kSelectionColor[3] = {0.96f, 0.96f, 0.98f};
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

void apply_dimension(App& state, int new_dimension) {
    if (new_dimension <= 0) return;
    state.dimension = new_dimension;
    state.filled_cells.clear();
    state.has_last_cell = false;
    state.last_gx = 0;
    state.last_gy = 0;
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

/**
 * @brief Convert window pixel coordinates to grid cell coordinates.
 *
 * Maps a GLUT mouse-click position (win_x, win_y) in window space to the
 * integer grid cell (gx, gy). The visible grid uses an orthographic projection
 * of [-n-0.5, n+0.5] on both axes. The grid has 2n+1 cells per axis, with
 * integer coordinates denoting cell centers, so the center cell is (0, 0).
 * Cell (gx, gy) covers the world rectangle
 * [gx-0.5, gx+0.5] x [gy-0.5, gy+0.5].
 *
 * Conversion steps:
 *   1. Normalize pixel -> [0,1]: u = win_x / width, v = win_y / height
 *   2. Map to world using the visible ortho bounds:
 *      wx = (-n - 0.5) + u * (2n + 1)
 *      wy = ( n + 0.5) - v * (2n + 1)
 *      (v is flipped because GLUT y-axis points downward)
 *   3. Cell index: gx = floor(wx + 0.5), gy = floor(wy + 0.5)
 *   4. Clamp check: valid iff gx in [-n, n] and gy in [-n, n]
 *
 * @param win_x   Pixel x from the left edge of the window.
 * @param win_y   Pixel y from the top edge of the window (GLUT convention).
 * @param width   Window width in pixels (must be > 0).
 * @param height  Window height in pixels (must be > 0).
 * @param n       Half-dimension of the grid (coordinate range [-n, n]).
 * @param[out] gx Receives the grid column index in [-n, n].
 * @param[out] gy Receives the grid row index in [-n, n].
 * @return true if the click is inside the grid; false if it falls outside.
 */
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

void draw_filled_cells(const App& state) {
    glBegin(GL_QUADS);
    for (const auto& [gx, gy] : state.filled_cells) {
        const int parity = (gx + gy) & 1;
        glColor3fv(parity == 0 ? kFillColorA : kFillColorB);
        glVertex2f(static_cast<float>(gx) - 0.5f,
                   static_cast<float>(gy) - 0.5f);
        glVertex2f(static_cast<float>(gx) + 0.5f,
                   static_cast<float>(gy) - 0.5f);
        glVertex2f(static_cast<float>(gx) + 0.5f,
                   static_cast<float>(gy) + 0.5f);
        glVertex2f(static_cast<float>(gx) - 0.5f,
                   static_cast<float>(gy) + 0.5f);
    }
    glEnd();
}

void draw_last_cell_highlight(const App& state) {
    if (!state.has_last_cell) return;

    glColor3fv(kSelectionColor);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(static_cast<float>(state.last_gx) - 0.5f,
               static_cast<float>(state.last_gy) - 0.5f);
    glVertex2f(static_cast<float>(state.last_gx) + 0.5f,
               static_cast<float>(state.last_gy) - 0.5f);
    glVertex2f(static_cast<float>(state.last_gx) + 0.5f,
               static_cast<float>(state.last_gy) + 0.5f);
    glVertex2f(static_cast<float>(state.last_gx) - 0.5f,
               static_cast<float>(state.last_gy) + 0.5f);
    glEnd();
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
    else if (state.has_last_cell) {
        char buffer[64];
        char dimension_buf[32];
        std::snprintf(buffer, sizeof(buffer), "Cell: (%d, %d)", state.last_gx,
                      state.last_gy);
        std::snprintf(dimension_buf, sizeof(dimension_buf), "N = %d",
                      state.dimension);
        draw_bitmap_text(kOverlayMarginX, kOverlayMarginY, buffer);
        draw_bitmap_text(
            static_cast<float>(
                width -
                bitmap_text_width(GLUT_BITMAP_HELVETICA_18, dimension_buf) -
                static_cast<int>(kOverlayMarginX)),
            kOverlayMarginY, dimension_buf);
    }
    else {
        char dimension_buf[32];
        std::snprintf(dimension_buf, sizeof(dimension_buf), "N = %d",
                      state.dimension);
        draw_bitmap_text(kOverlayMarginX, kOverlayMarginY, "Click a cell");
        draw_bitmap_text(
            static_cast<float>(
                width -
                bitmap_text_width(GLUT_BITMAP_HELVETICA_18, dimension_buf) -
                static_cast<int>(kOverlayMarginX)),
            kOverlayMarginY, dimension_buf);
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

void App::display_cb() {
    App& state = app();
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);

    glClear(GL_COLOR_BUFFER_BIT);
    apply_projection(width, height, state.dimension);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    draw_filled_cells(state);
    draw_grid(state.dimension);
    draw_last_cell_highlight(state);
    draw_overlay(state);
    glutSwapBuffers();
}

void App::mouse_cb(int button, int state, int x, int y) {
    App& self = app();
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN ||
        self.state != State::kNormal) {
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

    const auto [it, inserted] = self.filled_cells.insert(Cell{gx, gy});
    if (!inserted) self.filled_cells.erase(it);
    self.has_last_cell = true;
    self.last_gx = gx;
    self.last_gy = gy;
    std::printf("Clicked cell: (%d, %d)\n", gx, gy);
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

    if (key == kKeyEscape) {
        self.filled_cells.clear();
        self.has_last_cell = false;
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
    const int win = glutCreateWindow("2D Clickable Grid");
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
