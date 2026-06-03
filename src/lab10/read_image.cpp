#include <GL/freeglut.h>

#include <cstdio>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace fs = std::filesystem;

namespace demo
{

struct App
{
    GLuint texture_id = 0;
    float rotation = 0.0f;

    static void display_cb();
    static void reshape_cb(int w, int h);
    static void timer_cb(int value);
};

namespace
{

App& app() {
    return *static_cast<App*>(glutGetWindowData());
}

bool load_texture(App& state, const fs::path& path) {
    int width = 0, height = 0, channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data =
        stbi_load(path.string().c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::fprintf(stderr, "Failed to load texture: %s\n", path.c_str());
        return false;
    }

    const GLenum format =
        channels == 4 ? GL_RGBA : channels == 1 ? GL_LUMINANCE : GL_RGB;

    glGenTextures(1, &state.texture_id);
    glBindTexture(GL_TEXTURE_2D, state.texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                 GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    std::printf("Loaded: %s (%dx%d, ch=%d)\n", path.c_str(), width, height,
                channels);
    return true;
}

void draw_cube() {
    glBegin(GL_QUADS);
    // front
    glTexCoord2f(0,0); glVertex3f(-1,-1, 1);
    glTexCoord2f(1,0); glVertex3f( 1,-1, 1);
    glTexCoord2f(1,1); glVertex3f( 1, 1, 1);
    glTexCoord2f(0,1); glVertex3f(-1, 1, 1);
    // back
    glTexCoord2f(1,0); glVertex3f(-1,-1,-1);
    glTexCoord2f(1,1); glVertex3f(-1, 1,-1);
    glTexCoord2f(0,1); glVertex3f( 1, 1,-1);
    glTexCoord2f(0,0); glVertex3f( 1,-1,-1);
    // top
    glTexCoord2f(0,1); glVertex3f(-1, 1,-1);
    glTexCoord2f(0,0); glVertex3f(-1, 1, 1);
    glTexCoord2f(1,0); glVertex3f( 1, 1, 1);
    glTexCoord2f(1,1); glVertex3f( 1, 1,-1);
    // bottom
    glTexCoord2f(1,1); glVertex3f(-1,-1,-1);
    glTexCoord2f(0,1); glVertex3f( 1,-1,-1);
    glTexCoord2f(0,0); glVertex3f( 1,-1, 1);
    glTexCoord2f(1,0); glVertex3f(-1,-1, 1);
    // right
    glTexCoord2f(1,0); glVertex3f( 1,-1,-1);
    glTexCoord2f(1,1); glVertex3f( 1, 1,-1);
    glTexCoord2f(0,1); glVertex3f( 1, 1, 1);
    glTexCoord2f(0,0); glVertex3f( 1,-1, 1);
    // left
    glTexCoord2f(0,0); glVertex3f(-1,-1,-1);
    glTexCoord2f(1,0); glVertex3f(-1,-1, 1);
    glTexCoord2f(1,1); glVertex3f(-1, 1, 1);
    glTexCoord2f(0,1); glVertex3f(-1, 1,-1);
    glEnd();
}

}  // namespace

void App::display_cb() {
    App& state = app();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -5.0f);
    glRotatef(state.rotation, 0.5f, 1.0f, 0.0f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, state.texture_id);
    draw_cube();
    glDisable(GL_TEXTURE_2D);
    glutSwapBuffers();
}

void App::reshape_cb(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, static_cast<double>(w) / h, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void App::timer_cb(int value) {
    App& state = app();
    state.rotation += 1.0f;
    if (state.rotation >= 360.0f) state.rotation -= 360.0f;
    glutPostRedisplay();
    glutTimerFunc(16, &App::timer_cb, value);
}

}  // namespace demo

int main(int argc, char** argv) {
    const fs::path texture = fs::path("src") / "lab10" / "texture.jpg";
    if (!fs::exists(texture)) {
        std::fprintf(stderr, "Texture not found: %s\n", texture.c_str());
        return 1;
    }

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    const int win = glutCreateWindow("lab10 texture cube");
    if (win <= 0) return 1;

    glEnable(GL_DEPTH_TEST);

    static demo::App app{};
    glutSetWindowData(&app);
    if (!demo::load_texture(app, texture)) return 1;

    glutDisplayFunc(&demo::App::display_cb);
    glutReshapeFunc(&demo::App::reshape_cb);
    glutTimerFunc(0, &demo::App::timer_cb, 0);
    glutMainLoop();
    return 0;
}
