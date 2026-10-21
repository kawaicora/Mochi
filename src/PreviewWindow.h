#pragma once
//=============================================================================
//  PreviewWindow.h  —  采集数据的预览窗口（Phase 1 验证用）
//  ---------------------------------------------------------------------------
//  因为我们把所有 Surface 绘制都砍掉了，游戏本身不出像素。
//  这个窗口把 DrawCapture 采集到的绘制命令自己光栅化出来，
//  用来肉眼验证"采集是否完整、坐标是否正确"。
//
//  同时负责反向通道：把预览窗口收到的鼠标/键盘事件转发给 gamemd 的窗口，
//  让游戏还能被操作（先用 PostMessage 验证；若引擎走 DirectInput 轮询，
//  下一步要改成注入 InputManager 层）。
//
//  注意：它画的是**命令**，不是原始像素，所以只覆盖被采集到的原语
//        （填充/矩形/线段/点/椭圆）。真正的像素级还原在外部 render 进程。
//=============================================================================
#include <Windows.h>

class PreviewWindow
{
public:
	// 在 DrawCapture::Install() 之后调用
	static bool Create();
	static void RenderFrame();		// 每帧刷新（从共享内存取最新命令）
	static void Destroy();

	static HWND GetHwnd();

	// 设置输入转发目标（gamemd 的窗口）
	static void SetInputTarget(HWND gameHwnd);
};
