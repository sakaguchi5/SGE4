#pragma once

#include <array>
#include <cstddef>
#include <functional>

namespace sge4::base
{
template<class T, std::size_t N, class Projection>
consteval bool ValuesAreUnique(const std::array<T, N>& values, Projection projection)
{
    for (std::size_t left = 0; left < N; ++left)
        for (std::size_t right = left + 1; right < N; ++right)
            if (std::invoke(projection, values[left]) == std::invoke(projection, values[right]))
                return false;
    return true;
}

template<class T, std::size_t N, class Projection>
consteval bool ValuesAreStrictlyIncreasing(const std::array<T, N>& values, Projection projection)
{
    for (std::size_t index = 1; index < N; ++index)
        if (!(std::invoke(projection, values[index - 1]) < std::invoke(projection, values[index])))
            return false;
    return true;
}

template<class T, std::size_t N, class Predicate>
consteval bool AllValuesSatisfy(const std::array<T, N>& values, Predicate predicate)
{
    for (const auto& value : values)
        if (!std::invoke(predicate, value))
            return false;
    return true;
}
}
