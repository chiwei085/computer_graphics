#include <GL/freeglut.h>

#include <filesystem>

#include "app.hpp"
#include "constants.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(demo::constants::kWindowWidth,
                       demo::constants::kWindowHeight);
    const int win = glutCreateWindow(demo::constants::kWindowTitle);
    if (win <= 0) return 1;

    static demo::App app{};
    glutSetWindowData(&app);

    const fs::path mesh_dir = fs::path("src") / "lab11" / "meshes";
    const fs::path font_path =
        fs::path("src") / "lab11" / "assets" / "NotoSans-Regular.ttf";
    if (!app.initialize(mesh_dir, font_path)) return 1;

    app.register_callbacks();
    glutMainLoop();
    return 0;
}
