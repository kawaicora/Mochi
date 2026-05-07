#include "Debug.h"
#include <cstdarg>
#include <stdio.h>
#include <ASMMacros.h>
#include <Windows.h>
#include <iostream>
char Debug::LogStringBuffer[4096];
wchar_t Debug::WLogStringBuffer[4096];
int Debug::CurrentBufferSize = 0;


std::string Debug::WStringToANSI(const wchar_t* wstr)
{
    if (!wstr)
        return {};

    int size = WideCharToMultiByte(
        CP_ACP,
        0,
        wstr,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (size <= 0)
        return {};

    std::string result(size - 1, 0);

    WideCharToMultiByte(
        CP_ACP,
        0,
        wstr,
        -1,
        result.data(),
        size,
        nullptr,
        nullptr
    );

    return result;
}

std::string Debug::WStringToGBK(const wchar_t* wstr)
{
    if (!wstr)
        return {};

    int size = WideCharToMultiByte(
        936,
        0,
        wstr,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (size <= 0)
        return {};

    std::string result(size - 1, 0);

    WideCharToMultiByte(
        936,
        0,
        wstr,
        -1,
        result.data(),
        size,
        nullptr,
        nullptr
    );

    return result;
}
std::string Debug::WStringToUTF8(const wchar_t* wstr)
{
    if (!wstr)
        return {};

    int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (size <= 0)
        return {};

    std::string result(size - 1, 0);

    WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr,
        -1,
        result.data(),
        size,
        nullptr,
        nullptr
    );

    return result;
}
void Debug::LogW(const wchar_t* pFormat, ...)
{
    va_list args;
    va_start(args, pFormat);

    vswprintf(
        Debug::WLogStringBuffer,
        sizeof(Debug::WLogStringBuffer) / sizeof(wchar_t),
        pFormat,
        args
    );

    va_end(args);
#ifdef LOG_UTF8
    std::string log = WStringToUTF8(Debug::WLogStringBuffer);
#elif defined(LOG_GBK)
    std::string log = WStringToGBK(Debug::WLogStringBuffer);
#else
    std::string log = WStringToANSI(Debug::WLogStringBuffer);
#endif

    LogGame("%s %s", "[Mochi]", log.c_str());
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

