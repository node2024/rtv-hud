#include "map_end.h"
#include "native_commands.h"
#include <cassert>
#include <limits>
#include <iostream>

int main() {
    const std::vector<rtv::Map> maps{{"A", "kz_a", "11"}, {"B", "kz_b", "22"}};
    assert(!rtv::mapSecondsLeft(0, 500, 0));
    assert(!rtv::mapSecondsLeft(-1, 500, 0));
    assert(!rtv::mapSecondsLeft(std::numeric_limits<double>::quiet_NaN(), 0, 0));
    assert(*rtv::mapSecondsLeft(2, 80, 20) == 60);
    assert(*rtv::mapSecondsLeft(2, 20, 30) == 120);
    assert(*rtv::mapSecondsLeft(2, 141, 20) == -1);
    rtv::MapEnd end;
    rtv::Vote vote;
    vote.reset(0);
    assert(!end.shouldStart({}, 60));
    assert(!end.shouldStart(60.01, 60));
    assert(end.shouldStart(60, 60));
    end.begin(vote, maps, 100, 60, 2);
    assert(vote.phase == rtv::Phase::Voting && vote.deadline == 130);
    assert(end.next->workshop == "11" && !end.shouldStart(59, 60));
    assert(vote.cast(1, 1, 110) && vote.cast(2, 1, 111));
    assert(!vote.finish(129.9));
    assert(vote.finish(130)); end.settle(vote);
    assert(end.next->workshop == "22");
    // Result display finishes before map time; there must be no early change.
    vote.reset(135, false);
    assert(!end.changeDue(25) && !end.changeDue(0.01));
    assert(end.changeDue(0));
    end.changeQueued = true;
    assert(!end.changeDue(-1));
    // Map reset must clear the reservation and permit the next map's vote.
    end.reset(); assert(!end.next && !end.started && !end.changeQueued);
    // Empty server: reserve fallback without creating a HUD vote.
    end.begin(vote, maps, 200, 60, 0);
    assert(vote.phase == rtv::Phase::Idle && end.next->workshop == "11");
    assert(!end.changeDue(1) && end.changeDue(0));
    // Everyone disconnects while voting; zero remaining ballots still selects next.
    end.reset(); vote.reset(0); end.begin(vote, maps, 0, 60, 1);
    vote.cast(1, 1, 5); vote.disconnect(1);
    assert(vote.finish(30) && vote.winner == -1); end.settle(vote);
    assert(vote.winner == 0 && end.next->workshop == "11");
    // Present players abstaining follows the same fallback policy.
    end.reset(); vote.reset(0); end.begin(vote, maps, 0, 60, 3);
    assert(vote.finish(30)); end.settle(vote); assert(end.next->workshop == "11");
    // A delayed frame crossing the time limit still creates a next map.
    end.reset(); vote.reset(0); end.begin(vote, maps, 500, -4, 2);
    assert(vote.phase == rtv::Phase::Idle && end.changeDue(-4));
    // One available alternative is sufficient; no illegal one-option vote.
    end.reset(); end.begin(vote, {maps[1]}, 0, 60, 4);
    assert(!end.timedVote && end.next->workshop == "22");
    // A long configured ballot cannot extend beyond the map deadline.
    end.reset(); vote.reset(0); vote.settings.duration = 150;
    end.begin(vote, maps, 0, 60, 2); assert(vote.deadline == 60);
    end.capDeadline(vote, 10, 5); assert(vote.deadline == 15);
    assert(vote.finish(15)); end.settle(vote); assert(end.changeDue(0));
    // Extension delays transition without rerolling the winning map.
    assert(!end.changeDue(*rtv::mapSecondsLeft(3, 120, 0)));
    assert(!end.changeDue(rtv::mapSecondsLeft(0, 120, 0)));
    assert(end.changeDue(*rtv::mapSecondsLeft(3, 180, 0)));
    // Existing manual RTV is shared at the end-of-map boundary.
    end.reset(); vote.reset(0); vote.settings.duration = 30; vote.start(maps, 0);
    vote.cast(1, 1, 1); end.adopt(vote, 5, 10);
    assert(vote.deadline == 15 && end.timedVote);
    assert(vote.finish(15)); end.settle(vote); assert(end.next->workshop == "22");
    // A result already on screen can also be adopted without losing its winner.
    end.reset(); end.adopt(vote, 16, 5); assert(end.next->workshop == "22");
    // The candidate helper excludes the current map even for a Workshop path.
    rtv::Nominations nominations;
    auto candidates = nominations.candidates(maps, "workshop/11/kz_a");
    assert(candidates.size() == 1 && candidates[0].workshop == "22");
    assert(rtv::parseNativeCommand("!nextmap", true).action == rtv::Action::NextMap);
    assert(rtv::parseNativeCommand("rtvhud_timeleft", false).action == rtv::Action::TimeLeft);
    std::cout << "Map timer, voting, zero voters, fallback, extension and one-shot transition passed\n";
}
