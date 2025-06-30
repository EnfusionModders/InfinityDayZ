#include <Windows.h>
#include <stdio.h>
#include <string>

#include "Patterns.h"
#include "Console.hpp"

#include "ScriptEngine.h"
#include "detours.h"


const std::string PATTERN_REG_ENGINE_CLASS_FUNCTION = "48 83 EC 38 45 0F B6 C8";
const std::string PATTERN_REG_STATIC_PROTO_FUNCTION = "48 89 74 24 ? 57 48 83 EC ? 48 8B F1 E8 ? ? ? ? 8B 54 24";
const std::string PATTERN_REG_DYNAMIC_PROTO_FUNCTION = "E9 ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? 48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B DA";
const std::string PATTERN_REG_GLOBAL_PROTO_FUNCTION = "48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 41 8B F1 48 8B E9";

BaseScriptManager* g_BaseScriptManager = NULL;
FnEngineRegisterClass f_EngineRegisterClass = NULL;
FnRegisterGlobalFunc f_RegisterGlobalFunc = NULL;
EnforceScriptCtx g_pEnforceScriptContext = NULL; //ptr to Core module script ctx, used for passing context when registering global fn

static bool detourComplete = false;
static int totalRegisteredClasses = 0;

/*
* Registers a global proto native function, this is the highest level of modules starts from 1_Core
* This detour gets hold of script ctx for us to register our own functions after our plugins have loaded up
*/
unsigned int*** RegisterGlobalFunction(__int64 a1, const char* name, void* addr, unsigned int a4)
{
	//Hold onto context
	if (!g_pEnforceScriptContext)
		g_pEnforceScriptContext = a1;

	/* TODO:: we could also overwrite already defined functions from the game and point it to something else!
	*         this would be nice for wanting to disable some game engine functionaility for security like CopyFile()
	if (std::string(name).find("GetObjectMaterials") != std::string::npos)
	{
		Debugln("RegisterGlobalFunction - > GlobalFnTest");
		return f_RegisterGlobalFunc(a1, (char*)("GlobalFnTest"), SomeGlobalFn, a4);
	}*/
	return f_RegisterGlobalFunc(a1, name, addr, a4);
}

/*
* This detour gets called when any scripted type is declared within game scripts
* we intercept this and get the context we require to define & declare our own proto methods to be built into the declared type
* a1 - > ScriptModule ctx, class name, a3 some flag value?
*/
__int64 EngineRegisterClass(__int64 moduleCtx, char* className, unsigned __int8 a3)
{
	__int64 classInstance = f_EngineRegisterClass(moduleCtx, className, a3); //trampoline to original engine call, we want the instance
	
	int toRegister = g_BaseScriptManager->GetTotalClassCount();
	if (toRegister == 0)
		return classInstance; //nothing to register yet...plugins probably still loading

	if (totalRegisteredClasses == toRegister && !detourComplete)
	{
		detourComplete = true;
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(reinterpret_cast<PVOID*>(&f_EngineRegisterClass), EngineRegisterClass);
		DetourTransactionCommit();
		Println("Custom classes (%d / %d) registration completed! Deattaching detour...", totalRegisteredClasses, toRegister);
		return classInstance;
	}

	Infinity::BaseScriptClass* ptr = g_BaseScriptManager->GetClassByName(className);
	if (!ptr)
		return classInstance; //skip, this type is not custom

	if (ptr->HasRegistered())
		return classInstance; //skip, this instance was already reg..

	Debugln("RegisterEngineClass -> %s  @  0x%llX", className, (unsigned long long)classInstance);

	//Register static class functions
	using FnRegisterClassStaticFunctions = __int64(__fastcall*)(
		__int64 a1,
		__int64 a2,
		const char* a3,
		void* a4,
		int a5
		);

	static FnRegisterClassStaticFunctions FnRegStaticFns = nullptr;
	auto pFnRegStatic = (uint8_t*)Infinity::Utils::FindPattern(PATTERN_REG_STATIC_PROTO_FUNCTION, GetModuleHandle(NULL), 0);
	if (pFnRegStatic)
		FnRegStaticFns = reinterpret_cast<FnRegisterClassStaticFunctions>(pFnRegStatic);

	if (!FnRegStaticFns)
	{
		Errorln("!ERROR! Unable to locate subroutine call for static function registration!");
		return classInstance;
	}

	// build one lambda that wraps the engine call:
	auto registerFunc = [moduleCtx, classInstance](const char* fName, void* fPtr) -> bool {
		__int64 res = FnRegStaticFns(
			moduleCtx,
			classInstance,
			fName,
			fPtr,
			0
		);
		return (res != 0);
	};
	ptr->RegisterStaticClassFunctions(registerFunc);


	//Register dynamic functions
	using FnRegisterClassFunctions = __int64(__fastcall*)(
		__int64 a1,
		__int64 a2,
		const char* a3,
		void* a4,
		int a5
		);

	static FnRegisterClassFunctions FnRegFns = nullptr;
	auto pImm = (uint8_t*)Infinity::Utils::FindPattern(PATTERN_REG_DYNAMIC_PROTO_FUNCTION,GetModuleHandle(NULL), 1);
	if (pImm)
	{
		uint32_t rel = *reinterpret_cast<uint32_t*>(pImm);
		uint64_t fnAddr = (uint64_t)pImm + 4 + rel;
		FnRegFns = reinterpret_cast<FnRegisterClassFunctions>(fnAddr);
	}

	if (!FnRegFns)
	{
		Errorln("!ERROR! Unable to locate subroutine call for dynamic function registration!");
		return classInstance;
	}

	auto registerDynamicFunc = [moduleCtx, classInstance](const char* fName, void* fPtr) -> bool {
		__int64 res = FnRegFns(
			moduleCtx,
			classInstance,
			fName,
			fPtr,
			0
		);
		return (res != 0);
	};
	ptr->RegisterDynamicClassFunctions(registerDynamicFunc);
	

	ptr->SetRegistered();
	++totalRegisteredClasses;
	return classInstance;
}

bool InitScriptEngine()
{
	Debugln("Init script engine.");
	
	g_BaseScriptManager = new BaseScriptManager();

	//Hook detour onto the engine function that handles setup of script declared typenames
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	static auto ptrFnRegisterClass = Infinity::Utils::FindPattern(PATTERN_REG_ENGINE_CLASS_FUNCTION, GetModuleHandle(NULL), 0);
	f_EngineRegisterClass = (FnEngineRegisterClass)(ptrFnRegisterClass);
	DetourAttach(&(PVOID&)f_EngineRegisterClass, EngineRegisterClass);
	DetourTransactionCommit();


	//Detour for registering global script functions
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	static auto ptrFnRegisterGlobalFn = Infinity::Utils::FindPattern(PATTERN_REG_GLOBAL_PROTO_FUNCTION, GetModuleHandle(NULL), 0);
	f_RegisterGlobalFunc = (FnRegisterGlobalFunc)(ptrFnRegisterGlobalFn);
	DetourAttach(&(PVOID&)f_RegisterGlobalFunc, RegisterGlobalFunction);
	DetourTransactionCommit();

	return true;
}