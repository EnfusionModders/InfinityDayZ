#pragma once
#include <InfinityPlugin.h>
#include <algorithm>
#include <thread>
#include "detours.h"
#include "ExampleClass.h"
#include "NetworkTypes.hpp"

namespace PluginUtils{
    namespace NetworkUtils{

        const std::string PATTERN_FN_STAGE_DISCONNECT = "49 8B C0 44 0F B6 C2"; //DIAG & RETAIL
        
        //RETAIL
        std::string PATTERN_INIT_NETWORK_SERVER     = "48 83 EC ? E8 ? ? ? ? 33 C0 48 89 51 ? 48 89 41 ? 48 89 41 ? 48 89 41";
        std::string PATTERN_FN_COMMIT_DISCONNECT    = "48 8B C2 8B D1 48 8B C8 E9 ? ? ? ? ? ? ? 48 8B C2";
        std::string PATTERN_FN_PLAYER_CONNECT       = "48 89 5C 24 ? 55 41 56 41 57 48 83 EC ? 49 8B E9";
        std::string PATTERN_FN_PLAYER_DISCONNECT    = "40 55 53 56 41 55 41 57 48 8B EC";
        std::string PATTERN_FN_ON_QUEUE_ADD         = "4C 89 4C 24 ? 89 54 24 ? 55 53";
        std::string PATTERN_FN_ON_QUEUE_REMOVE      = "48 89 74 24 ? 57 48 83 EC ? 8B FA 48 8B F1 E8 ? ? ? ? 8B 4E";

        NetworkTypes::NetworkServer* p_EnfNetworkServer = nullptr;
        NetworkTypes::LoginMachine* p_EnfLoginMachine = nullptr;

        using FnInitLoginMachine = __int64(__fastcall*)(__int64* a1, NetworkTypes::NetworkServer* pNs);
        static FnInitLoginMachine f_InitLoginMachine;

        using FnStageDisconnect = __int64(__fastcall*)(int* dpid, unsigned __int8 flag, NetworkTypes::NetworkServer* pNs); //dpid, 1, ptr to Networkserver
        static FnStageDisconnect f_StageDisconnect = nullptr;

        using FnCommitDisconnect = __int64(__fastcall*)(int* dpid, NetworkTypes::NetworkServer* a2); //dpid, ptr to Networkserver
        static FnCommitDisconnect f_CommitDisconnect = nullptr;

        //Connect
        using FnOnConnect = __int64(__fastcall*)(int* dpid, char* steam64Id, char flag, char* name, __int64, NetworkTypes::NetworkServer* pNs);
        static FnOnConnect  f_OnPlayerConnect = nullptr;

        //Disconnect
        using FnOnDisconnect = __int64(__fastcall*)(NetworkTypes::NetworkServer* pNs, unsigned int dpid);
        static FnOnDisconnect f_OnPlayerDisconnected = nullptr;

        //On Add to queue
        using FnOnAddToQueue = void(__fastcall*)(__int64 loginMachine, unsigned int dpid, void* steam64Id, char* name); //ptr to LoginMachine, dpid, Steam64, name
        static FnOnAddToQueue f_OnAddToQueue = nullptr;

        //On Remove from queue
        using FnOnRemoveFromQueue = __int64(__fastcall*)(__int64 loginMachine, unsigned int dpid); //ptr to LoginMachine, dpid
        static FnOnRemoveFromQueue f_OnRemoveFromQueue = nullptr;

        //Detour engine function, this gets called when engine creates LoginMachine & NetworkServer
        static __int64 __fastcall InitLoginMachine(__int64* a1, NetworkTypes::NetworkServer* a2)
        {
            __int64 result = f_InitLoginMachine(a1, a2); //call original

            p_EnfLoginMachine = reinterpret_cast<NetworkTypes::LoginMachine*>(result);

            Infinity::Logging::Debugln(
                "InitLoginMachine(a1=0x%llX, NetworkServer=0x%llX, LoginMachine=0x%llX)",
                (unsigned long long)a1,
                (unsigned long long)a2,
                (unsigned long long)result
            );

            
            if (!p_EnfNetworkServer)
                p_EnfNetworkServer = a2; //hold onto ptr to NetworkServer
            
            //Find function calls for disconnecting
            f_StageDisconnect = reinterpret_cast<FnStageDisconnect>(Infinity::Utils::FindPattern(PATTERN_FN_STAGE_DISCONNECT, GetModuleHandle(NULL), 0));
            f_CommitDisconnect = reinterpret_cast<FnCommitDisconnect>(Infinity::Utils::FindPattern(PATTERN_FN_COMMIT_DISCONNECT, GetModuleHandle(NULL), 0));

            //Create a dynamic instance of our custom class, using it for callbacks to Enforce
            if (!ExampleClass::enfInstancePtr) {
                ExampleClass::CreateSingleton();
            }

            return result;
        }

