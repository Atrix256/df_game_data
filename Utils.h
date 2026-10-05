#pragma once

#include <functional>

#define AT_EXIT(func) AtExit atExit##__LINE__([&](){ func; })

struct AtExit
{
    AtExit(std::function<void()> func) : m_func(func) {}
    ~AtExit() { m_func(); }
    std::function<void()> m_func;
};