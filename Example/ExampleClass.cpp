#include <InfinityPlugin.h>
#include <EnfusionTypes.hpp>

#include "NetworkServer.h"
#include "ExampleClass.h"

ManagedClass* enfTypePtr = nullptr; //ptr to the Enfusion typename declartion of this class (not an actual class instace!)

ExampleClass::ExampleClass() : BaseScriptClass("ExampleClass")
{
    PluginUtils::NetworkUtils::FindNetworkServer();
    Infinity::PrintToConsole((const char*)"[Plugin] ExampleClass instance created! %s", (char*)"some charrrrrr");
}

//static proto function that live within ExampleClass on Enforce side of things
void ExampleClass::RegisterStaticClassFunctions(Infinity::RegistrationFunction registerMethod){
	registerMethod("TestStaticFunction", &ExampleClass::TestStaticFunction);
    registerMethod("TestStaticNativeFunction", &ExampleClass::TestStaticNativeFunction);
};

//dynamic proto native functions that live within ExampleClass on Enforce side of things
void ExampleClass::RegisterDynamicClassFunctions(Infinity::RegistrationFunction registerMethod) {
	registerMethod("DynamicProtoMethod", &ExampleClass::DynamicProtoMethod);
	registerMethod("DynamicProtoNativeMethod", &ExampleClass::DynamicProtoNativeMethod);

    enfTypePtr = GetEnfTypePtr();
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
    Println("GlobalFnTest was called! %s  @ 0x%llX", someData, (unsigned long long)someData);
}

/*
* proto void GlobalNonNativeFn(string someData);
*/
void ExampleClass::GlobalNonNativeFn(FunctionContext* args, FunctionResult* result)
{
    char* data = (char*)args->GetArgument(0)->Value;
    result->Result->Value = (char*)"LOL!";
    
    Println("GlobalNonNativeFn called! %s result ctx: 0x%llX  full @ 0x%llX", data, (unsigned long long)result->Result, (unsigned long long)result);
}

