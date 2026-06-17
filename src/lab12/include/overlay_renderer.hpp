#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

#include "gl_resources.hpp"
#include "scene_renderer.hpp"

namespace lab12
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
    void draw(const RenderState& state, const char* active_light_label) const;

private:
    void draw_pane(int height) const;
    void draw_mini_pane(int height) const;
    void draw_keycap(float x, float y, float w, float h, std::string_view label,
                     bool active, int height) const;
    void draw_label(float x, float y, std::string_view text, int height) const;
    void draw_text_line(float x, float y, std::string_view text,
                        const std::array<float, 3>& color) const;
    [[nodiscard]] float text_width(std::string_view text) const;

    FontAtlas font_{};
};

}  // namespace lab12
