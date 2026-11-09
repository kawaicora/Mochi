#include "INIHook.h"
#include "MochiINI.h"

#include <Helpers/Macro.h>

//--------------------------------------------------------------------------------
// State
//
// Recursion is real here: an [#include]d file is loaded by calling
// CCINIClass::ReadCCFile recursively, which re-enters both hooks. A small
// stack keeps one INIClass per nesting level. The game loads INI files on a
// single thread, so no locking is needed.
//--------------------------------------------------------------------------------
static INIClass* s_pendingINI[INIHook::MaxDepth] = {};
static int s_depth = 0;

void INIHook::Register(INIClass* pINI)
{
	// A new root load starts -> forget the names collected by the previous one.
	if (s_depth <= 0)
	{
		s_depth = 0;
		MochiINI::ResetIncludeCache();
	}

	if (s_depth < MaxDepth)
	{
		s_pendingINI[s_depth] = pINI;
	}

	++s_depth;
}

void INIHook::Flush()
{
	if (s_depth > 0)
	{
		--s_depth;
	}

	if (s_depth < MaxDepth)
	{
		INIClass* pINI = s_pendingINI[s_depth];
		s_pendingINI[s_depth] = nullptr;

		if (pINI)
		{
			MochiINI::ProcessIncludes(pINI);
		}
	}

	if (s_depth <= 0)
	{
		s_depth = 0;
		MochiINI::ResetIncludeCache();
	}
}

//--------------------------------------------------------------------------------
// CCINIClass::ReadCCFile entry
//
//   ECX = CCINIClass* (this)
//   [esp+0x4] = FileClass*
//
// Only ECX is needed; the file name is read back from the INIClass content.
// Saved bytes: sub esp,0xA8 / mov eax,[esp+0xAC]  (exactly 0xA, whole
// instructions), so returning 0 resumes cleanly at 0x4741FA.
//--------------------------------------------------------------------------------
DEFINE_HOOK(0x4741F0, Mochi_INIHook_ReadCCFile_Entry, 0xA)
{
	GET(INIClass*, pINI, ECX);

	INIHook::Register(pINI);

	return 0;
}

//--------------------------------------------------------------------------------
// CCINIClass::ReadCCFile tail, hit on both the "file parsed" and "file missing"
// paths, right before the frame is torn down (0x474306) and returned (0x47431A).
//
// Saved bytes: mov esi,ebp / lea ecx,[esp+0xC]  (exactly 0x6, whole
// instructions, no relative branch), so returning 0 resumes at 0x47430A.
//
// This address is chosen because Ares owns 0x474314 and Phobos owns 0x474230.
//--------------------------------------------------------------------------------
DEFINE_HOOK(0x474304, Mochi_INIHook_ReadCCFile_Includes, 0x6)
{
	INIHook::Flush();

	return 0;
}
