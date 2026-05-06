#pragma once
#include <cstdarg>
#include <string>
#include <windows.h>


class Debug {
public:
	static void Log(const char* pFormat, ...);
	static void LogW(const wchar_t* pFormat, ...);
	static void LogGame(const char* pFormat, ...);
	static std::string WStringToUTF8(const wchar_t* wstr);
	static std::string WStringToGBK(const wchar_t* wstr);
	static char LogStringBuffer[0x1000];
	static int CurrentBufferSize;
};

