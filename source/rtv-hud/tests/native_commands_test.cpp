#include "native_commands.h"
#include <cassert>
#include <iostream>

int main() {
    const uint64_t admin = 76561198000000001ULL, other = admin + 1;
    std::istringstream valid("# SteamID64\n\n// comment\n 76561198000000001\r\n76561198000000001\n");
    const auto admins = rtv::readAdmins(valid);
    assert(admins == std::set<uint64_t>{admin});
    for (const auto* value : {"STEAM_1:0:123", "0", "18446744073709551616", "76561198000000001junk",
                              "76561198000000001;quit", "76561197960265728", "-76561198000000001"}) {
        std::istringstream invalid(value); bool rejected = false;
        try { rtv::readAdmins(invalid); } catch (const std::exception&) { rejected = true; }
        assert(rejected);
    }
    std::istringstream empty("# Nobody can change maps directly\n");
    assert(rtv::readAdmins(empty).empty());
    using A = rtv::Action;
    const auto extend = rtv::parseNativeCommand("!extend 10", true);
    assert(extend.action == A::Extend && rtv::extensionMinutes(extend.query) == 10);
    assert(rtv::parseNativeCommand("rtvhud_extend 15", false).action == A::Extend);
    for (const auto* value : {"", "0", "-1", "+10", "121", "1.5", "10;quit", "10 20", "999999999999999999"})
        assert(rtv::extensionMinutes(value) == 0);
    assert(rtv::extensionMinutes("120") == 120);
    assert(rtv::extendedLimit(480, 10) == 490);
    assert(rtv::extendedLimit(0, 10) == 0); // Unlimited must stay unlimited.
    assert(rtv::extendedLimit(10080, 1) == 0);
    assert(rtv::extendedLimit(10, -1) == 0);
    for (const auto* text : {"!rtv", "/rtv", "\"!RTV\""})
        assert(rtv::parseNativeCommand(text, true).action == A::Rtv);
    auto search = rtv::parseNativeCommand("\"!nominate t2 bhop\"", true);
    assert(search.action == A::Nominate && search.query == "t2 bhop" && search.valid);
    assert(rtv::parseNativeCommand("/mapmenu t1", true).action == A::Map);
    assert(rtv::parseNativeCommand("rtvhud_map t2", false).action == A::Map);
    assert(rtv::parseNativeCommand("!hudclose", true).action == A::Close);
    assert(rtv::parseNativeCommand("rtvhud_close", false).action == A::Close);
    for (const auto* text : {"hello !map", "!mapping", "!rtv;quit", "rtv"})
        assert(rtv::parseNativeCommand(text, true).action == A::Unknown);
    assert(rtv::parseNativeCommand("css_map", false).action == A::Unknown);
    assert(!rtv::parseNativeCommand("!map " + std::string(129, 'a'), true).valid);
    assert(!rtv::parseNativeCommand("!map kz_a\nquit", true).valid);
    assert(!rtv::parseNativeCommand("!rtv something", true).valid);
    rtv::Map map{"[T1] kz_test", "kz_test", "1234567890"};
    rtv::MapTransition change;
    assert(!change.queue(map, "kz_test", 1, 0, admin));
    assert(change.queue(map, "another_map", 1, 0, admin));
    assert(change.active() && !change.dispatched && change.executeAt > 1);
    assert(!change.queue(map, "another_map", 2, 0, admin));
    assert(change.authorized(admin, admins));
    assert(!change.authorized(other, admins)); // Reused slot.
    assert(!change.authorized(0, admins)); // Disconnected.
    assert(!change.authorized(admin, {})); // Permission revoked before dispatch.
    change = {};
    assert(change.queue(map, "another_map", 1)); // RTV winner needs no admin.
    assert(change.authorized(0, {}));
    change = {};
    map.workshop = "1;quit";
    assert(!change.queue(map, "another_map", 1));
    std::cout << "native command parsing, SteamID allowlist and map transition authorization passed\n";
}
