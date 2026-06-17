#include "app.hpp"

#include <ft2build.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "constants.hpp"
#include "vertex.hpp"

#include FT_FREETYPE_H

namespace lab12
{
namespace
{

App& current() {
    return *static_cast<App*>(glutGetWindowData());
}

struct MatrixScope
{
    MatrixScope() { glPushMatrix(); }
    ~MatrixScope() { glPopMatrix(); }
    MatrixScope(const MatrixScope&) = delete;
    MatrixScope& operator=(const MatrixScope&) = delete;
};

constexpr RobotConfig kRobot = {
    {{38.0f, 74.0f, 24.0f}, {0.28f, 0.34f, 0.38f}},
    {{32.0f, 30.0f, 28.0f}, {0.58f, 0.62f, 0.56f}},
    {{4.6f, 6.0f, 2.4f}, {0.02f, 0.06f, 0.10f}},
    {{{11.0f, 34.0f, 12.0f}, {0.18f, 0.27f, 0.34f}},
     {{10.0f, 32.0f, 11.0f}, {0.12f, 0.22f, 0.30f}},
     {{12.0f, 16.0f, 12.0f}, {0.08f, 0.16f, 0.23f}},
     1.0f,
     -0.72f},
    {{{12.0f, 40.0f, 13.0f}, {0.24f, 0.31f, 0.36f}},
     {{11.0f, 38.0f, 12.0f}, {0.18f, 0.26f, 0.32f}},
     {{20.0f, 8.0f, 24.0f}, {0.08f, 0.14f, 0.20f}},
     0.82f,
     -0.55f},
    {{20.0f, 8.0f, 24.0f}, {0.08f, 0.14f, 0.20f}},
};

constexpr std::array<LightConfig, constants::kLightCount> kLights = {{
    LightConfig{"Light 1: left-front warm",
                {-70.0f, 180.0f, 80.0f, 1.0f},
                {1.0f, 0.78f, 0.45f, 1.0f},
                {1.0f, 0.88f, 0.58f, 1.0f}},
    LightConfig{"Light 2: right-side cool",
                {135.0f, 125.0f, 25.0f, 1.0f},
                {0.42f, 0.66f, 1.0f, 1.0f},
                {0.70f, 0.84f, 1.0f, 1.0f}},
    LightConfig{"Light 3: rear red",
                {10.0f, 135.0f, -145.0f, 1.0f},
                {1.0f, 0.22f, 0.16f, 1.0f},
                {1.0f, 0.30f, 0.22f, 1.0f}},
}};

constexpr Vec3 kPlaneCenter = {0.0f, constants::kGroundY, 0.0f};
constexpr float kGroundHalfWidth = 420.0f;
constexpr float kGroundHalfDepth = 250.0f;
constexpr float kShadowLift = 0.20f;
constexpr float kTorsoCenterY = 78.0f;
constexpr float kTorsoTopY = kTorsoCenterY + 37.0f;
// foot bottom = hip_y - upper - lower - foot = (78-37) - 40 - 38 - 8 = -45
constexpr float kRobotFootRelY =
    kTorsoCenterY - kRobot.torso.size[1] * 0.5f - kRobot.leg.upper.size[1] -
    kRobot.leg.lower.size[1] - kRobot.leg.end.size[1];

void emit_ground_quad() {
    quads() | face(0.0f, 1.0f, 0.0f)
                  .v(kGroundHalfWidth, 0.0f, -kGroundHalfDepth)
                  .v(-kGroundHalfWidth, 0.0f, -kGroundHalfDepth)
                  .v(-kGroundHalfWidth, 0.0f, kGroundHalfDepth)
                  .v(kGroundHalfWidth, 0.0f, kGroundHalfDepth);
}

void upload_alpha_texture(int w, int h, const void* data) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA,
                 GL_UNSIGNED_BYTE, data);
}

Vec3 transform_plane_point(const Vec3& local, const PlaneState& plane,
                           float lift = 0.0f) {
    Vec3 lifted = local;
    lifted[1] += lift;
    Vec3 p = rotate_z(rotate_x(lifted, plane.pitch_deg), plane.roll_deg);
    return add(kPlaneCenter, p);
}

