#include "Debug.h"

#include <stringapiset.h>
#include <ASMMacros.h>
#include <iostream>
char Debug::LogStringBuffer[4096];
wchar_t Debug::WLogStringBuffer[4096];
int Debug::CurrentBufferSize = 0;

std::string Debug::GBKToUTF8(const std::string& gbk)
{
    if (gbk.empty())
        return {};

    // 1) GBK(936) -> UTF-16
    int wlen = MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.size(), nullptr, 0);
    if (wlen <= 0)
        return gbk;

    std::wstring wide(wlen, L'\0');
    MultiByteToWideChar(936, 0, gbk.c_str(), (int)gbk.size(), &wide[0], wlen);

    // 2) UTF-16 -> UTF-8
    int u8len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wlen, nullptr, 0, nullptr, nullptr);
    if (u8len <= 0)
        return gbk;

    std::string utf8(u8len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wlen, &utf8[0], u8len, nullptr, nullptr);

    return utf8;
}


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

bool Debug::CreateConsoleAndSetUTF8()
{
    // 检查当前进程是否已有控制台
    HWND hConWnd = GetConsoleWindow();
    const wchar_t title[15] = L"红色警戒2 Mochi 日志";
    if (hConWnd != nullptr)
    {
        // 已有控制台，直接设置编码
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleTitleW(title);
        return true;
    }

    // 进程没有控制台 → 新建控制台窗口
    if (!AllocConsole())
        return false;
    
    // 重定向C++标准流到新建控制台
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);
    std::cout.clear();

    // 设置UTF8 + VT
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(title);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode{};
    GetConsoleMode(hOut, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, mode);
    std::cout.flush();
    return true;
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
    std::string log = WStringToGBK(Debug::WLogStringBuffer);
    log = GBKToUTF8(log);
    LogGame("%s %s", "[Mochi]", log.c_str());
}

void Debug::Log(const char* pFormat, ...)
{
	va_list args;
	va_start(args, pFormat);
	vsprintf_s(Debug::LogStringBuffer, pFormat, args);
    std::string log = GBKToUTF8(std::string(Debug::LogStringBuffer));
	LogGame("%s %s", "[Mochi]", log.c_str());
	va_end(args);
}
void Debug::LogGame(const char* pFormat, ...)
{
	JMP_STD(0x4068E0);
}

