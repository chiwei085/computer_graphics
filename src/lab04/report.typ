#import "@preview/articulate-coderscompass:0.1.7": articulate-coderscompass, callout

#show: articulate-coderscompass.with(
  title: "Lab 04: Arbitrary Axis Rotation",
  subtitle: "Mouse-Driven Axis Selection and World-Space Quaternion Rotation",
  authors: (
    (name: "Chi-Wei Yeh", email: "yehchiwei.work@gmail.com", affiliation: "Computer Graphics"),
  ),
  abstract: [
    Lab 04 extends the Lab 03 pipeline with arbitrary axis rotation. A three-state machine
    (Normal / PickAxis / AxisLocked) lets the user click a point in the viewport; the coordinate
    is unprojected onto the world-space $z = 0$ plane via the inverse clip matrix, defining a
    rotation axis through the origin. Rotation is accumulated with pre-multiplied quaternions,
    keeping the axis fixed in world space.
  ],
  keywords: (
    "arbitrary axis rotation",
    "quaternion",
    "ray casting",
    "matrix unprojection",
  ),
  date: datetime(year: 2026, month: 4, day: 15),
  reading-time: "5 minutes",
)

= Overview

Lab 04 adds *arbitrary axis rotation* to the Lab 03 MVP pipeline. The user picks a rotation
axis by clicking in the viewport; the click is unprojected to a world-space point that defines
the axis through the origin, and rotation is accumulated via pre-multiplied quaternions.
All Lab 03 controls are preserved.

#figure(
  image("demo.png", width: 100%),
  caption: [Lab 04 — dashed arbitrary axis and rotated triangular prism],
)

= State Machine & Controls

Interaction uses three explicit states so mouse behaviour is unambiguous.

#figure(
  table(
    columns: (auto, auto, auto),
    inset: 0.55em,
    align: center,
    stroke: 0.5pt,
    [*From*], [*Trigger*], [*To*],
    [Normal], [`M`], [PickAxis],
    [PickAxis], [`M`], [Normal],
    [PickAxis], [Left click (success)], [AxisLocked],
    [AxisLocked], [`M`], [PickAxis],
    [Any], [`R`], [Normal],
  ),
  caption: [State transitions],
)

In *AxisLocked*, W/S/A/D/Q/E are suppressed; only T/G, translation, scaling, and camera
orbit remain active.

#figure(
  table(
    columns: (auto, 1fr),
    inset: 0.55em,
    stroke: 0.5pt,
    [*Input*], [*Action*],
    [`M`], [Toggle PickAxis / resume from AxisLocked],
    [Left click (Pick)], [Set axis point → AxisLocked],
    [`T` / `G`], [Rotate ± along arbitrary axis],
    [`W`/`S` `A`/`D` `Q`/`E`], [Rotate local X / Y / Z],
    [`J`/`L` `I`/`K` `U`/`O`], [Translate X / Y / Z],
    [`Z`/`X` `C`/`V` `B`/`N`], [Scale X / Y / Z],
    [Drag / Arrows], [Orbit camera],
    [`R`], [Reset all → Normal],
  ),
  caption: [Key bindings],
)

= Implementation Notes

== Mouse-to-World Unprojection

Window coordinate $(x, y)$ is converted to a world-space point in four steps:

+ NDC: $x_"ndc" = 2x/W - 1$, $quad y_"ndc" = 1 - 2y/H$.
+ Invert $M_"clip" = M_"proj" dot M_"view" dot M_"orbit"$ and unproject near/far clip points.
+ Ray–plane intersection with $z = 0$: $t = -p_"near".z \/ d.z$, $quad p_"world" = p_"near" + t d$.

Projecting onto $z = 0$ keeps the picked point, the visual axis, and the rotation formula
all in the same coordinate frame with no depth buffer read required.

== Quaternion Composition

Two distinct multiplication orders are used:

- *Local axis* (W/S/A/D/Q/E): $q = "norm"(q_"orient" dot Q_"local"))$ — axis follows model pose.
- *World axis* (T/G): $q = "norm"(Q_"axis" dot q_"orient")$ — axis fixed in world space.

The pre-multiply rule for T/G is the central result of this lab.

= Source Code

The uploaded source code is commented. In particular, comments are placed around the key
setting, the three-state interaction flow, the mouse unprojection path, and the distinction
between local-axis and world-axis quaternion composition so the grading focus can be checked
directly in code.

== `view_controller.hpp`

A self-contained math library shared across all labs. It defines `Vec3`, `Vec4`, `Matrix4`
(with `inverted()` via cofactor expansion), `Quaternion` (Hamilton product, `to_matrix()`,
`from_axis_angle()`), and the `Transform`, `View`, and `Projection` namespaces.
`OrbitViewController` accumulates yaw/pitch from mouse drag and arrow keys and applies
the resulting rotation to the OpenGL modelview stack. `Vec4`, `Matrix4 * Vec4`, and
`Matrix4::inverted()` are the three additions over Lab 03, required by the unprojection pipeline.

