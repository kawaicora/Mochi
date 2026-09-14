//=============================================================================
//  SurfaceDebug.cpp — 用 YRpp 类型的实现
//  ---------------------------------------------------------------------------
//  对游戏真实 Surface 的读取完全走 YRpp Surface 的 virtual(GetWidth/GetHeight/
//  GetPitch/GetBytesPerPixel/Lock/Unlock)。因为对象是游戏真实例,vtable 运行时
//  是真实现 → 这些调用分派到真实代码, 取值可靠。全部以 try/catch 包裹,失败只
//  日志, 不崩。
//=============================================================================
#include "SurfaceDebug.h"

#include <cstring>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <exception>
#include <vector>
#include <Debug.h>

struct WinData
{
	Surface*         surface  = nullptr;
	int              w = 0, h = 0;           // 客户区(=最近成功的几何)
	std::vector<unsigned char> rgb;           // 24bpp 主缓冲
	HANDLE           hThread  = nullptr;
	HWND             hWnd     = nullptr;
	std::atomic<bool>running  { true };
	std::mutex       mtx;
};
constexpr const wchar_t* kClassName = L"SurfaceDebugView";

LRESULT CALLBACK SD_WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) noexcept;

static bool EnsureClass() noexcept {
	static bool ok = []() noexcept {
		WNDCLASSW wc{};
		wc.lpfnWndProc = SD_WndProc;
		wc.hInstance   = (HINSTANCE)GetModuleHandleW(nullptr);
		wc.hCursor     = LoadCursorW(nullptr,(LPCWSTR) IDC_ARROW);
		wc.hbrBackground = nullptr;
		wc.lpszClassName = kClassName;
		return RegisterClassW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	}();
	return ok;
}

LRESULT CALLBACK SD_WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) noexcept {
	WinData* d = (WinData*)(LONG_PTR)GetWindowLongPtrW(h, GWLP_USERDATA);
	if (d) {
		switch (m)
		{
		case WM_PAINT: {
			PAINTSTRUCT ps{}; HDC dc = BeginPaint(h, &ps);
			RECT rc{}; if (GetClientRect(h, &rc)) {
				int cw = rc.right - rc.left, chh = rc.bottom - rc.top;
				std::lock_guard<std::mutex> g(d->mtx);
				if (cw > 0 && chh > 0) {
					RECT all{ 0,0,cw,chh };
					FillRect(dc, &all, (HBRUSH)GetStockObject(0));
					if (!d->rgb.empty() && d->w == cw && d->h == chh) {
						BITMAPINFO bi{};
						bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
						bi.bmiHeader.biWidth = cw;
						bi.bmiHeader.biHeight = -chh;          // top-down
						bi.bmiHeader.biPlanes = 1;
						bi.bmiHeader.biBitCount = 24;
						bi.bmiHeader.biCompression = BI_RGB;
						SetDIBitsToDevice(dc, 0, 0, cw, chh,
							0, 0, 0, chh, d->rgb.data(), &bi, DIB_RGB_COLORS);
					}
				}
			}
			EndPaint(h, &ps);
			return 0;
		}
		case WM_ERASEBKGND: return 1;   // 全量贴,不重绘背景以免闪烁
		case WM_SIZE:
			InvalidateRect(h, nullptr, FALSE);
			return 0;
		}
	}
	return DefWindowProcW(h, m, wp, lp);
}

static void ThreadLoop(WinData* d) noexcept {
	while (d && d->running.load()) {
		try { SurfaceDebug::RenderSurfaceWindow(d->hWnd); } catch (...) { Debug::Log("bg refresh exc"); }
		std::this_thread::sleep_for(std::chrono::milliseconds(33));
	}
}

static bool TryGetGeom(Surface* s, int& w, int& h) noexcept {
	try {
		if (!s) return false;
		// 多一种取法兜底(GetRect virtual),谁非 0 用谁
		RectangleStruct r{};
		try { s->GetRect(&r); } catch (...) {}
		int w2 = 0, h2 = 0;
		try { w2 = s->GetWidth();  } catch (...) {}
		try { h2 = s->GetHeight(); } catch (...) {}
		if (w2 > 0 && h2 > 0) { w = w2; h = h2; return true; }
		if (r.Width > 0 && r.Height > 0) { w = r.Width; h = r.Height; return true; }
		return false;
	} catch (...) { Debug::Log("TryGetGeom exc"); return false; }
}

