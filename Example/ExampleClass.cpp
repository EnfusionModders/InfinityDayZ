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


void ExampleClass::TestGlobalFunction()
{
	Debugln("global method was called!");
}

/*
* Dynamic proto native methods will always include callerPtr as the first arg
* eg; if CGame has a proto native FnTest(arg), it's first arg will be a ptr to CGame instance followed by regular args. 
*/
void ExampleClass::TestMethod(ManagedScriptInstance* selfPtr, FunctionContext* args)
{
    Debugln("TestMethod args -> @  0x%llX", (unsigned long long)args);
    Debugln("TestMethod selfPtr -> @  0x%llX", (unsigned long long)selfPtr);
    Debugln("TestMethod pScriptModule -> @  0x%llX", (unsigned long long)selfPtr->pType->pScriptModule); //Correct

    int fnCount = selfPtr->pType->pScriptModule->pType->functionCount;

    type* t = selfPtr->pType->pScriptModule->pType;
    typename_functions* tfns = t->pFunctions;

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

        if (std::string(fn->name).find("CallFunction") != std::string::npos)
        {
            /*
            //Test out calling an Enforce function
            FunctionContext* fnCtx = new FunctionContext; //Create context
            
            NativeArgument* ptrSlot = new NativeArgument;
            ptrSlot->Value = selfPtr; //Class pointer
            fnCtx->SetArgument(0, ptrSlot);

            char* fnName = (char*)"MethodName";
            NativeArgument* nameSlot = new NativeArgument;
            nameSlot->Value = fnName; //Function name
            fnCtx->SetArgument(1, nameSlot);

            char* someRet = (char*)"Yo this is a test!";
            NativeArgument* retSlot = new NativeArgument;
            retSlot->Value = someRet; //Pointer for returnVal
            fnCtx->SetArgument(2, retSlot);

            char* someParam = (char*)"Yo this is a PARAM!";
            NativeArgument* paramSlot = new NativeArgument;
            paramSlot->Value = someParam; //Pointer for parameter
            fnCtx->SetArgument(3, paramSlot);
            */

            char ok = Infinity::CallEnforceFunction(selfPtr->pType->pScriptModule, args, fn);
        }
    }
}

//void ExampleClass::TestFunction(ManagedScriptInstance* inst, __int64 strPtr)
void ExampleClass::TestFunction(Infinity::Enfusion::Enscript::FunctionContext* args, Infinity::Enfusion::Enscript::FunctionResult* result)
{
    const char* input = (const char*)args->GetArgument(0)->Value; // arg0 is a string

    Println("Testing Function: %s", input);
    result->Result->Value = reinterpret_cast<void*>((char*)"Bitch ass!");
    

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