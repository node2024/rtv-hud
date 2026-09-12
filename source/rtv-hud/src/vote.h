#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace rtv {
struct Map { std::string label, name, workshop; };
inline bool digits(const std::string& s) {
    return !s.empty() && s.size() <= 20 && s.find_first_not_of("0123456789") == std::string::npos;
}
inline std::vector<Map> readMaps(std::istream& in) {
    std::vector<Map> out;
    std::set<std::string> ids;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#' || line.rfind("//", 0) == 0) continue;
        auto colon = line.rfind(':');
        if (colon == std::string::npos) throw std::runtime_error("maplist: expected label:workshop_id");
        Map m{line.substr(0, colon), {}, line.substr(colon + 1)};
        m.name = m.label.substr(m.label.find_last_of(" \t") + 1);
        if (!digits(m.workshop) || m.workshop == "0" || m.name.empty() || m.label.size() > 160 ||
            m.name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
            throw std::runtime_error("maplist: invalid map name or workshop id");
        if (ids.insert(m.workshop).second) out.push_back(m);
    }
    if (out.size() < 2) throw std::runtime_error("maplist: at least two unique maps required");
    return out;
}
enum class Phase { Idle, Voting, Result };
struct VoteSettings {
    int percent = 100, initialDelay = 30, cooldown = 30, duration = 30, resultDuration = 5;
    void normalize() {
        percent = std::clamp(percent, 1, 100);
        initialDelay = std::clamp(initialDelay, 0, 86400);
        cooldown = std::clamp(cooldown, 0, 86400);
        duration = std::clamp(duration, 1, 3600);
        resultDuration = std::clamp(resultDuration, 0, 300);
    }
};
class Vote {
public:
    VoteSettings settings;
    Phase phase = Phase::Idle;
    std::set<uint64_t> requests;
    std::map<uint64_t, int> ballots;
    std::vector<Map> choices;
    double deadline = 0, availableAt = 0;
    int winner = -1;
    double resetAt = 0;
    bool initialWait = true;
    int resultSeconds = 5;
    void configure(VoteSettings value) {
        value.normalize(); settings = value;
        if (phase == Phase::Idle) availableAt = resetAt + (initialWait ? settings.initialDelay : settings.cooldown);
    }
    void reset(double now, bool initial = true) {
        const auto saved = settings; *this = Vote{};
        resetAt = now; initialWait = initial; configure(saved);
    }
    int required(int humans) const { return std::max(1, static_cast<int>(std::ceil(std::max(0, humans) * settings.percent / 100.0))); }
    bool thresholdReached(int humans, double now) const {
        return phase == Phase::Idle && now >= availableAt && !requests.empty() &&
            static_cast<int>(requests.size()) >= required(humans);
    }
    bool request(uint64_t id, int humans, double now) {
        if (!id || phase != Phase::Idle || now < availableAt) return false;
        requests.insert(id);
        return thresholdReached(humans, now);
    }
    void start(std::vector<Map> maps, double now) {
        if (phase != Phase::Idle || maps.size() < 2 || maps.size() > 6) throw std::runtime_error("invalid vote start");
        choices = std::move(maps); ballots.clear(); requests.clear(); winner = -1;
        deadline = now + settings.duration; resultSeconds = settings.resultDuration; phase = Phase::Voting;
    }
    bool cast(uint64_t id, int index, double now) {
        if (!id || phase != Phase::Voting || now >= deadline || index < 0 || index >= static_cast<int>(choices.size())) return false;
        ballots[id] = index; return true;
    }
    void disconnect(uint64_t id) { requests.erase(id); ballots.erase(id); }
    std::vector<int> counts() const {
        std::vector<int> result(choices.size());
        for (auto [id, index] : ballots) ++result.at(index);
        return result;
    }
    bool finish(double now) {
        if (phase != Phase::Voting || now < deadline) return false;
        auto tally = counts();
        if (!ballots.empty()) winner = static_cast<int>(std::max_element(tally.begin(), tally.end()) - tally.begin());
        phase = Phase::Result; deadline = now + resultSeconds; return true;
    }
};
inline std::string resultText(const Vote& vote, bool allowChange) {
    if (vote.phase != Phase::Result) throw std::runtime_error("vote result is not ready");
    if (vote.winner < 0) return "[RTV] No votes - map vote cancelled.";
    const auto tally = vote.counts();
    int total = 0;
    for (int count : tally) total += count;
    return "[RTV] Winner: " + vote.choices.at(vote.winner).label + " (" +
        std::to_string(tally.at(vote.winner)) + "/" + std::to_string(total) + " votes). " +
        (allowChange ? "Changing map in " + std::to_string(vote.resultSeconds) + " seconds." : "Dry run - map will not change.");
}
}
