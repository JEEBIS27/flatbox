#include "tilt_output.h"
#include <cassert>
int main() {
    auto p = apply_tilt_limit(128, 128);
    assert(p.x == 128 && p.y == 128);
    p = apply_tilt_limit(255, 128);
    assert(p.x == 216 && p.y == 128);
    p = apply_tilt_limit(0, 128);
    assert(p.x == 40 && p.y == 128);
    p = apply_tilt_limit(128, 0);
    assert(p.x == 128 && p.y == 52);
    p = apply_tilt_limit(128, 255);
    assert(p.x == 128 && p.y == 204);
    p = apply_tilt_limit(160, 150);
    assert(p.x == 160 && p.y == 150); // Preserve weaker macro inputs.
    for (int x = 0; x <= 255; ++x) {
        for (int y = 0; y <= 255; ++y) {
            p = apply_tilt_limit(x, y);
            assert(std::hypot(p.x - 128.0, p.y - 128.0) <= 127 * TiltSettings::Magnitude + 0.000001);
            assert(std::abs(p.y - 128) <= 127 * TiltSettings::MaxVertical);
        }
    }
}
