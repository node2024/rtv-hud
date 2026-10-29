#include "browser.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace rtv;
static std::set<std::string> enumerate(const std::vector<Map>& maps, const std::string& query="", const std::string& current="") {
    std::set<std::string> seen;
    const auto list=browserAll(maps,query,current);
    std::set<size_t> headers;
    for(const auto& row:list.rows){
        if(row.map<0){assert(headers.insert(row.group).second);continue;}
        int i=row.map;
        assert(headers.count(row.group));
        assert(tier(maps[i])==tiers[row.group]);
        assert(matches(maps[i],query));assert(!isCurrent(maps[i],current));
        assert(seen.insert(maps[i].workshop).second);
    }
    assert(seen.size()==static_cast<size_t>(list.total));
    return seen;
}
int main(int argc,char** argv) {
    assert(argc==2);std::ifstream file(argv[1]);auto maps=readMaps(file);
    assert(enumerate(maps).size()==maps.size());
    auto ids=enumerate(maps);
    std::reverse(maps.begin(),maps.end());assert(enumerate(maps)==ids);
    // Every requested correction is loaded from the real deployment file.
    const std::map<std::string,std::pair<std::string,std::string>> expected={
        {"3759997670",{"t5","kz_engram"}}, {"3082850001",{"t6","kz_sxb2_misato"}},
        {"3636272568",{"t6","kz_angina"}}, {"3679728328",{"t3","kz_royal_purple"}},
        {"3776801864",{"t6","kz_hb_wangdefa"}}, {"3678967447",{"t5","kz_toofaded"}},
        {"3764093804",{"t3","kz_bhop_majka"}}, {"3718800235",{"t2","kz_silly_metamodernity"}},
        {"3781516256",{"t3","kz_mescaline"}}, {"3780087724",{"t2","kz_phamous"}}
    };
    for(const auto& [id,want]:expected) {
        auto it=std::find_if(maps.begin(),maps.end(),[&](const Map& m){return m.workshop==id;});
        assert(it!=maps.end()&&tier(*it)==want.first&&it->name==want.second&&hasGlobalFlag(*it));
        assert(enumerate(maps,want.first+" "+want.second).count(id));
    }
    assert(!ids.count("3104579274")&&!ids.count("3778551358"));
    auto current=maps.front();assert(enumerate(maps,"","workshop/"+current.workshop+"/"+current.name).size()==maps.size()-1);
    // Previously unpublished maps and entirely changed labels work without catalogs.
    maps.push_back({"[T3][G] kz_new_unpublished","kz_new_unpublished","999999991"});
    assert(enumerate(maps,"t3").count("999999991"));
    maps.back()={"[T6] kz_renamed","kz_renamed","999999992"};
    assert(!enumerate(maps,"t3").count("999999992"));
    assert(enumerate(maps,"t6 renamed").count("999999992"));
    assert(!hasGlobalFlag(maps.back()));
    maps.pop_back();assert(enumerate(maps)==ids);
    // More than the old 363-map catalog and the reusable row pool, in ONE tier.
    std::vector<Map> many;
    for(int i=0;i<2050;++i)many.push_back({"[T2] arbitrary_"+std::to_string(i),"arbitrary_"+std::to_string(i),std::to_string(i+1)});
    assert(enumerate(many).size()==many.size());
    assert(firstExpandedTier(browserAll(many,"",""))==std::set<std::string>{"t2"});
    assert(firstExpandedTier(browserAll(many,"no_match","")).empty());
    std::vector<Map> unknown{{"[T?] unknown","unknown","1"}};
    assert(firstExpandedTier(browserAll(unknown,"",""))==std::set<std::string>{"unknown"});
    assert(hasGlobalFlag({"[T1] [G] test","test","1"}));
    assert(!hasGlobalFlag({"[T1] test[G]","test[G]","1"}));
    std::cout<<"Dynamic maplist: requested edits, rename, ID replacement, tier move, deletion, search, reorder and 2050-map complete data model passed\n";
}
