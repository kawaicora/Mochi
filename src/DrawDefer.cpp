//=============================================================================
//  DrawDefer.cpp — CC_Draw_Shape 调用计数
//  ---------------------------------------------------------------------------
//  重要：0x4AED70 上已经有一个 DEFINE_HOOK（RadarHook.cpp 的 CC_Draw_ShapeHook，
//  size=6）。Syringe 同一地址不能挂两个钩子，重复挂钩会互相覆盖 -> 跳飞。
//  因此这里不声明钩子，只提供计数函数，由那个已有钩子回调。
//
//  另注：Syringe 的 return 0 语义是"重新执行保存的原字节后继续"，即正常直通。
//  所以 handler 里**绝对不能**再手动补 sub esp,94h —— 会造成栈帧错位 0x94 字节，
//  函数内所有 [esp+XX] 全部错位，这正是之前两次崩溃的原因。
//=============================================================================

#include "DrawDefer.h"

#include <YRPP.h>
#include <Debug.h>

#include <intrin.h>
#include <stdint.h>

namespace {

volatile LONG	g_totalCalls = 0;
volatile LONG	g_windowCalls = 0;
}

void DrawDefer::CountCall()
{
	InterlockedIncrement(&g_totalCalls);
	InterlockedIncrement(&g_windowCalls);
}

//=============================================================================
bool DrawDefer::Install()
{
	Debug::Log("[DrawDefer] counting via existing 0x4AED70 hook (no extra hook)\n");
	return true;
}

bool DrawDefer::IsInstalled()
{
	return true;
}

uint64_t DrawDefer::TakeCallCount()
{
	return (uint64_t)InterlockedExchange(&g_windowCalls, 0);
}

uint64_t DrawDefer::GetTotalCalls()
{
	return (uint64_t)g_totalCalls;
}
// 单位精灵真正干活的函数（FootClass::Draw_A_SHP @ 0x41C090 转发到这里）。
// 已确认：0x705E00 没有被 Phobos/Ares/Mochi 任何 DLL 占用。
// 实测序言：
//   8B 44 24 04   mov eax,[esp+4]    4 字节
//   83 EC 44      sub esp,44h         3 字节   合计 7 字节（两条完整指令）
// size=7 + return 0，由 Syringe 重放这 7 字节后继续。
// 纪律：不碰 ESP、不动栈参数布局，只读不写。
DEFINE_HOOK(0x705E00, TechnoDraw_Entry, 7)
{
	DrawDefer::CountCall();
	return 0;
}
