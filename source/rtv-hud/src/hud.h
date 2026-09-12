#pragma once
#include <schemasystem/schemasystem.h>
#include <entity2/entityinstance.h>
#include <entity2/entitykeyvalues.h>
#include <tier1/utlstring.h>
#include <link.h>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Linux signatures from cs2kz v0.0.167 gamedata. Resolve exactly one executable match.
inline void* signature(const std::vector<int>& pattern) {
    struct Search { const std::vector<int>& p; void* address = nullptr; int count = 0; } s{pattern};
    dl_iterate_phdr([](dl_phdr_info* info, size_t, void* data) {
        auto& s = *static_cast<Search*>(data);
        std::string name = info->dlpi_name;
        if (name.size() < 12 || name.substr(name.size() - 12) != "libserver.so") return 0;
        for (int j = 0; j < info->dlpi_phnum; ++j) {
            auto& h = info->dlpi_phdr[j];
            if (h.p_type != PT_LOAD || !(h.p_flags & PF_X) || h.p_memsz < s.p.size()) continue;
            auto* base = reinterpret_cast<unsigned char*>(info->dlpi_addr + h.p_vaddr);
            for (size_t k = 0; k <= h.p_memsz - s.p.size(); ++k) {
                size_t n = 0;
                while (n < s.p.size() && (s.p[n] < 0 || base[k+n] == s.p[n])) ++n;
                if (n == s.p.size()) { s.address = base + k; ++s.count; }
            }
        }
        return 0;
    }, &s);
    if (s.count != 1) throw std::runtime_error("server signature is missing or ambiguous; update gamedata");
    return s.address;
}

struct Field {
    int offset = 0;
    SchemaCollectionManipulatorFn_t manipulate = nullptr;
    Field() = default;
    Field(ISchemaSystem* system, const char* cls, const char* member, bool collection = false) {
        auto* scope = system->FindTypeScopeForModule("libserver.so");
        auto* c = scope ? scope->FindDeclaredClass(cls).Get() : nullptr;
        if (!c) throw std::runtime_error(std::string("missing schema class: ") + cls);
        for (int i = 0; i < c->m_nFieldCount; ++i) {
            auto& f = c->m_pFields[i];
            if (std::strcmp(f.m_pszName, member)) continue;
            offset = f.m_nSingleInheritanceOffset;
            if (offset < 0) break;
            if (collection) {
                auto* t = f.m_pType;
                if (!t || t->m_eTypeCategory != SCHEMA_TYPE_ATOMIC || t->m_eAtomicCategory != SCHEMA_ATOMIC_COLLECTION_OF_T) break;
                manipulate = static_cast<CSchemaType_Atomic_CollectionOfT*>(t)->m_pfnManipulator;
                if (!manipulate) break;
            }
            return;
        }
        throw std::runtime_error(std::string("missing/invalid schema field: ") + cls + "::" + member);
    }
    void* at(void* base) const { return static_cast<char*>(base) + offset; }
    template<class T> T& value(void* base) const { return *static_cast<T*>(at(base)); }
    int count(void* base) const { return static_cast<int>(reinterpret_cast<intptr_t>(manipulate(SCHEMA_COLLECTION_MANIPULATOR_ACTION_GET_COUNT, at(base), 0, 0))); }
    void* element(void* base, int i) const { return manipulate(SCHEMA_COLLECTION_MANIPULATOR_ACTION_GET_ELEMENT, at(base), i, 0); }
    void* append(void* base) const {
        int n = count(base);
        manipulate(SCHEMA_COLLECTION_MANIPULATOR_ACTION_SET_COUNT, at(base), n + 1, 0);
        if (count(base) != n + 1) throw std::runtime_error("schema collection growth failed");
        return element(base, n);
    }
};

