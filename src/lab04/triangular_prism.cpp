#include <GL/freeglut.h>

#include <cctype>
#include <cstdio>

#include "view_controller.hpp"

namespace demo
{

static void draw_axis_label(float x, float y, float z, const char label) {
    glRasterPos3f(x, y, z);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, label);
}

static void draw_axes() {
    constexpr float axis_length = 2.0f;
    constexpr float arrow_size = 0.15f;

    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT);
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);

    glBegin(GL_LINES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(axis_length, 0.0f, 0.0f);
    glVertex3f(axis_length, 0.0f, 0.0f);
    glVertex3f(axis_length - arrow_size, arrow_size * 0.5f, 0.0f);
    glVertex3f(axis_length, 0.0f, 0.0f);
    glVertex3f(axis_length - arrow_size, -arrow_size * 0.5f, 0.0f);

    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, axis_length, 0.0f);
    glVertex3f(0.0f, axis_length, 0.0f);
    glVertex3f(arrow_size * 0.5f, axis_length - arrow_size, 0.0f);
    glVertex3f(0.0f, axis_length, 0.0f);
    glVertex3f(-arrow_size * 0.5f, axis_length - arrow_size, 0.0f);

    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, axis_length);
    glVertex3f(0.0f, 0.0f, axis_length);
    glVertex3f(0.0f, arrow_size * 0.5f, axis_length - arrow_size);
    glVertex3f(0.0f, 0.0f, axis_length);
    glVertex3f(0.0f, -arrow_size * 0.5f, axis_length - arrow_size);
    glEnd();

    glColor3f(1.0f, 0.0f, 0.0f);
    draw_axis_label(axis_length + 0.12f, 0.0f, 0.0f, 'X');
    glColor3f(0.0f, 1.0f, 0.0f);
    draw_axis_label(0.0f, axis_length + 0.12f, 0.0f, 'Y');
    glColor3f(0.0f, 0.0f, 1.0f);
    draw_axis_label(0.0f, 0.0f, axis_length + 0.12f, 'Z');

    glPopAttrib();
}

static void draw_bitmap_text(int x, int y, const char* text) {
    glRasterPos2i(x, y);
    for (const char* p = text; *p != '\0'; ++p) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *p);
    }
}

static Matrix4 build_scene_projection_matrix(int width, int height) {
    return Projection::perspective(45.0f, static_cast<float>(width) / height,
                                   0.1f, 100.0f);
}

static Matrix4 build_scene_view_matrix() {
    return View::look_at(Vec3{0.0f, 2.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f},
                         Vec3{0.0f, 1.0f, 0.0f});
}

static Matrix4 build_scene_orbit_matrix(const OrbitViewController& view) {
    return Transform::rotation_x(view.pitch) * Transform::rotation_y(view.yaw);
}

static Vec3 homogenize_point(const Vec4& value) {
    assert(std::fabs(value.w) > kEpsilon &&
           "homogenize_point() requires a non-zero w");
    return Vec3{value.x / value.w, value.y / value.w, value.z / value.w};
}

static bool unproject_mouse_to_world_plane(
    int mouse_x, int mouse_y, int width, int height,
    const OrbitViewController& view, Vec3* world_point) {
    if (height <= 0) return false;

    const float ndc_x = 2.0f * static_cast<float>(mouse_x) / width - 1.0f;
    const float ndc_y =
        1.0f - 2.0f * static_cast<float>(mouse_y) / static_cast<float>(height);

    const Matrix4 inverse_clip_to_world =
        (build_scene_projection_matrix(width, height) * build_scene_view_matrix() *
         build_scene_orbit_matrix(view))
            .inverted();

    const Vec3 near_point = homogenize_point(
        inverse_clip_to_world * Vec4{ndc_x, ndc_y, -1.0f, 1.0f});
    const Vec3 far_point = homogenize_point(
        inverse_clip_to_world * Vec4{ndc_x, ndc_y, 1.0f, 1.0f});
    const Vec3 ray_direction = far_point - near_point;

    if (std::fabs(ray_direction.z) <= kEpsilon) return false;

    const float t = -near_point.z / ray_direction.z;
    if (t < 0.0f) return false;

    *world_point = near_point + ray_direction * t;
    return true;
}

