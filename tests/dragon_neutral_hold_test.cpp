#include "dragon_neutral_hold.h"
#include <cassert>
int main() {
    constexpr unsigned Down = 2, Left = 4, Right = 8, Up = 1;
    DragonNeutralHold h;
    h.begin(Down | Right, true);
    for (int i = 0; i < 100; ++i) h.update(Down | Right, true);
    assert(h.holding());
    h.update(Right, true); // Releasing just one direction does not unlock.
    assert(h.holding());
    h.update(Down | Right, true); // Repressing a starting direction unlocks.
    assert(!h.holding());
    assert(h.centering());
    for (unsigned frame = 0; frame < 3; ++frame) {
        for (int poll = 0; poll < 20; ++poll) h.update(Down | Right, true);
        assert(h.centering()); // Polls cannot shorten the frame interval.
        h.update(Down | Right, true, true);
    }
    assert(!h.centering());
    h.begin(Down | Right, true);
    assert(h.facing() == 1 && !h.centering());
    h.update(Down | Right | Up, true);
    assert(!h.holding());
    h.begin(Down | Left, true);
    h.update(0, true);
    assert(!h.holding());
    h.begin(Down | Left, true);
    h.update(Down | Left, false);
    assert(!h.holding());
    h.begin(0, true);
    assert(!h.holding());
    h.begin(Down, false);
    assert(!h.holding());
    h.begin(Down | Right | Left, true, -1);
    h.update(Down | Left, true); // Opposite direction becomes exclusive on release.
    assert(!h.holding() && h.centering() && h.facing() == -1);
    h.watch(Down | Right, true, 1);
    h.update(Down | Right | Up, true);
    h.update(Down | Right, true); // A released early tap still reserves recovery.
    for (unsigned i = 0; i < 20; ++i) h.update(Down | Right, true, true);
    assert(!h.holding() && !h.centering());
    h.finish(Down | Right, true);
    assert(!h.holding() && h.centering());
    for (unsigned i = 0; i < 3; ++i) h.update(Down | Right, true, true);
    assert(!h.centering());
    h.watch(Down | Left, true, -1);
    h.finish(Down | Left, true);
    assert(h.holding() && h.facing() == -1 && !h.centering());
    h.begin(Down, true);
    h.clear();
    assert(!h.holding() && !h.centering());
}
