#pragma once

#include <array>
#include <filesystem>

#include "animation.hpp"
#include "gl_resources.hpp"

namespace demo
{

struct LightConfig
{
    GLenum light_id = GL_LIGHT0;
    std::array<GLfloat, 4> ambient{};
    std::array<GLfloat, 4> diffuse{};
    std::array<GLfloat, 4> specular{};
    std::array<GLfloat, 4> position{};
    std::array<GLfloat, 3> spot_direction{};
    GLfloat spot_cutoff = 180.0f;
    GLfloat spot_exponent = 0.0f;
    GLfloat constant_attenuation = 1.0f;
    GLfloat linear_attenuation = 0.0f;
    GLfloat quadratic_attenuation = 0.0f;
};

struct MaterialConfig
{
    std::array<GLfloat, 4> specular{};
    GLfloat shininess = 0.0f;
};

class SceneRenderer
{
public:
    bool load_textures(const std::filesystem::path& mesh_dir);
    void draw_scene(const AnimationState& animation) const;

private:
    void apply_frame_state() const;
    void configure_material() const;
    void configure_lights() const;
    void draw_floor() const;
    void draw_room() const;
    void draw_monitor(const AnimationState& animation) const;
    void draw_monitor_frame() const;
    void draw_monitor_screen() const;
    void draw_monitor_stand() const;
    void draw_monitor_base() const;
    void bind_and_color(const TextureHandle& texture,
                        const std::array<float, 3>& color) const;
    static void draw_box(float x_min, float y_min, float z_min, float x_max,
                         float y_max, float z_max, float u_repeat = 1.0f,
                         float v_repeat = 1.0f);
    static void draw_box_smooth(float x_min, float y_min, float z_min,
                                float x_max, float y_max, float z_max,
                                float u_repeat = 1.0f, float v_repeat = 1.0f);

    TextureSet textures_{};
    MaterialConfig material_{{0.40f, 0.40f, 0.40f, 1.0f}, 48.0f};
    std::array<GLfloat, 4> ambient_model_ = {0.25f, 0.22f, 0.18f, 1.0f};
    std::array<LightConfig, 3> lights_ = {{
        {GL_LIGHT0,
         {0.0f, 0.0f, 0.0f, 1.0f},
         {0.95f, 0.12f, 0.12f, 1.0f},
         {1.0f, 0.10f, 0.10f, 1.0f},
         {1.0f, 1.0f, 0.4f, 0.0f},
         {0.0f, 0.0f, -1.0f},
         180.0f,
         0.0f,
         1.0f,
         0.0f,
         0.0f},
        {GL_LIGHT1,
         {0.0f, 0.0f, 0.0f, 1.0f},
         {0.10f, 0.25f, 1.0f, 1.0f},
         {0.05f, 0.15f, 1.0f, 1.0f},
         {-3.5f, 4.0f, 2.5f, 1.0f},
         {0.0f, 0.0f, -1.0f},
         180.0f,
         0.0f,
         1.0f,
         0.04f,
         0.008f},
        {GL_LIGHT2,
         {0.0f, 0.0f, 0.0f, 1.0f},
         {0.10f, 1.0f, 0.15f, 1.0f},
         {0.05f, 1.0f, 0.10f, 1.0f},
         {0.0f, 5.0f, 0.0f, 1.0f},
         {0.0f, -1.0f, 0.0f},
         35.0f,
         5.0f,
         1.0f,
         0.015f,
         0.004f},
    }};
};

}  // namespace demo