```cpp
#pragma once

#include <GL/freeglut.h>

#include <array>
#include <cassert>
#include <cmath>

namespace demo
{

static constexpr float kPi = 3.14159265358979323846f;
static constexpr float kEpsilon = 1e-6f;

inline float degrees_to_radians(float degrees) {
    return degrees * kPi / 180.0f;
}

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3 operator+(const Vec3& rhs) const {
        return Vec3{x + rhs.x, y + rhs.y, z + rhs.z};
    }

    Vec3 operator-(const Vec3& rhs) const {
        return Vec3{x - rhs.x, y - rhs.y, z - rhs.z};
    }

    Vec3 operator*(float scalar) const {
        return Vec3{x * scalar, y * scalar, z * scalar};
    }
};

struct Vec4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
}

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float length_squared(const Vec3& v) {
    return dot(v, v);
}

inline Vec3 normalize(const Vec3& v) {
    const float squared_length = length_squared(v);
    assert(squared_length > kEpsilon &&
           "normalize() requires a non-zero vector");
    const float length = std::sqrt(squared_length);
    return v * (1.0f / length);
}

struct Matrix4
{
    std::array<float, 16> value{};

    static Matrix4 identity() {
        Matrix4 matrix{};
        matrix.at(0, 0) = 1.0f;
        matrix.at(1, 1) = 1.0f;
        matrix.at(2, 2) = 1.0f;
        matrix.at(3, 3) = 1.0f;
        return matrix;
    }

    Matrix4 operator*(const Matrix4& rhs) const {
        Matrix4 result{};
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += at(row, k) * rhs.at(k, col);
                }
                result.at(row, col) = sum;
            }
        }
        return result;
    }

    Vec4 operator*(const Vec4& rhs) const {
        return Vec4{
            at(0, 0) * rhs.x + at(0, 1) * rhs.y + at(0, 2) * rhs.z +
                at(0, 3) * rhs.w,
            at(1, 0) * rhs.x + at(1, 1) * rhs.y + at(1, 2) * rhs.z +
                at(1, 3) * rhs.w,
            at(2, 0) * rhs.x + at(2, 1) * rhs.y + at(2, 2) * rhs.z +
                at(2, 3) * rhs.w,
            at(3, 0) * rhs.x + at(3, 1) * rhs.y + at(3, 2) * rhs.z +
                at(3, 3) * rhs.w,
        };
    }

    Matrix4 inverted() const {
        Matrix4 inverse{};
        const float* m = value.data();
        float inv[16];

        inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] -
                 m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
                 m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

        inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] +
                 m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
                 m[12] * m[6] * m[11] + m[12] * m[7] * m[10];

        inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] -
                 m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
                 m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

        inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] +
                  m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
                  m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

        inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] +
                 m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
                 m[13] * m[2] * m[11] + m[13] * m[3] * m[10];

        inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] -
                 m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
                 m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

        inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] +
                 m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
                 m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

        inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] -
                  m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
                  m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

        inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] -
                 m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
                 m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

        inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] +
                 m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
                 m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

        inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] -
                  m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
                  m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

        inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] +
                  m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
                  m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

        inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] +
                 m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
                 m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

        inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] -
                 m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
                 m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

        inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] +
                  m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
                  m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

        inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] -
                  m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
                  m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

        float determinant =
            m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
        assert(std::fabs(determinant) > kEpsilon &&
               "Matrix4::inverted() requires an invertible matrix");

        determinant = 1.0f / determinant;
        for (int i = 0; i < 16; ++i) {
            inverse.value[i] = inv[i] * determinant;
        }
        return inverse;
    }

    const float* data() const { return value.data(); }
    float at(int row, int col) const { return value[col * 4 + row]; }
    float& at(int row, int col) { return value[col * 4 + row]; }
};

struct Quaternion
{
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    static Quaternion identity() { return Quaternion{}; }

    static Quaternion from_axis_angle(const Vec3& axis, float degrees) {
        assert(length_squared(axis) > kEpsilon &&
               "Quaternion::from_axis_angle() requires a non-zero axis");
        const Vec3 unit_axis = normalize(axis);
        const float radians = degrees * kPi / 180.0f;
        const float half_angle = radians * 0.5f;
        const float s = std::sin(half_angle);
        return Quaternion{std::cos(half_angle), unit_axis.x * s,
                          unit_axis.y * s, unit_axis.z * s};
    }

    Quaternion operator*(const Quaternion& rhs) const {
        return Quaternion{
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
        };
    }

    Quaternion normalized() const {
        const float squared_length = w * w + x * x + y * y + z * z;
        assert(squared_length > kEpsilon &&
               "Quaternion::normalized() requires a non-zero quaternion");
        const float length = std::sqrt(squared_length);
        return Quaternion{w / length, x / length, y / length, z / length};
    }

    Quaternion conjugate() const { return Quaternion{w, -x, -y, -z}; }

    Vec3 rotate(const Vec3& v) const {
        const Quaternion qv{0.0f, v.x, v.y, v.z};
        const Quaternion result = (*this) * qv * conjugate();
        return Vec3{result.x, result.y, result.z};
    }

    Matrix4 to_matrix() const {
        // Assumes this quaternion is already normalized by the caller.
        const float xx = x * x;
        const float yy = y * y;
        const float zz = z * z;
        const float xy = x * y;
        const float xz = x * z;
        const float yz = y * z;
        const float wx = w * x;
        const float wy = w * y;
        const float wz = w * z;

        Matrix4 matrix = Matrix4::identity();
        matrix.at(0, 0) = 1.0f - 2.0f * (yy + zz);
        matrix.at(0, 1) = 2.0f * (xy - wz);
        matrix.at(0, 2) = 2.0f * (xz + wy);
        matrix.at(1, 0) = 2.0f * (xy + wz);
        matrix.at(1, 1) = 1.0f - 2.0f * (xx + zz);
        matrix.at(1, 2) = 2.0f * (yz - wx);
        matrix.at(2, 0) = 2.0f * (xz - wy);
        matrix.at(2, 1) = 2.0f * (yz + wx);
        matrix.at(2, 2) = 1.0f - 2.0f * (xx + yy);
        return matrix;
    }
};

namespace Transform
{

inline Matrix4 rotation_x(float degrees) {
    const float radians = degrees_to_radians(degrees);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    Matrix4 matrix = Matrix4::identity();
    matrix.at(1, 1) = c;
    matrix.at(1, 2) = -s;
    matrix.at(2, 1) = s;
    matrix.at(2, 2) = c;
    return matrix;
}

inline Matrix4 rotation_y(float degrees) {
    const float radians = degrees_to_radians(degrees);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    Matrix4 matrix = Matrix4::identity();
    matrix.at(0, 0) = c;
    matrix.at(0, 2) = s;
    matrix.at(2, 0) = -s;
    matrix.at(2, 2) = c;
    return matrix;
}

inline Matrix4 rotation_z(float degrees) {
    const float radians = degrees_to_radians(degrees);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    Matrix4 matrix = Matrix4::identity();
    matrix.at(0, 0) = c;
    matrix.at(0, 1) = -s;
    matrix.at(1, 0) = s;
    matrix.at(1, 1) = c;
    return matrix;
}

inline Matrix4 translation(float tx, float ty, float tz) {
    Matrix4 matrix = Matrix4::identity();
    matrix.at(0, 3) = tx;
    matrix.at(1, 3) = ty;
    matrix.at(2, 3) = tz;
    return matrix;
}

inline Matrix4 translation(const Vec3& offset) {
    return Transform::translation(offset.x, offset.y, offset.z);
}

inline Matrix4 scaling(float sx, float sy, float sz) {
    Matrix4 matrix{};
    matrix.at(0, 0) = sx;
    matrix.at(1, 1) = sy;
    matrix.at(2, 2) = sz;
    matrix.at(3, 3) = 1.0f;
    return matrix;
}

inline Matrix4 scaling(const Vec3& scale) {
    return Transform::scaling(scale.x, scale.y, scale.z);
}

inline Matrix4 trs(const Vec3& position, const Quaternion& rotation,
                   const Vec3& scale) {
    return translation(position) * rotation.to_matrix() * scaling(scale);
}

}  // namespace Transform

namespace View
{

inline Matrix4 look_at(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 view_direction = center - eye;
    assert(length_squared(view_direction) > kEpsilon &&
           "View::look_at() requires eye and center to be different");
    const Vec3 forward = normalize(center - eye);
    const Vec3 side_unnormalized = cross(forward, up);
    assert(
        length_squared(side_unnormalized) > kEpsilon &&
        "View::look_at() requires up to be non-collinear with view direction");
    const Vec3 side = normalize(side_unnormalized);
    const Vec3 camera_up = cross(side, forward);

    Matrix4 matrix = Matrix4::identity();
    matrix.at(0, 0) = side.x;
    matrix.at(0, 1) = side.y;
    matrix.at(0, 2) = side.z;
    matrix.at(0, 3) = -dot(side, eye);
    matrix.at(1, 0) = camera_up.x;
    matrix.at(1, 1) = camera_up.y;
    matrix.at(1, 2) = camera_up.z;
    matrix.at(1, 3) = -dot(camera_up, eye);
    matrix.at(2, 0) = -forward.x;
    matrix.at(2, 1) = -forward.y;
    matrix.at(2, 2) = -forward.z;
    matrix.at(2, 3) = dot(forward, eye);
    return matrix;
}

}  // namespace View

namespace Projection
{

inline Matrix4 orthographic(float left, float right, float bottom, float top,
                            float near_plane, float far_plane) {
    assert(std::fabs(right - left) > kEpsilon &&
           "Projection::orthographic() requires left != right");
    assert(std::fabs(top - bottom) > kEpsilon &&
           "Projection::orthographic() requires bottom != top");
    assert(std::fabs(far_plane - near_plane) > kEpsilon &&
           "Projection::orthographic() requires near != far");
    Matrix4 matrix{};
    matrix.at(0, 0) = 2.0f / (right - left);
    matrix.at(1, 1) = 2.0f / (top - bottom);
    matrix.at(2, 2) = -2.0f / (far_plane - near_plane);
    matrix.at(0, 3) = -(right + left) / (right - left);
    matrix.at(1, 3) = -(top + bottom) / (top - bottom);
    matrix.at(2, 3) = -(far_plane + near_plane) / (far_plane - near_plane);
    matrix.at(3, 3) = 1.0f;
    return matrix;
}

inline Matrix4 perspective(float fov_y_degrees, float aspect, float near_plane,
                           float far_plane) {
    assert(fov_y_degrees > 0.0f && fov_y_degrees < 180.0f &&
           "Projection::perspective() requires 0 < fov_y_degrees < 180");
    assert(aspect > kEpsilon &&
           "Projection::perspective() requires a positive aspect ratio");
    assert(near_plane > 0.0f &&
           "Projection::perspective() requires near_plane > 0");
    assert(far_plane > near_plane &&
           "Projection::perspective() requires far_plane > near_plane");
    const float radians = degrees_to_radians(fov_y_degrees);
    const float f = 1.0f / std::tan(radians * 0.5f);
    Matrix4 matrix{};
    matrix.at(0, 0) = f / aspect;
    matrix.at(1, 1) = f;
    matrix.at(2, 2) = (far_plane + near_plane) / (near_plane - far_plane);
    matrix.at(2, 3) =
        (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    matrix.at(3, 2) = -1.0f;
    return matrix;
}

}  // namespace Projection

// Orbit camera that rotates the view around the world origin.
// Call apply_rotation() each frame before drawing to apply the accumulated
// yaw/pitch. Input is accepted via mouse drag (left button) and arrow keys.
struct OrbitViewController
{
    static constexpr float kInitialYaw = 25.0f;
    static constexpr float kInitialPitch = 0.0f;

    float yaw = kInitialYaw;
    float pitch = kInitialPitch;

    bool dragging = false;
    int last_x = 0;
    int last_y = 0;

    float mouse_sensitivity = 0.4f;
    float key_step = 5.0f;

    void apply_rotation() const {
        // This is an orbit-style post-view rotation: the base camera is first
        // built by View::look_at(), then yaw/pitch rotate the already-built
        // view frame instead of recomputing eye/center from spherical angles.
        const Matrix4 rotation =
            Transform::rotation_x(pitch) * Transform::rotation_y(yaw);
        glMultMatrixf(rotation.data());
    }

    void reset() {
        yaw = kInitialYaw;
        pitch = kInitialPitch;
        dragging = false;
        glutPostRedisplay();
    }

    void on_mouse_button(int button, int state, int x, int y) {
        if (button != GLUT_LEFT_BUTTON) return;

        dragging = (state == GLUT_DOWN);
        last_x = x;
        last_y = y;
    }

    void on_mouse_drag(int x, int y) {
        if (!dragging) return;

        rotate_by((x - last_x) * mouse_sensitivity,
                  (y - last_y) * mouse_sensitivity);
        last_x = x;
        last_y = y;
    }

    void on_special_key(int key) {
        switch (key) {
            case GLUT_KEY_LEFT:
                rotate_by(-key_step, 0.0f);
                break;
            case GLUT_KEY_RIGHT:
                rotate_by(key_step, 0.0f);
                break;
            case GLUT_KEY_UP:
                rotate_by(0.0f, -key_step);
                break;
            case GLUT_KEY_DOWN:
                rotate_by(0.0f, key_step);
                break;
            default:
                break;
        }
    }

private:
    void rotate_by(float delta_yaw, float delta_pitch) {
        yaw += delta_yaw;
        pitch += delta_pitch;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        glutPostRedisplay();
    }
};

}  // namespace demo

```

