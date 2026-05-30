#include "MochiINI.h"
MochiINI MochiINI::INIReader;
MochiINI::Section* MochiINI::PlayerBaseConfig;
//***********************RA2 INIClass ***********************
static INIClass* OpenConfig(const char* file) {
    CCINIClass* pINI = GameCreate<CCINIClass>();

    if (pINI) {
        CCFileClass* cfg = GameCreate<CCFileClass>(file);

        if (cfg) {
            if (cfg->Exists()) {
                pINI->ReadCCFile(cfg);
            }
            GameDelete(cfg);
        }
    }

    return pINI;
};
//***********************RA2 INIClass ***********************



bool MochiINI::HasSection(const std::string& section) const
{
    return MochiINI::Sections.find(MochiINI::ToUpper(section)) != MochiINI::Sections.end();
}

MochiINI::Section& MochiINI::operator[](const std::string& section)
{
    return MochiINI::Sections[MochiINI::ToUpper(section)];
}

const MochiINI::Section* MochiINI::GetSection(const std::string& section) const
{
    auto it = MochiINI::Sections.find(MochiINI::ToUpper(section));

    if (it == MochiINI::Sections.end())
        return nullptr;

    return &it->second;
}

bool MochiINI::LoadINI(
    const char* filename,
    const char* searchPaths[])
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

void MochiINI::Clear()
{
    Sections.clear();
}
std::vector<std::string> MochiINI::Split(
        const std::string& src,
        char split_suffix)
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


bool MochiINI::IsNumber(const std::string& s)
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

std::string MochiINI::ToLower(std::string s)
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

std::string MochiINI::ToUpper(std::string s)
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

std::string MochiINI::Trim(const std::string& s)
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

std::string MochiINI::RemoveQuotes(const std::string& s)
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

std::string MochiINI::RemoveComment(
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

std::vector<std::string> MochiINI::SplitArray(const std::string& s)
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
};

int MochiINI::Value::AsInt(int defaultValue) const{
    try
    {
        return std::stoi(Raw);
    }
    catch (...)
    {
        return defaultValue;
    }
}
float MochiINI::Value::AsFloat(float defaultValue) const
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

bool MochiINI::Value::AsBool(bool defaultValue) const
{
    std::string s = ToLower(Raw);

    if (s == "true" || s == "yes" || s == "1")
        return true;

    if (s == "false" || s == "no" || s == "0")
        return false;

    return defaultValue;
}

const char* MochiINI::Value::c_str() const
{
    return Raw.c_str();
}



bool MochiINI::Section::HasKey(const std::string& key) const 
{
    return Keys.find(ToUpper(key)) != Keys.end();
}

MochiINI::Value& MochiINI::Section::operator[](const std::string& key)
{
    return Keys[ToUpper(key)];
}


const MochiINI::Value* MochiINI::Section::Get(const std::string& key) const
{
    auto it = Keys.find(ToUpper(key));

    if (it == Keys.end())
        return nullptr;

    return &it->second;
}