#include "MochiGraph.h"

MochiGraph::GraphData MochiGraph::GetCharacterBitmap(
    const wchar_t* pText,
    int nFontSize,
    ColorStruct textColor,
    int outLineSize,
    ColorStruct outLineColor,
    int boldSize)
{
    GraphData result{};

    auto* pFont = BitFont::Instance;
    if (!pFont || !pFont->InternalPTR || !pText || !*pText)
        return result;

    auto* pData = pFont->InternalPTR;

    const int fontHeight = pData->FontHeight;
    const int stride = pData->SymbolDataSize / fontHeight;

    //-----------------------------------------
    // 计算宽度
    //-----------------------------------------
    int totalWidth = 0;

    for (const wchar_t* p = pText; *p; ++p)
    {
        auto glyph = pFont->GetCharacterBitmap(*p);

        int advance = glyph ? (glyph[0] & 0x0F) : (pData->FontWidth / 2);
        totalWidth += advance + boldSize;
    }

    int width = totalWidth + outLineSize * 2;
    int height = fontHeight + outLineSize * 2;

    //-----------------------------------------
    // mask
    //-----------------------------------------
    std::vector<uint8_t> mask(width * height, 0);

    int cursorX = outLineSize;

    //-----------------------------------------
    // 画 glyph
    //-----------------------------------------
    for (const wchar_t* p = pText; *p; ++p)
    {
        auto glyph = pFont->GetCharacterBitmap(*p);

        int advance = glyph ? (glyph[0] & 0x0F) : (pData->FontWidth / 2);

        if (!glyph)
        {
            cursorX += advance + boldSize;
            continue;
        }

        BYTE* bitmap = glyph + 1;

        for (int y = 0; y < fontHeight; ++y)
        {
            BYTE* row = bitmap + y * stride;

            for (int x = 0; x < advance && x < 32 * stride; ++x)
            {
                int byteIndex = x >> 3;
                int bitIndex = 7 - (x & 7);   // ★ MSB → LSB（关键修复）

                if (row[byteIndex] & (1 << bitIndex))
                {
                    int px = cursorX + x;
                    int py = outLineSize + y;

                    if (px >= 0 && py >= 0 && px < width && py < height)
                        mask[py * width + px] = 1;
                }
            }
        }

        cursorX += advance + boldSize;
    }

    //-----------------------------------------
    // 结构
    //-----------------------------------------
    result.Width = width;
    result.Heigth = height;
    result.Data.assign(width * height, ColorStruct(0, 0, 0));

    //-----------------------------------------
    // 描边
    //-----------------------------------------
    if (outLineSize > 0)
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                if (!mask[y * width + x])
                    continue;

                for (int oy = -outLineSize; oy <= outLineSize; ++oy)
                {
                    for (int ox = -outLineSize; ox <= outLineSize; ++ox)
                    {
                        int nx = x + ox;
                        int ny = y + oy;

                        if (nx < 0 || ny < 0 || nx >= width || ny >= height)
                            continue;

                        auto& pixel = result.Data[ny * width + nx];

                        if (pixel.R == 0 && pixel.G == 0 && pixel.B == 0)
                            pixel = outLineColor;
                    }
                }
            }
        }
    }

    //-----------------------------------------
    // 填充文字颜色
    //-----------------------------------------
    for (int i = 0; i < width * height; ++i)
    {
        if (mask[i])
            result.Data[i] = textColor;
    }

    //-----------------------------------------
    //  缩放
    //-----------------------------------------
    if (nFontSize > 0 && nFontSize != fontHeight)
    {
        float scale = (float)nFontSize / (float)fontHeight;

        int newW = std::max(1, (int)(width * scale));
        int newH = std::max(1, (int)(height * scale));

        std::vector<ColorStruct> scaled(newW * newH);

        for (int y = 0; y < newH; ++y)
        {
            int sy = std::min(height - 1, (int)(y / scale));

            for (int x = 0; x < newW; ++x)
            {
                int sx = std::min(width - 1, (int)(x / scale));

                scaled[y * newW + x] =
                    result.Data[sy * width + sx];
            }
        }

        result.Width = newW;
        result.Heigth = newH;
        result.Data.swap(scaled);
    }

    return result;
}


