#include <GL/freeglut.h>
#include <GL/glu.h>

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
    glColor3f(1.0f, 0.0f, 0.0f);  // X axis
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(axis_length, 0.0f, 0.0f);
    glVertex3f(axis_length, 0.0f, 0.0f);
    glVertex3f(axis_length - arrow_size, arrow_size * 0.5f, 0.0f);
    glVertex3f(axis_length, 0.0f, 0.0f);
    glVertex3f(axis_length - arrow_size, -arrow_size * 0.5f, 0.0f);

    glColor3f(0.0f, 1.0f, 0.0f);  // Y axis
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, axis_length, 0.0f);
    glVertex3f(0.0f, axis_length, 0.0f);
    glVertex3f(arrow_size * 0.5f, axis_length - arrow_size, 0.0f);
    glVertex3f(0.0f, axis_length, 0.0f);
    glVertex3f(-arrow_size * 0.5f, axis_length - arrow_size, 0.0f);

    glColor3f(0.0f, 0.0f, 1.0f);  // Z axis
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

// Draws a 2-D HUD in the top-left corner by temporarily replacing the
// projection matrix with a pixel-space orthographic projection (origin at
// bottom-left).  Both matrix stacks and GL enable bits are fully restored
// on exit via Push/PopMatrix and glPushAttrib / glPopAttrib.
static void draw_controls_overlay() {
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);

    glPushAttrib(GL_ENABLE_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, width, 0.0, height);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glColor3f(0.15f, 0.15f, 0.15f);

    draw_bitmap_text(12, height - 24, "View: drag mouse or Arrow keys");
    draw_bitmap_text(12, height - 46, "Rotate model: W/S=X  A/D=Y  Q/E=Z");
    draw_bitmap_text(12, height - 68, "Translate: J/L=X  I/K=Y  U/O=Z");
    draw_bitmap_text(12, height - 90, "Scale: Z/X=X  C/V=Y  B/N=Z");
    draw_bitmap_text(12, height - 112, "R: reset all transforms");

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

    float rotate_x = 0.0f;
    float rotate_y = 0.0f;
    float rotate_z = 0.0f;

    float translate_x = 0.0f;
    float translate_y = 0.0f;
    float translate_z = 0.0f;

    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float scale_z = 1.0f;

    void apply() const {
        // OpenGL multiplies on the right, so vertices are transformed in
        // reverse order: Scale → Rz → Ry → Rx → Translate (standard TRS).
        glTranslatef(translate_x, translate_y, translate_z);
        glRotatef(rotate_x, 1.0f, 0.0f, 0.0f);
        glRotatef(rotate_y, 0.0f, 1.0f, 0.0f);
        glRotatef(rotate_z, 0.0f, 0.0f, 1.0f);
        glScalef(scale_x, scale_y, scale_z);
    }

    void reset() {
        rotate_x = 0.0f;
        rotate_y = 0.0f;
        rotate_z = 0.0f;
        translate_x = 0.0f;
        translate_y = 0.0f;
        translate_z = 0.0f;
        scale_x = 1.0f;
        scale_y = 1.0f;
        scale_z = 1.0f;
        glutPostRedisplay();
    }

    bool on_keyboard(unsigned char key) {
        // Key bindings are grouped by nearby keys:
        // W/S rotate around X, A/D around Y, Q/E around Z.
        // I/K translate along Y, J/L along X, U/O along Z.
        // Z/X scale X, C/V scale Y, B/N scale Z.
        switch (key) {
            case 'w':
            case 'W':
                rotate_x += kRotationStep;
                glutPostRedisplay();
                return true;
            case 's':
            case 'S':
                rotate_x -= kRotationStep;
                glutPostRedisplay();
                return true;
            case 'a':
            case 'A':
                rotate_y -= kRotationStep;
                glutPostRedisplay();
                return true;
            case 'd':
            case 'D':
                rotate_y += kRotationStep;
                glutPostRedisplay();
                return true;
            case 'q':
            case 'Q':
                rotate_z -= kRotationStep;
                glutPostRedisplay();
                return true;
            case 'e':
            case 'E':
                rotate_z += kRotationStep;
                glutPostRedisplay();
                return true;
            case 'j':
            case 'J':
                translate_x -= kTranslateStep;
                glutPostRedisplay();
                return true;
            case 'l':
            case 'L':
                translate_x += kTranslateStep;
                glutPostRedisplay();
                return true;
            case 'i':
            case 'I':
                translate_y += kTranslateStep;
                glutPostRedisplay();
                return true;
            case 'k':
            case 'K':
                translate_y -= kTranslateStep;
                glutPostRedisplay();
                return true;
            case 'u':
            case 'U':
                translate_z += kTranslateStep;
                glutPostRedisplay();
                return true;
            case 'o':
            case 'O':
                translate_z -= kTranslateStep;
                glutPostRedisplay();
                return true;
            case 'z':
            case 'Z':
                scale_x = clamp_scale(scale_x - kScaleStep);
                glutPostRedisplay();
                return true;
            case 'x':
            case 'X':
                scale_x += kScaleStep;
                glutPostRedisplay();
                return true;
            case 'c':
            case 'C':
                scale_y = clamp_scale(scale_y - kScaleStep);
                glutPostRedisplay();
                return true;
            case 'v':
            case 'V':
                scale_y += kScaleStep;
                glutPostRedisplay();
                return true;
            case 'b':
            case 'B':
                scale_z = clamp_scale(scale_z - kScaleStep);
                glutPostRedisplay();
                return true;
            case 'n':
            case 'N':
                scale_z += kScaleStep;
                glutPostRedisplay();
                return true;
            default:
                return false;
        }
    }

