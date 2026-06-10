#pragma once

#include <array>
#include <filesystem>
#include <string>

#include "gl_resources.hpp"

namespace demo
{

struct OverlayLine
{
    std::string text;
    std::array<float, 3> color;
};

class OverlayRenderer
{
public:
    bool load_font(const std::filesystem::path& font_path, int size_px);
    void draw() const;

private:
    void draw_circle_marker(float cx, float cy, float radius,
                            const std::array<float, 3>& color) const;
    void draw_text_line(float x, float y, const std::string& text) const;

    FontAtlas font_{};
    std::array<OverlayLine, 3> lines_ = {{
        {"Directional: red, upper-right", {0.92f, 0.16f, 0.16f}},
        {"Point: blue, upper-left, att.", {0.18f, 0.38f, 0.98f}},
        {"Spotlight: green, top-down", {0.18f, 0.82f, 0.24f}},
    }};
};

}  // namespace demo
