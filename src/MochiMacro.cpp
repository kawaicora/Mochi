#include "MochiMacro.h"
#include <Syringe.h>
#include <cstring>
#include "Debug.h"

// Mochi 工程约定：不用 namespace，不用文件级 static。
// 所有内部状态放在类的 inline static 成员里。

void BeforeHook::WriteDword(BYTE* pWhere, DWORD value)
{
	memcpy(pWhere, &value, sizeof(value));
}

BeforeHook::Entry* BeforeHook::Find(DWORD addr)
{
	for (int i = 0; i < s_Count; ++i)
	{
		if (s_Entries[i].Addr == addr)
			return &s_Entries[i];
	}
	return nullptr;
}

bool BeforeHook::PatchCode(DWORD addr, const BYTE* pBytes, DWORD size)
{
	if (!addr || !pBytes || !size)
		return false;

	DWORD pageBase = addr & 0xFFFFF000u;
	DWORD span = (addr - pageBase) + size;
	if (span > 0x1000)
		span = 0x2000;

	DWORD oldProtect = 0;
	if (!VirtualProtect((void*)(uintptr_t)pageBase, span, PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		Debug::Log("[BeforeHook] VirtualProtect 0x%08X 失败 err=%lu\n", addr, GetLastError());
		return false;
	}

	memcpy((void*)(uintptr_t)addr, pBytes, size);

	DWORD tmpProtect = 0;
	VirtualProtect((void*)(uintptr_t)pageBase, span, oldProtect, &tmpProtect);
	FlushInstructionCache(GetCurrentProcess(), (void*)(uintptr_t)addr, size);
	return true;
}

// 静态初始化阶段（DllMain 内）只登记，不写内存：
// 那时 Syringe 还没写它的 code hook，自己先写进 0x50AF10 的 jmp 会被它后面整体覆盖掉。
bool BeforeHook::Queue(DWORD addr, Handler handler, DWORD size, const char* tag)
{
	if (!addr || !handler)
	{
		Debug::Log("[BeforeHook] Queue 参数无效：addr=0x%08X handler=0x%08X（%s）\n",
			addr, (DWORD)(uintptr_t)handler, tag ? tag : "?");
		return false;
	}

	if (s_PendingCount >= (int)MAX_PENDING)
	{
		Debug::Log("[BeforeHook] 待装队列满，0x%08X（%s）登记失败\n", addr, tag ? tag : "?");
		return false;
	}

	Pending& p = s_Pending[s_PendingCount++];
	p.Addr = addr;
	p.Fn = handler;
	p.Size = size;
	p.Tag = tag;
	return true;
}

// 第一个渲染帧调用：这时 Syringe 的 code hook 早已写完，0x50AF10 上是 Ares 的 E9，
// Install 会走"接管别人的 jmp"分支。幂等，队列清空后直接返回。
int BeforeHook::ApplyPending()
{
	if (s_PendingCount <= 0)
		return 0;

	int done = 0;
	const int count = s_PendingCount;
	// 先清计数：Install 里若失败会打日志，但不再重试，避免每帧刷屏
	s_PendingCount = 0;

	for (int i = 0; i < count; ++i)
	{
		Pending& p = s_Pending[i];
		if (p.Fn && p.Addr)
		{
			if (Install(p.Addr, p.Fn, p.Size, p.Tag))
				++done;
		}
		p.Addr = 0;
		p.Fn = nullptr;
		p.Size = 0;
		p.Tag = nullptr;
	}

	Debug::Log("[BeforeHook] ApplyPending 完成：%d/%d 安装成功\n", done, count);
	return done;
}

bool BeforeHook::Install(DWORD addr, Handler handler, DWORD size, const char* tag)
{
	if (!addr || !handler)
		return false;

	// size 语义与 DEFINE_HOOK 一致：小于 5 按 5，大于 5 用它自己
	DWORD len = size ? size : 5;
	if (len < 5)
		len = 5;
	if (len > (DWORD)MAX_SIZE)
	{
		Debug::Log("[BeforeHook] 0x%08X size=0x%X 超过上限 %d，安装失败\n", addr, size, MAX_SIZE);
		return false;
	}

	// 1. 先把覆盖区原始字节全部读出来 —— 还原必须，不能只认 JMP
	BYTE original[MAX_SIZE] = {};
	memcpy(original, (const void*)(uintptr_t)addr, len);

	// 2. 判断这条地址是否已经被别人 hook 过
	const bool bChained = (original[0] == 0xE9);
	DWORD chainedTarget = 0;
	if (bChained)
	{
		int rel = 0;
		memcpy(&rel, original + 1, sizeof(rel));
		chainedTarget = addr + 5 + (DWORD)rel;   // 别人的跳板入口
	}

	Entry* pEntry = Find(addr);
	if (pEntry && pEntry->Active)
	{
		Debug::Log("[BeforeHook] 0x%08X 已装过（%s），重复注册被忽略\n",
			addr, pEntry->Tag ? pEntry->Tag : "?");
		return false;
	}

	if (!pEntry)
	{
		if (s_Count >= (int)(sizeof(s_Entries) / sizeof(s_Entries[0])))
		{
			Debug::Log("[BeforeHook] 表满，0x%08X（%s）安装失败\n", addr, tag ? tag : "?");
			return false;
		}
		pEntry = &s_Entries[s_Count];
		s_Count++;
		memset(pEntry, 0, sizeof(*pEntry));
		pEntry->Addr = addr;
	}

	// 3. 分配跳板：前 16 字节放 ReturnEIP，后面是代码
	DWORD codeLen = 38 + (bChained ? 6 : (len + 5));
	BYTE* pBase = (BYTE*)VirtualAlloc(nullptr, 16 + codeLen + 8,
		MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!pBase)
	{
		Debug::Log("[BeforeHook] VirtualAlloc 失败，0x%08X 安装失败\n", addr);
		return false;
	}

	DWORD retEip = (DWORD)(uintptr_t)pBase;
	WriteDword(pBase, 0);

	BYTE* pCode = pBase + 16;
	size_t pos = 0;

	// ---- code_call：与 Syringe 的模板逐字节同构 ----
	pCode[pos++] = 0x60;                                   // pushad
	pCode[pos++] = 0x9C;                                   // pushfd
	pCode[pos++] = 0x68;                                   // push HookAddress -> REGISTERS::Origin
	WriteDword(pCode + pos, addr);
	pos += 4;
	pCode[pos++] = 0x54;                                   // push esp -> REGISTERS*
	pCode[pos++] = 0xE8;                                   // call handler
	{
		int callRel = (int)((uintptr_t)handler - (uintptr_t)(pCode + pos + 4));
		WriteDword(pCode + pos, (DWORD)callRel);
		pos += 4;
	}
	pCode[pos++] = 0x83; pCode[pos++] = 0xC4; pCode[pos++] = 0x08;  // add esp, 8
	pCode[pos++] = 0xA3;                                   // mov ds:ReturnEIP, eax
	WriteDword(pCode + pos, retEip);
	pos += 4;
	pCode[pos++] = 0x9D;                                   // popfd
	pCode[pos++] = 0x61;                                   // popad
	pCode[pos++] = 0x83; pCode[pos++] = 0x3D;              // cmp ds:ReturnEIP, 0
	WriteDword(pCode + pos, retEip);
	pos += 4;
	pCode[pos++] = 0x00;
	pCode[pos++] = 0x74; pCode[pos++] = 0x06;              // jz .proceed
	pCode[pos++] = 0xFF; pCode[pos++] = 0x25;              // jmp ds:ReturnEIP（非 0 直接去目标）
	WriteDword(pCode + pos, retEip);
	pos += 4;

	// ---- .proceed ----
	if (bChained)
	{
		// 别人已经 hook 过：交还他的跳板，原指令与后续由他那条链负责
		pCode[pos++] = 0x68;
		WriteDword(pCode + pos, chainedTarget);
		pos += 4;
		pCode[pos++] = 0xC3;                               // ret
	}
	else
	{
		// 没人 hook 过：原样执行备份的 len 字节，再跳 addr+len（正好是原指令之后一条）
		memcpy(pCode + pos, original, len);
		pos += len;

		pCode[pos++] = 0xE9;
		int jmpRel = (int)((uintptr_t)(addr + len) - (uintptr_t)(pCode + pos + 4));
		WriteDword(pCode + pos, (DWORD)jmpRel);
		pos += 4;
	}

	// 4. 覆盖目标地址：E9 + 补齐到 len 的 NOP（与 Syringe 的 max(size,5) 一致）
	BYTE patch[MAX_SIZE] = {};
	patch[0] = 0xE9;
	{
		int jmpRel = (int)((uintptr_t)pCode - (uintptr_t)(addr + 5));
		memcpy(patch + 1, &jmpRel, sizeof(jmpRel));
	}
	for (DWORD i = 5; i < len; ++i)
		patch[i] = 0x90;

	if (!PatchCode(addr, patch, len))
	{
		VirtualFree(pBase, 0, MEM_RELEASE);
		return false;
	}

	pEntry->Size = len;
	memcpy(pEntry->Original, original, len);
	pEntry->ChainedTarget = chainedTarget;
	pEntry->Stub = pBase;
	pEntry->Tag = tag;
	pEntry->Active = true;

	Debug::Log("[BeforeHook] 0x%08X 已装：size=0x%X，%s，跳板 0x%08X（%s）\n",
		addr, len,
		bChained ? "接管别人的 jmp" : "无人 hook，回放原字节",
		(DWORD)(uintptr_t)pBase, tag ? tag : "?");
	if (bChained)
		Debug::Log("[BeforeHook] 0x%08X 返回 0 时交还给 0x%08X\n", addr, chainedTarget);

	return true;
}

bool BeforeHook::Uninstall(DWORD addr)
{
	Entry* pEntry = Find(addr);
	if (!pEntry || !pEntry->Active)
		return false;

	// 还原：写回覆盖前的全部 len 字节，不是只还原那条 jmp
	if (!PatchCode(addr, pEntry->Original, pEntry->Size))
	{
		Debug::Log("[BeforeHook] 0x%08X 还原原字节失败，保留跳板不释放\n", addr);
		return false;
	}

	if (pEntry->Stub)
		VirtualFree(pEntry->Stub, 0, MEM_RELEASE);

	Debug::Log("[BeforeHook] 0x%08X 已卸载（%s）\n", addr, pEntry->Tag ? pEntry->Tag : "?");
	pEntry->Stub = nullptr;
	pEntry->Active = false;
	pEntry->Tag = nullptr;
	return true;
}

void BeforeHook::UninstallAll()
{
	for (int i = 0; i < s_Count; ++i)
	{
		if (s_Entries[i].Active)
			Uninstall(s_Entries[i].Addr);
	}
}

bool BeforeHook::IsInstalled(DWORD addr)
{
	Entry* pEntry = Find(addr);
	return pEntry && pEntry->Active;
}

DWORD BeforeHook::GetChainedTarget(DWORD addr)
{
	Entry* pEntry = Find(addr);
	if (pEntry)
		return pEntry->ChainedTarget;

	if (*(const BYTE*)(uintptr_t)addr != 0xE9)
		return 0;

	int rel = 0;
	memcpy(&rel, (const void*)(uintptr_t)(addr + 1), sizeof(rel));
	return addr + 5 + (DWORD)rel;
}
