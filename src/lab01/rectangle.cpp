#include "lab01.hpp"

namespace demo
{

struct App final
{
    ShadeModel shade = ShadeModel::Smooth;

    static void reshape_cb(int w, int h) { demo::set_ortho_projection(w, h); }

    static void display_cb() {
        auto& a = demo::ctx<App>();

        demo::begin_frame();
        demo::set_default_view();
        demo::apply_shade_model(a.shade);

        const float L = -4.f, R = 4.f, B = -4.f, T = 4.f;

        // GL_FLAT uses the last vertex color (cyan) and applies it to the
        // entire primitive
        // GL_SMOOTH interpolates between vertex colors,
        // producing a gradient effect
        glBegin(GL_QUADS);
        glColor3f(0.f, 0.f, 1.f);
        glVertex3f(L, B, 0.f);  // left-bottom:  blue
        glColor3f(0.f, 1.f, 0.f);
        glVertex3f(L, T, 0.f);  // left-top:     green
        glColor3f(1.f, 0.f, 0.f);
        glVertex3f(R, T, 0.f);  // right-top:    red
        glColor3f(0.f, 1.f, 1.f);
        glVertex3f(R, B,
                   0.f);  // right-bottom: cyan  ← flat color (last vertex)
        glEnd();

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
    demo::init_window(&argc, argv, "Rectangle");
    demo::App app{};
    glutSetWindowData(&app);

    demo::init_gl_white_bg();
    demo::install_shade_menu(&demo::App::menu_cb);

    glutReshapeFunc(&demo::App::reshape_cb);
    glutDisplayFunc(&demo::App::display_cb);
    glutIdleFunc(nullptr);

    demo::force_initial_projection();
    glutMainLoop();
}
