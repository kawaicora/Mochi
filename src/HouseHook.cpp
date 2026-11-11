#include "HouseHook.h"
#include "Debug.h"
Event<HouseClass*> HouseHook::HouseClassCreateEvent;
DEFINE_HOOK(0x4F6532, HouseClass_CTOR, 5) {
	GET(HouseClass*, pHouseClass, EAX);
	HouseHook::HouseClassCreateEvent.Invoke(pHouseClass);
	return 0;
}
DEFINE_HOOK(0x4F8361, HouseClass_CanBuild, 0x8) {

	/* return
		 1 - Tech shown
		 0 - Tech not shown
		-1 - Tech greyed out
	 */
	GET(HouseClass*, pHouse, ECX);
	GET_STACK(TechnoTypeClass const* const, pItem, 0x4);
	GET_STACK(bool const, buildLimitOnly, 0x8);
	GET_STACK(bool const, includeInProduction, 0xC);
	if (MochiHouse::IsTechUnlocked(pHouse)) {
		R->EAX(1);
	}

	return 0x4F7BEE;
}

// AI 接管（CurrentPlayer->IsHumanPlayer == false）时的胜利判定。
// 原版在 0x4FC585 用"人类玩家数 == 0"判胜；AI 接管会把玩家自己排除出这个计数，
// 于是打掉任意一家就弹胜利。这里改成：非盟友全部消失才算胜利。
int HouseHook::CountAliveNonAlliedHouses()
{
	HouseClass* pMe = HouseClass::CurrentPlayer;

	if (!pMe)
		return 0;

	int count = 0;

	for (auto const& pHouse : HouseClass::Array) {
		if (!pHouse || pHouse == pMe) {
			continue;
		}
		if (pHouse->Defeated) {
			continue;
		}
		if (pHouse->IsObserver()) {
			continue;
		}
		if (pHouse->IsNeutral()) {
			continue;
		}
		if (pMe->IsAlliedWith(pHouse)) {
			continue;
		}
		++count;
	}

	return count;
}

// 0x4FC580  cmp ebx, 1 / je 0x4FC591   (5 bytes)
DEFINE_HOOK(0x4FC580, HouseClass_AcceptDefeat_AIControlWin, 5)
{
	HouseClass* pMe = HouseClass::CurrentPlayer;
	// 玩家自己还在操作：逐字节复刻被覆盖的两条指令
	if (!pMe || pMe->IsHumanPlayer) {
		GET(int, aliveHouses, EBX);
		if (aliveHouses == 1) {
			return 0x4FC591;
		}
		return 0x4FC585;
	}

	// AI 接管：所有非盟友都被清空，才走游戏自带的判胜流程
	if (HouseHook::CountAliveNonAlliedHouses() == 0) {
		return 0x4FC591;
	}

	// 还有非盟友活着：不判，直接走到函数尾部返回
	return 0x4FC6BC;
}

// AI 开关把 IsHumanPlayer / IsInPlayerControl 都翻成 false 后，恢复本机对自己那家的操作权：
// 只看"是不是本机 CurrentPlayer"，不看那两个标志
DEFINE_HOOK(0x50B6F0, HouseClass_IsControlledByCurrentPlayer_AISync, 0x7)
{
	GET(HouseClass*, pThis, ECX);
	// 只对本机自己那家放行，其它家保持原逻辑（IsHumanPlayer || IsInPlayerControl）
	if (pThis == HouseClass::CurrentPlayer) {
		return 0x50B716;               // cmp ecx,[0xA83D4C] / sete al / ret
	}
	if (*(DWORD*)0xA8B238) {
		R->AL(pThis->IsHumanPlayer ? 1 : 0);
		return 0x50B752;               // ret
	}
	if (pThis->IsHumanPlayer) {
		R->AL(1);
		return 0x50B752;
	}
	return 0x50B743;                   // 回落：mov al,[ecx+0x1ED] 继续原逻辑
}

// 两个标志都翻 false 后，引擎原有的 IsHumanPlayer || IsInPlayerControl 会把"对面那家"的
// 所有事件整家跳过（含 CoraSuperWeaponCharge），导致对面超武在本机永远充不满 → 各玩各的。
// 事件必须无条件执行，才和 Mochi 的“每家事件两端都跑一遍”设计一致。
DEFINE_HOOK(0x64C4FB, DoList_HouseFilter_AISync, 0x18)
{
	return 0x64C513;
}
// 诊断（安全挂法）：0x4FAE58 是 6 字节的 mov eax,[ebx+0x258]，正好 >= 5 字节 jmp，
// 这里自己补回这条指令（R->EAX），再回 0x4FAE5E，不动栈
DEFINE_HOOK(0x4FAE58, HouseClass_Fire_SW_AISyncLog, 0x6)
{
	GET(HouseClass*, pThis, EBX);
	GET(int, idx, ECX);
	R->EAX(*reinterpret_cast<DWORD*>(reinterpret_cast<char*>(pThis) + 0x258));
	Debug::Log("[AISync] FireSW f=%d house=%d super=%d human=%d inpc=%d me=%d\n",
		Unsorted::CurrentFrame, pThis->ArrayIndex, idx,
		pThis->IsHumanPlayer ? 1 : 0, pThis->IsInPlayerControl ? 1 : 0,
		HouseClass::CurrentPlayer ? HouseClass::CurrentPlayer->ArrayIndex : -1);
	return 0x4FAE5E;
}
