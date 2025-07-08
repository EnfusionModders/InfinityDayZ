#include <chrono>
#include <thread>
#include <Windows.h>
#include <io.h>
#include <fcntl.h>

#include "ScriptRegistrator.h"
#include "ScriptEngine.h"
#include "InfinityPlugin.h"
#include "Console.hpp"

#include "Entry.h"

GInfinity globalobj; // hold infinity in memory indefinitely

// this is the main entrypoint for our library.
// it runs on it's own thread
void start(GInfinity* g_pInfinity)
{
	// if no console, create one
	if (!GetConsoleWindow())
	{
		if (AllocConsole())
		{
			HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
			int hCrt = _open_osfhandle((long)handle_out, _O_TEXT);
			FILE* hf_out = _fdopen(hCrt, "w");
			setvbuf(hf_out, NULL, _IONBF, 1);
			*stdout = *hf_out;

			HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
			hCrt = _open_osfhandle((long)handle_in, _O_TEXT);
			FILE* hf_in = _fdopen(hCrt, "r");
			setvbuf(hf_in, NULL, _IONBF, 128);
			*stdin = *hf_in;

			freopen("CONOUT$", "w", stdout);
			freopen("CONOUT$", "w", stderr);
		}
	}

	bool diag = IsDiagBuild();
	WORD attr = diag ? COL_DIAG : COL_RETAIL;
	PrintlnColored("Initializing infinity...Mode %s", attr, diag ? "DayZ DIAG" : "DayZ RETAIL");


	g_pInfinity->Init(); // finds patterns

	// wait until the global registrator head is inserted & Core modules exist
	auto start = std::chrono::steady_clock::now();
	bool found = false;
	while ((!f_EngineRegisterClass && !g_pEnforceScriptContext) || !found)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));

		do
		{
			if (f_EngineRegisterClass && g_pEnforceScriptContext)
			{
				Debugln("Game modules found!");
				found = true;
				break;
			}
		} while ((!f_EngineRegisterClass && !g_pEnforceScriptContext));

		if (!found)
		{
			// check if timeout
			auto end = std::chrono::steady_clock::now();
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
			if (elapsed >= 5)
			{
				Errorln("Registration timeout. Library out of date?");
				return; 
			}
		}
	}

	// game modules are loading, time to load our stuff
	g_pInfinity->LoadPlugins();
}

// --------------------------------------------------------------------------

GInfinity::GInfinity() : startup(start, this) {}
GInfinity::~GInfinity()
{
	startup.join();
}
void GInfinity::Init()
{
	// find all of our offsets
	if (!InitScriptEngine())
	{
		Errorln("Failed to init script engine.");
		return;
	}
	
	Debugln("GInfinity init complete.");
}
void GInfinity::LoadPlugins()
{
	// load all plugin libraries and call "OnPluginLoad"
	Infinity::LoadPlugins();
}
