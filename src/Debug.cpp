#include "Debug.h"
#include <cstdarg>
#include <stdio.h>
#include <ASMMacros.h>
#include <Windows.h>
#include <iostream>
char Debug::LogStringBuffer[0x1000];
int Debug::CurrentBufferSize = 0;

static bool IsInitedConsole = false;

void Debug::InitConsole()
{
	AllocConsole();
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

	std::cout << "[Mochi] Console initialized" << std::endl;
}
void Debug::Log(const char* pFormat, ...)
{
	va_list args;
	va_start(args, pFormat);
	vsprintf_s(Debug::LogStringBuffer, pFormat, args);
	std::cout << Debug::LogStringBuffer << std::endl;
	LogGame("%s %s", "[Mochi]", Debug::LogStringBuffer);
	va_end(args);
}

void Debug::LogGame(const char* pFormat, ...)
{
	JMP_STD(0x4068E0);
}