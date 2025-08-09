#include <chrono>
#include <thread>
#include <Windows.h>
#include <io.h>
#include <fcntl.h>

#include <chrono>
#include <ctime>
#include <string>
#include <filesystem>

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
	if (!IsConsoleDisabled())
	{
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
	}
	else
	{
		// 1) find the Plugins folder next to the host EXE
		char exePath[MAX_PATH];
		GetModuleFileNameA(NULL, exePath, MAX_PATH);
		std::string dir(exePath);
		auto pos = dir.find_last_of("\\/");
		if (pos != std::string::npos)
			dir.resize(pos);
		std::filesystem::path pluginsDir = std::filesystem::path(dir) / "Plugins";
		std::filesystem::create_directories(pluginsDir);

		// 2) build timestamped filename
		using clock = std::chrono::system_clock;
		auto now = clock::now();
		std::time_t t = clock::to_time_t(now);
		struct tm tm;
		localtime_s(&tm, &t);
		char timestamp[32];
		strftime(timestamp, sizeof(timestamp),
			"plugins_%Y-%m-%d_%H-%M-%S.log", &tm);

		std::filesystem::path logFile = pluginsDir / timestamp;
		auto path = logFile.string();

		// 3) Redirect stdout → logFile
		FILE* fOut = freopen(path.c_str(), "w", stdout);
		if (fOut)
			setvbuf(stdout, nullptr, _IONBF, 0);
		else
			freopen("NUL", "w", stdout);

		// 4) Redirect stderr → same logFile
		FILE* fErr = freopen(path.c_str(), "w", stderr);
		if (fErr)
			setvbuf(stderr, nullptr, _IONBF, 0);
		else
			freopen("NUL", "w", stderr);
	}
	

	bool diag = IsDiagBuild();
	WORD attr = diag ? COL_DIAG : COL_RETAIL;
	PrintlnColored("INITIALIZING INFINITY :: Mode -> [%s]", attr, diag ? "DayZ DIAG" : "DayZ RETAIL");


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
			if (elapsed >= 30)
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
