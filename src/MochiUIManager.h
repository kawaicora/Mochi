#pragma once
#include <YRPP.h>
#include <GadgetClass.h>
#include <functional>
#include <string>
#include <vector>

class MochiUIGadget;      // 定义在 MochiUIManager.cpp
struct MochiUIButton;

// 回调参数
struct MochiUIEvent
{
	MochiUIButton* Button = nullptr;
	int  MouseX = 0, MouseY = 0;
	int  X = 0, Y = 0, Width = 0, Height = 0;
	bool Disabled = false;
};

// 锚点：X/Y 为屏幕比例（0~1），OffsetX/OffsetY 为像素
struct MochiUIAnchor
{
	float X = 0.0f;
	float Y = 0.0f;
	int   OffsetX = 0;
	int   OffsetY = 0;
	int   Pivot = 0;            // 0 左上 1 上中 2 右上 3 左中 4 正中 5 右中 6 左下 7 下中 8 右下
	bool  ScaleOffset = true;   // 偏移是否按分辨率缩放（基准 800x600）
};

struct MochiUIElement
{
	SHPStruct* Shape = nullptr;
	ConvertClass* Palette = nullptr;      // 空 = FileSystem::PALETTE_PAL
	MochiUIAnchor Anchor{};
	MochiUIButton* Owner = nullptr;       // 非空 = 这是按钮的画布

	int  Layer = 0;                       // 绘制层级：小的先画（在底层），同层按注册顺序
	int  Frame = 0;
	int  FrameCount = 1;
	int  FrameDelay = 1;
	bool Loop = true;
	bool Visible = true;
	int  Width = 0, Height = 0;           // 0 = 用 SHP 自身尺寸
	int  X = 0, Y = 0;                    // Layout 解析后的屏幕坐标
};

struct MochiUIButton
{
	MochiUIElement Canvas;
	int  Id = 0;
	int  HoverFrame = -1;                 // -1 = 不用悬停帧
	int  PressedFrame = -1;
	int  DisabledBrightness = 500;        // 0~2000，1000 = 原色
	bool Disabled = false;
	bool Hovered = false;
	bool Pressed = false;

	std::function<void(const MochiUIEvent&)> OnEnter;    // 鼠标移入（一次）
	std::function<void(const MochiUIEvent&)> OnLeave;    // 鼠标移出
	std::function<void(const MochiUIEvent&)> OnHover;    // 停留中，每帧
	std::function<void(const MochiUIEvent&)> OnDown;     // 左键按下
	std::function<void(const MochiUIEvent&)> OnUp;       // 左键抬起

	MochiUIGadget* Gadget = nullptr;
};

class MochiUIManager
{
public:
	static void Init();      // 订阅 DrawOnTop，只做一次
	static void Destroy();   // 移除全部 gadget，释放全部元素

	// 背景图 / 动画（SHP + 层级）
	static MochiUIElement* RegisterImage(SHPStruct* pShp, int x, int y, int layer,
		ConvertClass* pPalette = nullptr);
	static MochiUIElement* RegisterAnimation(SHPStruct* pShp, int x, int y, int layer,
		int delay = 1, bool loop = true, ConvertClass* pPalette = nullptr);

	// 按钮（SHP + 层级 + 禁用 + 五个回调）
	static MochiUIButton* RegisterButton(SHPStruct* pShp, int x, int y, int w, int h,
		int layer, bool isDisabled,
		std::function<void(const MochiUIEvent&)> onEnter,
		std::function<void(const MochiUIEvent&)> onLeave,
		std::function<void(const MochiUIEvent&)> onHover,
		std::function<void(const MochiUIEvent&)> onDown,
		std::function<void(const MochiUIEvent&)> onUp,
		int id = 0);

	static void RemoveButton(int id);
	static void RemoveElement(MochiUIElement* pElement);

	static void SetLayer(MochiUIElement* pElement, int layer);
	static void SetVisible(MochiUIElement* pElement, bool visible);
	static void SetDisabled(MochiUIButton* pButton, bool disabled);

	static void Draw();
	static void Layout();

	static RectangleStruct ScreenRect();
	static Point2D        Resolve(const MochiUIAnchor& anchor, int w, int h);
	static int            GetFrameCount(SHPStruct* pShape);
	static MochiUIEvent   BuildEvent(MochiUIButton* pButton);

	inline static std::vector<MochiUIElement*> Items;
	inline static std::vector<MochiUIButton*>  Buttons;
	inline static bool Inited = false;
};