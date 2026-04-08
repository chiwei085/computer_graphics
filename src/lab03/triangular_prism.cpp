#include <GL/freeglut.h>

#include <cctype>

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

static void draw_controls_overlay() {
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);
    constexpr const char* kOverlayLines[] = {
        "View: drag mouse or Arrow keys", "Rotate model: W/S=X  A/D=Y  Q/E=Z",
        "Translate: J/L=X  I/K=Y  U/O=Z", "Scale: Z/X=X  C/V=Y  B/N=Z",
        "R: reset all transforms",
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

    for (int i = 0; i < static_cast<int>(std::size(kOverlayLines)); ++i) {
        draw_bitmap_text(12, height - 24 - i * 22, kOverlayLines[i]);
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

    bool on_keyboard(unsigned char key) {
        key = static_cast<unsigned char>(std::tolower(key));
        switch (key) {
            case 'w':
                rotate_about_local_axis(Vec3{1.0f, 0.0f, 0.0f}, kRotationStep);
                break;
            case 's':
                rotate_about_local_axis(Vec3{1.0f, 0.0f, 0.0f}, -kRotationStep);
                break;
            case 'a':
                rotate_about_local_axis(Vec3{0.0f, 1.0f, 0.0f}, -kRotationStep);
                break;
            case 'd':
                rotate_about_local_axis(Vec3{0.0f, 1.0f, 0.0f}, kRotationStep);
                break;
            case 'q':
                rotate_about_local_axis(Vec3{0.0f, 0.0f, 1.0f}, -kRotationStep);
                break;
            case 'e':
                rotate_about_local_axis(Vec3{0.0f, 0.0f, 1.0f}, kRotationStep);
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
    OrbitViewController view{};
    ModelTransform model{};

    static void reshape_cb(int w, int h) {
        if (h == 0) h = 1;

        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        const Matrix4 projection = Projection::perspective(
            45.0f, static_cast<float>(w) / h, 0.1f, 100.0f);
        glMultMatrixf(projection.data());
        glMatrixMode(GL_MODELVIEW);
    }

    static void display_cb() {
        auto* self = static_cast<App*>(glutGetWindowData());

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        const Matrix4 view_matrix = View::look_at(
            Vec3{0.0f, 2.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f},
            Vec3{0.0f, 1.0f, 0.0f});
        glMultMatrixf(view_matrix.data());

        self->view.apply_rotation();

        draw_axes();
        glPushMatrix();
        self->model.apply();
        draw_triangular_prism();
        glPopMatrix();
        draw_controls_overlay();
        glutSwapBuffers();
    }

    static void mouse_cb(int button, int state, int x, int y) {
        auto* self = static_cast<App*>(glutGetWindowData());
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
            return;
        }

        self->model.on_keyboard(key);
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
