#pragma once
//=============================================================================
//  DrawDefer.h  —  绘制入口接管（异步渲染的地基）
//  ---------------------------------------------------------------------------
//  目标函数：BSurface::CC_Draw_Shape @ 0x4AED70（IDA 反编译已确认 ABI）
//      this = ECX, a2 = EDX, a3..a16 走栈
//
//  该函数序言第一条指令是 81 EC 94 00 00 00 (sub esp,94h)，**6 字节单条指令**，
//  因此 trampoline 只需定长复制 6 字节再跳回 0x4AED76，不需要指令长度解码。
//
//  阶段 1（当前）：钩子立即转发到 trampoline，只做计数。
//    验证标准：画面必须与未挂钩时完全一致 => 证明 trampoline 与 ABI 正确。
//  阶段 2（后续）：入队延迟，由渲染线程重放。
//=============================================================================
#include <Windows.h>
#include <cstdint>

class DrawDefer
{
public:
	// 安装 trampoline 并在 0x4AED70 写入跳转。失败返回 false（不改动游戏）。
	static bool Install();
	static bool IsInstalled();

	// 由 RadarHook 里已有的 0x4AED70 钩子调用（不可重复挂钩同一地址）
	static void CountCall();

	// 上一次统计窗口内的 CC_Draw_Shape 调用次数（每帧重算）
	static uint64_t  TakeCallCount();
	static uint64_t  GetTotalCalls();
};