std::vector<COLORREF> MochiGraph::RGBToInt(GraphData data)
{
    std::vector<COLORREF> result;

    if (data.Data.size() != 0 || data.Width <= 0 || data.Heigth <= 0)
        return result;

    const int pixelCount = data.Width * data.Heigth;

    result.resize(pixelCount);

    for (int i = 0; i < pixelCount; ++i)
    {
        
        result[i] = Drawing::RGB_To_Int(data.Data[i].R, data.Data[i].G, data.Data[i].B);
    }
    return result;
}

void MochiGraph::DrawText(
    const wchar_t* pText,
    DSurface* surface,
    Point2D& Location,
    bool bIsCenterAnchor,
    int fontSize,
    ColorStruct textColor,
    int outLineSize,
    ColorStruct outLineColor,
    int blodSize)
{
    auto data = MochiGraph::GetCharacterBitmap(
        pText,
        fontSize,
        textColor,
        outLineSize,
        outLineColor,
        blodSize);

    if (data.Data.empty())
        return;

    if (!surface || !surface->CanLock())
        return;

    BitFont::Instance->Lock((Surface*)surface);
    if (bIsCenterAnchor) {
        Location.X -= data.Width / 2;
    }
    BYTE* base =
        (BYTE*)surface->Buffer;

    int pitch =
        surface->GetPitch();

    int bpp =
        surface->GetBytesPerPixel(); // 关键！

    int sw = surface->Width;
    int sh = surface->Height;

    auto* src =
        data.Data.data();

    for (int y = 0; y < data.Heigth; y++)
    {
        int dstY = Location.Y + y;
        if (dstY < 0 || dstY >= sh)
            continue;

        BYTE* dstLine =
            base + dstY * pitch;

        for (int x = 0; x < data.Width; x++)
        {
            int dstX = Location.X + x;
            if (dstX < 0 || dstX >= sw)
                continue;

            const ColorStruct& c =
                src[y * data.Width + x];

            if (c.R == 0 && c.G == 0 && c.B == 0)
                continue;

            DWORD color =
                Drawing::RGB_To_Int(c.R, c.G, c.B);

            if (bpp == 2)
            {
                *(WORD*)(dstLine + dstX * 2) =
                    (WORD)color;
            }
            else
            {
                *(DWORD*)(dstLine + dstX * 4) =
                    color;
            }
        }
    }

    BitFont::Instance->UnLock((Surface*)surface);
}



void MochiGraph::DrawGraphData(
    GraphData data,
    DSurface* surface,
    Point2D& Location)
{
    
    if (data.Data.empty())
        return;

    if (!surface || !surface->CanLock())
        return;

    BitFont::Instance->Lock((Surface*)surface);

    BYTE* base =
        (BYTE*)surface->Buffer;

    int pitch =
        surface->GetPitch();

    int bpp =
        surface->GetBytesPerPixel(); // 关键！

    int sw = surface->Width;
    int sh = surface->Height;

    auto* src =
        data.Data.data();

    for (int y = 0; y < data.Heigth; y++)
    {
        int dstY = Location.Y + y;
        if (dstY < 0 || dstY >= sh)
            continue;

        BYTE* dstLine =
            base + dstY * pitch;

        for (int x = 0; x < data.Width; x++)
        {
            int dstX = Location.X + x;
            if (dstX < 0 || dstX >= sw)
                continue;

            const ColorStruct& c =
                src[y * data.Width + x];

            if (c.R == 0 && c.G == 0 && c.B == 0)
                continue;

            DWORD color =
                Drawing::RGB_To_Int(c.R, c.G, c.B);

            if (bpp == 2)
            {
                *(WORD*)(dstLine + dstX * 2) =
                    (WORD)color;
            }
            else
            {
                *(DWORD*)(dstLine + dstX * 4) =
                    color;
            }
        }
    }

    BitFont::Instance->UnLock((Surface*)surface);
}