#pragma once

#define XXH_INLINE_ALL
#include "xxhash.h"

#include <string>

struct Hasher
{
public:
    Hasher(uint64_t seed)
    {
        m_state = XXH64_createState();
        XXH64_reset(m_state, seed);
    }

    ~Hasher()
    {
        XXH64_freeState(m_state);
        m_state = nullptr;
    }

    uint64_t Result()
    {
        return XXH64_digest(m_state);
    }

    template <typename T>
    void Add(const T& value)
    {
        XXH64_update(m_state, &value, sizeof(value));
    }

    void Add(const char* s)
    {
        XXH64_update(m_state, s, strlen(s));
    }

    void Add(const std::string& s)
    {
        Add(s.c_str());
    }

private:
    XXH64_state_t* m_state = nullptr;
};
