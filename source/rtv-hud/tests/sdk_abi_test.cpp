#include <engine2/ivengineserver2.h>
#include <engine2/inetworkserverservice.h>
#include <engine2/isource2server.h>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>

// Linux x86_64 Itanium member-pointer ABI: a virtual pointer encodes offset+1.
// No engine object or fabricated pointer is dereferenced by this test.
template<class T> std::ptrdiff_t virtualOffset(T method) {
    struct Representation { std::ptrdiff_t function, adjustment; } value{};
    static_assert(sizeof(method)==sizeof(value));
    std::memcpy(&value,&method,sizeof(value));
    assert((value.function&1)&&value.adjustment==0);
    return value.function-1;
}

int main() {
    const auto app=virtualOffset(&IVEngineServer2::GetAppID);
    const auto steam=virtualOffset(&IVEngineServer2::GetClientSteamID);
    const auto auth=virtualOffset(&IVEngineServer2::IsClientFullyAuthenticated);
    // The reported crash called 0x228 for SteamID, receiving AppID 730 instead.
    assert(app==0x228);
    assert(steam==0x230);
    assert(auth==0x258);
    assert(virtualOffset(&INetworkServerService::GetIGameServer)==0xc0);
    std::printf("Engine ABI: AppID=%#tx SteamID=%#tx authentication=%#tx globals=%#tx\n",
        app,steam,auth,virtualOffset(&IVEngineServer2::GetServerGlobals));
    std::printf("Server ABI: GetIGameServer=%#tx; transmit recipient offset=%zu\n",
        virtualOffset(&INetworkServerService::GetIGameServer),offsetof(CCheckTransmitInfo,m_nPlayerSlot));
}
