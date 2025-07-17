#include <Windows.h>
#include <string>
#include <filesystem>

#include "ScriptEngine.h"
#include "ScriptRegistrator.h"
#include "Console.hpp"

#include "InfinityPlugin.h"

#include <cstring>
#include <cstdlib>

namespace fs = std::filesystem;
namespace EnfTypes = Infinity::Enfusion::Enscript::Framework;

//Printing to DayZ console
const std::string PATTERN_PRINT_TO_CONSOLE = "48 8B C4 48 89 50 ? 4C 89 40 ? 4C 89 48 ? 53 57 48 81 EC ? ? ? ? C6 44 24"; //DIAG & RETAIL

//Pattern for calling global methods (non-engine/proto)
const std::string PATTERN_LOOKUP_GLOBAL_METHOD = "40 53 48 83 EC ? 48 8B D9 E8 ? ? ? ? 48 89 44 24"; //DIAG & RETAIL

//Patterns for calling dynamic & static class methods (non-engine/proto)
const std::string PATTERN_LOOKUP_METHOD = "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B FA 48 8B D9 48 85 C9 74 ? 48 89 54 24"; //DIAG & RETAIL
const std::string PATTERN_CALLUP_METHOD = "44 89 44 24 ? 4C 89 4C 24 ? 53"; //DIAG & RETAIL

//Pattern for creating a Enforce class instance of a Enforce type
const std::string PATTERN_FN_CREATE_INSTANCE = "48 83 EC ? 45 0F B6 C8 4C 8B C2"; //DIAG & RETAIL

void Infinity::LoadPlugins()
{
    Println("Loading plugins...");
    Infinity::InitPatterns(); //Locate patterns once

    char buffer[MAX_PATH];
    if (!GetModuleFileNameA(NULL, buffer, MAX_PATH)) {
        Warnln("GetModuleFileNameA failed (%u)", GetLastError());
        return;
    }
    std::string exePath(buffer);
    auto pos = exePath.find_last_of("\\/");
    if (pos == std::string::npos) {
        Warnln("Cannot determine executable directory from '%s'", exePath.c_str());
        return;
    }
    std::string exeDir = exePath.substr(0, pos);

    fs::path pluginsDir = fs::path(exeDir) / "Plugins";
    if (!fs::exists(pluginsDir) || !fs::is_directory(pluginsDir)) {
        Warnln("Plugins directory not found: '%s'", pluginsDir.string().c_str());
        return;
    }

    for (auto& entry : fs::directory_iterator(pluginsDir)) {
        if (!entry.is_regular_file())
            continue;

        auto path = entry.path();
        if (_stricmp(path.extension().string().c_str(), ".dll") != 0)
            continue;

        // skip our host DLL itself
        if (_stricmp(path.filename().string().c_str(), "InfinityHost.dll") == 0)
            continue;

        auto dllPath = path.string();
        Debugln("Attempting to load plugin '%s'", dllPath.c_str());

        HMODULE hLib = LoadLibraryA(dllPath.c_str());
        if (!hLib) {
            DWORD err = GetLastError();
            Warnln("LoadLibraryA failed (0x%X) on '%s'", err, dllPath.c_str());
            continue;
        }

        PrintlnColored("Successfully loaded '%s'", COL_RETAIL, dllPath.c_str());

        //call its OnPluginLoad export
        auto OnPluginLoad = reinterpret_cast<void(*)()>(GetProcAddress(hLib, "?OnPluginLoad@@YAXXZ"));
        if (OnPluginLoad) {
            OnPluginLoad();
            Debugln("OnPluginLoad() called in '%s'", dllPath.c_str());
        }
        else {
            Warnln("No OnPluginLoad export in '%s'", dllPath.c_str());
        }
    }
}

void Infinity::RegisterScriptClass(std::unique_ptr<Infinity::BaseScriptClass> pScriptClass)
{
    g_BaseScriptManager->Register(std::move(pScriptClass)); //add script instance to class manager and begin init
}

Infinity::FnLogToConsole Infinity::f_LogToConsole;

Infinity::FnLookupGlobalMethod Infinity::f_LookupGlobalMethod;
Infinity::FnCallGlobalMethod Infinity::f_CallGlobalMethod;

