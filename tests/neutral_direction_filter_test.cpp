#include "neutral_direction_filter.h"
#include <cassert>
int main() {
    constexpr unsigned Up = 1, Down = 2, Left = 4, Right = 8;
    NeutralDirectionFilter f;
    assert(f.update(false, Left) == Left);
    assert(f.update(true, Left) == 0); // Neutral cancels an existing hold.
    assert(f.update(true, Left | Up) == Up); // A later direction wins.
    assert(f.update(true, Left | Up | Right) == (Up | Right));
    assert(f.update(true, Left) == 0); // Releasing later directions returns to neutral.
    assert(f.update(true, 0) == 0);
    assert(f.update(true, Left) == Left); // Release and repress re-enables a direction.
    assert(f.update(false, Left | Down) == (Left | Down));
    assert(f.update(true, Left | Down | Up) == Up); // Same-sample new direction wins.
    assert(f.update(false, Left | Down | Up) == (Left | Down | Up));
    assert(f.update(false, Right) == Right); // Other modes pass through unchanged.
}
