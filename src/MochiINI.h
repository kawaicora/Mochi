#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <YRPP.h>

class MochiINI
{
public:
   
	//***********************RA2 INIClass ***********************
    inline static INIClass* OpenConfig(const char* file);
    //***********************RA2 INIClass ***********************

    struct Value
    {
        std::string Raw;
        std::vector<std::string> Array;
        int AsInt(int defaultValue = 0) const;
        float AsFloat(float defaultValue = 0.0f) const;
        bool AsBool(bool defaultValue = false) const;
        const char* c_str() const;
        
    };

    struct Section
    {
        std::unordered_map<std::string, Value> Keys;

        bool HasKey(const std::string& key) const;

        Value& operator[](const std::string& key);
        

        const Value* Get(const std::string& key) const;

        template<typename T>
        T GetValueByKey(
            const std::string& key,
            const T& defaultValue = T()) const;
    };
    std::unordered_map<std::string, Section> Sections;
    bool HasSection(const std::string& section) const;
    Section& operator[](const std::string& section);
    const Section* GetSection(const std::string& section) const;
    bool LoadINI(
        const char* filename,
        const char* searchPaths[] = nullptr);
    void Clear();
    static std::vector<std::string>
        Split(
            const std::string& src,
            char split_suffix = ',');
    static MochiINI INIReader;
    static MochiINI::Section* PlayerBaseConfig;


private:

    static bool IsNumber(const std::string& s);
    static std::string ToLower(std::string s);
    static std::string ToUpper(std::string s);
    static std::string Trim(const std::string& s);
    static std::string RemoveQuotes(const std::string& s);
    static std::string RemoveComment(
        const std::string& line);
    static std::vector<std::string>
        SplitArray(const std::string& s);
};

#include "MochiINI.inl"