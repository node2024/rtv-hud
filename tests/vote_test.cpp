#include "vote.h"
#include "navigation.h"
#include <cassert>
#include <iostream>
int main() {
    rtv::Navigation nav;
    assert(!nav.update(rtv::Use | rtv::Down, 6)); // Held at opening: no action.
    assert(!nav.update(0, 6));
    assert(!nav.update(rtv::Up, 6) && nav.selected == 5);
    assert(!nav.update(rtv::Up, 6) && nav.selected == 5); // No repeat.
    nav.update(0, 6);
    assert(!nav.update(rtv::Down, 6) && nav.selected == 0);
    assert(nav.update(rtv::Down | rtv::Use, 6));
    assert(!nav.update(rtv::Down | rtv::Use, 6));
    nav.update(0, 6);
    assert(!nav.update(rtv::Up | rtv::Down, 6) && nav.selected == 0);
    nav = {};
    nav.update(0, 2);
    nav.update(rtv::Up, 2);
    assert(nav.selected == 1);
    std::istringstream input("[T1] kz_a:123\n[T2] kz_b:456\n[T1] duplicate:123\n");
    auto maps = rtv::readMaps(input); assert(maps.size() == 2 && maps[0].name == "kz_a");
    for (auto bad : {"bad:12;quit\nx:2", "bad:0\nx:2", "a:1"}) {
        bool rejected = false; try { std::istringstream s(bad); rtv::readMaps(s); } catch (...) { rejected = true; }
        assert(rejected);
    }
    rtv::Vote v; v.reset(0);
    assert(v.required(0) == 1 && v.required(1) == 1 && v.required(7) == 7);
    for (uint64_t id = 1; id <= 6; ++id) assert(!v.request(id, 7, 30));
    assert(!v.request(6, 7, 30)); // Duplicate requests cannot satisfy unanimity.
    v.disconnect(3);
    assert(!v.request(7, 7, 30)); // A disconnected request no longer counts.
    assert(v.request(3, 7, 30));
    assert(!v.request(3, 8, 30)); // A new player must also agree.
    assert(v.request(8, 8, 30));
    v.reset(0);
    assert(!v.request(1, 2, 29)); assert(!v.request(1, 2, 30));
    assert(!v.request(1, 2, 30)); assert(v.request(2, 2, 30));
    v.start(maps, 30); assert(v.cast(1, 0, 31)); assert(v.cast(1, 1, 32));
    assert(v.ballots.size() == 1 && v.counts()[1] == 1);
    assert(!v.cast(2, -1, 32) && !v.cast(2, 2, 32) && !v.cast(2, 0, 60));
    v.disconnect(1); assert(v.ballots.empty()); assert(v.finish(60) && v.winner == -1);
    assert(rtv::resultText(v, true) == "[RTV] No votes - map vote cancelled.");
    assert(!v.finish(61)); // Result announcement must happen only on the transition.
    v.reset(70); v.start(maps, 100); v.cast(1, 1, 101); v.cast(2, 0, 101);
    assert(v.finish(130) && v.winner == 0); // tie: first in shuffled candidate order
    assert(rtv::resultText(v, true) == "[RTV] Winner: [T1] kz_a (1/2 votes). Changing map in 5 seconds.");
    assert(rtv::resultText(v, false).find("Dry run - map will not change.") != std::string::npos);
    v.reset(140); assert(v.phase == rtv::Phase::Idle && v.ballots.empty() && v.requests.empty());
    v.configure({60, 10, 20, 12, 3}); v.reset(0);
    assert(v.required(0) == 1 && v.required(1) == 1 && v.required(3) == 2 && v.required(7) == 5);
    assert(!v.request(1, 3, 9));
    assert(!v.request(1, 3, 10));
    assert(!v.request(1, 3, 10)); // Duplicate agreement never counts twice.
    assert(v.request(2, 3, 10));
    v.start(maps, 10); assert(v.deadline == 22);
    v.configure({80, 0, 20, 100, 9}); // An active vote keeps its captured durations.
    assert(v.deadline == 22 && v.resultSeconds == 3);
    assert(v.cast(1, 0, 21)); assert(!v.finish(21)); assert(v.finish(22));
    assert(v.deadline == 25);
    assert(rtv::resultText(v, true).find("in 3 seconds") != std::string::npos);
    v.reset(25, false); assert(v.availableAt == 45);
    v.configure({50, 0, 5, 10, 0}); assert(v.availableAt == 30);
    assert(!v.request(1, 4, 29)); assert(!v.request(1, 4, 30));
    assert(v.thresholdReached(2, 30)); // Population drop can satisfy the threshold.
    v.disconnect(1); assert(!v.thresholdReached(0, 30));
    v.configure({-1, -1, -1, 0, -1}); v.reset(0);
    assert(v.settings.percent == 1 && v.settings.initialDelay == 0 && v.settings.cooldown == 0);
    assert(v.settings.duration == 1 && v.settings.resultDuration == 0);
    v.start(maps, 0); assert(v.finish(1) && v.deadline == 1);
    v.configure({101, 90000, 90000, 4000, 400});
    assert(v.settings.percent == 100 && v.settings.initialDelay == 86400 && v.settings.cooldown == 86400);
    assert(v.settings.duration == 3600 && v.settings.resultDuration == 300);
    // RTV offers five actual maps plus a separate continuation option.
    v.reset(0); v.settings.duration=30;
    std::vector<rtv::Map> five;
    for(int i=0;i<5;++i)five.push_back({"Map "+std::to_string(i),"map_"+std::to_string(i),std::to_string(100+i)});
    v.start(five,0,true);
    assert(v.choices.size()==5&&v.optionCount()==6&&v.counts().size()==6);
    assert(v.isExtension(5)&&!v.isExtension(4)&&!v.isExtension(6));
    assert(v.choiceLabel(5).find("+1 hour")!=std::string::npos);
    assert(!v.cast(1,6,1));assert(v.cast(1,5,1));assert(v.cast(2,4,1));assert(v.cast(3,5,1));
    assert(v.finish(30)&&v.extensionWon()&&v.winner==5);
    assert(v.counts()[5]==2&&v.counts()[4]==1);
    assert(rtv::resultText(v,true).find("Continuing the current map")!=std::string::npos);
    assert(rtv::resultText(v,true).find("Changing map in")==std::string::npos);
    assert(rtv::resultText(v,false).find("Dry run")!=std::string::npos);
    v.reset(40,false);assert(!v.offerExtension&&v.optionCount()==0&&!v.extensionWon());
    v.start(five,40,true);v.cast(1,4,41);assert(v.finish(70)&&!v.extensionWon()&&v.winner==4);
    v.reset(80);v.start(five,80,true);v.cast(1,5,81);v.disconnect(1);
    assert(v.finish(110)&&v.winner==-1&&!v.extensionWon());
    v.reset(120);v.start(five,120,true);v.cast(1,5,121);v.cast(2,0,121);
    assert(v.finish(150)&&v.winner==0); // A tie keeps the existing first-option rule.
    v.reset(160);v.start({five[0]},160,true);assert(v.optionCount()==2&&v.isExtension(1));
    std::cout << "vote tests passed\n";
}
