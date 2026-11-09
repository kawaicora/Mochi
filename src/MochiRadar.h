#pragma once
#include <YRPP.h>

// 小地图（侧边栏）视频相关薄壳 / 入口
// 说明见《Mochi-侧边栏视频复刻步骤.md》
class MochiRadar {
public:
	// ECX = 文件名；DL = 是否暂停整局（1 = 播放期间自动暂停，播完自动恢复）
	static void __fastcall PlayMovieInSidebarByName(const char* movieName, char pauseGame);

	// ECX = 电影索引（必须 >= 0 且 < DAT_00ABF3A0）；DL = 是否暂停整局
	// 名字由引擎自己从 [0xABF394] 表里取，等价于 TActionClass::PlayMovieInSidebar
	static void __fastcall PlayMovieInSidebarByIndex(int index, char pauseGame);

	// 引擎内部暂停计数器（与 ResumeGameEngine 成对、可嵌套）
	// seconds = 0 表示无限暂停；seconds = n 表示 n 秒后自动恢复
	static void __fastcall PauseGameEngine(int seconds);
	static void ResumeGameEngine();

	// 雷达（小地图）当前是否可用（雷达建筑在、通电）
	static bool IsRadarAvailable();

	// 推荐入口：播一段小地图（雷达区）视频
	// 返回值只表示“已提交入队”，不代表播放结束
	static bool PlayMovieInSidebar(const char* fileName, bool pauseGame = true);
};
