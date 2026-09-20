#pragma once

#include <nlohmann/json.hpp>
using json = nlohmann::ordered_json;
using json_pointer = json::json_pointer;

template <typename T>
T GetOrDefault(const json& json, const json_pointer& path, T& defaultValue)
{
    if (!json.contains(path))
        return defaultValue;

    const auto& v = json.at(path);
    if (v.is_null())
        return defaultValue;

    return json.value<T>(path, defaultValue);
}

template <typename T>
inline T GetValueFromString(const char* valueStr);

template <>
inline bool GetValueFromString<bool>(const char* valueStr)
{
    return (!_stricmp(valueStr, "true") || !_stricmp(valueStr, "1"));
}

template <>
inline int64_t GetValueFromString<int64_t>(const char* valueStr)
{
    int64_t value = 0;
    sscanf_s(valueStr, "%lld", &value);
    return value;
}

template <>
inline uint64_t GetValueFromString<uint64_t>(const char* valueStr)
{
    uint64_t value = 0;
    sscanf_s(valueStr, "%llu", &value);
    return value;
}

template <>
inline double GetValueFromString<double>(const char* valueStr)
{
    double value = 0.0;
    sscanf_s(valueStr, "%lf", &value);
    return value;
}

template <>
inline float GetValueFromString<float>(const char* valueStr)
{
    float value = 0.0f;
    sscanf_s(valueStr, "%f", &value);
    return value;
}
