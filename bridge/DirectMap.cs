using System.Text;
using CounterStrikeSharp.API;
using CounterStrikeSharp.API.Core;
using CounterStrikeSharp.API.Modules.Admin;
using CounterStrikeSharp.API.Modules.Commands;
using CounterStrikeSharp.API.Modules.Timers;

namespace DirectMap;

public sealed class DirectMap : BasePlugin
{
    public override string ModuleName => "Direct Map HUD Bridge";
    public override string ModuleVersion => "2.0.0";
    public override string ModuleAuthor => "Local server configuration";
    public override string ModuleDescription => "Live nomination/map browser and authoritative CSS admin checks.";
    private readonly Dictionary<ulong, DateTime> _lastOpen = new();

    public override void Load(bool hotReload)
    {
        AddCommand("css_nominate", "Open the map nomination HUD", (p,c) => Open(p,c,false));
        AddCommand("css_nom", "Open the map nomination HUD", (p,c) => Open(p,c,false));
        AddCommand("css_map", "Open the map change HUD", (p,c) => Open(p,c,true));
        AddCommand("css_mapmenu", "Open the map change HUD", (p,c) => Open(p,c,true));
        AddCommand("css_mm", "Open the map change HUD", (p,c) => Open(p,c,true));
        AddCommand("css_rtv", "Request a map vote", (p,c) => { if (Valid(p)) Send("rtv",p!); });
        AddCommand("css_hudclose", "Close the map HUD and release the cursor", (p,c) => { if (Valid(p)) Send("close",p!); });
        AddCommand("css_rtvhud_commit", "Internal server-only map authorization", Commit);
        AddTimer(2.0f, () => {
            var connected = new HashSet<ulong>();
            foreach (var p in Utilities.GetPlayers().Where(Valid)) {
                connected.Add(p.SteamID);
                Send("role",p, CanMap(p) ? "1" : "0");
            }
            foreach (var id in _lastOpen.Keys.Where(id => !connected.Contains(id)).ToArray()) _lastOpen.Remove(id);
        }, TimerFlags.REPEAT);
        RegisterListener<Listeners.OnMapStart>(_ => { _lastOpen.Clear(); SyncMap(); });
        SyncMap();
        Server.PrintToConsole("[DirectMap] HUD bridge 2.0.0 loaded. !nominate / !map / !rtv / !hudclose; @css/root required for map changes.");
    }
    private static bool Valid(CCSPlayerController? p) => p is { IsValid: true, IsBot: false, IsHLTV: false } && p.AuthorizedSteamID is not null;
    private static bool CanMap(CCSPlayerController p) => AdminManager.PlayerHasPermissions(p, "@css/root");
    private static string Hex(string value) {
        var bytes=Encoding.UTF8.GetBytes(value);
        if(bytes.Length>128)bytes=bytes[..128];
        return bytes.Length==0?"-":Convert.ToHexString(bytes);
    }
    private static void SyncMap() => Server.NextFrame(() => Server.ExecuteCommand("rtvhud_bridge mapname " + Hex(Server.MapName)));
    private static void Send(string action,CCSPlayerController p,string suffix="") =>
        Server.ExecuteCommand($"rtvhud_bridge {action} {p.Slot} {p.SteamID}" + (suffix.Length==0?"":" "+suffix));
    private void Open(CCSPlayerController? p,CommandInfo command,bool map) {
        if(!Valid(p)){command.ReplyToCommand("[HUD] Join the server to open the map browser.");return;}
        if(_lastOpen.TryGetValue(p!.SteamID,out var last) && DateTime.UtcNow-last<TimeSpan.FromSeconds(0.5))return;
        _lastOpen[p.SteamID]=DateTime.UtcNow;
        var query=string.Join(" ",Enumerable.Range(1,command.ArgCount-1).Select(command.GetArg));
        if(query.Any(char.IsControl)){command.ReplyToCommand("[HUD] Invalid search text.");return;}
        Send("open",p,$"{(CanMap(p)?1:0)} {(map?"map":"nominate")} {Hex(query)}");
    }
    private void Commit(CCSPlayerController? caller,CommandInfo command) {
        // Only the native plugin's server-console request may enter this path.
        if(caller is not null || command.ArgCount!=4)return;
        if(!int.TryParse(command.GetArg(1),out int slot)||slot<0||slot>=64||
           !ulong.TryParse(command.GetArg(2),out ulong steam))return;
        string workshop=command.GetArg(3);
        if(workshop.Length is <1 or >20 || workshop.Any(c=>c<'0'||c>'9') || workshop=="0")return;
        Server.NextFrame(() => {
            var p=Utilities.GetPlayerFromSlot(slot);
            if(!Valid(p)||p!.SteamID!=steam)return;
            bool allowed=CanMap(p)&&MapExists(workshop);
            Send("result",p,$"{workshop} {(allowed?1:0)}");
            Server.PrintToConsole($"[DirectMap] Map authorization: slot={slot} workshop={workshop} allowed={allowed}");
        });
    }
    private bool MapExists(string workshop) {
        var path=Path.GetFullPath(Path.Combine(ModuleDirectory,"..","..","..","rtv_hud","maplist.txt"));
        try {
            return File.ReadLines(path).Any(raw => {
                var line=raw.Trim();if(line.StartsWith('#')||line.StartsWith("//"))return false;
                int colon=line.LastIndexOf(':');return colon>=0&&line[(colon+1)..]==workshop;
            });
        } catch(IOException e) { Server.PrintToConsole("[DirectMap] Maplist read failed: "+e.Message);return false; }
    }
    public override void Unload(bool hotReload) {
        foreach(var p in Utilities.GetPlayers().Where(Valid))Send("close",p);
        _lastOpen.Clear();
    }
}
