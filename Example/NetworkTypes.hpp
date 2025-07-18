#ifndef PLUGIN_NETWORK_TYPES
#define PLUGIN_NETWORK_TYPES

#include <Windows.h>
#include <stdint.h>
#include <algorithm>
#include <cstdlib>

namespace NetworkTypes {
    class Steam64Id
    {
    private:
        char pad_0000[16]; //0x0000
    public:
        char steamId[18]; //0x0010
    };

    class queued_player
    {
    public:
        int32_t dpid; //0x0000
    private:
        char pad_0004[12]; //0x0004
    public:
        class Steam64Id* pSteam64; //0x0010
    public:
        int32_t positionIdx; //0x0018
    private:
        char pad_001C[8]; //0x001C
    public:
        char pName[32]; //0x0024
    };

    class queued_players
    {
    public:
        class queued_player* List[32];
    };

    class NetworkServer
    {
    
    };

    class LoginMachine
    {
    private:
        char pad_0000[40]; //0x0000
    public:
        class queued_players* pQueue; //0x0028
    public:
        int32_t SomeOtherSize; //0x0034
        int32_t MaxQueueSize; //0x0038
        int32_t loginQueueConcurrentPlayers; //0x003C
        int32_t QueueSize; //0x0040
    private:
        char pad_0044[1276]; //0x0044
    public:
        queued_player* GetQueuedPlayer(int32_t dpid)
        {
            if (!dpid || !pQueue || QueueSize < 1)
                return nullptr;

            for (int i = 0; i < QueueSize; i++)
            {
                queued_player* player = pQueue->List[i];
                if (player && player->dpid == dpid)
                    return player;
            }

            return nullptr;
        }
    }; //Size: 0x0540
}

#endif