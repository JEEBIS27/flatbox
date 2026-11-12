#pragma once
#include <stdint.h>
#include <cmath>
#include "tilt_settings.h"

struct TiltOutput { uint8_t x, y; };
inline TiltOutput apply_tilt_limit(uint8_t x, uint8_t y) {
    double dx = x - 128.0, dy = y - 128.0;
    const double radius = std::hypot(dx, dy);
    const double maximum = 127 * TiltSettings::Magnitude;
    if (radius > maximum) { dx *= maximum / radius; dy *= maximum / radius; }
    dy = std::fmax(-127 * TiltSettings::MaxVertical, std::fmin(dy, 127 * TiltSettings::MaxVertical));
    // Round toward center so quantization cannot exceed either configured bound.
    return {static_cast<uint8_t>(128 + std::trunc(dx)),
            static_cast<uint8_t>(128 + std::trunc(dy))};
}
