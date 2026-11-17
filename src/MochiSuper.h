#pragma once
#include <SuperClass.h>
#include <vector>

class HouseClass;
class BuildingClass;
class BuildingTypeClass;
class TechnoClass;

// 与 Ares 的 SWStatus 同构（字段顺序一致）
struct SWStatus
{
	bool Available;
	bool PowerSourced;
	bool Charging;
};

class MochiSuper
{
public:
	static signed int __fastcall GetCameoChargeState(SuperClass* pThis);

	// Ares src\Ext\SWType\Hooks.Update.cpp::GetSuperWeaponStatuses 的等价实现。
	// 逻辑逐句一致；Ares 的 Ext 数据（BuildingTypeExt / BuildingExt / TechnoExt / SWTypeExt）
	// 在 Mochi 里没有，替换规则见 .cpp 头部注释与各处行内标注。
	static std::vector<SWStatus> GetSuperWeaponStatuses(HouseClass* pHouse);

	// 等价于 BuildingTypeExt::ExtData::GetSuperWeaponCount()
	static size_t GetSuperWeaponCount(BuildingTypeClass* pType);

	// 等价于 BuildingTypeExt::ExtData::GetSuperWeaponIndex(index)
	static int GetSuperWeaponIndex(BuildingTypeClass* pType, size_t index);

	// 等价于 BuildingTypeExt::ExtData::GetSuperWeaponIndex(index, pHouse)
	static int GetSuperWeaponIndex(BuildingTypeClass* pType, size_t index, HouseClass* pHouse);

	// 等价于 TechnoExt::ExtData::IsOperated()
	static bool IsOperated(TechnoClass* pTechno);

	// Ares 里 GetSuperWeaponStatuses 内那个 UpdateStatus lambda 的等价物
	static void UpdateStatus(BuildingClass* pBld, std::vector<SWStatus>& Statuses, int idxSW);
};
