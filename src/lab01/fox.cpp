#include "lab01.hpp"

namespace demo
{

static void draw_cube(float x, float y, float z, float sx, float sy, float sz,
                      float r, float g, float b) {
    float x0 = x - sx / 2, x1 = x + sx / 2;
    float y0 = y - sy / 2, y1 = y + sy / 2;
    float z0 = z - sz / 2, z1 = z + sz / 2;

    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    // Front
    glNormal3f(0, 0, 1);
    glVertex3f(x0, y0, z1);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z1);
    // Back
    glNormal3f(0, 0, -1);
    glVertex3f(x1, y0, z0);
    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y1, z0);
    glVertex3f(x1, y1, z0);
    // Left
    glNormal3f(-1, 0, 0);
    glVertex3f(x0, y0, z0);
    glVertex3f(x0, y0, z1);
    glVertex3f(x0, y1, z1);
    glVertex3f(x0, y1, z0);
    // Right
    glNormal3f(1, 0, 0);
    glVertex3f(x1, y0, z1);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y1, z0);
    glVertex3f(x1, y1, z1);
    // Top
    glNormal3f(0, 1, 0);
    glVertex3f(x0, y1, z1);
    glVertex3f(x1, y1, z1);
    glVertex3f(x1, y1, z0);
    glVertex3f(x0, y1, z0);
    // Bottom
    glNormal3f(0, -1, 0);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y0, z0);
    glVertex3f(x1, y0, z1);
    glVertex3f(x0, y0, z1);
    glEnd();
}

// Draw only the front face (+Z) of a cube, inset slightly and pushed forward
// to sit on top of the base cube without z-fighting.
static void draw_ear_inner(float x, float y, float z, float sx, float sy,
                           float sz, float r, float g, float b) {
    const float margin = 0.06f;
    float x0 = x - sx / 2 + margin, x1 = x + sx / 2 - margin;
    float y0 = y - sy / 2 + margin, y1 = y + sy / 2 - margin;
    float zf = z + sz / 2 + 0.001f;  // front face + tiny offset

    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(x0, y0, zf);
    glVertex3f(x1, y0, zf);
    glVertex3f(x1, y1, zf);
    glVertex3f(x0, y1, zf);
    glEnd();
}

struct App final
{
    ShadeModel shade = ShadeModel::Smooth;

    static void reshape_cb(int w, int h) {
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, static_cast<double>(w) / h, 0.1, 100.0);
        glMatrixMode(GL_MODELVIEW);
    }

    static void display_cb() {
        auto& a = demo::ctx<App>();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        gluLookAt(0, 1, 4, 0, 0, 0, 0, 1, 0);
        glRotatef(30.0f, 0, 1, 0);  // yaw

        // Smooth: lighting on, interpolated shading
        // Flat: lighting off, each face is pure unlit color
        if (a.shade == ShadeModel::Smooth) {
            glEnable(GL_LIGHTING);
            glShadeModel(GL_SMOOTH);
        }
        else {
            glDisable(GL_LIGHTING);
            glShadeModel(GL_FLAT);
        }

        // Head (fox orange)
        draw_cube(0.00f, 0.00f, 0.00f, 1.6f, 1.4f, 1.4f, 0.85f, 0.42f, 0.05f);
        // Left ear
        draw_cube(-0.55f, 0.95f, 0.00f, 0.5f, 0.5f, 0.3f, 0.85f, 0.42f, 0.05f);
        draw_ear_inner(-0.55f, 0.95f, 0.00f, 0.5f, 0.5f, 0.3f, 1.00f, 0.75f,
                       0.80f);
        // Right ear
        draw_cube(0.55f, 0.95f, 0.00f, 0.5f, 0.5f, 0.3f, 0.85f, 0.42f, 0.05f);
        draw_ear_inner(0.55f, 0.95f, 0.00f, 0.5f, 0.5f, 0.3f, 1.00f, 0.75f,
                       0.80f);
        // Snout / muzzle (cream)
        draw_cube(0.00f, -0.25f, 0.70f, 0.7f, 0.5f, 0.3f, 0.95f, 0.85f, 0.70f);
        // Nose (dark)
        draw_cube(0.00f, -0.10f, 0.85f, 0.25f, 0.2f, 0.1f, 0.10f, 0.08f, 0.08f);
        // Left eye (dark brown)
        draw_cube(-0.35f, 0.20f, 0.71f, 0.2f, 0.2f, 0.05f, 0.12f, 0.08f, 0.05f);
        // Right eye (dark brown)
        draw_cube(0.35f, 0.20f, 0.71f, 0.2f, 0.2f, 0.05f, 0.12f, 0.08f, 0.05f);
        // Neck / chest patch (cream white)
        draw_cube(0.00f, -1.00f, 0.00f, 0.5f, 0.6f, 0.5f, 0.95f, 0.88f, 0.75f);

        glutSwapBuffers();
    }

    static void menu_cb(int index) {
        auto& a = demo::ctx<App>();
        switch (static_cast<MenuCmd>(index)) {
            case MenuCmd::Smooth:
                a.shade = ShadeModel::Smooth;
                break;
            case MenuCmd::Flat:
                a.shade = ShadeModel::Flat;
                break;
            default:
                break;
        }
        glutPostRedisplay();
    }
};

}  // namespace demo

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(500, 500);
    glutInitWindowPosition(600, 80);
    const int win = glutCreateWindow("Fox");
    if (win <= 0) return 1;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    float lightPos[] = {2.f, 3.f, 4.f, 1.f};
    float ambient[] = {0.3f, 0.3f, 0.3f, 1.f};
    float diffuse[] = {0.8f, 0.8f, 0.8f, 1.f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    glClearColor(0.85f, 0.82f, 0.75f, 1.f);

    demo::App app{};
    glutSetWindowData(&app);

    demo::install_shade_menu(&demo::App::menu_cb);
    glutReshapeFunc(&demo::App::reshape_cb);
    glutDisplayFunc(&demo::App::display_cb);
    glutIdleFunc(nullptr);

    glutMainLoop();
}
