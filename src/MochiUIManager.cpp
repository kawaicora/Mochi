#include "MochiUIManager.h"
#include "GeneralHook.h"
#include <WWMouseClass.h>
#include <algorithm>
class MochiUIGadget : public GadgetClass
{
public:
	MochiUIGadget()
		: GadgetClass(0, 0, 0, 0,
			GadgetFlag::LeftPress | GadgetFlag::LeftRelease | GadgetFlag::LeftUp | GadgetFlag::RightPress, false) {
	}

	bool Draw(bool forced) override { return true; }   // 画面由 MochiUIManager 统一按层级画

	void OnMouseEnter() override
	{
		if (!this->Owner)
			return;

		this->Owner->Hovered = true;

		if (!this->Owner->Disabled && this->Owner->OnEnter)
			this->Owner->OnEnter(MochiUIManager::BuildEvent(this->Owner));
	}

	void OnMouseLeave() override
	{
		if (!this->Owner)
			return;

		this->Owner->Hovered = false;
		this->Owner->Pressed = false;

		if (!this->Owner->Disabled && this->Owner->OnLeave)
			this->Owner->OnLeave(MochiUIManager::BuildEvent(this->Owner));
	}

	bool Clicked(DWORD* pKey, GadgetFlag flags, int x, int y, KeyModifier modifier) override
	{
		if (!this->Owner)
			return false;

		if (this->Owner->Disabled)
			return true;                        // 禁用：吃掉事件，不触发回调

		if ((flags & GadgetFlag::LeftPress) != (GadgetFlag)0)
		{
			this->Owner->Pressed = true;
			if (this->Owner->OnDown) this->Owner->OnDown(MochiUIManager::BuildEvent(this->Owner));
			return true;
		}

		if ((flags & GadgetFlag::LeftRelease) != (GadgetFlag)0
			|| (flags & GadgetFlag::LeftUp) != (GadgetFlag)0)
		{
			if (!this->Owner->Pressed)
				return true;

			this->Owner->Pressed = false;
			if (this->Owner->OnUp) this->Owner->OnUp(MochiUIManager::BuildEvent(this->Owner));
			return true;
		}

		return false;
	}

	MochiUIButton* Owner = nullptr;
};

void MochiUIManager::Init()
{
	if (Inited)
		return;
	GeneralHook::GScreenClassDrawOnTopEvent.Subscribe([]() { MochiUIManager::Draw(); });
	Inited = true;
}

void MochiUIManager::Destroy()
{
	for (auto pButton : Buttons)
	{
		if (pButton->Gadget)
		{
			GScreenClass::Instance.RemoveButton(pButton->Gadget);
			GameDelete(pButton->Gadget);
		}
		delete pButton;
	}

	for (auto pElement : Items)
	{
		if (pElement->Owner)                 // 按钮画布随 Button 一起释放了
			continue;
		delete pElement;
	}

	Buttons.clear();
	Items.clear();
	Inited = false;
}

RectangleStruct MochiUIManager::ScreenRect()
{
	return DSurface::Primary->GetRect();
}

Point2D MochiUIManager::Resolve(const MochiUIAnchor& anchor, int w, int h)
{
	RectangleStruct screen = ScreenRect();
	float sx = anchor.ScaleOffset ? (float)screen.Width / 800.0f : 1.0f;
	float sy = anchor.ScaleOffset ? (float)screen.Height / 600.0f : 1.0f;

	int x = screen.X + (int)(anchor.X * screen.Width) + (int)(anchor.OffsetX * sx);
	int y = screen.Y + (int)(anchor.Y * screen.Height) + (int)(anchor.OffsetY * sy);

	switch (anchor.Pivot)
	{
	case 0: break;
	case 1: x -= w / 2; break;
	case 2: x -= w; break;
	case 3: y -= h / 2; break;
	case 4: x -= w / 2; y -= h / 2; break;
	case 5: x -= w; y -= h / 2; break;
	case 6: y -= h; break;
	case 7: x -= w / 2; y -= h; break;
	case 8: x -= w; y -= h; break;
	default: break;
	}

	return Point2D{ x, y };
}

