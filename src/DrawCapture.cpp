//=============================================================================
//  DrawCapture.cpp  —  逻辑只管逻辑，渲染只管渲染
//  ---------------------------------------------------------------------------
//  【性能契约：游戏逻辑必须 100% 全速，绝不被绘制干扰】
//
//  热路径（每次 surface 绘制调用）只允许做这些事：
//      * 一次 g_writeIdx 越界判断
//      * 32 字节连续内存写入
//      * g_writeIdx++
//      * 返回成功
//  明确禁止（任何一条都会造成逻辑抖动）：
//      x malloc / new / free        — 一律 Install 期预分配
//      x mutex / critical section   — 无锁，单生产者
//      x 文件/注册表/网络 syscall     — 热路径零系统调用
//      x Debug::Log / printf        — 热路径零日志
//      x C++ 异常 / try-catch       — 无
//      x 等待消费端                 — 环形缓冲写满就丢弃，绝不阻塞
//
//  跨进程同步：seqlock 语义
//      * 生产端每帧只付出 **一次** 32 位原子发布（InterlockedExchange）
//      * 生产端从不读消费端状态，因此消费端再慢也影响不到逻辑
//      * 消费端（render.exe）读 Epoch → 取槽 → 读完再读 Epoch 校验，
//        变化则整帧丢弃（宁可渲染端看到旧帧，也不让逻辑等一纳秒）
//
//  净效果：绘制工作被整体移出本进程，逻辑线程不但不被干扰，
//          还省掉了原本的像素搬运，实际是**加速**。
//=============================================================================

#include "DrawCapture.h"

#include <YRPP.h>
#include <Helpers/Macro.h>
#include <Debug.h>

#include <intrin.h>
#include <stdint.h>

