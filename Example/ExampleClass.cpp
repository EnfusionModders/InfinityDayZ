#include <InfinityPlugin.h>
#include <EnfusionTypes.hpp>

#include "NetworkServer.h"
#include "ExampleClass.h"

ManagedClass* ExampleClass::enfTypePtr = nullptr; //ptr to the Enfusion typename declartion of this class (not an actual class instace!)
                                                 //useful for invoking static class member functions

ManagedScriptInstance* ExampleClass::enfInstancePtr = nullptr; //ptr to the Enfusion singleton instance of ExampleClass

ExampleClass::ExampleClass() : BaseScriptClass("ExampleClass")
{
    PluginUtils::NetworkUtils::InitPatterns();
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


    enfTypePtr = GetEnfTypePtr(); //Get ptr here, at this point our typename in Enforce has fully been declared.
};

//These are global, doesn't live within our Enforce typename declaration, but it points it's functionality here :)
void ExampleClass::RegisterGlobalFunctions(Infinity::RegistrationFunction registerFunction) {
	registerFunction("GlobalFnTest", &ExampleClass::GlobalFnTest);
	registerFunction("GlobalNonNativeFn", &ExampleClass::GlobalNonNativeFn);
};

bool ExampleClass::CreateSingleton()
{
    //Create a dynamic Enforce instance of our class
    enfInstancePtr = Infinity::CreateEnforceInstance(enfTypePtr->pScriptModule, (const char*)"ExampleClass");

    return (enfInstancePtr != 0);
}

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

    if (!playerIdentity)
    {
        Warnln("ExampleClass::DynamicProtoNativeMethod playerIdentity was NULL!");
        return;
    }

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
void ExampleClass::DynamicProtoMethod(ManagedScriptInstance* selfPtr, FunctionContext* args, FunctionResult* result)
{
    Println("DynamicProtoMethod was called!");

    //Example, calling dynamic method within a class instance (for non proto/engine linked class declared methods!)
    char* retRes = (char*)Infinity::CallEnforceMethod(selfPtr, "JtMethod", (char*)"Hello this is a message!");
    Println("CallEnforceMethod -> ExampleClass::JtMethod returned: %s", retRes);
    
    //Example Calling a global "proto native" method at a given ScriptContext
    typename_function* fn = selfPtr->pType->pScriptModule->pContext->FindGlobalFunctionPtr("GlobalFnTest");
    if (fn)
    {
        using FnGlobalFnTest = void*(__fastcall*)(char* someData);
        FnGlobalFnTest callFn = reinterpret_cast<FnGlobalFnTest>(fn->fnPtr);
        callFn((char*)"Hey from C++");
    }

    
    Println("\n\n");

    //Example calling a class defined dynamic "proto native" method
    typename_function* fnCtx = selfPtr->FindFunctionPointer("DynamicProtoNativeMethod");
    if (fnCtx)
    {
        using FnDynamicProtoNativeMethod = void*(__fastcall*)(ManagedScriptInstance*, PlayerIdentity*);
        FnDynamicProtoNativeMethod CallFn = reinterpret_cast<FnDynamicProtoNativeMethod>(fnCtx->fnPtr);

        PlayerIdentity* pidPtr = reinterpret_cast<PlayerIdentity*>(args->GetArgument(0)->Value);
        CallFn(selfPtr, pidPtr); //must pass selfPtr as first arg because it's a dynamic function belonging to an instance of class
    }

    Println("\n\n");
    
    
    //Example calling a class defined static "proto native" method
    using FnTestStaticNativeFunction = char*(__fastcall*)(char*);
    typename_function* tfnCtx = selfPtr->FindFunctionPointer("TestStaticNativeFunction");
    if (tfnCtx)
    {
        FnTestStaticNativeFunction _CallFn = reinterpret_cast<FnTestStaticNativeFunction>(tfnCtx->fnPtr);
        char* strReturn = _CallFn((char*)"hey there from C++!");
        Println("strReturn: %s", strReturn);
    }
    

    Println("\n\n");

    //Example calling a global scope non-native proto function
    typename_function* gPtrFn = selfPtr->pType->pScriptModule->pContext->FindGlobalFunctionPtr("GlobalNonNativeFn");
    if (gPtrFn)
    {
        using FnGlobalNonNativeFn = int(__fastcall*)(FunctionContext*, PNativeArgument*); //input args, return arg
        FnGlobalNonNativeFn FnCall = reinterpret_cast<FnGlobalNonNativeFn>(gPtrFn->fnPtr);

        Println("fn @ 0x%llX: name='%s' fnPtr: @ 0x%llX ,pContext=0x%llX", (unsigned long long)gPtrFn, gPtrFn->name, (unsigned long long)gPtrFn->fnPtr, (unsigned long long)gPtrFn->pContext);

        PNativeArgument inputArg = CreateNativeArgument(
            (char*)"yo, here's your string",
            (const char*)"someData",
            enfTypePtr->pScriptModule->pContext,
            ARG_TYPE_STRING,
            ARG_FLAG_NONE
        );
        FunctionContext* argsCtx = CreateFunctionContext(); //create context that will hold our input params/args
        argsCtx->SetArgument(0, inputArg);

        PNativeArgument returnArg = CreateNativeArgument(
            /* value */        gPtrFn->name, //THIS CANNOT BE NULL/EMPTY
            /* variableName */ "#return.",
            /* pContext */     enfTypePtr->pScriptModule->pContext,
            /* typeTag */      ARG_TYPE_STRING,
            /* flags */        ARG_FLAG_NONE
        );
        FunctionResult* returnCtx = CreateFunctionResult(returnArg); //create context that will hold our return value

        FnCall(argsCtx, &returnCtx->Result); //Call & pass the required arguments to the Enforce engine call

        //Now we read the return from the call
        char* resultStr = static_cast<char*>(returnArg->Value);
        Println("GlobalNonNativeFn() returned: %s", resultStr);

        //Must cleanup to free-up memory we allocated
        DestroyFunctionResult(returnCtx);
        DestroyFunctionContext(argsCtx);
    }
}