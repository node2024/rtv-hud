#pragma once
#include "browser.h"
#include <cstdint>
class ISchemaSystem;
class CEntityInstance;

template<class HudType>
class BrowserUIBase {
    HudType hud{"panorama/layout/custom_game/rtv_hud/browser_scroll.xml"};
    // Track per-user slots to clear stale contents on search/reopen. Only slots
    // ever used are interned, and there is no tier-specific multiplication.
    std::map<int,size_t> previousRows;
public:
    double timeout=120;
    int selectedByClick=-1;
    std::map<int,rtv::BrowserSession> sessions;
    void init(ISchemaSystem* schemas){hud.init(schemas);}
    CEntityInstance* entity(){return hud.get();}
    void forget(){sessions.clear();previousRows.clear();hud.forget();}
    void close(int slot){
        if(!sessions.count(slot))return;
        hud.capture(slot,false);
        if(hud.get()){
            hud.setClass("browser","collapsed",true,slot);
            // Clear admin controls before this slot is reused or reopened.
            hud.setClass("mode_map","collapsed",true,slot);
            hud.setClass("mode_map_label","collapsed",true,slot);
            hud.set("browser_title","NOMINATE A MAP",slot);
            hud.set("action_text","Nominate",slot);
        }
        sessions.erase(slot);
    }
    void closeAll(){while(!sessions.empty())close(sessions.begin()->first);}
    void destroy(){closeAll();hud.destroy();previousRows.clear();}
    void render(int slot,const std::vector<rtv::Map>& maps,const std::string& current){
        auto found=sessions.find(slot);if(found==sessions.end())return;auto& s=found->second;
        const auto list=rtv::browserAll(maps,s.query,current);
        if(list.rows.size()>rtv::BrowserRowCapacity)
            throw std::runtime_error("HUD row capacity exceeded (960 including tier headers); narrow the search. No results were truncated.");
        s.rows.clear();s.rowGroups.clear();s.visible.clear();
        for(const auto& row:list.rows){
            s.rows.push_back(row.map);s.rowGroups.push_back(row.group);
            if(row.map>=0&&s.expanded.count(rtv::tiers[row.group]))s.visible.insert(row.map);
        }
        if(!s.visible.count(s.selected))s.selected=-1;
        auto text=[&](const std::string& p,const std::string& v){hud.set(p,v,slot);};
        auto cls=[&](const std::string& p,const std::string& c,bool v){hud.setClass(p,c,v,slot);};
        if(!s.canMap)s.mapMode=false;
        text("browser_title",s.mapMode?"CHANGE MAP":"NOMINATE A MAP");
        text("action_text",s.mapMode?"Change map":"Nominate");
        text("search_summary","maplist.txt: "+std::to_string(maps.size())+" maps ["+rtv::maplistRevision(maps)+"] / "+(s.query.empty()?"All maps":"Search: "+s.query)+" / "+std::to_string(list.total)+" matches");
        cls("mode_map","collapsed",!s.canMap);cls("mode_map_label","collapsed",!s.canMap);
        cls("mode_nominate","active",!s.mapMode);cls("mode_map","active",s.mapMode);
        cls("empty_results","collapsed",list.total!=0);
        const size_t count=std::max(previousRows[slot],list.rows.size());
        for(size_t row=0;row<count;++row){
            const auto id="row_"+std::to_string(row);
            const bool present=row<list.rows.size();
            const bool header=present&&list.rows[row].map<0;
            const int index=present?list.rows[row].map:-1;
            const bool expanded=present&&s.expanded.count(rtv::tiers[list.rows[row].group]);
            cls(id,"collapsed",!present||(!header&&!expanded));
            cls(id,"tier",header);
            cls(id,"selected",index>=0&&index==s.selected);
            std::string label;
            if(header){
                const auto group=list.rows[row].group;const auto& key=rtv::tiers[group];
                label=std::string(expanded?"[-] ":"[+] ")+"TIER "+(key=="unknown"?"?":key.substr(1))+" / "+std::to_string(list.counts[group])+" maps";
            }else if(index>=0)label=rtv::browserLabelKey(maps[index].label);
            // Dialog variables on the button are inherited by its two labels.
            // Only the button needs an interned ID; metadata never lives in XML.
            text(id,label);
            hud.setVariable(id,"global",index>=0&&rtv::hasGlobalFlag(maps[index])?"GLOBAL":"",slot);
        }
        previousRows[slot]=count;
        text("selected_name",s.selected<0?"Select a map":rtv::browserLabelKey(maps[s.selected].label));
        text("selected_detail",s.selected<0?"Choose a row, then confirm.":"Workshop ID: "+maps[s.selected].workshop);
        cls("selected_global","global-visible",s.selected>=0&&rtv::hasGlobalFlag(maps[s.selected]));
        text("result",s.result);
    }
    void open(int slot,uint64_t steam,bool canMap,bool mapMode,const std::string& query,double now,const std::vector<rtv::Map>& maps,const std::string& current){
        // Reject unauthorized map commands without opening or replacing a HUD.
        if(mapMode&&!canMap)return;
        hud.ensure();
        rtv::BrowserSession s;s.steam=steam;s.canMap=canMap;s.mapMode=mapMode&&canMap;s.query=query;s.expires=now+timeout;
        s.expanded=rtv::firstExpandedTier(rtv::browserAll(maps,query,current));
        s.result="Choose a map, then confirm.";
        sessions[slot]=std::move(s);
        try{render(slot,maps,current);}catch(...){sessions.erase(slot);hud.capture(slot,false);hud.setClass("browser","collapsed",true,slot);throw;}
        hud.setClass("browser","collapsed",false,slot);hud.capture(slot,true);
    }
    bool click(int slot,const std::string& button,double now,const std::vector<rtv::Map>& maps,const std::string& current){
        selectedByClick=-1;
        auto found=sessions.find(slot);if(found==sessions.end())return false;auto& s=found->second;
        if(button=="close"){close(slot);return false;}
        if(now<s.nextClick)return false;
        s.nextClick=now+0.1;s.expires=now+timeout;
        if(button=="confirm"){
            if(s.selected>=0&&static_cast<size_t>(s.selected)<maps.size()&&s.visible.count(s.selected)&&!rtv::isCurrent(maps[s.selected],current))return true;
            s.result="Select a map first.";
        }else if(button=="mode_nominate"||button=="mode_map"){
            if(button=="mode_map"&&!s.canMap)return false;
            s.mapMode=button=="mode_map";s.result="Choose a map, then confirm.";
        }else if(button=="clear_search"){
            s.query.clear();s.selected=-1;s.expanded=rtv::firstExpandedTier(rtv::browserAll(maps,"",current));
        }else if(button=="expand_all")s.expanded=std::set<std::string>(rtv::tiers.begin(),rtv::tiers.end());
        else if(button=="collapse_all")s.expanded.clear();
        else if(button.rfind("row_",0)==0){
            const auto id=button.substr(4);
            if(id.empty()||id.size()>3||id.find_first_not_of("0123456789")!=std::string::npos)return false;
            const size_t row=static_cast<size_t>(std::stoi(id));
            if(row>=s.rows.size()||row>=rtv::BrowserRowCapacity)return false;
            const auto& key=rtv::tiers[s.rowGroups[row]];const int index=s.rows[row];
            if(index<0){if(s.expanded.count(key))s.expanded.erase(key);else s.expanded.insert(key);}
            else{
                if(static_cast<size_t>(index)>=maps.size()||!s.visible.count(index)||!s.expanded.count(key)||rtv::isCurrent(maps[index],current))return false;
                s.selected=index;s.result="Press the confirmation button to continue.";selectedByClick=index;
            }
        }else return false;
        render(slot,maps,current);return false;
    }
};
