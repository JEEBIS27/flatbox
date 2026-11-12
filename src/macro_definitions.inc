static void apply_step(const MacroStep& step) {
    report.buttons |= step.buttons;
    switch (step.stick) {
        case Stick::Keep: break;
        case Stick::Neutral: report.leftStickXAxis = 128; report.leftStickYAxis = 128; break;
        case Stick::Down: report.leftStickXAxis = 128; report.leftStickYAxis = 255; break;
        case Stick::Diagonal: report.leftStickXAxis = player.direction < 0 ? 38 : 218; report.leftStickYAxis = 218; break;
        case Stick::Side: report.leftStickXAxis = player.direction < 0 ? 0 : 255; report.leftStickYAxis = 128; break;
    }
    if (step.apply) step.apply();
}
static DragonNeutralHold dragon_neutral_hold;
static void apply_dragon_stick_lock() {
    if (dragon_neutral_hold.holding()) {
        report.leftStickXAxis = dragon_neutral_hold.facing() < 0 ? 0 : 255;
        report.leftStickYAxis = 128;
    } else if (dragon_neutral_hold.centering()) {
        report.leftStickXAxis = 128;
        report.leftStickYAxis = 128;
    }
}
enum class DragonAction { None, Attack, CDown };
static DragonAction pending_dragon_action = DragonAction::None;
static int dragon_action_facing = 1;
static void dragon_back_down() {
    report.leftStickXAxis = dragon_action_facing < 0 ? 218 : 38;
    report.leftStickYAxis = 218;
}
static void dragon_cdown_output() {
    report.leftStickXAxis = 128; report.leftStickYAxis = 255;
    report.rightStickXAxis = 128; report.rightStickYAxis = 255;
}
constexpr uint32_t DragonStartedWithL1 = 1;
static uint32_t direction_keys() {
    return (!gpio_get(PIN_UP) ? 1u : 0u) | (!gpio_get(PIN_DOWN) ? 2u : 0u) |
           (!gpio_get(PIN_LEFT) ? 4u : 0u) | (!gpio_get(PIN_RIGHT) ? 8u : 0u);
}
static ConsumedInputs macro_consumed;
static void update_consumed_inputs() { macro_consumed.update(read_active_pins_mask()); }
static bool normal_l1_allowed() { return macro_consumed.allows(1u << PIN_L1); }
static void consume_dragon_l1() {
    if (!gpio_get(PIN_L1)) macro_consumed.consume(1u << PIN_L1);
}
// Firmware-specific live input actions. Step tables below are the timing source.
enum NilBranch { NoFollowup, AttackFollowup, UpFollowup };
enum NilStep { NilJump, NilMovement, NilAttack, NilSteer, NilFollowup, NilRecovery, NilRelease };
static void horizontal_input(int right, int left, bool allow_down = false) {
    report.leftStickXAxis = !gpio_get(PIN_RIGHT) ? right : (!gpio_get(PIN_LEFT) ? left : 128);
    report.leftStickYAxis = !gpio_get(PIN_UP) ? 0 : (allow_down && !gpio_get(PIN_DOWN) ? 255 : 128);
}
static void nil1_movement() {
    report.leftStickXAxis = 128 + ((!gpio_get(PIN_RIGHT) ? 1 : 0) - (!gpio_get(PIN_LEFT) ? 1 : 0)) *
                                   127;
    report.leftStickYAxis = !gpio_get(PIN_UP) ? 0 : 128;
}
static void nil1_jump() {
    nil1_movement();
    if (player.at_last_frame() && (!gpio_get(PIN_RIGHT) || !gpio_get(PIN_LEFT)) &&
        gpio_get(PIN_DOWN) && gpio_get(PIN_UP) && gpio_get(PIN_TILTE))
        player.seek(NilMovement, 3); // Resume the movement step after its first three frames.
}
static void medium_movement() { horizontal_input(205, 50); }
static void soft_movement() { horizontal_input(185, 70, true); }
static void full_horizontal() {
    horizontal_input(255, 0);
    report.leftStickYAxis = 128;
}
static void nil_followup(int attack_right, int attack_left, void (*movement)()) {
    const unsigned branch = player.select_branch();
    if (branch == AttackFollowup) {
        report.buttons |= BButton | AButton;
        horizontal_input(attack_right, attack_left);
        report.leftStickYAxis = (gpio_get(PIN_RIGHT) && gpio_get(PIN_LEFT)) || !gpio_get(PIN_UP) ? 0 : 128;
    } else if (branch == UpFollowup) {
        report.rightStickXAxis = 128;
        report.rightStickYAxis = 0;
        movement();
    } else {
        player.seek(NilRelease); // Skip optional recovery to the release step.
    }
}
static void nil1_followup() { nil_followup(255, 0, medium_movement); }
static void nil2_followup() { nil_followup(205, 50, soft_movement); }
static void neutral_attack_output() {
    if (dpad_mode == 0) report.buttons |= AButton;
    if (macro_mode == 1 && !neutral_attack.holding()) { report.leftStickXAxis = 128; report.leftStickYAxis = 128; }
}
static void neutral_special_output() {
    report.buttons |= YButton;
    if (macro_mode == 1 && !neutral_special.holding()) { report.leftStickXAxis = 128; report.leftStickYAxis = 128; }
}
static void yoga_finish() {
    report.leftStickXAxis = SJ && EightBottuns ? (x_i > 0 ? 0 : 255) : (x_i > 0 ? 38 : 218);
    report.leftStickYAxis = SJ && EightBottuns ? 128 : 218;
}

