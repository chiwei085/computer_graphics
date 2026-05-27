#pragma once

#include <algorithm>
#include <cstddef>
#include <set>
#include <utility>
#include <vector>

#include "jordan curve.hpp"

namespace demo
{

struct AnimationStep
{
    Cell cell;
    OddEvenQueryResult query;
    bool visual_boundary = false;
};

enum class PlaybackMode
{
    Stopped,
    Playing,
    Reversing
};

struct AnimationPlaybackState
{
    std::size_t revealed_step_count = 0;
    PlaybackMode mode = PlaybackMode::Stopped;
    int playback_delay_ms = 180;
    int playback_generation = 0;
};

using CellSet = std::set<std::pair<int, int>>;

inline void stop_animation(AnimationPlaybackState& state) {
    state.mode = PlaybackMode::Stopped;
    ++state.playback_generation;
}

inline void reset_animation(std::vector<AnimationStep>& steps,
                            AnimationPlaybackState& state) {
    stop_animation(state);
    steps.clear();
    state.revealed_step_count = 0;
}

inline const AnimationStep* current_animation_step(
    const std::vector<AnimationStep>& steps,
    const AnimationPlaybackState& state) {
    if (steps.empty() || state.revealed_step_count == 0) return nullptr;
    const std::size_t index =
        std::min(state.revealed_step_count, steps.size()) - 1;
    return &steps[index];
}

inline bool has_boundary_neighbor(const CellSet& boundary_cells, int x, int y) {
    for (int ny = y - 1; ny <= y + 1; ++ny) {
        for (int nx = x - 1; nx <= x + 1; ++nx) {
            if (nx == x && ny == y) continue;
            if (boundary_cells.count(std::make_pair(nx, ny)) != 0) {
                return true;
            }
        }
    }
    return false;
}

inline std::vector<AnimationStep> build_animation_steps(
    const std::vector<Cell>& vertices, const CellSet& boundary_cells) {
    const std::size_t n = vertices.size();
    if (n < 3 || signed_area2(vertices) == 0) return {};

    const BoundingBox bb = compute_bounding_box(vertices);
    const int scan_x_min = bb.x_min - 1;
    const int scan_x_max = bb.x_max + 1;
    const int scan_y_min = bb.y_min - 1;
    const int scan_y_max = bb.y_max + 1;

    std::vector<AnimationStep> steps;
    steps.reserve(static_cast<std::size_t>(scan_x_max - scan_x_min + 1) *
                  static_cast<std::size_t>(scan_y_max - scan_y_min + 1));

    for (int y = scan_y_max; y >= scan_y_min; --y) {
        for (int x = scan_x_min; x <= scan_x_max; ++x) {
            const OddEvenQueryResult query = query_point_odd_even(
                vertices, static_cast<double>(x), static_cast<double>(y));
            const bool visual_boundary =
                boundary_cells.count(std::make_pair(x, y)) != 0;
            if (!query.inside && !visual_boundary &&
                !has_boundary_neighbor(boundary_cells, x, y)) {
                continue;
            }
            steps.push_back(AnimationStep{Cell{x, y}, query, visual_boundary});
        }
    }

    return steps;
}

}  // namespace demo
