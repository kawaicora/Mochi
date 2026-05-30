#pragma once
#define WAIT_DEBUGGER_ATTACH

typedef void* HANDLE;
class Mochi {
public:
	static HANDLE hInstance;
	static void RegisterEvent();
	
private:
	static bool isRegistered;
};