#include <MochiMegaMission.h>
#include <Debug.h>



void MochiMegaMission::ShowMegaMissionInfo(EventData* data) {	switch ((Mission)data->MegaMission.Mission)
	{
	case Mission::None:
		Debug::LogW(L"MegaMission Event: 无任务\n");
		break;

	case Mission::Sleep:
		Debug::LogW(L"MegaMission Event: 睡眠\n");
		break;

	case Mission::Attack:
		if ((AbstractType)data->MegaMission.Whom.m_RTTI == AbstractType::Abstract )
		{
			AbstractClass* pWhomAbstract = data->MegaMission.Whom.As_Abstract();
			if ((AbstractType)data->MegaMission.Target.m_RTTI == AbstractType::Abstract)
			{
				AbstractClass* pTargetAbstract = data->MegaMission.Target.As_Abstract();
				Debug::LogW(L"MegaMission Event: %ls 收到攻击 %ls 命令\n", ((ObjectClass*)pWhomAbstract)->GetType()->UIName, ((ObjectClass*)pTargetAbstract)->GetType()->UIName);
			}
			else if((AbstractType)data->MegaMission.Target.m_RTTI == AbstractType::Cell){
				CellClass* pCell = data->MegaMission.Target.As_Cell();
				Debug::LogW(L"MegaMission Event: %ls 收到攻击坐标 %d %d 命令\n", ((ObjectClass*)pWhomAbstract)->GetType()->UIName, pCell->MapCoords.X, pCell->MapCoords.Y);
			}
		}
		break;

	case Mission::Move:
		if ((AbstractType)data->MegaMission.Whom.m_RTTI == AbstractType::Abstract) {
			AbstractClass* pWhomAbstract = data->MegaMission.Whom.As_Abstract();
			if ((AbstractType)data->MegaMission.Destination.m_RTTI == AbstractType::Cell) {
				CellClass* pCell = data->MegaMission.Destination.As_Cell();
				Debug::LogW(L"MegaMission Event: %ls 收到移动到 %d %d 命令\n", ((ObjectClass*)pWhomAbstract)->GetType()->UIName, pCell->MapCoords.X, pCell->MapCoords.Y);
			}
		}
		
		break;

	case Mission::QMove:
		if ((AbstractType)data->MegaMission.Whom.m_RTTI == AbstractType::Abstract) {
			AbstractClass* pWhomAbstract = data->MegaMission.Whom.As_Abstract();
			if ((AbstractType)data->MegaMission.Destination.m_RTTI == AbstractType::Cell) {
				CellClass* pCell = data->MegaMission.Destination.As_Cell();
				Debug::LogW(L"MegaMission Event: %ls 收到强制移动到 %d %d 命令\n", ((ObjectClass*)pWhomAbstract)->GetType()->UIName, pCell->MapCoords.X, pCell->MapCoords.Y);
			}
		}
		break;

	case Mission::Retreat:
		Debug::LogW(L"MegaMission Event: 撤退\n");
		break;

	case Mission::Guard:
		if ((AbstractType)data->MegaMission.Whom.m_RTTI == AbstractType::Abstract) {
			AbstractClass* pWhomAbstract = data->MegaMission.Whom.As_Abstract();
			if ((AbstractType)data->MegaMission.Target.m_RTTI == AbstractType::Cell) {
				CellClass* pCell = data->MegaMission.Target.As_Cell();
				Debug::LogW(L"MegaMission Event: %ls 收到警戒 %d %d 命令\n", ((ObjectClass*)pWhomAbstract)->GetType()->UIName, pCell->MapCoords.X, pCell->MapCoords.Y);
			}
		}
		break;

	case Mission::Sticky:
		Debug::LogW(L"MegaMission Event: 粘附/跟随\n");
		break;

	case Mission::Enter:
		Debug::LogW(L"MegaMission Event: 进入\n");
		break;

	case Mission::Capture:
		if ((AbstractType)data->MegaMission.Whom.m_RTTI == AbstractType::Abstract) 
		{
			AbstractClass* pWhomAbstract = data->MegaMission.Whom.As_Abstract();
			if ((AbstractType)data->MegaMission.Destination.m_RTTI == AbstractType::Abstract) {
				AbstractClass* pDestinationAbstract = data->MegaMission.Destination.As_Abstract();
				Debug::LogW(L"MegaMission Event: %ls 收到占领 %ls 命令\n", ((ObjectClass*)pWhomAbstract)->GetType()->UIName, ((ObjectClass*)pDestinationAbstract)->GetType()->UIName);
			}
			else {

			}
		}
		else {

		}
		break;

	case Mission::Eaten:
		Debug::LogW(L"MegaMission Event: 被吞噬\n");
		break;

	case Mission::Harvest:
		Debug::LogW(L"MegaMission Event: 采矿\n");
		break;

	case Mission::Area_Guard:
		Debug::LogW(L"MegaMission Event: 区域警戒\n");
		break;

	case Mission::Return:
		Debug::LogW(L"MegaMission Event: 返回\n");
		break;

	case Mission::Stop:
		Debug::LogW(L"MegaMission Event: 停止\n");
		break;

	case Mission::Ambush:
		Debug::LogW(L"MegaMission Event: 伏击\n");
		break;

	case Mission::Hunt:
		Debug::LogW(L"MegaMission Event: 猎杀\n");
		break;

	case Mission::Unload:
		if ((AbstractType)data->MegaMission.Whom.m_RTTI == AbstractType::Abstract)
		{
			AbstractClass* pWhomAbstract = data->MegaMission.Whom.As_Abstract();
			if ((AbstractType)data->MegaMission.Target.m_RTTI == AbstractType::Abstract)
			{
				AbstractClass* pTargetAbstract = data->MegaMission.Target.As_Abstract();
				Debug::LogW(L"MegaMission Event: %ls 收到卸载 %ls 命令\n", ((ObjectClass*)pWhomAbstract)->GetType()->UIName, ((ObjectClass*)pTargetAbstract)->GetType()->UIName);
			}
		}
		break;

	case Mission::Sabotage:
		Debug::LogW(L"MegaMission Event: 破坏\n");
		break;

	case Mission::Construction:
		Debug::LogW(L"MegaMission Event: 建造\n");
		break;

	case Mission::Selling:
		Debug::LogW(L"MegaMission Event: 出售\n");
		break;

	case Mission::Repair:
		Debug::LogW(L"MegaMission Event: 修理\n");
		break;

	case Mission::Rescue:
		Debug::LogW(L"MegaMission Event: 救援\n");
		break;

	case Mission::Missile:
		Debug::LogW(L"MegaMission Event: 导弹任务\n");
		break;

	case Mission::Harmless:
		Debug::LogW(L"MegaMission Event: 无害任务\n");
		break;

	case Mission::Open:
		Debug::LogW(L"MegaMission Event: 展开/打开\n");
		break;

	case Mission::Patrol:
		Debug::LogW(L"MegaMission Event: 巡逻\n");
		break;

	case Mission::ParadropApproach:
		Debug::LogW(L"MegaMission Event: 空降接近\n");
		break;

	case Mission::ParadropOverfly:
		Debug::LogW(L"MegaMission Event: 空降飞越\n");
		break;

	case Mission::Wait:
		Debug::LogW(L"MegaMission Event: 等待\n");
		break;

	case Mission::AttackMove:
		Debug::LogW(L"MegaMission Event: 攻击移动\n");
		break;

	case Mission::SpyplaneApproach:
		Debug::LogW(L"MegaMission Event: 间谍飞机接近\n");
		break;

	case Mission::SpyplaneOverfly:
		Debug::LogW(L"MegaMission Event: 间谍飞机飞越\n");
		break;

	default:
		Debug::LogW(
			L"MegaMission Event: 未知任务 %d\n",
			(int)data->MegaMission.Mission);
		break;
	}

}