Infinity::FnLookupMethod Infinity::f_LookUpMethod;
Infinity::FnCallUpMethod Infinity::f_CallUpMethod;
Infinity::FnCleanupMethodCall Infinity::f_CleanupUpMethodCall;

Infinity::FnCreateInstance Infinity::f_CreateInstance;

//Find patterns for some Enforce function
void Infinity::InitPatterns()
{
    const std::string PATTERN_CALLUP_GLOBAL_METHOD =
        IsDiagBuild()
        ? "40 53 48 83 EC ? ? ? ? ? 48 8D 44 24 ? 48 8B DA 4C 8D 4C 24 ? 48 89 44 24 ? 0F 29 44 24 ? E8 ? ? ? ? 48 8B C3 48 83 C4 ? 5B C3 ? 40 53" //DIAG
        : "40 53 48 83 EC ? ? ? ? ? 48 8D 44 24 ? 48 8B DA 4C 8D 4C 24 ? 48 89 44 24 ? 0F 29 44 24 ? E8 ? ? ? ? 48 8B C3 48 83 C4 ? 5B C3 ? 48 89 5C 24 ? 48 89 6C 24"; //RETAIL

    //DayZ's console logging
    Infinity::f_LogToConsole = reinterpret_cast<Infinity::FnLogToConsole>(Infinity::Utils::FindPattern(PATTERN_PRINT_TO_CONSOLE, GetModuleHandle(NULL), 0));
    if (!f_LogToConsole)
        Errorln("InitPatterns: failed to locate f_LogToConsole, check pattern!");

    //Call-up global method in a given script module
    Infinity::f_LookupGlobalMethod = reinterpret_cast<Infinity::FnLookupGlobalMethod>(Infinity::Utils::FindPattern(PATTERN_LOOKUP_GLOBAL_METHOD, GetModuleHandle(NULL), 0));
    Infinity::f_CallGlobalMethod = reinterpret_cast<Infinity::FnCallGlobalMethod>(Infinity::Utils::FindPattern(PATTERN_CALLUP_GLOBAL_METHOD, GetModuleHandle(NULL), 0));
    
    if (!f_LookupGlobalMethod || !f_CallGlobalMethod)
        Errorln("InitPatterns: failed to locate f_LookupGlobalMethod or f_CallGlobalMethod, check patterns!");

    //Call-up dynamic method from instance of enforce class
    Infinity::f_LookUpMethod = reinterpret_cast<Infinity::FnLookupMethod>(Infinity::Utils::FindPattern(PATTERN_LOOKUP_METHOD, GetModuleHandle(NULL), 0));
    Infinity::f_CallUpMethod = reinterpret_cast<Infinity::FnCallUpMethod>(Infinity::Utils::FindPattern(PATTERN_CALLUP_METHOD, GetModuleHandle(NULL), 0));

    if (!f_LookUpMethod || !f_CallUpMethod)
        Errorln("InitPatterns: failed to locate f_LookUpMethod or f_CallUpMethod, check patterns!");

    const std::string PATTEN_CALL_CLEANUP_METHOD =
        IsDiagBuild()
        ? "40 53 48 83 EC ? ? ? ? 48 85 DB 74 ? 48 8B CB E8 ? ? ? ? 84 C0" //DIAG
        : "48 83 EC ? ? ? ? 4D 85 C9 74 ? 49 8B C9"; //RETAIL

    //Engine cleanup method
    Infinity::f_CleanupUpMethodCall = reinterpret_cast<FnCleanupMethodCall>(Infinity::Utils::FindPattern(PATTEN_CALL_CLEANUP_METHOD, GetModuleHandle(NULL), 0));
    if (!f_CleanupUpMethodCall)
        Errorln("InitPatterns: failed to locate f_CleanupUpMethodCall, check pattern!");
}

