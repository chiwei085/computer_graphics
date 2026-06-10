#pragma once

namespace demo::constants
{

inline constexpr float kPi = 3.14159265358979323846f;

inline constexpr float kSpinSpeedDegPerSec = 90.0f;
inline constexpr float kPrecessionSpeedDegPerSec = 24.0f;
inline constexpr float kNutationAmplitudeDeg = 12.0f;
inline constexpr float kNutationFrequencyHz = 0.8f;

inline constexpr int kWindowWidth = 1080;
inline constexpr int kWindowHeight = 760;
inline constexpr const char kWindowTitle[] = "A lit spinning-top screen";

inline constexpr int kOverlayFontSizePx = 24;
inline constexpr float kOverlayMarkerOffsetX = 430.0f;
inline constexpr float kOverlayBaseOffsetY = 36.0f;
inline constexpr float kOverlayLineSpacingY = 34.0f;
inline constexpr float kOverlayMarkerRadius = 8.0f;
inline constexpr float kOverlayMarkerVerticalAdjust = 4.0f;
inline constexpr float kOverlayTextOffsetX = 20.0f;
inline constexpr float kOverlayTextOffsetY = 12.0f;
inline constexpr int kOverlayCircleSegments = 24;

inline constexpr float kCameraEyeX = 4.2f;
inline constexpr float kCameraEyeY = 3.4f;
inline constexpr float kCameraEyeZ = 6.6f;
inline constexpr float kCameraCenterX = 0.0f;
inline constexpr float kCameraCenterY = 1.2f;
inline constexpr float kCameraCenterZ = 0.0f;
inline constexpr float kCameraUpX = 0.0f;
inline constexpr float kCameraUpY = 1.0f;
inline constexpr float kCameraUpZ = 0.0f;

inline constexpr double kPerspectiveFovDeg = 50.0;
inline constexpr double kPerspectiveNear = 0.1;
inline constexpr double kPerspectiveFar = 50.0;

}  // namespace demo::constants
