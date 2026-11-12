#include "input_filter.h"
#include <cassert>
int main() {
    InputDebouncer filter(5000);
    assert(filter.update(0, 0) == 0);
    assert(filter.update(1, 1000) == 0);
    assert(filter.update(0, 2000) == 0);
    assert(filter.update(1, 3000) == 0);
    assert(filter.update(1, 7999) == 0);
    assert(filter.update(1, 8000) == 1);
    assert(filter.update(0, 9000) == 1);
    assert(filter.update(1, 10000) == 1);
    assert(filter.update(0, 11000) == 1);
    assert(filter.update(0, 15999) == 1);
    assert(filter.update(0, 16000) == 0);
    // Contacts are independent: one bouncing direction cannot delay another key.
    assert(filter.update(2, 17000) == 0);
    assert(filter.update(3, 20000) == 0);
    assert(filter.update(2, 21000) == 0);
    assert(filter.update(2, 22000) == 2);
    assert(filter.update(3, 25000) == 2);
    assert(filter.update(3, 30000) == 3);
    ConsumedInputs consumed;
    consumed.consume(1);
    assert(consumed.filter(3) == 2);
    consumed.update(3);
    assert(!consumed.allows(1));
    consumed.update(2);
    assert(consumed.filter(3) == 3); // Release, then a new press, is allowed.
}
