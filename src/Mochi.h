#pragma once
#include <Windows.h>
#include <string>
#include <MochiINIReader.h>
class Mochi {

public:
	static HANDLE hInstance;
	static void RegisterEvent();
	static MochiINIReader INIReader;
	static MochiINIReader::Section* PlayerBaseConfig;
private:
	static bool isRegistered;
};
