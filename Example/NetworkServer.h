#pragma once
#include <InfinityPlugin.h>
#include <algorithm>
#include <thread>
#include <vector>
#include <string>
#include <fstream>
#include <charconv>
#include <cstring>

#include "detours.h"
#include "ExampleClass.h"
#include "NetworkTypes.hpp"

namespace PluginUtils{

    template<typename FuncPtr, typename DetourFunc>
    bool AttachDetour(const std::string& pattern, FuncPtr& func_ptr, DetourFunc detour_func, const char* error_name)
    {
        void* addr = Infinity::Utils::FindPattern(pattern, GetModuleHandle(nullptr), 0);
        if (!addr) {
            Infinity::Logging::Errorln("Hook(%s): pattern not found", error_name);
            return false;
        }
        func_ptr = reinterpret_cast<FuncPtr>(addr);
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)func_ptr, detour_func);
        DetourTransactionCommit();
        return true;
    }

    namespace AdminPriority
    {
        enum PriorityQueueRoles: unsigned __int64
        {
            User = 0,
            Priority = 2,
            Admin = 3
        };
    }
    
    namespace NetworkUtils{

        const std::string PATTERN_FN_STAGE_DISCONNECT = "49 8B C0 44 0F B6 C2"; //DIAG & RETAIL
        
        //RETAIL
        std::string PATTERN_INIT_NETWORK_SERVER     = "48 83 EC ? E8 ? ? ? ? 33 C0 48 89 51 ? 48 89 41 ? 48 89 41 ? 48 89 41";
        std::string PATTERN_FN_COMMIT_DISCONNECT    = "48 8B C2 8B D1 48 8B C8 E9 ? ? ? ? ? ? ? 48 8B C2";
        std::string PATTERN_FN_PLAYER_CONNECT       = "48 89 5C 24 ? 55 41 56 41 57 48 83 EC ? 49 8B E9";
        std::string PATTERN_FN_PLAYER_DISCONNECT    = "40 55 53 56 41 55 41 57 48 8B EC";
        std::string PATTERN_FN_ON_QUEUE_ADD         = "4C 89 4C 24 ? 89 54 24 ? 55 53";
        std::string PATTERN_FN_ON_QUEUE_REMOVE      = "48 89 74 24 ? 57 48 83 EC ? 8B FA 48 8B F1 E8 ? ? ? ? 8B 4E";
        std::string PATTERN_IS_PRIORITY_USER = "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 4C 24 ? 57 41 56 41 57 48 83 EC ? 49 8B C9";

        NetworkTypes::NetworkServer* p_EnfNetworkServer = nullptr;
        NetworkTypes::LoginMachine* p_EnfLoginMachine = nullptr;
        NetworkTypes::UserListSteam* p_EnfUserListSteam = nullptr;
        __int64 p_EnfUserListSteamSomePtr = -1;
        
        using FnInitLoginMachine = __int64(__fastcall *)(__int64* a1, __int64* networkServer);
        FnInitLoginMachine f_InitLoginMachine = nullptr;

        using FnStageDisconnect = __int64(__fastcall*)(int* dpid, unsigned __int8 flag, __int64* networkServer);
        static FnStageDisconnect f_StageDisconnect = nullptr;

        using FnCommitDisconnect = __int64(__fastcall*)(int* dpid, __int64* networkServer);
        static FnCommitDisconnect f_CommitDisconnect = nullptr;

        using FnOnConnect = __int64(__fastcall*)(int* dpid, char* steam64Id, char flag, char* name, __int64 a5, __int64* networkServer);
        static FnOnConnect f_OnPlayerConnect = nullptr;

        using FnOnDisconnect = __int64(__fastcall*)(__int64* networkServer, unsigned int dpid);
        static FnOnDisconnect f_OnPlayerDisconnected = nullptr;

        using FnOnAddToQueue = void(__fastcall*)(__int64* loginMachine, unsigned int dpid, void *steam64Id, char *name);
        static FnOnAddToQueue f_OnAddToQueue = nullptr;

        using FnOnRemoveFromQueue = __int64(__fastcall*)(__int64 loginMachine, unsigned int dpid);
        static FnOnRemoveFromQueue f_OnRemoveFromQueue = nullptr;

        using FnIsPriorityUser = __int64(__fastcall*)(char *userSteamList, char *steam64Id, __int64 a3, __int64 a4);
        static FnIsPriorityUser f_IsPriorityUser = nullptr;

        inline bool IsAdminSteamID(const std::string& steam64Id)
        {
            std::ifstream file("Plugins/admins.txt");
            if (!file.is_open())
                return false;

            std::string line;
            if (!std::getline(file, line)) {
                file.close();
                return false;
            }
            file.close();

            std::string_view view(line);
            size_t start = 0;

            while (start < view.size()) {
                const size_t pos = view.find_first_of(";,", start);

                if (const size_t end = (pos == std::string_view::npos) ? view.size() : pos; view.substr(start, end - start) == steam64Id)
                    return true;

                if (pos == std::string_view::npos)
                    break;

                start = pos + 1;
            }

            return false;
        }

        inline uint8_t __fastcall GetPriorityType(char* userSteamList, char* steam64Id, __int64 a2, __int64 a3)
        {
            uint8_t priorityType = AdminPriority::User;
            
            std::string targetId(steam64Id);
            if (IsAdminSteamID(steam64Id))
            {
                priorityType = AdminPriority::Admin;
            }
            else if (static_cast<uint8_t>(f_IsPriorityUser(userSteamList, steam64Id, a2, a3)))
            {
                priorityType = AdminPriority::Priority;
            }
            
            return priorityType;
        }

        inline void __fastcall MovePlayerInQueue(NetworkTypes::queued_player** queue, unsigned int fromIndex, unsigned int toIndex, unsigned int currentSize)
        {
            if (fromIndex >= currentSize || toIndex >= currentSize || fromIndex == toIndex
                || std::cmp_greater_equal(fromIndex, p_EnfLoginMachine->QueueSize)
                || std::cmp_greater_equal(toIndex, p_EnfLoginMachine->QueueSize))
                return;
            
            NetworkTypes::queued_player* playerToMove = queue[fromIndex];
            if (fromIndex < toIndex) {
                memmove(&queue[fromIndex], &queue[fromIndex + 1], 
                        sizeof(NetworkTypes::queued_player*) * (toIndex - fromIndex));
            }
            else {
                memmove(&queue[toIndex + 1], &queue[toIndex], 
                        sizeof(NetworkTypes::queued_player*) * (fromIndex - toIndex));
            }
            queue[toIndex] = playerToMove;
        }
        
        //Detour Event: Called when engine initializes LoginMachine & NetworkServer.
        static __int64 __fastcall InitLoginMachine(__int64* a1, __int64* networkServer)
        {
            __int64 result = f_InitLoginMachine(a1, networkServer);

            Infinity::Logging::Debugln(
                "InitLoginMachine(LoginMachine=0x%llX)",
                (unsigned long long)a1
            );
            
            p_EnfLoginMachine = reinterpret_cast<NetworkTypes::LoginMachine*>(result);
            if (!p_EnfNetworkServer) p_EnfNetworkServer = reinterpret_cast<NetworkTypes::NetworkServer*>(networkServer);

            f_StageDisconnect = reinterpret_cast<FnStageDisconnect>(Infinity::Utils::FindPattern(PATTERN_FN_STAGE_DISCONNECT, GetModuleHandle(NULL), 0));
            f_CommitDisconnect = reinterpret_cast<FnCommitDisconnect>(Infinity::Utils::FindPattern(PATTERN_FN_COMMIT_DISCONNECT, GetModuleHandle(NULL), 0));

            if (!ExampleClass::enfInstancePtr) {
                ExampleClass::CreateSingleton();
            }
            
            return f_InitLoginMachine(a1, networkServer);
        }

        //Detour Event: Called when a player is added to queue.
        static void __fastcall OnAddToQueue(__int64* loginMachine, unsigned int dpid, void *steam64Id, char *name)
        {
            f_OnAddToQueue(loginMachine, dpid, steam64Id, name);
            if (!p_EnfLoginMachine) return;

            NetworkTypes::queued_player* player = p_EnfLoginMachine->GetQueuedPlayer(dpid);
            if (!player || !p_EnfUserListSteam || p_EnfUserListSteamSomePtr == -1) return;

            const uint8_t priority = GetPriorityType(reinterpret_cast<char*>(p_EnfUserListSteam), player->pSteam64->steamId, reinterpret_cast<__int64>(name), p_EnfUserListSteamSomePtr);
            if (priority == AdminPriority::User) return;

            player->isPriority = priority;

            Infinity::Logging::Println("OnAddToQueue()::Added player to queue %s (%s). IsAdminPriority=%d IsPriority=%d",
                player->pSteam64->ToString().c_str(),
                player->displayName,
                player->isPriority == AdminPriority::Admin,
                player->isPriority == AdminPriority::Priority);

            int currentPos = p_EnfLoginMachine->GetQueuedPlayerIndex(dpid);
            if (currentPos <= 0) return;

            for (int i = 0; i < p_EnfLoginMachine->QueueSize; i++)
            {
                auto& queuedPlayer = p_EnfLoginMachine->pQueue[i];
                bool shouldMove = (priority == AdminPriority::Admin && queuedPlayer->isPriority != AdminPriority::Admin) ||
                                  (priority == AdminPriority::Priority && queuedPlayer->isPriority == AdminPriority::User);
                
                if (shouldMove)
                {
                    int updatedPos = p_EnfLoginMachine->GetQueuedPlayerIndex(dpid);
                    Infinity::Logging::Println("OnAddToQueue()::Moving %s (%s) from %d to %d",
                        player->pSteam64->ToString().c_str(),
                        player->displayName,
                        updatedPos,
                        i);
                    MovePlayerInQueue(p_EnfLoginMachine->pQueue, updatedPos, i, p_EnfLoginMachine->QueueSize);
                    break;
                }
            }
        }

        //Detour engine function, this gets called when a player is removed from queue
        static __int64 __fastcall OnRemoveFromQueue(__int64 loginMachine, unsigned int dpid)
        {
            __int64 _result = f_OnRemoveFromQueue(loginMachine, dpid); //Call original
            // Infinity::Logging::Debugln("OnRemoveFromQueue(LoginMachine=0x%llX, dpid=%d)", (unsigned long long)loginMachine, dpid);
            // Infinity::Logging::Warnln("Removed player %d from queue", dpid);

            //Callback is just for demo purpose...we don't want to call it.
            //Infinity::CallEnforceMethod(ExampleClass::enfInstancePtr, "OnRemoveFromQueue", dpid);

            return _result;
        }

        //Detour Event: Called when a player connects at the earliest stage possible.
        static __int64 __fastcall OnPlayerConnect(int* dpid, char* steam64Id, char flag, char* name, __int64 a5, __int64* networkServer)
        {
            //call the original
            __int64 ret = f_OnPlayerConnect(dpid, steam64Id, flag, name, a5, networkServer);

            // Infinity::Logging::Debugln(
            //     "OnPlayerConnect(a1=%d, a2=%s, a3=%d, a4=\"%s\", a5=0x%llX, a6=0x%llX)",
            //     dpid,
            //     steam64Id,
            //     (int)flag,
            //     name ? name : "<null>",
            //     (unsigned long long)a5,
            //     (unsigned long long)networkServer
            // );

            return ret;
        }

        //Detour Event: Called when player disconnect is completed.
        static __int64 __fastcall OnPlayerDisconnected(__int64* networkServer, unsigned int dpid)
        {
            // Infinity::Logging::Debugln("OnPlayerDisconnected(a1=0x%llX, a2=%d)", (unsigned long long)networkServer, dpid);

            //call original
            return f_OnPlayerDisconnected(networkServer, dpid);
        }

        //Detour Event: Called to check a players priority.
        static __int64 __fastcall IsPriorityUser(char* userSteamList, char* steam64Id, __int64 a2, __int64 a3)
        {
            if (!p_EnfUserListSteam) p_EnfUserListSteam = reinterpret_cast<NetworkTypes::UserListSteam*>(userSteamList);
            if (p_EnfUserListSteamSomePtr == -1) p_EnfUserListSteamSomePtr = a3;
            
            // Forcing false, to avoid normal priority players getting moved without our function.
            return false;
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
                PATTERN_IS_PRIORITY_USER = "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 83 EC ? 49 8B C9";
            }

            //Login machine
            if (!AttachDetour(PATTERN_INIT_NETWORK_SERVER, f_InitLoginMachine, InitLoginMachine, "FnInitLoginMachine"))
                return false;
            
            //Player connect event
            if (!AttachDetour(PATTERN_FN_PLAYER_CONNECT, f_OnPlayerConnect, OnPlayerConnect, "FnOnPlayerConnect"))
                return false;

            //Player disconnect event
            if (!AttachDetour(PATTERN_FN_PLAYER_DISCONNECT, f_OnPlayerDisconnected, OnPlayerDisconnected, "FnOnPlayerDisconnected"))
                return false;

            //Add to queue event
            if (!AttachDetour(PATTERN_FN_ON_QUEUE_ADD, f_OnAddToQueue, OnAddToQueue, "FnOnAddToQueue"))
                return false;
            
            //Remove from queue event
            if (!AttachDetour(PATTERN_FN_ON_QUEUE_REMOVE, f_OnRemoveFromQueue, OnRemoveFromQueue, "FnOnRemoveFromQueue"))
                return false;

            //Determines user's priority event
            if (!AttachDetour(PATTERN_IS_PRIORITY_USER, f_IsPriorityUser, IsPriorityUser, "FnIsPriorityUser"))
                return false;
            
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

            f_StageDisconnect((int*)dpid, 1, (__int64*)p_EnfNetworkServer);
            f_CommitDisconnect((int*)dpid, (__int64*)p_EnfNetworkServer);
        }
    }
}
