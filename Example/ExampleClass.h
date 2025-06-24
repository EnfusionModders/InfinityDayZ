#pragma once

#include <InfinityPlugin.h>
#include <EnfusionTypes.hpp>


class ExampleClass : public Infinity::BaseScriptClass {
public:
	ExampleClass();
	void RegisterStaticClassFunctions(Infinity::RegistrationFunction registerMethod) override;
	void RegisterDynamicClassFunctions(Infinity::RegistrationFunction registerMethod) override;
	void RegisterGlobalFunctions(Infinity::RegistrationFunction registerFunction) override;
private:
//	static void TestFunction(Infinity::Enfusion::Enscript::FunctionContext* args, Infinity::Enfusion::Enscript::FunctionResult* result);
	static void TestFunction(Infinity::Enfusion::Enscript::Framework::EnArray* arr);
	static void TestGlobalFunction(char* message);
	static void TestMethod(__int64 callerPtr, char* message);
};