        //Detour engine function, this gets called when a player is added to queue
        static void __fastcall OnAddToQueue(__int64 loginMachine, unsigned int dpid, void* steam64Id, char* name)
        {
            //Call original
            f_OnAddToQueue(loginMachine, dpid, steam64Id, name);

            Infinity::Logging::Debugln(
                "OnAddToQueue(a1=0x%llX, a2=%d, a3=%s, a4=\"%s\")",
                (unsigned long long)loginMachine,
                dpid,
                steam64Id,
                name ? name : "<null>"
            );

            
            auto loginMachinePtr = p_EnfLoginMachine;
            std::thread([dpid, loginMachinePtr]() {
                NetworkTypes::queued_player* player = loginMachinePtr->GetQueuedPlayer(static_cast<int32_t>(dpid));
  
                //wait until index is populated by engine
                while (player && player->positionIdx == -1)
                {
                    if (loginMachinePtr->QueueSize == 0) {
                        Infinity::Logging::Warnln("Queue is empty...");
                        break;
                    }
                    Sleep(50);
                }

                if (player && loginMachinePtr->QueueSize > 0)
                {
                    Infinity::Logging::Warnln("pos: %d", player->positionIdx);
                    Infinity::CallEnforceMethod(ExampleClass::enfInstancePtr, "OnAddToQueue", player->dpid, player->pSteam64->steamId, player->pName, player->positionIdx);
                }

            }).detach();
            

            /*
            NetworkTypes::queued_players* queue = p_EnfNetworkServer->pQueue;
            if (queue && size > 1)
            {
                // 1) Grab the newly-added player from the last slot
                NetworkTypes::queued_player* newPlayer = queue->List[size - 1];

                // 2) Shift the existing [0 .. size-2] entries one slot right
                std::memmove(
                    &queue->List[1],               // dest: start writing at index 1
                    &queue->List[0],               // src: read from index 0
                    (size - 1) * sizeof(queue->List[0])
                );

                // 3) Put the prioritized player at the front
                queue->List[0] = newPlayer;
            }

            Infinity::Logging::Debugln("Current queue size: %d", size);
            NetworkTypes::queued_player* player = queue->List[0];
            Infinity::Logging::Debugln("Player dpid: (%d) a4=\"%s\" Steam64Id: (%s) @ queue pos: %d", player->dpid, player->pName, player->pSteam64->steamId, player->positionIdx);
            */
        }

        //Detour engine function, this gets called when a player is removed from queue
        static __int64 __fastcall OnRemoveFromQueue(__int64 loginMachine, unsigned int dpid)
        {
            Infinity::Logging::Debugln("OnRemoveFromQueue(a1=0x%llX, a2=%d)", (unsigned long long)loginMachine, dpid);

            if (p_EnfLoginMachine->QueueSize > 0)
            {
                NetworkTypes::queued_player* player = p_EnfLoginMachine->GetQueuedPlayer(dpid);
                if (player)
                {
                    Infinity::Logging::Warnln("Removed player %d from queue", player->dpid);
                    Infinity::CallEnforceMethod(ExampleClass::enfInstancePtr, "OnRemoveFromQueue", player->dpid);
                }
            }

            return f_OnRemoveFromQueue(loginMachine, dpid); //Call original
        }

        //Detour engine function, this gets called when a player connects most early stage possible.
        static __int64 __fastcall OnPlayerConnect(int* dpid, char* steam64Id, char flag, char* name, __int64 a5, NetworkTypes::NetworkServer* a6)
        {
            //call the original
            __int64 ret = f_OnPlayerConnect(dpid, steam64Id, flag, name, a5, a6);

            Infinity::Logging::Debugln(
                "OnPlayerConnect(a1=%d, a2=%s, a3=%d, a4=\"%s\", a5=0x%llX, a6=0x%llX)",
                dpid,
                steam64Id,
                (int)flag,
                name ? name : "<null>",
                (unsigned long long)a5,
                (unsigned long long)a6
            );

            return ret;
        }

        //Detour engine function, this gets called when a player disconnect completes
        static __int64 __fastcall OnPlayerDisconnected(NetworkTypes::NetworkServer* a1, unsigned int dpid)
        {
            Infinity::Logging::Debugln("OnPlayerDisconnected(a1=0x%llX, a2=%d)", (unsigned long long)a1, dpid);

            //call original
            return f_OnPlayerDisconnected(a1, dpid);
        }

