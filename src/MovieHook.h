#pragma once
#include <EventSystem.h>

//--------------------------------------------------------------------------------
// 通用电影（Bink）播放事件
//
// 全屏过场（Game::PlayMovie 0x5BED40）和雷达区视频（MochiRadar::PlayMovieInSidebar
// 0x5BF2B0 / 0x5BF390）走的是同一个 BinkMovieHandle，逐帧绘制都汇到
// FUN_00432E40；所以这里一套 hook 两种通道都能拿到。
//
// 时间来源就是引擎自己算给字幕用的那个值（0x432F78-0x432F7B）：
//      CurrentTime = [BINK+0x0C] * [anim+0x24]
// 其中 [anim+0x24] 在 0x432750 里由 0x3C / (FrameRate / FrameRateDiv) 得到，
// 单位是 1/60 秒（一帧占多少个 60Hz tick），所以毫秒 = tick * 1000 / 60。
//--------------------------------------------------------------------------------
class MovieHook
{
public:
	struct MovieInfo
	{
		void* pAnim;          // BinkMovieHandle 内部的动画对象（0x34 字节），每个影片唯一
		void* pBink;          // pAnim+0x04 指向的 BINK*
		int   Width;          // BINK+0x00
		int   Height;         // BINK+0x04
		int   TotalFrames;    // BINK+0x08
		int   CurrentFrame;   // BINK+0x0C，BinkDoFrame 之后 = 正在显示的这一帧
		int   Fps;            // BINK+0x14 / BINK+0x18
		int   FrameTicks;     // anim+0x24，每帧占多少个 1/60 秒 tick
		int   CurrentTick;    // CurrentFrame * FrameTicks
		int   DurationTick;   // TotalFrames * FrameTicks
		int   CurrentTimeMs;  // CurrentTick * 1000 / 60
		int   DurationMs;     // DurationTick * 1000 / 60
		int   LoopStartFrame; // anim+0x30，循环起点帧
		bool  IsFinished;     // 与引擎 0x432C50 的判定一致
		bool  IsPaused;       // anim+0x2C == 0（暂停中）
	};

	// 第一帧真正被解码出来时触发（队列里的影片在排队期间不会触发）
	static Event<const MovieInfo&> MovieStartEvent;

	// 每解码出一帧触发一次，带这一帧对应的实时播放时间
	static Event<const MovieInfo&> MovieTimeEvent;

	// 播放结束触发；句柄被释放时若还没触发过会兜底补一次
	static Event<const MovieInfo&> MovieEndEvent;

	// 暂停（含游戏整体暂停拖停影片）/ 恢复，由 BinkPause 调用点驱动
	static Event<const MovieInfo&> MoviePauseEvent;
	static Event<const MovieInfo&> MovieResumeEvent;

	// 从动画对象填充信息，失败（不是 Bink 或对象已失效）返回 false
	static bool FillMovieInfo(void* pAnim, MovieInfo& info);

	// 引擎自己的取时间函数（0x4333E0）：返回当前帧 × 每帧 tick，单位 1/60 秒
	// 有 ECX = 动画对象时可直接调，等于 MovieInfo::CurrentTick
	// static int(__fastcall* GetCurrentTick)(void* pAnim) = 0x4333E0;

	// hook 内部调用
	static void OnFrame(void* pAnim);
	static void OnFinished(void* pAnim);
	static void OnRelease(void* pAnim);
	static void OnPause(void* pAnim);
	static void OnResume(void* pAnim);
};
