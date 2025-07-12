#include <Windows.h>
#include <stdio.h>
#include <string>

#include "Patterns.h"
#include "Console.hpp"

#include "ScriptEngine.h"
#include "detours.h"

//Patterns
std::string PATTERN_REG_ENGINE_CLASS_FUNCTION;
std::string PATTERN_REG_STATIC_PROTO_FUNCTION;
std::string PATTERN_REG_DYNAMIC_PROTO_FUNCTION;
std::string PATTERN_REG_GLOBAL_PROTO_FUNCTION;

BaseScriptManager* g_BaseScriptManager = NULL;
FnEngineRegisterClass f_EngineRegisterClass = NULL;
FnRegisterGlobalFunc f_RegisterGlobalFunc = NULL;
EnforceScriptCtx g_pEnforceScriptContext = NULL; //ptr to Core module script ctx, used for passing context when registering global fn

static bool detourComplete = false;
static int totalRegisteredClasses = 0;


using FnSubCELoop = __int64(__fastcall*)(__int64* a1, unsigned int a2, unsigned __int8 a3);
static FnSubCELoop f_FnSubCELoop = nullptr;

static __int64 __fastcall detour_FnSubCELoop(__int64* a1, unsigned int a2, unsigned __int8 a3)
{
	return NULL;
}

using FnSub140668C00 = __int64(__fastcall*)(
	int*,
	char*,
	char,
	char*,
	__int64,
	__int64
	);

static FnSub140668C00  orig_sub_140668C00 = nullptr;

static __int64 __fastcall detour_sub_140668C00(
	int* a1,
	char* a2,
	char         a3,
	char* a4,
	__int64      a5,
	__int64      a6
) {
	// Pre-call logging
	Infinity::Logging::Debugln(
		"▶ sub_140668C00(a1=%d, a2=%s, a3=%d, a4=\"%s\", a5=0x%llX, a6=0x%llX)",
		a1,
		a2,
		(int)a3,
		a4 ? a4 : "<null>",
		(unsigned long long)a5,
		(unsigned long long)a6
	);

	// Call the original
	__int64 result = orig_sub_140668C00(a1, a2, a3, a4, a5, a6);

	// Post-call logging
	Infinity::Logging::Debugln(
		"◀ sub_140668C00 returned 0x%llX",
		(unsigned long long)result
	);

	return result;
}


/*
* Registers a global proto native function, this is the highest level of modules starts from 1_Core
* This detour gets hold of script ctx for us to register our own functions after our plugins have loaded up
*/
unsigned int*** RegisterGlobalFunction(__int64 a1, const char* name, void* addr, unsigned int a4)
{
	//Hold onto context
	if (!g_pEnforceScriptContext) {
		Debugln("g_pEnforceScriptContext: @ -> 0x%llX ", (unsigned long long)a1);
		g_pEnforceScriptContext = a1;
	}

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

	//Hold onto a ptr to the Enforce static instance of class definitions (useful for calling static member functions)
	ptr->SetEnfTypePtr(classInstance);

	Debugln("RegisterEngineClass -> %s  @  0x%llX", className, (unsigned long long)classInstance);

	//Register static class functions
	using FnRegisterClassStaticFunctions = __int64(__fastcall*)(
		__int64 a1,
		__int64 a2,
		const char* a3,
		void* a4,
		int a5,
		...
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
		__int64 res = NULL;
		if (IsDiagBuild())
		{
			res = FnRegStaticFns(moduleCtx, classInstance, fName, fPtr, 0, 1);//!One extra param on diag exe
		}else{
			res = FnRegStaticFns(moduleCtx, classInstance, fName, fPtr, 0); //!One less param on retail exe
		}
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
	void* pImm = nullptr;

	//Diag exe uses different pattern
	if (IsDiagBuild())
	{
		pImm = (uint8_t*)Infinity::Utils::FindPattern(PATTERN_REG_DYNAMIC_PROTO_FUNCTION, GetModuleHandle(NULL), 0);
		if (pImm)
		{
			FnRegFns = reinterpret_cast<FnRegisterClassFunctions>(pImm);
		}
	}
	else
	{
		pImm = (uint8_t*)Infinity::Utils::FindPattern(PATTERN_REG_DYNAMIC_PROTO_FUNCTION, GetModuleHandle(NULL), 1);
		if (pImm)
		{
			uint32_t rel = *reinterpret_cast<uint32_t*>(pImm);
			uint64_t fnAddr = (uint64_t)pImm + 4 + rel;
			FnRegFns = reinterpret_cast<FnRegisterClassFunctions>(fnAddr);
		}
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

	//Define Patterns
	PATTERN_REG_ENGINE_CLASS_FUNCTION = "48 83 EC 38 45 0F B6 C8"; //BOTH
	PATTERN_REG_GLOBAL_PROTO_FUNCTION = "48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 41 8B F1 48 8B E9"; //BOTH

	PATTERN_REG_STATIC_PROTO_FUNCTION =
		IsDiagBuild()
		? "48 89 74 24 ? 57 48 83 EC ? 0F B6 44 24 ? 48 8B F1" //DIAG
		: "48 89 74 24 ? 57 48 83 EC ? 48 8B F1 E8 ? ? ? ? 8B 54 24"; //RETAIL

	PATTERN_REG_DYNAMIC_PROTO_FUNCTION =
		IsDiagBuild()
		? "48 89 74 24 ? 57 48 83 EC ? 48 8B F1 E8 ? ? ? ? 8B 54 24" //DIAG
		: "E9 ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? 48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B DA"; //RETAIL

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

	void* addr2 = Infinity::Utils::FindPattern("48 89 5C 24 ? 55 41 56 41 57 48 83 EC ? 49 8B E9", GetModuleHandle(NULL), /*skip=*/0);
	if (!addr2) {
		Infinity::Logging::Errorln("Hook(sub_140668C00): pattern not found");
	}
	orig_sub_140668C00 = reinterpret_cast<FnSub140668C00>(addr2);

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(reinterpret_cast<PVOID*>(&orig_sub_140668C00), detour_sub_140668C00);
	LONG status2 = DetourTransactionCommit();
	if (status2 != NO_ERROR) {
		Infinity::Logging::Errorln("DetourAttach(sub_140668C00) failed: %d", status2);
	}
	else {
		Infinity::Logging::Debugln("Hooked sub_140668C00 at %p", addr2);
	}

	///--Test CELOOP
	void* addr3 = Infinity::Utils::FindPattern("48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? ? ? ? 41 0F B6 F8 8B F2", GetModuleHandle(NULL), 0);
	if (!addr3) {
		Infinity::Logging::Errorln("Hook(sub_CELOOP): pattern not found");
	}
	f_FnSubCELoop = reinterpret_cast<FnSubCELoop>(addr3);

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(reinterpret_cast<PVOID*>(&f_FnSubCELoop), detour_FnSubCELoop);
	LONG status3 = DetourTransactionCommit();
	if (status3 != NO_ERROR) {
		Infinity::Logging::Errorln("DetourAttach(sub_CELOOP) failed: %d", status3);
	}
	else {
		Infinity::Logging::Debugln("Hooked sub_CELOOP at %p", addr3);
	}

	return true;
}