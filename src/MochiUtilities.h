#pragma once

#include <Windows.h>
#include <wincodec.h>
#include <mmsystem.h>

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <locale>
#include <codecvt>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "winmm.lib")

// RA2 / YR
#include <Surface.h>
#include <Drawing.h>
#include <GeneralDefinitions.h>

class MochiUtilities
{
public:

    // ============================================================
    // PIXEL
    // ============================================================

    struct Pixel
    {
        BYTE B;
        BYTE G;
        BYTE R;
        BYTE A;
    };

    // ============================================================
    // IMAGE
    // ============================================================

    struct Image
    {
        int Width = 0;
        int Height = 0;

        std::vector<Pixel> Pixels;

        bool Empty() const
        {
            return Pixels.empty();
        }
    };

    // ============================================================
    // SPRITE
    // ============================================================

    struct Sprite
    {
        bool Visible = false;

        Image ImageData;

        int X = 0;
        int Y = 0;

        int Width = 0;
        int Height = 0;
    };

    // ============================================================
    // SCRIPT COMMAND
    // ============================================================

    struct ScriptCommand
    {
        std::wstring Command;

        std::vector<std::wstring> Args;
    };

public:

    static inline IWICImagingFactory* WICFactory = nullptr;

    static inline WORD* FrameBuffer = nullptr;

    static inline int FramePitch = 0;

    static inline int ScreenWidth = 0;

    static inline int ScreenHeight = 0;

    // ============================================================
    // VN STATE
    // ============================================================

    static inline Sprite Background;

    static inline Sprite Avatar;

    static inline Image TextImage;

    static inline bool WaitingMouse = false;

    static inline std::vector<ScriptCommand> Script;

    static inline std::map<std::wstring, int> Labels;

    static inline int ScriptIndex = 0;

    static inline bool MouseClicked = false;

public:

    // ============================================================
    // INIT
    // ============================================================