        //Init & find patterns
        bool InitPatterns()
        {
            if (Infinity::Logging::IsDiagBuild())
            {
                //DIAG
                PATTERN_INIT_NETWORK_SERVER  = "48 89 5C 24 ? 57 48 83 EC ? 48 8B DA 48 8B F9 E8 ? ? ? ? 33 C0 48 89 5F ? 48 8B 5C 24 ? 48 89 47 ? 48 89 47 ? 48 89 47";
                PATTERN_FN_COMMIT_DISCONNECT = "48 8B C2 8B D1 48 8B C8 E9 ? ? ? ? ? ? ? 49 8B C0";
                PATTERN_FN_PLAYER_CONNECT    = "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 49 8B F9 48 8B EA";
                PATTERN_FN_PLAYER_DISCONNECT = "40 55 53 41 54 41 56 48 8B EC";
                PATTERN_FN_ON_QUEUE_ADD      = "4C 89 4C 24 ? 89 54 24 ? 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 ? 48 81 EC ? ? ? ? 4C 8B E9";
                PATTERN_FN_ON_QUEUE_REMOVE   = "48 89 74 24 ? 57 48 83 EC ? 48 8B F1 8B FA 48 8B 0D";
            }

            //Login machine
            void* addrLoginMachine = Infinity::Utils::FindPattern(PATTERN_INIT_NETWORK_SERVER, GetModuleHandle(NULL), 0);
            if (!addrLoginMachine) {
                Infinity::Logging::Errorln("Hook(FnInitLoginMachine): pattern not found");
                return false;
            }
            f_InitLoginMachine = reinterpret_cast<FnInitLoginMachine>(addrLoginMachine);

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(reinterpret_cast<PVOID*>(&f_InitLoginMachine), InitLoginMachine);
            DetourTransactionCommit();

            //Player connect event
            void* addrConnect = Infinity::Utils::FindPattern(PATTERN_FN_PLAYER_CONNECT, GetModuleHandle(NULL), 0);
            if (!addrConnect) {
                Infinity::Logging::Errorln("Hook(FnOnConnect): pattern not found");
                return false;
            }
            f_OnPlayerConnect = reinterpret_cast<FnOnConnect>(addrConnect);

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(reinterpret_cast<PVOID*>(&f_OnPlayerConnect), OnPlayerConnect);
            DetourTransactionCommit();

            //Player disconnect event
            void* addrDisconnect = Infinity::Utils::FindPattern(PATTERN_FN_PLAYER_DISCONNECT, GetModuleHandle(NULL), 0);
            if (!addrDisconnect) {
                Infinity::Logging::Errorln("Hook(FnOnDisconnect): pattern not found");
                return false;
            }
            f_OnPlayerDisconnected = reinterpret_cast<FnOnDisconnect>(addrDisconnect);

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(reinterpret_cast<PVOID*>(&f_OnPlayerDisconnected), OnPlayerDisconnected);
            DetourTransactionCommit();


            //Add to queue event
            void* addrAddToQueue = Infinity::Utils::FindPattern(PATTERN_FN_ON_QUEUE_ADD, GetModuleHandle(NULL), 0);
            if (!addrAddToQueue) {
                Infinity::Logging::Errorln("Hook(FnOnAddToQueue): pattern not found");
                return false;
            }
            f_OnAddToQueue = reinterpret_cast<FnOnAddToQueue>(addrAddToQueue);

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(reinterpret_cast<PVOID*>(&f_OnAddToQueue), OnAddToQueue);
            DetourTransactionCommit();

            //Remove from queue event
            void* addrRemoveQueue = Infinity::Utils::FindPattern(PATTERN_FN_ON_QUEUE_REMOVE, GetModuleHandle(NULL), 0);
            if (!addrRemoveQueue) {
                Infinity::Logging::Errorln("Hook(FnOnRemoveFromQueue): pattern not found");
                return false;
            }
            f_OnRemoveFromQueue = reinterpret_cast<FnOnRemoveFromQueue>(addrRemoveQueue);

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(reinterpret_cast<PVOID*>(&f_OnRemoveFromQueue), OnRemoveFromQueue);
            DetourTransactionCommit();

            return true;
        }

        //Issues a disconnect via dpid
        void DisconnectPlayer(int32_t dpid)
        {
            if (!p_EnfNetworkServer)
            {
                Infinity::Logging::Errorln("[NetworkUtils] DisconnectPlayer: p_EnfNetworkServer is nullptr!");
                return;
            }

            if (!f_StageDisconnect || !f_CommitDisconnect)
            {
                Infinity::Logging::Errorln("[NetworkUtils] DisconnectPlayer: failed, no pointer to function calls!");
                return;
            }

            f_StageDisconnect((int*)dpid, 1, p_EnfNetworkServer);
            f_CommitDisconnect((int*)dpid, p_EnfNetworkServer);
        }

        //Swap two given index with eachother
        void SwapPlayerInQueue(int from, int to)
        {
            NetworkTypes::queued_players* queue = p_EnfLoginMachine->pQueue;
            auto swapSlots = [&](int i, int j) {
                if (i < p_EnfLoginMachine->QueueSize
                    && j < p_EnfLoginMachine->QueueSize)
                {
                    NetworkTypes::queued_player*& P = queue->List[i];
                    NetworkTypes::queued_player*& Q = queue->List[j];
                    std::swap(P, Q);
                }
            };

            if (queue)
                swapSlots(from, to);
        }
    }
}