#pragma once
//
// template specializations
//

template<>
inline int MochiINI::Section::GetValueByKey<int>(
    const std::string& key,
    const int& defaultValue) const
{
    auto v = Get(key);

    if (!v)
        return defaultValue;

    return v->AsInt(defaultValue);
}

template<>
inline float MochiINI::Section::GetValueByKey<float>(
    const std::string& key,
    const float& defaultValue) const
{
    auto v = Get(key);

    if (!v)
        return defaultValue;

    return v->AsFloat(defaultValue);
}

template<>
inline bool MochiINI::Section::GetValueByKey<bool>(
    const std::string& key,
    const bool& defaultValue) const
{
    auto v = Get(key);

    if (!v)
        return defaultValue;

    return v->AsBool(defaultValue);
}

template<>
std::string
inline MochiINI::Section::GetValueByKey<std::string>(
    const std::string& key,
    const std::string& defaultValue) const
{
    auto v = Get(key);

    if (!v)
        return defaultValue;

    return v->Raw;
}