    static bool Init()
    {
        CoInitializeEx(
            nullptr,
            COINIT_MULTITHREADED);

        HRESULT hr =
            CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&WICFactory));

        return SUCCEEDED(hr);
    }

    // ============================================================
    // RGB565
    // ============================================================

    static WORD RGB565(
        BYTE r,
        BYTE g,
        BYTE b)
    {
        return
            ((r >> 3) << 11)
            |
            ((g >> 2) << 5)
            |
            (b >> 3);
    }

    // ============================================================
    // LOAD IMAGE
    // ============================================================

    static Image LoadImage(
        const wchar_t* filename)
    {
        Image result;

        if (!WICFactory)
            return result;

        IWICBitmapDecoder* decoder = nullptr;

        HRESULT hr =
            WICFactory->CreateDecoderFromFilename(
                filename,
                nullptr,
                GENERIC_READ,
                WICDecodeMetadataCacheOnLoad,
                &decoder);

        if (FAILED(hr))
            return result;

        IWICBitmapFrameDecode* frame = nullptr;

        decoder->GetFrame(
            0,
            &frame);

        IWICFormatConverter* converter = nullptr;

        WICFactory->CreateFormatConverter(
            &converter);

        converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);

        UINT width = 0;
        UINT height = 0;

        converter->GetSize(
            &width,
            &height);

        result.Width =
            (int)width;

        result.Height =
            (int)height;

        result.Pixels.resize(
            width * height);

        converter->CopyPixels(
            nullptr,
            width * 4,
            width * height * 4,
            (BYTE*)result.Pixels.data());

        converter->Release();
        frame->Release();
        decoder->Release();

        return result;
    }

    // ============================================================
    // CREATE TEXT IMAGE
    // ============================================================

    static Image CreateTextImage(
        const std::wstring& text,
        int width,
        int height,
        int fontSize,
        COLORREF color)
    {
        Image result;

        result.Width = width;
        result.Height = height;

        result.Pixels.resize(
            width * height);

        HDC dc =
            CreateCompatibleDC(nullptr);

        BITMAPINFO bmi = {};

        bmi.bmiHeader.biSize =
            sizeof(BITMAPINFOHEADER);

        bmi.bmiHeader.biWidth =
            width;

        bmi.bmiHeader.biHeight =
            -height;

        bmi.bmiHeader.biPlanes =
            1;

        bmi.bmiHeader.biBitCount =
            32;

        bmi.bmiHeader.biCompression =
            BI_RGB;

        void* bits = nullptr;

        HBITMAP bmp =
            CreateDIBSection(
                dc,
                &bmi,
                DIB_RGB_COLORS,
                &bits,
                nullptr,
                0);

        SelectObject(dc, bmp);

        RECT rc =
        {
            0,
            0,
            width,
            height
        };

        HBRUSH brush =
            CreateSolidBrush(
                RGB(0, 0, 0));

        FillRect(
            dc,
            &rc,
            brush);

        DeleteObject(brush);

        HFONT font =
            CreateFontW(
                fontSize,
                0,
                0,
                0,
                FW_NORMAL,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                ANTIALIASED_QUALITY,
                DEFAULT_PITCH,
                L"Microsoft YaHei");

        SelectObject(dc, font);

        SetBkMode(
            dc,
            TRANSPARENT);

        SetTextColor(
            dc,
            color);

        DrawTextW(
            dc,
            text.c_str(),
            -1,
            &rc,
            DT_LEFT |
            DT_TOP |
            DT_WORDBREAK);

        DWORD* src =
            (DWORD*)bits;

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                DWORD p =
                    src[
                        y * width + x];

                Pixel px;

                px.B =
                    (p >> 0) & 0xFF;

                px.G =
                    (p >> 8) & 0xFF;

                px.R =
                    (p >> 16) & 0xFF;

                if (
                    px.R == 0 &&
                    px.G == 0 &&
                    px.B == 0)
                {
                    px.A = 0;
                }
                else
                {
                    px.A = 255;
                }

                result.Pixels[
                    y * width + x] = px;
            }
        }

        DeleteObject(font);
        DeleteObject(bmp);
        DeleteDC(dc);

        return result;
    }

    // ============================================================
    // BEGIN FRAME
    // ============================================================

    static bool BeginFrame(
        DSurface* surface)
    {
        if (!surface)
            return false;

        FrameBuffer =
            (WORD*)surface->Lock(0, 0);

        if (!FrameBuffer)
            return false;

        FramePitch =
            surface->GetPitch() / 2;

        ScreenWidth =
            surface->GetWidth();

        ScreenHeight =
            surface->GetHeight();

        return true;
    }

    // ============================================================
    // END FRAME
    // ============================================================

    static void EndFrame(
        DSurface* surface)
    {
        if (surface)
        {
            surface->Unlock();
        }

        FrameBuffer = nullptr;
    }

    // ============================================================
    // DRAW IMAGE SCALED
    // ============================================================

    static void DrawImageScaled(
        const Image& image,
        int drawX,
        int drawY,
        int drawWidth,
        int drawHeight)
    {
        if (!FrameBuffer)
            return;

        if (image.Empty())
            return;

        if (drawWidth <= 0)
            return;

        if (drawHeight <= 0)
            return;

        for (int y = 0; y < drawHeight; y++)
        {
            int screenY =
                drawY + y;

            if (screenY < 0)
                continue;

            if (screenY >= ScreenHeight)
                continue;

            int srcY =
                (y * image.Height)
                / drawHeight;

            WORD* row =
                FrameBuffer
                + screenY * FramePitch;

            for (int x = 0; x < drawWidth; x++)
            {
                int screenX =
                    drawX + x;

                if (screenX < 0)
                    continue;

                if (screenX >= ScreenWidth)
                    continue;

                int srcX =
                    (x * image.Width)
                    / drawWidth;

                const Pixel& pixel =
                    image.Pixels[
                        srcY * image.Width
                            + srcX];

                if (pixel.A == 0)
                    continue;

                row[screenX] =
                    RGB565(
                        pixel.R,
                        pixel.G,
                        pixel.B);
            }
        }
    }

    // ============================================================
    // PLAY SOUND
    // ============================================================

    static void PlaySoundFile(
        const wchar_t* filename)
    {
        PlaySoundW(
            filename,
            nullptr,
            SND_FILENAME |
            SND_ASYNC |
            SND_LOOP);
    }

    // ============================================================
    // PARSE SIZE
    // ============================================================

    static int ParseSize(
        const std::wstring& value,
        int imageSize)
    {
        if (value == L"auto")
        {
            return imageSize;
        }

        return _wtoi(value.c_str());
    }

    // ============================================================
    // PARSE LINE
    // ============================================================

    static ScriptCommand ParseLine(
        const std::wstring& line)
    {
        ScriptCommand cmd;

        std::wstringstream ss(line);

        ss >> cmd.Command;

        std::wstring arg;

        while (ss >> arg)
        {
            cmd.Args.push_back(arg);
        }

        return cmd;
    }

    // ============================================================
    // LOAD SCRIPT UTF8
    // ============================================================

    static void LoadScript(
        const wchar_t* filename)
    {
        Script.clear();
        Labels.clear();

        std::ifstream file(
            filename,
            std::ios::binary);

        if (!file.is_open())
            return;

        std::string utf8(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        std::wstring_convert
            <
            std::codecvt_utf8_utf16<wchar_t>
            > converter;

        std::wstring content =
            converter.from_bytes(utf8);

        std::wstringstream stream(content);

        std::wstring line;

        int index = 0;

        while (std::getline(stream, line))
        {
            if (!line.empty() &&
                line.back() == L'\r')
            {
                line.pop_back();
            }

            if (line.empty())
                continue;

            if (line[0] == L';')
                continue;

            if (line[0] == L':')
            {
                Labels[
                    line.substr(1)] = index;

                continue;
            }

            Script.push_back(
                ParseLine(line));

            index++;
        }
    }

    // ============================================================
    // EXECUTE COMMAND
    // ============================================================

    static void ExecuteCommand(
        const ScriptCommand& cmd)
    {
        // =====================================
        // ShowBackground
        // =====================================

        if (cmd.Command == L"ShowBackground")
        {
            Background.Visible = true;

            Background.ImageData =
                LoadImage(
                    cmd.Args[0].c_str());

            Background.X =
                _wtoi(cmd.Args[1].c_str());

            Background.Y =
                _wtoi(cmd.Args[2].c_str());

            Background.Width =
                ParseSize(
                    cmd.Args[3],
                    Background.ImageData.Width);

            Background.Height =
                ParseSize(
                    cmd.Args[4],
                    Background.ImageData.Height);
        }

        // =====================================
        // ShowAvatar
        // =====================================

        else if (cmd.Command == L"ShowAvatar")
        {
            Avatar.Visible = true;

            Avatar.ImageData =
                LoadImage(
                    cmd.Args[0].c_str());

            Avatar.X =
                _wtoi(cmd.Args[1].c_str());

            Avatar.Y =
                _wtoi(cmd.Args[2].c_str());

            Avatar.Width =
                ParseSize(
                    cmd.Args[3],
                    Avatar.ImageData.Width);

            Avatar.Height =
                ParseSize(
                    cmd.Args[4],
                    Avatar.ImageData.Height);
        }

        // =====================================
        // ShowText
        // =====================================

        else if (cmd.Command == L"ShowText")
        {
            std::wstring text;

            for (size_t i = 0;
                i < cmd.Args.size();
                i++)
            {
                text += cmd.Args[i];

                if (i + 1 <
                    cmd.Args.size())
                {
                    text += L" ";
                }
            }

            TextImage =
                CreateTextImage(
                    text,
                    1000,
                    120,
                    32,
                    RGB(255, 255, 255));
        }

        // =====================================
        // PlaySound
        // =====================================

        else if (cmd.Command == L"PlaySound")
        {
            PlaySoundFile(
                cmd.Args[1].c_str());
        }

        // =====================================
        // WaitMouse
        // =====================================

        else if (cmd.Command == L"WaitMouse")
        {
            WaitingMouse = true;
        }

        // =====================================
        // Jump
        // =====================================

        else if (cmd.Command == L"Jump")
        {
            auto label =
                cmd.Args[0];

            if (label[0] == L'@')
            {
                label.erase(0, 1);
            }

            auto it =
                Labels.find(label);

            if (it != Labels.end())
            {
                ScriptIndex =
                    it->second;
            }
        }
    }

    // ============================================================
    // UPDATE SCRIPT
    // ============================================================

    static void UpdateScript()
    {
        if (WaitingMouse)
        {
            if (!MouseClicked)
                return;

            WaitingMouse = false;

            MouseClicked = false;
        }

        if (ScriptIndex >=
            (int)Script.size())
        {
            return;
        }

        ExecuteCommand(
            Script[ScriptIndex]);

        ScriptIndex++;
    }

    // ============================================================
    // RENDER
    // ============================================================

    static void Render()
    {
        DSurface* surface =
            DSurface::Composite;

        if (!surface)
            return;

        if (!BeginFrame(surface))
            return;

        if (Background.Visible)
        {
            DrawImageScaled(
                Background.ImageData,
                Background.X,
                Background.Y,
                Background.Width,
                Background.Height);
        }

        if (Avatar.Visible)
        {
            DrawImageScaled(
                Avatar.ImageData,
                Avatar.X,
                Avatar.Y,
                Avatar.Width,
                Avatar.Height);
        }

        DrawImageScaled(
            TextImage,
            40,
            580,
            TextImage.Width,
            TextImage.Height);

        EndFrame(surface);
    }

    // ============================================================
    // UPDATE
    // CALL FROM RA2 LOGIC LOOP
    // ============================================================

    static void Update()
    {
        UpdateScript();

        Render();
    }

    // ============================================================
    // INPUT
    // ============================================================

    static void OnMouseDown()
    {
        MouseClicked = true;
    }
};