/*
* static proto string TestStaticFunction(string someStr);
*/
void ExampleClass::TestStaticFunction(FunctionContext* args, FunctionResult* result)
{
    const char* input = (const char*)args->GetArgument(0)->Value; // arg0 is a string

    Println("TestStaticFunction: %s", input);
    result->Result->Value = reinterpret_cast<void*>((char*)"Some callback string"); //write our return value

    //Get our array ptr and write to it
    EnArray* ptr = reinterpret_cast<EnArray*>(args->GetArgument(1)->Value);
    if (ptr)
    {
        Println("Array type: %s", ptr->pType->name);
        char* someStr = ptr->Get<char*>(0);
        Println("Array has: %s @ index: 0", someStr);

        ptr->Insert((char*)"another string for you!");
    }

    Println("enfPtr fncount - > %d", enfTypePtr->functionsCount);

    //Example calling a non-engine global enforce method
    char* retRes = (char*) Infinity::CallGlobalEnforceMethod(enfTypePtr->pScriptModule, "MyGlobalMethod", (char*)"this is a test!");
    Println("CallGlobalEnforceMethod 'MyGlobalMethod' returned: %s", retRes);

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

/*
* static proto native string TestStaticNativeFunction(string someData);
*/
char* ExampleClass::TestStaticNativeFunction(char* data)
{
    Println("TestStaticNativeFunction was called! %s", data);
    return (char*)"Here's your return!";
}

/*
* proto native void DynamicProtoNativeMethod(PlayerIdentity pid);
* 
* Dynamic proto methods will always include selfPtr as the first arg
* eg; if ExampleClass has a proto native/non-native DynamicProtoNativeMethod(arg), it's first arg will be a ptr to ExampleClass instance followed by regular args.
*/
void ExampleClass::DynamicProtoNativeMethod(ManagedScriptInstance* selfPtr, PlayerIdentity* playerIdentity)
{
    Println("ExampleClass::DynamicProtoNativeMethod called!");

    typename_function* fn = playerIdentity->FindFunctionPointer("GetPlayer");
    if (fn)
    {
        using FnGetPlayer = int(__fastcall*)(ManagedScriptInstance*, FunctionContext*, PNativeArgument*); //ptr to PlayerIdentity, input args, return arg
        FnGetPlayer GetPlayer = reinterpret_cast<FnGetPlayer>(fn->fnPtr);

        Println("fn @ 0x%llX: name='%s' fnPtr: @ 0x%llX ,pContext=0x%llX", (unsigned long long)fn, fn->name, (unsigned long long)fn->fnPtr, (unsigned long long)fn->pContext);

        PNativeArgument returnArg = CreateNativeArgument(
            /* value */        fn->name, //THIS CANNOT BE NULL/EMPTY
            /* variableName */ "#return.",
            /* pContext */     playerIdentity->pType->pScriptModule->pContext,
            /* typeTag */      ARG_TYPE_ENTITY,
            /* flags */        ARG_FLAG_NONE
        );
        FunctionResult* returnCtx = CreateFunctionResult(returnArg); //create context that will hold our return value which will be written by Enforce

        GetPlayer(playerIdentity, nullptr, &returnCtx->Result); //Call & pass the required arguments to the Enforce engine call

        //Now we have the instance to player from PlayerIdentity::proto Man GetPlayer();
        ManagedScriptInstance* player = static_cast<ManagedScriptInstance*>(returnArg->Value);
        Println("GetPlayer() returned: @ 0x%llX", (unsigned long long)player);

        //Must cleanup to free-up memory we allocated
        DestroyFunctionResult(returnCtx);
    }
}


/*
* proto void DynamicProtoMethod();
* 
* Dynamic proto methods will always include selfPtr as the first arg
* eg; if ExampleClass has a proto native/non-native DynamicProtoMethod(arg), it's first arg will be a ptr to ExampleClass instance followed by regular args.
* if the method is "non-native" -> first arg points to self instance, 2nd arg is FunctionContext, 3rd arg is FunctionResult
*/
void ExampleClass::DynamicProtoMethod(ManagedScriptInstance* selfPtr, FunctionContext* args)
{
    Println("DynamicProtoMethod was called!");

    //Example, calling dynamic method within a class instance (for non proto/engine linked class declared methods!)
    char* retRes = (char*)Infinity::CallEnforceMethod(selfPtr, "JtMethod", (char*)"Hello this is a message!");
    Println("CallEnforceMethod returned: %s", retRes);
    

    //Example Calling a global "proto native" method at a given ScriptContext
    int fnCount = selfPtr->pType->pScriptModule->pContext->GlobalFunctionCount;
    typename_functions* tfns = selfPtr->pType->pScriptModule->pContext->pGlobalFunctions;

    using RawFn = char* (__fastcall*)(char* someData);

    for (int i = 0; i < fnCount; ++i)
    {
        typename_function* fn = tfns->List[i];
        if (!fn || !fn->name || !fn->pContext)
            break;

        Println("  [%d] fn @ 0x%llX: name='%s' fnPtr: @ 0x%llX ,pContext=0x%llX",
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

    Println("");
    Println("");
    Println("");

    //Example calling a class defined dynamic "proto native" method
    using RawFn_ = char* (__fastcall*)(ManagedScriptInstance*, char*);
    int fnCount_ = selfPtr->pType->functionCount;
    typename_functions* tfns_ = selfPtr->pType->pFunctions;
    for (int i = 0; i < fnCount_; ++i)
    {
        typename_function* fn_ = tfns_->List[i];
        if (!fn_ || !fn_->name || !fn_->pContext)
            break;

        Println("  [%d] fn @ 0x%llX: name='%s' fnPtr: @ 0x%llX ,pContext=0x%llX",
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

    Println("");
    Println("");
    Println("");

    //Example calling a class defined static "proto native" method
    using RawFn___ = char* (__fastcall*)(char*);
    int fnCount___ = selfPtr->pType->functionCount;
    typename_functions* tfns___ = selfPtr->pType->pFunctions;
    for (int i = 0; i < fnCount___; ++i)
    {
        typename_function* fn___ = tfns___->List[i];
        if (!fn___ || !fn___->name || !fn___->pContext)
            break;

        Println("  [%d] fn @ 0x%llX: name='%s' fnPtr: @ 0x%llX ,pContext=0x%llX",
            i,
            (unsigned long long)fn___,
            fn___->name,
            (unsigned long long)fn___->fnPtr,
            (unsigned long long)fn___->pContext);

        if (std::string(fn___->name).find("TestStaticNativeMethod") != std::string::npos)
        {
            RawFn___ callFn___ = reinterpret_cast<RawFn___>(fn___->fnPtr);
            char* strRET = callFn___((char*)"hey there from C++!");
            Println("strRET: %s", strRET);
            break;
        }
    }

    Println("");
    Println("");
    Println("");
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
