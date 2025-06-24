#pragma once

#include <vector>
#include "InfinityPlugin.h"

class BaseScriptManager {
public:
	BaseScriptManager();
	~BaseScriptManager();
	bool Register(std::unique_ptr<Infinity::BaseScriptClass> pScriptClass); // Handles logic for storing, initializing & registeration of global function
	Infinity::BaseScriptClass* GetClassByName(char* name);
	int GetTotalClassCount();
private:
	std::vector<std::unique_ptr<Infinity::BaseScriptClass>> g_BaseClasses;
};