#include "macro_player.h"
#include <cassert>
#include <cstdlib>

enum { PIN_RIGHT, PIN_LEFT, PIN_UP, PIN_DOWN, PIN_TILTE, PIN_NA, PIN_CUP,
       PIN_CLEFT, PIN_CRIGHT, PIN_CDOWN, PIN_B, PIN_Y, PIN_A };
static bool pins[13];
static bool gpio_get(int pin) { return pins[pin]; }
static struct {
    unsigned buttons = 0;
    int leftStickXAxis = 128, leftStickYAxis = 128;
    int rightStickXAxis = 128, rightStickYAxis = 128;
} report;
static bool TILTE = false, SJ = false, EightBottuns = false;
static double tilt_abs_value = 64.5;
static int x_i = 0, y_i = 0, macro_mode = 1, dpad_mode = 0;
static MacroPlayer player, neutral_attack, neutral_special, yoga;
#include "macro_definitions.inc"

static unsigned duration(const MacroDefinition& definition) {
    MacroPlayer state;
    state.request(definition);
    unsigned ticks = 0;
    while (state.active()) { state.tick(); assert(++ticks < 100); }
    return ticks;
}
int main() {
    for (auto& pin : pins) pin = true;
    assert(duration(nil1_macro) == 38);
    assert(duration(nil2_macro) == 36);
    assert(duration(dragon_macro) == 5);
    assert(macro_bindings[0].definition == &dragon_macro);
    assert(macro_bindings[1].definition == &nil2_macro);
    assert(macro_bindings[2].definition == &nil1_macro);

    player.request(nil1_macro);
    player.seek(NilJump, 2);
    pins[PIN_RIGHT] = false;
    nil1_jump();
    assert(player.step_index == NilMovement && player.step_frame == 3);
    player.seek(NilFollowup);
    nil1_followup(); // No optional input: release without waiting through recovery.
    assert(player.step_index == NilRelease);

    player.clear(); player.request(nil2_macro);
    player.seek(NilFollowup);
    pins[PIN_NA] = false;
    assert(smash_neutral_pressed());
    assert(!trigger_neutral_attack());
    pins[PIN_Y] = false;
    assert(!trigger_neutral_special());
    pins[PIN_Y] = true;
    assert(!neutral_followup_pressed()); // Modifier alone must not attack.
    pins[PIN_A] = false;
    assert(neutral_followup_pressed());
    nil2_followup();
    assert(player.step_index == NilFollowup);
    assert(report.buttons == ((1 << 1) | (1 << 2)));
    assert(report.leftStickXAxis == 205 && report.leftStickYAxis == 128);
    player.clear();
    dpad_mode = 1;
    pins[PIN_A] = true;
    assert(!smash_neutral_pressed() && trigger_neutral_attack());
    assert(neutral_followup_pressed()); // Other modes retain legacy NA.
    dpad_mode = 0;
    player.request(nil2_macro);
    player.seek(NilFollowup);
    pins[PIN_NA] = true; pins[PIN_CUP] = false;
    nil2_followup();
    assert(report.rightStickXAxis == 128 && report.rightStickYAxis == 0);

    player.clear();
    neutral_attack.request(neutral_attack_macro);
    neutral_attack.tick(); neutral_attack.tick();
    assert(neutral_attack.holding() && !(neutral_attack.policy() & CenterDirections));
    assert(neutral_attack.policy() & BlockCpad);
}
