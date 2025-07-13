

DayZ Enfusion Infinity 
===============

## What is it?

InfinityDayZ is a C++ plugin and extension framework for the Enfusion engine. The goal of Infinity is to empower modders with the ability to extend Enscript with custom native functionality.  

Simply put, InfinityDayZ provides modders with the framework to implement their own `proto` functionality into the game engine. Natively,`proto` methods call underlying C++ functionality, which modders can not access on their own. 

This is achieved with a plugin system implemented by the InfinityHost library. Modders develop their own drag-and-drop plugins.

## Feature List

- [x] Load Library on Engine Start using `hid.dll`
- [x] Pretty print to console
- [x] Plugin loading from `./Plugins` directory
- [x] Plugins can register custom `class` definitions
- [x] Plugins can register class dynamic & static `proto` definitions
- [x] Plugins can register class dynamic & static `proto native` definitions
- [x] Plugins can register global scope `proto` definitions
- [x] Plugins can register global scope `proto native` definitions
- [x] Enabled `FindPattern` for plugins, so they can add more advanced functionality.
- [x] Plugins can implement Engine function detouring to override and intercept original native engine calls (ExamplePlugin includes a demo "NetworkServer")
- [x] Framework for handling non-native `proto` definitions easily
- [x] Framework for easily handling Managed classes from Enscript
- [x] Framework for easily handling custom registered `class` definitions
- [x] Plugins can construct custom `class` definitions natively via declaring their own types using Enforce Script
- [x] Plugins can register non-static class `proto native` & `native` definitions
- [x] Plugins can call original `proto native/non-native` engine functions from any given dynamic or static class types
- [x] Plugins can call any global/class-scope defined methods/functions `non-engine` at a given script Module scope (eg; Game, World, Mission etc..)  
      
## Plugins

Plugins are the main feature of InfinityDayZ. With plugins, you can load your own native assembly into the Enfusion Server process. This assembly can register it's own `proto` functions with the Enfusion scripting engine. 

Plugins go into the `Plugins` folder in the DayZ Server. These libraries are automatically loaded on server start. 

### Technical Details

Plugins are loaded during the Module Init phase. During this time, the game registers it's own script classes and types. Plugins are loaded inconjunction when the game loads it's own functions.

In order for a plugin to start, it must export the `OnPluginLoad` method. Here is a sample:

```c++
#include <InfinityPlugin.h>
#include "ExampleClass.h"

using namespace Infinity;
using namespace Infinity::Logging;

// this routine is called when it's time to register our script classes
__declspec(dllexport) void OnPluginLoad()
{
	Println("Loading example plugin...");

	RegisterScriptClass(std::make_unique<ExampleClass>()); // register our example class

	Println("Example plugin loaded!");
}
```

#### More Code Samples

