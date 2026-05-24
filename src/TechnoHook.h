#pragma once

#include <HouseClass.h>
class TechnoHook
{
public:
	static int GetIndex(AbstractType abs, TechnoTypeClass* pTechnoTypeClass) {
		
		
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
		
	}
};

