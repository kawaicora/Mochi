//=============================================================================
//  DrawCapture.cpp  —  逻辑只管逻辑，渲染只管渲染
//  ---------------------------------------------------------------------------
//  【像素级一致】
//    不重写任何绘制。只把 Surface::Lock 返回的指针换成我们自己的常驻内存，
//    原版 CC_Draw_Shape / 全部 blitter 原封不动地画进这块内存。
//    跑的是原版代码 => 画面与原本像素级一致。
//
//  【本版新增：自诊断 + 自动选面对】
//    引擎最终可能是用 DDraw Blt 把 Hidden/Composite 推到 Primary 的，
//    那条路绕过所有 Surface 虚函数，Primary 的 scratch 就会一直是空的。
//    所以这里对每块缓冲采样非零像素数，GetFrame 返回"最满"的那块，
//    并在每个逻辑帧打印一次各面的填充率 —— 一眼看出画面落在哪。
//
//  【性能契约：逻辑 100% 全速】
//    热路径只有查表 + 指针加法。无锁/无分配/无系统调用/无日志。
//=============================================================================

#include "DrawCapture.h"

#include <YRPP.h>
#include <Debug.h>

#include <stdint.h>
#include <string.h>

namespace {

using namespace DrawCaptureProto;

constexpr uint32_t kPixelOffset = 4096;
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
	bool		InShared;
	const char*	Name;
	uint32_t	NonZero;		// 采样得到的非零像素数（填充率参考）
};

SurfScratch	g_scr[kMaxSurfSlot];
int			g_scrN = 0;

void*		g_sharedBase = nullptr;
HANDLE		g_map = nullptr;
FrameInfo*	g_info = nullptr;

uint64_t	g_locks = 0;
uint64_t	g_unlocks = 0;
uint64_t	g_blits = 0;
uint64_t	g_frames = 0;
bool		g_installed = false;
int			g_delta = 0;
uint64_t	g_lastStatTick = 0;

// 安全兜底：Lock 永远不会返回 NULL，避免调用方解引用崩溃
static uint8_t g_nullSink[8 * 1024 * 1024];

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

	// 只试 GetWidth/GetHeight 可能落下的纯 getter 槽位。
	// 绝不 20..48 全扫 —— 那会调到 DrawLine_/DrawRectEx/DrawGradientLine
	// 这类要指针参数的函数，用垃圾指针写内存会把游戏直接搞崩。
	static const int kCand[] = { 31, 32, 30, 29 };
	for (int c = 0; c < 4; ++c)
	{
		const int i = kCand[c];
		const int w = SafeCall((Fn)vt[i], s);
		const int h = SafeCall((Fn)vt[i + 1], s);
		Debug::Log("[DrawCapture]   slot %d -> %d , %d\n", i, w, h);
		if (w == wantW && h == wantH)
		{
			delta = i - 31;
			Debug::Log("[DrawCapture] anchor FOUND at slot %d (delta=%d)\n", i, delta);
			return true;
		}
	}
	return false;
}

// ---- 取 pitch/bpp ----------------------------------------------------------
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

// ---- 挂钩实现 --------------------------------------------------------------
static void* __fastcall Hook_Lock(void* self, void* /*edx*/, int X, int Y)
{
	++g_locks;
	SurfScratch* sc = FindScratch((Surface*)self);
	if (!sc || !sc->Buf)
		return g_nullSink;	// 绝不返回 NULL：否则绘制方只加行偏移就往低地址写，必崩

	long long off = (long long)Y * sc->Pitch + (long long)X * sc->Bpp;
	if (off < 0)
		off = 0;
	if ((size_t)off >= sc->Cap)
		off = 0;
	return sc->Buf + off;
}

static bool __fastcall Hook_Unlock(void* /*self*/, void* /*edx*/)
{
	++g_unlocks;
	return true;
}

static bool __fastcall Hook_IsLocked(void* /*self*/, void* /*edx*/)
{
	return false;
}

static bool __fastcall Hook_CanLock(void* /*self*/, void* /*edx*/, DWORD, DWORD)
{
	return true;
}

// ---- CPU 拷贝：替代走不通的 DDraw Blt --------------------------------------
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
	++g_blits;
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

// ---- vtable 补丁 -----------------------------------------------------------
struct Patch { void** Orig; Fn* Patched; };
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
	const int n = 40;
	Fn* t = (Fn*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Fn) * n);
	if (!t)
		return nullptr;
	for (int i = 0; i < n; ++i)
		t[i] = (Fn)orig[i];

	// t[VS_COPYFROMWHOLE + g_delta] = (Fn)&Hook_CopyFromWhole;	// 签名未证实，先摘掉
	// t[VS_COPYFROMPART + g_delta]  = (Fn)&Hook_CopyFromPart;	// 签名未证实，先摘掉
	// t[VS_COPYFROM + g_delta]      = (Fn)&Hook_CopyFrom;	// 签名未证实，先摘掉
	t[VS_LOCK + g_delta]          = (Fn)&Hook_Lock;
	t[VS_UNLOCK + g_delta]        = (Fn)&Hook_Unlock;
	t[VS_CANLOCK + g_delta]       = (Fn)&Hook_CanLock;
	t[VS_ISLOCKED + g_delta]      = (Fn)&Hook_IsLocked;
	return t;
}

static bool PatchSurface(Surface* s, bool primary, const char* name)
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
		sc.Name = name;
		sc.NonZero = 0;
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
		Debug::Log("[DrawCapture] scratch %-10s %dx%d pitch=%d bpp=%d %p\n",
			name, w, h, pitch, bpp, sc.Buf);
	}

	if (!FindScratch(s))
	{
		Debug::Log("[DrawCapture] %s has no scratch -> vtable NOT patched", name ? name : "?");
		return false;
	}

	*(void***)s = (void**)patched;
	return true;
}

