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

//Patterns for calling dynamic class methods
const std::string PATTERN_LOOKUP_METHOD = "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B FA 48 8B D9 48 85 C9 74 ? 48 89 54 24";
const std::string PATTERN_CALLUP_METHOD = "44 89 44 24 ? 4C 89 4C 24 ? 53";
const std::string PATTEN_CALL_CLEANUP_METHOD = "48 83 EC ? ? ? ? 4D 85 C9 74 ? 49 8B C9";

void Infinity::LoadPlugins()
{
    // load the single Example.dll next to our EXE
    Println("Loading plugins....");

    // get full path to our running EXE
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    std::string exeDir = std::string(buffer).substr(0, pos);

    // build the path to Example.dll
    std::string dllPath = exeDir + "\\Example.dll";

    // ensure it exists
    if (!fs::exists(dllPath))
    {
        Warnln("Example.dll not found. Expected at '%s'.", dllPath.c_str());
        return;
    }

    Debugln("Attempting to load '%s'.", dllPath.c_str());

    // load it
    HMODULE hLib = LoadLibraryA(dllPath.c_str());
    if (!hLib)
    {
        DWORD err = GetLastError();
        Warnln("LoadLibraryA failed (0x%X) when loading '%s'.", err, dllPath.c_str());
        return;
    }

    Debugln("Successfully loaded '%s'.", dllPath.c_str());

    // call its OnPluginLoad export
    ONPLUGINLOAD OnPluginLoad =
        (ONPLUGINLOAD)GetProcAddress(hLib, "?OnPluginLoad@@YAXXZ");
    if (OnPluginLoad)
    {
        OnPluginLoad();
        Debugln("OnPluginLoad was called in Example.dll.");
    }
    else
    {
        Warnln("Unable to find OnPluginLoad export in Example.dll.");
    }
}

void Infinity::RegisterScriptClass(std::unique_ptr<Infinity::BaseScriptClass> pScriptClass)
{
    //add script instance to class manager and begin init
    g_BaseScriptManager->Register(std::move(pScriptClass));
}

//Call-up dynamic method from instance of enforce class
Infinity::FnLookupMethod Infinity::f_LookUpMethod = reinterpret_cast<Infinity::FnLookupMethod>(Infinity::Utils::FindPattern(PATTERN_LOOKUP_METHOD, GetModuleHandle(NULL), 0));
Infinity::FnCallUpMethod Infinity::f_CallUpMethod = reinterpret_cast<Infinity::FnCallUpMethod>(Infinity::Utils::FindPattern(PATTERN_CALLUP_METHOD, GetModuleHandle(NULL), 0));
Infinity::FnCleanupMethodCall Infinity::f_CleanupUpMethodCall = reinterpret_cast<Infinity::FnCleanupMethodCall>(Infinity::Utils::FindPattern(PATTEN_CALL_CLEANUP_METHOD, GetModuleHandle(NULL), 0));

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
    this->pEnfClass = nullptr;
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
Infinity::Enfusion::Enscript::Framework::ManagedClass* Infinity::BaseScriptClass::GetEnfClassPtr()
{
    return this->pEnfClass;
}
void Infinity::BaseScriptClass::SetEnfClassPtr(__int64 ptr)
{
    this->pEnfClass = reinterpret_cast<Infinity::Enfusion::Enscript::Framework::ManagedClass*>(ptr);
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
	printf("PLUGIN    (W): ");
	va_list vl;
	va_start(vl, format);
	vprintf(format, vl);
	va_end(vl);
	printf("\n");
}
void Infinity::Logging::Errorln(const char* format, ...)
{
	printf("PLUGIN    (E): ");
	va_list vl;
	va_start(vl, format);
	vprintf(format, vl);
	va_end(vl);
	printf("\n");
}
#pragma endregion