#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace rtv {
// Prototype must be initialized but must not own a live entity.
template<class T> class PersonalHuds {
public:
    struct Entry { uint64_t steam; T hud; };
    std::array<std::optional<Entry>, 64> entries;
    T& open(int slot, uint64_t steam, const T& prototype) {
        if (slot < 0 || slot >= 64 || !steam) throw std::invalid_argument("invalid HUD owner");
        auto& entry = entries[slot];
        if (entry && entry->steam != steam) close(slot);
        if (!entry) entry.emplace(Entry{steam, prototype});
        entry->hud.ensure();
        return entry->hud;
    }
    void close(int slot) {
        if (slot < 0 || slot >= 64 || !entries[slot]) return;
        entries[slot]->hud.destroy();
        entries[slot].reset();
    }
    void clear() { for (int slot = 0; slot < 64; ++slot) close(slot); }
    // Called after a level change, when the engine has already removed entities.
    void forget() { for (auto& entry : entries) entry.reset(); }
    bool visibleTo(int owner, int recipient, uint64_t authenticatedOwner) const {
        return owner >= 0 && owner < 64 && owner == recipient && entries[owner] &&
            authenticatedOwner && entries[owner]->steam == authenticatedOwner;
    }
};
}
