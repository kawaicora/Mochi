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
//
// The push/pop sites must be a *strict pair*: one Register on entry, one Flush
// on the single exit that every return path funnels through (0x47430F).
// Hooking 0x474304 instead loses the Flush on the two "skip" paths
// (0x474269 jmp 0x474306 / 0x474302 jne 0x474306), which desyncs the depth and
// eventually hands ProcessIncludes a stale INIClass -> wild pointer crash.
//--------------------------------------------------------------------------------
static INIClass* s_pendingINI[INIHook::MaxDepth] = {};
static bool      s_pendingValid[INIHook::MaxDepth] = {};
static int       s_depth = 0;

// True while ProcessIncludes runs. Its recursive ReadCCFile must not clear the
// include cache, or a cyclic include would loop forever.
static bool      s_processingIncludes = false;

void INIHook::Register(INIClass* pINI)
{
	// A new root load starts -> forget the names collected by the previous one.
	if (s_depth == 0 && !s_processingIncludes)
	{
		MochiINI::ResetIncludeCache();
	}

	if (s_depth < MaxDepth)
	{
		s_pendingINI[s_depth] = pINI;
		s_pendingValid[s_depth] = true;
	}

	++s_depth;
}

void INIHook::Flush()
{
	if (s_depth <= 0)
	{
		// More Flush than Register: never touch the table in that state.
		return;
	}

	--s_depth;

	INIClass* pINI = nullptr;
	bool valid = false;

	if (s_depth < MaxDepth)
	{
		valid = s_pendingValid[s_depth];
		pINI = s_pendingINI[s_depth];
		s_pendingINI[s_depth] = nullptr;
		s_pendingValid[s_depth] = false;
	}

	if (valid && pINI)
	{
		s_processingIncludes = true;
		MochiINI::ProcessIncludes(pINI);
		s_processingIncludes = false;
	}

	// Only a real root load ends the include session. While ProcessIncludes is
	// on the stack, s_depth also returns to 0 on every nested ReadCCFile return;
	// clearing the cache there wipes the dedupe list mid-recursion, so a cyclic
	// [#include] (or an include that re-adds keys) re-enters forever and blows
	// the 1 MB stack (C00000FD).
	if (s_depth == 0 && !s_processingIncludes)
	{
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
// Saved bytes: sub esp,0xA8 (6) / mov eax,[esp+0xAC] (7) = exactly 0xD, whole
// instructions, so returning 0 replays both and resumes at 0x4741FD.
// 0xA would cut the 7-byte mov in half and the replayed copy would run garbage.
//--------------------------------------------------------------------------------
DEFINE_HOOK(0x4741F0, Mochi_INIHook_ReadCCFile_Entry, 0xD)
	{
	GET(INIClass*, pINI, ECX);

	INIHook::Register(pINI);

	return 0;
	}

//--------------------------------------------------------------------------------
// CCINIClass::ReadCCFile tail, the single point every return path reaches:
//
//   0x474237 je  0x474304      \
//   0x474244 je  0x474304       > 0x474304 -> 0x474306 -> 0x47430A -> 0x47430F
//   0x474269 jmp 0x474306      /
//   0x47426E jle 0x474304     /
//   0x474302 jne 0x474306    /
//
// Saved bytes: mov eax,esi / pop esi / pop ebp / pop ebx  (exactly 5 bytes,
// whole instructions, no relative branch), so returning 0 replays the epilogue
// and resumes at 0x474314 (Ares' hook).
//
// Address choice: Ares owns 0x474314, Phobos owns 0x474230, the old tail
// 0x474304 is skipped by two paths - 0x47430F..0x474313 is free.
//--------------------------------------------------------------------------------
DEFINE_HOOK(0x47430F, Mochi_INIHook_ReadCCFile_Tail, 0x5)
{
	INIHook::Flush();

	return 0;
}
