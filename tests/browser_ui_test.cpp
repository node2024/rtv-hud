#include "browser_ui.h"
#include <cassert>
#include <iostream>
#include <tuple>

// Model persistent network state, not just the last render's writes.
struct FakeHud {
    using Key=std::tuple<int,std::string,std::string>;
    struct State {
        std::map<Key,std::string> text;
        std::map<Key,bool> classes;
        std::set<std::string> panels;
        bool capture=false;
    };
    inline static int next=0;
    inline static std::map<int,State> live;
    inline static bool failWrite=false;
    int id=0;
    explicit FakeHud(std::string p){assert(p=="panorama/layout/custom_game/rtv_hud/browser_scroll.xml");}
    void init(ISchemaSystem*){}
    CEntityInstance* get(){return id?reinterpret_cast<CEntityInstance*>(static_cast<uintptr_t>(id)):nullptr;}
    void ensure(){if(!id){id=++next;live.emplace(id,State{});}}
    void destroy(){assert(id&&live.erase(id)==1);id=0;}
    void capture(int,bool enabled){live.at(id).capture=enabled;}
    void set(std::string p,std::string v,int slot){setVariable(p,"text",v,slot);}
    void setVariable(std::string p,std::string n,std::string v,int slot){
        if(failWrite)throw std::runtime_error("test write failure");
        auto& s=live.at(id);s.panels.insert(p);s.text[{slot,p,n}]=v;
    }
    void setClass(std::string p,std::string c,bool v,int slot){
        auto& s=live.at(id);s.panels.insert(p);s.classes[{slot,p,c}]=v;
    }
};

