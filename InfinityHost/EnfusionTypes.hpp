﻿#ifndef PLUGIN_TYPES
#define PLUGIN_TYPES

#include <Windows.h>
#include <stdint.h>
#include <algorithm>
#include <cstdlib>

namespace Infinity {
	namespace Enfusion {
		namespace Enscript {
			//——— Enforce engine type tags ---
			static constexpr uint32_t ARG_TYPE_BOOL = 0x00000000; // ????
			static constexpr uint32_t ARG_TYPE_INT = 0x00020000; // signed int
			static constexpr uint32_t ARG_TYPE_FLOAT = 0x00030000; // float
			static constexpr uint32_t ARG_TYPE_STRING = 0x00050000; // string
			static constexpr uint32_t ARG_TYPE_VECTOR = 0x00000000; // ????
			static constexpr uint32_t ARG_TYPE_ENTITY = 0x00060000; // EntityRef

			//——— flags  ———
			static constexpr uint32_t ARG_FLAG_NONE = 0x00000000;
			static constexpr uint32_t ARG_FLAG_SKIP = 0x00000040; // skip / readonly / reserved

			//--- custom function context data structure
			typedef struct NativeArgument
			{
				void* Value;
				void* VariableName;
				uint8_t _pad0[0x20 - 0x10];
				void* pContext;
			} NativeArgument, * PNativeArgument;

			typedef struct Arguments
			{
				PNativeArgument List[8];
			} Arguments, * PArguments;
			
			class FunctionContext
			{
			public:
				PArguments Arguments;
				PNativeArgument GetArgument(int idx)
				{
					if (!Arguments || !Arguments->List)
						return NULL;
					return Arguments->List[idx];
				}

				PNativeArgument SetArgument(int idx, PNativeArgument arg)
				{
					if (!Arguments || !Arguments->List)
						return NULL;
					return Arguments->List[idx] = arg;
				}
			};

			typedef struct FunctionResult {
				PNativeArgument Result;
			} FunctionResult, * PFunctionResult;


			namespace Framework {
				class type;
				class ScriptContext;
				class ScriptModule;
				class WeakPtrTracker;
				class ManagedScriptInstance;

				struct typename_variable;
				struct typename_variables;
				struct typename_function;
				struct typename_functions;

#pragma pack(push,1)
				struct typename_function
				{
					uint8_t _pad0[0x08];	// 0x00–0x07: skip whatever lives here
					void* fnPtr;				// 0x08–0x0F: the real code pointer
					uint8_t _pad1[0x40 - 0x10];  // = 0x30 bytes  0x10–0x3F: skip up to the context field at 0x40
					ScriptContext* pContext; // 0x40–0x47: the script-context pointer
					char* name;				// 0x48–0x4F: function name pointer
				};

				struct typename_functions
				{
					class typename_function* List[10]; //0x0000, 10 here because "class Class" in Enforce has 10 functions
				};
#pragma pack(pop)

				class typename_variable
				{
				public:
					void* varPtr; //0x0000
					char* variable_name; //0x0008
				private:
					char pad_0010[16]; //0x0010
				public:
					class ScriptContext* pScriptContext; //0x0020
				private:
					char pad_0028[32]; //0x0028
				}; //Size: 0x0048

				struct typename_variables
				{
					class typename_variable* List[32]; //0x0000
				};

				class ScriptContext
				{
				private:
					char pad_0000[8]; //0x0000
				public:
					class type* pType; //0x0008
				private:
					char pad_0010[32]; //0x0010
				public:
					char* pName; //0x0030
					typename_functions* pGlobalFunctions; //0x0038 (this holds ENGINE proto globals)
				private:
					char pad_0040[4]; //0x0040
				public:
					int32_t GlobalFunctionCount; //0x0044
					typename_variables* pGlobalVariables; //0x0048
				private:
					char pad_0050[4]; //0x0050
				public:
					int32_t GlobalVarCount; //0x0054
				private:
					char pad_0058[104]; //0x0058
				public:
					typename_function* FindGlobalFunctionPtr(std::string fnName)
					{
						if (!pGlobalFunctions)
							return nullptr;

						for (int i = 0; i < GlobalFunctionCount; ++i)
						{
							typename_function* fn = pGlobalFunctions->List[i];
							if (!fn || !fn->name || !fn->pContext)
								break;

							if (std::string(fn->name) == fnName)
								return fn;
						}
						return nullptr;
					}
				}; //Size: 0x00C0
			
				class ScriptModule
				{
				private:
					char pad_0000[8]; //0x0000
				public:
					class type* pType; //0x0008
				private:
					char pad_0010[32]; //0x0010
				public:
					char* pName; //0x0030
					typename_functions* pGlobalFunctions; //0x0038 (this holds non engine globals)
				private:
					char pad_0040[4]; //0x0040
				public:
					int32_t GlobalFunctionCount; //0x0044
					typename_variables* pGlobalVariables; //0x0048
				private:
					char pad_0050[4]; //0x0050
				public:
					int32_t GlobalVarCount; //0x0054
				private:
					char pad_0058[48]; //0x0058
				public:
					class ScriptContext* pContext; //0x0088
				private:
					char pad_0090[48]; //0x0090
				}; //Size: 0x00C0

