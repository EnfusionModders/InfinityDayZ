#pragma once

#include <functional>
#include <vector>
#include <Windows.h>
#include "EnfusionTypes.hpp"

#ifdef INFINITYHOST_EXPORTS
#define _CLINKAGE __declspec(dllexport)
#else
#define _CLINKAGE __declspec(dllimport)
#endif

namespace Infinity {
#ifdef INFINITYHOST_EXPORTS
	typedef void(__fastcall* ONPLUGINLOAD)();
	void LoadPlugins(); // calls OnPluginLoad() export so Register can be called
#endif
	// -------------------------------------------------------------------------
	// script class registration
	typedef std::function<bool(const char*, void*)> RegistrationFunction;

	//dynamic instance enforce method call
	typedef __int64(__fastcall* FnLogToConsole)(int a1, const char* a2, ...);
	typedef __int64(__fastcall* FnLookupMethod)(__int64 classPtr, const char* methodName);
	typedef __int64(__fastcall* FnCallUpMethod)(__int64 instancePtr, Infinity::Enfusion::Enscript::FunctionResult* a2, int idx, ...);
	typedef void(__fastcall* FnCleanupMethodCall)(Infinity::Enfusion::Enscript::FunctionResult* a1);

	class _CLINKAGE BaseScriptClass {
	public:
		BaseScriptClass(const char* name);
		const char* GetName();
		bool HasRegistered();
		void SetRegistered();
		void SetEnfClassPtr(__int64 ptr);
		Infinity::Enfusion::Enscript::Framework::ManagedClass* GetEnfClassPtr();
		// override this function to register your script routines to this class.
		virtual void RegisterStaticClassFunctions(RegistrationFunction registerMethod);
		// override this function to register your script routines to this class.
		virtual void RegisterDynamicClassFunctions(RegistrationFunction registerMethod);
		// override this function to register your global functions to this class
		virtual void RegisterGlobalFunctions(RegistrationFunction registerFunction);
	protected:
		const char* className;
		bool hasRegistered;
		Infinity::Enfusion::Enscript::Framework::ManagedClass* pEnfClass;
	};

	// Call this routine during OnPluginLoad to register custom script classes
	_CLINKAGE void RegisterScriptClass(std::unique_ptr<BaseScriptClass> pScriptClass); // register a script class 

	// --------------------------------------------------------------------------
	// useful methods exposed for plugins
	namespace Logging {
		_CLINKAGE void Println(const char* format, ...);
		_CLINKAGE void Errorln(const char* format, ...);
		_CLINKAGE void Warnln(const char* format, ...);
		_CLINKAGE void Debugln(const char* format, ...);
	}
	namespace Utils {
		_CLINKAGE void* FindPattern(std::string pattern, HMODULE module, int offset);
	}
	namespace Enfusion {
		//_CLINKAGE bool RegisterKeyPath(const char* directory, const char* key, bool allow_write = true); // register path to $key: for file access
	}

	_CLINKAGE extern FnLogToConsole f_LogToConsole;

	template<typename... Args>
	void PrintToConsole(const char* message, Args&&... args) {
		f_LogToConsole(1, message, std::forward<Args>(args)...);
	}

	_CLINKAGE extern FnLookupMethod f_LookUpMethod;
	_CLINKAGE extern FnCallUpMethod f_CallUpMethod;
	_CLINKAGE extern FnCleanupMethodCall f_CleanupUpMethodCall;

	//Helper functions to create function calling & return context
	_CLINKAGE Infinity::Enfusion::Enscript::FunctionContext* CreateFunctionContext();
	_CLINKAGE Infinity::Enfusion::Enscript::PNativeArgument CreateNativeArgument(void* value, const char* variableName, void* pContext, uint32_t typeTag, uint32_t flags);
	_CLINKAGE void DestroyNativeArgument(Infinity::Enfusion::Enscript::PNativeArgument arg);
	_CLINKAGE void DestroyFunctionContext(Infinity::Enfusion::Enscript::FunctionContext* ctx);
	_CLINKAGE Infinity::Enfusion::Enscript::PFunctionResult CreateFunctionResult(Infinity::Enfusion::Enscript::PNativeArgument resultArg);
	_CLINKAGE void DestroyFunctionResult(Infinity::Enfusion::Enscript::PFunctionResult fr, bool freeInnerArgument = true);
	
	/*
	* Use this function to lookup and call an Enforce level method at a given enforce class instance
	* Returns void* result data of the method call.
	* NOTE: You cannot call a proto engine method using this! Strictly regular class methods/functions (non static!)
	*/
	template<typename... Args>
	void* CallEnforceMethod(Enfusion::Enscript::Framework::ManagedScriptInstance* pInstance, const std::string& methodName, Args&&... args) {

		if (!pInstance) {
			Infinity::Logging::Errorln("CallEnforceMethod: call failed, pInstance cannot be null!");
			return false;
		}

		if (methodName.empty()) {
			Infinity::Logging::Errorln("CallEnforceMethod: call failed, method name cannot be empty!");
			return false;
		}

		int idx = f_LookUpMethod(reinterpret_cast<__int64>(pInstance->pType), methodName.c_str());
		if (idx == -1){
			Infinity::Logging::Errorln("CallEnforceMethod: call failed, method could not be found! Check name");
			return false;
		}

		Infinity::Enfusion::Enscript::FunctionResult ret;
		auto callUpRet = f_CallUpMethod(
			reinterpret_cast<__int64>(pInstance),
			&ret,
			idx,
			std::forward<Args>(args)...
		);

		f_CleanupUpMethodCall(&ret);

		return ret.Result->Value;
	}
}