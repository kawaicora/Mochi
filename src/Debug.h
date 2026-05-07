#pragma once
#include <cstdarg>
#include <string>
#include <windows.h>
#define LOG_ANSI
//#define LOG_GBK
//#define LOG_UTF8
class Debug {
public:
	static void Log(const char* pFormat, ...);
	static void LogGame(const char* pFormat, ...);
	static std::string WStringToUTF8(const wchar_t* wstr);
	static std::string WStringToGBK(const wchar_t* wstr);
	static std::string WStringToANSI(const wchar_t* wstr);
	static void LogW(const wchar_t* pFormat, ...);
	
	static char LogStringBuffer[4096];
	static wchar_t WLogStringBuffer[4096];
	static int CurrentBufferSize;
};

