#pragma once
#include "browser.h"
#include <charconv>

namespace rtv {
inline std::string trim(std::string s) {
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

// Independent allowlist. Only public-universe individual SteamID64 accounts.
inline std::set<uint64_t> readAdmins(std::istream& in) {
    std::set<uint64_t> result;
    std::string line;
    size_t number = 0;
    while (std::getline(in, line)) {
        ++number;
        line = trim(line);
        if (line.empty() || line[0] == '#' || line.rfind("//", 0) == 0) continue;
        uint64_t id = 0;
        const auto parsed = std::from_chars(line.data(), line.data() + line.size(), id);
        if (parsed.ec != std::errc{} || parsed.ptr != line.data() + line.size() ||
            (id >> 32) != 0x01100001 || !(id & 0xffffffff))
            throw std::runtime_error("admins.txt: invalid SteamID64 on line " + std::to_string(number));
        result.insert(id);
    }
    if (in.bad()) throw std::runtime_error("admins.txt: read failed");
    return result;
}

enum class Action { Unknown, Rtv, Nominate, Map, Close, Extend, NextMap, TimeLeft };
struct NativeCommand { Action action = Action::Unknown; std::string query; bool valid = true; };
inline NativeCommand parseNativeCommand(std::string text, bool chat) {
    text = trim(text);
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
        text = text.substr(1, text.size() - 2);
    if (chat) {
        if (text.empty() || (text[0] != '!' && text[0] != '/')) return {};
        text.erase(0, 1);
    }
    const auto end = text.find_first_of(" \t");
    auto name = lower(text.substr(0, end));
    if (!chat) {
        if (name.rfind("rtvhud_", 0) != 0) return {};
        name.erase(0, 7);
    }
    NativeCommand out;
    if (name == "rtv") out.action = Action::Rtv;
    else if (name == "nextmap") out.action = Action::NextMap;
    else if (name == "timeleft") out.action = Action::TimeLeft;
    else if (name == "extend") out.action = Action::Extend;
    else if (name == "nominate" || name == "nom") out.action = Action::Nominate;
    else if (name == "map" || name == "mapmenu" || name == "mm") out.action = Action::Map;
    else if (name == "hudclose" || name == "close") out.action = Action::Close;
    else return {};
    out.query = end == std::string::npos ? "" : trim(text.substr(end + 1));
    out.valid = out.query.size() <= 128 && std::none_of(out.query.begin(), out.query.end(), [](unsigned char c) {
        return c < 32 || c == 127;
    });
    if ((out.action == Action::Rtv || out.action == Action::Close) && !out.query.empty()) out.valid = false;
    return out;
}

// Whole minutes only, bounded against accidental or malicious huge extensions.
inline int extensionMinutes(const std::string& text) {
    int minutes = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), minutes);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
           minutes >= 1 && minutes <= 120 ? minutes : 0;
}
inline float extendedLimit(float current, int minutes) {
    if (!std::isfinite(current) || current <= 0 || minutes < 1 || minutes > 120 || current + minutes > 10080)
        return 0; // Unlimited maps stay unlimited; cap total at one week.
    return current + minutes;
}

struct MapTransition {
    std::string workshop;
    int slot = -1;
    uint64_t steam = 0;
    double executeAt = 0, expires = 0;
    bool dispatched = false;
    bool active() const { return !workshop.empty(); }
    bool queue(const Map& map, const std::string& current, double time, int player = -1, uint64_t id = 0) {
        if (active() || !digits(map.workshop) || map.workshop == "0" || isCurrent(map, current)) return false;
        workshop = map.workshop; slot = player; steam = id;
        executeAt = time + 0.1; expires = time + 30; dispatched = false;
        return true;
    }
    bool authorized(uint64_t connected, const std::set<uint64_t>& admins) const {
        return slot == -1 || (steam && connected == steam && admins.count(steam));
    }
};
}
