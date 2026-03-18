#include <GL/freeglut.h>
#include <GL/glu.h>

namespace demo
{

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
    float yaw = 25.0f;
    float pitch = 0.0f;

    bool dragging = false;
    int last_x = 0;
    int last_y = 0;

    static void reshape_cb(int w, int h) {
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

        glRotatef(self->pitch, 1, 0, 0);
        glRotatef(self->yaw, 0, 1, 0);

        draw_triangular_prism();
        glutSwapBuffers();
    }

    static void mouse_cb(int button, int state, int x, int y) {
        auto* self = static_cast<App*>(glutGetWindowData());
        if (button == GLUT_LEFT_BUTTON) {
            self->dragging = (state == GLUT_DOWN);
            self->last_x = x;
            self->last_y = y;
        }
    }

    static void motion_cb(int x, int y) {
        auto* self = static_cast<App*>(glutGetWindowData());
        if (!self->dragging) return;

        const float sensitivity = 0.4f;
        self->yaw += (x - self->last_x) * sensitivity;
        self->pitch += (y - self->last_y) * sensitivity;

        if (self->pitch > 89.0f) self->pitch = 89.0f;
        if (self->pitch < -89.0f) self->pitch = -89.0f;

        self->last_x = x;
        self->last_y = y;

        glutPostRedisplay();
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

    float lightPos[] = {2.f, 4.f, 5.f, 1.f};
    float ambient[] = {0.3f, 0.3f, 0.3f, 1.f};
    float diffuse[] = {0.8f, 0.8f, 0.8f, 1.f};
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

    glutMainLoop();
}
