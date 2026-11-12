#pragma once
#include <stdint.h>

// Both press and release must remain stable before becoming visible to firmware.
class InputDebouncer {
public:
    explicit InputDebouncer(uint64_t interval_us) : interval(interval_us) {}
    uint32_t update(uint32_t raw, uint64_t now) {
        const uint32_t changed = raw ^ candidate;
        candidate = raw;
        for (unsigned bit = 0; bit < 32; ++bit) {
            const uint32_t mask = uint32_t(1) << bit;
            if (changed & mask) since[bit] = now;
            if (((stable ^ candidate) & mask) && now - since[bit] >= interval)
                stable = (stable & ~mask) | (candidate & mask);
        }
        return stable;
    }
private:
    uint64_t interval;
    uint64_t since[32] {};
    uint32_t stable = 0, candidate = 0;
};

// A physical press used by a macro cannot leak out again as an ordinary press.
class ConsumedInputs {
public:
    void update(uint32_t held) { blocked &= held; }
    void consume(uint32_t inputs) { blocked |= inputs; }
    bool allows(uint32_t input) const { return (blocked & input) == 0; }
    uint32_t filter(uint32_t inputs) const { return inputs & ~blocked; }
private:
    uint32_t blocked = 0;
};