namespace {

using namespace DrawCaptureProto;

constexpr uint32_t kSlotCount = 3;	// 3 槽，配合 seqlock，消费端落后也不影响生产端

// ---- 共享内存布局（跨 32/64 位，全部定宽 POD）------------------------------
#pragma pack(push, 4)
struct Slot
{
	Header	Hdr;
	Cmd		Cmds[kMaxCmds];
};

struct Region
{
	uint32_t		Magic;
	uint32_t		Version;
	uint32_t		SlotCount;
	uint32_t		Pad0;
	volatile LONG	Epoch;		// 已发布帧号；槽位 = Epoch % kSlotCount
	uint64_t		QpcFreq;
	uint64_t		Pad1[2];
	Slot			Slots[kSlotCount];
};
#pragma pack(pop)

// ---- 模块状态 --------------------------------------------------------------
Region*		g_region = nullptr;
HANDLE		g_map = nullptr;
HANDLE		g_frameEvent = nullptr;

Cmd*		g_writeBase = nullptr;		// 当前槽的命令数组
uint32_t	g_writeIdx = 0;
uint32_t	g_dropped = 0;
uint32_t	g_frameId = 0;
uint64_t	g_qpcFreq = 1;
uint64_t	g_frameStartQpc = 0;
uint64_t	g_totalCmds = 0;
uint64_t	g_framesPub = 0;
bool		g_installed = false;
int32_t		g_surfaceW = 1024;
int32_t		g_surfaceH = 768;

// Lock 用的常驻 scratch（Install 期一次性分配，热路径绝不分配）
void*		g_scratch = nullptr;
size_t		g_scratchSize = 0;

// 逻辑 surface 编号映射
struct SurfEntry
{
	Surface*	S;
	SurfaceId	Id;
};
constexpr int kMaxSurf = 8;
SurfEntry	g_surfs[kMaxSurf];
int			g_surfCount = 0;

inline SurfaceId SurfaceIdOf(Surface* s)
{
	for (int i = 0; i < g_surfCount; ++i)
		if (g_surfs[i].S == s)
			return g_surfs[i].Id;
	return kSurfUnknown;
}

// ---- 热路径：唯一允许的写入动作 --------------------------------------------
__forceinline void Emit(uint16_t type, uint16_t surf,
	int x, int y, int w, int h,
	uint32_t color, uint32_t a0, uint32_t a1) noexcept
{
	if (!g_writeBase)					// 未安装：直接吞掉
		return;
	if (g_writeIdx >= kMaxCmds)			// 写满就丢，绝不等待
	{
		++g_dropped;
		return;
	}
	Cmd& c = g_writeBase[g_writeIdx++];
	c.Type = type;
	c.Surface = surf;
	c.X = x;  c.Y = y;  c.W = w;  c.H = h;
	c.Color = color;
	c.A0 = a0; c.A1 = a1; c.A2 = 0;
}

//=============================================================================
// vtable 重定向
//=============================================================================
// Surface 虚函数槽位（以 GetWidth 为锚点，运行期自检反推）
enum : int
{
	V_CopyFromWhole = 1,
	V_CopyFromPart,
	V_CopyFrom,
	V_FillRectEx,
	V_FillRect,
	V_Fill,
	V_FillRectTrans,
	V_DrawEllipse,
	V_SetPixel,
	V_GetPixel,
	V_DrawLineEx,
	V_DrawLine,
	V_DrawLineColor_AZ,
	V_DrawMultiplyingLine_AZ,
	V_DrawSubtractiveLine_AZ,
	V_DrawRGBMultiplyingLine_AZ,
	V_PlotLine,
	V_DrawDashedLine,
	V_DrawDashedLine_,
	V_DrawLine_,
	V_DrawRectEx,
	V_DrawRect,
	V_Lock,
	V_Unlock,
	V_CanLock,
	V_VtEntry68,
	V_IsLocked,
	V_GetBytesPerPixel,
	V_GetPitch,
	V_GetRect,
	V_GetWidth,					// ← 锚点
	V_GetHeight,				// ← 锚点
	V_IsDSurface,
	V_PutPixelClip,				// XSurface
	V_GetPixelClip,				// XSurface
	V_DrawGradientLine,			// DSurface
	V_CanBlit,					// DSurface
	V_SlotCount
};

using Fn = void(*)();
using Vtbl = Fn[V_SlotCount];

// 每个不同的原始 vtable 各建一份补丁副本
struct VtblPatch
{
	void**	Orig;
	Fn*		Patched;
	bool	Ok;
};
VtblPatch	g_patches[8];
int			g_patchCount = 0;

// ---- 自检：定位 GetWidth/GetHeight 锚点，反推 dtor 槽位是否存在 --------------
// 返回 true 表示布局可信，delta 是实际索引与头文件假设索引的差。
static bool LocateAnchor(void** vt, Surface* s, int& delta, int& vw, int& vh)
{
	if (!vt || !s)
		return false;

	const int expectW = s->Width;
	const int expectH = s->Height;
	if (expectW <= 0 || expectH <= 0)
		return false;

	// 在合理窗口内找 i 使 vt[i]() == Width 且 vt[i+1]() == Height
	for (int i = 26; i <= 36; ++i)
	{
		if (!vt[i] || !vt[i + 1])
			continue;

		int w = 0, h = 0;
		__try
		{
			w = ((int(__fastcall*)(void*))vt[i])(s);
			h = ((int(__fastcall*)(void*))vt[i + 1])(s);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			continue;
		}

		if (w == expectW && h == expectH)
		{
			delta = i - V_GetWidth;
			vw = w; vh = h;
			return true;
		}
	}
	return false;
}

//=============================================================================
// 被替换的虚函数实现（全部 __fastcall：ECX=this, EDX=占位，其余走栈 ——
// 与 __thiscall 的栈布局和清理字节数完全一致）
//=============================================================================
#define THISARG void* self, void*

// --- 锁相关：砍掉 DDraw 副作用 ---
static void* __fastcall Hook_Lock(void* self, void* /*edx*/, int /*X*/, int /*Y*/)
{
	// 返回常驻 scratch，让「Lock 后直接写像素」的老代码不崩；Unlock 不会提交。
	return g_scratch;
}

static bool __fastcall Hook_Unlock(void* /*self*/, void* /*edx*/)
{
	return true;						// 空操作：不提交、不 Blit
}

static bool __fastcall Hook_IsLocked(void* /*self*/, void* /*edx*/)
{
	return false;
}

static bool __fastcall Hook_CanLock(void* /*self*/, void* /*edx*/, DWORD, DWORD)
{
	return true;
}

// --- 填充类 ---
static bool __fastcall Hook_Fill(void* self, void* /*edx*/, COLORREF color)
{
	Emit(kFill, SurfaceIdOf((Surface*)self), 0, 0, 0, 0, (uint32_t)color, 0, 0);
	return true;
}

static bool __fastcall Hook_FillRect(void* self, void* /*edx*/, RectangleStruct* pRect, COLORREF color)
{
	if (pRect)
		Emit(kFillRect, SurfaceIdOf((Surface*)self), pRect->X, pRect->Y, pRect->Width, pRect->Height, (uint32_t)color, 0, 0);
	return true;
}

static bool __fastcall Hook_FillRectEx(void* self, void* /*edx*/, RectangleStruct* pClip, RectangleStruct* pFill, COLORREF color)
{
	if (pFill)
		Emit(kFillRect, SurfaceIdOf((Surface*)self), pFill->X, pFill->Y, pFill->Width, pFill->Height, (uint32_t)color, 0, (uint32_t)(uintptr_t)pClip);
	return true;
}

static bool __fastcall Hook_FillRectTrans(void* self, void* /*edx*/, RectangleStruct* pClip, ColorStruct* pColor, int opacity)
{
	if (pClip)
		Emit(kFillRect, SurfaceIdOf((Surface*)self), pClip->X, pClip->Y, pClip->Width, pClip->Height,
			pColor ? (uint32_t)(pColor->R | (pColor->G << 8) | (pColor->B << 16)) : 0, (uint32_t)opacity, 0);
	return true;
}

// --- 矩形 / 线 / 点 ---
static bool __fastcall Hook_DrawRect(void* self, void* /*edx*/, RectangleStruct* pRect, DWORD color)
{
	if (pRect)
		Emit(kDrawRect, SurfaceIdOf((Surface*)self), pRect->X, pRect->Y, pRect->Width, pRect->Height, color, 0, 0);
	return true;
}

static bool __fastcall Hook_DrawRectEx(void* self, void* /*edx*/, RectangleStruct* pClip, RectangleStruct* pRect, int color)
{
	if (pRect)
		Emit(kDrawRect, SurfaceIdOf((Surface*)self), pRect->X, pRect->Y, pRect->Width, pRect->Height, (uint32_t)color, 0, (uint32_t)(uintptr_t)pClip);
	return true;
}

static bool __fastcall Hook_DrawLine(void* self, void* /*edx*/, Point2D* pStart, Point2D* pEnd, COLORREF color)
{
	if (pStart && pEnd)
		Emit(kDrawLine, SurfaceIdOf((Surface*)self), pStart->X, pStart->Y, pEnd->X, pEnd->Y, (uint32_t)color, 0, 0);
	return true;
}

static bool __fastcall Hook_DrawLineEx(void* self, void* /*edx*/, RectangleStruct* pClip, Point2D* pStart, Point2D* pEnd, COLORREF color)
{
	if (pStart && pEnd)
		Emit(kDrawLine, SurfaceIdOf((Surface*)self), pStart->X, pStart->Y, pEnd->X, pEnd->Y, (uint32_t)color, 0, (uint32_t)(uintptr_t)pClip);
	return true;
}

static bool __fastcall Hook_DrawLine_(void* self, void* /*edx*/, Point2D* pStart, Point2D* pEnd, int color, bool /*bUnk*/)
{
	if (pStart && pEnd)
		Emit(kDrawLine, SurfaceIdOf((Surface*)self), pStart->X, pStart->Y, pEnd->X, pEnd->Y, (uint32_t)color, 0, 0);
	return true;
}

static bool __fastcall Hook_SetPixel(void* self, void* /*edx*/, Point2D* pPoint, COLORREF color)
{
	if (pPoint)
		Emit(kSetPixel, SurfaceIdOf((Surface*)self), pPoint->X, pPoint->Y, 1, 1, (uint32_t)color, 0, 0);
	return true;
}

static bool __fastcall Hook_DrawEllipse(void* self, void* /*edx*/, int XOff, int YOff, int cx, int cy, RectangleStruct rect, COLORREF color)
{
	Emit(kEllipse, SurfaceIdOf((Surface*)self), rect.X + XOff, rect.Y + YOff, cx, cy, (uint32_t)color, 0, 0);
	return true;
}

// --- 提交/拷贝：砍掉落盘到 DDraw surface ---
static bool __fastcall Hook_CopyFromWhole(void* self, void* /*edx*/, Surface* pSrc, bool, bool)
{
	Emit(kCopyFrom, SurfaceIdOf((Surface*)self), 0, 0, 0, 0, 0, (uint32_t)(uintptr_t)pSrc, 0);
	return true;
}

static bool __fastcall Hook_CopyFromPart(void* self, void* /*edx*/, RectangleStruct* pClip, Surface* pSrc, RectangleStruct* pSrcRect, bool, bool)
{
	Emit(kCopyFrom, SurfaceIdOf((Surface*)self),
		pClip ? pClip->X : 0, pClip ? pClip->Y : 0,
		pClip ? pClip->Width : 0, pClip ? pClip->Height : 0,
		0, (uint32_t)(uintptr_t)pSrc, (uint32_t)(uintptr_t)pSrcRect);
	return true;
}

static bool __fastcall Hook_CopyFrom(void* self, void* /*edx*/, RectangleStruct* pClip, RectangleStruct* pClip2,
	Surface* pSrc, RectangleStruct* pDest, RectangleStruct* pSrcRect, bool, bool)
{
	Emit(kCopyFrom, SurfaceIdOf((Surface*)self),
		pDest ? pDest->X : 0, pDest ? pDest->Y : 0,
		pDest ? pDest->Width : 0, pDest ? pDest->Height : 0,
		0, (uint32_t)(uintptr_t)pSrc, (uint32_t)(uintptr_t)pSrcRect);
	return true;
}

static bool __fastcall Hook_DrawGradientLine(void* self, void* /*edx*/, RectangleStruct* pRect, Point2D* pStart, Point2D* pEnd,
	ColorStruct* pC0, ColorStruct* pC1, float /*step*/, int /*color*/)
{
	if (pStart && pEnd)
		Emit(kGradientLine, SurfaceIdOf((Surface*)self), pStart->X, pStart->Y, pEnd->X, pEnd->Y,
			pC0 ? (uint32_t)(pC0->R | (pC0->G << 8) | (pC0->B << 16)) : 0,
			pC1 ? (uint32_t)(pC1->R | (pC1->G << 8) | (pC1->B << 16)) : 0, 0);
	return true;
}

//=============================================================================
// 安装
//=============================================================================
} // anonymous namespace

