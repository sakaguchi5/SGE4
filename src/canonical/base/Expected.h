#pragma once

#include <expected>
#include <type_traits>
#include <utility>

namespace sge4::base
{
template<class T, class E>
using Expected = std::expected<T, E>;

template<class T, class E>
[[nodiscard]] Expected<T, E> Success(T value)
    requires (!std::is_void_v<T>)
{
    return Expected<T, E>(std::in_place, std::move(value));
}

template<class T, class E>
[[nodiscard]] Expected<T, E> Success()
    requires std::is_void_v<T>
{
    return Expected<T, E>{};
}

template<class T, class E>
[[nodiscard]] Expected<T, E> Failure(E error)
{
    return std::unexpected<E>(std::move(error));
}
}