HWND SurfaceDebug::CreateSurfaceWindow(Surface* s, const wchar_t* title,bool IsStartRenderThread) {
	int h, w;
	if (!EnsureClass()) return nullptr;
		
	TryGetGeom(s, w, h);

	WinData* d = new WinData();
	d->surface = s; d->w = w; d->h = h;
	try { d->rgb.assign((size_t)w * (size_t)h * 3, 0); } catch (...) { d->rgb.clear(); }
	constexpr DWORD kStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX; // 可拖动

	RECT wr{ 0,0,w,h };
	AdjustWindowRectEx(&wr, kStyle, FALSE, 0);
	HWND hh = CreateWindowExW(0, kClassName,
		title, kStyle,
		CW_USEDEFAULT, CW_USEDEFAULT,
		wr.right - wr.left, wr.bottom - wr.top,
		nullptr, nullptr, (HINSTANCE)GetModuleHandleW(nullptr), nullptr);
	if (!hh) { delete d; return nullptr; }

	d->hWnd = hh;
	SetWindowLongPtrW(hh, GWLP_USERDATA, (LONG_PTR)d);
	d->running = true;
	if (IsStartRenderThread) {
		d->hThread = CreateThread(nullptr, 0,
		[](LPVOID p) -> DWORD { ThreadLoop((WinData*)p); return 0; }, d, 0, nullptr);
	}
	
	ShowWindow(hh, SW_SHOW);
	return hh;
}

void SurfaceDebug::RenderSurfaceWindow(HWND hWnd) {
	if (!hWnd) return;
	WinData* d = (WinData*)(LONG_PTR)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
	if (!d || !d->surface) { InvalidateRect(hWnd, nullptr, FALSE); return; }

	int gw = d->w, gh = d->h;
	if (!TryGetGeom(d->surface, gw, gh)) { InvalidateRect(hWnd, nullptr, FALSE); return; }

	try {
		if (d->surface->IsLocked()) {
			Debug::Log("Target Surface Locked! Skip!");
			return;
		}
		void* bits = d->surface->Lock(0, 0);
		if (bits) {
			int bpp = 2; int pitch = 0;
			try { bpp = d->surface->GetBytesPerPixel(); } catch (...) { bpp = 2; }
			try { pitch = d->surface->GetPitch(); } catch (...) {}
			if (pitch <= 0) pitch = gw * bpp;

			std::lock_guard<std::mutex> g(d->mtx);
			size_t need = (size_t)gw * (size_t)gh * 3;
			if (d->w != gw || d->h != gh || d->rgb.size() < need) {
				d->rgb.assign(need, 0);
				d->w = gw; d->h = gh;
			}
			const unsigned char* src = (const unsigned char*)bits;
			size_t o = 0;
			for (int y = 0; y < gh && y < d->h; ++y) {
				const unsigned char* row = src + (size_t)y * pitch;
				for (int x = 0; x < gw && x < d->w; ++x) {
					unsigned short v = (unsigned short)(row[(size_t)x * bpp] | (row[(size_t)x * bpp + 1] << 8));
					unsigned r0 = (v >> 11) & 31, g0 = (v >> 5) & 63, b0 = v & 31;
					d->rgb[o++] = (unsigned char)((b0 * 255) / 31);   // B
					d->rgb[o++] = (unsigned char)((g0 * 255) / 63);   // G
					d->rgb[o++] = (unsigned char)((r0 * 255) / 31);   // R
				}
			}
			try { d->surface->Unlock(); } catch (...) {}
			InvalidateRect(hWnd, nullptr, FALSE);
			return;
		}
	} catch (...) { Debug::Log("RenderWindow lock/render exc"); }
	InvalidateRect(hWnd, nullptr, FALSE);
}

void SurfaceDebug::DestroySurfaceWindow(HWND hWnd) {
	if (!hWnd) return;
	WinData* d = (WinData*)(LONG_PTR)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
	if (d) {
		try { d->running = false; } catch (...) {}
		if (d->hThread) {
			try { WaitForSingleObject(d->hThread, INFINITE); CloseHandle(d->hThread); } catch (...) {}
		}
		SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
		delete d;
	}
	DestroyWindow(hWnd);
}
