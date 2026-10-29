#include "personal_huds.h"
#include <cassert>
#include <set>
#include <iostream>

struct FakeHud {
    static inline int next = 0;
    static inline std::set<int> alive;
    int entity = 0;
    void ensure() { if (!entity) { entity = ++next; alive.insert(entity); } }
    void destroy() { assert(entity && alive.erase(entity) == 1); entity = 0; }
};
int main() {
    rtv::PersonalHuds<FakeHud> huds;
    FakeHud prototype;
    const int first = huds.open(0, 100, prototype).entity;
    const int second = huds.open(1, 200, prototype).entity;
    assert(first != second && FakeHud::alive.size() == 2);
    assert(huds.open(0, 100, prototype).entity == first); // Rendering reuses the entity.
    assert(huds.visibleTo(0, 0, 100));
    assert(!huds.visibleTo(0, 1, 100)); // Another client / spectator receives no copy.
    assert(!huds.visibleTo(0, 0, 101) && !huds.visibleTo(0, 0, 0));
    huds.close(0); // Voting destroys only the voter’s entity.
    assert(!FakeHud::alive.count(first) && FakeHud::alive.count(second));
    assert(!huds.visibleTo(0, 0, 100));
    huds.close(0); // Repeated close is harmless.
    const int reopened = huds.open(0, 100, prototype).entity;
    assert(reopened != first && huds.entries[1]->hud.entity == second);
    const int replacement = huds.open(1, 300, prototype).entity; // Slot reuse after disconnect.
    assert(replacement != second && !FakeHud::alive.count(second));
    assert(!huds.visibleTo(1, 1, 200) && huds.visibleTo(1, 1, 300));
    huds.clear(); assert(FakeHud::alive.empty());
    huds.open(63, 400, prototype);
    FakeHud::alive.clear(); // The engine removed entities on level shutdown.
    huds.forget(); assert(!huds.entries[63]); // Must not delete a stale entity again.
    for (int bad : {-1, 64}) {
        bool rejected = false;
        try { huds.open(bad, 1, prototype); } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    std::cout << "personal HUD lifecycle and ownership tests passed\n";
}
