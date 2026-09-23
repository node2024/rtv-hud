#include <ISmmPlugin.h>
#include <eiface.h>
#include <icvar.h>
#include <tier1/convar.h>
#include <entity2/entitysystem.h>
#include <ehandle.h>
#include <engine/igameeventsystem.h>
#include <networksystem/inetworkmessages.h>
#include "usermessages.pb.h"
#include "vote.h"
#include "navigation.h"
#include "hud.h"
#include "personal_huds.h"
#include "map_end.h"
#include <chrono>
#include <random>
#include <charconv>
#include <filesystem.h>
#include "browser_ui.h"
using BrowserUI = BrowserUIBase<Hud>;
#include "native_commands.h"
#include <iserver.h>
#include <array>

PLUGIN_GLOBALVARS();
static IVEngineServer* engine;
static ISource2Server* server;
static ISource2GameClients* clients;
static ISource2GameEntities* gameEntities;
static ISchemaSystem* schemas;
static INetworkMessages* networkMessages;
static IGameEventSystem* gameEvents;
static void* resourceService;
static IFileSystem* filesystem;
static INetworkServerService* networkServer;
// SDK entity handles and keyvalues resolve through this plugin-local accessor.
CGameEntitySystem* GameEntitySystem() {
    return resourceService ? *reinterpret_cast<CGameEntitySystem**>(static_cast<char*>(resourceService) + 80) : nullptr;
}
static double now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
static CConVar<bool> allowChange("rtvhud_allow_change", FCVAR_NONE, "Actually change to the winning workshop map (enabled by default)", true);
static CConVar<bool> endVoteEnabled("rtvhud_end_vote", FCVAR_NONE, "Automatically choose the next map at the time limit", true);
static CConVar<int32> endVoteLead("rtvhud_end_vote_lead", FCVAR_NONE, "Seconds before time limit to start voting (1-3600)", 60);
static CConVar<int32> requiredPercent("rtvhud_required_percent", FCVAR_NONE, "RTV agreement percentage (1-100, rounded up to whole players)", 100);
static CConVar<int32> initialDelay("rtvhud_initial_delay", FCVAR_NONE, "Seconds before RTV is available on a new map (0-86400)", 30);
static CConVar<int32> cooldown("rtvhud_cooldown", FCVAR_NONE, "Seconds before another RTV after a vote (0-86400)", 30);
static CConVar<int32> voteDuration("rtvhud_vote_duration", FCVAR_NONE, "Vote duration in seconds (1-3600)", 30);
static CConVar<int32> resultDuration("rtvhud_result_duration", FCVAR_NONE, "Result display / map change delay in seconds (0-300)", 5);
static CConVar<int32> browserTimeout("rtvhud_browser_timeout", FCVAR_NONE, "Browser inactivity timeout in seconds (1-3600)", 120);
static CConVar<CUtlString> moveSound("rtvhud_move_sound", FCVAR_NONE, "Sound event or vsnd path on selection; empty disables", "UIPanorama.round_report_odds_none");
static CConVar<CUtlString> selectSound("rtvhud_select_sound", FCVAR_NONE, "Sound event or vsnd path on confirmation; empty disables", "UIPanorama.round_report_odds_up");
// Linux CS2KZ v0.0.167: transmit bitset at +0, recipient slot at +576.

