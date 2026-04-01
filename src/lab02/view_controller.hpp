#pragma once

#include <GL/freeglut.h>

namespace demo
{

// Orbit camera that rotates the view around the world origin.
// Call apply_rotation() each frame before drawing to apply the accumulated
// yaw/pitch as OpenGL rotations.  Input is accepted via mouse drag (left
// button) and arrow keys.
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
        // Pitch first, yaw second: OpenGL multiplies on the right, so vertices
        // see M_yaw * M_pitch * v — pitch around world X, then yaw around world
        // Y.
        glRotatef(pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(yaw, 0.0f, 1.0f, 0.0f);
    }

    void reset() {
        yaw = kInitialYaw;
        pitch = kInitialPitch;
        dragging = false;  // prevent a held button from continuing a stale drag
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

        // ±89° (not ±90°) keeps a small gap from vertical to prevent the view
        // from flipping when the camera looks straight up or down.
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        glutPostRedisplay();
    }
};

}  // namespace demo
