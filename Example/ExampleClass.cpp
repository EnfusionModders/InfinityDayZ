#include <InfinityPlugin.h>
#include <EnfusionTypes.hpp>

#include "ExampleClass.h"

ExampleClass::ExampleClass() : BaseScriptClass("ExampleClass") {}

//static proto function that live within ExampleClass on Enforce side of things
void ExampleClass::RegisterStaticClassFunctions(Infinity::RegistrationFunction registerMethod){
	registerMethod("TestFunction", &ExampleClass::TestFunction);
	registerMethod("TestStaticNativeMethod", &ExampleClass::TestStaticNativeMethod);
};

//dynamic proto native functions that live within ExampleClass on Enforce side of things
void ExampleClass::RegisterDynamicClassFunctions(Infinity::RegistrationFunction registerMethod) {
	registerMethod("TestMethod", &ExampleClass::TestMethod);
	registerMethod("BigMethod", &ExampleClass::BigMethod);
};

//These are global, doesn't live within our Enforce typename declaration, but it points it's functionality here :)
void ExampleClass::RegisterGlobalFunctions(Infinity::RegistrationFunction registerFunction) {
	registerFunction("GlobalFnTest", &ExampleClass::GlobalFnTest);
	registerFunction("GlobalNonNativeFn", &ExampleClass::GlobalNonNativeFn);
};


/*
* proto native void GlobalFnTest(string someData);
*/
void ExampleClass::GlobalFnTest(char* someData)
{
	Debugln("GlobalFnTest was called! %s  @ 0x%llX", someData, (unsigned long long)someData);
}

/*
* proto void GlobalNonNativeFn(string someData);
*/
void ExampleClass::GlobalNonNativeFn(Infinity::Enfusion::Enscript::FunctionContext* args, Infinity::Enfusion::Enscript::FunctionResult* result)
{
    char* data = (char*)args->GetArgument(0)->Value;
    result->Result->Value = (char*)"LOL!";
    
    Debugln("GlobalNonNativeFn called! %s result ctx: 0x%llX  full @ 0x%llX", data, (unsigned long long)result->Result, (unsigned long long)result);

   // while (!GetAsyncKeyState(VK_F12)) {
 //       Sleep(1);
   // }
}

/*
* proto native void BigMethod(string someData);
* 
* Dynamic proto methods will always include selfPtr as the first arg
* eg; if ExampleClass has a proto native/non-native FnTest(arg), it's first arg will be a ptr to ExampleClass instance followed by regular args.
*/
void ExampleClass::BigMethod(ManagedScriptInstance* selfPtr, ManagedScriptInstance* playerIdentity)
{
    Debugln("ExampleClass::BigMethod called!");

    ManagedScriptInstance* player = nullptr;

    using RawFn = int(__fastcall*)(ManagedScriptInstance*, FunctionContext*, PNativeArgument*);
    int fnCount = playerIdentity->pType->functionCount;
    typename_functions* tfns = playerIdentity->pType->pFunctions;
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

        if (std::string(fn->name) == "GetPlayer")
        {
            RawFn callFn = reinterpret_cast<RawFn>(fn->fnPtr);

            PNativeArgument retArg = CreateNativeArgument(
                /* value */        fn->name, //THIS CANNOT BE NULL/EMPTY
                /* variableName */ "#return.",
                /* pContext */     playerIdentity->pType->pScriptModule->pContext,
                /* typeTag */      ARG_TYPE_ENTITY,
                /* flags */        ARG_FLAG_NONE
            );
            FunctionResult* retCtx = CreateFunctionResult(retArg);

            Debugln("retArg @ 0x%llX full ctx @ 0x%llX", (unsigned long long)retCtx->Result, (unsigned long long)retCtx);

            callFn(playerIdentity, nullptr, &retCtx->Result);

            player = static_cast<ManagedScriptInstance*>(retArg->Value);
            Debugln("GetPlayer() returned: @ 0x%llX  @ 0x%llX", (unsigned long long)player, (unsigned long long)retArg);

            //Cleanup
            DestroyFunctionResult(retCtx);
            break;
        }
    }

    if (player)
    {
        int fnCount = player->pType->functionCount;
        typename_functions* pFns = player->pType->pFunctions;
        for (int i = 0; i < fnCount; i++)
        {
            typename_function* fn = pFns->List[i];
            if (!fn || !fn->name || !fn->pContext)
                break;

            Debugln("  [%d] fn @ 0x%llX: name='%s' fnPtr: @ 0x%llX ,pContext=0x%llX",
                i,
                (unsigned long long)fn,
                fn->name,
                (unsigned long long)fn->fnPtr,
                (unsigned long long)fn->pContext);
        }
    }

}

char* ExampleClass::TestStaticNativeMethod(char* data)
{
    Debugln("TestStaticNativeMethod was called! %s", data);
    return (char*)"Here's your return!";
}

