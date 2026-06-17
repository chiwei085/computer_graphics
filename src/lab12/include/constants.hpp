#pragma once

#include <GL/freeglut.h>

#include <array>

namespace lab12::constants
{

inline constexpr int kWindowWidth = 1080;
inline constexpr int kWindowHeight = 760;
inline constexpr const char kWindowTitle[] = "Robot Shadow";

inline constexpr float kRotationStepDeg = 5.0f;
inline constexpr float kPlaneRotationStepDeg = 4.0f;
inline constexpr float kJointStepDeg = 5.0f;
inline constexpr float kHeadStepDeg = 8.0f;
inline constexpr float kCameraStepDeg = 3.0f;
inline constexpr float kGroundY = -150.0f;
inline constexpr double kPerspectiveFovDeg = 60.0;
inline constexpr double kPerspectiveNear = 20.0;
inline constexpr double kPerspectiveFar = 900.0;

inline constexpr std::array<GLfloat, 4> kAmbientLight = {0.30f, 0.30f, 0.30f,
                                                         1.0f};
inline constexpr int kLightCount = 3;
inline constexpr std::array<GLfloat, 4> kMaterialSpecular = {1.0f, 1.0f, 1.0f,
                                                             1.0f};

inline constexpr int kOverlayFontSizePx = 22;
inline constexpr float kOverlayLeftX = 20.0f;
inline constexpr float kOverlayTopY = 48.0f;
inline constexpr float kOverlayWidth = 820.0f;
inline constexpr float kOverlayHeight = 254.0f;
inline constexpr float kMiniOverlayWidth = 308.0f;
inline constexpr float kMiniOverlayHeight = 54.0f;

}  // namespace lab12::constants