Vec4 current_plane_equation(const PlaneState& plane) {
    const Vec3 p1 =
        transform_plane_point({-30.0f, 0.0f, -20.0f}, plane, kShadowLift);
    const Vec3 p2 =
        transform_plane_point({-30.0f, 0.0f, 20.0f}, plane, kShadowLift);
    const Vec3 p3 =
        transform_plane_point({40.0f, 0.0f, 20.0f}, plane, kShadowLift);
    return plane_equation(p1, p2, p3);
}

Vec3 light_position3(const LightConfig& light) {
    return {light.position[0], light.position[1], light.position[2]};
}

void set_color(const Vec3& color, bool shadow) {
    if (shadow) {
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    }
    else {
        glColor3f(color[0], color[1], color[2]);
    }
}

float wrap_deg(float value) {
    return std::fmod(value + 360.0f, 360.0f);
}

float clamp_pose(float value) {
    return std::clamp(value, -70.0f, 70.0f);
}

float clamp_camera(float value) {
    return std::clamp(value, -24.0f, 24.0f);
}

}  // namespace

Vec3 subtract(const Vec3& a, const Vec3& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

Vec3 add(const Vec3& a, const Vec3& b) {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}

Vec3 normalize(const Vec3& v) {
    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len == 0.0f) return {0.0f, 1.0f, 0.0f};
    return {v[0] / len, v[1] / len, v[2] / len};
}

Vec3 rotate_x(const Vec3& v, float deg) {
    const float rad = deg * 3.14159265358979323846f / 180.0f;
    const float s = std::sin(rad);
    const float c = std::cos(rad);
    return {v[0], v[1] * c - v[2] * s, v[1] * s + v[2] * c};
}

Vec3 rotate_z(const Vec3& v, float deg) {
    const float rad = deg * 3.14159265358979323846f / 180.0f;
    const float s = std::sin(rad);
    const float c = std::cos(rad);
    return {v[0] * c - v[1] * s, v[0] * s + v[1] * c, v[2]};
}

Vec3 triangle_normal(const Vec3& p1, const Vec3& p2, const Vec3& p3) {
    return normalize(cross(subtract(p1, p2), subtract(p2, p3)));
}

Vec4 plane_equation(const Vec3& p1, const Vec3& p2, const Vec3& p3) {
    const Vec3 normal = normalize(cross(subtract(p3, p1), subtract(p2, p1)));
    return {normal[0], normal[1], normal[2],
            -(normal[0] * p3[0] + normal[1] * p3[1] + normal[2] * p3[2])};
}

Mat4 planar_shadow_matrix(const Vec4& plane, const Vec3& light_position) {
    const float a = plane[0];
    const float b = plane[1];
    const float c = plane[2];
    const float d = plane[3];
    const float dx = -light_position[0];
    const float dy = -light_position[1];
    const float dz = -light_position[2];

    return {b * dy + c * dz,         -a * dy, -a * dz, 0.0f,    -b * dx,
            a * dx + c * dz,         -b * dz, 0.0f,    -c * dx, -c * dy,
            a * dx + b * dy,         0.0f,    -d * dx, -d * dy, -d * dz,
            a * dx + b * dy + c * dz};
}

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
    if (id_ != 0) glDeleteTextures(1, &id_);
    id_ = id;
}

FontAtlas::~FontAtlas() {
    release();
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
        if (FT_Load_Char(face_, c, FT_LOAD_RENDER) != 0) continue;

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
            upload_alpha_texture(glyph.width, glyph.height,
                                 face_->glyph->bitmap.buffer);
            glyph.texture = std::move(texture);
        }
        glyphs_[c - kFirstGlyph] = std::move(glyph);
    }

    return true;
}

