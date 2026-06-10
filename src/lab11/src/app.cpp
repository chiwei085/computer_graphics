#include "app.hpp"

#include <GL/freeglut.h>
#include <ft2build.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "vertex.hpp"

#include FT_FREETYPE_H

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace demo
{

TextureHandle::TextureHandle(GLuint id) : id_(id) {}

TextureHandle::~TextureHandle() {
    reset();
}

TextureHandle::TextureHandle(TextureHandle&& other) noexcept : id_(other.id_) {
    other.id_ = 0;
}

TextureHandle& TextureHandle::operator=(TextureHandle&& other) noexcept {
    if (this != &other) {
        reset();
        id_ = other.id_;
        other.id_ = 0;
    }
    return *this;
}

GLuint TextureHandle::id() const {
    return id_;
}

bool TextureHandle::valid() const {
    return id_ != 0;
}

void TextureHandle::reset(GLuint id) {
    if (id_ != 0) {
        glDeleteTextures(1, &id_);
    }
    id_ = id;
}

FontAtlas::~FontAtlas() {
    release();
}

const Glyph* FontAtlas::glyph(char ch) const {
    const int idx = static_cast<unsigned char>(ch) - kFirstGlyph;
    if (idx < 0 || idx >= kGlyphCount) return nullptr;
    return &glyphs_[idx];
}

static void upload_texture_2d(GLenum wrap, GLenum format, int w, int h,
                              const void* data) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE,
                 data);
}

