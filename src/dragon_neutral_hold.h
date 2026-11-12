#pragma once
#include <stdint.h>

// Dragon's final side input persists until the player changes direction.
// Unlocking inserts three neutral frames before returning to live input.
class DragonNeutralHold {
public:
    void watch(uint32_t held_directions, bool sj, int facing) {
        begin(held_directions, sj, facing);
        active = false;
        running = true;
        release_requested = false;
    }
    void finish(uint32_t held_directions, bool sj) {
        running = false;
        active = !release_requested && held_directions != 0 && sj;
        neutral_frames = release_requested ? 3 : 0;
    }
    void begin(uint32_t held_directions, bool sj, int facing = 1) {
        previous = held_directions;
        running = release_requested = false;
        active = held_directions != 0 && sj;
        neutral_frames = 0;
        direction = facing;
    }
    void update(uint32_t held_directions, bool sj, bool advance = false) {
        if (advance && neutral_frames) --neutral_frames;
        const uint32_t horizontal = held_directions & 12u;
        const bool switched_side = (horizontal == 4u || horizontal == 8u) &&
                                  horizontal != (previous & 12u);
        if ((active || running) && (!sj || held_directions == 0 ||
                       (held_directions & ~previous) != 0 || switched_side)) {
            active = false;
            release_requested = true;
            if (!running) neutral_frames = 3;
        }
        previous = held_directions;
    }
    void clear() { active = running = release_requested = false; neutral_frames = 0; }
    bool holding() const { return active; }
    bool centering() const { return neutral_frames != 0; }
    int facing() const { return direction; }
private:
    uint32_t previous = 0;
    unsigned neutral_frames = 0;
    int direction = 1;
    bool active = false;
    bool running = false, release_requested = false;
};
