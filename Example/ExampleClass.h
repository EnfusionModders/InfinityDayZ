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
	static void GlobalFnTest(char* somedata);
	static void GlobalNonNativeFn(FunctionContext* args, FunctionResult* result);

	static void TestStaticFunction(FunctionContext* args, FunctionResult* result);
	static char* TestStaticNativeFunction(char*);

	static void DynamicProtoMethod(ManagedScriptInstance* selfPtr, FunctionContext* args, FunctionResult* result);
	static void DynamicProtoNativeMethod(ManagedScriptInstance*, PlayerIdentity*);
};