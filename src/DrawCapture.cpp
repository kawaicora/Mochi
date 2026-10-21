//=============================================================================
//  DrawCapture.cpp  —  逻辑只管逻辑，渲染只管渲染
//  ---------------------------------------------------------------------------
//  【像素级一致的做法】
//    不重写任何绘制！只把 Surface::Lock 返回的指针换成我们自己的内存，
//    原版 CC_Draw_Shape / blitter / 全部绘制代码原封不动地画进这块内存。
//    跑的是原版代码 => 画面与原本像素级一致。
//
//  【两条路径，都能让预览窗看到"和原本一样"的画面】
//    A) vtable 探针成功  -> 重定向 Lock/Unlock/IsLocked/CanLock，
//                           DDraw 不再提交，像素落在我们的常驻缓冲
//    B) vtable 探针失败  -> 不重定向（安全失败），预览窗退化为
//                           用原版 Lock/Unlock 直读真实 DDraw surface
//    两条路上预览窗看到的都是**真实渲染结果**，不是重新光栅化的近似。
//
//  【性能契约：逻辑 100% 全速】
//    热路径：一次线性查表 + 指针加法 + 返回。无锁/无分配/无系统调用/无日志。
//=============================================================================

#include "DrawCapture.h"

#include <YRPP.h>
#include <Debug.h>

#include <stdint.h>
#include <string.h>

