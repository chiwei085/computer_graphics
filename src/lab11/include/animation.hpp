#pragma once

#include <cmath>

#include "constants.hpp"

namespace demo
{

struct AnimationState
{
    float spin_deg = 0.0f;
    float precession_deg = 0.0f;
    float nutation_deg = 0.0f;
    int last_time_ms = 0;

    void update_from_elapsed_ms(int now_ms) {
        if (last_time_ms == 0) {
            last_time_ms = now_ms;
            return;
        }

        const float delta_seconds =
            static_cast<float>(now_ms - last_time_ms) / 1000.0f;
        last_time_ms = now_ms;

        spin_deg = std::fmod(
            spin_deg + constants::kSpinSpeedDegPerSec * delta_seconds, 360.0f);
        precession_deg =
            std::fmod(precession_deg +
                          constants::kPrecessionSpeedDegPerSec * delta_seconds,
                      360.0f);
        const float elapsed_seconds = static_cast<float>(now_ms) / 1000.0f;
        nutation_deg = std::sin(elapsed_seconds * 2.0f * constants::kPi *
                                constants::kNutationFrequencyHz) *
                       constants::kNutationAmplitudeDeg;
    }
};

}  // namespace demo
