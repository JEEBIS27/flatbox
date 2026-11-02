// In Smash, the legacy logical NA input is now a shared neutral modifier.
static bool smash_neutral_pressed() { return dpad_mode == 0 && !gpio_get(PIN_NA); }
static bool neutral_followup_pressed() {
    return dpad_mode == 0 ? smash_neutral_pressed() && !gpio_get(PIN_A) : !gpio_get(PIN_NA);
}
// Firmware-specific live input actions. Step tables below are the timing source.
enum NilStep { NilJump, NilMovement, NilAttack, NilSteer, NilFollowup, NilRecovery, NilRelease };
static void horizontal_input(int right, int left, bool allow_down = false) {
    report.leftStickXAxis = !gpio_get(PIN_RIGHT) ? right : (!gpio_get(PIN_LEFT) ? left : 128);
    report.leftStickYAxis = !gpio_get(PIN_UP) ? 0 : (allow_down && !gpio_get(PIN_DOWN) ? 255 : 128);
}
static void nil1_movement() {
    report.leftStickXAxis = 128 + ((!gpio_get(PIN_RIGHT) ? 1 : 0) - (!gpio_get(PIN_LEFT) ? 1 : 0)) *
                                   (TILTE ? tilt_abs_value : 127);
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
    if (neutral_followup_pressed()) {
        report.buttons |= BButton | AButton;
        horizontal_input(attack_right, attack_left);
        report.leftStickYAxis = (gpio_get(PIN_RIGHT) && gpio_get(PIN_LEFT)) || !gpio_get(PIN_UP) ? 0 : 128;
    } else if (!gpio_get(PIN_CUP)) {
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
    if (macro_mode == 1) { report.leftStickXAxis = 128; report.leftStickYAxis = 128; }
}
static void neutral_special_output() {
    report.buttons |= YButton;
    if (macro_mode == 1) { report.leftStickXAxis = 128; report.leftStickYAxis = 128; }
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
    {  5_f, nil1_followup,   NoButtons, BlockCpad },       // Optional neutral+A (Smash), NA (other modes), or C-up
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
static const MacroStep dragon_steps[] = {
    { 2_f, Stick::Down,     NoButtons, BlockButtons | BlockL1 },
    { 2_f, Stick::Diagonal, NoButtons, BlockL1 },
    { 1_f, Stick::Side },
};
static const MacroStep neutral_attack_steps[] = {
    { 2_f, neutral_attack_output, NoButtons, CenterDirections },
    { 1_f, Stick::Keep }, // Hold this step until trigger release; no repeated attack.
};
static const MacroStep neutral_special_steps[] = {
    { 2_f, neutral_special_output, NoButtons, CenterDirections | BlockCpad },
    { 1_f, Stick::Keep },
};
static const MacroStep yoga_steps[] = {
    { 1_f, Stick::Down },
    { 1_f, yoga_finish }, // Hold this output until release.
};

static constexpr auto nil1_macro = define_macro("NIL1", nil1_steps, BlockButtons);
static constexpr auto nil2_macro = define_macro("NIL2", nil2_steps, BlockButtons);
static constexpr auto dragon_macro = define_macro("Dragon", dragon_steps);
static constexpr auto neutral_attack_macro = define_macro("Neutral attack", neutral_attack_steps, BlockCpad, Finish::HoldUntilRelease);
static constexpr auto neutral_special_macro = define_macro("Neutral special", neutral_special_steps, PassInput, Finish::HoldUntilRelease);
static constexpr auto yoga_macro = define_macro("Yoga", yoga_steps, PassInput, Finish::HoldUntilRelease);

// Trigger bindings are in descending simultaneous-input priority.
static bool should_start_dragon(void) {
    return !smash_neutral_pressed() && SJ && EightBottuns && abs(x_i) >= y_i && !gpio_get(PIN_DOWN) &&
           ((!gpio_get(PIN_LEFT) && gpio_get(PIN_RIGHT)) || (gpio_get(PIN_LEFT) && !gpio_get(PIN_RIGHT))) &&
           gpio_get(PIN_TILTE);
}

static bool trigger_yoga(void) {
    return !smash_neutral_pressed() && !gpio_get(PIN_LEFT) && !gpio_get(PIN_RIGHT) && !gpio_get(PIN_DOWN) && gpio_get(PIN_UP) &&
           gpio_get(PIN_CLEFT) && gpio_get(PIN_CRIGHT) && gpio_get(PIN_CUP) && gpio_get(PIN_CDOWN) &&
           gpio_get(PIN_TILTE) && abs(y_i) > 1;
}

static bool release_yoga(void) {
    return gpio_get(PIN_LEFT) || gpio_get(PIN_RIGHT) || gpio_get(PIN_DOWN) || !gpio_get(PIN_UP) ||
           !gpio_get(PIN_CLEFT) || !gpio_get(PIN_CRIGHT) || !gpio_get(PIN_CUP) || !gpio_get(PIN_CDOWN);
}


static bool trigger_nil1() { return !smash_neutral_pressed() && !gpio_get(PIN_B) && SJ && macro_mode == 1; }
static bool trigger_nil2() {
    return !smash_neutral_pressed() && !gpio_get(PIN_Y) && SJ && macro_mode == 1 && gpio_get(PIN_CLEFT) && gpio_get(PIN_CRIGHT);
}
static bool trigger_neutral_attack() { return dpad_mode != 0 && !gpio_get(PIN_NA) && !player.active(); }
static bool release_neutral_attack() { return gpio_get(PIN_NA); }
static bool trigger_neutral_special() {
    return dpad_mode != 0 && !gpio_get(PIN_Y) && ((!SJ && macro_mode == 1) || macro_mode > 1);
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
};
static MacroBinding macro_bindings[] = {
    { &dragon_macro, &player, should_start_dragon, nullptr, TriggerMode::QueuedEdge, false },
    { &nil2_macro, &player, trigger_nil2, nullptr, TriggerMode::QueuedEdge, false },
    { &nil1_macro, &player, trigger_nil1, nullptr, TriggerMode::QueuedEdge, false },
    { &neutral_attack_macro, &neutral_attack, trigger_neutral_attack, release_neutral_attack, TriggerMode::HeldHelper, false },
    { &neutral_special_macro, &neutral_special, trigger_neutral_special, release_neutral_special, TriggerMode::HeldHelper, false },
    { &yoga_macro, &yoga, trigger_yoga, release_yoga, TriggerMode::HeldHelper, false },
};