// Each duration is local to its row, not an absolute end-frame threshold.
// duration, output (direction or live action), added buttons, input policy
static const MacroStep nil1_steps[] = {
    {  3_f, nil1_jump,       Jump, BlockCpad }, // Jump
    { 10_f, nil1_movement,   NoButtons, BlockCpad },       // Movement
    {  2_f, Stick::Neutral,  BButton, BlockCpad },
    {  5_f, medium_movement, NoButtons, BlockCpad },
    {  5_f, nil1_followup,   NoButtons, BlockCpad },       // Buffered NA / C-up follow-up
    { 12_f, medium_movement, NoButtons, BlockCpad },       // Recovery
    {  1_f, Stick::Keep },                     // Resume physical stick
};
static const MacroStep nil2_steps[] = {
    {  7_f, full_horizontal, Jump, BlockCpad },
    {  3_f, full_horizontal, NoButtons, BlockCpad },
    {  3_f, Stick::Neutral,  Jump | BButton, BlockCpad },
    {  5_f, soft_movement,   NoButtons, BlockCpad },
    {  5_f, nil2_followup,   NoButtons, BlockCpad },
    { 12_f, medium_movement, NoButtons, BlockCpad },
    {  1_f, Stick::Keep },
};
enum DragonStep { DragonNeutral, DragonForward, DragonDown, DragonDiagonal, DragonSide };
static const MacroStep dragon_steps[] = {
    { 3_f, Stick::Neutral,  NoButtons, BlockButtons | BlockL1 }, // Queued entry
    { 3_f, Stick::Side,     NoButtons, BlockButtons | BlockL1 }, // Forward before restarting
    { 2_f, Stick::Down,     NoButtons, BlockButtons | BlockL1 },
    { 2_f, Stick::Diagonal, NoButtons, BlockL1 },
    { 5_f, Stick::Side },
};
static const MacroStep neutral_attack_steps[] = {
    { 1_f, Stick::Neutral, NoButtons, CenterDirections }, // Before press
    { 2_f, neutral_attack_output, NoButtons, CenterDirections }, // Minimum press at neutral
    { 1_f, neutral_attack_output }, // Hold button; restore normal stick input
};
static const MacroStep neutral_special_steps[] = {
    { 1_f, Stick::Neutral, NoButtons, CenterDirections | BlockCpad },
    { 2_f, neutral_special_output, NoButtons, CenterDirections | BlockCpad }, // Minimum press at neutral
    { 1_f, neutral_special_output, NoButtons, BlockCpad }, // Hold button; restore normal stick input
};
static const MacroStep dragon_attack_steps[] = {
    { 3_f, Stick::Neutral, NoButtons, CenterDirections | BlockCpad | BlockButtons },
    { 2_f, dragon_back_down, NoButtons, BlockCpad | BlockButtons },
    { 2_f, neutral_attack_output, NoButtons, CenterDirections | BlockCpad },
    { 1_f, neutral_attack_output },
};
static const MacroStep dragon_cdown_steps[] = {
    { 3_f, Stick::Neutral, NoButtons, CenterDirections | BlockCpad | BlockButtons },
    { 2_f, dragon_back_down, NoButtons, BlockCpad | BlockButtons },
    { 2_f, dragon_cdown_output, NoButtons, BlockCpad },
};
static const MacroStep yoga_steps[] = {
    { 2_f, Stick::Down },
    { 2_f, yoga_finish }, // Hold this output until release.
};