				class ManagedClass
				{
				private:
					char pad_0000[16]; //0x0000
				public:
					char* name; //0x0010
					class ScriptModule* pScriptModule; //0x0018
					class ManagedClass* pParentType; //0x0020
				private:
					char pad_0028[16]; //0x0028
				public:
					typename_variables* pVariables; //0x0038
				private:
					char pad_0040[4]; //0x0040
				public:
					int32_t VarCount; //0x0044
					typename_functions* pFunctions; //0x0048
				private:
					char pad_0050[4]; //0x0050
				public:
					int32_t functionsCount; //0x0054
				private:
					char pad_0064[64]; //0x0064
				public:
					typename_function* FindFunctionPointer(std::string fnName)
					{
						if (!pFunctions)
							return nullptr;

						typename_functions* tfns = pFunctions;
						if (!tfns)
							return nullptr;

						for (int i = 0; i < functionsCount; ++i)
						{
							typename_function* fn = tfns->List[i];
							if (!fn || !fn->name || !fn->pContext)
								break;

							if (std::string(fn->name) == fnName)
								return fn;
						}
						return nullptr;
					}
				}; //Size: 0x00A4

				
				class type
				{
				private:
					char pad_0000[8];            // 0x0000
				public:
					uint32_t flags;              // 0x0008
				private:
					char pad_000C[4];            // 0x000C
				public:
					char* name;           // 0x0010
					ScriptModule* pScriptModule; // 0x0018
				private:
					char pad_0020[8];            // 0x0020
				public:
					type* pParent;        // 0x0028
				private:
					char pad_0030[8];            // 0x0030
				public:
					typename_variables* pVariables; // 0x0038
				private:
					char pad_0040[4];            // 0x0040
				public:
					uint32_t     variableCount;  // 0x0044
					typename_functions* pFunctions; // 0x0048 (holds all types of functions, including proto engine ones)
				private:
					char _pad_0050[4];           // 0x0050–0x53
				public:
					uint32_t     functionCount;  // 0x0054
				private:
					char _pad_0058[136];         // 0x0058–0xDF (fills out to sizeof 0xE0)
				};

				class WeakPtrTracker
				{
				public:
					char pad_0000[24]; //0x0000
				}; //Size: 0x0018

				class ManagedScriptInstance
				{
				private:
					char pad_0000[8]; //0x0000
				public:
					class type* pType; //0x0008
				private:
					char pad_0010[16]; //0x0010
				public:
					class WeakPtrTracker* pPtrTracker; //0x0020
				private:
					char pad_0028[8]; //0x0028
				public:
					typename_function* FindFunctionPointer(std::string fnName)
					{
						if (!pType)
							return nullptr;

						typename_functions* tfns = pType->pFunctions;
						if (!tfns)
							return nullptr;

						for (int i = 0; i < pType->functionCount; ++i)
						{
							typename_function* fn = tfns->List[i];
							if (!fn || !fn->name || !fn->pContext)
								break;

							if (std::string(fn->name) == fnName)
								return fn;
						}
						return nullptr;
					}
				}; //Size: 0x0040

				class PlayerIdentity: public ManagedScriptInstance
				{
				public:
					int32_t dpnid; //0x0030
				private:
					char pad_0034[0x40 - 0x34];
				};

				class FileSystemDef
				{
				public:
					char pad_0000[24]; //0x0000
					char* path; //0x0018
					char* key; //0x0020
					uint32_t type_flag; //0x0028
					char pad_002C[12]; //0x002C
				}; //Size: 0x0038

				class ListEntry
				{
				public:
					char pad_0000[8]; //0x0000
					class FileSystemDef* pDefinition; //0x0008
				}; //Size: 0x0010

				class FileSystemDefinitions
				{
				public:
					ListEntry List[127]; //0x0000
					char pad_07F0[1208]; //0x07F0
				}; //Size: 0x0CA8

				class FileHandler
				{
				public:
					char pad_0000[280]; //0x0000
					class FileSystemDefinitions* pDefinitions; //0x0118
					char pad_0120[672]; //0x0120
				}; //Size: 0x03C0

				class KeyValuePair
				{
				private:
					char pad_0000[4]; //0x0000
				public:
					bool hasValue; //0x0004
				private:
					char pad_0005[3]; //0x0005
				public:
					void* pKey; //0x0008
					void* pValue; //0x0010
				}; //Size: 0x0018

				class KeyValuePair_Map
				{
				public:
					KeyValuePair Pairs[255]; //0x0000
				}; //Size: 0x0120

