#pragma once

#include <InfinityPlugin.h>
#include <EnfusionTypes.hpp>

using namespace Infinity;
using namespace Infinity::Logging;
using namespace Infinity::Enfusion::Enscript;
using namespace Infinity::Enfusion::Enscript::Framework;

class ExampleClass : public Infinity::BaseScriptClass {
public:
	ExampleClass();
	void RegisterStaticClassFunctions(RegistrationFunction registerMethod) override;
	void RegisterDynamicClassFunctions(RegistrationFunction registerMethod) override;
	void RegisterGlobalFunctions(RegistrationFunction registerFunction) override;
private:
	//static void TestFunction(ManagedScriptInstance* inst, __int64 strPtr);
	static void TestFunction(FunctionContext* args, FunctionResult* result);
	static void GlobalFnTest(char* somedata);
	static void TestMethod(ManagedScriptInstance* selfPtr, FunctionContext* args);
	static void BigMethod(ManagedScriptInstance*, ManagedScriptInstance*);
	static void GlobalNonNativeFn(FunctionContext* args, FunctionResult* result);
	static char* TestStaticNativeMethod(char*);
};