static constexpr auto nil1_macro = define_macro("NIL1", nil1_steps, BlockButtons);
static constexpr auto nil2_macro = define_macro("NIL2", nil2_steps, BlockButtons);
static constexpr auto dragon_macro = define_macro("Dragon", dragon_steps, PassInput, Finish::Complete, DragonDown);
static constexpr auto neutral_attack_macro = define_macro("Neutral attack", neutral_attack_steps, BlockCpad, Finish::HoldUntilRelease);
static constexpr auto neutral_special_macro = define_macro("Neutral special", neutral_special_steps, PassInput, Finish::HoldUntilRelease);
static constexpr auto yoga_macro = define_macro("Yoga", yoga_steps, PassInput, Finish::HoldUntilRelease);

static constexpr auto dragon_attack_macro = define_macro("Dragon to NA", dragon_attack_steps, BlockCpad, Finish::HoldUntilRelease);
static constexpr auto dragon_cdown_macro = define_macro("Dragon to CDown", dragon_cdown_steps, BlockCpad);

// Trigger bindings are in descending simultaneous-input priority.
static bool dragon_sequence_established = false;
static uint32_t dragon_attack_mask() {
    return (1u << PIN_A) | (1u << PIN_B) | (1u << PIN_L1) |
        (dpad_mode == 0 ? (1u << PIN_R1) : (1u << PIN_X) | (1u << PIN_Y) | (1u << PIN_L2));
}
static bool dragon_condition_keys_held() {
    return SJ && !gpio_get(PIN_DOWN) &&
        ((!gpio_get(PIN_LEFT) && gpio_get(PIN_RIGHT)) || (gpio_get(PIN_LEFT) && !gpio_get(PIN_RIGHT))) &&
        gpio_get(PIN_TILTE);
}
static bool should_start_dragon(void) {
    return dragon_condition_keys_held() && EightBottuns &&
        (dragon_sequence_established || abs(x_i) >= y_i);
}

static bool trigger_yoga(void) {
    return !player.active() && !dragon_followup.active() && pending_dragon_action == DragonAction::None && !neutral_attack.active() && !neutral_special.active() && !gpio_get(PIN_LEFT) && !gpio_get(PIN_RIGHT) && !gpio_get(PIN_DOWN) && gpio_get(PIN_UP) &&
           gpio_get(PIN_CLEFT) && gpio_get(PIN_CRIGHT) && gpio_get(PIN_CUP) && gpio_get(PIN_CDOWN) &&
           gpio_get(PIN_TILTE) && abs(y_i) > 1;
}

static bool release_yoga(void) {
    return gpio_get(PIN_LEFT) || gpio_get(PIN_RIGHT) || gpio_get(PIN_DOWN) || !gpio_get(PIN_UP) ||
           !gpio_get(PIN_CLEFT) || !gpio_get(PIN_CRIGHT) || !gpio_get(PIN_CUP) || !gpio_get(PIN_CDOWN);
}


