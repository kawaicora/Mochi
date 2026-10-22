//=============================================================================
//  DrawCapture.cpp  —  逻辑只管逻辑，渲染只管渲染
//  ---------------------------------------------------------------------------
//  【像素级一致】
//    不重写任何绘制。只把 Surface::Lock 返回的指针换成我们自己的常驻内存，
//    原版 CC_Draw_Shape / 全部 blitter 原封不动画进这块内存。
//    跑的是原版代码 => 画面与原本像素级一致。
//
//  【本版关键改变：补丁打在 vtable 表本身，而不是逐个对象】
//    DSurface 的所有实例共用同一张 vtable（在只读 .rdata 里）。
//    之前是给已知 7 个对象各建一份副本再替换 vptr —— 那么运行时新建的
//    surface（加载界面、设置面板、计分板、字幕……）就完全漏掉。
//    现在直接 VirtualProtect 那张共享表并原地改写，**现存与将来创建的
//    所有 DSurface 实例自动生效**，未知 surface 的 scratch 按需分配。
//
//  【性能契约：逻辑 100% 全速】
//    热路径：查表 + 指针加法。scratch 只在首次遇到该 surface 时分配一次。
//=============================================================================

#include "DrawCapture.h"

#include <YRPP.h>
#include <Debug.h>

#include <stdint.h>
#include <string.h>

namespace {

using namespace DrawCaptureProto;

constexpr uint32_t kPixelOffset = 4096;
constexpr int      kMaxSurfSlot = 32;
constexpr size_t   kScratchBudget = 256u * 1024u * 1024u;	// 所有 scratch 总量上限

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
	const char*	Name;			// 已知面给名字，未知面为 nullptr
	uint32_t	NonZero;
	uint32_t	LastTick;		// 最后被 Lock 的时刻
	uint64_t	LockCount;
};

SurfScratch	g_scr[kMaxSurfSlot];
int			g_scrN = 0;
size_t		g_scratchUsed = 0;

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

// Lock 永不返回 NULL 的兜底（8MB，足够任何单面）
static uint8_t g_nullSink[8 * 1024 * 1024];

using Fn = void(*)();

// ---- 基础查询 --------------------------------------------------------------
__forceinline SurfScratch* FindScratch(Surface* s) noexcept
{
	for (int i = 0; i < g_scrN; ++i)
		if (g_scr[i].S == s && g_scr[i].Buf)
			return &g_scr[i];
	return nullptr;
}

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