bool FontAtlas::load(const std::filesystem::path& path, int size_px) {
    release();

    if (FT_Init_FreeType(&library_) != 0) {
        std::cerr << "Failed to initialize FreeType\n";
        return false;
    }
    if (FT_New_Face(library_, path.string().c_str(), 0, &face_) != 0) {
        std::cerr << "Failed to load font: " << path << '\n';
        release();
        return false;
    }
    if (FT_Set_Pixel_Sizes(face_, 0, static_cast<FT_UInt>(size_px)) != 0) {
        std::cerr << "Failed to set font size\n";
        release();
        return false;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (unsigned char c = kFirstGlyph; c < kLastGlyph; ++c) {
        if (FT_Load_Char(face_, c, FT_LOAD_RENDER) != 0) {
            std::cerr << "Failed to rasterize glyph '" << c << "'\n";
            continue;
        }

        Glyph glyph{};
        glyph.bearing_x = face_->glyph->bitmap_left;
        glyph.bearing_y = face_->glyph->bitmap_top;
        glyph.advance = static_cast<unsigned int>(face_->glyph->advance.x);
        glyph.width = static_cast<int>(face_->glyph->bitmap.width);
        glyph.height = static_cast<int>(face_->glyph->bitmap.rows);

        if (glyph.width != 0 && glyph.height != 0) {
            GLuint texture_id = 0;
            glGenTextures(1, &texture_id);
            TextureHandle texture(texture_id);
            glBindTexture(GL_TEXTURE_2D, texture.id());
            upload_texture_2d(GL_CLAMP_TO_EDGE, GL_ALPHA, glyph.width,
                              glyph.height, face_->glyph->bitmap.buffer);
            glyph.texture = std::move(texture);
        }

        glyphs_[c - kFirstGlyph] = std::move(glyph);
    }

    return true;
}

void FontAtlas::release() {
    glyphs_ = {};
    if (face_ != nullptr) {
        FT_Done_Face(face_);
        face_ = nullptr;
    }
    if (library_ != nullptr) {
        FT_Done_FreeType(library_);
        library_ = nullptr;
    }
}

bool load_texture_file(TextureHandle& texture,
                       const std::filesystem::path& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data =
        stbi_load(path.string().c_str(), &width, &height, &channels, 0);
    if (data == nullptr) {
        std::cerr << "Failed to load texture: " << path << '\n';
        return false;
    }

    const GLenum format = channels == 4   ? GL_RGBA
                          : channels == 1 ? GL_LUMINANCE
                                          : GL_RGB;

    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    texture.reset(texture_id);
    glBindTexture(GL_TEXTURE_2D, texture.id());
    upload_texture_2d(GL_REPEAT, format, width, height, data);
    stbi_image_free(data);

    std::cout << "Loaded texture: " << path << " (" << width << 'x' << height
              << ", ch=" << channels << ")\n";
    return true;
}

bool OverlayRenderer::load_font(const std::filesystem::path& font_path,
                                int size_px) {
    return font_.load(font_path, size_px);
}

void OverlayRenderer::draw() const {
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, static_cast<double>(width), 0.0,
               static_cast<double>(height));

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const float marker_x =
        static_cast<float>(width) - constants::kOverlayMarkerOffsetX;
    const float base_y =
        static_cast<float>(height) - constants::kOverlayBaseOffsetY;
    for (int i = 0; i < static_cast<int>(lines_.size()); ++i) {
        const OverlayLine& line = lines_[i];
        const float y =
            base_y - static_cast<float>(i) * constants::kOverlayLineSpacingY;
        draw_circle_marker(marker_x,
                           y - constants::kOverlayMarkerVerticalAdjust,
                           constants::kOverlayMarkerRadius, line.color);
        draw_text_line(marker_x + constants::kOverlayTextOffsetX,
                       y - constants::kOverlayTextOffsetY, line.text);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

void OverlayRenderer::draw_circle_marker(
    float cx, float cy, float radius, const std::array<float, 3>& color) const {
    static const auto unit_circle = []() {
        std::array<std::array<float, 2>, constants::kOverlayCircleSegments + 1>
            pts{};
        for (int i = 0; i <= constants::kOverlayCircleSegments; ++i) {
            const float a =
                static_cast<float>(i) /
                static_cast<float>(constants::kOverlayCircleSegments) * 2.0f *
                constants::kPi;
            pts[i] = {std::cos(a), std::sin(a)};
        }
        return pts;
    }();
    glColor3f(color[0], color[1], color[2]);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= constants::kOverlayCircleSegments; ++i) {
        glVertex2f(cx + unit_circle[i][0] * radius,
                   cy + unit_circle[i][1] * radius);
    }
    glEnd();
}

void OverlayRenderer::draw_text_line(float x, float y,
                                     const std::string& text) const {
    glEnable(GL_TEXTURE_2D);
    glColor3f(0.92f, 0.90f, 0.86f);

    float pen_x = x;
    for (char ch : text) {
        const Glyph* glyph_ptr = font_.glyph(ch);
        if (glyph_ptr == nullptr) continue;

        const Glyph& glyph = *glyph_ptr;
        if (!glyph.texture.valid() || glyph.width == 0 || glyph.height == 0) {
            pen_x += static_cast<float>(glyph.advance >> 6);
            continue;
        }

        const float x_pos = pen_x + static_cast<float>(glyph.bearing_x);
        const float y_pos =
            y - static_cast<float>(glyph.height - glyph.bearing_y);
        const float w = static_cast<float>(glyph.width);
        const float h = static_cast<float>(glyph.height);

        glBindTexture(GL_TEXTURE_2D, glyph.texture.id());
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(x_pos, y_pos);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(x_pos + w, y_pos);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(x_pos + w, y_pos + h);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(x_pos, y_pos + h);
        glEnd();

        pen_x += static_cast<float>(glyph.advance >> 6);
    }
    glDisable(GL_TEXTURE_2D);
}

bool SceneRenderer::load_textures(const std::filesystem::path& mesh_dir) {
    return load_texture_file(textures_.floor, mesh_dir / "floor.tga") &&
           load_texture_file(textures_.frame, mesh_dir / "Block4.tga") &&
           load_texture_file(textures_.panel, mesh_dir / "Block5.tga") &&
           load_texture_file(textures_.stand, mesh_dir / "Block6.tga");
}

void SceneRenderer::draw_scene(const AnimationState& animation) const {
    apply_frame_state();
    bind_and_color(textures_.floor, {0.82f, 0.78f, 0.70f});
    draw_floor();
    draw_room();
    draw_monitor(animation);
}

void SceneRenderer::draw_room() const {
    bind_and_color(textures_.floor, {0.86f, 0.84f, 0.80f});
    draw_box(-4.0f, 0.0f, -4.1f, 4.0f, 5.0f, -4.0f, 4.0f, 2.5f);  // back
    draw_box(-4.1f, 0.0f, -4.0f, -4.0f, 5.0f, 4.0f, 4.0f, 2.5f);  // left
    draw_box(4.0f, 0.0f, -4.0f, 4.1f, 5.0f, 4.0f, 4.0f, 2.5f);    // right
    draw_box(-4.1f, 5.0f, -4.1f, 4.1f, 5.1f, 4.0f, 4.0f, 4.0f);   // ceiling
}

void SceneRenderer::apply_frame_state() const {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(constants::kCameraEyeX, constants::kCameraEyeY,
              constants::kCameraEyeZ, constants::kCameraCenterX,
              constants::kCameraCenterY, constants::kCameraCenterZ,
              constants::kCameraUpX, constants::kCameraUpY,
              constants::kCameraUpZ);
    configure_lights();
    configure_material();
}

void SceneRenderer::configure_material() const {
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, material_.specular.data());
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, material_.shininess);
}

void SceneRenderer::configure_lights() const {
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient_model_.data());
    for (const LightConfig& light : lights_) {
        glLightfv(light.light_id, GL_AMBIENT, light.ambient.data());
        glLightfv(light.light_id, GL_DIFFUSE, light.diffuse.data());
        glLightfv(light.light_id, GL_SPECULAR, light.specular.data());
        glLightfv(light.light_id, GL_POSITION, light.position.data());
        glLightfv(light.light_id, GL_SPOT_DIRECTION,
                  light.spot_direction.data());
        glLightf(light.light_id, GL_SPOT_CUTOFF, light.spot_cutoff);
        glLightf(light.light_id, GL_SPOT_EXPONENT, light.spot_exponent);
        glLightf(light.light_id, GL_CONSTANT_ATTENUATION,
                 light.constant_attenuation);
        glLightf(light.light_id, GL_LINEAR_ATTENUATION,
                 light.linear_attenuation);
        glLightf(light.light_id, GL_QUADRATIC_ATTENUATION,
                 light.quadratic_attenuation);
    }
}