private:
    static float clamp_scale(float value) {
        return value < kMinScale ? kMinScale : value;
    }
};

// Equilateral triangular prism centred at origin.
// Bottom at y = -1, top at y = +1, circumradius = 1.
//   A = front        (  0,    y,  1.000)
//   B = back-left    (-0.866, y, -0.500)
//   C = back-right   ( 0.866, y, -0.500)
static void draw_triangular_prism() {
    const float yb = -1.0f, yt = 1.0f;

    const float ax = 0.000f, az = 1.000f;
    const float bx = -0.866f, bz = -0.500f;
    const float cx = 0.866f, cz = -0.500f;

    glBegin(GL_TRIANGLES);

    // ── Bottom face  (outward normal: 0, -1, 0) ──────────────────────────
    glColor3f(0.85f, 0.20f, 0.20f);  // red
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(ax, yb, az);  // A0
    glVertex3f(cx, yb, cz);  // C0  (CCW from below)
    glVertex3f(bx, yb, bz);  // B0

    // ── Top face  (outward normal: 0, +1, 0) ─────────────────────────────
    glColor3f(0.20f, 0.45f, 1.00f);  // blue
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(ax, yt, az);  // A1
    glVertex3f(bx, yt, bz);  // B1  (CCW from above)
    glVertex3f(cx, yt, cz);  // C1

    // ── Side A  A0-A1-B1-B0  (outward normal: -0.866, 0, +0.500) ─────────
    glColor3f(0.20f, 0.75f, 0.30f);  // green
    glNormal3f(-0.866f, 0.0f, 0.500f);
    // Triangle 1: A0, A1, B1
    glVertex3f(ax, yb, az);
    glVertex3f(ax, yt, az);
    glVertex3f(bx, yt, bz);
    // Triangle 2: A0, B1, B0
    glVertex3f(ax, yb, az);
    glVertex3f(bx, yt, bz);
    glVertex3f(bx, yb, bz);

    // ── Side B  B0-B1-C1-C0  (outward normal: 0, 0, -1) ─────────────────
    glColor3f(0.90f, 0.80f, 0.10f);  // yellow
    glNormal3f(0.0f, 0.0f, -1.0f);
    // Triangle 1: B0, B1, C1
    glVertex3f(bx, yb, bz);
    glVertex3f(bx, yt, bz);
    glVertex3f(cx, yt, cz);
    // Triangle 2: B0, C1, C0
    glVertex3f(bx, yb, bz);
    glVertex3f(cx, yt, cz);
    glVertex3f(cx, yb, cz);

    // ── Side C  C0-C1-A1-A0  (outward normal: +0.866, 0, +0.500) ─────────
    glColor3f(0.70f, 0.20f, 0.90f);  // purple
    glNormal3f(0.866f, 0.0f, 0.500f);
    // Triangle 1: C0, C1, A1
    glVertex3f(cx, yb, cz);
    glVertex3f(cx, yt, cz);
    glVertex3f(ax, yt, az);
    // Triangle 2: C0, A1, A0
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
        gluPerspective(45.0, static_cast<double>(w) / h, 0.1, 100.0);
        glMatrixMode(GL_MODELVIEW);
    }

    static void display_cb() {
        auto* self = static_cast<App*>(glutGetWindowData());

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        gluLookAt(0, 2, 5, 0, 0, 0, 0, 1, 0);

        self->view.apply_rotation();

        // Axes are drawn before pushing the model matrix so they always show
        // the world coordinate frame, unaffected by the model transform.
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

    // Static lifetime: GLUT callbacks hold a raw pointer to this object and
    // may fire after the stack frame would otherwise be gone.
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
