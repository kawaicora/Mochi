#pragma once
#include <YRPP.h>
#include<BitFont.h>
#include <vector>
#include <filesystem>
class MochiGraph
{
public:
    inline static BitFont::InternalData* __fastcall LoadInternalData(const char* pFileName)
    {
        JMP_STD(0x433990);
    }
    struct GraphData {
        int Width;
        int Heigth;
        std::vector<ColorStruct> Data;
    };

    static ColorStruct GetComplementColor(const ColorStruct& c);
   
    static GraphData GetCharacterBitmap(
        const wchar_t* pText,
        int nFontSize,
        ColorStruct textColor,
        int outLineSize,
        ColorStruct outLineColor,
        int boldSize);



    static std::vector<COLORREF> RGBToInt(GraphData data);

    static void DrawGraphData(
        GraphData data,
        DSurface* surface,
        Point2D& Location);


    static void DrawText(const wchar_t* pText,
        DSurface* surface, 
        Point2D& Location,
        bool bIsCenterAnchor,
        int fontSize,
        ColorStruct textColor,
        int outLineSize,
        ColorStruct outLineColor,
        int blodSize);
};

