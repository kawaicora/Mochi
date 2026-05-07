#pragma once
#include <Windows.h>
#include <string>

class Mochi {

public:
	static HANDLE hInstance;
	static void RegisterEvent();
private:
	static bool isRegistered;
};
