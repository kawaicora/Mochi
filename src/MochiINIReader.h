#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <cctype>

class MochiINIReader
{
public:

    struct Value
    {
        std::string Raw;
        std::vector<std::string> Array;

        inline int AsInt(int defaultValue = 0) const
        {
            try
            {
                return std::stoi(Raw);
            }
            catch (...)
            {
                return defaultValue;
            }
        }

        inline float AsFloat(float defaultValue = 0.0f) const
        {
            try
            {
                return std::stof(Raw);
            }
            catch (...)
            {
                return defaultValue;
            }
        }

        inline bool AsBool(bool defaultValue = false) const
        {
            std::string s = ToLower(Raw);

            if (s == "true" || s == "yes" || s == "1")
                return true;

            if (s == "false" || s == "no" || s == "0")
                return false;

            return defaultValue;
        }

        inline const char* c_str() const
        {
            return Raw.c_str();
        }
    };

    struct Section
    {
        std::unordered_map<std::string, Value> Keys;

        inline bool HasKey(const std::string& key) const
        {
            return Keys.find(ToUpper(key)) != Keys.end();
        }

        inline Value& operator[](const std::string& key)
        {
            return Keys[ToUpper(key)];
        }

        inline const Value* Get(const std::string& key) const
        {
            auto it = Keys.find(ToUpper(key));

            if (it == Keys.end())
                return nullptr;

            return &it->second;
        }

        template<typename T>
        inline T GetValueByKey(
            const std::string& key,
            const T& defaultValue = T()) const;
    };

public:

    std::unordered_map<std::string, Section> Sections;

public:

    inline bool HasSection(const std::string& section) const
    {
        return Sections.find(ToUpper(section)) != Sections.end();
    }

    inline Section& operator[](const std::string& section)
    {
        return Sections[ToUpper(section)];
    }

    inline const Section* GetSection(const std::string& section) const
    {
        auto it = Sections.find(ToUpper(section));

        if (it == Sections.end())
            return nullptr;

        return &it->second;
    }

public:

    inline bool LoadINI(
        const char* filename,
        const char* searchPaths[] = nullptr)
    {
        Clear();

        std::vector<std::filesystem::path> paths;

        //
        // 默认扫描根目录
        //
        paths.emplace_back(".");

        if (searchPaths)
        {
            for (size_t i = 0; searchPaths[i]; ++i)
            {
                paths.emplace_back(searchPaths[i]);
            }
        }

        std::filesystem::path foundPath;

        for (auto& p : paths)
        {
            auto full = p / filename;

            if (std::filesystem::exists(full))
            {
                foundPath = full;
                break;
            }
        }

        if (foundPath.empty())
            return false;

        std::ifstream file(foundPath);

        if (!file.is_open())
            return false;

        std::string line;
        std::string currentSection;

        while (std::getline(file, line))
        {
            //
            // UTF8 BOM
            //
            if (!line.empty() &&
                (unsigned char)line[0] == 0xEF)
            {
                if (line.size() >= 3 &&
                    (unsigned char)line[1] == 0xBB &&
                    (unsigned char)line[2] == 0xBF)
                {
                    line.erase(0, 3);
                }
            }

            line = RemoveComment(line);
            line = Trim(line);

            if (line.empty())
                continue;

            //
            // [Section]
            //
            if (line.front() == '[' &&
                line.back() == ']')
            {
                currentSection =
                    ToUpper(
                        Trim(
                            line.substr(1, line.size() - 2)));

                continue;
            }

            if (currentSection.empty())
                continue;

            Section& sec = Sections[currentSection];

            //
            // +=
            //
            auto appendPos = line.find("+=");

            if (appendPos != std::string::npos)
            {
                std::string value =
                    Trim(line.substr(appendPos + 2));

                //
                // 找最大数字 key
                //
                int maxIndex = -1;

                for (auto& kv : sec.Keys)
                {
                    if (!IsNumber(kv.first))
                        continue;

                    int idx = atoi(kv.first.c_str());

                    if (idx > maxIndex)
                    {
                        maxIndex = idx;
                    }
                }

                int newIndex = maxIndex + 1;

                Value v;

                v.Raw = RemoveQuotes(value);
                v.Array = SplitArray(value);

                sec.Keys[std::to_string(newIndex)] =
                    std::move(v);

                continue;
            }

            //
            // key=value
            //
            auto pos = line.find('=');

            if (pos == std::string::npos)
                continue;

            std::string key =
                ToUpper(
                    Trim(line.substr(0, pos)));

            std::string value =
                Trim(line.substr(pos + 1));

            Value v;

            v.Raw = RemoveQuotes(value);
            v.Array = SplitArray(value);

            sec.Keys[key] = std::move(v);
        }

        return true;
    }