int MochiUIManager::GetFrameCount(SHPStruct* pShape)
{
	if (!pShape)
		return 1;

	if (pShape->IsReference())
	{
		SHPReference* pRef = pShape->AsReference();
		if (!pRef->Data) pShape->Load();
		if (pRef->Data && pRef->Data->Frames > 0)
			return pRef->Data->Frames;
	}

	return pShape->Frames > 0 ? pShape->Frames : 1;
}

MochiUIEvent MochiUIManager::BuildEvent(MochiUIButton* pButton)
{
	MochiUIEvent evt{};
	if (!pButton)
		return evt;

	Point2D mouse = { 0, 0 };
	WWMouseClass::Instance->GetCoords(&mouse);

	evt.Button = pButton;
	evt.MouseX = mouse.X;
	evt.MouseY = mouse.Y;
	evt.X = pButton->Canvas.X;
	evt.Y = pButton->Canvas.Y;
	evt.Width = pButton->Canvas.Width;
	evt.Height = pButton->Canvas.Height;
	evt.Disabled = pButton->Disabled;
	return evt;
}

MochiUIElement* MochiUIManager::RegisterImage(SHPStruct* pShp, int x, int y, int layer, ConvertClass* pPalette)
{
	MochiUIElement* pElement = new MochiUIElement();
	pElement->Shape = pShp;
	pElement->Palette = pPalette;
	pElement->Anchor = MochiUIAnchor{ 0.0f, 0.0f, x, y, 0, false };   // 绝对像素
	pElement->Layer = layer;
	pElement->FrameCount = GetFrameCount(pShp);
	if (pShp)
	{
		pElement->Width = pShp->Width;
		pElement->Height = pShp->Height;
	}
	Items.push_back(pElement);
	return pElement;
}

MochiUIElement* MochiUIManager::RegisterAnimation(SHPStruct* pShp, int x, int y, int layer,
	int delay, bool loop, ConvertClass* pPalette)
{
	MochiUIElement* pElement = RegisterImage(pShp, x, y, layer, pPalette);
	pElement->FrameDelay = delay > 0 ? delay : 1;
	pElement->Loop = loop;
	return pElement;
}

MochiUIButton* MochiUIManager::RegisterButton(SHPStruct* pShp, int x, int y, int w, int h,
	int layer, bool isDisabled,
	std::function<void(const MochiUIEvent&)> onEnter,
	std::function<void(const MochiUIEvent&)> onLeave,
	std::function<void(const MochiUIEvent&)> onHover,
	std::function<void(const MochiUIEvent&)> onDown,
	std::function<void(const MochiUIEvent&)> onUp,
	int id)
{
	MochiUIButton* pButton = new MochiUIButton();
	pButton->Id = id;
	pButton->Disabled = isDisabled;
	pButton->OnEnter = onEnter;
	pButton->OnLeave = onLeave;
	pButton->OnHover = onHover;
	pButton->OnDown = onDown;
	pButton->OnUp = onUp;

	pButton->Canvas.Owner = pButton;
	pButton->Canvas.Shape = pShp;
	pButton->Canvas.Anchor = MochiUIAnchor{ 0.0f, 0.0f, x, y, 0, false };
	pButton->Canvas.Layer = layer;
	pButton->Canvas.FrameCount = GetFrameCount(pShp);
	pButton->Canvas.Width = w > 0 ? w : (pShp ? pShp->Width : 0);
	pButton->Canvas.Height = h > 0 ? h : (pShp ? pShp->Height : 0);

	MochiUIGadget* pGadget = GameCreate<MochiUIGadget>();
	pGadget->Owner = pButton;
	pGadget->Zap();
	GScreenClass::Instance.AddButton(pGadget);
	pButton->Gadget = pGadget;

	Items.push_back(&pButton->Canvas);
	Buttons.push_back(pButton);
	return pButton;
}

