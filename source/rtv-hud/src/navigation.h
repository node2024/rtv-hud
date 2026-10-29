#pragma once
#include <cstdint>

namespace rtv {
// CS2 action bits, independent of the player's physical key bindings.
constexpr uint64_t Use = 0x20, Up = 0x8, Down = 0x10;
struct Navigation {
    uint64_t previous = 0;
    bool initialized = false;
    int selected = 0;
    bool update(uint64_t down, int count) {
        if (!initialized) { previous = down; initialized = true; return false; }
        const auto pressed = down & ~previous;
        previous = down;
        if (count <= 0) return false;
        if ((down & (Up | Down)) != (Up | Down)) {
            if (pressed & Up) selected = (selected + count - 1) % count;
            if (pressed & Down) selected = (selected + 1) % count;
        }
        return (pressed & Use) != 0;
    }
};
}