// ---- IAT：按名字匹配（只碰 user32 的具名项，最保守）------------------------
static BOOL WINAPI NoClipCursor(const RECT*) { return TRUE; }
static HWND WINAPI NoSetCapture(HWND) { return nullptr; }

static bool PatchIATByName(HMODULE mod, const char* dllName, const char* fnName, void* repl)
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
			const ULONG_PTR a = oft->u1.AddressOfData;
			if (!a || (a & IMAGE_ORDINAL_FLAG))
				continue;
			auto* hint = (IMAGE_IMPORT_BY_NAME*)(base + a);
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

	const bool a = PatchIATByName(game, "user32.dll", "ClipCursor", (void*)&NoClipCursor);
	const bool b = PatchIATByName(game, "user32.dll", "SetCapture", (void*)&NoSetCapture);

	ClipCursor(nullptr);
	Debug::Log("[DrawCapture] input unlock: ClipCursor=%d SetCapture=%d\n", (int)a, (int)b);
}
// ---- 填充率采样 ------------------------------------------------------------
static uint32_t SampleNonZero(const SurfScratch& sc) noexcept
{
	if (!sc.Buf || sc.W <= 0 || sc.H <= 0)
		return 0;

	uint32_t nz = 0;
	const int bpp = (sc.Bpp > 0) ? sc.Bpp : 2;
	const size_t total = (size_t)sc.W * (size_t)sc.H;
	const size_t step = 61;			// 质数步长，避开对齐偏差
	for (size_t i = 0; i < total; i += step)
	{
		const size_t y = i / (size_t)sc.W;
		const size_t x = i % (size_t)sc.W;
		const uint8_t* p = sc.Buf + y * (size_t)sc.Pitch + x * (size_t)bpp;
		if (p[0] || (bpp > 1 && p[1]))
			++nz;
	}
	return nz;
}
}

//=============================================================================
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

	const bool anchored = LocateAnchor(vt, primary, g_delta);
	if (!anchored)
	{
		Debug::Log("[DrawCapture] anchor NOT found -> safe-fail: DDraw untouched\n");
		return false;
	}

	int pitch, bpp, w, h;
	QueryFormat(primary, pitch, bpp, w, h);
	const size_t pixCap = (size_t)pitch * (size_t)h;
	const size_t total = kPixelOffset + pixCap + (1u << 20);

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
		g_info->Pad = (uint32_t)total;
	}
	else
	{
		Debug::Log("[DrawCapture] shared mem failed err=%lu, heap-only\n", GetLastError());
	}

	int ok = 0;
	__try
	{
		if (PatchSurface(DSurface::Primary, true, "Primary"))     ++ok;
		if (PatchSurface(DSurface::Composite, false, "Composite")) ++ok;
		if (PatchSurface(DSurface::Hidden, false, "Hidden"))       ++ok;
		if (PatchSurface(DSurface::Sidebar, false, "Sidebar"))     ++ok;
		if (PatchSurface(DSurface::Alternate, false, "Alternate")) ++ok;
		if (PatchSurface(DSurface::Temp, false, "Temp"))           ++ok;
		if (PatchSurface(DSurface::Tile, false, "Tile"))           ++ok;
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

	// 每 2 秒报一次各面填充率，直接看出画面落在哪块
	const uint64_t now = GetTickCount64();
	if (g_installed && now - g_lastStatTick >= 2000)
	{
		g_lastStatTick = now;
		for (int i = 0; i < g_scrN; ++i)
			g_scr[i].NonZero = SampleNonZero(g_scr[i]);

		char buf[512];
		int n = 0;
		for (int i = 0; i < g_scrN && n < 460; ++i)
		{
			n += sprintf_s(buf + n, sizeof(buf) - n, "%s=%u ",
				g_scr[i].Name ? g_scr[i].Name : "?", g_scr[i].NonZero);
		}
		Debug::Log("[DrawCapture] fill: %s | locks=%llu blits=%llu\n",
			buf, (unsigned long long)g_locks, (unsigned long long)g_blits);
	}
}

bool DrawCapture::GetFrame(void** pixels, int* width, int* height, int* pitch, int* bpp)
{
	if (pixels) *pixels = nullptr;

	// 选"最满"的那块；同等时优先 Primary
	SurfScratch* best = nullptr;
	uint32_t bestNz = 0;
	for (int i = 0; i < g_scrN; ++i)
	{
		SurfScratch& sc = g_scr[i];
		if (!sc.Buf)
			continue;
		const uint32_t nz = sc.NonZero;
		if (!best || nz > bestNz || (nz == bestNz && sc.Name && sc.Name[0] == 'P'))
		{
			best = &sc;
			bestNz = nz;
		}
	}

	if (best && best->Buf)
	{
		if (pixels) *pixels = best->Buf;
		if (width)  *width = best->W;
		if (height) *height = best->H;
		if (pitch)  *pitch = best->Pitch;
		if (bpp)    *bpp = best->Bpp;
		return true;
	}

	// 退路：直读真实 DDraw surface
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

	int p = 0, b = 2, ww = primary->Width, hh = primary->Height;
	QueryFormat(primary, p, b, ww, hh);

	if (pixels) *pixels = bits;
	if (width)  *width = ww;
	if (height) *height = hh;
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
	s.UnlocksSkipped = g_unlocks;
	return s;
}