    inline void Clear()
    {
        Sections.clear();
    }
public:
    inline static std::vector<std::string>
        Split(
            const std::string& src,
            char split_suffix = ',')
    {
        std::vector<std::string> result;

        std::string current;

        bool inDouble = false;
        bool inSingle = false;

        for (char c : src)
        {
            //
            // "
            //
            if (c == '"' && !inSingle)
            {
                inDouble = !inDouble;
                continue;
            }

            //
            // '
            //
            if (c == '\'' && !inDouble)
            {
                inSingle = !inSingle;
                continue;
            }

            //
            // split
            //
            if (c == split_suffix &&
                !inDouble &&
                !inSingle)
            {
                auto v =
                    RemoveQuotes(
                        Trim(current));

                if (!v.empty())
                {
                    result.push_back(v);
                }

                current.clear();

                continue;
            }

            current += c;
        }

        //
        // last
        //
        auto v =
            RemoveQuotes(
                Trim(current));

        if (!v.empty())
        {
            result.push_back(v);
        }

        return result;
    }
private:

    inline static bool IsNumber(const std::string& s)
    {
        if (s.empty())
            return false;

        for (char c : s)
        {
            if (!isdigit((unsigned char)c))
                return false;
        }

        return true;
    }

    inline static std::string ToLower(std::string s)
    {
        std::transform(
            s.begin(),
            s.end(),
            s.begin(),
            [](unsigned char c)
            {
                return (char)std::tolower(c);
            });

        return s;
    }

    inline static std::string ToUpper(std::string s)
    {
        std::transform(
            s.begin(),
            s.end(),
            s.begin(),
            [](unsigned char c)
            {
                return (char)std::toupper(c);
            });

        return s;
    }

    inline static std::string Trim(const std::string& s)
    {
        size_t start = 0;

        while (start < s.size() &&
            std::isspace((unsigned char)s[start]))
        {
            ++start;
        }

        size_t end = s.size();

        while (end > start &&
            std::isspace((unsigned char)s[end - 1]))
        {
            --end;
        }

        return s.substr(start, end - start);
    }

    inline static std::string RemoveQuotes(const std::string& s)
    {
        if (s.size() >= 2)
        {
            if (
                (s.front() == '"' && s.back() == '"') ||
                (s.front() == '\'' && s.back() == '\''))
            {
                return s.substr(1, s.size() - 2);
            }
        }

        return s;
    }

    inline static std::string RemoveComment(
        const std::string& line)
    {
        bool inDouble = false;
        bool inSingle = false;

        for (size_t i = 0; i < line.size(); ++i)
        {
            char c = line[i];

            if (c == '"' && !inSingle)
            {
                inDouble = !inDouble;
            }
            else if (c == '\'' && !inDouble)
            {
                inSingle = !inSingle;
            }
            else if (c == ';' &&
                !inDouble &&
                !inSingle)
            {
                return line.substr(0, i);
            }
        }

        return line;
    }

    inline static std::vector<std::string>
        SplitArray(const std::string& s)
    {
        std::vector<std::string> result;

        std::string current;

        bool inDouble = false;
        bool inSingle = false;

        for (char c : s)
        {
            if (c == '"' && !inSingle)
            {
                inDouble = !inDouble;
                continue;
            }

            if (c == '\'' && !inDouble)
            {
                inSingle = !inSingle;
                continue;
            }

            if (c == ',' &&
                !inDouble &&
                !inSingle)
            {
                auto v =
                    RemoveQuotes(
                        Trim(current));

                if (!v.empty())
                    result.push_back(v);

                current.clear();
                continue;
            }

            current += c;
        }

        auto v =
            RemoveQuotes(
                Trim(current));

        if (!v.empty())
            result.push_back(v);

        return result;
    }
};

//
// template specializations
//

template<>
inline int MochiINIReader::Section::GetValueByKey<int>(
    const std::string& key,
    const int& defaultValue) const
{
    auto v = Get(key);

    if (!v)
        return defaultValue;

    return v->AsInt(defaultValue);
}

template<>
inline float MochiINIReader::Section::GetValueByKey<float>(
    const std::string& key,
    const float& defaultValue) const
{
    auto v = Get(key);

    if (!v)
        return defaultValue;

    return v->AsFloat(defaultValue);
}

template<>
inline bool MochiINIReader::Section::GetValueByKey<bool>(
    const std::string& key,
    const bool& defaultValue) const
{
    auto v = Get(key);

    if (!v)
        return defaultValue;

    return v->AsBool(defaultValue);
}

template<>
inline std::string
MochiINIReader::Section::GetValueByKey<std::string>(
    const std::string& key,
    const std::string& defaultValue) const
{
    auto v = Get(key);

    if (!v)
        return defaultValue;

    return v->Raw;
}