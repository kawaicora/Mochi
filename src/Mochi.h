#pragma once
#include <string>
#include <MochiINIReader.h>
#define WAIT_DEBUGGER_ATTACH

typedef void* HANDLE;
class Mochi {
public:
	static HANDLE hInstance;
	static void RegisterEvent();
	static MochiINIReader INIReader;
	static MochiINIReader::Section* PlayerBaseConfig;
private:
	static bool isRegistered;
};