static bool trigger_nil1() { return !gpio_get(PIN_B) && macro_consumed.allows(1u << PIN_B) && SJ && macro_mode == 1; }
static bool trigger_nil2() {
    return !gpio_get(PIN_Y) && macro_consumed.allows(1u << PIN_Y) && SJ && macro_mode == 1 && gpio_get(PIN_CLEFT) && gpio_get(PIN_CRIGHT);
}
static bool cstick_pressed() {
    return !gpio_get(PIN_CUP) || !gpio_get(PIN_CDOWN) ||
           !gpio_get(PIN_CLEFT) || !gpio_get(PIN_CRIGHT);
}
static bool neutral_attack_suppressed = false;
static void update_cstick_priority() {
    if (gpio_get(PIN_NA)) neutral_attack_suppressed = false;
    else if (cstick_pressed()) neutral_attack_suppressed = true;
    if (cstick_pressed()) neutral_attack.clear();
}
static bool trigger_neutral_attack() { return !gpio_get(PIN_NA) && macro_consumed.allows(1u << PIN_NA) && !player.active() && !dragon_followup.active() && pending_dragon_action == DragonAction::None && !cstick_pressed() && !neutral_attack_suppressed; }
static bool release_neutral_attack() { return gpio_get(PIN_NA); }
static bool trigger_neutral_special() {
    return !gpio_get(PIN_Y) && macro_consumed.allows(1u << PIN_Y) && !player.active() && !yoga.active() && ((!SJ && macro_mode == 1) || macro_mode > 1);
}
static bool release_neutral_special() { return gpio_get(PIN_Y); }
enum class TriggerMode { QueuedEdge, HeldHelper };
struct MacroBinding {
    const MacroDefinition* definition;
    MacroPlayer* state;
    bool (*trigger)();
    bool (*released)();
    TriggerMode trigger_mode;
    bool previous;
    uint32_t activation_mask;
};
static MacroBinding macro_bindings[] = {
    { &dragon_macro, &player, should_start_dragon, nullptr, TriggerMode::QueuedEdge, false,
      (1u << PIN_UP) | (1u << PIN_DOWN) | (1u << PIN_LEFT) | (1u << PIN_RIGHT) |
      (1u << PIN_SJ) | (1u << PIN_TILTE) | (1u << PIN_A) | (1u << PIN_B) |
      (1u << PIN_L1) | (1u << PIN_R1) | (1u << PIN_X) | (1u << PIN_Y) | (1u << PIN_L2) },
    { &nil2_macro, &player, trigger_nil2, nullptr, TriggerMode::QueuedEdge, false, 1u << PIN_Y },
    { &nil1_macro, &player, trigger_nil1, nullptr, TriggerMode::QueuedEdge, false, 1u << PIN_B },
    { &neutral_attack_macro, &neutral_attack, trigger_neutral_attack, release_neutral_attack, TriggerMode::HeldHelper, false, 0 },
    { &neutral_special_macro, &neutral_special, trigger_neutral_special, release_neutral_special, TriggerMode::HeldHelper, false, 0 },
    { &yoga_macro, &yoga, trigger_yoga, release_yoga, TriggerMode::HeldHelper, false, 0 },
};

// Accept branches throughout the parent macro, until its branch is selected.
static void update_nil_branch_input() {
    static bool previous_attack = false, previous_up = false;
    static uint32_t previous_run = 0;
    const bool attack = !gpio_get(PIN_NA);
    const bool up = !gpio_get(PIN_CUP);
    const bool new_run = player.run_id != previous_run;
    if ((player.is(nil1_macro) || player.is(nil2_macro)) && player.step_index <= NilFollowup) {
        // Latest new input wins; simultaneous NA/C-up gives C-up priority.
        if (up && (new_run || !previous_up)) player.reserve_branch(UpFollowup);
        else if (attack && !up && (new_run || !previous_attack)) player.reserve_branch(AttackFollowup);
    }
    previous_attack = attack; previous_up = up; previous_run = player.run_id;
}