/*
* proto void TestMethod();
* 
* Dynamic proto methods will always include selfPtr as the first arg
* eg; if CGame has a proto native/non-native TestMethod(arg), it's first arg will be a ptr to CGame instance followed by regular args.
* if the method is "non-native" -> first arg points to self instance, 2nd arg is FunctionContext, 3rd arg is FunctionResult
*/
void ExampleClass::TestMethod(ManagedScriptInstance* selfPtr, FunctionContext* args)
{
    Debugln("TestMethod args -> @  0x%llX", (unsigned long long)args);
    Debugln("TestMethod selfPtr -> @  0x%llX", (unsigned long long)selfPtr);
    Debugln("TestMethod pScriptModule -> @  0x%llX", (unsigned long long)selfPtr->pType->pScriptModule);
    Debugln("TestMethod pScriptContext -> @  0x%llX", (unsigned long long)selfPtr->pType->pScriptModule->pContext);
    
    Debugln("TestMethod pScriptModule name -> %s", selfPtr->pType->pScriptModule->pName);

    Infinity::CallEnforceMethod(selfPtr, "GetTestName");

    //Example, calling dynamic method within a class instance (for non proto/engine linked class declared methods!)
    const char* retRes = (const char*)Infinity::CallEnforceMethod(selfPtr, "JtMethod", (char*)"Hello this is a message!");
    Debugln("CallEnforceMethod returned: %s", retRes);
    // pause here until you hit a key or debugger resumes
    //while (!GetAsyncKeyState(VK_F12)) {
    //    Sleep(1);
   // }

    //Example Calling a global "proto native" method at a given ScriptContext
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
            callFn((char*)"Hey from C++");
            break;
        }
    }

    Debugln("");
    Debugln("");
    Debugln("");

    //Example calling a class defined dynamic "proto native" method
    using RawFn_ = char* (__fastcall*)(ManagedScriptInstance*, char*);
    int fnCount_ = selfPtr->pType->functionCount;
    typename_functions* tfns_ = selfPtr->pType->pFunctions;
    for (int i = 0; i < fnCount_; ++i)
    {
        typename_function* fn_ = tfns_->List[i];
        if (!fn_ || !fn_->name || !fn_->pContext)
            break;

        Debugln("  [%d] fn @ 0x%llX: name='%s' fnPtr: @ 0x%llX ,pContext=0x%llX",
            i,
            (unsigned long long)fn_,
            fn_->name,
            (unsigned long long)fn_->fnPtr,
            (unsigned long long)fn_->pContext);

        if (std::string(fn_->name).find("BigMethod") != std::string::npos)
        {
            RawFn_ callFn_ = reinterpret_cast<RawFn_>(fn_->fnPtr);
            callFn_(selfPtr, (char*)"Hey From C++");
            break;
        }
    }

    Debugln("");
    Debugln("");
    Debugln("");

    //Example calling a class defined static "proto native" method
    using RawFn___ = char* (__fastcall*)(char*);
    int fnCount___ = selfPtr->pType->functionCount;
    typename_functions* tfns___ = selfPtr->pType->pFunctions;
    for (int i = 0; i < fnCount___; ++i)
    {
        typename_function* fn___ = tfns___->List[i];
        if (!fn___ || !fn___->name || !fn___->pContext)
            break;

        Debugln("  [%d] fn @ 0x%llX: name='%s' fnPtr: @ 0x%llX ,pContext=0x%llX",
            i,
            (unsigned long long)fn___,
            fn___->name,
            (unsigned long long)fn___->fnPtr,
            (unsigned long long)fn___->pContext);

        if (std::string(fn___->name).find("TestStaticNativeMethod") != std::string::npos)
        {
            RawFn___ callFn___ = reinterpret_cast<RawFn___>(fn___->fnPtr);
            char* strRET = callFn___((char*)"hey there from C++!");
            Debugln("strRET: %s", strRET);
            break;
        }
    }

    Debugln("");
    Debugln("");
    Debugln("");
    /*
    //Example calling a global scope non-native proto function
    using RawFn__ = char* (__fastcall*)();
    int fnCount__ = selfPtr->pType->pScriptModule->pContext->GlobalFunctionCount;
    typename_functions* tfns__ = selfPtr->pType->pScriptModule->pContext->pGlobalFunctions;
    for (int i = 0; i < fnCount__; ++i)
    {
        typename_function* fn__ = tfns__->List[i];
        if (!fn__ || !fn__->name || !fn__->pContext)
            break;

        Debugln("  [%d] fn @ 0x%llX: name='%s' fnPtr: @ 0x%llX ,pContext=0x%llX",
            i,
            (unsigned long long)fn__,
            fn__->name,
            (unsigned long long)fn__->fnPtr,
            (unsigned long long)fn__->pContext);

        if (std::string(fn__->name).find("GlobalNonNativeFn") != std::string::npos)
        {
            RawFn__ callFn__ = reinterpret_cast<RawFn__>(fn__->fnPtr);
            callFn__();
            break;
        }
    }
    */
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