int main(){
    BrowserUIBase<FakeHud> ui;
    std::vector<rtv::Map> maps{{"[T1] kz_easy","kz_easy","10"}};
    for(int i=0;i<90;++i){auto n="kz_map_"+std::to_string(100+i);maps.push_back({std::string(i==0?"[T2][G] ":"[T2] ")+n,n,std::to_string(1000+i)});}
    auto state=[&](int slot)->FakeHud::State&{
        return FakeHud::live.at(static_cast<int>(reinterpret_cast<uintptr_t>(ui.entity(slot))));
    };
    auto text=[&](int slot,const std::string& panel,const std::string& name="text"){
        auto& values=state(slot).text;auto found=values.find({slot,panel,name});
        return found==values.end()?std::string{}:found->second;
    };
    auto collapsed=[&](int slot,const std::string& panel){
        auto& values=state(slot).classes;auto found=values.find({slot,panel,"collapsed"});
        return found==values.end()||found->second;
    };
    auto bound=[&](int slot){
        const auto& s=state(slot);
        assert(ui.sessions.at(slot).rows.size()<=rtv::BrowserPageSize+2);
        assert(s.panels.size()<=rtv::BrowserPageSize+14);
        assert(s.text.size()<=2*(rtv::BrowserPageSize+2)+6);
        assert(s.classes.size()<=3*(rtv::BrowserPageSize+2)+8);
        for(const auto& panel:s.panels)if(panel.rfind("row_",0)==0)
            assert(std::stoul(panel.substr(4))<rtv::BrowserPageSize+2);
    };
    double now=1;
    auto click=[&](int slot,const std::string& button){now+=1;return ui.click(slot,button,now,maps,"");};
    auto rowFor=[&](int slot,int index){
        const auto& rows=ui.sessions.at(slot).rows;
        auto found=std::find(rows.begin(),rows.end(),index);assert(found!=rows.end());
        return "row_"+std::to_string(found-rows.begin());
    };

    ui.open(1,101,false,true,"",now,maps,"");
    assert(!ui.entity(1)&&ui.sessions.empty()&&FakeHud::live.empty());
    ui.open(0,100,true,true,"",now,maps,"");
    assert(ui.sessions.at(0).mapMode&&!collapsed(0,"mode_map")&&!collapsed(0,"mode_map_label"));
    assert(text(0,"action_text")=="Change map"&&state(0).capture);
    assert(ui.sessions.at(0).rows.size()==3); // T1 header/map, collapsed T2 header.
    assert(text(0,"row_2")=="[+] TIER 2 / 90 maps");
    assert(text(0,"row_3").empty()); // Collapsed data is never networked.
    click(0,"row_92");assert(ui.selectedByClick==-1);
    click(0,"row_2");bound(0);
    assert(ui.sessions.at(0).pages==4&&ui.sessions.at(0).rows.size()==25);
    assert(text(0,"row_3","global")=="GLOBAL");
    click(0,"row_3");assert(ui.selectedByClick==1);
    assert(click(0,"confirm"));
    click(0,rowFor(0,rtv::BrowserNextPage));
    assert(ui.sessions.at(0).page==1&&ui.sessions.at(0).selected==-1);
    assert(!click(0,"confirm"));
    click(0,rowFor(0,rtv::BrowserPreviousPage));assert(ui.sessions.at(0).page==0);

    ui.open(1,101,false,false,"map_189",now,maps,"");
    assert(ui.entity(0)!=ui.entity(1));
    assert(ui.visibleTo(0,0,100)&&!ui.visibleTo(0,1,100)&&!ui.visibleTo(0,0,101));
    assert(!ui.visibleTo(0,0,0)&&!ui.visibleTo(0,63,100)); // Disconnect/spectator/late join.
    assert(collapsed(1,"mode_map")&&collapsed(1,"mode_map_label"));
    assert(text(1,"action_text")=="Nominate"&&text(1,"row_1")=="[T2] kz_map_189");
    ui.open(1,101,false,true,"easy",now,maps,"");
    assert(ui.sessions.at(1).query=="map_189");
    click(1,"mode_map");assert(!ui.sessions.at(1).mapMode);
    assert(!collapsed(0,"mode_map"));
    click(1,"clear_search");assert(ui.sessions.at(1).page==0&&ui.sessions.at(1).rows.size()==3);

    // Search/reopen reuses bounded IDs and clears stale badges/hit targets.
    ui.open(0,100,true,true,"map_189",now,maps,"");
    assert(collapsed(0,"row_24")&&text(0,"row_24").empty()&&text(0,"row_3","global").empty());
    click(0,"row_24");assert(ui.selectedByClick==-1);
    click(0,"row_1");assert(ui.selectedByClick==90);
    now+=1;assert(!ui.click(0,"confirm",now,maps,"kz_map_189"));
    auto* old=ui.entity(0);ui.close(0);
    assert(!ui.entity(0)&&ui.entity(1)&&FakeHud::live.size()==1);
    ui.open(0,102,false,false,"",now,maps,"");
    assert(ui.entity(0)!=old&&collapsed(0,"mode_map")&&text(0,"action_text")=="Nominate");
    old=ui.entity(0);ui.open(0,103,true,true,"",now,maps,"");
    assert(ui.entity(0)!=old&&FakeHud::live.size()==2); // Slot ownership changed.
    ui.sessions.at(0).canMap=false;ui.render(0,maps,"");
    assert(!ui.sessions.at(0).mapMode&&collapsed(0,"mode_map_label")&&text(0,"action_text")=="Nominate");
    ui.closeAll();assert(ui.sessions.empty()&&FakeHud::live.empty());

    // Visit every map without accumulating all pages in replicated state.
    for(int total:{364,900,2050}){
        maps.clear();
        for(int i=0;i<total;++i){auto name="m_"+std::to_string(i);maps.push_back({"[T2][G] "+std::string(100,'x')+" "+name,name,std::to_string(i+1)});}
        ui.open(0,100,true,true,"",now,maps,"");
        std::set<int> seen;
        while(true){
            bound(0);
            const auto visible=ui.sessions.at(0).visible;
            for(int index:visible){
                assert(seen.insert(index).second);
                click(0,rowFor(0,index));assert(ui.selectedByClick==index&&click(0,"confirm"));
            }
            if(ui.sessions.at(0).page+1==ui.sessions.at(0).pages)break;
            click(0,rowFor(0,rtv::BrowserNextPage));
        }
        assert(seen.size()==static_cast<size_t>(total));
        click(0,"collapse_all");assert(ui.sessions.at(0).rows.size()==1&&ui.sessions.at(0).selected==-1);
        click(0,"expand_all");assert(ui.sessions.at(0).page==0);bound(0);
        ui.closeAll();assert(FakeHud::live.empty());
    }
    // 64 simultaneous users never share an entity or receive another page.
    for(int slot=0;slot<64;++slot)ui.open(slot,100+slot,false,false,"",now,maps,"");
    assert(FakeHud::live.size()==64);
    for(int owner=0;owner<64;++owner){
        bound(owner);
        for(int recipient=0;recipient<64;++recipient)
            assert(ui.visibleTo(owner,recipient,100+owner)==(owner==recipient));
    }
    ui.destroy();assert(FakeHud::live.empty()&&ui.sessions.empty());
    FakeHud::failWrite=true;
    bool failed=false;try{ui.open(0,100,false,false,"",now,maps,"");}catch(const std::runtime_error&){failed=true;}
    assert(failed&&FakeHud::live.empty()&&ui.sessions.empty());FakeHud::failWrite=false;
    ui.open(0,100,false,false,"no_match",now,maps,"");
    assert(ui.sessions.at(0).pages==1&&ui.sessions.at(0).rows.empty()&&!click(0,"confirm"));
    FakeHud::live.clear();ui.forget();assert(!ui.entity(0)&&ui.sessions.empty());
    std::cout<<"Bounded browser state, complete pagination, 64 isolated viewers, cleanup and admin controls passed\n";
}
