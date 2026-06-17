#pragma once

#include <filesystem>

#include "overlay_renderer.hpp"
#include "scene_renderer.hpp"

namespace lab12
{

class App
{
public:
    bool initialize(const std::filesystem::path& font_path);
    void register_callbacks();
    void display();
    void reshape(int w, int h);
    void on_key(unsigned char key);
    void on_special_key(int key);

private:
    SceneRenderer scene_renderer_{};
    OverlayRenderer overlay_renderer_{};
    RenderState state_{};
};

}  // namespace lab12
