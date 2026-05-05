#pragma once
#include <Windows.h>
#include <string>
#include <YRpp.h>
class Mochi {

public:
	static HANDLE hInstance;
	static void RegisterEvent();
private:
	static bool isRegistered;
};
