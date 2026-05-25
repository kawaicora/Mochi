#pragma once


enum class MochiEventType : unsigned char
{

	// Vanilla game used Events from 0x00 to 0x2F
	// CnCNet reserved BuildCatEvents from 0x30 to 0x3F
	// Ares used Events 0x60 and 0x61
	// 0x70 Start  - 0x8F
	CoraCompleteProduction = 0x70,
	CoraSuperWeaponCharge = 0x71,
	CoraMoneyChange = 0x72,
	CoraSpecialPlace = 0x73,
	CoraPlace = 0x74,
	CoraActiveSuperWeapon = 0x75,
	CoraUnlockAllTech = 0x76,
};