class RTVHud : public ISmmPlugin, public IMetamodListener {
    KHook::Virtual<ISource2GameEntities, void, CCheckTransmitInfo**, int, CBitVec<16384>&, CBitVec<16384>&, const Entity2Networkable_t**, const uint16*, int> transmitHook{&ISource2GameEntities::CheckTransmit, this, nullptr, &RTVHud::transmit};
    KHook::Virtual<ISource2Server, void, bool, bool, bool> frameHook{&ISource2Server::GameFrame, this, nullptr, &RTVHud::frame};
    KHook::Virtual<ISource2GameClients, void, CPlayerSlot, const CCommand&> commandHook{&ISource2GameClients::ClientCommand, this, &RTVHud::command, nullptr};
    KHook::Virtual<ISource2GameClients, void, CPlayerSlot, int, uint32, const void*> clickHook{&ISource2GameClients::ClientSvcUserMessage, this, &RTVHud::click, nullptr};
    KHook::Virtual<ICvar, void, ConCommandRef, const CCommandContext&, const CCommand&> dispatchHook{&ICvar::DispatchConCommand, this, &RTVHud::dispatch, nullptr};
    rtv::Vote vote;
    Hud hudPrototype;
    rtv::PersonalHuds<Hud> voteHuds;
    BrowserUI browser;
    rtv::Nominations nominations;
    rtv::MapTransition pendingMap;
    rtv::MapEnd mapEnd;
    CEntityHandle rulesHandle;
    Field rulesPointer, gameStartTime;
    bool mapActive = false;
    std::optional<double> timeLeft() {
        auto* game = networkServer->GetIGameServer();
        auto* globals = game ? game->GetGlobals() : nullptr;
        auto* system = GameEntitySystem();
        if (!mapActive || !globals || !system) return {};
        auto* proxy = rulesHandle.IsValid() ? system->GetEntityInstance(rulesHandle) : nullptr;
        if (!proxy) {
            for (int i = 0; i < 16384; ++i) {
                auto* entity = system->GetEntityInstance(CEntityIndex(i));
                if (entity && !std::strcmp(entity->GetClassname(), "cs_gamerules")) {
                    proxy = entity; rulesHandle = entity->GetRefEHandle(); break;
                }
            }
        }
        auto* rules = proxy ? rulesPointer.value<void*>(proxy) : nullptr;
        ConVarRefAbstract limit("mp_timelimit");
        if (!rules || !limit.IsValidRef()) return {};
        return rtv::mapSecondsLeft(limit.GetFloat(), globals->curtime, gameStartTime.value<float>(rules));
    }
    void publishNextMap() {
        if (!mapEnd.next) return;
        // The engine uses nextlevel; Workshop dispatch below uses the exact ID.
        if (allowChange.Get()) {
            ConVarRefAbstract nextlevel("nextlevel");
            if (!nextlevel.IsValidRef()) throw std::runtime_error("nextlevel convar unavailable");
            nextlevel.SetString(("workshop/" + mapEnd.next->workshop + "/" + mapEnd.next->name).c_str());
        }
        META_CONPRINTF("[RTV HUD] Next map: %s (workshop %s); change at time limit.\n",
            mapEnd.next->name.c_str(), mapEnd.next->workshop.c_str());
    }
    std::set<uint64_t> admins;
    std::map<uint64_t, double> lastOpen;
    std::string configDirectory;
    std::vector<rtv::Map> maps;
    std::set<uint64_t> humans;
    std::string currentMap;
    double nextFrame = 0;
    bool ready = false;
    Field pawnHandle, movementServices, movementButtons, playerName;
    std::set<uint64_t> closedVotes;
    std::map<uint64_t, double> lastExtend;
    struct PlayerInput {
        rtv::Navigation navigation;
        CEntityHandle pawn;
        double nextHint = 0;
    };
    std::map<uint64_t, PlayerInput> inputs;
    void syncSettings() {
        vote.configure({requiredPercent.Get(), initialDelay.Get(), cooldown.Get(), voteDuration.Get(), resultDuration.Get()});
        browser.timeout = std::clamp(static_cast<int>(browserTimeout.Get()), 1, 3600);
    }
    uint64_t authenticated(int slot) {
        if(slot<0||slot>=64)return 0;
        const auto* steam=engine->GetClientSteamID(CPlayerSlot(slot));
        return steam&&steam->BIndividualAccount()&&engine->IsClientFullyAuthenticated(CPlayerSlot(slot))?steam->ConvertToUint64():0;
    }
    bool browserAssets() {
        return filesystem->FileExists("panorama/layout/custom_game/rtv_hud/browser_scroll.vxml_c","GAME") &&
               filesystem->FileExists("panorama/styles/custom_game/rtv_hud/browser_scroll.vcss_c","GAME");
    }
    void maintainBrowser() {
        std::vector<int> close;
        for(const auto& [slot,s]:browser.sessions)if(s.steam!=authenticated(slot)||now()>=s.expires)close.push_back(slot);
        for(int slot:close)browser.close(slot);
        if(pendingMap.active() && now()>pendingMap.expires) {
            META_CONPRINTF("[RTV HUD] Map transition timed out; gate released.\n");
            pendingMap={}; mapEnd.changeQueued = false;
        }
    }
    void readAdminFile() {
        // Fail closed: stale privileges must not survive an invalid reload.
        admins.clear();
        std::ifstream file(configDirectory + "/admins.txt");
        if (!file) throw std::runtime_error("cannot open admins.txt; direct map changes disabled");
        admins = rtv::readAdmins(file);
    }
    void executeMapChange() {
        if (!pendingMap.active() || pendingMap.dispatched || now() < pendingMap.executeAt) return;
        const auto map = std::find_if(maps.begin(), maps.end(), [&](const auto& m) { return m.workshop == pendingMap.workshop; });
        if (!pendingMap.authorized(authenticated(pendingMap.slot), admins) || map == maps.end() ||
            rtv::isCurrent(*map, currentMap) || vote.phase != rtv::Phase::Idle) {
            META_CONPRINTF("[RTV HUD] Map transition cancelled: permission, player or map changed.\n");
            pendingMap = {}; return;
        }
        if (!allowChange.Get()) {
            chat("[Map] Dry run: " + map->label + " (map unchanged)");
            pendingMap = {}; return;
        }
        browser.closeAll();
        chat("[Map] Changing map to " + map->label);
        // Execute after the input callback returns. Block duplicate transitions
        // until map initialization or timeout.
        pendingMap.dispatched = true;
        engine->ServerCommand(("host_workshop_map " + map->workshop + "\n").c_str());
    }

