#pragma once
#include <string>
#include <YRPP.h>
#include <MochiGame.h>
class AIControlCommandClass: public CommandClass
{
public:
	//CommandClass
	virtual const char* GetName() const override
	{
		return "AIControl";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"AI Control";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"切换AI或手动控制";
	}

	virtual void Execute(WWKey eInput) const override
	{

		MochiGame::ToggleCurrentPlayerAIControl();
		return;
	}
};

class InstantConstructionCommandClass : public CommandClass
{
public:
	//CommandClass
	virtual const char* GetName() const override
	{
		return "InstantConstruction";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Instant Construction";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"启用或者禁用瞬间建造";
	}

	virtual void Execute(WWKey eInput) const override
	{

		MochiHouse::IsPlayerInstantConstruction = !MochiHouse::IsPlayerInstantConstruction;
		return;
	}
};

class AutoChargePlayerAllSuperweaponCommandClass : public CommandClass 
{
	//CommandClass
	virtual const char* GetName() const override
	{
		return "AutoChargePlayerAllSuperweapon";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Auto Charge Player All Superweapon";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"切换是否自动为玩家充能所有超武";
	}

	virtual void Execute(WWKey eInput) const override
	{

		MochiHouse::IsAutoChargePlayerAllSuperweapon = !MochiHouse::IsAutoChargePlayerAllSuperweapon;
		return;
	}
};
class GiveMoneyCommandClass : public CommandClass
{
public:
	//CommandClass
	virtual const char* GetName() const override
	{
		return "GiveMoney";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Give Money";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"给玩家一定数量的金钱";
	}

	virtual void Execute(WWKey eInput) const override
	{

		MochiEvent::SendChangeMoneyEvent(HouseClass::CurrentPlayer, 1000);
		return;
	}
};


class UnlockAllUnitCommandClass
	: public CommandClass
{
public:
	//CommandClass
	virtual const char* GetName() const override
	{
		return "UnlockAllUnit";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Unlock All Unit";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"解锁所有单位";
	}

	virtual void Execute(WWKey eInput) const override
	{
		MochiEvent::SendUnlockAllTechEvent(HouseClass::CurrentPlayer, !MochiHouse::IsTechUnlocked(HouseClass::CurrentPlayer));
		return;
	}
};


class LaunchSuperWeaponACommandClass : public CommandClass
{
public:
	//CommandClass
	virtual const char* GetName() const override
	{
		return "LaunchSuperWeaponA";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Launch Super Weapon A";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"Launch Super Weapon A Config  __Moshi.ini ";
	}

	virtual void Execute(WWKey eInput) const override
	{

		MochiGame::LaunchSuperWeaponAtMouseLocation(HouseClass::CurrentPlayer, 0);
		return;
	}
};

class LaunchSuperWeaponBCommandClass : public CommandClass
{
public:
	//CommandClass
	virtual const char* GetName() const override
	{
		return "LaunchSuperWeaponB";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Launch Super Weapon B";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"Launch Super Weapon C Config  __Moshi.ini ";
	}

	virtual void Execute(WWKey eInput) const override
	{

		MochiGame::LaunchSuperWeaponAtMouseLocation(HouseClass::CurrentPlayer, 1);
		return;
	}
};
class LaunchSuperWeaponCCommandClass : public CommandClass
{
public:
	//CommandClass
	virtual const char* GetName() const override
	{
		return "LaunchSuperWeaponC";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Launch Super Weapon C";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"Launch Super Weapon C  Config  __Moshi.ini ";
	}

	virtual void Execute(WWKey eInput) const override
	{

		MochiGame::LaunchSuperWeaponAtMouseLocation(HouseClass::CurrentPlayer, 2);
		return;
	}
};


class LaunchSuperWeaponDCommandClass : public CommandClass
{
public:
	//CommandClass
	virtual const char* GetName() const override
	{
		return "LaunchSuperWeaponD";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Launch Super Weapon D";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"Launch Super Weapon D Config  __Moshi.ini ";
	}

	virtual void Execute(WWKey eInput) const override
	{
		MochiGame::LaunchSuperWeaponAtMouseLocation(HouseClass::CurrentPlayer, 3);
		return;
	}
};

class PlayMovieCommandClass : public CommandClass 
{
public:
	static DWORD __fastcall sub_5C07D0(const char* FullPath, DSurface* surface, int* a3)
	{
		JMP_STD(0x5C07D0);
	}

	virtual const char* GetName() const override
	{
		return "PlayMovie";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Play Movie";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"Play Movie";
	}

	virtual void Execute(WWKey eInput) const override
	{
		sub_5C07D0("V_001", DSurface::Sidebar, 0);
		return;
	}
};

class MoveCommandClass : public CommandClass
{
public:
	static DWORD __fastcall sub_5C07D0(const char* FullPath, DSurface* surface, int* a3)
	{
		JMP_STD(0x5C07D0);
	}

	virtual const char* GetName() const override
	{
		return "Move";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Move";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"移动命令";
	}

	virtual void Execute(WWKey eInput) const override
	{
		std::vector<ObjectClass*> ObjList = MochiGame::GetSelectedObjectList();
		for (ObjectClass* obj : ObjList) {
			MochiEvent::SendMoveEvent(obj, DisplayClass::Display_ZoneCell);
		}
		
		return;
	}
};

class AttackCommandClass : public CommandClass
{
public:
	static DWORD __fastcall sub_5C07D0(const char* FullPath, DSurface* surface, int* a3)
	{
		JMP_STD(0x5C07D0);
	}

	virtual const char* GetName() const override
	{
		return "Attack";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Attack";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"攻击命令";
	}

	virtual void Execute(WWKey eInput) const override
	{
		std::vector<ObjectClass*> ObjList = MochiGame::GetSelectedObjectList();
		for (ObjectClass* obj : ObjList) {
			MochiEvent::SendAttackEvent(obj, DisplayClass::Display_ZoneCell);
		}

		return;
	}
};

//************************************Bad Function, Use With Caution!************************************



class GenUnitCommandClass : public CommandClass
{
public:
	//CommandClass
	virtual const char* GetName() const override
	{
		return "GenUnit";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Gen Unit";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"刷单位";
	}

	virtual void Execute(WWKey eInput) const override
	{

		MochiGame::RandomGenUnits(HouseClass::CurrentPlayer);
		return;
	}
};

class ActiveAllSuperWeaponCommandClass
	: public CommandClass
{
public:
	//CommandClass
	virtual const char* GetName() const override
	{
		return "ActiveAllSuperWeapon";
	}

	virtual const wchar_t* GetUIName() const override
	{
		return L"Active All Super Weapon";
	}

	virtual const wchar_t* GetUICategory() const override
	{
		return L"Mochi开发选项";
	}

	virtual const wchar_t* GetUIDescription() const override
	{
		return L"激活所有超武";
	}

	virtual void Execute(WWKey eInput) const override
	{

		MochiGame::ActiveAllSuperWeapon(HouseClass::CurrentPlayer);
		return;
	}
};


template <typename T>
void MakeCommand() {
	T* command = GameCreate<T>();
	CommandClass::Array.AddItem(command);
};
