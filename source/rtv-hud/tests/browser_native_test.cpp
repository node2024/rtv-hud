#include "browser.h"
#include <cassert>
#include <iostream>

int main() {
    std::vector<rtv::Map> maps;
    for(int i=0;i<40;++i)maps.push_back({"[T2] kz_bhop_"+std::to_string(i),"kz_bhop_"+std::to_string(i),std::to_string(100+i)});
    maps.push_back({"[T1] kz_easy","kz_easy","999"});
    assert(rtv::matches(maps[0],"2t BHOP") && rtv::matches(maps[0],"t2 bhop"));
    assert(!rtv::matches(maps[0],"t1")&&!rtv::matches(maps[0],"t2 unknown"));
    auto all=rtv::browserAll(maps,"2t","kz_bhop_0");
    assert(all.total==39&&all.counts[1]==39);
    std::set<int> seen;
    for(const auto& row:all.rows)if(row.map>=0)assert(seen.insert(row.map).second);
    assert(seen.size()==39&&!seen.count(0));
    assert(rtv::browserAll(maps,"nomatch","").total==0);
    maps.push_back({"[T2] new_map","new_map","1000"});
    assert(rtv::browserAll(maps,"t2","").total==41);
    rtv::Nominations n;
    assert(!n.nominate(1,maps[0],maps[0].name,false).empty());
    assert(!n.nominate(1,maps[0],"",true).empty());
    assert(n.nominate(1,maps[0],"",false).empty());
    assert(!n.nominate(2,maps[0],"",false).empty());
    assert(n.nominate(1,maps[1],"",false).empty());
    auto choices=n.candidates(maps,"");assert(choices.size()==6&&choices[0].workshop==maps[1].workshop);
    for(int id=2;id<=6;++id)assert(n.nominate(id,maps[id],"",false).empty());
    assert(!n.nominate(7,maps[7],"",false).empty());
    n.disconnect(1);assert(n.nominate(7,maps[7],"",false).empty());
    choices=n.candidates(maps,"");assert(choices[0].workshop==maps[2].workshop);
    std::set<std::string> ids;for(const auto& m:choices)assert(ids.insert(m.workshop).second);
    n.clear();assert(n.candidates(maps,maps[0].name)[0].workshop!=maps[0].workshop);
    assert(rtv::decodeHex("74322062686f70")=="t2 bhop");
    assert(rtv::decodeHex("-")=="");assert(!rtv::decodeHex("0"));assert(!rtv::decodeHex("GG"));
    assert(!rtv::decodeHex("000a"));assert(!rtv::decodeHex(std::string(258,'a')));
    uint32_t handle=0;std::string button;
    const unsigned char click[]={8,123,18,7,'c','o','n','f','i','r','m'};
    assert(rtv::parseClick(click,sizeof(click),handle,button)&&handle==123&&button=="confirm");
    for(size_t size=0;size<sizeof(click);++size)assert(!rtv::parseClick(click,size,handle,button));
    const unsigned char forged[]={8,1,18,5,'m','a','p',';','1'};
    assert(!rtv::parseClick(forged,sizeof(forged),handle,button));
    std::string oversized(257,'x');assert(!rtv::parseClick(oversized.data(),oversized.size(),handle,button));
    std::cout<<"search, full scroll list, maplist updates, nominations, bounded bridge text and click packets passed\n";
}
