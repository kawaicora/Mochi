#include "MochiGame.h"
#include <EventData.h>
#include <MochiGraph.h>

void MochiGame::ReadINI(const char* file) {
	CCINIClass* pINI = GameCreate<CCINIClass>();
	if (pINI) {
		CCFileClass* cfg = GameCreate<CCFileClass>(file);

		if (cfg) {
			if (cfg->Exists()) {
				pINI->ReadCCFile(cfg);
			}
			GameDelete(cfg);
		}
	}
};

std::vector<HouseClass*> MochiGame::GetSelectedObjectOwnerHouse() {
	std::vector<HouseClass*> HouseList;
	for (auto const& abs : ObjectClass::Array) {
		if (!abs) continue;
		auto obj = dynamic_cast<ObjectClass*>(abs);
		if (!obj) continue;
		if (obj->IsSelected == false) continue;
		if (HouseClass* pHouse = obj->GetOwningHouse()) {
			if (std::find(HouseList.begin(), HouseList.end(), pHouse) == HouseList.end()) {
				HouseList.push_back(pHouse);
			}
		}
	}
	return HouseList;
};
void MochiGame::DrawAllFactoryProduction() {

	for (FactoryClass* pFactory : FactoryClass::Array) {
		if (!pFactory) continue;
		if (!pFactory->Owner) continue;
		if (!pFactory->Object) continue;

		DrawFactoryProduction(pFactory);
	}

};


void MochiGame::DrawFactoryProduction(FactoryClass* pFactory)
{
	if (!pFactory) return;
	if (!pFactory->Owner) return;
	if (!pFactory->Object) return;
	for (const auto& building : pFactory->Owner->Buildings) {
		if (!building) {
			continue;
		}
		if (
			pFactory->Object->GetTechnoType()->WhatAmI() == AbstractType::Building ||
			pFactory->Object->GetTechnoType()->WhatAmI() == AbstractType::BuildingType) goto DRAW_FACTORY_PRODUCTION_SKIP_IF_NAVAL;

		if (pFactory->Object->GetTechnoType()->Naval) {
			if (!building->Type->Naval) continue; //如果工厂生产的单位是海军单位，但建筑不是海军建筑，则跳过
		}
		else {
			if (building->Type->Naval) continue; //如果工厂生产的单位不是海军单位，但建筑是海军建筑，则跳过
		}

	DRAW_FACTORY_PRODUCTION_SKIP_IF_NAVAL:

		if (pFactory->Object->GetTechnoType()->WhatAmI() != building->Type->Factory) continue; // Skip if the building's factory type doesn't match the factory's object type
		//if (!building->IsPrimaryFactory) continue; // Skip if it's not a primary factory
		const int cameoWidth = 60;
		const int cameoHeight = 48;
		SHPStruct* cameo = pFactory->Object->GetType()->GetCameo();
		const wchar_t* uiname = pFactory->Object->GetType()->UIName;
		std::pair<Point2D, bool> result = TacticalClass::Instance->CoordsToClient(building->GetCoords());
		if (!result.second) continue;
		Point2D location = result.first;
		location.Y -= 50; // Adjust the Y coordinate to be above the building

		RectangleStruct darkenBounds{ 0, 0, location.X + cameoWidth, location.Y + cameoHeight };
		DSurface::Composite->DrawSHP(
			FileSystem::CAMEO_PAL,
			cameo,
			0,
			&location,
			&darkenBounds,
			BlitterFlags(0xE00),
			0,
			0,
			ZGradient::Ground,
			1000,
			0,
			nullptr,
			0,
			0,
			0
		);
	}

};


