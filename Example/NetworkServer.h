#pragma once
#include <InfinityPlugin.h>
#include "detours.h"

namespace PluginUtils{
    namespace NetworkUtils{

        const std::string PATTERN_INIT_NETWORK_SERVER = "48 8B C4 55 56 57 48 8D A8";
        const std::string PATTERN_FN_STAGE_DISCONNECT = "49 8B C0 44 0F B6 C2";
        const std::string PATTERN_FN_COMMIT_DISCONNECT = "48 8B C2 8B D1 48 8B C8 E9 ? ? ? ? ? ? ? 48 8B C2";
        const std::string PATTERN_FN_PLAYER_CONNECT = "48 89 5C 24 ? 55 41 56 41 57 48 83 EC ? 49 8B E9";


        typedef __int64 EnfNetworkServer;
        EnfNetworkServer p_EnfNetworkServer = NULL;

        using FnInitNetworkServer = __int64(__fastcall*)(__int64);
        static FnInitNetworkServer f_InitNetworkServer = nullptr;

        using FnStageDisconnect = __int64(__fastcall*)(int* a1, unsigned __int8 a2, __int64 a3); //dpid, 1, ptr to Networkserver
        static FnStageDisconnect f_StageDisconnect = nullptr;

        using FnCommitDisconnect = __int64(__fastcall*)(int* a1, __int64 a2); //dpid, ptr to Networkserver
        static FnCommitDisconnect f_CommitDisconnect = nullptr;

        using FnOnConnect = __int64(__fastcall*)(int*, char*, char, char*, __int64, __int64);
        static FnOnConnect  f_OnPlayerConnect = nullptr;

        
        //Detour engine function, find our ptr to internal NetworkServer
        static __int64 __fastcall InitNetworkServer(__int64 a1)
        {
            __int64 result = f_InitNetworkServer(a1);
            if (!p_EnfNetworkServer)
            {
                p_EnfNetworkServer = a1; //hold onto ptr to NetworkServer

                Infinity::Logging::Debugln("InitNetworkServer(a1=0x%llX)", (unsigned long long)a1);
                Infinity::Logging::Debugln("InitNetworkServer returned 0x%llX", (unsigned long long)result);

                //Find function calls for disconnecting
                f_StageDisconnect = reinterpret_cast<FnStageDisconnect>(Infinity::Utils::FindPattern(PATTERN_FN_STAGE_DISCONNECT, GetModuleHandle(NULL), 0));
                f_CommitDisconnect = reinterpret_cast<FnCommitDisconnect>(Infinity::Utils::FindPattern(PATTERN_FN_COMMIT_DISCONNECT, GetModuleHandle(NULL), 0));


                //Unhook, we got our ptr
                DetourTransactionBegin();
                DetourUpdateThread(GetCurrentThread());
                DetourDetach(reinterpret_cast<PVOID*>(&f_InitNetworkServer), InitNetworkServer);
                DetourTransactionCommit();
            }
            return result;
        }

        //Detour engine function, this gets called when a player connects most early stage possible.
        static __int64 __fastcall OnPlayerConnect(int* a1, char* a2, char a3, char* a4, __int64 a5, __int64 a6)
        {
            // Pre-call logging
            Infinity::Logging::Debugln(
                "OnPlayerConnect(a1=%d, a2=%s, a3=%d, a4=\"%s\", a5=0x%llX, a6=0x%llX)",
                a1,
                a2,
                (int)a3,
                a4 ? a4 : "<null>",
                (unsigned long long)a5,
                (unsigned long long)a6
            );

            //call the original
            return f_OnPlayerConnect(a1, a2, a3, a4, a5, a6);
        }

        //Init & find patterns
        bool InitPatterns()
        {
            void* addr = Infinity::Utils::FindPattern(PATTERN_INIT_NETWORK_SERVER, GetModuleHandle(NULL), 0);
            if (!addr) {
                Infinity::Logging::Errorln("Hook(FindNetworkServer): pattern not found");
                return false;
            }

            f_InitNetworkServer = reinterpret_cast<FnInitNetworkServer>(addr);

            //Network Server finder
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(reinterpret_cast<PVOID*>(&f_InitNetworkServer), InitNetworkServer);
            DetourTransactionCommit();

            //Player connect event
            void* addr2 = Infinity::Utils::FindPattern(PATTERN_FN_PLAYER_CONNECT, GetModuleHandle(NULL), 0);
            if (!addr2) {
                Infinity::Logging::Errorln("Hook(FnOnConnect): pattern not found");
                return false;
            }
            f_OnPlayerConnect = reinterpret_cast<FnOnConnect>(addr2);

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(reinterpret_cast<PVOID*>(&f_OnPlayerConnect), OnPlayerConnect);
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
    }
}