Custom `proto` functionality is implemented using the `BaseScriptClass` and `RegisterScriptClass` tooling.
Example on C++ Side (Plugin implementation)
```c++
#include <InfinityPlugin.h>
#include <EnfusionTypes.hpp>
#include "ExampleClass.h"

ExampleClass::ExampleClass() : BaseScriptClass("ExampleClass"){}

//static proto function that live within ExampleClass on Enforce side of things
void ExampleClass::RegisterStaticClassFunctions(Infinity::RegistrationFunction registerMethod){
	registerMethod("TestStaticFunction", &ExampleClass::TestStaticFunction);
    	registerMethod("TestStaticNativeFunction", &ExampleClass::TestStaticNativeFunction);
};

//dynamic proto native functions that live within ExampleClass on Enforce side of things
void ExampleClass::RegisterDynamicClassFunctions(Infinity::RegistrationFunction registerMethod) {
	registerMethod("DynamicProtoMethod", &ExampleClass::DynamicProtoMethod);
	registerMethod("DynamicProtoNativeMethod", &ExampleClass::DynamicProtoNativeMethod);
};

//These are global, doesn't live within our Enforce typename declaration, but it points it's functionality here :)
void ExampleClass::RegisterGlobalFunctions(Infinity::RegistrationFunction registerFunction) {
	registerFunction("GlobalFnTest", &ExampleClass::GlobalFnTest);
	registerFunction("GlobalNonNativeFn", &ExampleClass::GlobalNonNativeFn);
};


/*
* proto native void GlobalFnTest(string someData);
*/
void ExampleClass::GlobalFnTest(char* someData){
    ....
}

/*
* proto void GlobalNonNativeFn(string someData);
*/
void ExampleClass::GlobalNonNativeFn(FunctionContext* args, FunctionResult* result){
    ...
}

/*
* static proto string TestStaticFunction(string someStr);
*/
void ExampleClass::TestStaticFunction(FunctionContext* args, FunctionResult* result){
   ...
}

/*
* static proto native string TestStaticNativeFunction(string someData);
*/
char* ExampleClass::TestStaticNativeFunction(char* data){
    ...
}

/*
* proto native void DynamicProtoNativeMethod(PlayerIdentity pid);
* 
* Dynamic proto methods will always include selfPtr as the first arg
* eg; if ExampleClass has a proto native/non-native DynamicProtoNativeMethod(arg), it's first arg will be a ptr to ExampleClass instance followed by regular args.
*/
void ExampleClass::DynamicProtoNativeMethod(ManagedScriptInstance* selfPtr, PlayerIdentity* playerIdentity){
   ...
}


/*
* proto void DynamicProtoMethod();
* 
* Dynamic proto methods will always include selfPtr as the first arg
* eg; if ExampleClass has a proto native/non-native DynamicProtoMethod(arg), it's first arg will be a ptr to ExampleClass instance followed by regular args.
* if the method is "non-native" -> first arg points to self instance, 2nd arg is FunctionContext, 3rd arg is FunctionResult
*/
void ExampleClass::DynamicProtoMethod(ManagedScriptInstance* selfPtr, FunctionContext* args, FunctionResult* result){
    ...
}
```

Example of Enforce script implementation that reflects our proto C++ plugin 
```c++
proto native void GlobalFnTest(string someData);
proto string GlobalNonNativeFn(string someData);

class ExampleClass
{
	static proto string TestStaticFunction(string someStr, array<string> someArr);
	static proto native string TestStaticNativeFunction(string someData);
	
	proto void DynamicProtoMethod(PlayerIdentity pid);
	proto native void DynamicProtoNativeMethod(PlayerIdentity pid);

	string JtMethod(string myMsg)
	{
		Print("Ez pz DZ has been spanked - > " + myMsg);
		return "here's some return for you from Enforce!";
	}
}
```

### Examples

Included in this repository is an example plugin project. Comes ready with examples showcasing the feature list the InfinityDayZ Framework crafts.

## Installation

Infinity is a *server-only* mod. This means you can only add plugins, and therefore implement `proto` methods, on the server.

### Host Install

The following steps must be complete prior to installing Infinity

1. Install your Enfusion Server 
2. Download the latest runtime RC. 
2. Create a `Plugins` folder in your *server root* directory.
3. Extract the runtime `InfinityHost.dll` & place it within the `Plugins` directory along with all your other plugins e.g; `ExamplePlugin.dll`.
4. Extract and place `hid.dll` in the *server root*.
5. Place the DayZ mod `@ExamplePlugin` within *server root* (this mod includes our Enforce declaration of our C++ side implementation)
6. Edit your server mods parameter to include `-serverMod=@ExamplePlugin`

### Start-up Parameters
- Add `-debugprint` to enable output of debug logging
- Add `-noconsole` to disable the console window the host framework creates. (Logging will be silent to file within `Plugins\plugins_xxx.log`)
- Add `-diag` to enable support for DayZ Diagnostics executable builds.

## Future Plans & Features
- [ ] Plugins can register custom path keys. Ex: `$customkey:test.c` could map to `C:\test.c`
- [ ] Custom path keys can be written to (if enabled)
- [ ] Plugins can get the profile path
- [ ] Add `-globalwrite` launch paramter to enable write to all file paths
- [ ] Plugins can enable write to custom path keys post-creation
- [ ] .NET plugins
And more will be added!

## Final Notes & License

This code was originally based on: https://github.com/EnfusionModders/InfinityC
Minimal amount of code from InfinityC remains in the current version of the project.

Infinity is licensed under the MIT license. You can find the full license in the LICENSE file.