class Hud {
    using Create = CEntityInstance* (*)(const char*, int);
    using Spawn = void (*)(CEntityInstance*, CEntityKeyValues*);
    using Remove = void (*)(CEntityInstance*);
    Create create = nullptr; Spawn spawn = nullptr; Remove remove = nullptr;
    Field state, panels, names, variables, panelIndex, nameIndex, value, isSet;
    Field playerStates, playerSlot, classNames, classes, classPanel, classIndex, classStatus, inputCapture;
    std::string layoutPath;
    CEntityHandle handle;
    std::map<std::string, std::string> cache;
    std::map<int, int> highlights;
    std::map<std::string, bool> classCache;
    void* layoutState(CEntityInstance* entity, int slot) {
        if (slot == -1) return state.at(entity);
        if (slot < 0 || slot >= playerStates.count(entity)) throw std::runtime_error("HUD player state missing");
        void* s = playerStates.element(entity, slot);
        if (!s) throw std::runtime_error("HUD player state is null");
        playerSlot.value<CPlayerSlot>(s) = CPlayerSlot(slot);
        return s;
    }
    static void notifyState(void* s) {
        NetworkStateChangedData changed(true);
        reinterpret_cast<void (*)(void*, const NetworkStateChangedData*)>((*reinterpret_cast<void***>(s))[1])(s, &changed);
    }
    int intern(CEntityInstance* entity, const Field& field, const char* text) {
        int n = field.count(entity);
        for (int i = 0; i < n; ++i)
            if (!std::strcmp(static_cast<CUtlString*>(field.element(entity, i))->Get(), text)) return i;
        if (n >= 1024) throw std::runtime_error("HUD interned string limit reached");
        *static_cast<CUtlString*>(field.append(entity)) = text;
        entity->NetworkStateChanged(NetworkStateChangedData(true));
        return n;
    }
public:
    explicit Hud(std::string path = "panorama/layout/custom_game/rtv_hud/vote.xml") : layoutPath(std::move(path)) {}
    void init(ISchemaSystem* schema) {
        create = reinterpret_cast<Create>(signature({0x48,0x8d,0x05,-1,-1,-1,-1,0x55,0x48,0x89,0xfa}));
        spawn = reinterpret_cast<Spawn>(signature({0x48,0x85,0xff,0x74,-1,0x55,0x48,0x89,0xe5,0x41,0x55,0x41,0x54,0x49,0x89,0xfc}));
        remove = reinterpret_cast<Remove>(signature({0x48,0x89,0xfe,0x48,0x85,0xff,0x74,-1,0x48,0x8d,0x05,-1,-1,-1,-1,0x48}));
        state = Field(schema, "CCSCustomHudLayout", "m_globalLayoutState");
        panels = Field(schema, "CCSCustomHudLayout", "m_vecPanelIds", true);
        names = Field(schema, "CCSCustomHudLayout", "m_vecDialogVariableNames", true);
        variables = Field(schema, "CCSCustomHudLayoutState", "m_vecDialogVariableStrings", true);
        panelIndex = Field(schema, "HUDPanelDialogVariableString_t", "m_nPanelIdIndex");
        nameIndex = Field(schema, "HUDPanelDialogVariableString_t", "m_nDialogVariableIndex");
        value = Field(schema, "HUDPanelDialogVariableString_t", "m_sValue");
        isSet = Field(schema, "HUDPanelDialogVariableString_t", "m_bIsSet");
        playerStates = Field(schema, "CCSCustomHudLayout", "m_vecPlayerLayoutStates", true);
        playerSlot = Field(schema, "CCSCustomHudLayoutState", "m_playerSlot");
        classNames = Field(schema, "CCSCustomHudLayout", "m_vecClassNames", true);
        classes = Field(schema, "CCSCustomHudLayoutState", "m_vecHasClasses", true);
        classPanel = Field(schema, "HUDPanelHasClass_t", "m_nPanelIdIndex");
        classIndex = Field(schema, "HUDPanelHasClass_t", "m_nClassNameIndex");
        classStatus = Field(schema, "HUDPanelHasClass_t", "m_eClassStatus");
        inputCapture = Field(schema, "CCSCustomHudLayoutState", "m_bInputCaptureEnabled");
    }
    CEntityInstance* get() { return handle.IsValid() && GameEntitySystem() ? GameEntitySystem()->GetEntityInstance(handle) : nullptr; }
    void ensure() {
        if (get()) return;
        auto* entity = create("custom_hud_layout", -1);
        if (!entity) throw std::runtime_error("cannot create custom_hud_layout");
        handle = entity->GetRefEHandle(); cache.clear(); highlights.clear(); classCache.clear();
        auto* kv = new CEntityKeyValues();
        kv->SetString("layout", layoutPath.c_str());
        kv->SetString("targetname", "rtv_hud_prototype");
        spawn(entity, kv);
    }
    void forget() { handle = CEntityHandle(); cache.clear(); highlights.clear(); classCache.clear(); }
    void destroy() { if (auto* entity = get()) remove(entity); forget(); }
    void capture(int slot, bool enabled) {
        auto* entity = get();
        if (!entity) return;
        auto* s = layoutState(entity, slot);
        if (inputCapture.value<bool>(s) == enabled) return;
        inputCapture.value<bool>(s) = enabled;
        notifyState(s);
    }
    void setClass(const std::string& panel, const std::string& name, bool enabled, int slot = -1) {
        const auto key = std::to_string(slot) + ":" + panel + ":" + name;
        const auto old = classCache.find(key);
        if (old != classCache.end() && old->second == enabled) return;
        auto* entity = get();
        if (!entity) throw std::runtime_error("HUD entity disappeared");
        const int p = intern(entity, panels, panel.c_str()), c = intern(entity, classNames, name.c_str());
        auto* s = layoutState(entity, slot);
        void* entry = nullptr;
        for (int j = 0; j < classes.count(s); ++j) {
            auto* candidate = classes.element(s, j);
            if (classPanel.value<uint16>(candidate) == p && classIndex.value<uint16>(candidate) == c) { entry = candidate; break; }
        }
        if (!entry) entry = classes.append(s);
        if (!entry) throw std::runtime_error("HUD class entry missing");
        classPanel.value<uint16>(entry) = p; classIndex.value<uint16>(entry) = c;
        classStatus.value<uint32>(entry) = enabled ? 1 : 0;
        notifyState(s); classCache[key] = enabled;
    }
    void highlight(int slot, int selected) {
        if (slot < 0 || slot >= 64 || selected < -1 || selected >= 6) return;
        const auto old = highlights.find(slot);
        if (old != highlights.end() && old->second == selected) return;
        if (old == highlights.end() && selected == -1) return;
        auto* entity = get();
        if (!entity) throw std::runtime_error("HUD entity disappeared");
        if (slot >= playerStates.count(entity)) throw std::runtime_error("HUD player state missing");
        void* s = playerStates.element(entity, slot);
        if (!s) throw std::runtime_error("HUD player state is null");
        playerSlot.value<CPlayerSlot>(s) = CPlayerSlot(slot);
        const int c = intern(entity, classNames, "selected");
        // Apply directly to each Label: parent class changes do not reliably
        // propagate to child styles in CS2. Explicit false states avoid leftovers.
        for (int i = 0; i < 6; ++i) {
            const int p = intern(entity, panels, ("choice" + std::to_string(i + 1)).c_str());
            void* entry = nullptr;
            for (int j = 0; j < classes.count(s); ++j) {
                auto* candidate = classes.element(s, j);
                if (classPanel.value<uint16>(candidate) == p && classIndex.value<uint16>(candidate) == c) {
                    entry = candidate; break;
                }
            }
            if (!entry) entry = classes.append(s);
            if (!entry) throw std::runtime_error("HUD class entry missing");
            classPanel.value<uint16>(entry) = p;
            classIndex.value<uint16>(entry) = c;
            classStatus.value<uint32>(entry) = i == selected ? 1 : 0;
        }
        NetworkStateChangedData changed(true);
        reinterpret_cast<void (*)(void*, const NetworkStateChangedData*)>((*reinterpret_cast<void***>(s))[1])(s, &changed);
        highlights[slot] = selected;
    }
    // A personal entity uses global state, so spectator target state cannot
    // override the owner's selection. Existing choice IDs/CSS are sufficient.
    void highlightGlobal(int selected) {
        for (int i = 0; i < 6; ++i)
            setClass("choice" + std::to_string(i + 1), "selected", i == selected);
    }
    void set(const std::string& panel, const std::string& text, int slot = -1) {
        setVariable(panel,"text",text,slot);
    }
    void setVariable(const std::string& panel, const std::string& variable, const std::string& text, int slot = -1) {
        auto* entity = get();
        if (!entity) throw std::runtime_error("HUD entity disappeared");
        const auto key = std::to_string(slot) + ":" + panel + ":" + variable;
        auto old = cache.find(key);
        if (old != cache.end() && old->second == text) return;
        int p = intern(entity, panels, panel.c_str()), v = intern(entity, names, variable.c_str());
        void* s = layoutState(entity, slot); void* entry = nullptr;
        for (int i = 0; i < variables.count(s); ++i) {
            void* e = variables.element(s, i);
            if (panelIndex.value<uint16>(e) == p && nameIndex.value<uint16>(e) == v) { entry = e; break; }
        }
        if (!entry) entry = variables.append(s);
        panelIndex.value<uint16>(entry) = p; nameIndex.value<uint16>(entry) = v;
        value.value<CUtlString>(entry) = text.c_str(); isSet.value<bool>(entry) = true;
        NetworkStateChangedData changed(true);
        // CCSCustomHudLayoutState::NetworkStateChanged is slot 1 (Linux, v0.0.167).
        reinterpret_cast<void (*)(void*, const NetworkStateChangedData*)>((*reinterpret_cast<void***>(s))[1])(s, &changed);
        cache[key] = text;
    }
};
