#include "browser_ui.h"
#include <cassert>
#include <iostream>
#include <tuple>
struct FakeHud {
    inline static std::map<std::tuple<int,std::string,std::string>,std::string> text;
    inline static std::map<std::tuple<int,std::string,std::string>,bool> classes;
    bool live=false;
    explicit FakeHud(std::string p){assert(p=="panorama/layout/custom_game/rtv_hud/browser_scroll.xml");}
    void init(ISchemaSystem*){}
    CEntityInstance* get(){return live?reinterpret_cast<CEntityInstance*>(this):nullptr;}
    void ensure(){live=true;}void forget(){live=false;}void destroy(){live=false;}void capture(int,bool){}
    void set(std::string p,std::string v,int slot){setVariable(p,"text",v,slot);}
    void setVariable(std::string p,std::string n,std::string v,int slot){text[{slot,p,n}]=v;}
    void setClass(std::string p,std::string c,bool v,int slot){classes[{slot,p,c}]=v;}
};
int main(){
    BrowserUIBase<FakeHud> ui;
    std::vector<rtv::Map> maps;
    maps.push_back({"[T1] kz_easy","kz_easy","10"});
    for(int i=0;i<90;++i){auto n="kz_map_"+std::to_string(100+i);maps.push_back({std::string(i==0?"[T2][G] ":"[T2] ")+n,n,std::to_string(1000+i)});}
    auto text=[](int slot,std::string p,std::string name="text"){return FakeHud::text.at({slot,p,name});};
    auto cls=[](int slot,std::string p,std::string c){return FakeHud::classes.at({slot,p,c});};
    // A non-admin map command must not create a HUD or a nomination session.
    ui.open(1,101,false,true,"",0,maps,"");
    assert(!ui.entity()&&ui.sessions.empty()&&FakeHud::text.empty()&&FakeHud::classes.empty());
    ui.open(0,100,true,true,"",0,maps,"");
    assert(ui.sessions.at(0).canMap&&ui.sessions.at(0).mapMode);
    assert(!cls(0,"mode_map","collapsed")&&!cls(0,"mode_map_label","collapsed"));
    assert(text(0,"action_text")=="Change map");
    // Initially, only the first nonempty tier opens. The 90-map second group
    // exists in full, with no pages, but its rows start collapsed.
    assert(ui.sessions.at(0).expanded==std::set<std::string>{"t1"});
    assert(ui.sessions.at(0).rows.size()==93);
    assert(!cls(0,"row_1","collapsed"));
    assert(cls(0,"row_3","collapsed")&&cls(0,"row_92","collapsed"));
    assert(text(0,"row_2")=="[+] TIER 2 / 90 maps");
    ui.click(0,"row_92",1,maps,"");assert(ui.selectedByClick==-1);
    ui.click(0,"row_2",2,maps,"");
    assert(!cls(0,"row_3","collapsed")&&!cls(0,"row_92","collapsed"));
    assert(text(0,"row_92")=="[T2] kz_map_189");
    assert(text(0,"row_3","global")=="GLOBAL");
    ui.click(0,"row_92",3,maps,"");assert(ui.selectedByClick==90);
    assert(text(0,"selected_detail")=="Workshop ID: 1089");
    assert(ui.click(0,"confirm",4,maps,""));
    ui.click(0,"row_2",5,maps,"");assert(ui.sessions.at(0).selected==-1);
    assert(!ui.click(0,"confirm",6,maps,""));
    // Removed pagination commands do nothing.
    const auto bindings=ui.sessions.at(0).rows;
    assert(!ui.click(0,"next_page",7,maps,""));assert(ui.sessions.at(0).rows==bindings);
    ui.open(1,101,false,false,"map_189",8,maps,"");
    assert(ui.sessions.at(1).expanded==std::set<std::string>{"t2"});
    assert(text(1,"row_1")=="[T2] kz_map_189");
    assert(text(1,"row_1","global").empty());
    assert(text(0,"row_1")=="[T1] kz_easy");
    // Rejected map commands must also preserve an existing nomination HUD.
    ui.open(1,101,false,true,"easy",8.5,maps,"");
    assert(!ui.sessions.at(1).mapMode&&ui.sessions.at(1).query=="map_189");
    assert(ui.sessions.at(1).expires==8+ui.timeout);
    assert(text(1,"browser_title")=="NOMINATE A MAP"&&cls(1,"mode_map","collapsed"));
    assert(cls(1,"mode_map_label","collapsed")&&text(1,"action_text")=="Nominate");
    assert(!cls(0,"mode_map","collapsed")); // The admin's view remains independent.
    ui.click(1,"mode_map",9,maps,"");assert(!ui.sessions.at(1).mapMode);
    ui.click(1,"clear_search",10,maps,"");
    assert(ui.sessions.at(1).expanded==std::set<std::string>{"t1"});
    assert(cls(1,"row_92","collapsed"));
    // A shorter search/reopen clears stale labels, badges and hit targets.
    ui.close(0);ui.open(0,100,true,true,"map_189",11,maps,"");
    assert(cls(0,"row_92","collapsed")&&text(0,"row_92").empty());
    assert(text(0,"row_3","global").empty());
    ui.click(0,"row_92",12,maps,"");assert(ui.selectedByClick==-1);
    ui.closeAll();assert(ui.sessions.empty());
    assert(cls(0,"mode_map","collapsed")&&cls(0,"mode_map_label","collapsed"));
    assert(text(0,"action_text")=="Nominate"&&text(0,"browser_title")=="NOMINATE A MAP");
    // A non-admin reusing an admin slot sees only nomination controls.
    ui.open(0,102,false,false,"",12,maps,"");
    assert(cls(0,"mode_map","collapsed")&&cls(0,"mode_map_label","collapsed"));
    assert(!ui.click(0,"mode_map",12.2,maps,""));
    assert(!ui.sessions.at(0).mapMode&&text(0,"action_text")=="Nominate");
    // A permission downgrade clears map controls on the next render too.
    ui.open(0,100,true,true,"",12.4,maps,"");
    ui.sessions.at(0).canMap=false;ui.render(0,maps,"");
    assert(!ui.sessions.at(0).mapMode);
    assert(cls(0,"mode_map","collapsed")&&cls(0,"mode_map_label","collapsed"));
    assert(text(0,"action_text")=="Nominate"&&text(0,"browser_title")=="NOMINATE A MAP");
    ui.closeAll();
    auto oldRevision=rtv::maplistRevision(maps);
    maps={{"[T3][G] kz_new","kz_new","3781516256"},{"[T6] kz_new2","kz_new2","3776801864"}};
    assert(rtv::maplistRevision(maps)!=oldRevision);
    ui.open(0,100,true,true,"",13,maps,"");
    assert(ui.sessions.at(0).expanded==std::set<std::string>{"t3"});
    assert(text(0,"row_1")=="[T3] kz_new");
    assert(cls(0,"row_3","collapsed"));
    ui.click(0,"row_1",14,maps,"");assert(ui.selectedByClick==0);
    assert(text(0,"selected_detail")=="Workshop ID: 3781516256");
    assert(!ui.click(0,"confirm",15,maps,"kz_new"));
    // All 900 maps are sent at once, including the last row, within native limits.
    std::vector<rtv::Map> many;
    for(int i=0;i<900;++i)many.push_back({"[T2] m_"+std::to_string(i),"m_"+std::to_string(i),std::to_string(i+1)});
    ui.closeAll();ui.open(0,100,true,true,"",16,many,"");
    assert(ui.sessions.at(0).rows.size()==901&&ui.sessions.at(0).visible.size()==900);
    assert(!cls(0,"row_900","collapsed"));
    ui.click(0,"row_900",17,many,"");assert(ui.selectedByClick>=0);
    // Never silently truncate a list exceeding the native layout budget.
    for(int i=900;i<1000;++i)many.push_back({"[T2] m_"+std::to_string(i),"m_"+std::to_string(i),std::to_string(i+1)});
    bool rejected=false;try{ui.open(2,102,false,false,"",18,many,"");}catch(const std::runtime_error&){rejected=true;}
    assert(rejected&&!ui.sessions.count(2));
    std::cout<<"Full scroll UI: first tier only, all 90/900 maps, tier toggles, search reset, two viewers, badges and selection passed\n";
}
