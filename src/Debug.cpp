#include "Debug.h"
#include <cstdarg>
#include <stdio.h>
#include <ASMMacros.h>

char Debug::LogStringBuffer[0x1000];
int Debug::CurrentBufferSize = 0;


void Debug::Log(const char* pFormat, ...)
{
	va_list args;
	va_start(args, pFormat);
	vsprintf_s(Debug::LogStringBuffer, pFormat, args);
	LogGame("%s %s", "[Mochi]", Debug::LogStringBuffer);
	va_end(args);
}

void Debug::LogGame(const char* pFormat, ...)
{
	JMP_STD(0x4068E0);
}