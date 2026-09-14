#pragma once
//=============================================================================
//  SurfaceDebug.h  —  RA2/YR Surface 内容实时查看小工具 (用 YRpp 类型)
//  ---------------------------------------------------------------------------
//  用途：给一个"游戏真实 Surface"指针(如 DSurface::Sidebar / 0x887300)建独立
//        窗口实时显示其内容, 方便肉眼核对某帧 SHP/绘制落点是否正确。
//
//  实现原则：直接使用 YRpp 类型与方法(Surface*、GetWidth/GetHeight/GetPitch/
//        Lock/Unlock/GetRect 等 virtual)。目标对象是游戏真实例, 其 vtable 在运行
//        时指向真实现, 因此这些 virtual 会分派到真实代码取得真值 —— 即便头文件里
//        R0={return 0;} 只是占位体(对 virtual, 那个体在 dispatch 下不起作用)。
//
//  重要：本 .h/.cpp 需在*已能 include 到你实际 YRpp(并 define 好游戏地址宏)*的
//         翻译单元中编译(即宿主插件工程)。文件本身仍只放工具人 workspace, 由宿主
//         整体复制挂进它的一个 .cpp。不要单独放进不具 YRpp 的工程。
//
//  稳健性约定：所有对游戏对象/DDraw 的用法一律 try/catch; 任一异常或取值失败只经
//       SetLogger 注入的日志(默认 OutputDebugString)记一行, 绝不抛出/崩溃,
//       窗口显示灰/上次有效帧。
//
//  API:
//     HWND  CreateWindow(Surface* s, int w=0, int h=0)   建与 s 同尺寸独立窗(自动刷)
//     void  RenderWindow(HWND h)                         手动刷新(线程也安全调用)
//     void  DestroyWindow(HWND h)                        停线程并销毁
//     void  SetLogger(LogFn)                             注入日志(如 Debug::Log)
//=============================================================================
#include <Windows.h>
#include <YRPP.h>      // 宿主 YRpp 伞头,yield Surface/DSurface/GameClasses
class SurfaceDebug {
public:
	// 核心(全局函数,不用 namespace;调用处请直接调名前缀)
	static HWND CreateSurfaceWindow(class Surface* s, const wchar_t* title, bool IsStartRenderThread = false);
	static void RenderSurfaceWindow(HWND hWnd);
	static void DestroySurfaceWindow(HWND hWnd);
};

