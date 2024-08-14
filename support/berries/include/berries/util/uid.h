#pragma once

#include <cstdint>
#include <limits>
#include <concepts>

class GenericID;

template<typename T = GenericID, typename UnderlyingType = uint32_t>
requires std::unsigned_integral<UnderlyingType>
class uid {
    UnderlyingType id;

public:
    inline static UnderlyingType constexpr INVALID_ID = std::numeric_limits<UnderlyingType>::max();

    uid()
        : id(INVALID_ID)
    {
    }

    explicit uid(UnderlyingType id)
        : id(id)
    {
    }

    template<typename ConvertibleType>
    explicit uid(ConvertibleType id)
        : id(static_cast<UnderlyingType>(id))
    {
    }

    uid(uid&& rhs) = default;
    uid& operator=(uid&& rhs) noexcept = default;
    uid(uid const& rhs) = default;
    uid& operator=(uid const& rhs) = default;

    auto operator<=>(uid<T, UnderlyingType> const&) const = default;

    [[nodiscard]] UnderlyingType get() const
    {
        return id;
    }

    explicit operator UnderlyingType() const
    {
        return id;
    }

    [[nodiscard]] bool isValid() const
    {
        return id != INVALID_ID;
    }
// private:
//     friend struct std::hash<uid<T, UnderlyingType>>;
};

// namespace std {
// template<typename T, typename UnderlyingType>
// struct hash<::uid<T, UnderlyingType>> {
//     std::size_t operator()(const ::uid<T, UnderlyingType>& uid) const noexcept
//     {
//         return std::hash<UnderlyingType> {}(uid.id);
//     }
// };
// }
