#pragma once
#include <stdint.h>

// Neutral cancels already-held directions. New presses during neutral pass through.
class NeutralDirectionFilter {
public:
    uint32_t update(bool neutral, uint32_t directions) {
        if (!neutral) blocked = 0;
        else if (!was_neutral) blocked = directions & previous;
        blocked &= directions; // A released direction may be pressed again.
        previous = directions;
        was_neutral = neutral;
        return directions & ~blocked;
    }
private:
    uint32_t blocked = 0;
    uint32_t previous = 0;
    bool was_neutral = false;
};