namespace {

using namespace DrawCaptureProto;

constexpr uint32_t kPixelOffset = 4096;			// 共享内存里像素数据的起始偏移
constexpr int      kMaxSurfSlot = 8;

// ---- 状态 ------------------------------------------------------------------
struct SurfScratch
{
	Surface*	S;
	uint8_t*	Buf;
	size_t		Cap;
	int			Pitch;
	int			Bpp;
	int			W, H;
	bool		InShared;		// true = 这块缓冲在共享内存里（Primary）
};

SurfScratch	g_scr[kMaxSurfSlot];
int			g_scrN = 0;

void*		g_sharedBase = nullptr;		// 共享内存映射基址
HANDLE		g_map = nullptr;
FrameInfo*	g_info = nullptr;

uint64_t	g_locks = 0;
uint64_t	g_unlocksSkipped = 0;
uint64_t	g_frames = 0;
bool		g_installed = false;
int			g_delta = 0;				// vtable 索引修正

// ---- 热路径 ----------------------------------------------------------------
__forceinline SurfScratch* FindScratch(Surface* s) noexcept
{
	for (int i = 0; i < g_scrN; ++i)
		if (g_scr[i].S == s)
			return &g_scr[i];
	return nullptr;
}

// ---- vtable 探针（安全版）--------------------------------------------------
using Fn = void(*)();

// 关键修复：不再在 __except 里 continue，改为独立函数返回哨兵；
// 并且把 EDX 显式置 0，避免 GetRect 之类拿垃圾指针写坏内存。
static int SafeCall(Fn fn, void* self) noexcept
{
	__try
	{
		return ((int(__fastcall*)(void*, void*))fn)(self, nullptr);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return -987654321;
	}
}

static bool LocateAnchor(void** vt, Surface* s, int& delta) noexcept
{
	const int wantW = s->Width;
	const int wantH = s->Height;
	if (wantW <= 0 || wantH <= 0)
	{
		Debug::Log("[DrawCapture] bad surface dims %dx%d\n", wantW, wantH);
		return false;
	}

	Debug::Log("[DrawCapture] probing vtable, expect %dx%d\n", wantW, wantH);

	// 严格只试 GetWidth/GetHeight 可能落下的纯 getter 槽位。
	// 绝不 20..48 全扫 —— 那会调到 DrawLine_/DrawRectEx/DrawGradientLine
	// 这类要指针参数的函数，用垃圾指针写内存会把游戏直接搞崩。
	static const int kCand[] = { 31, 32, 30, 29 };
	for (int c = 0; c < 4; ++c)
	{
		const int i = kCand[c];
		const int w = SafeCall((Fn)vt[i], s);
		const int h = SafeCall((Fn)vt[i + 1], s);
		Debug::Log("[DrawCapture]   slot %2d -> %d , %d\n", i, w, h);
		if (w == wantW && h == wantH)
		{
			delta = i - 31;			// 31 = YRpp 里 GetWidth 的声明序号
			Debug::Log("[DrawCapture] anchor FOUND at slot %d (delta=%d)\n", i, delta);
			return true;
		}
	}
	return false;
}

// ---- 取 pitch/bpp（读 DDraw surface 描述，不依赖 vtable）-------------------
static void QueryFormat(Surface* s, int& pitch, int& bpp, int& w, int& h) noexcept
{
	w = s->Width;
	h = s->Height;
	pitch = 0;
	bpp = 2;

	__try
	{
		DSurface* ds = (DSurface*)s;
		DDSURFACEDESC2* desc = ds->VideoSurfaceDescription;
		if (desc)
		{
			const LONG lp = desc->lPitch;
			const DWORD bc = desc->ddpfPixelFormat.dwRGBBitCount;
			if (lp > 0)
				pitch = (int)lp;
			if (bc == 8)  bpp = 1;
			else if (bc == 16) bpp = 2;
			else if (bc == 32) bpp = 4;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}

	if (pitch <= 0)
		pitch = w * bpp;
	if (pitch <= 0)
		pitch = w * 2;
}

// ---- 挂钩实现（只替换 4 个锁相关虚函数，其余全部保留原样）------------------
static void* __fastcall Hook_Lock(void* self, void* /*edx*/, int X, int Y)
{
	++g_locks;
	SurfScratch* sc = FindScratch((Surface*)self);
	if (!sc || !sc->Buf)
		return nullptr;

	long long off = (long long)Y * sc->Pitch + (long long)X * sc->Bpp;
	if (off < 0)
		off = 0;
	if ((size_t)off >= sc->Cap)
		off = 0;					// 越界保护：回退到缓冲起点
	return sc->Buf + off;
}

static bool __fastcall Hook_Unlock(void* /*self*/, void* /*edx*/)
{
	++g_unlocksSkipped;
	return true;					// 不提交、不 Blit
}

static bool __fastcall Hook_IsLocked(void* /*self*/, void* /*edx*/)
{
	return false;
}

static void BlitScratch(SurfScratch* d, SurfScratch* s, int dx, int dy, int sx, int sy, int w, int h)
{
	if (!d || !s || !d->Buf || !s->Buf)
		return;
	if (w <= 0 || h <= 0) { w = d->W; h = d->H; }

	if (sx < 0) { dx -= sx; w += sx; sx = 0; }
	if (sy < 0) { dy -= sy; h += sy; sy = 0; }
	if (sx + w > s->W) w = s->W - sx;
	if (sy + h > s->H) h = s->H - sy;
	if (dx < 0) { sx -= dx; w += dx; dx = 0; }
	if (dy < 0) { sy -= dy; h += dy; dy = 0; }
	if (dx + w > d->W) w = d->W - dx;
	if (dy + h > d->H) h = d->H - dy;
	if (w <= 0 || h <= 0)
		return;

	const size_t bpp = (d->Bpp > 0) ? (size_t)d->Bpp : 2;
	const size_t rowBytes = (size_t)w * bpp;
	for (int y = 0; y < h; ++y)
	{
		const uint8_t* sp = s->Buf + (size_t)(sy + y) * (size_t)s->Pitch + (size_t)sx * bpp;
		uint8_t* dp = d->Buf + (size_t)(dy + y) * (size_t)d->Pitch + (size_t)dx * bpp;
		memcpy(dp, sp, rowBytes);
	}
}

static bool __fastcall Hook_CopyFromWhole(void* self, void* /*edx*/, Surface* pSrc, bool, bool)
{
	SurfScratch* d = FindScratch((Surface*)self);
	SurfScratch* s = FindScratch(pSrc);
	if (!d || !s || !d->Buf || !s->Buf)
		return true;
	BlitScratch(d, s, 0, 0, 0, 0, d->W, d->H);
	return true;
}

static bool __fastcall Hook_CopyFromPart(void* self, void* /*edx*/,
	RectangleStruct* pClip, Surface* pSrc, RectangleStruct* pSrcRect, bool, bool)
{
	SurfScratch* d = FindScratch((Surface*)self);
	SurfScratch* s = FindScratch(pSrc);
	if (!d || !s || !d->Buf || !s->Buf)
		return true;
	BlitScratch(d, s,
		pClip ? pClip->X : 0, pClip ? pClip->Y : 0,
		pSrcRect ? pSrcRect->X : 0, pSrcRect ? pSrcRect->Y : 0,
		pSrcRect ? pSrcRect->Width : d->W, pSrcRect ? pSrcRect->Height : d->H);
	return true;
}

static bool __fastcall Hook_CopyFrom(void* self, void* /*edx*/,
	RectangleStruct* /*pClip*/, RectangleStruct* /*pClip2*/, Surface* pSrc,
	RectangleStruct* pDest, RectangleStruct* pSrcRect, bool, bool)
{
	SurfScratch* d = FindScratch((Surface*)self);
	SurfScratch* s = FindScratch(pSrc);
	if (!d || !s || !d->Buf || !s->Buf)
		return true;
	BlitScratch(d, s,
		pDest ? pDest->X : 0, pDest ? pDest->Y : 0,
		pSrcRect ? pSrcRect->X : 0, pSrcRect ? pSrcRect->Y : 0,
		pDest ? pDest->Width : d->W, pDest ? pDest->Height : d->H);
	return true;
}

static bool __fastcall Hook_CanLock(void* /*self*/, void* /*edx*/, DWORD, DWORD)
{
	return true;
}

// ---- vtable 补丁 -----------------------------------------------------------
struct Patch
{
	void**	Orig;
	Fn*		Patched;
};
Patch	g_patch[8];
int		g_patchN = 0;

#define VS_COPYFROMWHOLE 1
#define VS_COPYFROMPART  2
#define VS_COPYFROM      3
#define VS_LOCK          23
#define VS_UNLOCK        24
#define VS_CANLOCK       25
#define VS_ISLOCKED      27

static Fn* BuildPatchedVtbl(void** orig)
{
	// 只改 4 个槽：Lock / Unlock / CanLock / IsLocked。
	// 其余（含所有绘制原语与 CC_Draw_Shape 的调用方）保持原版 —— 这是像素一致的前提。
	const int n = 40;
	Fn* t = (Fn*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Fn) * n);
	if (!t)
		return nullptr;
	for (int i = 0; i < n; ++i)
		t[i] = (Fn)orig[i];

	t[VS_LOCK + g_delta]     = (Fn)&Hook_Lock;
	t[VS_UNLOCK + g_delta]   = (Fn)&Hook_Unlock;
	t[VS_CANLOCK + g_delta]  = (Fn)&Hook_CanLock;
	t[VS_ISLOCKED + g_delta] = (Fn)&Hook_IsLocked;
	t[VS_COPYFROMWHOLE + g_delta] = (Fn)&Hook_CopyFromWhole;
	t[VS_COPYFROMPART + g_delta]  = (Fn)&Hook_CopyFromPart;
	t[VS_COPYFROM + g_delta]      = (Fn)&Hook_CopyFrom;
	return t;
}

static bool PatchSurface(Surface* s, bool primary)
{
	if (!s)
		return false;

	void** vt = *(void***)s;
	if (!vt)
		return false;

	Fn* patched = nullptr;
	for (int i = 0; i < g_patchN; ++i)
		if (g_patch[i].Orig == vt)
			patched = g_patch[i].Patched;

	if (!patched)
	{
		if (g_patchN >= 8)
			return false;
		patched = BuildPatchedVtbl(vt);
		if (!patched)
			return false;
		g_patch[g_patchN].Orig = vt;
		g_patch[g_patchN].Patched = patched;
		++g_patchN;
	}

	// 分配 scratch
	if (g_scrN < kMaxSurfSlot && !FindScratch(s))
	{
		int pitch, bpp, w, h;
		QueryFormat(s, pitch, bpp, w, h);
		if (w <= 0 || h <= 0 || pitch <= 0)
			return false;

		SurfScratch& sc = g_scr[g_scrN];
		sc.S = s;
		sc.Pitch = pitch;
		sc.Bpp = bpp;
		sc.W = w;
		sc.H = h;
		sc.Cap = (size_t)pitch * (size_t)h;

		if (primary && g_sharedBase && sc.Cap + kPixelOffset <= (size_t)g_info->Pad)
		{
			sc.Buf = (uint8_t*)g_sharedBase + kPixelOffset;
			sc.InShared = true;
		}
		else
		{
			sc.Buf = (uint8_t*)VirtualAlloc(nullptr, sc.Cap, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			sc.InShared = false;
		}
		if (!sc.Buf)
			return false;

		memset(sc.Buf, 0, sc.Cap);
		++g_scrN;
		Debug::Log("[DrawCapture] scratch %p %dx%d pitch=%d bpp=%d%s\n",
			sc.Buf, w, h, pitch, bpp, primary ? " (primary)" : "");
	}

	*(void***)s = (void**)patched;
	return true;
}
}

//=============================================================================
// ---- 解除鼠标锁定：把 gamemd 对 ClipCursor / SetCapture 的调用变成空操作 ----
static BOOL WINAPI NoClipCursor(const RECT*) { return TRUE; }
static HWND WINAPI NoSetCapture(HWND) { return nullptr; }

static bool PatchIAT(HMODULE mod, const char* dllName, const char* fnName, void* repl)
{
	if (!mod)
		return false;

	BYTE* base = (BYTE*)mod;
	auto* dos = (IMAGE_DOS_HEADER*)base;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		return false;
	auto* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
		return false;

	const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (!dir.VirtualAddress)
		return false;

	auto* imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress);
	for (; imp->Name; ++imp)
	{
		if (_stricmp((const char*)(base + imp->Name), dllName) != 0)
			continue;

		auto* ft = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
		auto* oft = (IMAGE_THUNK_DATA*)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
		for (; ft->u1.Function; ++ft, ++oft)
		{
			const ULONG_PTR addr = oft->u1.AddressOfData;
			if (!addr || (addr & IMAGE_ORDINAL_FLAG))
				continue;
			auto* hint = (IMAGE_IMPORT_BY_NAME*)(base + addr);
			if (strcmp((const char*)hint->Name, fnName) != 0)
				continue;

			DWORD oldProt = 0;
			if (!VirtualProtect(&ft->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProt))
				return false;
			ft->u1.Function = (ULONG_PTR)repl;
			VirtualProtect(&ft->u1.Function, sizeof(void*), oldProt, &oldProt);
			return true;
		}
	}
	return false;
}