void SceneRenderer::draw_floor() const {
    quads() | face(0, 1, 0)
                  .v(-4, 0, -4, 0, 0)
                  .v(-4, 0, 4, 0, 4)
                  .v(4, 0, 4, 4, 4)
                  .v(4, 0, -4, 4, 0);
}

void SceneRenderer::draw_monitor(const AnimationState& animation) const {
    glPushMatrix();
    glRotatef(animation.precession_deg, 0.0f, 1.0f, 0.0f);
    glRotatef(animation.nutation_deg, 1.0f, 0.0f, 0.0f);
    glRotatef(animation.spin_deg, 0.0f, 1.0f, 0.0f);

    draw_monitor_frame();
    draw_monitor_screen();
    draw_monitor_stand();
    draw_monitor_base();

    glPopMatrix();
}

void SceneRenderer::draw_monitor_frame() const {
    bind_and_color(textures_.frame, {0.54f, 0.54f, 0.56f});
    draw_box_smooth(-1.15f, 1.0f, -0.25f, 1.15f, 2.4f, 0.28f, 1.5f, 1.0f);
}

void SceneRenderer::draw_monitor_screen() const {
    bind_and_color(textures_.panel, {0.22f, 0.24f, 0.28f});
    quads() | face(0, 0, 1)
                  .v(-0.95f, 1.15f, 0.285f, 0, 0)
                  .v(0.95f, 1.15f, 0.285f, 1, 0)
                  .v(0.95f, 2.25f, 0.285f, 1, 1)
                  .v(-0.95f, 2.25f, 0.285f, 0, 1);
}

void SceneRenderer::draw_monitor_stand() const {
    bind_and_color(textures_.stand, {0.66f, 0.66f, 0.68f});
    draw_box_smooth(-0.14f, 0.45f, -0.12f, 0.14f, 1.0f, 0.12f, 1.0f, 1.0f);
}

void SceneRenderer::draw_monitor_base() const {
    bind_and_color(textures_.panel, {0.58f, 0.58f, 0.60f});
    draw_box_smooth(-0.55f, 0.12f, -0.42f, 0.55f, 0.45f, 0.42f, 1.0f, 1.0f);
}

void SceneRenderer::bind_and_color(const TextureHandle& texture,
                                   const std::array<float, 3>& color) const {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture.id());
    glColor3f(color[0], color[1], color[2]);
}

void SceneRenderer::draw_box(float x_min, float y_min, float z_min, float x_max,
                             float y_max, float z_max, float u_repeat,
                             float v_repeat) {
    quads() |
        flat_box(x_min, y_min, z_min, x_max, y_max, z_max, u_repeat, v_repeat);
}

void SceneRenderer::draw_box_smooth(float x_min, float y_min, float z_min,
                                    float x_max, float y_max, float z_max,
                                    float u_repeat, float v_repeat) {
    quads() | smooth_box(x_min, y_min, z_min, x_max, y_max, z_max, u_repeat,
                         v_repeat);
}

namespace
{
App& current() {
    return *static_cast<App*>(glutGetWindowData());
}
}  // namespace

void App::register_callbacks() {
    glutDisplayFunc([] { current().display(); });
    glutReshapeFunc([](int w, int h) { current().reshape(w, h); });
    glutIdleFunc([] { current().idle(); });
    glutKeyboardFunc([](unsigned char k, int, int) { current().on_key(k); });
}

bool App::initialize(const std::filesystem::path& mesh_dir,
                     const std::filesystem::path& font_path) {
    initialize_gl_defaults();
    return scene_renderer_.load_textures(mesh_dir) &&
           overlay_renderer_.load_font(font_path,
                                       constants::kOverlayFontSizePx);
}

void App::display() {
    animation_.update_from_elapsed_ms(glutGet(GLUT_ELAPSED_TIME));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    scene_renderer_.draw_scene(animation_);
    overlay_renderer_.draw();
    glutSwapBuffers();
}

void App::reshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(constants::kPerspectiveFovDeg, static_cast<double>(w) / h,
                   constants::kPerspectiveNear, constants::kPerspectiveFar);
    glMatrixMode(GL_MODELVIEW);
}

void App::idle() {
    glutPostRedisplay();
}

void App::on_key(unsigned char key) {
    if (key == 27 || key == 'q' || key == 'Q') {
        std::exit(0);
    }
}

void initialize_gl_defaults() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_LIGHT2);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.06f, 0.05f, 0.04f, 1.0f);
}

}  // namespace demo
