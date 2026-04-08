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
