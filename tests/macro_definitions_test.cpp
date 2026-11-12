#include "macro_player.h"
#include "dragon_neutral_hold.h"
#include "input_filter.h"
#include <cassert>
#include <cstdlib>
#include <initializer_list>

enum { PIN_RIGHT, PIN_LEFT, PIN_UP, PIN_DOWN, PIN_TILTE, PIN_NA, PIN_CUP,
       PIN_CLEFT, PIN_CRIGHT, PIN_CDOWN, PIN_B, PIN_Y, PIN_A, PIN_L1, PIN_R1, PIN_X, PIN_L2, PIN_SJ };
static bool pins[18];
static uint32_t read_active_pins_mask() {
    uint32_t held = 0;
    for (unsigned i = 0; i < 18; ++i) if (!pins[i]) held |= 1u << i;
    return held;
}
static bool gpio_get(int pin) { return pins[pin]; }
static struct {
    unsigned buttons = 0;
    int leftStickXAxis = 128, leftStickYAxis = 128;
    int rightStickXAxis = 128, rightStickYAxis = 128;
} report;
static bool SJ = false, EightBottuns = false;
static int x_i = 0, y_i = 0, macro_mode = 1, dpad_mode = 0;
static MacroPlayer player, neutral_attack, neutral_special, yoga, dragon_followup;
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
    assert(direction_keys() == 0);
    assert(!dragon_neutral_hold.holding());
    assert(DragonStartedWithL1 != 0);
    assert(duration(nil1_macro) == 38);
    assert(duration(nil2_macro) == 36);
    assert(duration(dragon_macro) == 9);
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
    update_nil_branch_input();
    pins[PIN_NA] = false;
    update_nil_branch_input(); // Early press, well before the branch step.
    pins[PIN_NA] = true;
    update_nil_branch_input(); // Releasing cannot discard the reservation.
    player.request(dragon_macro); // A next-macro reservation is independent.
    player.seek(NilFollowup);
    nil2_followup();
    assert(player.step_index == NilFollowup);
    assert(report.buttons == (BButton | AButton));
    assert(report.leftStickXAxis == 205 && report.leftStickYAxis == 128);
    assert(player.pending == &dragon_macro);
    pins[PIN_CUP] = false;
    update_nil_branch_input(); // Branch already selected: cannot change halfway through.
    report.buttons = 0;
    nil2_followup();
    assert(report.buttons == (BButton | AButton));
    for (unsigned i = 0; player.is(nil2_macro); ++i) {
        assert(i < 40);
        player.tick();
    }
    assert(player.is(dragon_macro));
    assert(player.select_branch() == NoFollowup); // Branch never follows the next-macro slot.

    player.clear(); player.request(nil1_macro);
    pins[PIN_CUP] = true;
    update_nil_branch_input();
    pins[PIN_NA] = false;
    update_nil_branch_input();
    pins[PIN_NA] = true;
    update_nil_branch_input();
    pins[PIN_CUP] = false;
    update_nil_branch_input(); // Later C-up replaces the earlier NA reservation.
    pins[PIN_CUP] = true;
    update_nil_branch_input();
    player.seek(NilFollowup);
    report.buttons = 0;
    nil1_followup();
    assert(report.buttons == 0);
    assert(report.rightStickXAxis == 128 && report.rightStickYAxis == 0);

    player.clear(); player.request(nil2_macro);
    update_nil_branch_input();
    player.seek(NilFollowup);
    nil2_followup(); // The previous run's branch must not leak into a new run.
    assert(player.step_index == NilRelease);

    player.clear();
    pins[PIN_NA] = false;
    assert(trigger_neutral_attack()); // Smash NA restored.
    report.buttons = 0;
    neutral_attack_output();
    assert(report.buttons == AButton);
    pins[PIN_Y] = false;
    assert(trigger_neutral_special()); // Smash NB restored.
    report.buttons = 0;
    neutral_special_output();
    assert(report.buttons == YButton);
    pins[PIN_NA] = pins[PIN_Y] = true;
    // Consumed L1 stays suppressed for the entire hold, even beyond Dragon completion.
    pins[PIN_L1] = false;
    assert(normal_l1_allowed());
    player.request(dragon_macro);
    consume_dragon_l1();
    assert(!normal_l1_allowed());
    for (int i = 0; i < 9; ++i) {
        update_consumed_inputs();
        player.tick();
        assert(!normal_l1_allowed());
    }
    assert(!player.active());
    update_consumed_inputs();
    assert(!normal_l1_allowed());
    player.clear(); // Cancellation does not re-enable a held L1 either.
    assert(!normal_l1_allowed());
    pins[PIN_L1] = true;
    update_consumed_inputs();
    pins[PIN_L1] = false;
    assert(normal_l1_allowed()); // A fresh press works normally.
    pins[PIN_L1] = true;
    consume_dragon_l1(); // Dragon triggered without L1 must not consume the next press.
    pins[PIN_L1] = false;
    assert(normal_l1_allowed());
    pins[PIN_L1] = true;
    neutral_attack.request(neutral_attack_macro);
    for (int i = 0; i < 4; ++i) neutral_attack.tick();
    assert(neutral_attack.holding() && !(neutral_attack.policy() & CenterDirections));
    assert(neutral_attack.policy() & BlockCpad);
    // Every C direction cancels an active NA helper and its C-input suppression.
    player.clear();
    for (int pin : {PIN_CLEFT, PIN_CRIGHT, PIN_CUP, PIN_CDOWN}) {
        pins[PIN_NA] = true;
        update_cstick_priority();
        pins[PIN_NA] = false;
        neutral_attack.request(neutral_attack_macro);
        pins[pin] = false;
        update_cstick_priority();
        assert(!neutral_attack.active() && !trigger_neutral_attack());
        pins[pin] = true;
        update_cstick_priority();
        assert(!trigger_neutral_attack()); // No delayed NA while NA stays held.
        pins[PIN_NA] = true;
        update_cstick_priority();
        pins[PIN_NA] = false;
        assert(trigger_neutral_attack()); // Release/repress re-arms NA.
    }
    pins[PIN_NA] = pins[PIN_CUP] = true;
    player.request(nil1_macro);
    update_nil_branch_input();
    pins[PIN_NA] = pins[PIN_CUP] = false;
    update_nil_branch_input();
    assert(player.select_branch() == UpFollowup);

    // Actual scheduler: one held NIL activation neither queues itself nor leaks B.
    for (auto& pin : pins) pin = true;
    player.clear(); neutral_attack.clear(); neutral_special.clear(); yoga.clear();
    update_consumed_inputs(); SJ = true; EightBottuns = false;
    update_macros(false);
    pins[PIN_B] = false;
    update_macros(false);
    assert(player.is(nil1_macro));
    const unsigned run = player.run_id;
    for (int i = 0; i < 80; ++i) {
        update_consumed_inputs();
        update_macros(true);
        assert(player.run_id == run && !player.pending);
        assert(!(macro_consumed.filter(read_active_pins_mask()) & (1u << PIN_B)));
    }
    assert(!player.active());
    // Releasing/repressing is still accepted as an intentional same-macro buffer.
    pins[PIN_B] = true; update_consumed_inputs(); update_macros(false);
    pins[PIN_B] = false; update_macros(false);
    assert(player.is(nil1_macro));
    pins[PIN_B] = true; update_consumed_inputs(); update_macros(false);
    pins[PIN_B] = false; update_macros(false);
    assert(player.pending == &nil1_macro);
    pins[PIN_B] = true; update_consumed_inputs(); update_macros(false);
    player.clear();

    // NB: pre-clear 1f, minimum 2f Y press, then uninterrupted physical hold.
    SJ = false;
    pins[PIN_CUP] = true;
    update_macros(false);
    pins[PIN_Y] = false;
    update_macros(false);
    assert(neutral_special.active());
    for (unsigned frame = 0; frame < 10; ++frame) {
        report.buttons = 0;
        report.leftStickXAxis = 0; report.leftStickYAxis = 255;
        apply_step(*neutral_special.step());
        assert(report.leftStickXAxis == (frame < 3 ? 128 : 0));
        assert(report.leftStickYAxis == (frame < 3 ? 128 : 255));
        const bool press_phase = frame >= 1;
        assert(((report.buttons & YButton) != 0) == press_phase);
        assert(!(macro_consumed.filter(read_active_pins_mask()) & (1u << PIN_Y)));
        assert(bool(neutral_special.policy() & CenterDirections) == (frame < 3));
        update_macros(true);
    }
    assert(neutral_special.holding());
    pins[PIN_Y] = true; update_consumed_inputs(); update_macros(false);
    assert(!neutral_special.active());

    // NA centers only during pre-clear and minimum press, then restores live stick input.
    pins[PIN_NA] = false;
    update_macros(false);
    assert(neutral_attack.active());
    for (unsigned frame = 0; frame < 10; ++frame) {
        report.buttons = 0;
        report.leftStickXAxis = 255; report.leftStickYAxis = 128;
        apply_step(*neutral_attack.step());
        assert(((report.buttons & AButton) != 0) == (frame >= 1));
        assert(report.leftStickXAxis == (frame < 3 ? 128 : 255) && report.leftStickYAxis == 128);
        assert(bool(neutral_attack.policy() & CenterDirections) == (frame < 3));
        update_macros(true);
    }
    pins[PIN_NA] = true; update_consumed_inputs(); update_macros(false);
    assert(!neutral_attack.active());

    // A tap released during pre-clear still produces one minimum 2f press.
    pins[PIN_NA] = false;
    update_macros(false);
    pins[PIN_NA] = true; update_consumed_inputs();
    for (unsigned frame = 0; frame < 3; ++frame) {
        report.buttons = 0;
        apply_step(*neutral_attack.step());
        assert(((report.buttons & AButton) != 0) == (frame >= 1));
        update_macros(true);
    }
    assert(!neutral_attack.active());

    // Feed contact bounce into the actual firmware macro runtime for 1.5 seconds.
    for (auto& pin : pins) pin = true;
    player.clear(); neutral_attack.clear(); neutral_special.clear(); yoga.clear();
    update_consumed_inputs(); SJ = true;
    update_macros(false);
    const unsigned before_bounce = player.run_id;
    InputDebouncer debouncer(5000);
    for (unsigned ms = 0; ms < 1500; ++ms) {
        bool contact = ms >= 10 && ms < 1000;
        if (ms == 11 || ms == 13) contact = false;
        if (ms == 1001 || ms == 1003) contact = true;
        const uint32_t stable = debouncer.update(contact ? 1u << PIN_B : 0, ms * 1000);
        pins[PIN_B] = (stable & (1u << PIN_B)) == 0;
        update_consumed_inputs();
        update_macros(ms % 17 == 0);
        assert(!player.pending);
    }
    assert(player.run_id == before_bounce + 1);
    assert(!player.active());

    // A changing direction-history eligibility check is not a new physical input.
    for (auto& pin : pins) pin = true;
    player.clear(); update_consumed_inputs();
    SJ = true; EightBottuns = false;
    update_macros(false);
    pins[PIN_A] = pins[PIN_DOWN] = pins[PIN_LEFT] = false;
    EightBottuns = true; x_i = 1; y_i = 0;
    update_macros(false);
    assert(player.is(dragon_macro));
    y_i = 3; update_macros(false);
    x_i = 4; update_macros(false);
    assert(!player.pending);

    // Keep SJ/down/left and A held: a fresh R1 press queues Dragon despite no condition edge.
    y_i = 100; // Initial history requirement no longer needs to re-establish.
    pins[PIN_R1] = false;
    update_macros(false);
    assert(player.pending == &dragon_macro);
    player.pending = nullptr;
    for (int i = 0; i < 10; ++i) update_macros(false);
    assert(!player.pending); // A held attack is never an auto-repeat.
    pins[PIN_R1] = true; update_consumed_inputs(); update_macros(false);
    pins[PIN_R1] = false; update_macros(false);
    assert(player.pending == &dragon_macro);
    player.pending = nullptr;
    pins[PIN_L1] = false; update_macros(false);
    assert(player.pending == &dragon_macro && !normal_l1_allowed());
    for (int i = 0; i < 9; ++i) update_macros(true);
    assert(player.is(dragon_macro) && !dragon_neutral_hold.holding());
    for (int i = 0; i < 15; ++i) update_macros(true);
    assert(!player.active() && dragon_neutral_hold.holding());
    report.leftStickXAxis = report.leftStickYAxis = 128;
    apply_dragon_stick_lock();
    assert(report.leftStickXAxis == 0 && report.leftStickYAxis == 128); // L1 ends at side, not neutral.
    pins[PIN_L1] = true; update_consumed_inputs(); update_macros(false);
    pins[PIN_L1] = false; update_macros(false);
    assert(player.is(dragon_macro) && !dragon_neutral_hold.holding());
    // Releasing a condition resets the established sequence.
    pins[PIN_DOWN] = true; update_macros(false);
    assert(!dragon_sequence_established);
    player.clear();
    pins[PIN_DOWN] = false; update_macros(false);
    assert(!player.active()); // A fresh sequence must meet the initial history requirement.

    auto reset_dragon_test = []() {
        for (auto& pin : pins) pin = true;
        player.clear(); neutral_attack.clear(); neutral_special.clear(); yoga.clear(); dragon_followup.clear();
        dragon_neutral_hold.clear(); pending_dragon_action = DragonAction::None;
        SJ = true; EightBottuns = false; macro_mode = 1; dpad_mode = 0;
        update_consumed_inputs(); update_macros(false);
    };
    auto start_dragon_test = [](int facing) {
        pins[PIN_DOWN] = pins[facing < 0 ? PIN_LEFT : PIN_RIGHT] = pins[PIN_A] = false;
        EightBottuns = true; x_i = 100; y_i = 1; // Deliberately stale right-facing history.
        update_macros(false);
        assert(player.is(dragon_macro) && player.direction == facing);
    };
    reset_dragon_test(); start_dragon_test(1);
    for (unsigned i = 0; i < 3; ++i) update_macros(true);
    const unsigned interrupted_run = player.run_id;
    pins[PIN_UP] = false; update_macros(false);
    pins[PIN_UP] = true; update_macros(false); // A tap must survive release.
    assert(player.run_id == interrupted_run && !player.pending);
    for (unsigned i = 0; i < 6; ++i) update_macros(true);
    assert(!player.active() && dragon_neutral_hold.centering());
    for (unsigned frame = 0; frame < 3; ++frame) {
        report.leftStickXAxis = 255; report.leftStickYAxis = 255;
        apply_dragon_stick_lock();
        assert(report.leftStickXAxis == 128 && report.leftStickYAxis == 128);
        update_macros(true);
    }
    assert(!dragon_neutral_hold.centering() && !dragon_neutral_hold.holding());

    // A direction-only reversal recovers after the current run, without queuing Dragon.
    reset_dragon_test(); start_dragon_test(1);
    pins[PIN_LEFT] = false; update_macros(false);
    pins[PIN_RIGHT] = true; update_macros(false);
    assert(!player.pending && player.direction == 1);
    // An already queued run still tracks the physically selected facing.
    player.request(dragon_macro, 1);
    update_macros(false);
    for (unsigned i = 0; i < 9; ++i) update_macros(true);
    assert(player.is(dragon_macro) && player.direction == -1);

    for (int facing : {-1, 1}) for (bool cdown : {false, true}) {
        reset_dragon_test(); start_dragon_test(facing);
        const int action_pin = cdown ? PIN_CDOWN : PIN_NA;
        pins[action_pin] = false; update_macros(false);
        pins[action_pin] = true; update_consumed_inputs(); update_macros(false);
        assert(pending_dragon_action != DragonAction::None);
        assert(!neutral_attack.active() && !dragon_followup.active());
        for (unsigned i = 0; i < 9; ++i) update_macros(true);
        assert(!player.active());
        for (unsigned frame = 0; frame < 7; ++frame) {
            report.buttons = 0;
            report.leftStickXAxis = 255; report.leftStickYAxis = 0;
            report.rightStickXAxis = report.rightStickYAxis = 128;
            MacroPlayer& action = cdown ? dragon_followup : neutral_attack;
            assert(action.active());
            apply_step(*action.step());
            if (frame < 3) {
                assert(report.leftStickXAxis == 128 && report.leftStickYAxis == 128);
            } else if (frame < 5) {
                assert(report.leftStickXAxis == (facing < 0 ? 218 : 38));
                assert(report.leftStickYAxis == 218);
            } else {
                assert(report.leftStickXAxis == 128);
                assert(report.leftStickYAxis == (cdown ? 255 : 128));
            }
            assert(bool(report.buttons & AButton) == (!cdown && frame >= 5));
            assert(report.rightStickYAxis == (cdown && frame >= 5 ? 255 : 128));
            update_macros(true);
        }
        assert(!neutral_attack.active() && !dragon_followup.active());
    }
    // Fixed-state CDown is consumed until physical release and cannot extend the 2f pulse.
    reset_dragon_test(); start_dragon_test(1);
    for (unsigned i = 0; i < 9; ++i) update_macros(true);
    assert(dragon_neutral_hold.holding());
    pins[PIN_CDOWN] = false; update_macros(false);
    assert(dragon_followup.active() && !dragon_neutral_hold.holding());
    for (unsigned i = 0; i < 7; ++i) update_macros(true);
    assert(!dragon_followup.active());
    assert(!(macro_consumed.filter(read_active_pins_mask()) & (1u << PIN_CDOWN)));
    // NA's button hold survives, but neutral stick output lasts only its minimum 2f.
    reset_dragon_test(); start_dragon_test(-1);
    for (unsigned i = 0; i < 9; ++i) update_macros(true);
    pins[PIN_NA] = false; update_macros(false);
    for (unsigned i = 0; i < 7; ++i) update_macros(true);
    assert(neutral_attack.holding());
    report.buttons = 0; report.leftStickXAxis = 0; report.leftStickYAxis = 255;
    apply_step(*neutral_attack.step());
    assert(report.buttons & AButton);
    assert(report.leftStickXAxis == 0 && report.leftStickYAxis == 255);
    reset_dragon_test();

    // Direct Dragon skips the prefix; queued Dragon includes neutral and forward steps.
    const int expected_y[] = {255,255,218,218,128,128,128,128,128,128,128,128,128,128,128,255,255,218,218,128,128,128,128,128};
    for (int facing : {-1, 1}) {
        player.clear();
        player.request(dragon_macro, facing);
        assert(player.step_index == DragonDown);
        player.request(dragon_macro, facing);
        for (unsigned frame = 0; frame < 24; ++frame) {
            apply_step(*player.step());
            assert(report.leftStickYAxis == expected_y[frame]);
            const int side = facing < 0 ? 0 : 255;
            const int diagonal = facing < 0 ? 38 : 218;
            const bool neutral_x = frame < 2 || (frame >= 9 && frame < 12) || frame == 15 || frame == 16;
            const bool diagonal_x = frame == 2 || frame == 3 || frame == 17 || frame == 18;
            assert(report.leftStickXAxis == (neutral_x ? 128 : diagonal_x ? diagonal : side));
            if (frame >= 9 && frame < 15) assert(player.policy() & BlockButtons);
            player.tick();
        }
        assert(!player.active());
    }

}