//Call to create and return ptr of a Enfusion class instance
EnfTypes::ManagedScriptInstance* Infinity::CreateEnforceInstance(EnfTypes::ScriptModule* pModule, const std::string& typeName)
{
    if (!f_CreateInstance){
        Infinity::f_CreateInstance = reinterpret_cast<FnCreateInstance>(Infinity::Utils::FindPattern(PATTERN_FN_CREATE_INSTANCE, GetModuleHandle(NULL), 0));
    }

    if (!f_CreateInstance) {
        Errorln("CreateEnforceInstance: failed to locate f_CreateInstance, check pattern!");
        return nullptr;
    }

    __int64 v0 = NULL;
    __int64 instancePtr = NULL;

    if (IsDiagBuild())
    {
        //DIAG
        using FnMagicCallDiag = __int64(__fastcall*)(__int64 a1, char a2);
        static FnMagicCallDiag f_FnMagicCallDiag = reinterpret_cast<FnMagicCallDiag>(Infinity::Utils::FindPattern("40 57 48 83 EC ? 48 8B F9 45 33 C9", GetModuleHandle(NULL), 0));
        
        if (!f_FnMagicCallDiag) {
            Errorln("CreateEnforceInstance: failed to locate f_FnMagicCallDiag, check pattern!");
            return nullptr;
        }

        v0 = f_CreateInstance(pModule, typeName.c_str(), 0);
        instancePtr = f_FnMagicCallDiag(v0, 1);
    }
    else
    {
       //RETAIL
       using FnMagicCall = __int64(__fastcall*)(__int64 a1);
       static FnMagicCall f_FnMagicCall = reinterpret_cast<FnMagicCall>(Infinity::Utils::FindPattern("48 89 5C 24 ? 57 48 83 EC ? 48 8B F9 48 8B D1", GetModuleHandle(NULL), 0));

       if (!f_FnMagicCall) {
           Errorln("CreateEnforceInstance: failed to locate f_FnMagicCall, check pattern!");
           return nullptr;
       }

       v0 = f_CreateInstance(pModule, typeName.c_str(), 0);
       instancePtr = f_FnMagicCall(v0);
    }

    if (instancePtr != 0)
    {
        Debugln("CreateEnforceInstance: Dynamic Instance of \'%s\' -> 0x%llX", typeName, (unsigned long long)v0);
        return reinterpret_cast<EnfTypes::ManagedScriptInstance*>(instancePtr);
    }
    
    Debugln("CreateEnforceInstance: Failed to create instance of type \'%s\' check typename!", typeName);
    return nullptr;
}

Infinity::Enfusion::Enscript::FunctionContext* Infinity::CreateFunctionContext()
{
    Infinity::Enfusion::Enscript::FunctionContext* ctx = new Infinity::Enfusion::Enscript::FunctionContext();

    ctx->Arguments = static_cast<Infinity::Enfusion::Enscript::PArguments>(std::malloc(sizeof(Infinity::Enfusion::Enscript::Arguments)));
    if (ctx->Arguments) {
        std::memset(ctx->Arguments, 0, sizeof(Infinity::Enfusion::Enscript::Arguments));
    }
    else {
        // handle allocation failure…
        delete ctx;
        return nullptr;
    }

    return ctx;
}

Infinity::Enfusion::Enscript::PNativeArgument Infinity::CreateNativeArgument(void* value, const char* variableName, void* pContext, uint32_t typeTag, uint32_t flags)
{
    using NArg = Infinity::Enfusion::Enscript::NativeArgument;

    // 1) alloc + zero
    NArg* arg = static_cast<NArg*>(std::malloc(sizeof(NArg)));
    if (!arg) return nullptr;
    std::memset(arg, 0, sizeof(NArg));

    arg->Value = value;

    //heap-copy the variableName
    if (variableName) {
        size_t len = std::strlen(variableName) + 1;
        char* nameCopy = static_cast<char*>(std::malloc(len));
        if (nameCopy) {
            std::memcpy(nameCopy, variableName, len);
            arg->VariableName = nameCopy;
        }
    }

    // 4) set the engine-inspected tag/flags inside _pad0
    uint8_t* base = reinterpret_cast<uint8_t*>(arg);
    uint32_t* padWords = reinterpret_cast<uint32_t*>(base + 0x10);
    padWords[0] = typeTag;   // offset 0x10–0x13
    padWords[1] = flags;     // offset 0x14–0x17

    // 5) context
    arg->pContext = pContext;

    return arg;
}

void Infinity::DestroyNativeArgument(Infinity::Enfusion::Enscript::PNativeArgument arg)
{
    if (!arg) return;
    if (arg->VariableName) {
        std::free(arg->VariableName);
        arg->VariableName = nullptr;
    }
    std::free(arg);
}

