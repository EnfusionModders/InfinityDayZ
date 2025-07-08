#include "DllMain.h"
#include "pch.h"
#include "exports.h"
#include <string>

// Helper to show a modal error and exit
static void FatalError(const std::wstring& msg)
{
    MessageBoxW(
        NULL,
        msg.c_str(),
        L"Fatal Initialization Error  ",
        MB_ICONERROR | MB_OK | MB_SYSTEMMODAL
    );
    ExitProcess(1);
}

VOID Initialise(PVOID lpParameter)
{
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(NULL, exePath, MAX_PATH)) {
        FatalError(L"Unable to locate executable path (GetModuleFileName failed).");
    }

    std::wstring root(exePath);
    size_t pos = root.find_last_of(L"\\/");
    if (pos == std::wstring::npos) {
        FatalError(L"Cannot parse application folder from path:\n" + root);
    }
    root.resize(pos);

    std::wstring pluginsDir = root + L"\\Plugins";

    std::wstring search = pluginsDir + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        FatalError(L"Plugins directory not found:\n" + pluginsDir);
    }

    std::wstring pluginPath;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (_wcsicmp(fd.cFileName, L"InfinityHost.dll") == 0) {
                pluginPath = pluginsDir + L"\\" + fd.cFileName;
                break;
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    if (pluginPath.empty()) {
        FatalError(
            L"Could not find \"InfinityHost.dll\" in:\n" +
            pluginsDir
        );
    }

    HMODULE hPlugin = LoadLibraryW(pluginPath.c_str());
    if (!hPlugin) {
        DWORD err = GetLastError();
        wchar_t buf[256];
        swprintf(buf, 256,
            L"Failed to load plugin:\n%s\n\nLoadLibraryW error %u",
            pluginPath.c_str(), err);
        FatalError(buf);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(Initialise), (PVOID)hModule, NULL, NULL);
        DisableThreadLibraryCalls(hModule);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