const Glyph* FontAtlas::glyph(char ch) const {
    const int idx = static_cast<unsigned char>(ch) - kFirstGlyph;
    if (idx < 0 || idx >= kGlyphCount) return nullptr;
    return &glyphs_[idx];
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

bool OverlayRenderer::load_font(const std::filesystem::path& font_path,
                                int size_px) {
    return font_.load(font_path, size_px);
}

void OverlayRenderer::draw(const RenderState& state,
                           const char* active_light_label) const {
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT |
                 GL_LINE_BIT);
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

    if (!state.overlay_expanded) {
        draw_mini_pane(height);
        draw_keycap(12, 10, 44, 34, "H", false, height);
        draw_label(66, 34, "help", height);
        draw_keycap(140, 10, 44, 34, "1", state.active_light == 0, height);
        draw_keycap(192, 10, 44, 34, "2", state.active_light == 1, height);
        draw_keycap(244, 10, 44, 34, "3", state.active_light == 2, height);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopAttrib();
        return;
    }

    draw_pane(height);
    draw_keycap(28, 28, 48, 34, "1", state.active_light == 0, height);
    draw_keycap(84, 28, 48, 34, "2", state.active_light == 1, height);
    draw_keycap(140, 28, 48, 34, "3", state.active_light == 2, height);
    draw_label(202, 52, "switch light / shadow", height);

    draw_keycap(28, 70, 48, 34, "<-", false, height);
    draw_keycap(84, 70, 48, 34, "->", false, height);
    draw_label(146, 94, "robot yaw", height);
    draw_keycap(430, 70, 48, 34, "up", false, height);
    draw_keycap(486, 70, 48, 34, "dn", false, height);
    draw_label(548, 94, "view pitch", height);

    draw_keycap(28, 112, 48, 34, "A", false, height);
    draw_keycap(84, 112, 48, 34, "D", false, height);
    draw_label(146, 136, "arm pose", height);
    draw_keycap(430, 112, 48, 34, "W", false, height);
    draw_keycap(486, 112, 48, 34, "S", false, height);
    draw_label(548, 136, "leg pose", height);

    draw_keycap(28, 154, 48, 34, "Q", false, height);
    draw_keycap(84, 154, 48, 34, "E", false, height);
    draw_label(146, 178, "head yaw", height);
    draw_keycap(430, 154, 74, 34, "IJKL", false, height);
    draw_label(510, 178, "plane", height);
    draw_keycap(574, 154, 48, 34, "R", false, height);
    draw_label(630, 178, "reset", height);
    draw_keycap(694, 154, 48, 34, "H", true, height);
    draw_label(750, 178, "hide", height);

    draw_label(28, 208, std::string("current: ") + active_light_label, height);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

void OverlayRenderer::draw_pane(int height) const {
    const float x0 = constants::kOverlayLeftX;
    const float y1 = static_cast<float>(height) - constants::kOverlayTopY;
    const float x1 = x0 + constants::kOverlayWidth;
    const float y0 = y1 - constants::kOverlayHeight;

    glColor4f(0.08f, 0.10f, 0.12f, 0.78f);
    glBegin(GL_QUADS);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
    glEnd();

    glColor4f(0.92f, 0.95f, 1.0f, 0.22f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
    glEnd();
}

void OverlayRenderer::draw_mini_pane(int height) const {
    const float x0 = constants::kOverlayLeftX;
    const float y1 = static_cast<float>(height) - constants::kOverlayTopY;
    const float x1 = x0 + constants::kMiniOverlayWidth;
    const float y0 = y1 - constants::kMiniOverlayHeight;

    glColor4f(0.08f, 0.10f, 0.12f, 0.72f);
    glBegin(GL_QUADS);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
    glEnd();

    glColor4f(0.92f, 0.95f, 1.0f, 0.20f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
    glEnd();
}

void OverlayRenderer::draw_keycap(float x, float y, float w, float h,
                                  std::string_view label, bool active,
                                  int height) const {
    const float sx = constants::kOverlayLeftX + x;
    const float sy = static_cast<float>(height) - constants::kOverlayTopY - y;

    if (active) {
        glColor4f(0.94f, 0.94f, 0.90f, 0.92f);
    }
    else {
        glColor4f(0.04f, 0.05f, 0.06f, 0.82f);
    }
    glBegin(GL_QUADS);
    glVertex2f(sx, sy - h);
    glVertex2f(sx + w, sy - h);
    glVertex2f(sx + w, sy);
    glVertex2f(sx, sy);
    glEnd();

    glColor4f(0.92f, 0.92f, 0.88f, 0.65f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(sx, sy - h);
    glVertex2f(sx + w, sy - h);
    glVertex2f(sx + w, sy);
    glVertex2f(sx, sy);
    glEnd();

    const float tx = sx + (w - text_width(label)) * 0.5f;
    const float ty = sy - h * 0.5f - 7.0f;
    draw_text_line(tx, ty, label,
                   active ? std::array<float, 3>{0.08f, 0.09f, 0.10f}
                          : std::array<float, 3>{0.92f, 0.92f, 0.88f});
}

void OverlayRenderer::draw_label(float x, float y, std::string_view text,
                                 int height) const {
    draw_text_line(constants::kOverlayLeftX + x,
                   static_cast<float>(height) - constants::kOverlayTopY - y,
                   text, {0.90f, 0.92f, 0.90f});
}

void OverlayRenderer::draw_text_line(float x, float y, std::string_view text,
                                     const std::array<float, 3>& color) const {
    glEnable(GL_TEXTURE_2D);
    glColor3f(color[0], color[1], color[2]);

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

float OverlayRenderer::text_width(std::string_view text) const {
    float width = 0.0f;
    for (char ch : text) {
        const Glyph* g = font_.glyph(ch);
        if (g) width += static_cast<float>(g->advance >> 6);
    }
    return width;
}

void SceneRenderer::initialize_gl() const {
    glEnable(GL_DEPTH_TEST);
    glFrontFace(GL_CCW);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glMaterialfv(GL_FRONT, GL_SPECULAR, constants::kMaterialSpecular.data());
    glMateriali(GL_FRONT, GL_SHININESS, 96);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.46f, 0.64f, 0.80f, 1.0f);
    glClearStencil(0);
}

const char* SceneRenderer::active_light_label(int active_light) const {
    return kLights[std::clamp(active_light, 0, constants::kLightCount - 1)]
        .label;
}

void SceneRenderer::draw_scene(const RenderState& state) const {
    const LightConfig& light =
        kLights[std::clamp(state.active_light, 0, constants::kLightCount - 1)];

    configure_frame();
    apply_camera(state);
    draw_ground(state.plane_rotation);

    {
        MatrixScope scope;
        glEnable(GL_LIGHTING);
        configure_lighting(light);
        draw_robot(state, false);
    }

    draw_unified_shadow(state, light);
    draw_light_markers(light);
}

void SceneRenderer::configure_frame() const {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, -20.0f, -430.0f);
}

void SceneRenderer::configure_lighting(const LightConfig& light) const {
    glLightfv(GL_LIGHT0, GL_AMBIENT, constants::kAmbientLight.data());
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light.diffuse.data());
    glLightfv(GL_LIGHT0, GL_SPECULAR, light.specular.data());
    glLightfv(GL_LIGHT0, GL_POSITION, light.position.data());
}

void SceneRenderer::draw_ground(const PlaneState& plane) const {
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    MatrixScope scope;
    apply_plane_rotation(plane);

    glColor3f(0.66f, 0.84f, 0.62f);
    emit_ground_quad();
    glPopAttrib();
}

void SceneRenderer::draw_robot(const RenderState& state, bool shadow) const {
    MatrixScope robot;
    glTranslatef(0.0f, constants::kGroundY - kRobotFootRelY, 0.0f);
    glRotatef(state.robot_yaw_deg, 0.0f, 1.0f, 0.0f);

    draw_box({0.0f, kTorsoCenterY, 0.0f}, kRobot.torso, shadow);
    draw_head(state, shadow);
    draw_arm(-1.0f, state.arm_pose_deg, shadow);
    draw_arm(1.0f, state.arm_pose_deg, shadow);
    draw_leg(-1.0f, state.leg_pose_deg, shadow);
    draw_leg(1.0f, state.leg_pose_deg, shadow);
}

void SceneRenderer::draw_box(const Vec3& center, const BoxConfig& box,
                             bool shadow) const {
    MatrixScope scope;
    glTranslatef(center[0], center[1], center[2]);
    glScalef(box.size[0], box.size[1], box.size[2]);
    set_color(box.color, shadow);
    quads() | flat_box(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f);
}

void SceneRenderer::draw_segment(const BoxConfig& box, bool shadow) const {
    MatrixScope scope;
    glTranslatef(0.0f, -box.size[1] * 0.5f, 0.0f);
    glScalef(box.size[0], box.size[1], box.size[2]);
    set_color(box.color, shadow);
    quads() | flat_box(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f);
}

void SceneRenderer::draw_head(const RenderState& state, bool shadow) const {
    MatrixScope scope;
    glTranslatef(0.0f, kTorsoTopY + 4.0f, 0.0f);
    glRotatef(state.head_yaw_deg, 0.0f, 1.0f, 0.0f);
    glTranslatef(0.0f, kRobot.head.size[1] * 0.5f, 0.0f);
    draw_box({0.0f, 0.0f, 0.0f}, kRobot.head, shadow);
    draw_box({-7.0f, 3.0f, kRobot.head.size[2] * 0.52f}, kRobot.eye, shadow);
    draw_box({7.0f, 3.0f, kRobot.head.size[2] * 0.52f}, kRobot.eye, shadow);
}

void SceneRenderer::draw_arm(float side, float pose_deg, bool shadow) const {
    MatrixScope scope;
    glTranslatef(side * 28.0f, kTorsoTopY - 8.0f, 0.0f);
    glRotatef(side * 7.0f, 0.0f, 0.0f, 1.0f);
    glRotatef(pose_deg * kRobot.arm.upper_ratio, 1.0f, 0.0f, 0.0f);
    draw_segment(kRobot.arm.upper, shadow);

    glTranslatef(0.0f, -kRobot.arm.upper.size[1], 0.0f);
    glRotatef(pose_deg * kRobot.arm.lower_ratio - 18.0f, 1.0f, 0.0f, 0.0f);
    draw_segment(kRobot.arm.lower, shadow);

    glTranslatef(0.0f, -kRobot.arm.lower.size[1], 0.0f);
    draw_segment(kRobot.arm.end, shadow);
}

void SceneRenderer::draw_leg(float side, float pose_deg, bool shadow) const {
    MatrixScope scope;
    glTranslatef(side * 11.0f, kTorsoCenterY - kRobot.torso.size[1] * 0.5f,
                 0.0f);
    glRotatef(pose_deg * kRobot.leg.upper_ratio, 1.0f, 0.0f, 0.0f);
    draw_segment(kRobot.leg.upper, shadow);

    glTranslatef(0.0f, -kRobot.leg.upper.size[1], 0.0f);
    glRotatef(pose_deg * kRobot.leg.lower_ratio, 1.0f, 0.0f, 0.0f);
    draw_segment(kRobot.leg.lower, shadow);

    glTranslatef(0.0f, -kRobot.leg.lower.size[1], 4.0f);
    draw_segment(kRobot.leg.end, shadow);
}

void SceneRenderer::draw_unified_shadow(const RenderState& state,
                                        const LightConfig& light) const {
    const Mat4 shadow_matrix = planar_shadow_matrix(
        current_plane_equation(state.plane_rotation), light_position3(light));

    glClear(GL_STENCIL_BUFFER_BIT);
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                 GL_STENCIL_BUFFER_BIT | GL_CURRENT_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_STENCIL_TEST);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glStencilMask(0xFF);

    {
        MatrixScope scope;
        glMultMatrixf(shadow_matrix.data());
        draw_robot(state, true);
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilMask(0x00);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_shadow_receiver(state.plane_rotation);

    glDepthMask(GL_TRUE);
    glPopAttrib();
}

void SceneRenderer::draw_shadow_receiver(const PlaneState& plane) const {
    MatrixScope scope;
    apply_plane_rotation(plane);
    glTranslatef(0.0f, kShadowLift, 0.0f);
    glColor4f(0.0f, 0.0f, 0.0f, 0.36f);
    emit_ground_quad();
}

void SceneRenderer::draw_light_markers(const LightConfig& light) const {
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    MatrixScope scope;
    glTranslatef(light.position[0], light.position[1], light.position[2]);
    glColor3f(light.diffuse[0], light.diffuse[1], light.diffuse[2]);
    glutSolidSphere(6.0, 18, 18);
    glPopAttrib();
}

void SceneRenderer::apply_plane_rotation(const PlaneState& plane) const {
    glTranslatef(kPlaneCenter[0], kPlaneCenter[1], kPlaneCenter[2]);
    glRotatef(plane.roll_deg, 0.0f, 0.0f, 1.0f);
    glRotatef(plane.pitch_deg, 1.0f, 0.0f, 0.0f);
}

void SceneRenderer::apply_camera(const RenderState& state) const {
    glRotatef(state.camera_pitch_deg, 1.0f, 0.0f, 0.0f);
}

bool App::initialize(const std::filesystem::path& font_path) {
    scene_renderer_.initialize_gl();
    return overlay_renderer_.load_font(font_path,
                                       constants::kOverlayFontSizePx);
}

void App::register_callbacks() {
    glutDisplayFunc([] { current().display(); });
    glutReshapeFunc([](int w, int h) { current().reshape(w, h); });
    glutKeyboardFunc(
        [](unsigned char key, int, int) { current().on_key(key); });
    glutSpecialFunc([](int key, int, int) { current().on_special_key(key); });
}

void App::display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    scene_renderer_.draw_scene(state_);
    overlay_renderer_.draw(
        state_, scene_renderer_.active_light_label(state_.active_light));
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

void App::on_key(unsigned char key) {
    if (key == 27) std::exit(0);

    const char lower =
        static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    switch (lower) {
        case '1':
        case '2':
        case '3':
            state_.active_light = lower - '1';
            break;
        case 'h':
            state_.overlay_expanded = !state_.overlay_expanded;
            glutPostRedisplay();
            return;
        case 'q':
            state_.head_yaw_deg -= constants::kHeadStepDeg;
            break;
        case 'e':
            state_.head_yaw_deg += constants::kHeadStepDeg;
            break;
        case 'a':
            state_.arm_pose_deg -= constants::kJointStepDeg;
            break;
        case 'd':
            state_.arm_pose_deg += constants::kJointStepDeg;
            break;
        case 'w':
            state_.leg_pose_deg -= constants::kJointStepDeg;
            break;
        case 's':
            state_.leg_pose_deg += constants::kJointStepDeg;
            break;
        case 'i':
            state_.plane_rotation.pitch_deg -= constants::kPlaneRotationStepDeg;
            break;
        case 'k':
            state_.plane_rotation.pitch_deg += constants::kPlaneRotationStepDeg;
            break;
        case 'j':
            state_.plane_rotation.roll_deg -= constants::kPlaneRotationStepDeg;
            break;
        case 'l':
            state_.plane_rotation.roll_deg += constants::kPlaneRotationStepDeg;
            break;
        case 'r':
            {
                const bool overlay_expanded = state_.overlay_expanded;
                state_ = {};
                state_.overlay_expanded = overlay_expanded;
            }
            glutPostRedisplay();
            return;
        default:
            return;
    }

    state_.head_yaw_deg = wrap_deg(state_.head_yaw_deg);
    state_.arm_pose_deg = clamp_pose(state_.arm_pose_deg);
    state_.leg_pose_deg = clamp_pose(state_.leg_pose_deg);
    state_.plane_rotation.pitch_deg = wrap_deg(state_.plane_rotation.pitch_deg);
    state_.plane_rotation.roll_deg = wrap_deg(state_.plane_rotation.roll_deg);
    glutPostRedisplay();
}

void App::on_special_key(int key) {
    if (key == GLUT_KEY_UP) {
        state_.camera_pitch_deg -= constants::kCameraStepDeg;
    }
    if (key == GLUT_KEY_DOWN) {
        state_.camera_pitch_deg += constants::kCameraStepDeg;
    }
    if (key == GLUT_KEY_LEFT) {
        state_.robot_yaw_deg -= constants::kRotationStepDeg;
    }
    if (key == GLUT_KEY_RIGHT) {
        state_.robot_yaw_deg += constants::kRotationStepDeg;
    }

    state_.camera_pitch_deg = clamp_camera(state_.camera_pitch_deg);
    state_.robot_yaw_deg = wrap_deg(state_.robot_yaw_deg);
    glutPostRedisplay();
}

}  // namespace lab12
