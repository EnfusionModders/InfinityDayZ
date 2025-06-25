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

void ExampleClass::TestFunction(ManagedScriptInstance* inst)
{
	Debugln("TestFunction map -> @  0x%llX", (unsigned long long)inst);

    // Step 1: grab the type
    type* t = inst->pType;
    if (!t) {
        Debugln("  pType == nullptr");
        return;
    }
    Debugln("Type '%s' @ 0x%llX",
        t->name ? t->name : "<null>",
        (unsigned long long)t);

    // Step 2: grab the function‐list container
    typename_functions* tfns = t->pFunctions;
    if (!tfns) {
        Debugln("  pFunctions == nullptr");
        return;
    }

    using RawFn = int(__fastcall*)(ManagedScriptInstance* inst);

    // Step 3: walk the fixed List[9]
    int found = 0;
    for (int i = 0; i < 32; ++i)
    {
        typename_function* fn = tfns->List[i];
        if (!fn)
            break;

        if (std::string(fn->name).find("Count") != std::string::npos)
        {
            RawFn callFn = reinterpret_cast<RawFn>(fn->fnPtr);
            int ret = callFn(inst);
            Debugln(" fn call returned %d", ret);
        }

        Debugln("  [%d] fn @ 0x%llX: name='%s' fnPtr: @ 0x%llX ,pContext=0x%llX",
            i,
            (unsigned long long)fn,
            fn->name ? fn->name : "<null>",
            (unsigned long long)fn->fnPtr,
            (unsigned long long)fn->pContext);
        ++found;
    }

    Debugln(" -> %d/%d functions populated\n", found, 32);

	/*
	if (!map)
	{
		Warnln("TestFunction called with null map!");
		return;
	}

	char* key = map->GetKey<char*>(0);
	int* data = map->Get<char*, int*>(key);

	Debugln("map element at key %s : %d", key, data);

	Debugln("map size : %d", map->Size);
	Debugln("map capacity : %d", map->Capacity);


	Debugln("map size after: %d", map->Size);
	Debugln("map capacity after: %d", map->Capacity);
	*/
}