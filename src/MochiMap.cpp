#include "MochiMap.h"


bool MochiMap::IsCellOccupationOccupied(CellClass* pCell)
{
	return pCell->OccupationFlags & 0x1 || pCell->OccupationFlags & 0x2 || pCell->OccupationFlags & 0x4 || pCell->OccupationFlags & 0x8 || pCell->OccupationFlags & 0x10 || pCell->OccupationFlags & 0x20 || pCell->OccupationFlags & 0x40 || pCell->OccupationFlags & 0x80 || pCell->GetInfantry(false);
};

bool MochiMap::IsFoundationValid(
	CellStruct base,
	TechnoTypeClass* pType)
{
	if (!pType)
	{
		return false;
	}

	// JumpJet / BalloonHover 永远允许
	if (pType->JumpJet || pType->BalloonHover)
	{
		return true;
	}

	const bool naval = pType->Naval;

	// 只有 BuildingType 才有 Foundation
	auto pBuildingType =
		abstract_cast<BuildingTypeClass*>(pType);

	// Unit / Infantry / Aircraft
	if (!pBuildingType)
	{
		if (!MapClass::Instance.CoordinatesLegal(base))
		{
			return false;
		}

		auto pCell =
			MapClass::Instance.TryGetCellAt(base);

		if (!pCell)
		{
			return false;
		}
		if (MochiMap::IsCellOccupationOccupied(pCell))
		{
			return false;
		}
		if (pCell->ContainsBridge())
		{
			return false;
		}

		if (naval)
		{
			return pCell->Tile_Is_Water();
		}

		return !pCell->Tile_Is_Water();
	}

	// Building Foundation
	auto foundation =
		pBuildingType->GetFoundationData(false);

	if (!foundation)
	{
		return false;
	}
	// deterministic foundation traversal
	for (CellStruct* pOffset = foundation;
		*pOffset != CellStruct{ 0x7FFF, 0x7FFF };
		++pOffset)
	{
		CellStruct check = {
			static_cast<short>(base.X + pOffset->X),
			static_cast<short>(base.Y + pOffset->Y)
		};

		if (!MapClass::Instance.CoordinatesLegal(check))
		{
			return false;
		}

		auto pCell =
			MapClass::Instance.TryGetCellAt(check);

		if (!pCell)
		{
			return false;
		}
		if (MochiMap::IsCellOccupationOccupied(pCell))
		{
			return false;
		}
		if (pCell->ContainsBridge())
		{
			return false;
		}

		// Naval Building
		if (naval)
		{
			if (!pCell->Tile_Is_Water())
			{
				return false;
			}
		}
		else
		{
			if (pCell->Tile_Is_Water())
			{
				return false;
			}
		}
	}

	return true;
}

CellClass* MochiMap::FindNearestValidLandCell(
	CellStruct center,
	TechnoTypeClass* pType,
	int maxRadius)
{
	if (!pType)
	{
		return nullptr;
	}

	auto TryCell =
		[&](CellStruct cell) -> CellClass*
		{
			if (!MapClass::Instance.CoordinatesLegal(cell))
			{
				return nullptr;
			}

			auto pCell =
				MapClass::Instance.TryGetCellAt(cell);

			if (!pCell)
			{
				return nullptr;
			}

			if (!IsFoundationValid(cell, pType))
			{
				return nullptr;
			}

			return pCell;
		};

	// deterministic ring search
	for (int radius = 0;
		radius <= maxRadius;
		++radius)
	{
		int minX = center.X - radius;
		int maxX = center.X + radius;

		int minY = center.Y - radius;
		int maxY = center.Y + radius;

		// top
		for (int x = minX;
			x <= maxX;
			++x)
		{
			CellStruct cell = {
				(short)x,
				(short)minY
			};

			if (auto pResult = TryCell(cell))
			{
				return pResult;
			}
		}

		// right
		for (int y = minY + 1;
			y <= maxY;
			++y)
		{
			CellStruct cell = {
				(short)maxX,
				(short)y
			};

			if (auto pResult = TryCell(cell))
			{
				return pResult;
			}
		}

		// bottom
		for (int x = maxX - 1;
			x >= minX;
			--x)
		{
			CellStruct cell = {
				(short)x,
				(short)maxY
			};

			if (auto pResult = TryCell(cell))
			{
				return pResult;
			}
		}

		// left
		for (int y = maxY - 1;
			y > minY;
			--y)
		{
			CellStruct cell = {
				(short)minX,
				(short)y
			};

			if (auto pResult = TryCell(cell))
			{
				return pResult;
			}
		}
	}

	return nullptr;
}