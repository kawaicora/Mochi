
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
	if (!pTechno) return false;

	TechnoTypeClass* pType = pTechno->GetTechnoType();
	HouseClass* pHouse = pTechno->Owner;
	if (!pType || !pHouse) { pTechno->UnInit(); return false; }

	const auto IsCoordLegal = [](CellStruct* p) -> bool {
		typedef bool(__thiscall* Fn)(MapClass*, CellStruct*);
		return reinterpret_cast<Fn>(0x568300)(&MapClass::Instance, p);
		};
	const auto FetchCell = [](CellStruct* p) -> CellClass* {
		typedef CellClass* (__thiscall* Fn)(MapClass*, CellStruct*);
		return reinterpret_cast<Fn>(0x5657A0)(&MapClass::Instance, p);
		};

	// 0 = 不限（飞行/两栖）、1 = 必须陆地、2 = 必须水面
	int requiredTerrain = 1;
	const Layer layer = pTechno->InWhichLayer();
	BuildingClass* pIsBuilding = abstract_cast<BuildingClass*>(pTechno);

	if (!pIsBuilding)
	{
		if (layer == Layer::Air || pType->JumpJet || pType->BalloonHover)
			requiredTerrain = 0;      // 会飞：海里原地创建
		else if (pType->Naval)
			requiredTerrain = 2;      // 船：只要水
		else switch (pType->MovementZone)
		{
		case MovementZone::Amphibious:
		case MovementZone::AmphibiousCrusher:
		case MovementZone::AmphibiousDestroyer:
		case MovementZone::WaterBeach:
			requiredTerrain = 0; break;
		default:
			requiredTerrain = 1; break;   // 陆军：不要水
		}
	}

	const auto TerrainOk = [&](CellClass* pCell) -> bool {
		if (requiredTerrain == 0) return true;
		if (requiredTerrain == 2) return pCell->Tile_Is_Water();
		return !pCell->Tile_Is_Water();
		};

	const auto IsPlaceable = [&](CellStruct cell, CellClass** ppOut) -> bool {
		if (ppOut) *ppOut = nullptr;
		if (!IsCoordLegal(&cell)) return false;
		CellClass* pCell = FetchCell(&cell);
		if (!pCell) return false;

		if (pIsBuilding)
		{
			BuildingTypeClass* pBT = abstract_cast<BuildingTypeClass*>(pType);
			if (!pBT) return false;
			if (!pBT->CanPlaceHere(&cell, pHouse)) return false;   // 0x464AC0
		}
		else
		{
			if (!TerrainOk(pCell)) return false;
			if (pTechno->IsCellOccupied(pCell, FacingType::None, -1, nullptr, false)
				!= Move::OK) return false;                     // 与 Unlimbo 同一闸
		}

		if (ppOut) *ppOut = pCell;
		return true;
		};

	CellStruct target = location;
	CellClass* pTargetCell = nullptr;

	if (!IsPlaceable(target, &pTargetCell))
	{
		bool bFound = false;
		for (int radius = 1; radius <= 8 && !bFound; ++radius)
			for (int dy = -radius; dy <= radius && !bFound; ++dy)
				for (int dx = -radius; dx <= radius; ++dx)
				{
					if (dy > -radius && dy < radius && dx > -radius && dx < radius)
						continue;
					CellStruct cell = { (short)(location.X + dx), (short)(location.Y + dy) };
					if (IsPlaceable(cell, &pTargetCell))
					{
						location = cell; target = cell; bFound = true; break;
					}
				}

		if (!bFound)
		{
			Debug::Log("PlaceTechnoAtMap: no legal cell for %s near (%d,%d)\n",
				pType->ID, location.X, location.Y);
			pTechno->UnInit();
			return false;
		}

		Debug::Log("PlaceTechnoAtMap: %s moved (%d,%d) -> (%d,%d)\n",
			pType->ID, location.X, location.Y, target.X, target.Y);
	}

	if (!pTargetCell) pTargetCell = FetchCell(&target);
	if (!pTargetCell)
	{
		Debug::Log("PlaceTechnoAtMap: no cell at (%d,%d)\n", target.X, target.Y);
		pTechno->UnInit();
		return false;
	}

	pTechno->OnBridge = pTargetCell->ContainsBridge();
	CoordStruct coord = pTargetCell->GetCoordsWithBridge();

	if (BuildingClass* pBuilding = abstract_cast<BuildingClass*>(pTechno))
	{
		pBuilding->QueueMission(Mission::Construction, false);
		pBuilding->NextMission();
		if (!pBuilding->ForceCreate(coord))
		{
			Debug::Log("ForceCreate failed: %s at (%d,%d)\n", pType->ID, target.X, target.Y);
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

	if (!pTechno->Unlimbo(coord, DirType::East))
	{
		Debug::Log("Unlimbo failed: %s at (%d,%d) Layer=%d Naval=%d\n",
			pType->ID, target.X, target.Y, (int)layer, (int)pType->Naval);
		pTechno->UnInit();
		return false;
	}

	if (pType->JumpJet || pType->BalloonHover)
		pTechno->Scatter(CoordStruct::Empty, true, false);

	Debug::Log("Placed %s at (%d,%d) Occ=%02X\n",
		pType->ID, target.X, target.Y, pTargetCell->OccupationFlags);
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