void MochiMegaMission::ShowMegaMissionFInfo(EventData* data) {
	switch ((Mission)data->MegaMissionF.Mission)
	{
	case Mission::None:
		Debug::LogW(L"MegaMissionF Event: 无任务\n");
		break;

	case Mission::Sleep:
		Debug::LogW(L"MegaMissionF Event: 睡眠\n");
		break;

	case Mission::Attack:
		Debug::LogW(L"MegaMissionF Event: 攻击\n");
		break;

	case Mission::Move:



		Debug::LogW(L"MegaMissionF Event: 移动\n");
		break;

	case Mission::QMove:
		Debug::LogW(L"MegaMissionF Event: 强制移动\n");
		break;

	case Mission::Retreat:
		Debug::LogW(L"MegaMissionF Event: 撤退\n");
		break;

	case Mission::Guard:
		Debug::LogW(L"MegaMissionF Event: 警戒\n");
		break;

	case Mission::Sticky:
		Debug::LogW(L"MegaMissionF Event: 粘附/跟随\n");
		break;

	case Mission::Enter:
		Debug::LogW(L"MegaMissionF Event: 进入\n");
		break;

	case Mission::Capture:
		Debug::LogW(L"MegaMissionF Event: 占领\n");
		break;

	case Mission::Eaten:
		Debug::LogW(L"MegaMissionF Event: 被吞噬\n");
		break;

	case Mission::Harvest:
		Debug::LogW(L"MegaMissionF Event: 采矿\n");
		break;

	case Mission::Area_Guard:
		Debug::LogW(L"MegaMissionF Event: 区域警戒\n");
		break;

	case Mission::Return:
		Debug::LogW(L"MegaMissionF Event: 返回\n");
		break;

	case Mission::Stop:
		Debug::LogW(L"MegaMissionF Event: 停止\n");
		break;

	case Mission::Ambush:
		Debug::LogW(L"MegaMissionF Event: 伏击\n");
		break;

	case Mission::Hunt:
		Debug::LogW(L"MegaMissionF Event: 猎杀\n");
		break;

	case Mission::Unload:
		Debug::LogW(L"MegaMissionF Event: 卸载\n");
		break;

	case Mission::Sabotage:
		Debug::LogW(L"MegaMissionF Event: 破坏\n");
		break;

	case Mission::Construction:
		Debug::LogW(L"MegaMissionF Event: 建造\n");
		break;

	case Mission::Selling:
		Debug::LogW(L"MegaMissionF Event: 出售\n");
		break;

	case Mission::Repair:
		Debug::LogW(L"MegaMissionF Event: 修理\n");
		break;

	case Mission::Rescue:
		Debug::LogW(L"MegaMissionF Event: 救援\n");
		break;

	case Mission::Missile:
		Debug::LogW(L"MegaMissionF Event: 导弹任务\n");
		break;

	case Mission::Harmless:
		Debug::LogW(L"MegaMissionF Event: 无害任务\n");
		break;

	case Mission::Open:
		Debug::LogW(L"MegaMissionF Event: 展开/打开\n");
		break;

	case Mission::Patrol:
		Debug::LogW(L"MegaMissionF Event: 巡逻\n");
		break;

	case Mission::ParadropApproach:
		Debug::LogW(L"MegaMissionF Event: 空降接近\n");
		break;

	case Mission::ParadropOverfly:
		Debug::LogW(L"MegaMissionF Event: 空降飞越\n");
		break;

	case Mission::Wait:
		Debug::LogW(L"MegaMissionF Event: 等待\n");
		break;

	case Mission::AttackMove:
		Debug::LogW(L"MegaMissionF Event: 攻击移动\n");
		break;

	case Mission::SpyplaneApproach:
		Debug::LogW(L"MegaMissionF Event: 间谍飞机接近\n");
		break;

	case Mission::SpyplaneOverfly:
		Debug::LogW(L"MegaMissionF Event: 间谍飞机飞越\n");
		break;

	default:
		Debug::LogW(
			L"MegaMission Event: 未知任务 %d\n",
			(int)data->MegaMission.Mission);
		break;
	}
}