void MochiUIManager::RemoveButton(int id)
{
	for (size_t i = 0; i < Buttons.size(); ++i)
	{
		if (Buttons[i]->Id != id)
			continue;

		MochiUIButton* pButton = Buttons[i];
		if (pButton->Gadget)
		{
			GScreenClass::Instance.RemoveButton(pButton->Gadget);
			GameDelete(pButton->Gadget);
		}

		for (size_t j = 0; j < Items.size(); ++j)
		{
			if (Items[j] == &pButton->Canvas)
			{
				Items.erase(Items.begin() + j);
				break;
			}
		}

		delete pButton;
		Buttons.erase(Buttons.begin() + i);
		return;
	}
}

void MochiUIManager::RemoveElement(MochiUIElement* pElement)
{
	if (!pElement || pElement->Owner)      // 按钮画布走 RemoveButton
		return;

	for (size_t i = 0; i < Items.size(); ++i)
	{
		if (Items[i] != pElement)
			continue;

		Items.erase(Items.begin() + i);
		delete pElement;
		return;
	}
}

void MochiUIManager::SetLayer(MochiUIElement* pElement, int layer)
{
	if (pElement)
		pElement->Layer = layer;
}

void MochiUIManager::SetVisible(MochiUIElement* pElement, bool visible)
{
	if (pElement)
		pElement->Visible = visible;
}

void MochiUIManager::SetDisabled(MochiUIButton* pButton, bool disabled)
{
	if (!pButton)
		return;

	pButton->Disabled = disabled;
	if (disabled)
	{
		pButton->Pressed = false;
		pButton->Hovered = false;
	}
}

void MochiUIManager::Layout()
{
	for (auto pElement : Items)
	{
		Point2D pos = Resolve(pElement->Anchor, pElement->Width, pElement->Height);
		pElement->X = pos.X;
		pElement->Y = pos.Y;

		if (!pElement->Owner || !pElement->Owner->Gadget)
			continue;

		pElement->Owner->Gadget->SetPosition(pElement->X, pElement->Y);
		pElement->Owner->Gadget->SetDimension(pElement->Width, pElement->Height);
	}
}

void MochiUIManager::Draw()
{
	if (!Inited)
		return;

	Layout();

	// 层级：Layer 小的先画（在底层），同层按注册顺序（stable_sort 保证）
	std::stable_sort(Items.begin(), Items.end(),
		[](MochiUIElement* a, MochiUIElement* b) { return a->Layer < b->Layer; });

	ConvertClass* pDefaultPalette = FileSystem::PALETTE_PAL;
	RectangleStruct bounds = ScreenRect();

	for (auto pElement : Items)
	{
		if (!pElement->Visible || !pElement->Shape)
			continue;

		MochiUIButton* pButton = pElement->Owner;

		// 悬停回调：每帧一次
		if (pButton && pButton->Hovered && !pButton->Disabled && pButton->OnHover)
			pButton->OnHover(BuildEvent(pButton));

		int frame = 0;
		int brightness = 1000;

		if (pButton)
		{
			if (pButton->Disabled)
				brightness = pButton->DisabledBrightness;
			else if (pButton->Pressed && pButton->PressedFrame >= 0)
				frame = pButton->PressedFrame;
			else if (pButton->Hovered && pButton->HoverFrame >= 0)
				frame = pButton->HoverFrame;
			else if (pElement->FrameCount > 1)
				frame = (int)(Unsorted::CurrentFrame / pElement->FrameDelay) % pElement->FrameCount;
		}
		else if (pElement->FrameCount > 1)
		{
			int step = (int)(Unsorted::CurrentFrame / pElement->FrameDelay);
			pElement->Frame = pElement->Loop
				? step % pElement->FrameCount
				: (step < pElement->FrameCount ? step : pElement->FrameCount - 1);
			frame = pElement->Frame;
		}

		Point2D pos = { pElement->X, pElement->Y };
		DSurface::Composite->DrawSHP(pElement->Palette ? pElement->Palette : pDefaultPalette,
			pElement->Shape, frame, &pos, &bounds, BlitterFlags::bf_400,
			0, 0, ZGradient::Ground, brightness, 0, nullptr, 0, 0, 0);
	}
}