static void PatchInputHooks()
{
	HMODULE game = GetModuleHandleW(nullptr);
	if (!game)
		return;
	const bool a = PatchIAT(game, "user32.dll", "ClipCursor", (void*)&NoClipCursor);
	const bool b = PatchIAT(game, "user32.dll", "SetCapture", (void*)&NoSetCapture);
	ClipCursor(nullptr);
	Debug::Log("[DrawCapture] input unlock: ClipCursor=%d SetCapture=%d\n", (int)a, (int)b);
}
bool DrawCapture::Install()
{
	if (g_installed)
		return true;

	Surface* primary = nullptr;
	__try { primary = DSurface::Primary; }
	__except (EXCEPTION_EXECUTE_HANDLER) {}

	if (!primary)
	{
		Debug::Log("[DrawCapture] Primary surface null, abort\n");
		return false;
	}

	void** vt = *(void***)primary;

	// ---- 探针 ----
	const bool anchored = LocateAnchor(vt, primary, g_delta);
	if (!anchored)
	{
		Debug::Log("[DrawCapture] anchor NOT found -> safe-fail: DDraw untouched,\n"
			"[DrawCapture]   preview will read the real surface instead.\n");
		return false;		// 不装任何补丁，游戏行为完全不变
	}

	// ---- 共享内存 ----
	int pitch, bpp, w, h;
	QueryFormat(primary, pitch, bpp, w, h);
	const size_t pixCap = (size_t)pitch * (size_t)h;
	const size_t total = kPixelOffset + pixCap + (1u << 20);	// +1MB slack

	g_map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
		0, (DWORD)total, kMapName);
	if (g_map)
		g_sharedBase = MapViewOfFile(g_map, FILE_MAP_ALL_ACCESS, 0, 0, 0);

	if (g_sharedBase)
	{
		memset(g_sharedBase, 0, total);
		g_info = (FrameInfo*)g_sharedBase;
		g_info->Magic = kMagic;
		g_info->Version = kVersion;
		g_info->Width = w;
		g_info->Height = h;
		g_info->Pitch = pitch;
		g_info->Bpp = bpp;
		g_info->Pad = (uint32_t)total;		// 供 PatchSurface 校验容量
	}
	else
	{
		Debug::Log("[DrawCapture] shared mem failed err=%lu, heap-only\n", GetLastError());
	}

	// ---- 打补丁 ----
	int ok = 0;
	__try
	{
		if (PatchSurface(DSurface::Primary, true))   ++ok;
		if (PatchSurface(DSurface::Composite, false)) ++ok;
		if (PatchSurface(DSurface::Hidden, false))    ++ok;
		if (PatchSurface(DSurface::Sidebar, false))   ++ok;
		if (PatchSurface(DSurface::Alternate, false)) ++ok;
		if (PatchSurface(DSurface::Temp, false))      ++ok;
		if (PatchSurface(DSurface::Tile, false))      ++ok;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Debug::Log("[DrawCapture] exception during patching\n");
	}

	PatchInputHooks();
	HideGameWindows();

	if (g_info)
		g_info->Opaque = ok > 0 ? 1u : 0u;

	g_installed = (ok > 0);
	Debug::Log("[DrawCapture] installed=%d, %d surfaces, delta=%d, shared=%p\n",
		(int)g_installed, ok, g_delta, g_sharedBase);
	return g_installed;
}

