#pragma once
#include <YRPP.h>
#include <MapHook.h>
#include <Debug.h>
class TechnoHook
{
public:
	static int GetIndex(TechnoTypeClass* pTechnoTypeClass) {
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

	static FactoryClass* GetFactoryByTechnoTypeClass(HouseClass* pHouse, TechnoTypeClass* pTechnoTypeClass) {
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
				} else {
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


	static bool PlaceTechnoAtMap(TechnoClass* pTechno, CellStruct location) {
		TechnoTypeClass* pTechnoType = pTechno->GetTechnoType();
		HouseClass* pHouse = pTechno->Owner;
		if (CellClass* pCell = MapClass::Instance.TryGetCellAt(location)) {
			if (!MapHook::IsValidLandCell(pCell, pTechnoType))
			{
				pCell =
					MapHook::FindNearestValidLandCell(
						location,
						pTechnoType);
				if (!pCell)
				{
					Debug::LogW(
						L"Failed To Find Valid Cell (%d,%d)\n",
						location.X,
						location.Y);
					pTechno->UnInit();
					return false;
				}
				location =
					pCell->MapCoords;
			}
			pTechno->OnBridge = pCell->ContainsBridge();
			CoordStruct coord = pCell->GetCoordsWithBridge();
			++Unsorted::ScenarioInit;
			bool result = pTechno->Unlimbo(coord, DirType::East);
			--Unsorted::ScenarioInit;
			if (!result) {
				Debug::LogW(L"Failed To Place Object At Location (%d,%d). Object TechnoType: %ls\n",location.X, location.Y, pTechnoType->UIName);
				pTechno->UnInit();
				return false;
			}
			if (BuildingClass* pBuilding = abstract_cast<BuildingClass*, true>(pTechno)) {
				if (pBuilding->Type->Foundation != Foundation::_0x0) {
					pBuilding->QueueMission(Mission::Construction, false);
				}
				pBuilding->DiscoveredBy(pHouse);
				pBuilding->IsReadyToCommence = true;
			}
			else if (pTechnoType->JumpJet || pTechnoType->BalloonHover) {
				pTechno->Scatter(CoordStruct::Empty, true, false);
			}
			return true;
		}
		else {
			Debug::LogW(L"Failed To Get Cell At Location (%d,%d). Object TechnoType: %ls\n", location.X, location.Y, pTechnoType->UIName);
			return false;
		}
	};
	static bool PlaceTechnoAtMap(TechnoTypeClass* pTechnoType, CellStruct location,HouseClass* pHouse) {
		ObjectClass* obj = pTechnoType->CreateObject(pHouse);
		if (!obj) {
			Debug::LogW(L"Failed To Create Object By TechnoType: %ls\n", pTechnoType->UIName);
			return false;
		}
		if (TechnoClass* pTechno = abstract_cast<TechnoClass*>(obj)) {
			return PlaceTechnoAtMap(pTechno, location);

		}
		else {
			Debug::LogW(L"Failed To Cast Object To TechnoClass. Object TechnoType: %ls\n", obj->GetTechnoType()->UIName);
			obj->UnInit();
			return false;
		}
	};

	static bool PlaceTechnoAtMap(const char* id, CellStruct location, HouseClass* pHouse) {
		TechnoTypeClass* pTechnoType = TechnoTypeClass::Find(id);
		ObjectClass* obj = pTechnoType->CreateObject(pHouse);
		if (!obj) {
			Debug::LogW(L"Failed To Create Object By TechnoType: %ls\n", pTechnoType->UIName);
			return false;
		}
		if (TechnoClass* pTechno = abstract_cast<TechnoClass*>(obj)) {
			return PlaceTechnoAtMap(pTechno, location);

		}
		else {
			Debug::LogW(L"Failed To Cast Object To TechnoClass. Object TechnoType: %ls\n", obj->GetTechnoType()->UIName);
			obj->UnInit();
			return false;
		}
	};
};

