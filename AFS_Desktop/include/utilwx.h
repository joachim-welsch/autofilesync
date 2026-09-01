#pragma once

#include <type_traits>

namespace util {

template <typename E>
constexpr auto tul(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

}