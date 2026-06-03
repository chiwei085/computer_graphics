#include <GL/freeglut.h>

#include <cstdio>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace fs = std::filesystem;

namespace demo
{

enum class RenderMode
{
    MeshOnly = 0,
    MeshCull = 1,
    FilledColor = 2,
    FilledTexture = 3,
    FloorTextureOnly = 4,
};

struct TextureSet
{
    GLuint floor = 0;
    GLuint frame = 0;
    GLuint panel = 0;
    GLuint stand = 0;
};

struct App
{
    RenderMode mode = RenderMode::FilledTexture;
    TextureSet textures{};

    static void display_cb();
    static void reshape_cb(int w, int h);
    static void keyboard_cb(unsigned char key, int x, int y);
};

namespace
{

App& app() {
    return *static_cast<App*>(glutGetWindowData());
}

const char* mode_name(RenderMode mode) {
    switch (mode) {
        case RenderMode::MeshOnly:
            return "Mesh Only";
        case RenderMode::MeshCull:
            return "Mesh + Back-face Culling";
        case RenderMode::FilledColor:
            return "Filled Color";
        case RenderMode::FilledTexture:
            return "Filled Texture";
        case RenderMode::FloorTextureOnly:
            return "Textured Floor + Monitor Mesh";
    }
    return "Unknown";
}

bool load_texture(GLuint* texture_id, const fs::path& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data =
        stbi_load(path.string().c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::fprintf(stderr, "Failed to load texture: %s\n", path.c_str());
        return false;
    }

    const GLenum format = channels == 4   ? GL_RGBA
                          : channels == 1 ? GL_LUMINANCE
                                          : GL_RGB;

    glGenTextures(1, texture_id);
    glBindTexture(GL_TEXTURE_2D, *texture_id);
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

bool load_all_textures(App& state) {
    const fs::path mesh_dir = fs::path("src") / "lab10" / "meshes";
    // Load four distinct textures for the floor, frame, panel, and stand.
    return load_texture(&state.textures.floor, mesh_dir / "floor.tga") &&
           load_texture(&state.textures.frame, mesh_dir / "Block4.tga") &&
           load_texture(&state.textures.panel, mesh_dir / "Block5.tga") &&
           load_texture(&state.textures.stand, mesh_dir / "Block6.tga");
}

void draw_bitmap_text(int x, int y, const char* text) {
    glRasterPos2i(x, y);
    for (const char* p = text; *p != '\0'; ++p) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *p);
    }
}

void draw_overlay(RenderMode mode) {
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, static_cast<double>(width), 0.0,
               static_cast<double>(height));

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(0.15f, 0.15f, 0.15f);
    draw_bitmap_text(14, height - 28, "1: Mesh Only");
    draw_bitmap_text(14, height - 52, "2: Mesh + Back-face Culling");
    draw_bitmap_text(14, height - 76, "3: Filled Color");
    draw_bitmap_text(14, height - 100, "4: Filled Texture");
    draw_bitmap_text(14, height - 124, "5: Textured Floor + Monitor Mesh");
    draw_bitmap_text(14, height - 148, "Q or Esc: Quit");

    char mode_line[96];
    std::snprintf(mode_line, sizeof(mode_line), "Current Mode: %s",
                  mode_name(mode));
    draw_bitmap_text(14, height - 180, mode_line);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glPopAttrib();
}

void set_vertex(float x, float y, float z, float u, float v) {
    glTexCoord2f(u, v);
    glVertex3f(x, y, z);
}

void draw_box(float x_min, float y_min, float z_min, float x_max, float y_max,
              float z_max, float u_repeat = 1.0f, float v_repeat = 1.0f) {
    glBegin(GL_QUADS);
    set_vertex(x_min, y_min, z_max, 0.0f, 0.0f);
    set_vertex(x_max, y_min, z_max, u_repeat, 0.0f);
    set_vertex(x_max, y_max, z_max, u_repeat, v_repeat);
    set_vertex(x_min, y_max, z_max, 0.0f, v_repeat);

    set_vertex(x_max, y_min, z_min, 0.0f, 0.0f);
    set_vertex(x_min, y_min, z_min, u_repeat, 0.0f);
    set_vertex(x_min, y_max, z_min, u_repeat, v_repeat);
    set_vertex(x_max, y_max, z_min, 0.0f, v_repeat);

    set_vertex(x_min, y_max, z_max, 0.0f, 0.0f);
    set_vertex(x_max, y_max, z_max, u_repeat, 0.0f);
    set_vertex(x_max, y_max, z_min, u_repeat, v_repeat);
    set_vertex(x_min, y_max, z_min, 0.0f, v_repeat);

    set_vertex(x_min, y_min, z_min, 0.0f, 0.0f);
    set_vertex(x_max, y_min, z_min, u_repeat, 0.0f);
    set_vertex(x_max, y_min, z_max, u_repeat, v_repeat);
    set_vertex(x_min, y_min, z_max, 0.0f, v_repeat);

    set_vertex(x_max, y_min, z_max, 0.0f, 0.0f);
    set_vertex(x_max, y_min, z_min, u_repeat, 0.0f);
    set_vertex(x_max, y_max, z_min, u_repeat, v_repeat);
    set_vertex(x_max, y_max, z_max, 0.0f, v_repeat);

    set_vertex(x_min, y_min, z_min, 0.0f, 0.0f);
    set_vertex(x_min, y_min, z_max, u_repeat, 0.0f);
    set_vertex(x_min, y_max, z_max, u_repeat, v_repeat);
    set_vertex(x_min, y_max, z_min, 0.0f, v_repeat);
    glEnd();
}

