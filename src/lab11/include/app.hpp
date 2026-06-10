#pragma once

#include <filesystem>

#include "animation.hpp"
#include "overlay_renderer.hpp"
#include "scene_renderer.hpp"

namespace demo
{

class App
{
public:
    bool initialize(const std::filesystem::path& mesh_dir,
                    const std::filesystem::path& font_path);
    void register_callbacks();
    void display();
    void reshape(int w, int h);
    void idle();
    void on_key(unsigned char key);

private:
    SceneRenderer scene_renderer_{};
    OverlayRenderer overlay_renderer_{};
    AnimationState animation_{};
};

void initialize_gl_defaults();

}  // namespace demo
