//=============================================================================
//  DrawDefer.cpp
//  ---------------------------------------------------------------------------
//  BSurface::CC_Draw_Shape @ 0x4AED70 接管。
//
//  阶段 1：钩子计数后**立即转发**到 trampoline（语义等价于原函数）。
//          画面必须与未挂钩时完全一致 —— 这是验证 trampoline 正确性的判据。
//
//  trampoline 构造（已从 gamemd.exe 实测序言）：
//      0x4AED70: 81 EC 94 00 00 00   sub esp, 94h      <- 6 字节，单条指令
//      trampoline = [6 字节原始指令][E9 rel32 -> 0x4AED76]
//  因为第一条指令已完整覆盖 5 字节补丁空间，无需指令长度解码器。
//=============================================================================

#include "DrawDefer.h"

#include <YRPP.h>
#include <Debug.h>

#include <intrin.h>
#include <stdint.h>
#include <string.h>

namespace {

constexpr uintptr_t kCCDrawShape = 0x4AED70;
constexpr int       kCopyBytes   = 6;			// sub esp,94h 整条指令
constexpr uintptr_t kResumeAt    = kCCDrawShape + kCopyBytes;

// 原函数签名（IDA：__fastcall，this=ECX，a2=EDX，a3..a16 入栈）
using CCDrawShapeFn = void(__fastcall*)(
	int a1, int* a2, int a3, int a4, int* a5, int* a6,
	int a7, int a8, int a9, int a10, int a11, int a12,
	int a13, int a14, int a15, int a16);

alignas(16) uint8_t	g_tramp[64];
CCDrawShapeFn		g_orig = nullptr;

volatile LONG		g_totalCalls = 0;
volatile LONG		g_windowCalls = 0;
bool				g_installed = false;

// ---- 钩子本体：计数后原样转发 ----------------------------------------------
void __fastcall CC_Draw_Shape_Hook(
	int a1, int* a2, int a3, int a4, int* a5, int* a6,
	int a7, int a8, int a9, int a10, int a11, int a12,
	int a13, int a14, int a15, int a16)
{
	InterlockedIncrement(&g_totalCalls);
	InterlockedIncrement(&g_windowCalls);

	// 阶段 1：立即转发，行为等同原函数
	g_orig(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16);
}
}

//=============================================================================
bool DrawDefer::Install()
{
	if (g_installed)
		return true;

	// ---- 构造 trampoline ----
	DWORD oldProt = 0;
	if (!VirtualProtect(g_tramp, sizeof(g_tramp), PAGE_EXECUTE_READWRITE, &oldProt))
	{
		Debug::Log("[DrawDefer] tramp protect failed err=%lu\n", GetLastError());
		return false;
	}

	memcpy(g_tramp, (const void*)kCCDrawShape, kCopyBytes);

	// E9 rel32 -> 0x4AED76
	uint8_t* jmp = g_tramp + kCopyBytes;
	jmp[0] = 0xE9;
	const int32_t rel = (int32_t)(kResumeAt - ((uintptr_t)jmp + 5));
	memcpy(jmp + 1, &rel, 4);

	FlushInstructionCache(GetCurrentProcess(), g_tramp, sizeof(g_tramp));
	g_orig = (CCDrawShapeFn)(void*)g_tramp;

	// ---- 在 0x4AED70 写入 E9 跳转到钩子 ----
	if (!VirtualProtect((void*)kCCDrawShape, kCopyBytes, PAGE_EXECUTE_READWRITE, &oldProt))
	{
		Debug::Log("[DrawDefer] target protect failed err=%lu\n", GetLastError());
		return false;
	}

	uint8_t* p = (uint8_t*)kCCDrawShape;
	p[0] = 0xE9;
	const int32_t rel2 = (int32_t)((uintptr_t)&CC_Draw_Shape_Hook - (kCCDrawShape + 5));
	memcpy(p + 1, &rel2, 4);
	p[5] = 0x90;									// 占位，永不被执行

	VirtualProtect((void*)kCCDrawShape, kCopyBytes, oldProt, &oldProt);
	FlushInstructionCache(GetCurrentProcess(), (void*)kCCDrawShape, kCopyBytes);

	g_installed = true;
	Debug::Log("[DrawDefer] CC_Draw_Shape hooked: tramp=%p target=0x%X resume=0x%X\n",
		g_tramp, (unsigned)kCCDrawShape, (unsigned)kResumeAt);
	return true;
}

bool DrawDefer::IsInstalled()
{
	return g_installed;
}

uint64_t DrawDefer::TakeCallCount()
{
	return (uint64_t)InterlockedExchange(&g_windowCalls, 0);
}

uint64_t DrawDefer::GetTotalCalls()
{
	return (uint64_t)g_totalCalls;
}
