#include "Debug.h"
#include <cstdarg>
#include <stdio.h>
#include <ASMMacros.h>
#include <Windows.h>
#include <iostream>
char Debug::LogStringBuffer[0x1000];
int Debug::CurrentBufferSize = 0;


std::string Debug::WStringToGBK(const wchar_t* wstr)
{
    if (!wstr) return {};

    int size = WideCharToMultiByte(
        CP_ACP, 0,          // 👈 关键：系统ANSI（中文系统就是GBK）
        wstr, -1,
        nullptr, 0,
        nullptr, nullptr
    );

    if (size <= 0) return {};

    std::string result(size - 1, 0); // 去掉结尾 \0

    WideCharToMultiByte(
        CP_ACP, 0,
        wstr, -1,
        result.data(), size,
        nullptr, nullptr
    );

    return result;
}
std::string Debug::WStringToUTF8(const wchar_t* wstr)
{
    if (!wstr) return {};

    int size = WideCharToMultiByte(
        CP_UTF8, 0,
        wstr, -1,            // -1 表示自动到 '\0'
        nullptr, 0,
        nullptr, nullptr
    );

    if (size <= 0) return {};

    std::string result(size - 1, 0); // 去掉结尾 '\0'

    WideCharToMultiByte(
        CP_UTF8, 0,
        wstr, -1,
        result.data(), size,
        nullptr, nullptr
    );

    return result;
}
void Debug::LogW(const wchar_t* pFormat, ...)
{
    wchar_t wbuffer[1024];

    va_list args;
    va_start(args, pFormat);

    vswprintf(
        wbuffer,
        sizeof(wbuffer) / sizeof(wchar_t),
        pFormat,
        args
    );

    va_end(args);

    auto str = WStringToUTF8(wbuffer);

    // 👉 复用原 Log
    LogGame("%s %s", "[Mochi]", str.c_str());
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

