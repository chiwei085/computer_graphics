#pragma once

#include <GL/freeglut.h>
#include <GL/glu.h>

#include <cstdlib>   // std::abort

namespace demo {

// 對外共用的 shading mode
enum class ShadeModel { Smooth, Flat };
enum class MenuCmd : int { Smooth = 1, Flat = 2 };

// 把 "App*" 醜陋集中在這個 template，一次解決所有 cpp 的 cast
template <class T>
inline T& ctx() {
    return *static_cast<T*>(glutGetWindowData());
}

inline void init_window(int* argc, char** argv,
                        const char* title,
                        int w = 400, int h = 400,
                        int x = 600, int y = 80) {
    glutInit(argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

    glutInitWindowSize(w, h);
    glutInitWindowPosition(x, y);

    const int win = glutCreateWindow(title);
    if (win <= 0) std::abort();
}

inline void init_gl_white_bg() {
    glClearColor(1.f, 1.f, 1.f, 1.f);
}

inline void set_ortho_projection(int w, int h) {
    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-10, 10, -10, 10, -10, 10);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

inline void begin_frame() {
    glClear(GL_COLOR_BUFFER_BIT);
}

inline void set_default_view() {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(0, 0, 10.f, 0, 0, 0, 0, 1, 0);
}

inline void apply_shade_model(ShadeModel shade) {
    glShadeModel(shade == ShadeModel::Smooth ? GL_SMOOTH : GL_FLAT);
}

// 你之前踩到的坑：第一次 display 前 reshape 不一定會先來
inline void force_initial_projection() {
    set_ortho_projection(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
    glutPostRedisplay();
}

inline void install_shade_menu(void (*menu_cb)(int)) {
    glutCreateMenu(menu_cb);
    glutAddMenuEntry("smooth", static_cast<int>(MenuCmd::Smooth));
    glutAddMenuEntry("flat", static_cast<int>(MenuCmd::Flat));
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

}  // namespace demo
