#include <InfinityPlugin.h>
#include <EnfusionTypes.hpp>

#include "ExampleClass.h"

using namespace Infinity;
using namespace Infinity::Logging;
using namespace Infinity::Enfusion::Enscript;
using namespace Infinity::Enfusion::Enscript::Framework;


ExampleClass::ExampleClass() : BaseScriptClass("ExampleClass") {}

//static proto function that live within ExampleClass on Enforce side of things
void ExampleClass::RegisterStaticClassFunctions(Infinity::RegistrationFunction registerMethod){
	registerMethod("TestFunction", &ExampleClass::TestFunction);
};

//dynamic proto native functions that live within ExampleClass on Enforce side of things
void ExampleClass::RegisterDynamicClassFunctions(Infinity::RegistrationFunction registerMethod) {
	registerMethod("TestMethod", &ExampleClass::TestMethod);
};

//These are global, doesn't live within our Enforce typename declaration, but it points it's functionality here :)
void ExampleClass::RegisterGlobalFunctions(Infinity::RegistrationFunction registerFunction) {
	registerFunction("GlobalFnTest", &ExampleClass::TestGlobalFunction);
};


void ExampleClass::TestGlobalFunction(char* message)
{
	Debugln("global method was called from ExampleClass %s", message);
}

/*
* Dynamic proto native methods will always include callerPtr as the first arg
* eg; if CGame has a proto native FnTest(arg), it's first arg will be a ptr to CGame instance followed by regular args. 
*/

void ExampleClass::TestMethod(__int64 callerPtr, char* message)
{
	Debugln("Dynamic TestMethod was called from ExampleClass %s", message);
}

void ExampleClass::TestFunction(EnArray* arr)
{
	if (!arr)
	{
		Warnln("TestFunction called with null array!");
		return;
	}

	char* msg = arr->Get<char*>(0);
//	Debugln("element at index 0 : %s", msg);
	Debugln("array size : %d", arr->GetSize());
	Debugln("array capacity : %d", arr->GetCapacity());

	//Modifiy array
	arr->Insert((char*)"Special Message from Plugin!");

	Debugln("array size after: %d", arr->GetSize());
	Debugln("array capacity after: %d", arr->GetCapacity());
}