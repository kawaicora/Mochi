#pragma once
#include <cstdarg>


class Debug {
public:
		

	static void Log(const char* pFormat, ...);
	static void LogGame(const char* pFormat, ...);

	static char LogStringBuffer[0x1000];
	static int CurrentBufferSize;
};

