#pragma once
//=============================================================================
//  DrawCapture.h  —  逻辑只管逻辑，渲染只管渲染
//  ---------------------------------------------------------------------------
//  目标：把 gamemd 变成一个"无头模拟进程"。
//    1. Surface::Draw 系列原语全部拦下来：只记录参数，立即返回，不真正绘制
//    2. 砍掉 Surface 的提交渲染（CopyFrom / Blit 落不到 DDraw surface）
//    3. 砍掉 Surface::Lock / Unlock 的 DDraw 副作用（返回常驻 scratch，Unlock 空操作）
//    4. 不显示游戏窗口
//    5. 所有采集到的绘制数据发布到共享内存，交给外部 render 进程消费
//
//  实现方式：**vtable 重定向**（不硬编码任何 Surface 虚函数地址）。
//    安装时用运行期自检定位 GetWidth/GetHeight 槽位，反推整个 vtable 索引，
//    所以对 dtor 槽位是否存在、版本差异都免疫。自检失败则整体不安装并记日志。
//
//  线程模型：采集发生在游戏主线程，无锁；消费端（render）只读共享内存。
//    采用三缓冲 epoch 发布，渲染端来不及就直接跳过——永远不会卡住逻辑。
//=============================================================================
#include <Windows.h>
#include <cstdint>

// ---- 与外部 render 进程的共享内存契约 --------------------------------------
namespace DrawCaptureProto
{
	inline constexpr uint32_t kMagic = 0x4D434452u;		// 'MCDR'
	inline constexpr uint32_t kVersion = 1;
	inline constexpr wchar_t  kMapName[] = L"Local\\MochiDrawCapture_v1";
	inline constexpr wchar_t  kEventName[] = L"Local\\MochiDrawCapture_frame_v1";
	inline constexpr uint32_t kMaxCmds = 1u << 16;

	// 绘制命令类型
	enum CmdType : uint16_t
	{
		kNone = 0,
		kFillRect = 1,
		kFill = 2,
		kDrawRect = 3,
		kDrawLine = 4,
		kSetPixel = 5,
		kCopyFrom = 6,
		kGradientLine = 7,
		kEllipse = 8,
		kBlitShape = 9,		// CC_Draw_Shape（下一步接入）
		kText = 10,
	};

	// 逻辑 surface 编号（采集侧填，渲染侧据此决定画到哪一层）
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
	struct Cmd
	{
		uint16_t	Type;		// CmdType
		uint16_t	Surface;	// SurfaceId
		int32_t		X, Y;		// 目标坐标（像素）
		int32_t		W, H;		// 尺寸
		uint32_t	Color;		// COLORREF / 颜色
		uint32_t	A0;			// shape 指针 / frame index
		uint32_t	A1;			// flags / remap
		uint32_t	A2;			// 保留
	};

	struct Header
	{
		uint32_t	Magic;
		uint32_t	Version;
		uint32_t	Epoch;			// 每次 publish 自增，消费端靠它判新旧
		uint32_t	FrameId;		// 逻辑帧号
		uint32_t	CmdCount;		// 本帧有效命令数
		uint32_t	CmdCapacity;	// = kMaxCmds
		int32_t		SurfaceW;		// Primary 尺寸
		int32_t		SurfaceH;
		uint32_t	DroppedCmds;	// 溢出丢弃计数
		uint32_t	Reserved;
		uint64_t	CaptureNs;		// 本帧采集耗时（QPC）
		uint64_t	PresentMode;	// 0 = 无头（不提交）
		uint64_t	Pad[6];
	};
#pragma pack(pop)

	static_assert(sizeof(Cmd) == 36, "Cmd layout must stay stable");
	static_assert(sizeof(Header) % 8 == 0, "Header must stay 8-byte aligned");
}

// ---- 采集器 ----------------------------------------------------------------
class DrawCapture
{
public:
	// 安装：重定向所有 DSurface 的 vtable + 建共享内存 + 隐藏窗口。
	// 必须在 DSurface 实例已创建之后调用（ScenarioStart 或之后）。
	static bool Install();

	// 每逻辑帧结束时发布一次（递增 Epoch）。未安装时是空操作。
	static void Publish();

	// 隐藏本进程所有顶层窗口（不含调试控制台）。
	static void HideGameWindows();

	static bool IsInstalled();

	// 读取最新已发布帧（seqlock 校验，供预览窗/外部 render 消费）
	static bool ReadLatest(const DrawCaptureProto::Header** outHdr,
		 const DrawCaptureProto::Cmd** outCmds,
		 uint32_t* outCount);

	// 统计（调试用）
	struct Stats
	{
		uint64_t CmdsTotal;
		uint64_t CmdsDropped;
		uint64_t FramesPublished;
		uint64_t LastCaptureNs;
	};
	static Stats GetStats();
};
