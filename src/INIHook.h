#pragma once

#include <YRPP.h>

// Ares-compatible [#include] support.
//
// The engine-visible part lives in MochiINI (MochiINI::ProcessIncludes), the
// Syringe hook entry points live in INIHook.cpp.
//
// Hook addresses are deliberately offset so they do NOT overwrite the
// addresses used by other loaded DLLs:
//
//   Ares   . 0x474200  CCINIClass::ReadCCFile  (capture, size 6)
//   Ares   . 0x474314  CCINIClass::ReadCCFile  (process, size 6)
//   Phobos . 0x474230  CCINIClass::Load_Inheritance
//   Mochi  . 0x4741F0  CCINIClass::ReadCCFile  entry  (size 0xA)
//   Mochi  . 0x474304  CCINIClass::ReadCCFile  tail   (size 0x6)
//
// ReadCCFile layout (gamemd.exe):
//   0x4741F0  sub esp, 0xA8                        <- Mochi capture hook (0xA)
//   0x4741F6  mov eax, [esp+0xAC]
//   0x474200  mov esi, ecx                         <- Ares hook (6)
//   ...
//   0x474230  mov ebp, eax                         <- Phobos hook (5)
//   ...
//   0x474304  mov esi, ebp                         <- Mochi process hook (0x6)
//   0x474306  lea ecx, [esp+0x0C]
//   0x474314  add esp, 0xA8                        <- Ares hook (6)
//   0x47431A  ret 0xC

class INIHook
{
public:
	static const int MaxDepth = 16;

	// Push the INIClass currently being read by CCINIClass::ReadCCFile.
	static void Register(INIClass* pINI);

	// Pop it and apply the [#include] entries of that INIClass.
	static void Flush();
};
