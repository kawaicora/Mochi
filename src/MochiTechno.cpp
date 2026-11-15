
#include <MochiTechno.h>
#include <Debug.h>
int MochiTechno::GetIndex(TechnoTypeClass* pTechnoTypeClass) {
	AbstractType abs = pTechnoTypeClass->WhatAmI();
	switch (abs) {
	case AbstractType::Unit:
	case AbstractType::UnitType:
	{
		auto const pTypeClass = abstract_cast<UnitTypeClass*>(pTechnoTypeClass);
		return	pTypeClass->ArrayIndex;

	}

	case AbstractType::Aircraft:
	case AbstractType::AircraftType:
	{
		auto const pTypeClass = abstract_cast<AircraftTypeClass*>(pTechnoTypeClass);
		return	pTypeClass->ArrayIndex;
	}

	case AbstractType::Infantry:
	case AbstractType::InfantryType:
	{
		auto const pTypeClass = abstract_cast<InfantryTypeClass*>(pTechnoTypeClass);
		return	pTypeClass->ArrayIndex;
	}

	case AbstractType::Building:
	case AbstractType::BuildingType:
	{
		auto const pTypeClass = abstract_cast<BuildingTypeClass*>(pTechnoTypeClass);

		return	pTypeClass->ArrayIndex;


	}

	default:
		return -1;
	}

};
FactoryClass* MochiTechno::GetFactoryByTechnoTypeForNoOnlineAI(HouseClass* pHouse, TechnoTypeClass* pTechnoTypeClass) {
	for (BuildingClass* building : pHouse->Buildings) {

		if (!building->Factory) continue;
		if (!building->Factory->Owner) continue;
		if (building->Factory->Owner != pHouse) continue;
		if (!building->Factory->Object) continue;
		if (!building->Factory->Object->GetTechnoType()) continue;
		if (building->Factory->Object->GetTechnoType() != pTechnoTypeClass) continue;
		return building->Factory;

	}
	return nullptr;
};
FactoryClass* MochiTechno::GetFactoryByTechnoTypeClass(HouseClass* pHouse, TechnoTypeClass* pTechnoTypeClass) {
	FactoryClass* pFactory;
	if (!pHouse) {
		return nullptr;
	}
	AbstractType abs = pTechnoTypeClass->WhatAmI();
	switch (abs)
	{
	case AbstractType::Unit:
	case AbstractType::UnitType:
	{
		if (pTechnoTypeClass->Naval) {
			pFactory = pHouse->Primary_ForShips;
		}
		else {
			pFactory = pHouse->Primary_ForVehicles;
		}
		return pFactory;
	}
	case AbstractType::Aircraft:
	case AbstractType::AircraftType:
	{
		pFactory = pHouse->Primary_ForAircraft;
		return pFactory;
	}

	case AbstractType::Infantry:
	case AbstractType::InfantryType:
	{
		pFactory = pHouse->Primary_ForInfantry;
		return pFactory;
	}

	case AbstractType::Building:
	case AbstractType::BuildingType:
	{
		if (const auto pBuildingType = abstract_cast<BuildingTypeClass*>(pTechnoTypeClass)) {

			if (pBuildingType->BuildCat == BuildCat::Combat) {
				pFactory = pHouse->Primary_ForDefenses;
			}
			else {
				pFactory = pHouse->Primary_ForBuildings;
			}
			return pFactory;
		}
		else {
			return nullptr;
		}
	}

	default:
	{
		Debug::Log("*** Warning ***\n");
		Debug::Log("Can't Process This  RTTIType %d \n", (int)abs);
		return nullptr;
	}
	}
};

