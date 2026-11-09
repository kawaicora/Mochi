#include "MovieHook.h"

#include <Helpers/Macro.h>
#include <Debug.h>

#include <unordered_map>

//--------------------------------------------------------------------------------
// 事件
//--------------------------------------------------------------------------------
Event<const MovieHook::MovieInfo&> MovieHook::MovieStartEvent;
Event<const MovieHook::MovieInfo&> MovieHook::MovieTimeEvent;
Event<const MovieHook::MovieInfo&> MovieHook::MovieEndEvent;
Event<const MovieHook::MovieInfo&> MovieHook::MoviePauseEvent;
Event<const MovieHook::MovieInfo&> MovieHook::MovieResumeEvent;

//--------------------------------------------------------------------------------
// 每个影片的播放状态（按动画对象地址区分，一个影片一个对象）
//
// 游戏是单线程推进播放，用普通容器即可。
//--------------------------------------------------------------------------------
struct MovieState
{
	bool started = false;
	bool ended = false;
};

static std::unordered_map<void*, MovieState> s_movieStates;

//--------------------------------------------------------------------------------
// 读一个 int，越界（空指针 / 未映射）时给 0
//--------------------------------------------------------------------------------
static int ReadInt(const void* p, size_t offset, int fallback = 0)
{
	if (!p)
		return fallback;

	return *reinterpret_cast<const int*>(static_cast<const unsigned char*>(p) + offset);
}

//--------------------------------------------------------------------------------
// 把动画对象翻译成 MovieInfo
//
// 动画对象（0x4326C0 构造）关键字段：
//   +0x04  BINK*
//   +0x20  字幕时间轴对象（非空表示这个影片带字幕）
//   +0x24  每帧的 1/60 秒 tick 数
//   +0x2C  暂停标志
//   +0x30  上一次绘制到的帧（循环起点）
//
// BINK（引擎自己的封装，0x432750 里 BinkOpen 之后填）：
//   +0x00 宽  +0x04 高  +0x08 总帧数  +0x0C 当前帧  +0x14/+0x18 帧率与其除数
//--------------------------------------------------------------------------------
bool MovieHook::FillMovieInfo(void* pAnim, MovieInfo& info)
{
	info = MovieInfo{};

	if (!pAnim || reinterpret_cast<uintptr_t>(pAnim) < 0x10000)
		return false;

	void* pBink = *reinterpret_cast<void* const*>(
		static_cast<const unsigned char*>(pAnim) + 0x04);

	if (!pBink || reinterpret_cast<uintptr_t>(pBink) < 0x10000)
		return false;

	int width = ReadInt(pBink, 0x00);
	int height = ReadInt(pBink, 0x04);
	int totalFrames = ReadInt(pBink, 0x08);
	int currentFrame = ReadInt(pBink, 0x0C);

	// 合理性校验：挡住 VQ 句柄、已经关闭的句柄和野指针
	if (width <= 0 || width > 8192)
		return false;
	if (height <= 0 || height > 8192)
		return false;
	if (totalFrames <= 0 || totalFrames > 100000)
		return false;
	if (currentFrame < 0 || currentFrame > totalFrames + 1)
		return false;

	int frameRate = ReadInt(pBink, 0x14);
	int frameRateDiv = ReadInt(pBink, 0x18);
	int fps = (frameRate > 0 && frameRateDiv > 0) ? (frameRate / frameRateDiv) : 0;

	int frameTicks = ReadInt(pAnim, 0x24);
	if (frameTicks <= 0)
		frameTicks = (fps > 0) ? (0x3C / fps) : 0;   // 与 0x432750 的算法一致

	int loopStartFrame = ReadInt(pAnim, 0x30);

	info.pAnim = pAnim;
	info.pBink = pBink;
	info.Width = width;
	info.Height = height;
	info.TotalFrames = totalFrames;
	info.CurrentFrame = currentFrame;
	info.Fps = fps;
	info.FrameTicks = frameTicks;
	info.CurrentTick = currentFrame * frameTicks;
	info.DurationTick = totalFrames * frameTicks;
	info.CurrentTimeMs = info.CurrentTick * 1000 / 60;
	info.DurationMs = info.DurationTick * 1000 / 60;
	info.LoopStartFrame = loopStartFrame;

	// anim+0x2C：1 = 正在播放，0 = 已 BinkPause 挂起
	info.IsPaused = (ReadInt(pAnim, 0x2C) == 0);

	// 0x432C50 的判定：当前帧 >= 总帧数，或倒回到循环起点之前
	info.IsFinished =
		(currentFrame >= totalFrames) || (currentFrame < loopStartFrame);

	return true;
}

//--------------------------------------------------------------------------------
// 每帧：先补 "开始"（第一次解码出画面时），再报时间
//--------------------------------------------------------------------------------
void MovieHook::OnFrame(void* pAnim)
{
	MovieInfo info{};

	if (!FillMovieInfo(pAnim, info))
		return;

	MovieState& state = s_movieStates[pAnim];

	if (!state.started)
	{
		state.started = true;
		MovieStartEvent.Invoke(info);
	}

	MovieTimeEvent.Invoke(info);

	if (info.IsFinished && !state.ended)
	{
		state.ended = true;
		MovieEndEvent.Invoke(info);
	}
}