    void chat(const std::string& text, int slot = -1, int destination = 3) {
        // Network message registration is not complete during startup Load().
        auto* chatMessage = networkMessages->FindNetworkMessagePartial("TextMsg");
        if (!chatMessage) throw std::runtime_error("TextMsg network message unavailable");
        auto* message = chatMessage->AllocateMessage()->ToPB<CUserMessageTextMsg>();
        message->set_dest(destination); // HUD_PRINTTALK=3, HUD_PRINTCENTER=4
        message->add_param(text);
        for (int i = 0; i < 4; ++i) message->add_param("");
        const uint64 recipients = slot >= 0 ? (uint64(1) << slot) : 0;
        gameEvents->PostEventAbstract(0, false, slot >= 0 ? 1 : -1, slot >= 0 ? &recipients : nullptr, chatMessage, message, 0, BUF_RELIABLE);
        delete message;
        if (destination == 3) META_CONPRINTF("[RTV HUD] Chat: %s\n", text.c_str());
    }
    std::string name(int slot) {
        auto* controller = GameEntitySystem()->GetEntityInstance(CEntityIndex(slot + 1));
        if (!controller) return "Player";
        const auto* raw = static_cast<const char*>(playerName.at(controller));
        std::string result;
        for (size_t i = 0; i < 128 && raw[i]; ++i)
            if (static_cast<unsigned char>(raw[i]) >= 32 && raw[i] != 127) result += raw[i];
        return result.empty() ? "Player" : result;
    }
    void sound(int slot, const char* event = nullptr) {
        if (!event) event = moveSound.Get().String();
        if (!event || !*event) return;
        const std::string value(event);
        // Match CS2MenuManager's `play` transport for file paths. Restrict the
        // argument to a single safe token before passing it to the client console.
        if (value.size() > 240 || value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./-") != std::string::npos) return;
        if (value.find('/') != std::string::npos || value.find(".vsnd") != std::string::npos) {
            engine->ClientCommand(CPlayerSlot(slot), "play %s", event);
            return;
        }
        auto* type = networkMessages->FindNetworkMessagePartial("SendAudio");
        if (!type) return;
        auto* message = type->AllocateMessage()->ToPB<CUserMessageSendAudio>();
        message->set_soundname(event);
        const uint64 recipients = uint64(1) << slot;
        gameEvents->PostEventAbstract(0, false, 1, &recipients, type, message, 0, BUF_RELIABLE);
        delete message;
    }
    void closeVote(int slot, uint64_t steam) {
        closedVotes.insert(steam);
        voteHuds.close(slot);
        chat("", slot, 4);
    }
    bool castVote(int slot, uint64_t steam, int choice) {
        if (!vote.cast(steam, choice, now())) return false;
        sound(slot, selectSound.Get().String());
        closeVote(slot, steam);
        chat("[RTV] " + name(slot) + " voted for " + vote.choices[choice].label + ".");
        render();
        return true;
    }
    void extend(int slot, uint64_t steam, const std::string& text) {
        if (!admins.count(steam)) { chat("[Extend] Admin permission required.", slot); return; }
        const int minutes = rtv::extensionMinutes(text);
        if (!minutes) { chat("[Extend] Usage: !extend <1-120 minutes>", slot); return; }
        if (vote.phase != rtv::Phase::Idle || pendingMap.active()) {
            chat("[Extend] Wait until the vote or map change ends.", slot); return;
        }
        if (lastExtend.count(steam) && now() < lastExtend[steam] + 3) {
            chat("[Extend] Please wait 3 seconds before extending again.", slot); return;
        }
        ConVarRefAbstract limit("mp_timelimit");
        if (!limit.IsValidRef()) { chat("[Extend] Map time limit is unavailable.", slot); return; }
        const float before = limit.GetFloat(), after = rtv::extendedLimit(before, minutes);
        if (!after) {
            chat(before <= 0 ? "[Extend] This map already has unlimited time." : "[Extend] Time limit is outside the supported range.", slot);
            return;
        }
        limit.SetFloat(after);
        if (std::fabs(limit.GetFloat() - after) > 0.01f) {
            chat("[Extend] The server rejected the time limit change.", slot); return;
        }
        lastExtend[steam] = now();
        chat("[Extend] " + name(slot) + " extended this map by " + std::to_string(minutes) + " minutes.");
        sound(slot);
    }
    void pollInput() {
        if (vote.phase != rtv::Phase::Voting || now() >= vote.deadline) return;
        for (int slot = 0; slot < 64; ++slot) {
            const auto* steam = engine->GetClientSteamID(CPlayerSlot(slot));
            if (!steam || !steam->BIndividualAccount() || !engine->IsClientFullyAuthenticated(CPlayerSlot(slot))) continue;
            if (closedVotes.count(steam->ConvertToUint64())) continue;
            auto* controller = GameEntitySystem()->GetEntityInstance(CEntityIndex(slot + 1));
            if (!controller) continue;
            const auto handle = pawnHandle.value<CEntityHandle>(controller);
            auto* pawn = GameEntitySystem()->GetEntityInstance(handle);
            auto* movement = pawn ? movementServices.value<void*>(pawn) : nullptr;
            if (!movement) { inputs.erase(steam->ConvertToUint64()); continue; }
            // Pinned SDK's CInButtonState: vtable followed by three uint64 states.
            const auto* states = reinterpret_cast<const uint64*>(
                static_cast<const char*>(movementButtons.at(movement)) + sizeof(void*));
            auto& input = inputs[steam->ConvertToUint64()];
            if (input.pawn != handle) {
                input.navigation.initialized = false;
                input.pawn = handle;
            }
            const int previous = input.navigation.selected;
            const bool confirm = input.navigation.update(states[0], static_cast<int>(vote.choices.size()));
            auto& hud = voteHuds.open(slot, steam->ConvertToUint64(), hudPrototype);
            hud.highlightGlobal(input.navigation.selected);
            if (confirm && castVote(slot, steam->ConvertToUint64(), input.navigation.selected)) continue;
            if (previous != input.navigation.selected) sound(slot);
            if (previous != input.navigation.selected || now() >= input.nextHint) {
                std::string text = "[RTV] " + std::to_string(input.navigation.selected + 1) + ". " +
                    vote.choices[input.navigation.selected].label + "\nW / S: select | E: vote";
                const auto ballot = vote.ballots.find(steam->ConvertToUint64());
                if (ballot != vote.ballots.end()) text += "\nYour vote: " + std::to_string(ballot->second + 1);
                chat(text, slot, 4);
                input.nextHint = now() + 1.0;
            }
        }
    }
    void refreshPlayers() {
        std::set<uint64_t> connected;
        for (int i = 0; i < 64; ++i) {
            auto slot = CPlayerSlot(i);
            // Authenticated Steam users only: excludes bots/connecting clients, includes spectators.
            const auto* id = engine->GetClientSteamID(slot);
            if (id && id->BIndividualAccount() && engine->IsClientFullyAuthenticated(slot)) connected.insert(id->ConvertToUint64());
        }
        for (auto id : humans) if (!connected.count(id)) { vote.disconnect(id); nominations.disconnect(id); inputs.erase(id); lastOpen.erase(id); closedVotes.erase(id); lastExtend.erase(id); }
        humans = std::move(connected);
        for (int slot = 0; slot < 64; ++slot) {
            const auto id = authenticated(slot);
            const auto& entry = voteHuds.entries[slot];
            if (entry && (entry->steam != id || vote.phase == rtv::Phase::Idle || closedVotes.count(id))) voteHuds.close(slot);
        }
    }
    void render() {
        auto tally = vote.counts();
        for (int slot = 0; slot < 64; ++slot) {
            const auto steam = authenticated(slot);
            if (!steam || vote.phase == rtv::Phase::Idle || closedVotes.count(steam)) {
                voteHuds.close(slot); continue;
            }
            auto& hud = voteHuds.open(slot, steam, hudPrototype);
            hud.set("title", vote.phase == rtv::Phase::Voting ? (mapEnd.timedVote ? "NEXT MAP VOTE" : "ROCK THE VOTE") : "VOTE RESULT");
            hud.set("status", vote.phase == rtv::Phase::Voting ? std::to_string(std::max(0, static_cast<int>(std::ceil(vote.deadline-now())))) + " seconds remaining" :
                (vote.winner < 0 ? "No votes - cancelled" : "Winner: " + vote.choices[vote.winner].label));
            for (int i = 0; i < 6; ++i)
                hud.set("choice" + std::to_string(i+1), i < static_cast<int>(vote.choices.size()) ?
                    std::to_string(i+1) + ". " + vote.choices[i].label + "   [" + std::to_string(tally[i]) + "]" : "");
            hud.set("hint", "W / S: select | E: vote | " + std::string(allowChange.Get() ? (mapEnd.timedVote ? "Changes at time limit" : "Map change enabled") : "DRY RUN"));
            int selected = -1;
            if (vote.phase == rtv::Phase::Voting) {
                auto input = inputs.find(steam);
                selected = input == inputs.end() ? 0 : input->second.navigation.selected;
            }
            hud.highlightGlobal(selected);
        }
    }
    void start(bool timed = false, double secondsLeft = 0) {
        if (!ready || vote.phase != rtv::Phase::Idle || pendingMap.active() || (!timed && mapEnd.started)) return;
        if (!GameEntitySystem()) throw std::runtime_error("load a map before starting a HUD vote");
        auto candidates = maps;
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](const auto& m) {
            return m.name == currentMap || currentMap == "workshop/" + m.workshop + "/" + m.name;
        }), candidates.end());
        if (candidates.empty() || (!timed && candidates.size() < 2)) throw std::runtime_error("not enough maps other than the current map");
        std::mt19937 rng(std::random_device{}()); std::shuffle(candidates.begin(), candidates.end(), rng);
        candidates=nominations.candidates(candidates,currentMap);
        browser.closeAll();
        inputs.clear();
        closedVotes.clear();
        syncSettings();
        refreshPlayers();
        if (timed) {
            mapEnd.begin(vote, std::move(candidates), now(), secondsLeft, static_cast<int>(humans.size()));
            publishNextMap();
            if (!mapEnd.timedVote) {
                chat("[Next map] " + mapEnd.next->label + " selected automatically; changing at the time limit.");
                return;
            }
        } else vote.start(std::move(candidates), now());
        render();
        chat(std::string(timed ? "[Next map] " : "[RTV] ") + "Vote started. W / S: select, E: vote. " + std::to_string(static_cast<int>(std::ceil(std::max(0.0, vote.deadline - now())))) + " seconds.");
        for (int i = 0; i < 64; ++i) if (engine->IsClientFullyAuthenticated(CPlayerSlot(i)))
            engine->ClientPrintf(CPlayerSlot(i), "[RTV HUD] W/S: select, E: vote. Console: rtvhud_vote 1-6 also available.\n");
    }
    void failure(const std::exception& e) {
        META_CONPRINTF("[RTV HUD] Disabled after error: %s\n", e.what());
        ready = false; voteHuds.clear(); vote.reset(now());
        browser.destroy(); pendingMap={};
    }