bool DrawCapture::IsInstalled()
{
	return g_installed;
}

void DrawCapture::Publish()
{
	++g_frames;
	if (g_info)
	{
		g_info->FrameId = (uint32_t)g_frames;
		g_info->FramesPresented = g_frames;
		g_info->Opaque = g_installed ? 1u : 0u;
		MemoryBarrier();
		InterlockedExchange((volatile LONG*)&g_info->Epoch, (LONG)(g_frames + 1));
	}
}

bool DrawCapture::GetFrame(void** pixels, int* width, int* height, int* pitch, int* bpp)
{
	if (pixels) *pixels = nullptr;

	// 优先：我们自己的 Primary scratch（已被原版绘制代码填满）
	SurfScratch* sc = nullptr;
	for (int i = 0; i < g_scrN; ++i)
		if (g_scr[i].S && g_scr[i].InShared)
			sc = &g_scr[i];

	if (sc && sc->Buf)
	{
		if (pixels) *pixels = sc->Buf;
		if (width)  *width = sc->W;
		if (height) *height = sc->H;
		if (pitch)  *pitch = sc->Pitch;
		if (bpp)    *bpp = sc->Bpp;
		return true;
	}

	// 退路：直读真实 DDraw surface（用原版 Lock/Unlock，只读不改）
	Surface* primary = nullptr;
	__try { primary = DSurface::Primary; }
	__except (EXCEPTION_EXECUTE_HANDLER) {}
	if (!primary)
		return false;

	void* bits = nullptr;
	__try { bits = primary->Lock(0, 0); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return false; }
	if (!bits)
		return false;

	int p = 0, b = 2, w = primary->Width, h = primary->Height;
	QueryFormat(primary, p, b, w, h);

	if (pixels) *pixels = bits;
	if (width)  *width = w;
	if (height) *height = h;
	if (pitch)  *pitch = p;
	if (bpp)    *bpp = b;
	return true;
}

//=============================================================================
namespace {
struct HideCtx { DWORD Pid; HWND Console; int Hidden; };

BOOL CALLBACK EnumHideProc(HWND hwnd, LPARAM lp)
{
	HideCtx* c = (HideCtx*)lp;
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	if (pid != c->Pid) return TRUE;
	if (hwnd == c->Console) return TRUE;
	if (!IsWindowVisible(hwnd)) return TRUE;
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

DrawCapture::Stats DrawCapture::GetStats()
{
	Stats s{};
	s.Locks = g_locks;
	s.FramesPublished = g_frames;
	s.UnlocksSkipped = g_unlocksSkipped;
	return s;
}
