#pragma once

#include "ScriptRegistrator.h"

extern std::string PATTERN_REG_ENGINE_CLASS_FUNCTION;
extern std::string PATTERN_REG_STATIC_PROTO_FUNCTION;
extern std::string PATTERN_REG_DYNAMIC_PROTO_FUNCTION;
extern std::string PATTERN_REG_GLOBAL_PROTO_FUNCTION;

typedef __int64 EnforceScriptCtx;

typedef __int64(__fastcall* FnEngineRegisterClass) (__int64 moduleCtx, char* className, unsigned __int8 a3);
typedef unsigned int***(__fastcall* FnRegisterGlobalFunc) (__int64 a1, const char* name, void* fnPtr, unsigned int a4);

extern BaseScriptManager* g_BaseScriptManager;
extern FnEngineRegisterClass f_EngineRegisterClass;
extern FnRegisterGlobalFunc f_RegisterGlobalFunc;
extern EnforceScriptCtx g_pEnforceScriptContext;

bool InitScriptEngine();