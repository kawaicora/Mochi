#include "MochiRadar.h"
#include <RadarClass.h>	//RadarClass::Instance / IsAvailableNow

// ECX = 文件名；DL = 是否暂停整局
void __fastcall MochiRadar::PlayMovieInSidebarByName(const char* movieName, char pauseGame)
{
	JMP_STD(0x5BF2B0);
}

// ECX = 电影索引；DL = 是否暂停整局
void __fastcall MochiRadar::PlayMovieInSidebarByIndex(int index, char pauseGame)
{
	JMP_STD(0x5BF390);
}

// ECX = 暂停秒数（0 = 无限暂停）
void __fastcall MochiRadar::PauseGameEngine(int seconds)
{
	JMP_STD(0x684060);
}

void MochiRadar::ResumeGameEngine()
{
	JMP_STD(0x684180);
}

bool MochiRadar::IsRadarAvailable()
{
	return RadarClass::Instance.IsAvailableNow;
}

bool MochiRadar::PlayMovieInSidebar(const char* fileName, bool pauseGame)
{
	if (!fileName || !*fileName) {
		return false;
	}

	// 小地图没开就整段跳过
	//if (!IsRadarAvailable()) {
	//	return false;
	//}

	// 引擎入口硬性要求 GameMode == Campaign(0)，先存后改
	GameMode oldMode = SessionClass::Instance.GameMode;
	SessionClass::Instance.GameMode = GameMode::Campaign;

	PlayMovieInSidebarByName(fileName, static_cast<char>(pauseGame ? 1 : 0));

	// 入队是同步完成的，这里立刻还原
	SessionClass::Instance.GameMode = oldMode;

	return true;
}
