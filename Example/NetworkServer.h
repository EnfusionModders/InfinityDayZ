#pragma once
#include <InfinityPlugin.h>
#include "detours.h"

namespace PluginUtils{
    namespace NetworkUtils{

        const std::string PATTERN_INIT_NETWORK_SERVER = "48 8B C4 55 56 57 48 8D A8";
        const std::string PATTERN_FN_STAGE_DISCONNECT = "49 8B C0 44 0F B6 C2";
        const std::string PATTERN_FN_COMMIT_DISCONNECT = "48 8B C2 8B D1 48 8B C8 E9 ? ? ? ? ? ? ? 48 8B C2";

        typedef __int64 EnfNetworkServer;
        EnfNetworkServer p_EnfNetworkServer = NULL;

        using FnInitNetworkServer = __int64(__fastcall*)(__int64);
        static FnInitNetworkServer f_InitNetworkServer = nullptr;

        using FnStageDisconnect = __int64(__fastcall*)(int* a1, unsigned __int8 a2, __int64 a3); //dpid, 1, ptr to Networkserver
        static FnStageDisconnect f_StageDisconnect = nullptr;

        using FnCommitDisconnect = __int64(__fastcall*)(int* a1, __int64 a2); //dpid, ptr to Networkserver
        static FnCommitDisconnect f_CommitDisconnect = nullptr;

        //Detour method, find our ptr to internal NetworkServer
        static __int64 __fastcall InitNetworkServer(__int64 a1)
        {
            __int64 result = f_InitNetworkServer(a1);
            if (!p_EnfNetworkServer)
            {
                p_EnfNetworkServer = a1; //hold onto ptr to NetworkServer

                Infinity::Logging::Debugln("▶ sub_1408D1C70(a1=0x%llX)", (unsigned long long)a1);
                Infinity::Logging::Debugln("◀ sub_1408D1C70 returned 0x%llX", (unsigned long long)result);

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

        //Init
        bool FindNetworkServer()
        {
            void* addr = Infinity::Utils::FindPattern(PATTERN_INIT_NETWORK_SERVER, GetModuleHandle(NULL), 0);
            if (!addr) {
                Infinity::Logging::Errorln("Hook(FindNetworkServer): pattern not found");
                return false;
            }

            f_InitNetworkServer = reinterpret_cast<FnInitNetworkServer>(addr);

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(reinterpret_cast<PVOID*>(&f_InitNetworkServer), InitNetworkServer);
            LONG status = DetourTransactionCommit();
            if (status != NO_ERROR) {
                Infinity::Logging::Errorln("DetourAttach(InitNetworkServer) failed: %d", status);
                return false;
            }
            else {
                Infinity::Logging::Debugln("Hooked InitNetworkServer at %p", addr);
            }

            return true;
        }

        //Issue a disconnect via dpid
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