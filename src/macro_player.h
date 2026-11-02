#pragma once
#include <stddef.h>
#include <stdint.h>

using Frame = uint32_t;
constexpr Frame operator"" _f(unsigned long long n) { return static_cast<Frame>(n); }
enum class Stick { Keep, Neutral, Down, Diagonal, Side };
enum MacroButton : uint16_t {
    NoButtons = 0, YButton = 1 << 0, BButton = 1 << 1,
    AButton = 1 << 2, Jump = 1 << 6
};
enum InputPolicy : unsigned {
    PassInput = 0, BlockCpad = 1 << 0, BlockButtons = 1 << 1,
    BlockL1 = 1 << 2, CenterDirections = 1 << 3
};
struct MacroStep {
    Frame frames;
    Stick stick;
    uint16_t buttons;
    void (*apply)();
    unsigned policy;
    constexpr MacroStep(Frame n, Stick dir, uint16_t keys = NoButtons, unsigned p = PassInput)
        : frames(n), stick(dir), buttons(keys), apply(nullptr), policy(p) {}
    constexpr MacroStep(Frame n, void (*fn)(), uint16_t keys = NoButtons, unsigned p = PassInput)
        : frames(n), stick(Stick::Keep), buttons(keys), apply(fn), policy(p) {}
};
enum class Finish { Complete, HoldUntilRelease };
struct MacroDefinition {
    const char* name;
    const MacroStep* steps;
    size_t count;
    unsigned policy;
    Finish finish;
};
template<size_t N>
constexpr MacroDefinition define_macro(const char* name, const MacroStep (&steps)[N],
                                       unsigned policy = PassInput, Finish finish = Finish::Complete) {
    return {name, steps, N, policy, finish};
}
// Durations live only in step definitions; no independent total or end-frame constants.
class MacroPlayer {
public:
    const MacroDefinition* current = nullptr;
    const MacroDefinition* pending = nullptr;
    size_t step_index = 0;
    Frame step_frame = 0; // zero based within the current step
    int direction = 1;
    void request(const MacroDefinition& macro, int facing = 1) {
        if (!current) start(&macro, facing);
        else { pending = &macro; pending_direction = facing; }
    }
    const MacroStep* step() const { return current ? &current->steps[step_index] : nullptr; }
    bool active() const { return current != nullptr; }
    bool is(const MacroDefinition& macro) const { return current == &macro; }
    unsigned policy() const { return current ? current->policy | step()->policy : PassInput; }
    bool at_last_frame() const { return current && step_frame + 1 >= step()->frames; }
    bool holding() const { return current && current->finish == Finish::HoldUntilRelease && step_index + 1 == current->count; }
    void tick() {
        if (!current || holding()) return;
        if (++step_frame < step()->frames) return;
        step_frame = 0;
        if (++step_index < current->count) return;
        auto next = pending; pending = nullptr; start(next, pending_direction);
    }
    void seek(size_t index, Frame offset = 0) {
        if (current && index < current->count && offset < current->steps[index].frames) {
            step_index = index; step_frame = offset;
        }
    }
    void clear() { current = pending = nullptr; step_index = 0; step_frame = 0; }
private:
    int pending_direction = 1;
    void start(const MacroDefinition* macro, int facing) {
        current = macro; step_index = 0; step_frame = 0; direction = facing;
    }
};
