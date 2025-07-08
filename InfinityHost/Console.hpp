#pragma once
#include <iostream>
#include <cstdarg>
#include <Windows.h>
#include <cstdio>
#include <shellapi.h>

// bright white text on green background
static constexpr WORD COL_RETAIL =
(FOREGROUND_RED |
	FOREGROUND_GREEN |
	FOREGROUND_BLUE |
	FOREGROUND_INTENSITY) |
	BACKGROUND_GREEN;

// bright white text on red background
static constexpr WORD COL_ERROR =
(FOREGROUND_RED |
	FOREGROUND_GREEN |
	FOREGROUND_BLUE |
	FOREGROUND_INTENSITY) |
	BACKGROUND_RED;

// bright white text on yellow background
static constexpr WORD COL_WARN =
(FOREGROUND_RED |
	FOREGROUND_GREEN |
	FOREGROUND_BLUE |
	FOREGROUND_INTENSITY) |
	(BACKGROUND_RED |
		BACKGROUND_GREEN);

// bright red text on blue background
static constexpr WORD COL_DIAG =
(FOREGROUND_RED |
	FOREGROUND_INTENSITY) |
	BACKGROUND_BLUE;

inline bool IsDiagBuild()
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argv) return false;

	bool found = false;
	for (int i = 1; i < argc; ++i) {
		if (_wcsicmp(argv[i], L"-diag") == 0) {
			found = true;
			break;
		}
	}
	LocalFree(argv);
	return found;
}

inline bool IsDebug()
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argv) return false;

	bool found = false;
	for (int i = 1; i < argc; ++i) {
		if (_wcsicmp(argv[i], L"-PluginsDebug") == 0) {
			found = true;
			break;
		}
	}
	LocalFree(argv);
	return found;
}

inline int PrintlnColored(const char* fmt, WORD colorAttr, ...)
{
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hOut, &csbi);
	WORD oldAttrs = csbi.wAttributes;

	// set our combined color
	SetConsoleTextAttribute(hOut, colorAttr);

	// print the fixed prefix
	fputs("INFINITY     : ", stdout);

	// print the user message
	va_list args;
	va_start(args, colorAttr);
	int ret = vprintf(fmt, args);
	va_end(args);

	// newline
	fputc('\n', stdout);

	// restore original console attributes
	SetConsoleTextAttribute(hOut, oldAttrs);

	return ret;
}

inline int Println(const char* format, ...)
{
	printf("INFINITY     : ");
	va_list vl;
	va_start(vl, format);
	auto ret = vprintf(format, vl);
	va_end(vl);
	printf("\n");
	return ret;
}
inline int Debugln(const char* format, ...)
{
	if (!IsDebug()) return 0;

	printf("INFINITY  (D): ");
	va_list vl;
	va_start(vl, format);
	auto ret = vprintf(format, vl);
	va_end(vl);
	printf("\n");
	return ret;
}
inline int Errorln(const char* format, ...)
{
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hOut, &csbi);
	WORD oldAttrs = csbi.wAttributes;

	SetConsoleTextAttribute(hOut, COL_ERROR);

	fputs("INFINITY  (E): ", stdout);

	va_list vl;
	va_start(vl, format);
	auto ret = vprintf(format, vl);
	va_end(vl);

	fputc('\n', stdout);

	SetConsoleTextAttribute(hOut, oldAttrs);
	return ret;
}
inline int Warnln(const char* format, ...)
{
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(hOut, &csbi);
	WORD oldAttrs = csbi.wAttributes;

	SetConsoleTextAttribute(hOut, COL_WARN);

	fputs("INFINITY  (W): ", stdout);
	va_list vl;
	va_start(vl, format);
	auto ret = vprintf(format, vl);
	va_end(vl);
	fputc('\n', stdout);

	SetConsoleTextAttribute(hOut, oldAttrs);
	return ret;
}
