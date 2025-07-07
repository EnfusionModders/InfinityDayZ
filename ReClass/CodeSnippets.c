//Snippet for creating an Enforce class instance 
using FnCreateInst = __int64(__fastcall*)(ScriptModule* a1, char* a2, char a3);
static FnCreateInst f_createInst = reinterpret_cast<FnCreateInst>(Infinity::Utils::FindPattern("48 83 EC ? 45 0F B6 C8 4C 8B C2", GetModuleHandle(NULL), 0));

using FnMagicCall = __int64(__fastcall*)(__int64 a1);
static FnMagicCall f_FnMagicCall = reinterpret_cast<FnMagicCall>(Infinity::Utils::FindPattern("48 89 5C 24 ? 57 48 83 EC ? 48 8B F9 48 8B D1", GetModuleHandle(NULL), 0));

__int64 v0 = f_createInst(pCallBackHelper->GetEnfClassPtr()->pScriptModule, (char*)"EnfCallBackWrapper", 0);
__int64 ret = f_FnMagicCall(v0);

Debugln("v0 -> 0x%llX  ret: @ 0x%llX", (unsigned long long)v0, (unsigned long long)ret);

ManagedScriptInstance* pNewInst = reinterpret_cast<ManagedScriptInstance*>(ret);


//Snippet for declaring a global scope variable
using FnDeclareGlobalVar = void* (__fastcall*)(ScriptContext* pCtx, char* varName, __int64 pClassType, int flags);
static FnDeclareGlobalVar f_DeclareGlobalVar = reinterpret_cast<FnDeclareGlobalVar>(Infinity::Utils::FindPattern("48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 41 8B E9 49 8B D8", GetModuleHandle(NULL), 0));

void* varPtr = f_DeclareGlobalVar(pCallBackHelper->GetEnfClassPtr()->pScriptModule->pContext, (char*)"g_CallBackInst", ret, 0);

uint64_t* pGlobalVar = reinterpret_cast<uint64_t*>(varPtr);
*pGlobalVar = static_cast<uint64_t>(ret);

Debugln("varPtr:  @  0x%llX", (unsigned long long)varPtr);


//Snippet for calling a global scoped function (for non-engine/proto functions)
using FnGetIndex = int (__fastcall*)(__int64 a1, char* fnName);
static FnGetIndex f_GetIdx = reinterpret_cast<FnGetIndex>(Infinity::Utils::FindPattern("40 53 48 83 EC ? 48 8B D9 E8 ? ? ? ? 48 89 44 24", GetModuleHandle(NULL), 0));

using FnCallGlobalMethod = void* (__fastcall*)(__int64 scriptCtx, FunctionResult* a2, int index, __int64* someRet, ...);
static FnCallGlobalMethod f_CallGlobalFn = reinterpret_cast<FnCallGlobalMethod>(Infinity::Utils::FindPattern("40 53 48 83 EC ? ? ? ? ? 48 8D 44 24 ? 48 8B DA 4C 8D 4C 24 ? 48 89 44 24 ? 0F 29 44 24 ? E8 ? ? ? ? 48 8B C3 48 83 C4 ? 5B C3 ? 48 89 5C 24 ? 48 89 6C 24", GetModuleHandle(NULL), 0));


int index = f_GetIdx(reinterpret_cast<__int64>(selfPtr->pType->pScriptModule), (char*)"BigGlobalFunction");
Debugln("index -> %d", index);
if (index > -1)
{
    __int64 someRet;
    Infinity::Enfusion::Enscript::FunctionResult ret;
    auto callUpRet = f_CallGlobalFn(
        reinterpret_cast<__int64>(selfPtr->pType->pScriptModule),
        &ret,
        index,
        &someRet
     );

    Debugln("Return from Global method: 0x%llX", (unsigned long long)ret.Result->Value);

    f_CleanupUpMethodCall(&ret);
   // uint64_t* pGlobalVar = reinterpret_cast<uint64_t*>(pOurVar);
  //  *pGlobalVar = static_cast<uint64_t>(ret.Result->Value);

  //  Debugln("pOurVar -> @  0x%llX", (unsigned long long)pOurVar);
}



//-----------------
// 1) create your context
    Infinity::Enfusion::Enscript::FunctionContext* fc = CreateFunctionContext();

    // 2) build an argument
    Infinity::Enfusion::Enscript::PNativeArgument arg0 = CreateNativeArgument(
        /* value */        reinterpret_cast<void*>((char*)"Yo WHAT'S UP!"),
        /* variableName */ reinterpret_cast<void*>(const_cast<char*>("param1")),
        /* pContext */     selfPtr->pType->pScriptModule->pContext
    );

    // 3) install it into slot 0
    fc->SetArgument(0, arg0);

    //Return context
    // 1) Create a native-argument as before:
    Infinity::Enfusion::Enscript::PNativeArgument arg = CreateNativeArgument(
        reinterpret_cast<void*>((char*)"ReturnValue..."),
        reinterpret_cast<void*>(const_cast<char*>("return")),
        selfPtr->pType->pScriptModule->pContext
    );

    // 2) Wrap it in a FunctionResult:
    Infinity::Enfusion::Enscript::PFunctionResult fnResult = CreateFunctionResult(arg);