//--------------------------------------------------------------------------------
// 完成判定（雷达队列每帧都会问 IsFinished，全屏是内联判定的，不会走这里）
//--------------------------------------------------------------------------------
void MovieHook::OnFinished(void* pAnim)
{
	MovieInfo info{};

	if (!FillMovieInfo(pAnim, info) || !info.IsFinished)
		return;

	MovieState& state = s_movieStates[pAnim];

	if (state.ended)
		return;

	state.ended = true;
	MovieEndEvent.Invoke(info);
}

//--------------------------------------------------------------------------------
// 句柄释放：兜底补一次 "结束"，然后清掉这个影片的状态
//
// 全屏过场不会调用 IsFinished（0x432C70 里是内联比较），只能靠这里收尾。
//--------------------------------------------------------------------------------
void MovieHook::OnRelease(void* pAnim)
{
	if (!pAnim)
		return;

	auto it = s_movieStates.find(pAnim);

	if (it != s_movieStates.end())
	{
		if (!it->second.ended)
		{
			MovieInfo info{};

			if (FillMovieInfo(pAnim, info))
			{
				it->second.ended = true;
				MovieEndEvent.Invoke(info);
			}
		}

		s_movieStates.erase(it);
		return;
	}

	// 没走过任何一帧（比如打开失败），不补结束事件
}

//--------------------------------------------------------------------------------
// 暂停 / 恢复
//
// 引擎在这两处之前只改 anim+0x2C 并调 BinkPause(handle, 1/0)，所以事件里的
// IsPaused 直接给出"改完之后"的状态。
//--------------------------------------------------------------------------------
void MovieHook::OnPause(void* pAnim)
{
	MovieInfo info{};

	if (!FillMovieInfo(pAnim, info))
		return;

	info.IsPaused = true;
	MoviePauseEvent.Invoke(info);
}

void MovieHook::OnResume(void* pAnim)
{
	MovieInfo info{};

	if (!FillMovieInfo(pAnim, info))
		return;

	info.IsPaused = false;
	MovieResumeEvent.Invoke(info);
}

//--------------------------------------------------------------------------------
// FUN_00432E40 —— 全屏与雷达共用的逐帧绘制/推进
//
// 0x432F68: mov ecx,[esi+0x20] / xor bl,bl      (3 + 2 = 5 字节，整指令)
// 此处紧跟 BinkDoFrame 之后（0x432F62），[BINK+0xC] 已经是正在显示的那一帧，
// 下一句就是引擎自己算 CurrentTime 并喂给字幕时间轴的地方。
//--------------------------------------------------------------------------------
DEFINE_HOOK(0x432F68, Movie_Draw_Frame, 0x5)
{
	GET(void*, pAnim, ESI);

	MovieHook::OnFrame(pAnim);

	return 0;
}

//--------------------------------------------------------------------------------
// FUN_00432C50 —— IsFinished
//
// 0x432C50: mov eax,[ecx+4] / push esi / mov edx,[eax+0xC]  (3 + 1 + 3 = 7 字节)
//--------------------------------------------------------------------------------
DEFINE_HOOK(0x432C50, Movie_Anim_IsFinished, 0x7)
{
	GET(void*, pAnim, ECX);

	MovieHook::OnFinished(pAnim);

	return 0;
}

//--------------------------------------------------------------------------------
// FUN_00432700 —— 动画对象释放（MovieHandle::Release 时调）
//
// 0x432700: push esi / mov esi,ecx / push edi / mov edi,[esi+0x20]  (1+2+1+3 = 7 字节)
//--------------------------------------------------------------------------------
DEFINE_HOOK(0x432700, Movie_Anim_Release, 0x7)
{
	GET(void*, pAnim, ECX);

	MovieHook::OnRelease(pAnim);

	return 0;
}

//--------------------------------------------------------------------------------
// 暂停 / 恢复
//
// 引擎有两个绘制变体，各自有一对 push 日志字符串 + BinkPause 的分支：
//
//   0x432E94  push "Pause bink movie"  (68 50 8B 81 00, 5 字节)
//   0x432EBA  push "Resume bink movie" (68 3C 8B 81 00, 5 字节)
//   0x43340D  push "Pause bink movie"
//   0x43343A  push "Resume bink movie"
//
// 两处 ESI 都是动画对象（0x432E53 / 0x4333FD 的 mov esi,ecx）。
// 分支本身要求 anim+0x2C 为 1（暂停）/ 0（恢复），所以同一部片子不会被报两次。
//--------------------------------------------------------------------------------
DEFINE_HOOK(0x432E94, Movie_Anim_Pause, 0x5)
{
	GET(void*, pAnim, ESI);

	MovieHook::OnPause(pAnim);

	return 0;
}

DEFINE_HOOK(0x432EBA, Movie_Anim_Resume, 0x5)
{
	GET(void*, pAnim, ESI);

	MovieHook::OnResume(pAnim);

	return 0;
}

DEFINE_HOOK(0x43340D, Movie_Anim_Pause_Alt, 0x5)
{
	GET(void*, pAnim, ESI);

	MovieHook::OnPause(pAnim);

	return 0;
}

DEFINE_HOOK(0x43343A, Movie_Anim_Resume_Alt, 0x5)
{
	GET(void*, pAnim, ESI);

	MovieHook::OnResume(pAnim);

	return 0;
}
