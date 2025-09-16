#ifndef PLUGIN_NETWORK_TYPES
#define PLUGIN_NETWORK_TYPES

#include <Windows.h>
#include <stdint.h>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <cstring>

namespace NetworkTypes {
    class Steam64Id
    {
    private:
        char pad_0000[16]; //0x0000
    public:
        char steamId[17]; //0x0010
    public:
        std::string ToString() const
        {
            size_t len = ::strnlen(steamId, sizeof steamId);
            return std::string(steamId, len);
        }
    };

    class queued_player
    {
    public:
        uint32_t dpId; //0x0000
    private:
        char pad_0004[12]; //0x0004
    public:
        Steam64Id* pSteam64; //0x0010
    private:
        char pad_0018[12]; //0x0018
    public:
        char displayName[65]; //0x0024
        char playerName[65]; //0x0065
    private:
        char pad_00A6[8]; //0x00A6
    public:
        uint8_t isPriority; //0x00AE
    private:
        char pad_00AF[145]; //0x00AF
    }; //Size: 0x0140

    class UserListSteam
    {
    private:
        char pad_0000[288];
    public:
        __int64 pSomePtr;
    };

    class NetworkServer
    {
    private:
        char pad_0000[1368]; //0x0000
    public:
        class UserListSteam* pSteamUserList;
    };

    class LoginMachine
    {
    private:
        char pad_0000[32]; //0x0000
    public:
        class NetworkServer* pNetworkServer;
        class queued_player** pQueue; //0x0028
    public:
        int32_t QueueCapacity; //0x0030
        int32_t QueueSize; //0x0034
        int32_t MaxQueueSize; //0x0038
        int32_t loginQueueConcurrentPlayers; //0x003C
    private:
        char pad_0044[1276]; //0x0044
    public:
        queued_player* GetQueuedPlayer(uint32_t dpid)
        {
            if (!dpid || !pQueue)
                return nullptr;
        
            for (int i = 0; i < QueueSize; i++) {
                queued_player* player = pQueue[i];
                if (player && player->dpId == dpid)
                    return player;
            }
        
            return nullptr;
        }
        queued_player* GetQueuedPlayerBySteamId(char* steam64Id)
        {
            if (!steam64Id || !pQueue)
                return nullptr;
        
            std::string targetId(steam64Id);
        
            for (int i = 0; i < QueueSize; i++) {
                queued_player* player = pQueue[i];
        
                if (player && player->pSteam64->ToString() == targetId)
                {
                    return player;
                }
            }
            return nullptr;
        }
        int GetQueuedPlayerIndex(uint32_t dpid)
        {
            if (!dpid || !pQueue) return -1;
        
            for (int i = 0; i < QueueSize; i++)
            {
                queued_player* player = pQueue[i];
                if (player && player->dpId == dpid) return i;
            }
        
            return -1;
        }
    }; //Size: 0x0540
}

#endif