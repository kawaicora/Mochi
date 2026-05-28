#pragma once
#include<YRPP.h>

class MapHook
{
public:
	static bool IsValidLandCell(CellClass* pCell, TechnoTypeClass* pType)
	{
		if (!pCell || !pType)
		{
			return false;
		}
		if (pType->JumpJet || pType->BalloonHover)
		{
			return true;
		}
		// 海军单位
		if (pType->Naval)
		{
			if (!pCell->Tile_Is_Water())
			{
				return false;
			}
		}
		else
		{
			// 非海军不能放海里
			if (pCell->Tile_Is_Water())
			{
				return false;
			}
		}

		// 不允许桥
		if (pCell->ContainsBridge())
		{
			return false;
		}

		// 地图合法
		if (!MapClass::Instance.CoordinatesLegal(
			pCell->MapCoords))
		{
			return false;
		}

		// 可移动
		if (!pCell->IsClearToMove(
			pType->SpeedType,
			false,
			false,
			-1,
			pType->MovementZone,
			pCell->Level,
			false))
		{
			return false;
		}

		return true;
	}

	static CellClass* FindNearestValidLandCell(CellStruct center, TechnoTypeClass* pType, int maxRadius = 64)
	{
		if (!pType)
		{
			return nullptr;
		}

		const bool naval = pType->Naval;

		// deterministic ring search
		for (int radius = 0; radius <= maxRadius; ++radius)
		{
			int minX = center.X - radius;
			int maxX = center.X + radius;

			int minY = center.Y - radius;
			int maxY = center.Y + radius;

			// 上边
			for (int x = minX; x <= maxX; ++x)
			{
				CellStruct cell = {
					(short)x,
					(short)minY
				};

				auto pCell =
					MapClass::Instance.TryGetCellAt(cell);

				if (!pCell)
				{
					continue;
				}

				// 海军必须是水
				if (naval)
				{
					if (!pCell->Tile_Is_Water())
					{
						continue;
					}
				}
				else
				{
					// 非海军必须不是水
					if (pCell->Tile_Is_Water())
					{
						continue;
					}
				}

				if (IsValidLandCell(pCell, pType))
				{
					return pCell;
				}
			}

			// 右边
			for (int y = minY + 1; y <= maxY; ++y)
			{
				CellStruct cell = {
					(short)maxX,
					(short)y
				};

				auto pCell =
					MapClass::Instance.TryGetCellAt(cell);

				if (!pCell)
				{
					continue;
				}

				if (naval)
				{
					if (!pCell->Tile_Is_Water())
					{
						continue;
					}
				}
				else
				{
					if (pCell->Tile_Is_Water())
					{
						continue;
					}
				}

				if (IsValidLandCell(pCell, pType))
				{
					return pCell;
				}
			}

			// 下边
			for (int x = maxX - 1; x >= minX; --x)
			{
				CellStruct cell = {
					(short)x,
					(short)maxY
				};

				auto pCell =
					MapClass::Instance.TryGetCellAt(cell);

				if (!pCell)
				{
					continue;
				}

				if (naval)
				{
					if (!pCell->Tile_Is_Water())
					{
						continue;
					}
				}
				else
				{
					if (pCell->Tile_Is_Water())
					{
						continue;
					}
				}

				if (IsValidLandCell(pCell, pType))
				{
					return pCell;
				}
			}

			// 左边
			for (int y = maxY - 1; y > minY; --y)
			{
				CellStruct cell = {
					(short)minX,
					(short)y
				};

				auto pCell =
					MapClass::Instance.TryGetCellAt(cell);

				if (!pCell)
				{
					continue;
				}

				if (naval)
				{
					if (!pCell->Tile_Is_Water())
					{
						continue;
					}
				}
				else
				{
					if (pCell->Tile_Is_Water())
					{
						continue;
					}
				}

				if (IsValidLandCell(pCell, pType))
				{
					return pCell;
				}
			}
		}

		return nullptr;
	};
};