== `triangular_prism.cpp`

The main application file. It wires all GLUT callbacks through a single `App` struct
stored as GLUT window data, keeping all mutable state in one place.

*`ModelTransform`* owns orientation (quaternion), position, and scale. `on_keyboard()`
dispatches all transform keys and receives a `const Vec3*` for the arbitrary axis —
`nullptr` in Normal/PickAxis, non-null in AxisLocked — so T/G silently no-op when no
axis is selected.

*`App`* owns the view controller, model transform, current `Mode` enum, and
`selected_point`. `selected_axis()` returns `&selected_point` only in AxisLocked,
making axis availability a single-source-of-truth check.

*`mouse_cb`* branches on mode: in PickAxis it calls `unproject_mouse_to_world_plane`,
transitions to AxisLocked on success, and prints the window coordinate to stdout;
otherwise it forwards the event to `OrbitViewController` for camera drag.

*`display_cb`* draws in three layers: (1) world-space geometry (coordinate axes and
the optional dashed axis line) before `model.apply()`; (2) model geometry inside
`glPushMatrix`/`glPopMatrix`; (3) the 2D controls overlay under an orthographic
projection with depth test disabled.

```cpp
#include <GL/freeglut.h>

#include <cctype>
#include <cstdio>

#include "view_controller.hpp"

namespace demo
{

static void draw_axis_label(float x, float y, float z, const char label) {
    glRasterPos3f(x, y, z);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, label);
}

static void draw_axes() {
    constexpr float axis_length = 2.0f;
    constexpr float arrow_size = 0.15f;

    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT);
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);

    glBegin(GL_LINES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(axis_length, 0.0f, 0.0f);
    glVertex3f(axis_length, 0.0f, 0.0f);
    glVertex3f(axis_length - arrow_size, arrow_size * 0.5f, 0.0f);
    glVertex3f(axis_length, 0.0f, 0.0f);
    glVertex3f(axis_length - arrow_size, -arrow_size * 0.5f, 0.0f);

    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, axis_length, 0.0f);
    glVertex3f(0.0f, axis_length, 0.0f);
    glVertex3f(arrow_size * 0.5f, axis_length - arrow_size, 0.0f);
    glVertex3f(0.0f, axis_length, 0.0f);
    glVertex3f(-arrow_size * 0.5f, axis_length - arrow_size, 0.0f);

    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, axis_length);
    glVertex3f(0.0f, 0.0f, axis_length);
    glVertex3f(0.0f, arrow_size * 0.5f, axis_length - arrow_size);
    glVertex3f(0.0f, 0.0f, axis_length);
    glVertex3f(0.0f, -arrow_size * 0.5f, axis_length - arrow_size);
    glEnd();

    glColor3f(1.0f, 0.0f, 0.0f);
    draw_axis_label(axis_length + 0.12f, 0.0f, 0.0f, 'X');
    glColor3f(0.0f, 1.0f, 0.0f);
    draw_axis_label(0.0f, axis_length + 0.12f, 0.0f, 'Y');
    glColor3f(0.0f, 0.0f, 1.0f);
    draw_axis_label(0.0f, 0.0f, axis_length + 0.12f, 'Z');

    glPopAttrib();
}

static void draw_bitmap_text(int x, int y, const char* text) {
    glRasterPos2i(x, y);
    for (const char* p = text; *p != '\0'; ++p) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *p);
    }
}

static Matrix4 build_scene_projection_matrix(int width, int height) {
    return Projection::perspective(45.0f, static_cast<float>(width) / height,
                                   0.1f, 100.0f);
}

static Matrix4 build_scene_view_matrix() {
    return View::look_at(Vec3{0.0f, 2.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f},
                         Vec3{0.0f, 1.0f, 0.0f});
}

static Matrix4 build_scene_orbit_matrix(const OrbitViewController& view) {
    return Transform::rotation_x(view.pitch) * Transform::rotation_y(view.yaw);
}

static Vec3 homogenize_point(const Vec4& value) {
    assert(std::fabs(value.w) > kEpsilon &&
           "homogenize_point() requires a non-zero w");
    return Vec3{value.x / value.w, value.y / value.w, value.z / value.w};
}

static bool unproject_mouse_to_world_plane(
    int mouse_x, int mouse_y, int width, int height,
    const OrbitViewController& view, Vec3* world_point) {
    if (height <= 0) return false;

    const float ndc_x = 2.0f * static_cast<float>(mouse_x) / width - 1.0f;
    const float ndc_y =
        1.0f - 2.0f * static_cast<float>(mouse_y) / static_cast<float>(height);

    const Matrix4 inverse_clip_to_world =
        (build_scene_projection_matrix(width, height) * build_scene_view_matrix() *
         build_scene_orbit_matrix(view))
            .inverted();

    const Vec3 near_point = homogenize_point(
        inverse_clip_to_world * Vec4{ndc_x, ndc_y, -1.0f, 1.0f});
    const Vec3 far_point = homogenize_point(
        inverse_clip_to_world * Vec4{ndc_x, ndc_y, 1.0f, 1.0f});
    const Vec3 ray_direction = far_point - near_point;

    if (std::fabs(ray_direction.z) <= kEpsilon) return false;

    const float t = -near_point.z / ray_direction.z;
    if (t < 0.0f) return false;

    *world_point = near_point + ray_direction * t;
    return true;
}

static void draw_selected_axis(const Vec3& selected_point) {
    const Vec3 ray_direction = normalize(selected_point);
    const float ray_length = 6.0f;
    const Vec3 ray_start = ray_direction * -ray_length;
    const Vec3 ray_end = ray_direction * ray_length;

    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_POINT_BIT);
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(1, 0x00FF);
    glColor3f(0.1f, 0.1f, 0.1f);

    glBegin(GL_LINES);
    glVertex3f(ray_start.x, ray_start.y, ray_start.z);
    glVertex3f(ray_end.x, ray_end.y, ray_end.z);
    glEnd();

    glDisable(GL_LINE_STIPPLE);
    glPointSize(10.0f);
    glColor3f(1.0f, 0.4f, 0.1f);
    glBegin(GL_POINTS);
    glVertex3f(selected_point.x, selected_point.y, selected_point.z);
    glEnd();
    glPopAttrib();
}

static void draw_controls_overlay(bool pick_mode_armed, bool has_selected_axis,
                                  int last_click_x, int last_click_y) {
    const int width = glutGet(GLUT_WINDOW_WIDTH);
    const int height = glutGet(GLUT_WINDOW_HEIGHT);
    char pick_line[96];
    char axis_line[96];
    const char* pick_mode_text = "off";
    if (pick_mode_armed) {
        pick_mode_text = "waiting for click";
    } else if (has_selected_axis) {
        pick_mode_text = "axis locked";
    }
    std::snprintf(pick_line, sizeof(pick_line), "M: pick axis mode [%s]",
                  pick_mode_text);
    std::snprintf(axis_line, sizeof(axis_line), "T/G: arbitrary axis rotate [%s]",
                  has_selected_axis ? "ready" : "need picked point");
    const char* rotate_line = has_selected_axis
                                  ? "Rotate model: T/G=selected axis only"
                                  : "Rotate model: W/S=X  A/D=Y  Q/E=Z";
    constexpr const char* kOverlayLinesPrefix[] = {
        "View: drag mouse or Arrow keys",
    };
    constexpr const char* kOverlayLinesSuffix[] = {
        "Translate: J/L=X  I/K=Y  U/O=Z",
        "Scale: Z/X=X  C/V=Y  B/N=Z",
        "R: reset view/model/picked axis",
    };

    glPushAttrib(GL_ENABLE_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    const Matrix4 overlay_projection =
        Projection::orthographic(0.0f, static_cast<float>(width), 0.0f,
                                 static_cast<float>(height), -1.0f, 1.0f);
    glMultMatrixf(overlay_projection.data());

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glColor3f(0.15f, 0.15f, 0.15f);

    int line_index = 0;
    for (const char* line : kOverlayLinesPrefix) {
        draw_bitmap_text(12, height - 24 - line_index * 22, line);
        ++line_index;
    }
    draw_bitmap_text(12, height - 24 - line_index * 22, rotate_line);
    ++line_index;
    for (const char* line : kOverlayLinesSuffix) {
        draw_bitmap_text(12, height - 24 - line_index * 22, line);
        ++line_index;
    }
    draw_bitmap_text(12, height - 24 - line_index * 22,
                     pick_line);
    ++line_index;
    draw_bitmap_text(12, height - 24 - line_index * 22,
                     axis_line);
    ++line_index;
    if (has_selected_axis) {
        char click_line[96];
        std::snprintf(click_line, sizeof(click_line),
                      "Last click window coordinate: (%d, %d)", last_click_x,
                      last_click_y);
        draw_bitmap_text(12, height - 24 - line_index * 22, click_line);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glPopAttrib();
}

struct ModelTransform
{
    static constexpr float kRotationStep = 5.0f;
    static constexpr float kTranslateStep = 0.1f;
    static constexpr float kScaleStep = 0.1f;
    static constexpr float kMinScale = 0.1f;

    Quaternion orientation = Quaternion::identity();

    Vec3 position{};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    void apply() const {
        const Matrix4 transform = Transform::trs(position, orientation, scale);
        glMultMatrixf(transform.data());
    }

    void reset() {
        orientation = Quaternion::identity();
        position = Vec3{};
        scale = Vec3{1.0f, 1.0f, 1.0f};
        glutPostRedisplay();
    }

    // Key setting:
    // W/S, A/D, Q/E rotate around the object's local X/Y/Z axes.
    // T/G rotate around the arbitrary axis selected by the latest mouse click.
    // J/L, I/K, U/O translate the model. Z/X, C/V, B/N scale the model.
    bool on_keyboard(unsigned char key, const Vec3* arbitrary_axis,
                     bool arbitrary_axis_locked) {
        key = static_cast<unsigned char>(std::tolower(key));
        switch (key) {
            case 'w':
                if (arbitrary_axis_locked) return false;
                rotate_about_local_axis(Vec3{1.0f, 0.0f, 0.0f}, kRotationStep);
                break;
            case 's':
                if (arbitrary_axis_locked) return false;
                rotate_about_local_axis(Vec3{1.0f, 0.0f, 0.0f}, -kRotationStep);
                break;
            case 'a':
                if (arbitrary_axis_locked) return false;
                rotate_about_local_axis(Vec3{0.0f, 1.0f, 0.0f}, -kRotationStep);
                break;
            case 'd':
                if (arbitrary_axis_locked) return false;
                rotate_about_local_axis(Vec3{0.0f, 1.0f, 0.0f}, kRotationStep);
                break;
            case 'q':
                if (arbitrary_axis_locked) return false;
                rotate_about_local_axis(Vec3{0.0f, 0.0f, 1.0f}, -kRotationStep);
                break;
            case 'e':
                if (arbitrary_axis_locked) return false;
                rotate_about_local_axis(Vec3{0.0f, 0.0f, 1.0f}, kRotationStep);
                break;
            case 't':
                if (arbitrary_axis == nullptr) return false;
                rotate_about_world_axis(*arbitrary_axis, kRotationStep);
                break;
            case 'g':
                if (arbitrary_axis == nullptr) return false;
                rotate_about_world_axis(*arbitrary_axis, -kRotationStep);
                break;
            case 'j':
                position.x -= kTranslateStep;
                break;
            case 'l':
                position.x += kTranslateStep;
                break;
            case 'i':
                position.y += kTranslateStep;
                break;
            case 'k':
                position.y -= kTranslateStep;
                break;
            case 'u':
                position.z += kTranslateStep;
                break;
            case 'o':
                position.z -= kTranslateStep;
                break;
            case 'z':
                scale.x = clamp_scale(scale.x - kScaleStep);
                break;
            case 'x':
                scale.x += kScaleStep;
                break;
            case 'c':
                scale.y = clamp_scale(scale.y - kScaleStep);
                break;
            case 'v':
                scale.y += kScaleStep;
                break;
            case 'b':
                scale.z = clamp_scale(scale.z - kScaleStep);
                break;
            case 'n':
                scale.z += kScaleStep;
                break;
            default:
                return false;
        }
        glutPostRedisplay();
        return true;
    }

private:
    static float clamp_scale(float value) {
        return value < kMinScale ? kMinScale : value;
    }

    void rotate_about_local_axis(const Vec3& axis, float degrees) {
        orientation =
            (orientation * Quaternion::from_axis_angle(axis, degrees))
                .normalized();
    }

    void rotate_about_world_axis(const Vec3& axis, float degrees) {
        orientation =
            (Quaternion::from_axis_angle(axis, degrees) * orientation)
                .normalized();
    }
};

static void draw_triangular_prism() {
    const float yb = -1.0f, yt = 1.0f;

    const float ax = 0.000f, az = 1.000f;
    const float bx = -0.866f, bz = -0.500f;
    const float cx = 0.866f, cz = -0.500f;

    glBegin(GL_TRIANGLES);

    glColor3f(0.85f, 0.20f, 0.20f);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(ax, yb, az);
    glVertex3f(cx, yb, cz);
    glVertex3f(bx, yb, bz);

    glColor3f(0.20f, 0.45f, 1.00f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(ax, yt, az);
    glVertex3f(bx, yt, bz);
    glVertex3f(cx, yt, cz);

    glColor3f(0.20f, 0.75f, 0.30f);
    glNormal3f(-0.866f, 0.0f, 0.500f);
    glVertex3f(ax, yb, az);
    glVertex3f(ax, yt, az);
    glVertex3f(bx, yt, bz);
    glVertex3f(ax, yb, az);
    glVertex3f(bx, yt, bz);
    glVertex3f(bx, yb, bz);

    glColor3f(0.90f, 0.80f, 0.10f);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(bx, yb, bz);
    glVertex3f(bx, yt, bz);
    glVertex3f(cx, yt, cz);
    glVertex3f(bx, yb, bz);
    glVertex3f(cx, yt, cz);
    glVertex3f(cx, yb, cz);

    glColor3f(0.70f, 0.20f, 0.90f);
    glNormal3f(0.866f, 0.0f, 0.500f);
    glVertex3f(cx, yb, cz);
    glVertex3f(cx, yt, cz);
    glVertex3f(ax, yt, az);
    glVertex3f(cx, yb, cz);
    glVertex3f(ax, yt, az);
    glVertex3f(ax, yb, az);

    glEnd();
}

struct App final
{
    enum class Mode
    {
        kNormal,
        kPickAxis,
        kAxisLocked,
    };

    OrbitViewController view{};
    ModelTransform model{};
    Mode mode = Mode::kNormal;
    int last_click_x = 0;
    int last_click_y = 0;
    Vec3 selected_point{};

    const Vec3* selected_axis() const {
        return mode == Mode::kAxisLocked ? &selected_point : nullptr;
    }

    static void reshape_cb(int w, int h) {
        if (h == 0) h = 1;

        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        const Matrix4 projection = build_scene_projection_matrix(w, h);
        glMultMatrixf(projection.data());
        glMatrixMode(GL_MODELVIEW);
    }

    static void display_cb() {
        auto* self = static_cast<App*>(glutGetWindowData());

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        const Matrix4 view_matrix = build_scene_view_matrix();
        glMultMatrixf(view_matrix.data());

        self->view.apply_rotation();

        draw_axes();
        if (self->mode == Mode::kAxisLocked) {
            draw_selected_axis(self->selected_point);
        }
        glPushMatrix();
        self->model.apply();
        draw_triangular_prism();
        glPopMatrix();
        draw_controls_overlay(self->mode == Mode::kPickAxis,
                              self->mode == Mode::kAxisLocked,
                              self->last_click_x, self->last_click_y);
        glutSwapBuffers();
    }

    static void mouse_cb(int button, int state, int x, int y) {
        auto* self = static_cast<App*>(glutGetWindowData());
        if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN &&
            self->mode == Mode::kPickAxis) {
            Vec3 world_point{};
            const int width = glutGet(GLUT_WINDOW_WIDTH);
            const int height = glutGet(GLUT_WINDOW_HEIGHT);
            if (unproject_mouse_to_world_plane(x, y, width, height, self->view,
                                               &world_point)) {
                if (length_squared(world_point) <= kEpsilon) {
                    self->mode = Mode::kNormal;
                    glutPostRedisplay();
                    return;
                }
                self->selected_point = world_point;
                self->mode = Mode::kAxisLocked;
                self->last_click_x = x;
                self->last_click_y = y;
                std::printf("Mouse click window coordinate: (%d, %d)\n", x, y);
            }
            glutPostRedisplay();
            return;
        }
        self->view.on_mouse_button(button, state, x, y);
    }

    static void motion_cb(int x, int y) {
        auto* self = static_cast<App*>(glutGetWindowData());
        self->view.on_mouse_drag(x, y);
    }

    static void keyboard_cb(unsigned char key, int, int) {
        auto* self = static_cast<App*>(glutGetWindowData());
        if (key == 'r' || key == 'R') {
            self->view.reset();
            self->model.reset();
            self->mode = Mode::kNormal;
            return;
        }
        if (key == 'm' || key == 'M') {
            self->mode =
                self->mode == Mode::kPickAxis ? Mode::kNormal : Mode::kPickAxis;
            glutPostRedisplay();
            return;
        }

        self->model.on_keyboard(key, self->selected_axis(),
                                self->mode == Mode::kAxisLocked);
    }

    static void special_cb(int key, int, int) {
        auto* self = static_cast<App*>(glutGetWindowData());
        self->view.on_special_key(key);
    }
};

}  // namespace demo

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(500, 500);
    glutInitWindowPosition(600, 80);
    const int win = glutCreateWindow("Triangular Prism");
    if (win <= 0) return 1;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    const float lightPos[] = {2.f, 4.f, 5.f, 1.f};
    const float ambient[] = {0.3f, 0.3f, 0.3f, 1.f};
    const float diffuse[] = {0.8f, 0.8f, 0.8f, 1.f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    glClearColor(0.85f, 0.82f, 0.75f, 1.f);

    static demo::App app{};
    glutSetWindowData(&app);

    glutReshapeFunc(&demo::App::reshape_cb);
    glutDisplayFunc(&demo::App::display_cb);
    glutMouseFunc(&demo::App::mouse_cb);
    glutMotionFunc(&demo::App::motion_cb);
    glutKeyboardFunc(&demo::App::keyboard_cb);
    glutSpecialFunc(&demo::App::special_cb);

    glutMainLoop();
}
```
