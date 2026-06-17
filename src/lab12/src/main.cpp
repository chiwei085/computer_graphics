#include <GL/freeglut.h>

#include <filesystem>

#include "app.hpp"
#include "constants.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
    glutInitWindowSize(lab12::constants::kWindowWidth,
                       lab12::constants::kWindowHeight);
    const int win = glutCreateWindow(lab12::constants::kWindowTitle);
    if (win <= 0) return 1;

    static lab12::App app{};
    glutSetWindowData(&app);

    const fs::path font_path =
        fs::path("src") / "lab12" / "assets" / "NotoSans-Regular.ttf";
    if (!app.initialize(font_path)) return 1;

    app.register_callbacks();
    glutMainLoop();
    return 0;
}