// Shared runtime, also exercised by the host input-sequence tests.
static void update_macros(bool advance) {
    // Snapshot each Dragon start before tracking subsequent input changes.
    static uint32_t watched_dragon_run = 0;
    if (player.is(dragon_macro) && watched_dragon_run != player.run_id) {
        dragon_neutral_hold.watch(direction_keys(), SJ, player.direction);
        watched_dragon_run = player.run_id;
    }
    // Advance existing sequences before accepting fresh events.
    dragon_neutral_hold.update(direction_keys(), SJ, advance);
    // Releasing one side after an overlap must refresh an already queued Dragon too.
    if (gpio_get(PIN_LEFT) != gpio_get(PIN_RIGHT))
        player.update_pending_direction(dragon_macro, !gpio_get(PIN_LEFT) ? -1 : 1);
    if (advance) {
        const bool finishing_dragon = player.is(dragon_macro);
        const uint32_t previous_run = player.run_id;
        player.tick(); neutral_attack.tick(); neutral_special.tick(); yoga.tick(); dragon_followup.tick();
        if (finishing_dragon && !player.active() && !neutral_attack.active())
            dragon_neutral_hold.finish(direction_keys(), SJ);
        if (player.run_id != previous_run) {
            dragon_neutral_hold.clear();
            if (player.is(dragon_macro)) {
                dragon_neutral_hold.watch(direction_keys(), SJ, player.direction);
                watched_dragon_run = player.run_id;
            }
        }
    }
    // A short NA/C-down tap is retained independently of its physical release.
    const bool action_window = player.is(dragon_macro) || dragon_neutral_hold.holding();
    if (action_window) {
        DragonAction action = DragonAction::None;
        if (!gpio_get(PIN_NA) && macro_consumed.allows(1u << PIN_NA) && !cstick_pressed())
            action = DragonAction::Attack;
        if (!gpio_get(PIN_CDOWN) && macro_consumed.allows(1u << PIN_CDOWN))
            action = DragonAction::CDown;
        if (action != DragonAction::None) {
            pending_dragon_action = action;
            dragon_action_facing = player.is(dragon_macro) ? player.direction : dragon_neutral_hold.facing();
            macro_consumed.consume(1u << (action == DragonAction::Attack ? PIN_NA : PIN_CDOWN));
            player.pending = nullptr; // Latest request replaces the next-macro slot.
            neutral_attack.clear(); neutral_special.clear(); yoga.clear();
        }
    }
    if (!player.active() && pending_dragon_action != DragonAction::None) {
        dragon_neutral_hold.clear();
        if (pending_dragon_action == DragonAction::Attack)
            neutral_attack.request(dragon_attack_macro);
        else
            dragon_followup.request(dragon_cdown_macro);
        pending_dragon_action = DragonAction::None;
    }
    update_cstick_priority();
    if (!dragon_condition_keys_held() || macro_mode == 0) dragon_sequence_established = false;
    const uint32_t held = read_active_pins_mask();
    static uint32_t previous_held = 0;
    bool accepted = false;
    for (auto& binding : macro_bindings) {
        const bool triggered = binding.trigger();
        if (binding.trigger_mode == TriggerMode::QueuedEdge) {
            const uint32_t attack_mask = binding.definition == &dragon_macro
                ? dragon_attack_mask() : binding.activation_mask;
            const bool new_attack = (held & ~previous_held & attack_mask) != 0;
            const bool new_condition = !binding.previous && ((held ^ previous_held) & binding.activation_mask);
            if (triggered && (new_attack || (new_condition && !player.is(dragon_macro) && !dragon_neutral_hold.centering())) && !accepted && macro_mode > 0) {
                const uint32_t context = binding.definition == &dragon_macro &&
                    !gpio_get(PIN_L1) && normal_l1_allowed() ? DragonStartedWithL1 : 0;
                const bool immediate = !player.active();
                const int facing = binding.definition == &dragon_macro
                    ? (!gpio_get(PIN_LEFT) ? -1 : 1) : (x_i < 0 ? -1 : 1);
                pending_dragon_action = DragonAction::None;
                dragon_followup.clear();
                player.request(*binding.definition, facing, context);
                if (immediate) dragon_neutral_hold.clear();
                if (binding.definition == &dragon_macro) {
                    if (immediate) {
                        dragon_neutral_hold.watch(direction_keys(), SJ, facing);
                        watched_dragon_run = player.run_id;
                    }
                    dragon_sequence_established = true;
                    consume_dragon_l1();
                }
                else if (binding.definition == &nil1_macro) macro_consumed.consume(1u << PIN_B);
                else if (binding.definition == &nil2_macro) macro_consumed.consume(1u << PIN_Y);
                neutral_attack.clear(); neutral_special.clear();
                accepted = true;
            }
        } else {
            auto& state = *binding.state;
            if (state.holding() && binding.released()) state.clear();
            if (!state.active() && triggered) {
                state.request(*binding.definition);
                if (binding.definition == &neutral_attack_macro) {
                    macro_consumed.consume(1u << PIN_NA);
                    yoga.clear();
                    dragon_neutral_hold.clear();
                }
                if (binding.definition == &neutral_special_macro) macro_consumed.consume(1u << PIN_Y);
            }
        }
        binding.previous = triggered;
    }
    previous_held = held;
    if (macro_mode == 0) {
        player.clear(); dragon_neutral_hold.clear(); dragon_followup.clear();
        pending_dragon_action = DragonAction::None;
    }
    update_nil_branch_input();
}