bool MochiTechno::PlaceTechnoAtMap(TechnoClass* pTechno, CellStruct location)
{
	if (!pTechno)
	{
		return false;
	}

	TechnoTypeClass* pType = pTechno->GetTechnoType();
	HouseClass* pHouse = pTechno->Owner;

	if (!pType || !pHouse)
	{
		pTechno->UnInit();
		return false;
	}

	const CellStruct requested = location;

	const auto IsCoordLegal = [](CellStruct* pCell) -> bool
		{
			typedef bool(__thiscall* Fn)(MapClass*, CellStruct*);
			return reinterpret_cast<Fn>(0x568300)(&MapClass::Instance, pCell);
		};

	const auto FetchCell = [](CellStruct* pCell) -> CellClass*
		{
			typedef CellClass* (__thiscall* Fn)(MapClass*, CellStruct*);
			return reinterpret_cast<Fn>(0x5657A0)(&MapClass::Instance, pCell);
		};

	BuildingClass* pAsBuilding = abstract_cast<BuildingClass*>(pTechno);
	BuildingTypeClass* pBuildingType = abstract_cast<BuildingTypeClass*>(pType);

	// ---- 地形要求：0 = 不限（飞行 / 建筑），1 = 必须陆地，2 = 必须水面 ----
	// 只用 TechnoTypeClass 的静态字段，不使用 InWhichLayer() 这类运行时状态
	int requiredTerrain = 0;

	if (!pAsBuilding)
	{
		const bool bAirborne =
			pType->JumpJet
			|| pType->BalloonHover
			|| pType->MovementZone == MovementZone::Fly;

		const bool bNaval =
			pType->Naval
			|| pType->MovementZone == MovementZone::Water
			|| pType->MovementZone == MovementZone::WaterBeach;

		if (bAirborne)
		{
			requiredTerrain = 0;      // 会飞：海里原地创建
		}
		else if (bNaval)
		{
			requiredTerrain = 2;      // 海军：只要水
		}
		else
		{
			requiredTerrain = 1;      // 其余一律陆地（不再有两栖特例）
		}
	}

	const auto TerrainOk = [&](CellClass* pCell) -> bool
		{
			if (requiredTerrain == 0)
			{
				return true;
			}
			if (requiredTerrain == 2)
			{
				return pCell->Tile_Is_Water();
			}
			return !pCell->Tile_Is_Water();
		};

	// ---- 单格判据：地形 + 占用（占用与 Unlimbo 内部 [vtbl+0x1AC] 同一问询）----
	const auto IsPlaceable = [&](CellStruct cell, CellClass** ppOut) -> bool
		{
			if (ppOut)
			{
				*ppOut = nullptr;
			}

			if (!IsCoordLegal(&cell))
			{
				return false;
			}

			CellClass* pCell = FetchCell(&cell);
			if (!pCell)
			{
				return false;
			}

			if (pAsBuilding)
			{
				if (!pBuildingType)
				{
					return false;
				}
				// 0x464AC0：按 footprint 逐格走 0x4834A0（地形 + 建筑脚印）
				if (!pBuildingType->CanPlaceHere(&cell, pHouse))
				{
					return false;
				}
			}
			else
			{
				if (!TerrainOk(pCell))
				{
					return false;
				}
				if (pTechno->IsCellOccupied(
					pCell, FacingType::None, -1, nullptr, false) != Move::OK)
				{
					return false;
				}
			}

			if (ppOut)
			{
				*ppOut = pCell;
			}
			return true;
		};

	CellStruct target = requested;
	CellClass* pTargetCell = nullptr;




	if (!IsPlaceable(target, &pTargetCell))
	{
		const int kMaxRadius = 64;         // 环搜索半径上限（格）
		const int kScanBudget = 20000;     // 单次调用最多考察的格子数

		int scanned = 0;
		bool bFound = false;

		for (int radius = 1; radius <= kMaxRadius && !bFound && scanned < kScanBudget; ++radius)
		{
			for (int dy = -radius; dy <= radius && !bFound; ++dy)
			{
				for (int dx = -radius; dx <= radius; ++dx)
				{
					if (dy > -radius && dy < radius && dx > -radius && dx < radius)
					{
						continue;   // 只走环，不扫内部
					}

					++scanned;

					CellStruct cell = {
						(short)(requested.X + dx),
						(short)(requested.Y + dy)
					};

					if (IsPlaceable(cell, &pTargetCell))
					{
						target = cell;
						bFound = true;
						break;
					}
				}
			}
		}
		if (!bFound)
		{
			Debug::Log("PlaceTechnoAtMap: no legal cell for %s near (%d,%d) req=%d\n",
				pType->ID, requested.X, requested.Y, requiredTerrain);
			pTechno->UnInit();
			return false;
		}

		Debug::Log("PlaceTechnoAtMap: %s moved (%d,%d) -> (%d,%d) req=%d\n",
			pType->ID, requested.X, requested.Y,
			target.X, target.Y, requiredTerrain);
	}

	if (!pTargetCell)
	{
		pTargetCell = FetchCell(&target);
		if (!pTargetCell)
		{
			Debug::Log("PlaceTechnoAtMap: no cell at (%d,%d)\n", target.X, target.Y);
			pTechno->UnInit();
			return false;
		}
	}

	// ---- 落图前最后一道校验：地形与 required 不一致直接拒，不静默放行 ----
	if (!pAsBuilding && requiredTerrain != 0)
	{
		const bool bWater = pTargetCell->Tile_Is_Water();
		if (bWater != (requiredTerrain == 2))
		{
			Debug::Log("PlaceTechnoAtMap: terrain mismatch %s req=%d water=%d at (%d,%d)\n",
				pType->ID, requiredTerrain, (int)bWater, target.X, target.Y);
			pTechno->UnInit();
			return false;
		}
	}

	pTechno->OnBridge = pTargetCell->ContainsBridge();
	CoordStruct coord = pTargetCell->GetCoordsWithBridge();

	// ---- 建筑：引擎 TAction 125 Build At 原序 ----
	if (BuildingClass* pBuilding = abstract_cast<BuildingClass*>(pTechno))
	{
		pBuilding->QueueMission(Mission::Construction, false);
		pBuilding->NextMission();

		if (!pBuilding->ForceCreate(coord))
		{
			Debug::Log("ForceCreate failed: %s at (%d,%d)\n",
				pType->ID, target.X, target.Y);
			pBuilding->UnInit();
			return false;
		}

		pBuilding->BeginMode(BStateType::Idle);
		pBuilding->QueueMission(Mission::Guard, false);
		pBuilding->NextMission();
		pBuilding->Place(false);

		Debug::Log("Placed building %s at (%d,%d) Occ=%02X\n",
			pType->ID, target.X, target.Y, pTargetCell->OccupationFlags);
		return true;
	}

	// ---- 单位 / 步兵 / 空军：引擎 Unlimbo，不包 Unsorted::ScenarioInit ----
	if (!pTechno->Unlimbo(coord, DirType::East))
	{
		Debug::Log("Unlimbo failed: %s at (%d,%d) req=%d water=%d\n",
			pType->ID, target.X, target.Y,
			requiredTerrain, (int)pTargetCell->Tile_Is_Water());
		pTechno->UnInit();
		return false;
	}

	if (pType->JumpJet || pType->BalloonHover)
	{
		pTechno->Scatter(CoordStruct::Empty, true, false);
	}

	Debug::Log("Placed %s at (%d,%d) req=%d water=%d Occ=%02X\n",
		pType->ID, target.X, target.Y,
		requiredTerrain, (int)pTargetCell->Tile_Is_Water(),
		pTargetCell->OccupationFlags);
	return true;
}