				class EnMap
				{
				public:
					char pad_0000[8]; //0x0000
				public:
					class type* pType; //0x0008
				private:
					char pad_0010[16]; //0x0010
				public:
					class WeakPtrTracker* pWeakPointerTracker; //0x0020
				private:
					char pad_0028[8]; //0x0028
				public:
					class KeyValuePair_Map* pMap; //0x0030
					int32_t Capacity; //0x0038
				private:
					char pad_003C[4]; //0x003C
				public:
					int32_t Size; //0x0040
				private:
					char pad_0048[0xC]; //0x0044


				private:
					int GetRawIndex(int32_t index)
					{
						//--- iterate over the entire capacity & look for element with our desired index
						int it = 0;
						if (index < Capacity && pMap)
						{
							for (int i = 0; i < Capacity; i++)
							{
								KeyValuePair pair = pMap->Pairs[i];
								if (pair.hasValue)
								{
									if (it == index)
										return i;

									it++;
								}
							}
						}
						return -1;
					}
				public:

					template<class TKey>
					TKey GetKey(int32_t index)
					{
						int idx = GetRawIndex(index);
						if (idx >= 0)
						{
							return (TKey)pMap->Pairs[idx].pKey;
						}
						return NULL;
					}
					template<class TValue>
					TValue ElementAt(int32_t index)
					{
						int idx = GetRawIndex(index);
						if (idx >= 0)
						{
							return (TValue)pMap->Pairs[idx].pValue;
						}
						return NULL;
					}
					template<class TKey, class TValue>
					TValue Get(TKey key)
					{
						if (!pMap)
							return NULL;
						//iterate over entire capacity to find our key match ( sadly we didn't implement efficient lookup :( )
						for (int i = 0; i < Capacity; i++)
						{
							if (pMap->Pairs[i].hasValue)
							{
								TKey srcKey = (TKey)pMap->Pairs[i].pKey;
								if (srcKey == key)
								{
									return (TValue)pMap->Pairs[i].pValue;
								}
							}
						}
						return NULL;
					}

				}; //Size: 0x0050

#pragma pack(push,1)
				struct EnArray {
					char           _pad0[0x08];		// 0x00
					class type*    pType;			// 0x08
					char           _pad1[0x10];		// 0x10
					void*		   pWeakPtr;		// 0x20
					void*		   pBuffer;			// 0x28
					int32_t        Capacity;		// 0x30
					int32_t        Size;			// 0x34
					char           _pad2[0x18];		// 0x38

					inline int   GetCapacity() const { return Capacity; }
					inline int   GetSize()     const { return Size; }
					inline void  SetCapacity(int cap) { Capacity = cap; }

					template<typename T>
					inline T Get(int idx) const {
						if (idx < 0 || idx >= Size || !pBuffer)
							return T{};
						return reinterpret_cast<T*>(pBuffer)[idx];
					}

					template<typename T>
					inline bool Insert(T value)
					{
						if (!value)
							return false;

						// compute what the new size
						int newSize = Size + 1;

						// if we have no buffer yet, or we need more room allocate/grow
						if (!pBuffer)
						{
							// first allocation: give us capacity = newSize*2 (or at least 2)
							int newCap = (std::max)(2, newSize * 2);
							void** buf = static_cast<void**>(std::malloc(newCap * sizeof(void*)));
							if (!buf)
								return false;

							pBuffer = buf;
							Capacity = newCap;
						}
						else if (newSize > Capacity)
						{
							// grow existing buffer
							int newCap = newSize * 2;
							void** buf = static_cast<void**>(std::realloc(pBuffer, newCap * sizeof(void*)));
							if (!buf)
								return false;

							pBuffer = buf;
							Capacity = newCap;
						}

						// now we definitely have room
						auto* arr = reinterpret_cast<T*>(pBuffer);
						arr[Size] = value; // write into old‐Size slot
						Size = newSize; // bump to the new size

						return true;
					}

					template<typename T>
					inline bool Remove(T value)
					{
						if (!value || !pBuffer) return false;
						int sz = Size;
						auto* arr = reinterpret_cast<T*>(pBuffer);
						for (int i = 0; i < sz; ++i)
						{
							if (arr[i] == value)
							{
								// shift everything after i down one slot
								for (int j = i; j < sz - 1; ++j)
									arr[j] = arr[j + 1];
								arr[sz - 1] = T{};  // clear last slot
								Size = sz - 1;
								return true;
							}
						}
						return false;
					}

					inline bool RemoveAt(int idx)
					{
						if (idx < 0 || idx >= Size || !pBuffer)
							return false;
						int sz = Size;
						auto* arr = reinterpret_cast<void**>(pBuffer);
						for (int i = idx; i < sz - 1; ++i)
							arr[i] = arr[i + 1];
						arr[sz - 1] = nullptr;
						Size = sz - 1;
						return true;
					}
				};
#pragma pack(pop)
			}
		}
	}
}

#endif