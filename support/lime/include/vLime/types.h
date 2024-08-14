#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using f32 = float;
using f64 = double;

template<typename T>
concept HasSize = requires(T a) { { a.size() } -> std::convertible_to<size_t>; };

template<std::integral T, HasSize Container>
[[nodiscard]] inline constexpr T csize(Container const& container)
{
    assert(container.size() <= std::numeric_limits<T>::max());
    return static_cast<T>(container.size());
}