std::vector<ObjectClass*> MochiGame::GetSelectedObjectList() {
	std::vector<ObjectClass*> ObjectList;
	for (auto const& abs : ObjectClass::Array) {
		if (!abs) continue;
		auto obj = dynamic_cast<ObjectClass*>(abs);
		if (!obj) continue;
		if (obj->IsSelected == false) continue;

		ObjectList.push_back(obj);
	}
	return ObjectList;
};
void MochiGame::DrawAllGameObjectInfo(bool bFilterNotSelected, bool bFilterNotHouse)
{
	for (auto const& abs : ObjectClass::Array) {
		if (!abs) continue;

		auto obj = dynamic_cast<ObjectClass*>(abs);
		if (!obj) continue;
		if (bFilterNotHouse && !obj->GetOwningHouse()) continue;
		if (bFilterNotSelected && obj->IsSelected == false) continue;
		MochiGame::DrawObjectInfo(obj);
	}
};
void MochiGame::DrawObjectInfo(ObjectClass* obj) {
	wchar_t buf[512];
	std::pair<Point2D, bool> result = TacticalClass::Instance->CoordsToClient(obj->GetCoords());
	if (!result.second) {
		return;
	}
	Point2D loc = result.first;
	const char* id = obj->GetType()->ID;
	if (!id) return;
	int strength = obj->GetType()->Strength;
	int hp = obj->Health;
	const wchar_t* uiname = obj->GetType()->UIName;
	if (!uiname) uiname = L"";
	int len = 0;
	if (uiname == L"") {
		len = swprintf(buf, 512, L"%S %d/%d", id, hp, strength);
	}
	else {
		len = swprintf(buf, 512, L"%ls %d/%d", uiname, hp, strength);
	}
	int fontSize = 14;
	
	loc.Y += 20;
	
	//测试 
	ColorStruct Color = ColorStruct{ 128,128,128 };
	if (HouseClass* pHouse = obj->GetOwningHouse()) {
		Color = pHouse->Color;
	}
	MochiGraph::GraphData data=  MochiGraph::GetCharacterBitmap(
		buf,
		16,
		Color,
		1,
		MochiGraph::GetComplementColor(Color),
		1
	);
	loc.X -= data.Width / 2;
	MochiGraph::DrawGraphData(
		data,
		DSurface::Composite,
		loc
	);

};
void MochiGame::DrawHouseInfo() {
	wchar_t buf[512];
	std::vector<int> withNameHouseIndex;
	Point2D loc;
	loc.X = 0;
	const int AdvCommBarHeight = 32;
	int offset = AdvCommBarHeight;

	for (auto const& pHouse : HouseClass::Array) {
		if (!pHouse) {
			continue;
		}
		if (pHouse == HouseClass::FindNeutral() || pHouse == HouseClass::FindSpecial()) {
			continue;
		}
		if (pHouse->Defeated) {
			continue;
		}
		swprintf(
			buf,
			512,
			L"%ls %ls %d  ",
			MochiHouse::GetPlayerNameByHouseIndex(pHouse->ArrayIndex),
			L"\u8d44\u91d1:",
			pHouse->Available_Money()
		);
		auto wanted = Drawing::GetTextDimensions(buf, loc, (DWORD)-1, 0, 0);

		auto h = DSurface::Composite->GetHeight();
		RectangleStruct rect = { 0, h - wanted.Height - offset, wanted.Width, wanted.Height };
		DSurface::Composite->FillRect(&rect, COLOR_BLACK);

		DSurface::Composite->DrawTextA(buf, 0, rect.Y, Drawing::RGB_To_Int(pHouse->Color));
		offset += wanted.Height;

	}

	withNameHouseIndex.clear();
};

void MochiGame::DrawRadarTest() {
	auto pRadar = &RadarClass::Instance;
	RectangleStruct rect{ *(int*)((uintptr_t)pRadar + 0x11E4), *(int*)((uintptr_t)pRadar + 0x11EC), 160, 110 };
	Point2D location = { *(int*)((uintptr_t)pRadar + 0x11E4), *(int*)((uintptr_t)pRadar + 0x11EC) };
	DSurface::Sidebar->FillRect(&rect, COLOR_RED);
	//MapClass::Instance.RedrawSidebar(0);
	RectangleStruct bounds{ 0, 0, location.X + 160, location.Y + 110 };
	//RadarClass::Instance->SetVisibleRect(rect);
	DSurface::Sidebar->DrawSHP(
		(ConvertClass*)0xB0FBF8,
		(SHPStruct*)0xB04A38,
		32,
		&location,
		&bounds,
		BlitterFlags::bf_400,
		0,
		0,
		ZGradient::Ground,
		1000,
		0,
		nullptr,
		0,
		0,
		0
	);

};

void MochiGame::PlayMovie(const char* fileName,
	int queueTheme,
	char useHiddenSurface1,
	char stretchMovie,
	char useHiddenSurface2,
	char setState1) {

	GameMode gameMode = *(GameMode*)0xA8B238;  //保存当前游戏模式
	*(GameMode*)0xA8B238 = GameMode::Campaign;
	Game::PlayMovie(
		fileName,
		queueTheme,
		useHiddenSurface1,
		stretchMovie,
		useHiddenSurface2,
		setState1
	);
	*(GameMode*)0xA8B238 = gameMode;  //恢复原有游戏模式

};



