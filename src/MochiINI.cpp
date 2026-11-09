#include "MochiINI.h"
#include "Debug.h"
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

std::vector<std::string> MochiINI::TextIncludeStack;
std::vector<std::string> MochiINI::EngineIncludeCache;

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
    {
        //
        // 没有配置文件时，在默认位置创建一个空文件，
        // 让玩家有一个可以直接编辑的 INI
        //
        try
        {
            std::ofstream file(
                std::filesystem::path(filename),
                std::ios::out |
                std::ios::trunc |
                std::ios::binary);

            return file.good();
        }
        catch (...)
        {
            return false;
        }
    }

    //
    // 一次顶层加载 = 一次 include 去重表
    //
    TextIncludeStack.clear();

    TextIncludeStack.push_back(
        ToLower(foundPath.string()));

    return LoadFileInto(foundPath, paths, 0);
}

bool MochiINI::LoadFileInto(
    const std::filesystem::path& foundPath,
    const std::vector<std::filesystem::path>& searchPaths,
    int depth)
{
    if (depth > 8)
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

    file.close();

    //
    // Ares 风格 [#include]
    //
    const Section* includeSection = GetSection("#include");

    if (includeSection && !includeSection->Keys.empty())
    {
        //
        // 按数字 key 顺序加载
        //
        std::vector<std::pair<int, std::string>> entries;

        int fallbackIndex = 0x7FFFFFFF;

        for (auto& kv : includeSection->Keys)
        {
            int idx = IsNumber(kv.first)
                ? atoi(kv.first.c_str())
                : fallbackIndex++;

            if (!kv.second.Array.empty())
            {
                for (auto& one : kv.second.Array)
                {
                    entries.emplace_back(idx, one);
                }
            }
            else if (!kv.second.Raw.empty())
            {
                entries.emplace_back(idx, kv.second.Raw);
            }
        }

        std::stable_sort(
            entries.begin(),
            entries.end(),
            [](const std::pair<int, std::string>& a,
               const std::pair<int, std::string>& b)
            {
                return a.first < b.first;
            });

        for (auto& entry : entries)
        {
            std::filesystem::path incPath(entry.second);

            if (incPath.is_relative())
                incPath = foundPath.parent_path() / incPath;

            if (!std::filesystem::exists(incPath))
            {
                //
                // 再按搜索路径找一遍
                //
                for (auto& p : searchPaths)
                {
                    auto candidate = p / entry.second;

                    if (std::filesystem::exists(candidate))
                    {
                        incPath = candidate;
                        break;
                    }
                }
            }

            if (!std::filesystem::exists(incPath))
                continue;

            std::string key = ToLower(incPath.string());

            bool already = false;

            for (auto& loaded : TextIncludeStack)
            {
                if (loaded == key)
                {
                    already = true;
                    break;
                }
            }

            if (already)
                continue;

            TextIncludeStack.push_back(key);

            LoadFileInto(incPath, searchPaths, depth + 1);
        }
    }

    return true;
}

//
// 引擎侧 [#include]：由 INIHook 在 CCINIClass::ReadCCFile 返回前调用
//
// A hard depth ceiling backs up the name cache: if the cache is ever defeated
// (different spellings, relative paths, an include that grows its own key list),
// we bail out instead of eating the 1 MB game stack (C00000FD).
void MochiINI::ProcessIncludes(INIClass* pINI)
{
    if (!pINI)
        return;

    static int s_includeDepth = 0;

    if (s_includeDepth >= 16)
    {
        Debug::Log("[MochiINI] include depth limit reached, skipped\n");
        return;
    }

    ++s_includeDepth;

    const char* section = "#include";


    int count = pINI->GetKeyCount(section);

    for (int i = 0; i < count; ++i)
    {
        const char* key = pINI->GetKeyName(section, i);

        if (!key)
            continue;

        char buffer[0x100] = {};

        if (pINI->ReadString(
                section,
                key,
                "",
                buffer,
                sizeof(buffer)) <= 0)
        {
            continue;
        }

        std::string name = Trim(buffer);

        if (name.empty())
            continue;

        std::string lower = ToLower(name);

        bool already = false;

        for (auto& loaded : EngineIncludeCache)
        {
            if (loaded == lower)
            {
                already = true;
                break;
            }
        }

        if (already)
            continue;

        CCFileClass file(name.c_str());

        if (!file.Exists())
        {
            Debug::Log(
                "[MochiINI] include not found: %s\n",
                name.c_str());
            continue;
        }

        EngineIncludeCache.push_back(lower);

        static_cast<CCINIClass*>(pINI)->ReadCCFile(&file);
    }

    --s_includeDepth;
}

void MochiINI::ResetIncludeCache()
{
    EngineIncludeCache.clear();
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

//*********************** INI WRITE ***********************

static std::string EscapeINIValue(const std::string& value)
{
    bool quote = value.empty();

    for (char c : value)
    {
        if (c == ',' || c == ';' || c == '"' ||
            c == '\n' || c == '\r' || c == '\t')
        {
            quote = true;
            break;
        }
    }

    if (!quote)
        return value;

    std::string result = "\"";

    for (char c : value)
    {
        switch (c)
        {
        case '"':
            result += "\\\"";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += c;
            break;
        }
    }

    result += "\"";
    return result;
}


static std::string SerializeINIValue(const MochiINI::Value& value)
{
    return value.Raw;
}


bool MochiINI::SaveINI(const char* filename) const
{
    return WriteINI(filename, true);
}



bool MochiINI::WriteINI(
    const char* filename,
    bool createDirectories) const
{
    if (!filename || !*filename)
        return false;

    try
    {
        std::filesystem::path path(filename);

        if (createDirectories && path.has_parent_path())
        {
            std::filesystem::create_directories(
                path.parent_path());
        }

        std::ofstream file(
            path,
            std::ios::out |
            std::ios::trunc |
            std::ios::binary);

        if (!file.is_open())
            return false;

        for (const auto& sectionPair : Sections)
        {
            file << "["
                << sectionPair.first
                << "]\n";

            const Section& section = sectionPair.second;

            for (const auto& keyPair : section.Keys)
            {
                file << keyPair.first
                    << "="
                    << SerializeINIValue(keyPair.second)
                    << "\n";
            }

            file << "\n";
        }

        file.flush();

        return file.good();
    }
    catch (...)
    {
        return false;
    }
}

//*********************** INI WRITE ***********************