static void draw_selected_axis(const Vec3& selected_point) {
    const Vec3 ray_direction = normalize(selected_point);
    const float ray_length = 6.0f;
    const Vec3 ray_start = ray_direction * -ray_length;
    const Vec3 ray_end = ray_direction * ray_length;

    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_POINT_BIT);
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(1, 0x00FF);
    glColor3f(0.1f, 0.1f, 0.1f);

    glBegin(GL_LINES);
    glVertex3f(ray_start.x, ray_start.y, ray_start.z);
    glVertex3f(ray_end.x, ray_end.y, ray_end.z);
    glEnd();

    glDisable(GL_LINE_STIPPLE);
    glPointSize(10.0f);
    glColor3f(1.0f, 0.4f, 0.1f);
    glBegin(GL_POINTS);
    glVertex3f(selected_point.x, selected_point.y, selected_point.z);
    glEnd();
    glPopAttrib();
}

static void draw_controls_overlay(bool pick_mode_armed, bool has_selected_axis,
                                  int last_click_x, int last_click_y) {
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);
    char pick_line[96];
    char axis_line[96];
    const char* pick_mode_text = "off";
    if (pick_mode_armed) {
        pick_mode_text = "waiting for click";
    } else if (has_selected_axis) {
        pick_mode_text = "axis locked";
    }
    std::snprintf(pick_line, sizeof(pick_line), "M: pick axis mode [%s]",
                  pick_mode_text);
    std::snprintf(axis_line, sizeof(axis_line), "T/G: arbitrary axis rotate [%s]",
                  has_selected_axis ? "ready" : "need picked point");
    const char* rotate_line = has_selected_axis
                                  ? "Rotate model: T/G=selected axis only"
                                  : "Rotate model: W/S=X  A/D=Y  Q/E=Z";
    constexpr const char* kOverlayLinesPrefix[] = {
        "View: drag mouse or Arrow keys",
    };
    constexpr const char* kOverlayLinesSuffix[] = {
        "Translate: J/L=X  I/K=Y  U/O=Z",
        "Scale: Z/X=X  C/V=Y  B/N=Z",
        "R: reset view/model/picked axis",
    };

    glPushAttrib(GL_ENABLE_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    const Matrix4 overlay_projection =
        Projection::orthographic(0.0f, static_cast<float>(width), 0.0f,
                                 static_cast<float>(height), -1.0f, 1.0f);
    glMultMatrixf(overlay_projection.data());

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glColor3f(0.15f, 0.15f, 0.15f);

    int line_index = 0;
    for (const char* line : kOverlayLinesPrefix) {
        draw_bitmap_text(12, height - 24 - line_index * 22, line);
        ++line_index;
    }
    draw_bitmap_text(12, height - 24 - line_index * 22, rotate_line);
    ++line_index;
    for (const char* line : kOverlayLinesSuffix) {
        draw_bitmap_text(12, height - 24 - line_index * 22, line);
        ++line_index;
    }
    draw_bitmap_text(12, height - 24 - line_index * 22,
                     pick_line);
    ++line_index;
    draw_bitmap_text(12, height - 24 - line_index * 22,
                     axis_line);
    ++line_index;
    if (has_selected_axis) {
        char click_line[96];
        std::snprintf(click_line, sizeof(click_line),
                      "Last click window coordinate: (%d, %d)", last_click_x,
                      last_click_y);
        draw_bitmap_text(12, height - 24 - line_index * 22, click_line);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glPopAttrib();
}

struct ModelTransform
{
    static constexpr float kRotationStep = 5.0f;
    static constexpr float kTranslateStep = 0.1f;
    static constexpr float kScaleStep = 0.1f;
    static constexpr float kMinScale = 0.1f;

    Quaternion orientation = Quaternion::identity();

    Vec3 position{};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    void apply() const {
        const Matrix4 transform = Transform::trs(position, orientation, scale);
        glMultMatrixf(transform.data());
    }

    void reset() {
        orientation = Quaternion::identity();
        position = Vec3{};
        scale = Vec3{1.0f, 1.0f, 1.0f};
        glutPostRedisplay();
    }

    // Key setting:
    // W/S, A/D, Q/E rotate around the object's local X/Y/Z axes.
    // T/G rotate around the arbitrary axis selected by the latest mouse click.
    // J/L, I/K, U/O translate the model. Z/X, C/V, B/N scale the model.
    bool on_keyboard(unsigned char key, const Vec3* arbitrary_axis,
                     bool arbitrary_axis_locked) {
        key = static_cast<unsigned char>(std::tolower(key));
        switch (key) {
            case 'w':
                if (arbitrary_axis_locked) return false;
                rotate_about_local_axis(Vec3{1.0f, 0.0f, 0.0f}, kRotationStep);
                break;
            case 's':
                if (arbitrary_axis_locked) return false;
                rotate_about_local_axis(Vec3{1.0f, 0.0f, 0.0f}, -kRotationStep);
                break;
            case 'a':
                if (arbitrary_axis_locked) return false;
                rotate_about_local_axis(Vec3{0.0f, 1.0f, 0.0f}, -kRotationStep);
                break;
            case 'd':
                if (arbitrary_axis_locked) return false;
                rotate_about_local_axis(Vec3{0.0f, 1.0f, 0.0f}, kRotationStep);
                break;
            case 'q':
                if (arbitrary_axis_locked) return false;
                rotate_about_local_axis(Vec3{0.0f, 0.0f, 1.0f}, -kRotationStep);
                break;
            case 'e':
                if (arbitrary_axis_locked) return false;
                rotate_about_local_axis(Vec3{0.0f, 0.0f, 1.0f}, kRotationStep);
                break;
            case 't':
                if (arbitrary_axis == nullptr) return false;
                rotate_about_world_axis(*arbitrary_axis, kRotationStep);
                break;
            case 'g':
                if (arbitrary_axis == nullptr) return false;
                rotate_about_world_axis(*arbitrary_axis, -kRotationStep);
                break;
            case 'j':
                position.x -= kTranslateStep;
                break;
            case 'l':
                position.x += kTranslateStep;
                break;
            case 'i':
                position.y += kTranslateStep;
                break;
            case 'k':
                position.y -= kTranslateStep;
                break;
            case 'u':
                position.z += kTranslateStep;
                break;
            case 'o':
                position.z -= kTranslateStep;
                break;
            case 'z':
                scale.x = clamp_scale(scale.x - kScaleStep);
                break;
            case 'x':
                scale.x += kScaleStep;
                break;
            case 'c':
                scale.y = clamp_scale(scale.y - kScaleStep);
                break;
            case 'v':
                scale.y += kScaleStep;
                break;
            case 'b':
                scale.z = clamp_scale(scale.z - kScaleStep);
                break;
            case 'n':
                scale.z += kScaleStep;
                break;
            default:
                return false;
        }
        glutPostRedisplay();
        return true;
    }

private:
    static float clamp_scale(float value) {
        return value < kMinScale ? kMinScale : value;
    }

    void rotate_about_local_axis(const Vec3& axis, float degrees) {
        orientation =
            (orientation * Quaternion::from_axis_angle(axis, degrees))
                .normalized();
    }

    void rotate_about_world_axis(const Vec3& axis, float degrees) {
        orientation =
            (Quaternion::from_axis_angle(axis, degrees) * orientation)
                .normalized();
    }
};

static void draw_triangular_prism() {
    const float yb = -1.0f, yt = 1.0f;

    const float ax = 0.000f, az = 1.000f;
    const float bx = -0.866f, bz = -0.500f;
    const float cx = 0.866f, cz = -0.500f;

    glBegin(GL_TRIANGLES);

    glColor3f(0.85f, 0.20f, 0.20f);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(ax, yb, az);
    glVertex3f(cx, yb, cz);
    glVertex3f(bx, yb, bz);

    glColor3f(0.20f, 0.45f, 1.00f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(ax, yt, az);
    glVertex3f(bx, yt, bz);
    glVertex3f(cx, yt, cz);

    glColor3f(0.20f, 0.75f, 0.30f);
    glNormal3f(-0.866f, 0.0f, 0.500f);
    glVertex3f(ax, yb, az);
    glVertex3f(ax, yt, az);
    glVertex3f(bx, yt, bz);
    glVertex3f(ax, yb, az);
    glVertex3f(bx, yt, bz);
    glVertex3f(bx, yb, bz);

    glColor3f(0.90f, 0.80f, 0.10f);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(bx, yb, bz);
    glVertex3f(bx, yt, bz);
    glVertex3f(cx, yt, cz);
    glVertex3f(bx, yb, bz);
    glVertex3f(cx, yt, cz);
    glVertex3f(cx, yb, cz);

    glColor3f(0.70f, 0.20f, 0.90f);
    glNormal3f(0.866f, 0.0f, 0.500f);
    glVertex3f(cx, yb, cz);
    glVertex3f(cx, yt, cz);
    glVertex3f(ax, yt, az);
    glVertex3f(cx, yb, cz);
    glVertex3f(ax, yt, az);
    glVertex3f(ax, yb, az);

    glEnd();
}

struct App final
{
    enum class Mode
    {
        kNormal,
        kPickAxis,
        kAxisLocked,
    };

    OrbitViewController view{};
    ModelTransform model{};
    Mode mode = Mode::kNormal;
    int last_click_x = 0;
    int last_click_y = 0;
    Vec3 selected_point{};

    const Vec3* selected_axis() const {
        return mode == Mode::kAxisLocked ? &selected_point : nullptr;
    }

    static void reshape_cb(int w, int h) {
        if (h == 0) h = 1;

        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        const Matrix4 projection = build_scene_projection_matrix(w, h);
        glMultMatrixf(projection.data());
        glMatrixMode(GL_MODELVIEW);
    }

    static void display_cb() {
        auto* self = static_cast<App*>(glutGetWindowData());

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        const Matrix4 view_matrix = build_scene_view_matrix();
        glMultMatrixf(view_matrix.data());

        self->view.apply_rotation();

        draw_axes();
        if (self->mode == Mode::kAxisLocked) {
            draw_selected_axis(self->selected_point);
        }
        glPushMatrix();
        self->model.apply();
        draw_triangular_prism();
        glPopMatrix();
        draw_controls_overlay(self->mode == Mode::kPickAxis,
                              self->mode == Mode::kAxisLocked,
                              self->last_click_x, self->last_click_y);
        glutSwapBuffers();
    }

    static void mouse_cb(int button, int state, int x, int y) {
        auto* self = static_cast<App*>(glutGetWindowData());
        if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN &&
            self->mode == Mode::kPickAxis) {
            Vec3 world_point{};
            const int width = glutGet(GLUT_WINDOW_WIDTH);
            const int height = glutGet(GLUT_WINDOW_HEIGHT);
            if (unproject_mouse_to_world_plane(x, y, width, height, self->view,
                                               &world_point)) {
                if (length_squared(world_point) <= kEpsilon) {
                    self->mode = Mode::kNormal;
                    glutPostRedisplay();
                    return;
                }
                self->selected_point = world_point;
                self->mode = Mode::kAxisLocked;
                self->last_click_x = x;
                self->last_click_y = y;
                std::printf("Mouse click window coordinate: (%d, %d)\n", x, y);
            }
            glutPostRedisplay();
            return;
        }
        self->view.on_mouse_button(button, state, x, y);
    }

    static void motion_cb(int x, int y) {
        auto* self = static_cast<App*>(glutGetWindowData());
        self->view.on_mouse_drag(x, y);
    }

    static void keyboard_cb(unsigned char key, int, int) {
        auto* self = static_cast<App*>(glutGetWindowData());
        if (key == 'r' || key == 'R') {
            self->view.reset();
            self->model.reset();
            self->mode = Mode::kNormal;
            return;
        }
        if (key == 'm' || key == 'M') {
            self->mode =
                self->mode == Mode::kPickAxis ? Mode::kNormal : Mode::kPickAxis;
            glutPostRedisplay();
            return;
        }

        self->model.on_keyboard(key, self->selected_axis(),
                                self->mode == Mode::kAxisLocked);
    }

    static void special_cb(int key, int, int) {
        auto* self = static_cast<App*>(glutGetWindowData());
        self->view.on_special_key(key);
    }
};

}  // namespace demo

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(500, 500);
    glutInitWindowPosition(600, 80);
    const int win = glutCreateWindow("Triangular Prism");
    if (win <= 0) return 1;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    const float lightPos[] = {2.f, 4.f, 5.f, 1.f};
    const float ambient[] = {0.3f, 0.3f, 0.3f, 1.f};
    const float diffuse[] = {0.8f, 0.8f, 0.8f, 1.f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    glClearColor(0.85f, 0.82f, 0.75f, 1.f);

    static demo::App app{};
    glutSetWindowData(&app);

    glutReshapeFunc(&demo::App::reshape_cb);
    glutDisplayFunc(&demo::App::display_cb);
    glutMouseFunc(&demo::App::mouse_cb);
    glutMotionFunc(&demo::App::motion_cb);
    glutKeyboardFunc(&demo::App::keyboard_cb);
    glutSpecialFunc(&demo::App::special_cb);

    glutMainLoop();
}