//***********************************************Command Functions************************************************
void MochiGame::SetCurrentPlayerAIControl(bool bCurrentPlayIsAIControl) {
	HouseClass::CurrentPlayer->IsHumanPlayer = !bCurrentPlayIsAIControl; //设置成相反的值
	ToggleCurrentPlayerAIControl(); //调用切换函数以应用更改

};
void MochiGame::ToggleCurrentPlayerAIControl() {
	if (HouseClass::CurrentPlayer->IsHumanPlayer) {
		HouseClass::CurrentPlayer->IsHumanPlayer = false;
		HouseClass::CurrentPlayer->Production = HouseClass::CurrentPlayer->AutocreateAllowed = true;
		HouseClass::CurrentPlayer->IQLevel = 5;
		HouseClass::CurrentPlayer->IQLevel2 = 5;
		HouseClass::CurrentPlayer->AIDifficulty = AIDifficulty::Hard;
		MessageListClass::Instance.PrintMessage(L"AI接管控制!", 300, ColorScheme::Green);
	}
	else {


		HouseClass::CurrentPlayer->IsHumanPlayer = true;
		HouseClass::CurrentPlayer->Production = HouseClass::CurrentPlayer->AutocreateAllowed = false;
		HouseClass::CurrentPlayer->IQLevel = 0;
		HouseClass::CurrentPlayer->IQLevel2 = 0;
		HouseClass::CurrentPlayer->AIDifficulty = AIDifficulty::Normal;
		MessageListClass::Instance.PrintMessage(L"玩家接管控制!", 300, ColorScheme::Green);
	}
};
void MochiGame::ActiveAllSuperWeapon(HouseClass* pHouse) {
	for (int i = 0; i < pHouse->Supers.Count; i++) {
		EventData* event = EventData::EventClass_CTOR();
		SuperClass* super = pHouse->Supers[i];
		if (!super) {
			Debug::Log("super is zero\n");
			continue;
		}
		if (super->IsReady) {
			Debug::LogW(L"super Is Ready: %ls\n", super->Type->UIName);
			continue;
		}
		if (super->CanFire()) {
			Debug::LogW(L"super Can Fire: %ls\n", super->Type->UIName);
			continue;
		}
		//发送事件
		MochiEvent::SendActiveSuperWeaponEvent(pHouse, i);
	}
};

void MochiGame::RandomGenUnits(HouseClass* pHouse) {
	try {
		int count = MochiINI::PlayerBaseConfig->GetValueByKey<int>("GenCount", 10);
		for (int i = 0; i < count; i++) {
			auto RandomInfantryIDListStr = MochiINI::PlayerBaseConfig->GetValueByKey<std::string>("RandomInfantryID");
			std::vector<std::string> RandomInfantryIDList = MochiINI::Split(RandomInfantryIDListStr, ',');
			const char* id = RandomInfantryIDList[rand() % RandomInfantryIDList.size()].c_str();
			TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::Find(id);
			Debug::LogW(L"Generating unit: %S %ls\n", id, pTechnoTypeClass->UIName);
			MochiEvent::SendPlaceEvent(pHouse, id, DisplayClass::Instance.Display_ZoneCell);
		}
		for (int i = 0; i < count; i++) {
			auto RandomUnitIDListStr = MochiINI::PlayerBaseConfig->GetValueByKey<std::string>("RandomUnitID");
			std::vector<std::string> RandomUnitIDList = MochiINI::Split(RandomUnitIDListStr, ',');
			const char* id = RandomUnitIDList[rand() % RandomUnitIDList.size()].c_str();
			TechnoTypeClass* pTechnoTypeClass = TechnoTypeClass::Find(id);
			Debug::LogW(L"Generating unit: %S %ls\n", id, pTechnoTypeClass->UIName);
			MochiEvent::SendPlaceEvent(pHouse, id, DisplayClass::Instance.Display_ZoneCell);
		}
	}
	catch (...) {
		Debug::Log("Failed to generate units\n");
		return;
	}
};


void MochiGame::ChargeAllSuperWeapon(HouseClass* pHouse) {

	for (int i = 0;i < pHouse->Supers.Count; i++) {
		EventData* event = EventData::EventClass_CTOR();
		SuperClass* super = pHouse->Supers[i];
		if (!super) {

			Debug::Log("super is zero\n");
			continue;
		}
		int total = super->Type->RechargeTime;
		int remain = super->RechargeTimer.GetTimeLeft();

		int percent = (1.0f - (float)remain / total) * 100.0f;
		//Debug::Log("SuperWeapon: %ls IsReady: %ls CanFire: %ls  ChargePercent: %d \n",  super->Type->UIName,  super->IsReady ? L"true":L"false", super->CanFire()? L"true" : L"false",percent);

		if (super->IsReady) {
			continue;
		}
		if (super->CanFire()) {
			continue;
		}

		if (percent == 100) {
			continue;
		}
		MochiEvent::SendSuperWeaponChargeEvent(pHouse, i);
	}
};


void MochiGame::LaunchSuperWeaponAtMouseLocation(HouseClass* pHouse, int index) {
	try {
		auto LaunchSuperWeaponListStr = MochiINI::PlayerBaseConfig->GetValueByKey<std::string>("LaunchSuperWeaponList");
		std::vector<std::string> LaunchSuperWeaponList = MochiINI::Split(LaunchSuperWeaponListStr, ',');
		SuperWeaponTypeClass* superWeaponType = SuperWeaponTypeClass::Find(LaunchSuperWeaponList[index].c_str());
		MochiEvent::SendSpecialPlaceEvent(pHouse, superWeaponType, DisplayClass::Instance.Display_ZoneCell);
	}
	catch (...) {
		Debug::Log("Failed to launch super weapon\n");
	}
};
