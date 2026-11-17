#include "MochiSuper.h"
#include <ASMMacros.h>
#include <YRPP.h>

// ---------------------------------------------------------------------------
// Ares 原函数（src\Ext\SWType\Hooks.Update.cpp::GetSuperWeaponStatuses）里用到的
// 四处 Ext 数据，在 Mochi 侧没有对应实现，等价替换如下：
//
//   TechnoExt::ExtMap.Find(pBld)->IsOperated()
//       -> MochiSuper::IsOperated(pBld)
//          Ares 的语义：配置了 Operator= 就要车里坐着对应兵，配置了"谁都能坐"就要有乘客，
//          两者都没配置就恒为 true。Operator 是 Ares 独有 ini，原生 YR 无此字段，
//          因此 Mochi 侧等价于"未配置 Operator=" 的分支：恒 true。
//
//   BuildingTypeExt::ExtData::GetSuperWeaponCount()
//       -> 2（原生只有 BuildingTypeClass::SuperWeapon / SuperWeapon2 两个槽）
//          Ares 是 2 + SuperWeapons.size()，SuperWeapons 是 Ares 独有 ini 列表。
//
//   BuildingTypeExt::ExtData::GetSuperWeaponIndex(i, pHouse)
//       -> 原生 SuperWeapon / SuperWeapon2 两槽；原本还会过一遍 SWTypeExt::IsAvailable(pHouse)，
//          那是 Ares 私有可用性过滤，Mochi 取不到，这里保留"取不到就不过滤"的 Ares 原有分支。
//
//   BuildingExt::ExtData::GetSuperWeaponCount() / GetSuperWeaponIndex(i)
//       -> 直接调类型级的那两个函数（Ares 的实现本身就是转发到 BuildingTypeExt）。
//
// 其余判定（IsAlive / InLimbo / HasPower / IsUnderEMP / IsBeingWarpedOut /
// CurrentMission / QueuedMission / HasFullPower / DisableableFromShell /
// HasFullPower / IsPowered）全部是原生字段或虚函数，逐句保留。
// ---------------------------------------------------------------------------

size_t MochiSuper::GetSuperWeaponCount(BuildingTypeClass* pType)
{
	if (!pType)
		return 0;

	// Ares: 2 + this->SuperWeapons.size()
	return 2;
}

int MochiSuper::GetSuperWeaponIndex(BuildingTypeClass* pType, size_t index)
{
	if (!pType)
		return -1;

	// Ares: index < 2 ? (index ? SuperWeapon2 : SuperWeapon) : SuperWeapons[index - 2]
	if (index == 0)
		return pType->SuperWeapon;
	if (index == 1)
		return pType->SuperWeapon2;

	return -1;
}

int MochiSuper::GetSuperWeaponIndex(BuildingTypeClass* pType, size_t index, HouseClass* pHouse)
{
	const int idxSW = GetSuperWeaponIndex(pType, index);

	if (!pHouse)
		return idxSW;

	// Ares: 若 pHouse->Supers.GetItemOrDefault(idxSW) 非空，再过 SWTypeExt::IsAvailable(pHouse)。
	// 取不到 / 没有 Ext 时，Ares 同样是直接返回 idxSW，这里保持一致。
	return idxSW;
}

bool MochiSuper::IsOperated(TechnoClass* pTechno)
{
	// 见文件头注释：原生 YR 没有 Operator=，等价于 Ares 未配置 Operator= 时的恒 true 分支。
	return pTechno != nullptr;
}

void MochiSuper::UpdateStatus(BuildingClass* pBld, std::vector<SWStatus>& Statuses, int idxSW)
{
	if (!pBld || idxSW < 0 || idxSW >= (int)Statuses.size())
		return;

	auto& status = Statuses[idxSW];
	status.Available = true;

	if (!status.Charging
		&& pBld->HasPower
		&& !pBld->IsUnderEMP()
		&& IsOperated(pBld))
	{
		status.PowerSourced = true;

		if (!pBld->IsBeingWarpedOut()
			&& pBld->CurrentMission != Mission::Construction
			&& pBld->CurrentMission != Mission::Selling
			&& pBld->QueuedMission != Mission::Construction
			&& pBld->QueuedMission != Mission::Selling)
		{
			status.Charging = true;
		}
	}
}

std::vector<SWStatus> MochiSuper::GetSuperWeaponStatuses(HouseClass* pHouse)
{
	// Ares: std::vector<SWStatus> Statuses(pHouse->Supers.Count, { false, false, false });
	std::vector<SWStatus> Statuses;
	if (!pHouse)
		return Statuses;

	Statuses.assign(static_cast<size_t>(pHouse->Supers.Count > 0 ? pHouse->Supers.Count : 0),
		SWStatus{ false, false, false });

	// look at every sane building this player owns, if it is not defeated already.
	if (!pHouse->Defeated)
	{
		for (auto pBld : pHouse->Buildings)
		{
			if (!pBld || !pBld->IsAlive || pBld->InLimbo)
				continue;

			// check for upgrades. upgrades can give super weapons, too.
			// 原生 Upgrades 是 BuildingTypeClass*[3] 的定长数组，可能带空槽。
			for (int u = 0; u < 3; ++u)
			{
				BuildingTypeClass* pUpgrade = pBld->Upgrades[u];
				if (!pUpgrade)
					continue;

				const size_t count = GetSuperWeaponCount(pUpgrade);
				for (size_t i = 0; i < count; ++i)
					UpdateStatus(pBld, Statuses, GetSuperWeaponIndex(pUpgrade, i, pHouse));
			}

			// look for the main building.
			const size_t count = GetSuperWeaponCount(pBld->Type);
			for (size_t i = 0; i < count; ++i)
				UpdateStatus(pBld, Statuses, GetSuperWeaponIndex(pBld->Type, i, pHouse));
		}

		// kill off super weapons that are disallowed and factor in the player's power status
		const bool hasPower = pHouse->HasFullPower();

		for (auto pSuper : pHouse->Supers)
		{
			if (!pSuper || !pSuper->Type)
				continue;

			const int index = pSuper->Type->ArrayIndex;
			if (index < 0 || index >= (int)Statuses.size())
				continue;

			auto& status = Statuses[index];

			// turn off super weapons that are disallowed.
			if (SessionClass::Instance.GameMode != GameMode::Campaign
				&& !GameModeOptionsClass::Instance.SWAllowed)
			{
				if (pSuper->Type->DisableableFromShell)
					status.Available = false;
			}

			// if the house is generally on low power, powered super weapons aren't powered
			if (pSuper->IsPowered())
				status.PowerSourced &= hasPower;
		}
	}

	return Statuses;
}

signed int __fastcall MochiSuper::GetCameoChargeState(SuperClass* pThis)
{
	JMP_STD(0x6CBEE0);
}
