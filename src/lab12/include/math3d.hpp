#pragma once

#include <array>

namespace lab12
{

using Vec3 = std::array<float, 3>;
using Vec4 = std::array<float, 4>;
using Mat4 = std::array<float, 16>;

[[nodiscard]] Vec3 subtract(const Vec3& a, const Vec3& b);
[[nodiscard]] Vec3 add(const Vec3& a, const Vec3& b);
[[nodiscard]] Vec3 cross(const Vec3& a, const Vec3& b);
[[nodiscard]] Vec3 normalize(const Vec3& v);
[[nodiscard]] Vec3 rotate_x(const Vec3& v, float deg);
[[nodiscard]] Vec3 rotate_z(const Vec3& v, float deg);
[[nodiscard]] Vec3 triangle_normal(const Vec3& p1, const Vec3& p2,
                                   const Vec3& p3);
[[nodiscard]] Vec4 plane_equation(const Vec3& p1, const Vec3& p2,
                                  const Vec3& p3);
[[nodiscard]] Mat4 planar_shadow_matrix(const Vec4& plane,
                                        const Vec3& light_position);

}  // namespace lab12