void Infinity::DestroyFunctionContext(Infinity::Enfusion::Enscript::FunctionContext* ctx)
{
    if (!ctx) return;
    if (ctx->Arguments) {
        //free any allocated arguments
        for (int i = 0; i < 8; ++i) {
            DestroyNativeArgument(ctx->Arguments->List[i]);
        }
        std::free(ctx->Arguments);
    }
    delete ctx;
}

Infinity::Enfusion::Enscript::PFunctionResult Infinity::CreateFunctionResult(Infinity::Enfusion::Enscript::PNativeArgument resultArg)
{
    //Allocate & zero the entire struct
    Infinity::Enfusion::Enscript::FunctionResult* fr = static_cast<Infinity::Enfusion::Enscript::FunctionResult*>(
        std::malloc(sizeof(Infinity::Enfusion::Enscript::FunctionResult))
        );
    if (!fr) return nullptr;
    std::memset(fr, 0, sizeof(Infinity::Enfusion::Enscript::FunctionResult));

    fr->Result = resultArg;

    return fr;
}

void Infinity::DestroyFunctionResult(Infinity::Enfusion::Enscript::PFunctionResult fr, bool freeInnerArgument)
{
    if (!fr) return;

    if (freeInnerArgument && fr->Result) {
        DestroyNativeArgument(fr->Result);
        fr->Result = nullptr;
    }

    std::free(fr);
}

#pragma region BaseScriptClass
//--------------------------------------------------------------
//BaseScriptClass
Infinity::BaseScriptClass::BaseScriptClass(const char* name)
{
	this->className = name;
    this->hasRegistered = false;
    this->pEnfType = nullptr;
}
const char* Infinity::BaseScriptClass::GetName() 
{
	return this->className;
}
bool Infinity::BaseScriptClass::HasRegistered()
{
    return this->hasRegistered;
}
void Infinity::BaseScriptClass::SetRegistered()
{
    this->hasRegistered = true;
}
Infinity::Enfusion::Enscript::Framework::ManagedClass* Infinity::BaseScriptClass::GetEnfTypePtr()
{
    return this->pEnfType;
}
void Infinity::BaseScriptClass::SetEnfTypePtr(__int64 ptr)
{
    this->pEnfType = reinterpret_cast<Infinity::Enfusion::Enscript::Framework::ManagedClass*>(ptr);
}
void Infinity::BaseScriptClass::RegisterStaticClassFunctions(RegistrationFunction registerMethod)
{
	return; //to be implemented by plugin(s)
}
void Infinity::BaseScriptClass::RegisterDynamicClassFunctions(RegistrationFunction registerMethod)
{
    return; //to be implemented by plugin(s)
}
void Infinity::BaseScriptClass::RegisterGlobalFunctions(RegistrationFunction registerFunction)
{
    return; //to be implemented by plugin(s)
}
#pragma endregion


#pragma region Logging
void Infinity::Logging::Println(const char* format, ...)
{
	printf("PLUGIN       : ");
	va_list vl;
	va_start(vl, format);
	vprintf(format, vl);
	va_end(vl);
	printf("\n");
}
void Infinity::Logging::Debugln(const char* format, ...)
{
	printf("PLUGIN    (D): ");
	va_list vl;
	va_start(vl, format);
	vprintf(format, vl);
	va_end(vl);
	printf("\n");
}
void Infinity::Logging::Warnln(const char* format, ...)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hOut, &csbi);
    WORD oldAttrs = csbi.wAttributes;

    SetConsoleTextAttribute(hOut, COL_WARN);

    fputs("PLUGIN    (W): ", stdout);
    va_list vl;
    va_start(vl, format);
    vprintf(format, vl);
    va_end(vl);
    fputc('\n', stdout);

    SetConsoleTextAttribute(hOut, oldAttrs);
}
void Infinity::Logging::Errorln(const char* format, ...)
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hOut, &csbi);
    WORD oldAttrs = csbi.wAttributes;

    SetConsoleTextAttribute(hOut, COL_ERROR);

    fputs("PLUGIN    (E): ", stdout);
	va_list vl;
	va_start(vl, format);
	vprintf(format, vl);
	va_end(vl);
    fputc('\n', stdout);

    SetConsoleTextAttribute(hOut, oldAttrs);
}
bool Infinity::Logging::IsDiagBuild()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return false;

    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"-diag") == 0) {
            found = true;
            break;
        }
    }
    LocalFree(argv);
    return found;
}
#pragma endregion