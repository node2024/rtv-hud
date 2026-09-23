#pragma once
#include "vote.h"
#include <cctype>
#include <optional>
#include <array>

namespace rtv {
// Fingerprint the loaded data, not the current disk file: the displayed revision
// changes only when the maps actually used by the browser have changed.
inline std::string maplistRevision(const std::vector<Map>& maps) {
    uint32_t hash=2166136261u;
    auto add=[&](const std::string& text){for(unsigned char c:text){hash^=c;hash*=16777619u;}};
    for(const auto& map:maps){add(map.label);add(":");add(map.workshop);add("\n");}
    const char* digits="0123456789abcdef";
    std::string result(8,'0');
    for(int i=7;i>=0;--i){result[i]=digits[hash&15];hash>>=4;}
    return result;
}
inline std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
inline std::string tier(const Map& map) {
    auto s = lower(map.label);
    for (int i = 1; i <= 8; ++i) if (s.find("[t" + std::to_string(i) + "]") != std::string::npos) return "t" + std::to_string(i);
    return "unknown";
}
inline bool matches(const Map& map, const std::string& query) {
    std::istringstream in(lower(query)); std::string token;
    while (in >> token) {
        char level = 0;
        if (token.size() == 2 && token[0] == 't') level = token[1];
        if (token.size() == 2 && token[1] == 't') level = token[0];
        if (level == '?' || (level >= '1' && level <= '8')) {
            if (tier(map) != (level == '?' ? "unknown" : std::string("t") + level)) return false;
        } else if (lower(map.label).find(token) == std::string::npos) return false;
    }
    return true;
}
inline bool isCurrent(const Map& map, const std::string& current) {
    return map.name == current || "workshop/" + map.workshop + "/" + map.name == current;
}
inline std::optional<std::string> decodeHex(const std::string& input) {
    if (input == "-") return std::string{};
    if (input.size() > 256 || input.size() % 2) return {};
    auto nibble = [](char c) { if (c >= '0' && c <= '9') return c-'0'; if (c >= 'A' && c <= 'F') return c-'A'+10; if (c >= 'a' && c <= 'f') return c-'a'+10; return -1; };
    std::string out;
    for (size_t i=0; i<input.size(); i+=2) {
        int a=nibble(input[i]), b=nibble(input[i+1]); if (a<0 || b<0) return {};
        const auto c = static_cast<unsigned char>(a*16+b);
        if (c < 32 || c == 127) return {};
        out += static_cast<char>(c);
    }
    return out;
}
class Nominations {
    std::vector<std::pair<uint64_t, std::string>> entries;
public:
    void clear() { entries.clear(); }
    void disconnect(uint64_t id) { entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const auto& e){return e.first==id;}),entries.end()); }
    std::string nominate(uint64_t id, const Map& map, const std::string& current, bool voting) {
        if (!id || voting) return "Nominations are closed during a vote.";
        if (isCurrent(map,current)) return "The current map cannot be nominated.";
        for (const auto& e:entries) if (e.second==map.workshop && e.first!=id) return "That map is already nominated.";
        for (auto& e:entries) if (e.first==id) {e.second=map.workshop;return {};}
        if(entries.size()>=6) return "All six nomination slots are filled.";
        entries.emplace_back(id,map.workshop); return {};
    }
    std::vector<Map> candidates(const std::vector<Map>& shuffled, const std::string& current) const {
        std::vector<Map> out; std::set<std::string> used;
        auto add=[&](const Map& m) {if(out.size()<6 && !isCurrent(m,current) && used.insert(m.workshop).second)out.push_back(m);};
        for(const auto& e:entries)for(const auto& m:shuffled)if(m.workshop==e.second)add(m);
        for(const auto& m:shuffled)add(m);
        return out;
    }
};
struct BrowserSession {
    uint64_t steam = 0;
    bool canMap = false, mapMode = false;
    int selected = -1;
    std::string query, result;
    std::set<std::string> expanded{"t1"};
    std::set<int> visible;
    double expires = 0, nextClick = 0;
    std::vector<int> rows;
    std::vector<size_t> rowGroups;
    size_t page = 0, pages = 1;
};
inline const std::array<std::string,9> tiers{"t1","t2","t3","t4","t5","t6","t7","t8","unknown"};
// Capacity of the existing Workshop asset, not a safe network payload budget.
constexpr size_t BrowserRowCapacity = 960;
// Networked state must stay small even when the asset has hundreds of slots.
constexpr size_t BrowserPageSize = 24;
static_assert(BrowserPageSize + 2 <= BrowserRowCapacity);
constexpr int BrowserPreviousPage = -2, BrowserNextPage = -3;
struct BrowserRow { int map = -1; size_t group = 0; };
struct BrowserList {
    std::vector<BrowserRow> rows;
    std::array<int,9> counts{};
    int total = 0;
};
inline BrowserList browserAll(const std::vector<Map>& maps, const std::string& query, const std::string& current) {
    BrowserList out; std::array<std::vector<int>,9> groups;
    for(size_t i=0;i<maps.size();++i) {
        if(isCurrent(maps[i],current)||!matches(maps[i],query))continue;
        auto t=tier(maps[i]);auto g=std::find(tiers.begin(),tiers.end(),t)-tiers.begin();
        groups[g].push_back(static_cast<int>(i));++out.counts[g];++out.total;
    }
    for(size_t g=0;g<groups.size();++g) {
        auto& group=groups[g];
        std::sort(group.begin(),group.end(),[&](int a,int b){
            const auto an=lower(maps[a].name),bn=lower(maps[b].name);
            return an==bn?maps[a].workshop<maps[b].workshop:an<bn;
        });
        if(group.empty())continue;
        out.rows.push_back({-1,g});
        for(int index:group)out.rows.push_back({index,g});
    }
    return out;
}
// [G] is mutable metadata; changing it must not change a map's row identity.
inline std::string browserLabelKey(const std::string& label) {
    std::string tags;
    size_t pos = 0;
    while (pos < label.size()) {
        while (pos < label.size() && std::isspace(static_cast<unsigned char>(label[pos]))) ++pos;
        if (pos == label.size() || label[pos] != '[') break;
        const auto end = label.find(']', pos);
        if (end == std::string::npos) break;
        const auto tag = label.substr(pos, end - pos + 1);
        if (tag != "[G]") tags += tag;
        pos = end + 1;
    }
    return tags.empty() ? label.substr(pos) : tags + " " + label.substr(pos);
}
inline std::set<std::string> firstExpandedTier(const BrowserList& list) {
    for(size_t g=0;g<tiers.size();++g)if(list.counts[g]>0)return {tiers[g]};
    return {};
}
// Global is an explicit leading maplist tag, not part of a map name.
inline bool hasGlobalFlag(const Map& map) {
    size_t pos = 0;
    while (pos < map.label.size()) {
        while (pos < map.label.size() && std::isspace(static_cast<unsigned char>(map.label[pos]))) ++pos;
        if (pos >= map.label.size() || map.label[pos] != '[') break;
        const auto end = map.label.find(']', pos);
        if (end == std::string::npos) break;
        if (map.label.substr(pos, end - pos + 1) == "[G]") return true;
        pos = end + 1;
    }
    return false;
}
// Wire-compatible subset of CCSUsrMsg_CustomHudClicked (message 390).
// Bound and validate before resolving any client-supplied entity handle.
inline bool parseClick(const void* data, size_t size, uint32_t& handle, std::string& button) {
    if(!data || size>256) return false;
    const auto* p=static_cast<const unsigned char*>(data);size_t i=0;bool gotHandle=false,gotButton=false;
    auto varint=[&](uint64_t& n) { n=0;for(int k=0;k<10 && i<size;++k){auto c=p[i++];if(k==9 && c>1)return false;n|=uint64_t(c&127)<<(7*k);if(!(c&128))return true;}return false;};
    while(i<size){uint64_t tag=0,n=0;if(!varint(tag)||!tag)return false;
        if(tag==8){if(!varint(n)||n>UINT32_MAX)return false;handle=static_cast<uint32_t>(n);gotHandle=true;}
        else if(tag==18){if(!varint(n)||n>64||n>size-i)return false;button.assign(reinterpret_cast<const char*>(p+i),n);i+=n;gotButton=true;}
        else {switch(tag&7){case 0:if(!varint(n))return false;break;case 1:if(size-i<8)return false;i+=8;break;case 2:if(!varint(n)||n>size-i)return false;i+=n;break;case 5:if(size-i<4)return false;i+=4;break;default:return false;}}
    }
    return gotHandle&&gotButton&&!button.empty()&&button.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")==std::string::npos;
}
}
