#pragma once
#include <Windows.h>

class REGISTERS;

// BEFORE_HOOK
// -----------
// 和 DEFINE_HOOK 同一套契约，只是入口是运行期接管的：
//
//   DEFINE_HOOK(addr, funcname, size)      编译期登记，由 Syringe 统一写 jmp
//   BEFORE_HOOK(addr, funcname, size)      运行期登记，自己写 jmp
//
// 按目标地址当前状态自动分两种情况：
//
//   A. 目标地址还没有被别人 hook（首字节不是 E9）
//      -> 与 DEFINE_HOOK 完全等价：备份原始 size 字节，跳板里原样执行这 size 字节，
//         执行完 jmp addr+size，正好落在被复写区间之后的第一条指令上。
//
//   B. 目标地址已经被别人 hook（首字节是 E9，例如 Ares）
//      -> 备份这 5 字节（还原必须），把这条 jmp 改写指向自己的跳板；
//         处理函数返回 0 时把执行权交还别人的跳板（他们的链照跑）；
//         返回非 0 时直接 jmp 该地址，跳过他们。
//
// 返回值语义（与 DEFINE_HOOK 一致）：
//   eax == 0     -> 继续（A：执行备份的原始指令后回 addr+size；B：交还别人的跳板）
//   eax != 0     -> 直接 jmp 到 eax，其它什么都不做
//
// 硬约束（这条是踩过的坑，别改回去）：
//   1. 真正写 jmp 的时刻必须在 Syringe 写完 hook 之后。Syringe 的流程是
//      "LoadLibrary 所有插件 → 收集 .syhks → 进调试循环里统一写 code hook"，
//      也就是说 DllMain / 静态初始化阶段 0x50AF10 上还没有 Ares 的 E9，那时候自己写进去的
//      patch 会在后面被 Syringe 写 Ares 的 hook 覆盖掉，钩子既不生效也不报错。
//      所以宏本身只"登记"（Queue），由 BeforeHook::ApplyPending() 在第一个渲染帧统一装。
//   2. size 必须是完整指令的字节总和；覆盖区间内不得含 rel32（call/jmp/jcc rel32）。
//   3. size < 5 时按 5 处理；size 上限 64。
//
// 用法（函数体与 DEFINE_HOOK 一致，可以直接抄）：
//   BEFORE_HOOK(0x4FDCDD, OHMyHook, 0x8)     // 被复写的 8 字节刚好是完整指令
//   {
//       GET(TechnoClass*, pThis, ECX);
//       return 0;
//   }

class BeforeHook
{
public:
	typedef DWORD(__cdecl* Handler)(REGISTERS*);

	static bool  Install(DWORD addr, Handler handler, DWORD size, const char* tag);

	// 只登记，不碰内存。静态初始化阶段（DllMain 内）只能走这个。
	static bool  Queue(DWORD addr, Handler handler, DWORD size, const char* tag);

	// 把登记的项真正装上；幂等，装完清空队列。第一个渲染帧调一次就够。
	static int   ApplyPending();

	static bool  Uninstall(DWORD addr);
	static void  UninstallAll();
	static bool  IsInstalled(DWORD addr);
	static DWORD GetChainedTarget(DWORD addr);

private:
	enum { MAX_SIZE = 64 };

	struct Entry
	{
		DWORD       Addr = 0;
		DWORD       Size = 0;                  // 实际覆盖长度 = max(size, 5)
		BYTE        Original[MAX_SIZE] = {};   // 覆盖前的原始字节（还原必须）
		DWORD       ChainedTarget = 0;         // 非 0 = 目标原本就是别人的跳板
		void*       Stub = nullptr;
		const char* Tag = nullptr;
		bool        Active = false;
	};

	struct Pending
	{
		DWORD       Addr = 0;
		Handler     Fn = nullptr;
		DWORD       Size = 0;
		const char* Tag = nullptr;
	};

	enum { MAX_PENDING = 16 };

	static Entry* Find(DWORD addr);
	static void   WriteDword(BYTE* pWhere, DWORD value);
	static bool   PatchCode(DWORD addr, const BYTE* pBytes, DWORD size);

	inline static Entry s_Entries[8];
	inline static int   s_Count = 0;

	inline static Pending s_Pending[MAX_PENDING];
	inline static int     s_PendingCount = 0;
};

// 用法与 DEFINE_HOOK 完全一致：宏后面直接跟函数体。
//   BEFORE_HOOK(0x50AF10, HouseClass_UpdateSuperWeaponsOwned, 5)
//   {
//       GET(HouseClass*, pThis, ECX);
//       ...
//       return 0;
//   }
//
// 宏在静态初始化阶段只把 (addr, handler, size, tag) 塞进队列，不写内存；
// 真正写 jmp 的时刻由 BeforeHook::ApplyPending() 决定（见文件头硬约束 1）。
#define BEFORE_HOOK(addr, funcname, size)                                        \
	DWORD __cdecl funcname(REGISTERS* R);                                        \
	struct BeforeHookAuto_##funcname                                             \
	{                                                                            \
		BeforeHookAuto_##funcname()                                              \
		{ ::BeforeHook::Queue((DWORD)(addr), &funcname, (DWORD)(size), #funcname); } \
	};                                                                           \
	inline static BeforeHookAuto_##funcname s_BeforeHookAuto_##funcname;         \
	DWORD __cdecl funcname(REGISTERS* R)
