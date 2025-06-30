#include <InfinityPlugin.h>
#include <EnfusionTypes.hpp>
#include <Patterns.h>
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

FunctionContext* CreateManualContext() {
    auto* ctx = (FunctionContext*)std::calloc(1, sizeof(FunctionContext));
    if (!ctx) return nullptr;

    auto* args = (Arguments*)std::calloc(1, sizeof(Arguments));
    if (!args) {
        std::free(ctx);
        return nullptr;
    }

    ctx->Arguments = args;
    return ctx;
}


using FnLookupMethod = __int64(__fastcall*)(__int64 modulePtr, char* name);
static FnLookupMethod  RealLookupMethod = nullptr;

using FnCallUpMethod = __int64(__fastcall*)(__int64 a1, __int64 a2, int a3, ...);
static FnCallUpMethod  FnCallupMethod = nullptr;

using FnCleanupMethodCall = void(__fastcall*)(__int64 a1);
static FnCleanupMethodCall FnCallCleanup = nullptr;

/*
* Dynamic proto native methods will always include callerPtr as the first arg
* eg; if CGame has a proto native FnTest(arg), it's first arg will be a ptr to CGame instance followed by regular args. 
*/
void ExampleClass::TestMethod(ManagedScriptInstance* selfPtr, FunctionContext* args)
{
    Debugln("TestMethod args -> @  0x%llX", (unsigned long long)args);
    Debugln("TestMethod selfPtr -> @  0x%llX", (unsigned long long)selfPtr);
    Debugln("TestMethod pScriptModule -> @  0x%llX", (unsigned long long)selfPtr->pType->pScriptModule); //Correct

    void* patternLookup = Infinity::Utils::FindPattern(
        "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B FA 48 8B D9 48 85 C9 74 ? 48 89 54 24",
        GetModuleHandle(NULL),
        0
    );
    RealLookupMethod = reinterpret_cast<FnLookupMethod>(patternLookup);

    void* patternLookup2 = Infinity::Utils::FindPattern(
        "44 89 44 24 ? 4C 89 4C 24 ? 53",
        GetModuleHandle(NULL),
        0
    );
    FnCallupMethod = reinterpret_cast<FnCallUpMethod>(patternLookup2);
    

    void* patternLookup3 = Infinity::Utils::FindPattern(
        "48 83 EC ? ? ? ? 4D 85 C9 74 ? 49 8B C9",
        GetModuleHandle(NULL),
        0
    );
    FnCallCleanup = reinterpret_cast<FnCleanupMethodCall>(patternLookup3);

    __int64 idx = RealLookupMethod((__int64)selfPtr->pType, (char*)"BallSackMethodModded");
    Debugln("  => returned index = %d", reinterpret_cast<int*>(idx));
    int64_t v22 = 0;

    __int64 callUpRet = FnCallupMethod((__int64)selfPtr, (__int64)&v22, idx, (char*)"a message for you!");

    Debugln("  => callUpRet = 0x%llX  v22: 0x%llX", (unsigned long long)callUpRet, v22);


    FnCallCleanup(reinterpret_cast<int64_t>(&v22));
    Debugln("  => v22 = 0x%llX after cleanup call", v22);
   
    // pause here until you hit a key or debugger resumes
    //while (!GetAsyncKeyState(VK_F12)) {
     //   Sleep(1);
    //}

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

        /*
        if (std::string(fn->name).find("CallFunction") != std::string::npos)
        {
            Debugln("0x%llX", (unsigned long long)args->GetArgument(1)->pContext);
            //Test out calling an Enforce function
            auto* fnCtx = CreateManualContext(); //Create context
            Debugln("fnCtx: 0x%llX", (unsigned long long)fnCtx);

            // slot 0 = some pointer
            auto* ptrSlot = new NativeArgument{ (void*)selfPtr };
            ptrSlot->pContext = fn->pContext;
            fnCtx->Arguments->List[0] = ptrSlot;
            
            // slot 1 = method name
            auto* nameSlot = new NativeArgument{ (void*)"BallSackMethodModded" };
            nameSlot->pContext = fn->pContext;
            fnCtx->Arguments->List[1] = nameSlot;

            
            // slot 2 = return-value buffer
            auto* retSlot = new NativeArgument{ reinterpret_cast<void*>(const_cast<char*>("someRetBuffer") ) };
            fnCtx->Arguments->List[2] = retSlot;


            // slot 3 = extra parameter
            auto* paramSlot = new NativeArgument{ reinterpret_cast<void*>(const_cast<char*>("someParam")) };
            fnCtx->Arguments->List[3] = paramSlot;
            

            char ok = Infinity::CallEnforceFunction(selfPtr->pType->pScriptModule, fnCtx, fn);
            break;
        }
        */
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