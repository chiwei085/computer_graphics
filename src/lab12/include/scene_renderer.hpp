#pragma once

#include <GL/freeglut.h>

#include <array>

#include "math3d.hpp"

namespace lab12
{

struct PlaneState
{
    float pitch_deg = 0.0f;
    float roll_deg = 0.0f;
};

struct RenderState
{
    float robot_yaw_deg = 0.0f;
    float head_yaw_deg = 0.0f;
    float arm_pose_deg = 0.0f;
    float leg_pose_deg = 0.0f;
    float camera_pitch_deg = 0.0f;
    PlaneState plane_rotation{};
    int active_light = 0;
    bool overlay_expanded = false;
};

struct LightConfig
{
    const char* label = "";
    std::array<GLfloat, 4> position{};
    std::array<GLfloat, 4> diffuse{};
    std::array<GLfloat, 4> specular{};
};

struct BoxConfig
{
    Vec3 size;
    Vec3 color;
};

struct LimbConfig
{
    BoxConfig upper;
    BoxConfig lower;
    BoxConfig end;
    float upper_ratio = 1.0f;
    float lower_ratio = 1.0f;
};

struct RobotConfig
{
    BoxConfig torso;
    BoxConfig head;
    BoxConfig eye;
    LimbConfig arm;
    LimbConfig leg;
    BoxConfig foot;
};

class SceneRenderer
{
public:
    void initialize_gl() const;
    void draw_scene(const RenderState& state) const;
    [[nodiscard]] const char* active_light_label(int active_light) const;

private:
    void configure_frame() const;
    void configure_lighting(const LightConfig& light) const;
    void draw_ground(const PlaneState& plane) const;
    void draw_robot(const RenderState& state, bool shadow) const;
    void draw_box(const Vec3& center, const BoxConfig& box, bool shadow) const;
    void draw_segment(const BoxConfig& box, bool shadow) const;
    void draw_head(const RenderState& state, bool shadow) const;
    void draw_arm(float side, float pose_deg, bool shadow) const;
    void draw_leg(float side, float pose_deg, bool shadow) const;
    void draw_unified_shadow(const RenderState& state,
                             const LightConfig& light) const;
    void draw_shadow_receiver(const PlaneState& plane) const;
    void draw_light_markers(const LightConfig& light) const;
    void apply_plane_rotation(const PlaneState& plane) const;
    void apply_camera(const RenderState& state) const;
};

}  // namespace lab12
