#pragma once
#include "browser.h"
#include "personal_huds.h"
#include <cstdint>
class ISchemaSystem;
class CEntityInstance;

template<class HudType>
class BrowserUIBase {
    HudType prototype{"panorama/layout/custom_game/rtv_hud/browser_scroll.xml"};
    rtv::PersonalHuds<HudType> huds;
    // Reuse only a small page of row IDs; old pages never grow network state.
    std::map<int,size_t> previousRows;
public:
    double timeout=120;
    int selectedByClick=-1;
    std::map<int,rtv::BrowserSession> sessions;
    void init(ISchemaSystem* schemas){prototype.init(schemas);}
    CEntityInstance* entity(int slot){
        if(slot<0||slot>=64||!huds.entries[slot])return nullptr;
        return huds.entries[slot]->hud.get();
    }
    bool visibleTo(int owner,int recipient,uint64_t steam)const{return huds.visibleTo(owner,recipient,steam);}
    void forget(){sessions.clear();previousRows.clear();huds.forget();}
    void close(int slot){
        if(slot<0||slot>=64)return;
        if(huds.entries[slot])huds.entries[slot]->hud.capture(slot,false);
        huds.close(slot);
        previousRows.erase(slot);
        sessions.erase(slot);
    }
    void closeAll(){while(!sessions.empty())close(sessions.begin()->first);}
    void destroy(){closeAll();huds.clear();previousRows.clear();}
    void render(int slot,const std::vector<rtv::Map>& maps,const std::string& current){
        auto found=sessions.find(slot);if(found==sessions.end())return;auto& s=found->second;
        auto& hud=huds.open(slot,s.steam,prototype);
        const auto list=rtv::browserAll(maps,s.query,current);
        std::vector<rtv::BrowserRow> expandedRows;
        for(const auto& row:list.rows)
            if(row.map<0||s.expanded.count(rtv::tiers[row.group]))expandedRows.push_back(row);
        s.pages=std::max(size_t{1},(expandedRows.size()+rtv::BrowserPageSize-1)/rtv::BrowserPageSize);
        s.page=std::min(s.page,s.pages-1);
        s.rows.clear();s.rowGroups.clear();s.visible.clear();
        auto add=[&](const rtv::BrowserRow& row){
            s.rows.push_back(row.map);s.rowGroups.push_back(row.group);
            if(row.map>=0)s.visible.insert(row.map);
        };
        if(s.page>0)add({rtv::BrowserPreviousPage,0});
        const size_t begin=s.page*rtv::BrowserPageSize;
        for(size_t row=begin;row<std::min(begin+rtv::BrowserPageSize,expandedRows.size());++row)add(expandedRows[row]);
        if(s.page+1<s.pages)add({rtv::BrowserNextPage,0});
        if(!s.visible.count(s.selected))s.selected=-1;
        auto text=[&](const std::string& p,const std::string& v){hud.set(p,v,slot);};
        auto cls=[&](const std::string& p,const std::string& c,bool v){hud.setClass(p,c,v,slot);};
        if(!s.canMap)s.mapMode=false;
        text("browser_title",s.mapMode?"CHANGE MAP":"NOMINATE A MAP");
        text("action_text",s.mapMode?"Change map":"Nominate");
        text("search_summary",std::to_string(list.total)+" matches / Page "+std::to_string(s.page+1)+"/"+std::to_string(s.pages)+" / "+(s.query.empty()?"All maps":"Search: "+s.query));
        cls("mode_map","collapsed",!s.canMap);cls("mode_map_label","collapsed",!s.canMap);
        cls("mode_nominate","active",!s.mapMode);cls("mode_map","active",s.mapMode);
        cls("empty_results","collapsed",list.total!=0);
        const size_t count=std::max(previousRows[slot],s.rows.size());
        for(size_t row=0;row<count;++row){
            const auto id="row_"+std::to_string(row);
            const bool present=row<s.rows.size();
            const int index=present?s.rows[row]:-1;
            const bool header=present&&index==-1;
            const bool expanded=present&&s.expanded.count(rtv::tiers[s.rowGroups[row]]);
            cls(id,"collapsed",!present);
            cls(id,"tier",header);
            cls(id,"selected",index>=0&&index==s.selected);
            std::string label;
            if(header){
                const auto group=s.rowGroups[row];const auto& key=rtv::tiers[group];
                label=std::string(expanded?"[-] ":"[+] ")+"TIER "+(key=="unknown"?"?":key.substr(1))+" / "+std::to_string(list.counts[group])+" maps";
            }else if(index==rtv::BrowserPreviousPage)label="<< Previous page";
            else if(index==rtv::BrowserNextPage)label="Next page >>";
            else if(index>=0)label=rtv::browserLabelKey(maps[index].label);
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
        if(slot<0||slot>=64||!steam)throw std::invalid_argument("invalid browser owner");
        if(sessions.count(slot)&&sessions.at(slot).steam!=steam)close(slot);
        rtv::BrowserSession s;s.steam=steam;s.canMap=canMap;s.mapMode=mapMode&&canMap;s.query=query;s.expires=now+timeout;
        s.expanded=rtv::firstExpandedTier(rtv::browserAll(maps,query,current));
        s.result="Choose a map, then confirm.";
        sessions[slot]=std::move(s);
        try{
            render(slot,maps,current);
            auto& hud=huds.entries[slot]->hud;
            hud.setClass("browser","collapsed",false,slot);hud.capture(slot,true);
        }catch(...){close(slot);throw;}
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
            s.query.clear();s.selected=-1;s.page=0;s.expanded=rtv::firstExpandedTier(rtv::browserAll(maps,"",current));
        }else if(button=="expand_all"){s.expanded=std::set<std::string>(rtv::tiers.begin(),rtv::tiers.end());s.page=0;s.selected=-1;}
        else if(button=="collapse_all"){s.expanded.clear();s.page=0;s.selected=-1;}
        else if(button.rfind("row_",0)==0){
            const auto id=button.substr(4);
            if(id.empty()||id.size()>3||id.find_first_not_of("0123456789")!=std::string::npos)return false;
            const size_t row=static_cast<size_t>(std::stoi(id));
            if(row>=s.rows.size()||row>=rtv::BrowserRowCapacity)return false;
            const auto& key=rtv::tiers[s.rowGroups[row]];const int index=s.rows[row];
            if(index==rtv::BrowserPreviousPage){if(s.page==0)return false;--s.page;s.selected=-1;}
            else if(index==rtv::BrowserNextPage){if(s.page+1>=s.pages)return false;++s.page;s.selected=-1;}
            else if(index<0){if(s.expanded.count(key))s.expanded.erase(key);else s.expanded.insert(key);s.page=0;s.selected=-1;}
            else{
                if(static_cast<size_t>(index)>=maps.size()||!s.visible.count(index)||!s.expanded.count(key)||rtv::isCurrent(maps[index],current))return false;
                s.selected=index;s.result="Press the confirmation button to continue.";selectedByClick=index;
            }
        }else return false;
        render(slot,maps,current);return false;
    }
};
