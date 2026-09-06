#pragma once

#include <type_traits>

namespace utilwx {

template <typename E>
constexpr auto tul(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

}