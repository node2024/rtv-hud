#pragma once
#include "browser.h"
#include <optional>

namespace rtv {
// No wall clock: use the same map clock and start time as the server.
inline std::optional<double> mapSecondsLeft(double minutes, double current, double start) {
    if (!std::isfinite(minutes) || !std::isfinite(current) || !std::isfinite(start) || minutes <= 0)
        return {};
    return minutes * 60.0 - std::max(0.0, current - start);
}
class MapEnd {
public:
    bool started = false, timedVote = false, changeQueued = false;
    std::optional<Map> next;
    void reset() { *this = {}; }
    bool shouldStart(std::optional<double> left, int lead) const {
        return !started && left && *left <= std::clamp(lead, 1, 3600);
    }
    // Candidates are already randomized, nominated and filtered for current map.
    void begin(Vote& vote, std::vector<Map> candidates, double time, double left, int humans) {
        if (started || candidates.empty() || vote.phase != Phase::Idle)
            throw std::runtime_error("invalid end-of-map vote start");
        started = true;
        next = candidates.front(); // A fallback is reserved even before anyone votes.
        if (humans <= 0 || candidates.size() == 1 || left <= 0) return;
        vote.start(std::move(candidates), time);
        timedVote = true;
        capDeadline(vote, time, left);
    }
    // An RTV already running at the end of the map shares the same ballot.
    void adopt(Vote& vote, double time, double left) {
        if (started || vote.phase == Phase::Idle || vote.choices.empty()) return;
        started = true; timedVote = true; next = vote.choices.front();
        capDeadline(vote, time, left);
        if (vote.phase == Phase::Result) settle(vote);
    }
    void capDeadline(Vote& vote, double time, double left) const {
        if (timedVote && vote.phase == Phase::Voting)
            vote.deadline = std::min(vote.deadline, time + std::max(0.0, left));
    }
    void settle(Vote& vote) {
        if (!timedVote || vote.phase != Phase::Result || vote.choices.empty())
            throw std::runtime_error("end-of-map result is not ready");
        // Zero votes (including all players disconnecting) must still pick a map.
        if (vote.winner < 0) vote.winner = 0;
        next = vote.choices.at(vote.winner);
    }
    bool changeDue(std::optional<double> left) const {
        return left && *left <= 0 && next && !changeQueued;
    }
};
}
