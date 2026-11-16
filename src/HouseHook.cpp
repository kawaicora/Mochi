#include "HouseHook.h"
#include "Debug.h"
#include <MochiINI.h>
#include "MochiEvent.h"
Event<HouseClass*> HouseHook::HouseClassCreateEvent;
Event<HouseClass*> HouseHook::HouseClassDestoryEvent;
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


DEFINE_HOOK(0x4F7371, HouseClass_DTOR, 0x6)
{
	GET(HouseClass*, pItem, ESI);

	HouseHook::HouseClassDestoryEvent.Invoke(pItem);

	return 0;
}





//****************************超武部分********************************************

DEFINE_HOOK(0x4F92DD, HouseClass_Update_HideSupersBeforeAres, 5)
{
	GET(HouseClass*, pThis, ESI);

	if (pThis && !pThis->Defeated && MochiEvent::GetHouseActiveAllSuperWeaponFlag(pThis))
	{
		for (auto pSuper : pThis->Supers)
			if (pSuper) pSuper->IsPresent = false;   // 只在这两次调用之间"藏起来"
	}

	return 0;   // 执行被搬走的 mov ecx,0x87F7E8
}



Event<HouseClass*> HouseHook::HouseUpdateSuperAfterEvent;

DEFINE_HOOK(0x50B1CA, HouseClass_HouseUpdateSuperAfterEx, 5)   //用Ares的返回地址继续
{
	GET(HouseClass*, pThis, ECX);

	HouseHook::HouseUpdateSuperAfterEvent.Invoke(pThis);
	return 0x0;
}
//****************************超武部分结束********************************************

//
//DEFINE_HOOK(0x50B36E, HouseClass_UpdateSuperWeaponsUnavailableEX, 5)
//{
//	GET(HouseClass*, pThis, ECX);
//
//	if (!pThis->Defeated) {
//		SuperHook::HouseUpdateSuperAfterEvent.Invoke(pThis);
//	}
//	return 0;
//}

//**********************************AI 胜利判定*************************************************
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


//**********************************AI 胜利判定部分结束*************************************************