public:
    bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late) override {
        PLUGIN_SAVEVARS();
        if (!KHook::__exported__khook) {
            snprintf(error, maxlen, "KHook interface unavailable; RTV HUD 0.9.0 requires KHook Metamod (API 18)");
            return false;
        }
        GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer, INTERFACEVERSION_VENGINESERVER);
        GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
        GET_V_IFACE_ANY(GetServerFactory, server, ISource2Server, INTERFACEVERSION_SERVERGAMEDLL);
        GET_V_IFACE_ANY(GetServerFactory, gameEntities, ISource2GameEntities, INTERFACEVERSION_SERVERGAMEENTS);
        GET_V_IFACE_ANY(GetServerFactory, clients, ISource2GameClients, INTERFACEVERSION_SERVERGAMECLIENTS);
        GET_V_IFACE_CURRENT(GetEngineFactory, schemas, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
        GET_V_IFACE_CURRENT(GetEngineFactory, networkMessages, INetworkMessages, NETWORKMESSAGES_INTERFACE_VERSION);
        GET_V_IFACE_CURRENT(GetEngineFactory, gameEvents, IGameEventSystem, GAMEEVENTSYSTEM_INTERFACE_VERSION);
        GET_V_IFACE_CURRENT(GetFileSystemFactory, filesystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
        GET_V_IFACE_CURRENT(GetEngineFactory, networkServer, INetworkServerService, NETWORKSERVERSERVICE_INTERFACE_VERSION);
        resourceService = ismm->GetEngineFactory()("GameResourceServiceServerV001", nullptr);
        if (!resourceService) { snprintf(error, maxlen, "GameResourceServiceServerV001 unavailable"); return false; }
        try {
            rulesPointer = Field(schemas, "CCSGameRulesProxy", "m_pGameRules");
            gameStartTime = Field(schemas, "CCSGameRules", "m_flGameStartTime");
            hudPrototype.init(schemas);
            browser.init(schemas);
            playerName = Field(schemas, "CBasePlayerController", "m_iszPlayerName");
            pawnHandle = Field(schemas, "CBasePlayerController", "m_hPawn");
            movementServices = Field(schemas, "CBasePlayerPawn", "m_pMovementServices");
            movementButtons = Field(schemas, "CPlayer_MovementServices", "m_nButtons");
            CBufferString directory; engine->GetGameDir(directory);
            configDirectory = std::string(directory.Get()) + "/addons/rtv_hud";
            std::ifstream file(configDirectory + "/maplist.txt");
            if (!file) throw std::runtime_error("cannot open addons/rtv_hud/maplist.txt");
            maps = rtv::readMaps(file);

        } catch (const std::exception& e) { snprintf(error, maxlen, "%s", e.what()); return false; }
        try { readAdminFile(); }
        catch (const std::exception& e) { META_CONPRINTF("[RTV HUD] %s\n", e.what()); }
        if (auto* game = networkServer->GetIGameServer()) {
            const char* name = game->GetMapName(); currentMap = name ? name : "";
        }
        mapActive = late && !currentMap.empty();
        vote.reset(now()); ready = true;
        g_SMAPI->AddListener(this, this);
        transmitHook.Add(gameEntities);
        frameHook.Add(server);
        commandHook.Add(clients);
        clickHook.Add(clients);
        dispatchHook.Add(g_pCVar);
        ConVar_Register(FCVAR_RELEASE | FCVAR_GAMEDLL);
        engine->ServerCommand("exec rtv_hud.cfg\n");
        META_CONPRINTF("[RTV HUD] Loaded 0.9.2: dynamic maplist browser; names, tiers, Global flags and Workshop IDs come from maplist.txt.\n");
        return true;
    }
    bool Unload(char*, size_t) override {
        transmitHook.Remove(gameEntities);
        frameHook.Remove(server);
        commandHook.Remove(clients);
        clickHook.Remove(clients);
        dispatchHook.Remove(g_pCVar);
        browser.destroy();
        voteHuds.clear(); ConVar_Unregister(); ready = false; return true;
    }
    KHook::Return<void> transmit(ISource2GameEntities*, CCheckTransmitInfo** infos, int count, CBitVec<16384>&, CBitVec<16384>&,
                  const Entity2Networkable_t**, const uint16*, int) {
        for (int owner = 0; owner < 64; ++owner) {
            auto& entry = voteHuds.entries[owner];
            if (!entry) continue;
            auto* entity = entry->hud.get();
            if (!entity) continue;
            const auto ownerSteam = authenticated(owner);
            const auto index = entity->GetRefEHandle().GetEntryIndex();
            for (int i = 0; i < count; ++i) {
                if (!infos[i]) continue;
                const int slot = *reinterpret_cast<const int*>(reinterpret_cast<const char*>(infos[i]) + 576);
                // Each entity is sent only to its owner, including while spectating.
                if (!voteHuds.visibleTo(owner, slot, ownerSteam)) {
                    auto* bits = *reinterpret_cast<CBitVec<16384>**>(infos[i]);
                    if (bits) bits->Clear(index);
                }
            }
        }
        // Browser entities contain only one owner's bounded page, and must not
        // be replicated to other players (including late joiners/spectators).
        for (int owner = 0; owner < 64; ++owner) {
            auto* entity = browser.entity(owner);
            if (!entity) continue;
            const auto ownerSteam = authenticated(owner);
            const auto index = entity->GetRefEHandle().GetEntryIndex();
            for (int i = 0; i < count; ++i) {
                if (!infos[i]) continue;
                const int slot = *reinterpret_cast<const int*>(reinterpret_cast<const char*>(infos[i]) + 576);
                if (!browser.visibleTo(owner, slot, ownerSteam)) {
                    auto* bits = *reinterpret_cast<CBitVec<16384>**>(infos[i]);
                    if (bits) bits->Clear(index);
                }
            }
        }
        return {KHook::Action::Ignore};
    }
    void OnLevelInit(const char* map, const char*, const char*, const char*, bool, bool) override {
        mapEnd.reset(); rulesHandle = CEntityHandle(); mapActive = true;
        voteHuds.forget(); vote.reset(now()); inputs.clear(); currentMap = map ? map : ""; humans.clear(); nextFrame = 0;
        browser.forget();nominations.clear();pendingMap={};lastOpen.clear();closedVotes.clear();lastExtend.clear();
        engine->ServerCommand("exec rtv_hud.cfg\n");
    }
    void OnLevelShutdown() override { mapActive = false; mapEnd.reset(); rulesHandle = CEntityHandle(); browser.closeAll();browser.forget();nominations.clear();pendingMap={};lastOpen.clear();closedVotes.clear();lastExtend.clear();voteHuds.forget(); vote.reset(now()); inputs.clear(); humans.clear(); }
    KHook::Return<void> frame(ISource2Server*, bool simulating, bool, bool) {
        if (!ready || !mapActive || !GameEntitySystem()) return {KHook::Action::Ignore};
        try {
            syncSettings();
            if (simulating) pollInput();
            if (now() < nextFrame) return {KHook::Action::Ignore};
            nextFrame = now() + 0.25;
            maintainBrowser();
            refreshPlayers();
            executeMapChange();
            const auto left = endVoteEnabled.Get() ? timeLeft() : std::optional<double>{};
            if (!pendingMap.active() && mapEnd.shouldStart(left, endVoteLead.Get())) {
                if (vote.phase == rtv::Phase::Idle) start(true, *left);
                else {
                    mapEnd.adopt(vote, now(), *left);
                    publishNextMap();
                    chat("[Next map] Current vote will select the map for the time limit.");
                }
            }
            if (left) mapEnd.capDeadline(vote, now(), *left);
            if (!mapEnd.started && !pendingMap.active() && vote.thresholdReached(static_cast<int>(humans.size()), now())) start();
            if (vote.phase == rtv::Phase::Voting) {
                if (vote.finish(now())) {
                    if (mapEnd.timedVote) {
                        const bool noVotes = vote.winner < 0;
                        mapEnd.settle(vote); publishNextMap();
                        chat("[Next map] " + mapEnd.next->label + (noVotes ? " selected automatically (no votes)." : " won the vote.") +
                            (allowChange.Get() ? " Changing at the time limit." : " Dry run - map will not change."));
                    } else chat(rtv::resultText(vote, allowChange.Get()));
                }
                render();
            }
            else if (vote.phase == rtv::Phase::Result && now() >= vote.deadline) {
                if (!mapEnd.timedVote && vote.winner >= 0) pendingMap.queue(vote.choices[vote.winner], currentMap, now());
                voteHuds.clear(); vote.reset(now(), false); inputs.clear(); nominations.clear();
            }
            if (!pendingMap.active() && mapEnd.changeDue(left)) {
                voteHuds.clear(); vote.reset(now(), false); inputs.clear(); nominations.clear();
                if (pendingMap.queue(*mapEnd.next, currentMap, now())) {
                    mapEnd.changeQueued = true;
                    META_CONPRINTF("[RTV HUD] Time limit reached: queued workshop %s.\n", mapEnd.next->workshop.c_str());
                }
            }
        } catch (const std::exception& e) { failure(e); }
        return {KHook::Action::Ignore};
    }
    void request(CPlayerSlot slot, int choice) {
        if (!ready || slot.Get() < 0 || slot.Get() >= 64 || !GameEntitySystem()) return;
        try {
            syncSettings(); refreshPlayers(); const auto* steam = engine->GetClientSteamID(slot);
            if (!steam || !humans.count(steam->ConvertToUint64())) return;
            auto id = steam->ConvertToUint64();
            if (choice >= 0) {
                if (!castVote(slot.Get(), id, choice)) chat("[RTV] Vote closed or invalid choice.", slot.Get());
            } else {
                if (vote.phase != rtv::Phase::Idle) {
                    if (vote.phase == rtv::Phase::Voting) {
                        closedVotes.erase(id);
                        inputs[id].navigation.initialized = false; inputs[id].nextHint = 0;
                        render();
                        chat("[RTV] W/S: select, E: confirm.", slot.Get());
                    } else chat("[RTV] The vote has ended.", slot.Get());
                    return;
                }
                if (pendingMap.active()) { chat("[RTV] A map change is already pending.", slot.Get()); return; }
                if (mapEnd.started && vote.phase == rtv::Phase::Idle) { chat("[Next map] " + mapEnd.next->label + " is already selected.", slot.Get()); return; }
                if (now() < vote.availableAt) {
                    chat("[RTV] Available in " + std::to_string(static_cast<int>(std::ceil(vote.availableAt-now()))) + " seconds.", slot.Get()); return;
                }
                const bool already = vote.requests.count(id);
                const bool enough = vote.request(id, static_cast<int>(humans.size()), now());
                chat("[RTV] " + name(slot.Get()) + (already ? " already requested RTV. " : " requested RTV. ") +
                     std::to_string(vote.requests.size()) + "/" + std::to_string(vote.required(humans.size())) +
                     " (" + std::to_string(vote.settings.percent) + "% required).", already ? slot.Get() : -1);
                if (enough) start();
            }
        } catch (const std::exception& e) { failure(e); }
    }
    KHook::Return<void> click(ISource2GameClients*, CPlayerSlot playerSlot,int type,uint32 size,const void* data) {
        if(!ready||!GameEntitySystem()||type!=390)return {KHook::Action::Ignore};
        uint32_t packed=0;std::string button;
        if(!rtv::parseClick(data,size,packed,button))return {KHook::Action::Ignore};
        auto* entity=browser.entity(playerSlot.Get());
        if(!entity||GameEntitySystem()->GetEntityInstance(CEntityHandle::FromPackedInt(packed))!=entity)return {KHook::Action::Ignore};
        int slot=playerSlot.Get();const auto steam=authenticated(slot);auto found=browser.sessions.find(slot);
        if(!steam||found==browser.sessions.end()||found->second.steam!=steam)return {KHook::Action::Supersede};
        try {
            syncSettings();
            if(browser.click(slot,button,now(),maps,currentMap)) {
                auto& session=browser.sessions.at(slot);
                if(vote.phase!=rtv::Phase::Idle||pendingMap.active()) {
                    session.result="A vote or map change is already in progress.";chat("[HUD] " + session.result,slot);browser.render(slot,maps,currentMap);
                } else {
                    const auto& map=maps.at(session.selected);
                    if(session.mapMode) {
                        if(!admins.count(steam)) {
                            session.canMap=false;session.result="Map change permission denied.";
                            chat("[Map] " + session.result,slot);
                            browser.render(slot,maps,currentMap);return {KHook::Action::Supersede};
                        }
                        if(pendingMap.queue(map,currentMap,now(),slot,steam)) {
                            chat("[Map] " + name(slot) + " selected " + map.label + " for map change.");
                            sound(slot,selectSound.Get().String());browser.close(slot);
                        }
                    } else {
                        const auto error=nominations.nominate(steam,map,currentMap,false);
                        if(error.empty()) {
                            chat("[Nominate] " + name(slot) + " nominated " + map.label + ".");sound(slot,selectSound.Get().String());browser.close(slot);
                        } else {session.result=error;chat("[Nominate] " + error,slot);browser.render(slot,maps,currentMap);}
                    }
                }
            }
            if(browser.selectedByClick >= 0) {
                sound(slot);
            }
        } catch(const std::exception& e){
            META_CONPRINTF("[RTV HUD] Browser error: %s\n",e.what());browser.close(slot);
            chat("[HUD] The map browser was closed after an error.",slot);
        }
        return {KHook::Action::Supersede};
    }
    bool nativeCommand(CPlayerSlot slot, const rtv::NativeCommand& cmd) {
        if (cmd.action == rtv::Action::Unknown) return false;
        const auto steam = authenticated(slot.Get());
        if (!ready || !steam || !GameEntitySystem()) return true;
        try {
            if (!cmd.valid) { chat("[HUD] Invalid command or search text (maximum 128 bytes).", slot.Get()); return true; }
            if (cmd.action == rtv::Action::Map && !admins.count(steam)) {
                chat("[Map] Admin permission required. Use !nominate to nominate a map.", slot.Get()); return true;
            }
            if (cmd.action == rtv::Action::Rtv) { request(slot, -1); return true; }
            if (cmd.action == rtv::Action::Close) { browser.close(slot.Get()); closeVote(slot.Get(), steam); return true; }
            if (cmd.action == rtv::Action::NextMap) {
                chat(mapEnd.next ? "[Next map] " + mapEnd.next->label : "[Next map] Will be selected by vote before the time limit.", slot.Get()); return true;
            }
            if (cmd.action == rtv::Action::TimeLeft) {
                const auto left = timeLeft();
                chat(left ? "[Time left] " + std::to_string(static_cast<int>(std::ceil(std::max(0.0, *left)))) + " seconds." : "[Time left] No active time limit.", slot.Get()); return true;
            }
            if (cmd.action == rtv::Action::Extend) { extend(slot.Get(), steam, cmd.query); return true; }
            if (vote.phase != rtv::Phase::Idle || pendingMap.active()) {
                chat("[HUD] Please wait until the vote or map change ends.", slot.Get()); return true;
            }
            if (now() < lastOpen[steam] + 0.5) return true;
            lastOpen[steam] = now();
            if (!browserAssets()) {
                chat("[HUD] The dynamic browser assets (0.9.0) are not installed yet.", slot.Get()); return true;
            }
            refreshPlayers();
            syncSettings();
            browser.open(slot.Get(), steam, admins.count(steam), cmd.action == rtv::Action::Map,
                         cmd.query, now(), maps, currentMap);
        } catch (const std::exception& e) { failure(e); }
        return true;
    }
    KHook::Return<void> dispatch(ICvar*, ConCommandRef cmd, const CCommandContext& context, const CCommand& args) {
        if (!cmd.IsValidRef() || args.ArgC() < 2) return {KHook::Action::Ignore};
        if (std::strcmp(args[0], "say") && std::strcmp(args[0], "say_team")) return {KHook::Action::Ignore};
        const auto slot = context.GetPlayerSlot();
        if (slot.Get() < 0 || slot.Get() >= 64) return {KHook::Action::Ignore};
        if (nativeCommand(slot, rtv::parseNativeCommand(args.ArgS(), true))) return {KHook::Action::Supersede};
        return {KHook::Action::Ignore};
    }
    void reloadAdmins() {
        try { readAdminFile(); META_CONPRINTF("[RTV HUD] Reloaded %zu native admins.\n", admins.size()); }
        catch (const std::exception& e) { META_CONPRINTF("[RTV HUD] %s; admin allowlist cleared.\n", e.what()); }
        try { browser.closeAll(); } catch (const std::exception& e) { failure(e); }
    }
    void status() {
        const auto left = timeLeft();
        META_CONPRINTF("[RTV HUD] Map timer: left=%.2f end_vote=%d nextmap=%s next_workshop=%s queued=%d\n",
            left ? *left : -1.0, mapEnd.started,
            mapEnd.next ? mapEnd.next->name.c_str() : "unset",
            mapEnd.next ? mapEnd.next->workshop.c_str() : "unset", mapEnd.changeQueued);
        META_CONPRINTF("[RTV HUD] Global maplist flags: %zu\n",static_cast<size_t>(std::count_if(maps.begin(),maps.end(),rtv::hasGlobalFlag)));
        META_CONPRINTF("[RTV HUD] Native 0.9.2: ready=%d browser_assets=%d maps=%zu admins=%zu sessions=%zu phase=%d transition=%d current=%s\n",
            ready,browserAssets(),maps.size(),admins.size(),browser.sessions.size(),static_cast<int>(vote.phase),pendingMap.active(),currentMap.c_str());
    }
    void reloadMaps() {
        if(mapEnd.started){META_CONPRINTF("[RTV HUD] Reload refused: next map already reserved.\n");return;}
        if(vote.phase!=rtv::Phase::Idle||pendingMap.active()){META_CONPRINTF("[RTV HUD] Reload refused: vote/map change in progress.\n");return;}
        try {
            std::ifstream file(configDirectory+"/maplist.txt");
            auto replacement=rtv::readMaps(file);

            browser.closeAll();maps=std::move(replacement);nominations.clear();
            META_CONPRINTF("[RTV HUD] Reloaded %zu maps; nominations cleared.\n",maps.size());
        }catch(const std::exception& e){META_CONPRINTF("[RTV HUD] Reload failed; old maplist retained: %s\n",e.what());}
    }
    KHook::Return<void> command(ISource2GameClients*, CPlayerSlot slot, const CCommand& args) {
        if (args.ArgC() < 1) return {KHook::Action::Ignore};
        if (nativeCommand(slot, rtv::parseNativeCommand(args.GetCommandString(), false))) return {KHook::Action::Supersede};
        if (args.ArgC() == 1 && !std::strcmp(args[0], "rtvhud_preview")) {
            preview(slot); return {KHook::Action::Supersede};
        }
        if (args.ArgC() == 1 && !std::strcmp(args[0], "rtvhud_rtv")) {
            request(slot, -1); return {KHook::Action::Supersede};
        }
        if (!std::strcmp(args[0], "rtvhud_vote")) {
            if (args.ArgC() == 2 && std::strlen(args[1]) == 1 && args[1][0] >= '1' && args[1][0] <= '6') request(slot, args[1][0] - '1');
            return {KHook::Action::Supersede};
        }
        return {KHook::Action::Ignore};
    }
    void preview(CPlayerSlot slot) {
        if (slot.Get() != -1) {
            if (slot.Get() < 0 || slot.Get() >= 64) return;
            const auto* steam = engine->GetClientSteamID(slot);
            if (!steam || !steam->BIndividualAccount() || !engine->IsClientFullyAuthenticated(slot)) return;
            if (!admins.count(steam->ConvertToUint64())) { chat("[RTV] Preview requires admin permission.", slot.Get()); return; }
        }
        try { start(); } catch (const std::exception& e) { failure(e); }
    }
    const char* GetAuthor() override { return "RTV HUD contributors"; }
    const char* GetName() override { return "RTV Custom HUD Prototype"; }
    const char* GetDescription() override { return "Native custom_hud_layout map vote"; }
    const char* GetURL() override { return ""; }
    const char* GetLicense() override { return "AGPL-3.0-or-later"; }
    const char* GetVersion() override { return "0.9.2"; }
    const char* GetDate() override { return __DATE__; }
    const char* GetLogTag() override { return "RTVHUD"; }
};
static RTVHud plugin;
PLUGIN_EXPOSE(RTVHud, plugin);
CON_COMMAND_F(rtvhud_rtv, "Request prototype map vote", FCVAR_CLIENT_CAN_EXECUTE) {
    if (args.ArgC() == 1) plugin.request(context.GetPlayerSlot(), -1);
}
CON_COMMAND_F(rtvhud_vote, "Vote for prototype candidate 1-6", FCVAR_CLIENT_CAN_EXECUTE) {
    if (args.ArgC() == 2 && std::strlen(args[1]) == 1 && args[1][0] >= '1' && args[1][0] <= '6')
        plugin.request(context.GetPlayerSlot(), args[1][0] - '1');
}
CON_COMMAND_F(rtvhud_preview, "Start prototype HUD vote", FCVAR_CLIENT_CAN_EXECUTE) {
    if (args.ArgC() == 1) plugin.preview(context.GetPlayerSlot());
}
CON_COMMAND_F(rtvhud_status, "Show native HUD status", FCVAR_NONE) {
    if(context.GetPlayerSlot().Get()==-1)plugin.status();
}
CON_COMMAND_F(rtvhud_reload_admins, "Reload independent SteamID64 admin allowlist", FCVAR_NONE) {
    if(context.GetPlayerSlot().Get()==-1&&args.ArgC()==1)plugin.reloadAdmins();
}
CON_COMMAND_F(rtvhud_nominate, "Open native map nomination browser", FCVAR_CLIENT_CAN_EXECUTE) {
    plugin.nativeCommand(context.GetPlayerSlot(), rtv::parseNativeCommand(args.GetCommandString(), false));
}
CON_COMMAND_F(rtvhud_map, "Open native map change browser", FCVAR_CLIENT_CAN_EXECUTE) {
    plugin.nativeCommand(context.GetPlayerSlot(), rtv::parseNativeCommand(args.GetCommandString(), false));
}
CON_COMMAND_F(rtvhud_close, "Close native map browser", FCVAR_CLIENT_CAN_EXECUTE) {
    plugin.nativeCommand(context.GetPlayerSlot(), rtv::parseNativeCommand(args.GetCommandString(), false));
}
CON_COMMAND_F(rtvhud_reload, "Reload the server maplist", FCVAR_NONE) {
    if(context.GetPlayerSlot().Get()==-1&&args.ArgC()==1)plugin.reloadMaps();
}

CON_COMMAND_F(rtvhud_extend, "Extend the current map by 1-120 minutes (admin only)", FCVAR_CLIENT_CAN_EXECUTE) {
    plugin.nativeCommand(context.GetPlayerSlot(), rtv::parseNativeCommand(args.GetCommandString(), false));
}

CON_COMMAND_F(rtvhud_nextmap, "Show the reserved next map", FCVAR_CLIENT_CAN_EXECUTE) {
    if (context.GetPlayerSlot().Get() < 0) plugin.status();
    else plugin.nativeCommand(context.GetPlayerSlot(), rtv::parseNativeCommand("rtvhud_nextmap", false));
}
CON_COMMAND_F(rtvhud_timeleft, "Show seconds remaining", FCVAR_CLIENT_CAN_EXECUTE) {
    if (context.GetPlayerSlot().Get() < 0) plugin.status();
    else plugin.nativeCommand(context.GetPlayerSlot(), rtv::parseNativeCommand("rtvhud_timeleft", false));
}