void draw_floor() {
    // The floor is a single quad with tiled UVs for repeated texture mapping.
    glBegin(GL_QUADS);
    set_vertex(-4.0f, 0.0f, -4.0f, 0.0f, 0.0f);
    set_vertex(-4.0f, 0.0f, 4.0f, 0.0f, 4.0f);
    set_vertex(4.0f, 0.0f, 4.0f, 4.0f, 4.0f);
    set_vertex(4.0f, 0.0f, -4.0f, 4.0f, 0.0f);
    glEnd();
}

void draw_monitor_panel() {
    // Keep the screen face as a separate quad so its UV mapping is easy to
    // verify.
    glBegin(GL_QUADS);
    set_vertex(-0.95f, 1.15f, 0.285f, 0.0f, 0.0f);
    set_vertex(0.95f, 1.15f, 0.285f, 1.0f, 0.0f);
    set_vertex(0.95f, 2.25f, 0.285f, 1.0f, 1.0f);
    set_vertex(-0.95f, 2.25f, 0.285f, 0.0f, 1.0f);
    glEnd();
}

void draw_monitor(RenderMode mode, const TextureSet& textures) {
    const bool textured = mode == RenderMode::FilledTexture;

    // Build the monitor entirely from hand-written quads without using
    // glutSolidCube(). The frame, neck, and base are boxes with 6 quads each,
    // plus 1 quad for the screen face. This keeps the object itself well above
    // the required minimum of 10 quads.
    if (textured) glBindTexture(GL_TEXTURE_2D, textures.frame);
    glColor3f(0.24f, 0.26f, 0.30f);
    draw_box(-1.15f, 1.0f, -0.25f, 1.15f, 2.4f, 0.28f, 1.5f, 1.0f);

    if (textured) glBindTexture(GL_TEXTURE_2D, textures.panel);
    glColor3f(0.10f, 0.18f, 0.32f);
    draw_monitor_panel();

    if (textured) glBindTexture(GL_TEXTURE_2D, textures.stand);
    glColor3f(0.55f, 0.58f, 0.62f);
    draw_box(-0.14f, 0.45f, -0.12f, 0.14f, 1.0f, 0.12f, 1.0f, 1.0f);

    if (textured) glBindTexture(GL_TEXTURE_2D, textures.panel);
    glColor3f(0.38f, 0.39f, 0.44f);
    draw_box(-0.55f, 0.12f, -0.42f, 0.55f, 0.45f, 0.42f, 1.0f, 1.0f);
}

void configure_render_mode(RenderMode mode) {
    // Modes 1-4 are the original assignment modes; mode 5 mixes a textured
    // floor with a mesh-only monitor.
    const bool wireframe =
        mode == RenderMode::MeshOnly || mode == RenderMode::MeshCull;
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    glDisable(GL_CULL_FACE);
    if (mode == RenderMode::MeshCull) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    if (mode == RenderMode::FilledTexture) {
        glEnable(GL_TEXTURE_2D);
    }
    else {
        glDisable(GL_TEXTURE_2D);
    }
}

}  // namespace

void App::display_cb() {
    App& state = app();
    const RenderMode floor_mode =
        state.mode == RenderMode::FloorTextureOnly ? RenderMode::FilledTexture
                                                   : state.mode;
    const RenderMode monitor_mode =
        state.mode == RenderMode::FloorTextureOnly ? RenderMode::MeshOnly
                                                   : state.mode;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    // Use a fixed front-oblique camera so the floor and standing monitor are
    // visible immediately.
    gluLookAt(3.8, 2.8, 6.2, 0.0, 1.1, 0.0, 0.0, 1.0, 0.0);

    configure_render_mode(floor_mode);

    if (floor_mode == RenderMode::FilledTexture) {
        glBindTexture(GL_TEXTURE_2D, state.textures.floor);
    }
    glColor3f(0.82f, 0.78f, 0.70f);
    draw_floor();

    configure_render_mode(monitor_mode);
    draw_monitor(monitor_mode, state.textures);
    draw_overlay(state.mode);
    glutSwapBuffers();
}

void App::reshape_cb(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(50.0, static_cast<double>(w) / h, 0.1, 50.0);
    glMatrixMode(GL_MODELVIEW);
}

void App::keyboard_cb(unsigned char key, int, int) {
    App& state = app();
    // Use keys 1-5 to switch display modes.
    switch (key) {
        case '1':
            state.mode = RenderMode::MeshOnly;
            break;
        case '2':
            state.mode = RenderMode::MeshCull;
            break;
        case '3':
            state.mode = RenderMode::FilledColor;
            break;
        case '4':
            state.mode = RenderMode::FilledTexture;
            break;
        case '5':
            state.mode = RenderMode::FloorTextureOnly;
            break;
        case 27:
        case 'q':
        case 'Q':
            std::exit(0);
        default:
            return;
    }
    glutPostRedisplay();
}

}  // namespace demo

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(960, 720);
    const int win = glutCreateWindow("lab10 creative object - monitor");
    if (win <= 0) return 1;

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.94f, 0.94f, 0.90f, 1.0f);

    static demo::App app{};
    glutSetWindowData(&app);
    if (!demo::load_all_textures(app)) return 1;

    glutDisplayFunc(&demo::App::display_cb);
    glutReshapeFunc(&demo::App::reshape_cb);
    glutKeyboardFunc(&demo::App::keyboard_cb);
    glutMainLoop();
    return 0;
}
