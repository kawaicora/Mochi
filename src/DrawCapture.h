#pragma once
//=============================================================================
//  DrawCapture.h  —  逻辑只管逻辑，渲染只管渲染
//  ---------------------------------------------------------------------------
//  【关键设计：像素级一致】
//    不去重写任何绘制！只把 Surface::Lock 的目标换成我们自己的内存缓冲，
//    原版 CC_Draw_Shape / blitter / 所有绘制代码**一字不改**地画进这块内存。
//    => 画面与原本像素级一致，因为跑的就是原本的代码。
//
//    因此不需要 detour CC_Draw_Shape，也不需要重新实现任何 blitter。
//
//  砍掉的东西：
//    * Surface::Lock   → 返回常驻 scratch，不再锁 DDraw surface
//    * Surface::Unlock → 空操作，不再提交
//    * 窗口            → 全部隐藏
//
//  【性能契约：逻辑 100% 全速，绝不被绘制干扰】
//    热路径只剩：指针加法 + 返回。无锁、无分配、无系统调用、无日志。
//    发布只在每帧一次（可选，供外部 render 读取），生产端从不读消费端。
//=============================================================================
#include <Windows.h>
#include <cstdint>

// ---- 与外部 render 进程的共享内存契约 --------------------------------------
namespace DrawCaptureProto
{
	inline constexpr uint32_t kMagic = 0x4D434452u;		// 'MCDR'
	inline constexpr uint32_t kVersion = 2;
	inline constexpr wchar_t  kMapName[] = L"Local\\MochiDrawCapture_v2";

	enum SurfaceId : uint16_t
	{
		kSurfPrimary = 0,
		kSurfComposite = 1,
		kSurfHidden = 2,
		kSurfSidebar = 3,
		kSurfAlternate = 4,
		kSurfTemp = 5,
		kSurfTile = 6,
		kSurfUnknown = 0xFFFF,
	};

#pragma pack(push, 4)
	// 帧描述：告诉消费端「像素在哪、什么格式」
	struct FrameInfo
	{
		uint32_t	Magic;
		uint32_t	Version;
		uint32_t	Epoch;			// 每帧自增
		uint32_t	FrameId;
		int32_t		Width;
		int32_t		Height;
		int32_t		Pitch;			// 字节/行
		int32_t		Bpp;			// 1=8bpp调色板 2=RGB565 4=BGRA
		uint32_t	Opaque;			// 1=画面已可用
		uint32_t	Pad;
		uint64_t	FramesPresented;
	};
#pragma pack(pop)
}

// ---- 采集器 ----------------------------------------------------------------
class DrawCapture
{
public:
	// 安装：重定向 DSurface 的 Lock/Unlock/IsLocked/CanLock + 隐藏窗口
	// 必须在 DSurface 实例已创建之后调用（ScenarioStart 或之后）
	static bool Install();
	static bool IsInstalled();

	// 每帧调用一次：递增 Epoch（供外部消费端判断新旧）。空操作安全。
	static void Publish();

	// 取当前画面缓冲（预览窗/外部 render 用）。
	// 返回 false 表示还没准备好。pixels 指向稳定的常驻内存，只读。
	static bool GetFrame(void** pixels, int* width, int* height, int* pitch, int* bpp);

	// 从程序里查询「当前有哪些 surface、各自尺寸/像素/锁定次数」
	// 用于让使用者指认某块面装的是哪个界面（加载/设置/计分板/小地图）
	struct SurfaceInfo
	{
		const char*	Name;
		int			Width;
		int			Height;
		int			Pitch;
		int			Bpp;
		void*		Pixels;
		uint64_t	LockCount;
		uint32_t	NonZero;
	};
	static int  GetSurfaceCount();
	static bool GetSurfaceInfo(int index, SurfaceInfo& out);

	// —— 帧计时（先量化再优化）——
	// 一帧：LogicClassUpdate(帧开始) -> LogicClassUpdateLate(逻辑结束) -> DrawOnTop(绘制结束)
	static void TimingMarkFrameStart();
	static void TimingMarkLogicEnd();
	static void TimingMarkDrawEnd();

	static void HideGameWindows();

	struct Stats
	{
		uint64_t Locks;
		uint64_t FramesPublished;
		uint64_t UnlocksSkipped;
	};
	static Stats GetStats();
};