bool MochiTechno::PlaceTechnoAtMap(TechnoTypeClass* pTechnoType, CellStruct location, HouseClass* pHouse) {
	ObjectClass* obj = pTechnoType->CreateObject(pHouse);
	if (!obj) {
		Debug::LogW(L"Failed To Create Object By TechnoType: %ls\n", pTechnoType->UIName);
		return false;
	}
	if (TechnoClass* pTechno = abstract_cast<TechnoClass*>(obj)) {
		return MochiTechno::PlaceTechnoAtMap(pTechno, location);

	}
	else {
		Debug::LogW(L"Failed To Cast Object To TechnoClass. Object TechnoType: %ls\n", obj->GetTechnoType()->UIName);
		obj->UnInit();
		return false;
	}
};

bool MochiTechno::PlaceTechnoAtMap(const char* id, CellStruct location, HouseClass* pHouse) {
	TechnoTypeClass* pTechnoType = TechnoTypeClass::Find(id);
	ObjectClass* obj = pTechnoType->CreateObject(pHouse);
	if (!obj) {
		Debug::LogW(L"Failed To Create Object By TechnoType: %ls\n", pTechnoType->UIName);
		return false;
	}
	if (TechnoClass* pTechno = abstract_cast<TechnoClass*>(obj)) {
		return MochiTechno::PlaceTechnoAtMap(pTechno, location);

	}
	else {
		Debug::LogW(L"Failed To Cast Object To TechnoClass. Object TechnoType: %ls\n", obj->GetTechnoType()->UIName);
		obj->UnInit();
		return false;
	}
};