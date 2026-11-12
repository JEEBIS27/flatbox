#include "macro_player.h"
#include <cassert>

static const MacroStep first_steps[] = {
    { 2_f, Stick::Down, NoButtons, BlockButtons },
    { 3_f, Stick::Side },
};
static const MacroStep second_steps[] = {{ 1_f, Stick::Neutral }};
static const MacroStep held_steps[] = {{ 2_f, Stick::Down }, { 1_f, Stick::Keep }};
static constexpr auto first = define_macro("first", first_steps, BlockCpad);
static constexpr auto second = define_macro("second", second_steps);
static constexpr auto held = define_macro("held", held_steps, PassInput, Finish::HoldUntilRelease);
int main() {
    MacroPlayer p;
    assert(!p.active() && !p.step() && p.policy() == PassInput);
    p.request(first, -1, 7);
    assert(p.step_index == 0 && p.step_frame == 0 && p.direction == -1 && p.context == 7);
    assert(p.policy() == (BlockCpad | BlockButtons));
    p.request(second, 1, 9); // A short input survives until completion, without being held.
    p.tick();
    assert(p.step_index == 0 && p.at_last_frame());
    p.tick();
    assert(p.step_index == 1 && p.step_frame == 0 && p.policy() == BlockCpad);
    p.tick(); p.tick();
    assert(p.is(first)); // Exactly 2 + 3 frames, not inclusive end thresholds.
    p.tick();
    assert(p.is(second) && p.step_frame == 0 && p.direction == 1 && !p.pending && p.context == 9);
    p.tick();
    assert(!p.active());

    p.request(first);
    p.request(second);
    p.request(first, -1); // Latest wins; same-macro chaining is supported.
    for (int i = 0; i < 5; ++i) p.tick();
    assert(p.is(first) && p.step_index == 0 && p.direction == -1 && !p.pending);
    p.seek(1, 2);
    assert(p.at_last_frame());
    p.tick();
    assert(!p.active());

    p.request(held);
    assert(!p.holding());
    p.tick(); p.tick();
    assert(p.holding() && p.step_index == 1);
    for (int i = 0; i < 100; ++i) p.tick();
    assert(p.is(held) && p.step_frame == 0);
    p.clear(); // Trigger-release owner explicitly clears a held helper.
    assert(!p.active());
    p.request(first);
    p.request(second);
    p.clear();
    assert(!p.active() && !p.pending);
    p.request(first);
    p.seek(99); p.seek(1, 99); // Invalid jumps leave the sequence unchanged.
    assert(p.step_index == 0 && p.step_frame == 0);
}
