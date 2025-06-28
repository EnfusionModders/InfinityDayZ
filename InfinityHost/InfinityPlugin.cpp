#include <Windows.h>
#include <string>
#include <filesystem>

#include "ScriptEngine.h"
#include "ScriptRegistrator.h"
#include "Console.hpp"

#include "InfinityPlugin.h"

namespace fs = std::filesystem;

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

//Function pointer for calling/invoking Enforce script 
Infinity::FnCallFunction Infinity::CallEnforceFunction = reinterpret_cast<Infinity::FnCallFunction>(Infinity::Utils::FindPattern("48 89 6C 24 ? 56 57 41 54 41 55 41 57 48 83 EC ? 48 8B E9", GetModuleHandle(NULL), 0));


void Infinity::RegisterScriptClass(std::unique_ptr<Infinity::BaseScriptClass> pScriptClass)
{
	//add script instance to class manager and begin init
    g_BaseScriptManager->Register(std::move(pScriptClass));
}

Infinity::BaseScriptClass::BaseScriptClass(const char* name)
{
	this->className = name;
    this->hasRegistered = false;
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