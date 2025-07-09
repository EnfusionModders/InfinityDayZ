#include <string>
#include <stdint.h>
#include <functional>

#include "ScriptEngine.h"
#include "InfinityPlugin.h"
#include "Console.hpp"

#include "ScriptRegistrator.h"
#include "Patterns.h"


BaseScriptManager::BaseScriptManager(){}
BaseScriptManager::~BaseScriptManager() {}

int BaseScriptManager::GetTotalClassCount()
{
	return g_BaseClasses.size();
}

bool BaseScriptManager::Register(std::unique_ptr<Infinity::BaseScriptClass> pScriptClass)
{
	//sanity check, this shouldn't be null at this point because we wait for modules to init
	if (!g_pEnforceScriptContext)
	{
		Errorln("FAILED TO REGISTER: Something went wrong, Script Context to core modules is missing!");
		return false;
	}

	//start registration of our global functions
	auto registerFunc = [](const char* fName, void* fPtr) -> bool {
		auto res = f_RegisterGlobalFunc(
			g_pEnforceScriptContext,
			fName,
			fPtr,
			0
		);
		return (res != 0);
	};
	pScriptClass->RegisterGlobalFunctions(registerFunc);

	Debugln("Added class instance %s to g_BaseClasses.", pScriptClass->GetName());
	g_BaseClasses.emplace_back(std::move(pScriptClass));

	return true;
}

Infinity::BaseScriptClass* BaseScriptManager::GetClassByName(char* name)
{
	for (auto& uptr : g_BaseClasses)
	{
		Infinity::BaseScriptClass* ptr = uptr.get();
		if (std::string(name) == ptr->GetName())
		{
			return ptr;
		}
	}
	return nullptr;
}