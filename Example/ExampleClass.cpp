#include <InfinityPlugin.h>
#include <EnfusionTypes.hpp>

#include "ExampleClass.h"

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


void ExampleClass::TestGlobalFunction(char* someData)
{
	Debugln("global method was called! %s  @ 0x%llX", someData, (unsigned long long)someData);
}

/*
* Dynamic proto native methods will always include selfPtr as the first arg
* eg; if CGame has a proto native FnTest(arg), it's first arg will be a ptr to CGame instance followed by regular args. 
*/
void ExampleClass::TestMethod(ManagedScriptInstance* selfPtr, FunctionContext* args)
{
    Debugln("TestMethod args -> @  0x%llX", (unsigned long long)args);
    Debugln("TestMethod selfPtr -> @  0x%llX", (unsigned long long)selfPtr);
    Debugln("TestMethod pScriptModule -> @  0x%llX", (unsigned long long)selfPtr->pType->pScriptModule);
    Debugln("TestMethod pScriptContext -> @  0x%llX", (unsigned long long)selfPtr->pType->pScriptModule->pContext);
    
    Debugln("TestMethod pScriptModule name -> %s", selfPtr->pType->pScriptModule->pName);
    
    //Example, calling dynamic method within a class instance (for non proto class declared methods!)
    const char* retRes = (const char*)Infinity::CallEnforceMethod(selfPtr, "JtMethod", (char*)"Hello this is a message!");
    Debugln("CallEnforceMethod returned: %s", retRes);
    // pause here until you hit a key or debugger resumes
    //while (!GetAsyncKeyState(VK_F12)) {
    //    Sleep(1);
   // }

    //Example Calling a global proto method at a given ScriptContext
    int fnCount = selfPtr->pType->pScriptModule->pContext->GlobalFunctionCount;

    typename_functions* tfns = selfPtr->pType->pScriptModule->pContext->pGlobalFunctions;

    using RawFn = char* (__fastcall*)(char* someData);

    for (int i = 0; i < fnCount; ++i)
    {
        typename_function* fn = tfns->List[i];
        if (!fn || !fn->name || !fn->pContext)
            break;

        Debugln("  [%d] fn @ 0x%llX: name='%s' fnPtr: @ 0x%llX ,pContext=0x%llX",
            i,
            (unsigned long long)fn,
            fn->name,
            (unsigned long long)fn->fnPtr,
            (unsigned long long)fn->pContext);

        
        if (std::string(fn->name).find("GlobalFnTest") != std::string::npos)
        {
            RawFn callFn = reinterpret_cast<RawFn>(fn->fnPtr);
            callFn((char*)"HELLO!!!");
            break;
        }
    }
}

//void ExampleClass::TestFunction(ManagedScriptInstance* inst, __int64 strPtr)
void ExampleClass::TestFunction(Infinity::Enfusion::Enscript::FunctionContext* args, Infinity::Enfusion::Enscript::FunctionResult* result)
{
    const char* input = (const char*)args->GetArgument(0)->Value; // arg0 is a string

    Println("Testing Function: %s", input);
    result->Result->Value = reinterpret_cast<void*>((char*)"Some callback string");
    

    /*
	Debugln("TestFunction inst -> @  0x%llX  strPtr -> @ 0x%llx", (unsigned long long)inst, (unsigned long long)strPtr);

    // Step 1: grab the type
    type* t = inst->pType;
    if (!t) {
        Debugln("  pType == nullptr");
        return;
    }
    Debugln("Type '%s' @ 0x%llX total functions: %d",
        t->name ? t->name : "<null>",
        (unsigned long long)t,
        t->functionCount);

    // Step 2: grab the function‐list container
    typename_functions* tfns = t->pFunctions;
    if (!tfns) {
        Debugln("  pFunctions == nullptr");
        return;
    }

    using RawFn = char*(__fastcall*)(ManagedScriptInstance* inst);

    // Step 3: walk the fixed List with our function count
    int found = 0;
    for (int i = 0; i < t->functionCount; ++i)
    {
        auto* _fn = tfns->List[i];
        if (!_fn)
            break;

       typename_function* fn = tfns->List[i];
        if (!fn || !fn->name || !fn->pContext)
            break;

        Debugln("  [%d] fn @ 0x%llX: name='%s' fnPtr: @ 0x%llX ,pContext=0x%llX",
            i,
            (unsigned long long)fn,
            fn->name ? fn->name : "<null>",
            (unsigned long long)fn->fnPtr,
            (unsigned long long)fn->pContext);

        if (std::string(fn->name).find("ClassName") != std::string::npos)
        {
            RawFn callFn = reinterpret_cast<RawFn>(fn->fnPtr);
            char* ret = callFn(inst);
            Debugln(" fn call returned %s", ret);
        }

        ++found;
    }

    Debugln(" -> %d/%d functions populated\n", found, t->functionCount);
    */

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