static int g_delta = 0;		// vtable 索引修正（dtor 槽位导致的偏移）

#define SLOT(name) \
	(tab[V_##name + g_delta] = (Fn)&Hook_##name)

static Fn* MakePatchedVtbl(void** orig)
{
	// 真实 vtable 的槽位数取决于自检得到的 delta，不能写死 V_SlotCount
	const int n = V_SlotCount + g_delta;
	if (n <= 0 || n > V_SlotCount + 8)
		return nullptr;

	Fn* tab = (Fn*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Fn) * (size_t)n);
	if (!tab)
		return nullptr;

	for (int i = 0; i < n; ++i)
		tab[i] = (Fn)orig[i];

	// 只替换与"绘制/提交/锁"相关的槽位，其余（GetPixel/GetRect/几何查询）保持原样，
	// 避免改变游戏自检、命中测试等逻辑行为。
	SLOT(CopyFromWhole);
	SLOT(CopyFromPart);
	SLOT(CopyFrom);
	SLOT(FillRectEx);
	SLOT(FillRect);
	SLOT(Fill);
	SLOT(FillRectTrans);
	SLOT(DrawEllipse);
	SLOT(SetPixel);
	SLOT(DrawLineEx);
	SLOT(DrawLine);
	SLOT(DrawLine_);
	SLOT(DrawRectEx);
	SLOT(DrawRect);
	SLOT(Lock);
	SLOT(Unlock);
	SLOT(CanLock);
	SLOT(IsLocked);
	SLOT(DrawGradientLine);

	return tab;
}

static bool PatchSurface(Surface* s, SurfaceId id)
{
	if (!s)
		return false;

	void** vt = *(void***)s;
	if (!vt)
		return false;

	// 找已有补丁
	for (int i = 0; i < g_patchCount; ++i)
	{
		if (g_patches[i].Orig == vt)
		{
			*(void***)s = (void**)g_patches[i].Patched;
			g_surfs[g_surfCount].S = s;
			g_surfs[g_surfCount].Id = id;
			++g_surfCount;
			return g_patches[i].Ok;
		}
	}

	Fn* patched = MakePatchedVtbl(vt);
	if (!patched)
		return false;

	if (g_patchCount >= (int)(sizeof(g_patches) / sizeof(g_patches[0])))
	{
		HeapFree(GetProcessHeap(), 0, patched);
		return false;
	}

	g_patches[g_patchCount].Orig = vt;
	g_patches[g_patchCount].Patched = patched;
	g_patches[g_patchCount].Ok = true;
	++g_patchCount;

	*(void***)s = (void**)patched;

	if (g_surfCount < kMaxSurf)
	{
		g_surfs[g_surfCount].S = s;
		g_surfs[g_surfCount].Id = id;
		++g_surfCount;
	}
	return true;
}

static bool CreateSharedRegion()
{
	uint64_t need = sizeof(Region);

	g_map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
		(DWORD)(need >> 32), (DWORD)(need & 0xFFFFFFFFu), kMapName);
	if (!g_map)
	{
		Debug::Log("[DrawCapture] CreateFileMapping failed err=%lu\n", GetLastError());
		return false;
	}

	g_region = (Region*)MapViewOfFile(g_map, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (!g_region)
	{
		Debug::Log("[DrawCapture] MapViewOfFile failed err=%lu\n", GetLastError());
		CloseHandle(g_map);
		g_map = nullptr;
		return false;
	}

	g_region->Magic = kMagic;
	g_region->Version = kVersion;
	g_region->SlotCount = kSlotCount;
	g_region->Epoch = 0;
	g_region->QpcFreq = g_qpcFreq;

	g_writeBase = g_region->Slots[0].Cmds;
	g_writeIdx = 0;

	g_frameEvent = CreateEventW(nullptr, FALSE, FALSE, kEventName);

	Debug::Log("[DrawCapture] shared region ready, %llu bytes, %u cmd slots x %u\n",
		(unsigned long long)need, kMaxCmds, kSlotCount);
	return true;
}

static void AllocScratch()
{
	// 常驻 scratch：给「Lock 后直接写像素」的老代码一个可写落点。
	// 上限 32MB，避免 32 位进程地址空间被吃掉。
	constexpr size_t kCap = 32u * 1024u * 1024u;

	size_t want = 0;
	int w = 0, h = 0, bpp = 4;
	__try
	{
		if (DSurface::Primary)
		{
			w = DSurface::Primary->Width;
			h = DSurface::Primary->Height;
			bpp = DSurface::Primary->GetBytesPerPixel();
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}

	if (w <= 0 || h <= 0)
	{
		w = 1920; h = 1080;
	}
	g_surfaceW = w;
	g_surfaceH = h;

	if (bpp <= 0 || bpp > 4)
		bpp = 4;

	want = (size_t)w * (size_t)h * (size_t)bpp;
	if (want > kCap)
		want = kCap;
	if (want < 4096)
		want = 4096;

	g_scratch = VirtualAlloc(nullptr, want, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	g_scratchSize = g_scratch ? want : 0;

	Debug::Log("[DrawCapture] scratch %llu bytes (surface %dx%d bpp=%d)\n",
		(unsigned long long)g_scratchSize, w, h, bpp);
}

bool DrawCapture::Install()
{
	if (g_installed)
		return true;

	LARGE_INTEGER f{};
	if (QueryPerformanceFrequency(&f) && f.QuadPart > 0)
		g_qpcFreq = (uint64_t)f.QuadPart;

	// 1) 先看 Primary 的 vtable，做锚点自检
	Surface* primary = nullptr;
	__try
	{
		primary = DSurface::Primary;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}

	if (!primary)
	{
		Debug::Log("[DrawCapture] Primary surface not ready, abort install\n");
		return false;
	}

	void** vt = *(void***)primary;
	int vw = 0, vh = 0;
	if (!LocateAnchor(vt, primary, g_delta, vw, vh))
	{
		Debug::Log("[DrawCapture] vtable anchor probe FAILED, abort install (no layout change)\n");
		return false;
	}
	Debug::Log("[DrawCapture] vtable anchor ok: GetWidth=%d GetHeight=%d slotDelta=%d\n", vw, vh, g_delta);

	// 2) 共享内存 + scratch
	if (!CreateSharedRegion())
		return false;
	AllocScratch();

	// 3) 重定向所有 DSurface
	int okCount = 0;
	__try
	{
		if (PatchSurface(DSurface::Primary, kSurfPrimary))           ++okCount;
		if (PatchSurface(DSurface::Composite, kSurfComposite))       ++okCount;
		if (PatchSurface(DSurface::Hidden, kSurfHidden))             ++okCount;
		if (PatchSurface(DSurface::Sidebar, kSurfSidebar))           ++okCount;
		if (PatchSurface(DSurface::Alternate, kSurfAlternate))       ++okCount;
		if (PatchSurface(DSurface::Temp, kSurfTemp))                 ++okCount;
		if (PatchSurface(DSurface::Tile, kSurfTile))                 ++okCount;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Debug::Log("[DrawCapture] exception while patching surfaces\n");
	}

	// 4) 无窗口
	HideGameWindows();

	g_installed = (okCount > 0);
	Debug::Log("[DrawCapture] installed=%d, %d surfaces patched, %d distinct vtables\n",
		(int)g_installed, okCount, g_patchCount);
	return g_installed;
}

//=============================================================================
// 发布（每逻辑帧一次，唯一一次原子写）
//=============================================================================
void DrawCapture::Publish()
{
	if (!g_installed || !g_region)
		return;

	Slot& cur = g_region->Slots[g_frameId % kSlotCount];
	Header& h = cur.Hdr;

	h.Magic = kMagic;
	h.Version = kVersion;
	h.FrameId = g_frameId;
	h.CmdCount = g_writeIdx;
	h.CmdCapacity = kMaxCmds;
	h.SurfaceW = g_surfaceW;
	h.SurfaceH = g_surfaceH;
	h.DroppedCmds = g_dropped;
	h.CaptureNs = 0;		// 采样耗时不在热路径里做，避免干扰

	g_totalCmds += g_writeIdx;
	++g_framesPub;

	// ---- 唯一一次发布：release 语义，之后生产端立刻切到下一槽继续跑 ----
	MemoryBarrier();
	InterlockedExchange(&g_region->Epoch, (LONG)(g_frameId + 1));

	if (g_frameEvent)
		SetEvent(g_frameEvent);		// 每帧一次 syscall，可接受；不需要时置空

	++g_frameId;
	g_writeIdx = 0;
	g_dropped = 0;
	g_writeBase = g_region->Slots[g_frameId % kSlotCount].Cmds;
}

//=============================================================================
// 隐藏窗口
//=============================================================================
namespace {
struct HideCtx
{
	DWORD	Pid;
	HWND	Console;
	int		Hidden;
};

BOOL CALLBACK EnumHideProc(HWND hwnd, LPARAM lp)
{
	HideCtx* c = (HideCtx*)lp;
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid != c->Pid)
		return TRUE;
	if (hwnd == c->Console)
		return TRUE;
	if (!IsWindowVisible(hwnd))
		return TRUE;

	ShowWindow(hwnd, SW_HIDE);
	++c->Hidden;
	return TRUE;
}
}

void DrawCapture::HideGameWindows()
{
	HideCtx c{};
	c.Pid = GetCurrentProcessId();
	c.Console = GetConsoleWindow();
	c.Hidden = 0;
	EnumWindows(EnumHideProc, (LPARAM)&c);
	Debug::Log("[DrawCapture] hid %d window(s)\n", c.Hidden);
}

bool DrawCapture::IsInstalled()
{
	return g_installed;
}

DrawCapture::Stats DrawCapture::GetStats()
{
	Stats s{};
	s.CmdsTotal = g_totalCmds;
	s.CmdsDropped = g_dropped;
	s.FramesPublished = g_framesPub;
	s.LastCaptureNs = g_region ? g_region->Slots[g_frameId % kSlotCount].Hdr.CaptureNs : 0;
	return s;
}
//=============================================================================
// 读取最新已发布帧（seqlock 校验）
//=============================================================================
bool DrawCapture::ReadLatest(const DrawCaptureProto::Header** outHdr,
	const DrawCaptureProto::Cmd** outCmds, uint32_t* outCount)
{
	if (outHdr)   *outHdr = nullptr;
	if (outCmds)  *outCmds = nullptr;
	if (outCount) *outCount = 0;
	if (!g_region)
		return false;

	for (int attempt = 0; attempt < 4; ++attempt)
	{
		const uint32_t e = (uint32_t)InterlockedCompareExchange(&g_region->Epoch, 0, 0);
		if (e == 0)
			return false;

		const uint32_t slotIdx = (e - 1) % kSlotCount;
		Slot& s = g_region->Slots[slotIdx];
		uint32_t n = s.Hdr.CmdCount;
		if (n > kMaxCmds)
			n = kMaxCmds;

		if (outHdr)   *outHdr = &s.Hdr;
		if (outCmds)  *outCmds = s.Cmds;
		if (outCount) *outCount = n;

		MemoryBarrier();
		const uint32_t e2 = (uint32_t)InterlockedCompareExchange(&g_region->Epoch, 0, 0);
		if (e2 == e)
			return true;		// 期间未被覆盖，数据可信

		// 否则重试：宁可这帧不画，也不拖慢生产端
	}
	return false;
}
