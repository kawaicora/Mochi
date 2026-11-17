#include "HouseHook.h"
#include "Debug.h"
#include <MochiINI.h>
#include "MochiEvent.h"
#include "MochiMacro.h"
#include "MochiSuper.h"
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
Event<HouseClass*> HouseHook::HouseUpdateSuperAfterEvent;

BEFORE_HOOK(0x50AF10, HouseClass_UpdateSuperWeaponsOwned, 5)
{
	GET(HouseClass*, pThis, ECX);

	// 全超武模式：不信 Ares 的这段，直接返回它的返回地址（BEFORE_HOOK 返回非 0 地址
	// = 直接跳走，Ares 的 GetSuperWeaponStatuses + Lose 整段都不会执行）
	if (pThis && !pThis->Defeated && MochiEvent::GetHouseActiveAllSuperWeaponFlag(pThis)) {
		HouseHook::HouseUpdateSuperAfterEvent.Invoke(pThis);
		return 0x50B1CA;
	}

	// 普通模式：逐句复刻 Ares。status 只在下面用，放到这里算，省一帧一次的全建筑遍历。
	auto Statuses = MochiSuper::GetSuperWeaponStatuses(pThis);
	// now update every super weapon that is valid.
	// if this weapon has not been granted there's no need to update
	for (auto pSuper : pThis->Supers) {
		if (pSuper->IsPresent) {
			auto pType = pSuper->Type;
			auto index = pType->ArrayIndex;
			auto& status = Statuses[index];

			// is this a super weapon to be updated?
			// sw is bound to a building and no single-shot => create goody otherwise
			bool isCreateGoody = (!pSuper->CanHold || pSuper->IsOneTime);
			if (!isCreateGoody || pThis->Defeated) {

				// shut down or power up super weapon and decide whether
				// a sidebar tab update is needed.
				bool update = false;
				if (!status.Available || pThis->Defeated) {
					update = (pSuper->Lose() && HouseClass::CurrentPlayer);
				}
				else if (status.Charging && !pSuper->IsPowered()) {
					// Ares: pSuper->IsOnHold —— 就是 SuperClass+0x70（SetOnHold 读写的就是这个字节，
					// 见 sub_6CB4D0.c），Mochi YRpp 里叫 IsSuspended。CanHold 是 +0x60，不是这个意思。
					update = pSuper->IsSuspended && pSuper->SetOnHold(false);
				}
				else if (!status.Charging && !pSuper->IsPowered()) {
					update = !pSuper->IsSuspended && pSuper->SetOnHold(true);
				}
				else if (!status.PowerSourced) {
					update = (pSuper->IsPowered() && pSuper->SetOnHold(true));
				}
				else {
					update = (status.PowerSourced && pSuper->SetOnHold(false));
				}

				// update only if needed.
				if (update) {
					// only the human player can see the sidebar.
					if (pThis->IsCurrentPlayer()) {
						if (Unsorted::CurrentSWType == index) {
							Unsorted::CurrentSWType = -1;
						}
						int idxTab = SidebarClass::GetObjectTabIdx(SuperClass::AbsID, index, 0);
						MouseClass::Instance.RepaintSidebar(idxTab);
					}
					pThis->RecheckTechTree = true;
				}
			}
		}
	}

	return 0x50B1CA;
}
//
// 0x50B1D0（Ares 的 pass 2 / UpdateSuperWeaponsUnavailable）这里不加任何钩子：
//   - 用 DEFINE_HOOK 挂同址，永远拿不到控制权（Ares 那个钩子返回 0x50B36E，非 0 直接 jmp 走，
//     同址后面的钩子整条链都不执行）；
//   - 用 BEFORE_HOOK 接管，就得自己完整复刻 Ares 的 pass 2，而现在不需要：
//     全超武模式下 MochiEvent 已把 IsPresent 全置 true，Ares pass 2 的 !IsPresent 条件不成立；
//     普通模式下 Ares pass 2 本来就是我们要的行为。
// 同理 0x4F92DD / 0x50B1CA 那两条"把 IsPresent 补回来"的钩子也不再需要：
// 它们原本是为了救 Ares pass 1 里的 Lose()，而现在 BEFORE_HOOK 返回 0x50B1CA 表示
// Ares pass 1 整段不执行，压根没有 Lose 发生过。



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