// 调用未被替换的原版虚函数（vtable 已原地改写，但几何类槽位保持不变）
static int CallVirtual(int slot, Surface* s) noexcept
{
	void** vt = *(void***)s;
	if (!vt)
		return 0;
	return SafeCall((Fn)vt[slot + g_delta], s);
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

static void QueryFormat(Surface* s, int& pitch, int& bpp, int& w, int& h) noexcept
{
	w = s->Width;
	h = s->Height;
	pitch = 0;
	bpp = 0;

	// 1) 优先读 DDraw surface 描述
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

	// 2) 退回原版虚函数（注意：这两个槽位从未被替换）
	if (bpp <= 0)
	{
		const int v = CallVirtual(28, s);			// GetBytesPerPixel
		if (v == 1 || v == 2 || v == 4)
			bpp = v;
	}
	if (pitch <= 0)
	{
		const int v = CallVirtual(29, s);			// GetPitch
		if (v > 0)
			pitch = v;
	}

	if (bpp <= 0)
		bpp = 2;
	if (w <= 0 || h <= 0)
		return;
	if (pitch <= 0)
		pitch = w * bpp;
}

// ---- 按需给任意 surface 建 scratch ----------------------------------------
static SurfScratch* EnsureScratch(Surface* s, bool preferShared, const char* name) noexcept
{
	if (!s)
		return nullptr;

	SurfScratch* existing = FindScratch(s);
	if (existing)
		return existing;

	if (g_scrN >= kMaxSurfSlot)
		return nullptr;

	int pitch = 0, bpp = 0, w = 0, h = 0;
	__try { QueryFormat(s, pitch, bpp, w, h); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }

	if (w <= 0 || h <= 0 || pitch <= 0)
		return nullptr;

	if (bpp <= 0) bpp = 2;
	if (bpp > 4)  bpp = 4;

	const size_t cap = (size_t)pitch * (size_t)h;
	if (cap == 0 || cap > kScratchBudget)
		return nullptr;
	if (g_scratchUsed + cap > kScratchBudget)
	{
		Debug::Log("[DrawCapture] scratch budget exhausted (%llu bytes used)\n",
			(unsigned long long)g_scratchUsed);
		return nullptr;
	}

	SurfScratch& sc = g_scr[g_scrN];

	if (preferShared && g_sharedBase && g_info &&
		cap + kPixelOffset <= (size_t)g_info->Pad)
	{
		sc.Buf = (uint8_t*)g_sharedBase + kPixelOffset;
		sc.InShared = true;
	}
	else
	{
		sc.Buf = (uint8_t*)VirtualAlloc(nullptr, cap, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		sc.InShared = false;
	}
	if (!sc.Buf)
		return nullptr;

	memset(sc.Buf, 0, cap);

	sc.S = s;
	sc.Cap = cap;
	sc.Pitch = pitch;
	sc.Bpp = bpp;
	sc.W = w;
	sc.H = h;
	sc.Name = name;
	sc.NonZero = 0;

	g_scratchUsed += cap;
	++g_scrN;

	Debug::Log("[DrawCapture] scratch %-9s %dx%d pitch=%d bpp=%d %p%s\n",
		name ? name : "(auto)", w, h, pitch, bpp, sc.Buf,
		sc.InShared ? " shared" : "");
	return &sc;
}

// ---- 挂钩实现 --------------------------------------------------------------
static void* __fastcall Hook_Lock(void* self, void* /*edx*/, int X, int Y)
{
	++g_locks;

	SurfScratch* sc = FindScratch((Surface*)self);
	if (!sc)
		sc = EnsureScratch((Surface*)self, false, nullptr);	// 运行时新建的面
	if (!sc || !sc->Buf)
		return g_nullSink;

	sc->LastTick = GetTickCount();
	++sc->LockCount;

	long long off = (long long)Y * sc->Pitch + (long long)X * sc->Bpp;
	if (off < 0)
		off = 0;
	if ((size_t)off >= sc->Cap)
		off = 0;

	void* ptr = sc->Buf + off;

	// 原版 Lock 会把锁到的指针写进 DSurface::Buffer，而 MochiGraph 等既有
	// 代码直接读 surface->Buffer 当像素基址用。替换了 Lock 就必须照做。
	__try
	{
		((DSurface*)self)->Buffer = ptr;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {}

	return ptr;
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

static SurfScratch* ScratchFor(Surface* s)
{
	if (!s)
		return nullptr;
	SurfScratch* sc = FindScratch(s);
	if (!sc)
		sc = EnsureScratch(s, false, nullptr);
	return sc;
}

static bool __fastcall Hook_CopyFromWhole(void* self, void* /*edx*/, Surface* pSrc, bool, bool)
{
	SurfScratch* d = ScratchFor((Surface*)self);
	SurfScratch* s = ScratchFor(pSrc);
	if (!d || !s)
		return true;
	BlitScratch(d, s, 0, 0, 0, 0, d->W, d->H);
	return true;
}

static bool __fastcall Hook_CopyFromPart(void* self, void* /*edx*/,
	RectangleStruct* pClip, Surface* pSrc, RectangleStruct* pSrcRect, bool, bool)
{
	SurfScratch* d = ScratchFor((Surface*)self);
	SurfScratch* s = ScratchFor(pSrc);
	if (!d || !s)
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
	SurfScratch* d = ScratchFor((Surface*)self);
	SurfScratch* s = ScratchFor(pSrc);
	if (!d || !s)
		return true;
	BlitScratch(d, s,
		pDest ? pDest->X : 0, pDest ? pDest->Y : 0,
		pSrcRect ? pSrcRect->X : 0, pSrcRect ? pSrcRect->Y : 0,
		pDest ? pDest->Width : d->W, pDest ? pDest->Height : d->H);
	return true;
}

// ---- 原地改写共享 vtable ---------------------------------------------------
#define VS_COPYFROMWHOLE 1
#define VS_COPYFROMPART  2
#define VS_COPYFROM      3
#define VS_LOCK          23
#define VS_UNLOCK        24
#define VS_CANLOCK       25
#define VS_ISLOCKED      27

static bool PatchVtableInPlace(void** vt)
{
	constexpr int kSlots = 48;
	DWORD oldProt = 0;
	if (!VirtualProtect(vt, sizeof(void*) * kSlots, PAGE_READWRITE, &oldProt))
	{
		Debug::Log("[DrawCapture] VirtualProtect on vtable failed err=%lu\n", GetLastError());
		return false;
	}

	vt[VS_COPYFROMWHOLE + g_delta] = (void*)&Hook_CopyFromWhole;
	vt[VS_COPYFROMPART + g_delta] = (void*)&Hook_CopyFromPart;
	vt[VS_COPYFROM + g_delta] = (void*)&Hook_CopyFrom;
	vt[VS_LOCK + g_delta] = (void*)&Hook_Lock;
	vt[VS_UNLOCK + g_delta] = (void*)&Hook_Unlock;
	vt[VS_CANLOCK + g_delta] = (void*)&Hook_CanLock;
	vt[VS_ISLOCKED + g_delta] = (void*)&Hook_IsLocked;

	VirtualProtect(vt, sizeof(void*) * kSlots, oldProt, &oldProt);
	return true;
}

// ---- IAT：按名字匹配，把鼠标捕获相关调用变空操作 --------------------------
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
	const size_t step = 61;
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
	if (!LocateAnchor(vt, primary, g_delta))
	{
		Debug::Log("[DrawCapture] anchor NOT found -> safe-fail: DDraw untouched\n");
		return false;
	}

	// 共享内存（Primary 的像素直接放进去，外部 render 可零拷贝读取）
	int pitch = 0, bpp = 0, w = 0, h = 0;
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

	// ---- 关键：原地改写共享 vtable，一次性覆盖所有现存与将来的实例 ----
	const bool patched = PatchVtableInPlace(vt);
	if (!patched)
		return false;

	// 已知面先建好 scratch（Primary 用共享内存那块）
	__try
	{
		EnsureScratch(DSurface::Primary, true, "Primary");
		EnsureScratch(DSurface::Hidden, false, "Hidden");
		EnsureScratch(DSurface::Composite, false, "Composite");
		EnsureScratch(DSurface::Sidebar, false, "Sidebar");
		EnsureScratch(DSurface::Alternate, false, "Alternate");
		EnsureScratch(DSurface::Temp, false, "Temp");
		EnsureScratch(DSurface::Tile, false, "Tile");
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Debug::Log("[DrawCapture] exception while preparing scratches\n");
	}

	PatchInputHooks();
	HideGameWindows();

	if (g_info)
		g_info->Opaque = 1u;

	g_installed = true;
	Debug::Log("[DrawCapture] installed, vtable patched in place (delta=%d), %d scratches, %llu bytes\n",
		g_delta, g_scrN, (unsigned long long)g_scratchUsed);
	return true;
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

	const uint64_t now = GetTickCount64();
	if (g_installed && now - g_lastStatTick >= 3000)
	{
		g_lastStatTick = now;
		for (int i = 0; i < g_scrN; ++i)
			g_scr[i].NonZero = SampleNonZero(g_scr[i]);

		char buf[640];
		int n = 0;
		for (int i = 0; i < g_scrN && n < 560; ++i)
		{
			n += sprintf_s(buf + n, sizeof(buf) - n, "%s=%u ",
				g_scr[i].Name ? g_scr[i].Name : "auto", g_scr[i].NonZero);
		}
		Debug::Log("[DrawCapture] fill: %s| locks=%llu blits=%llu\n",
			buf, (unsigned long long)g_locks, (unsigned long long)g_blits);
	}
}

bool DrawCapture::GetFrame(void** pixels, int* width, int* height, int* pitch, int* bpp)
{
	if (pixels) *pixels = nullptr;

	// 显示"游戏当前正在画的那一块"：按最后锁定时刻取最新，并列时优先 Primary。
	// 加载界面 / 设置面板 / 计分板这些换面的时刻会自动跟随，
	// 不需要我们猜它到底用的是哪一个 surface。
	SurfScratch* best = nullptr;
	uint32_t bestTick = 0;
	uint64_t bestLocks = 0;

	for (int i = 0; i < g_scrN; ++i)
	{
		SurfScratch& sc = g_scr[i];
		if (!sc.Buf)
			continue;
		const bool isPrimary = (sc.Name && strcmp(sc.Name, "Primary") == 0);
		if (!best ||
			sc.LastTick > bestTick ||
			(sc.LastTick == bestTick && sc.LockCount > bestLocks) ||
			(sc.LastTick == bestTick && sc.LockCount == bestLocks && isPrimary))
		{
			best = &sc;
			bestTick = sc.LastTick;
			bestLocks = sc.LockCount;
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
