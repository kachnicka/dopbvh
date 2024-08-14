#pragma once
#include <type_traits>

namespace lime {

template<typename FlagBitsType>
struct FlagTraits {
    enum {
        allFlags = 0
    };
};

template<typename BitType>
class Flags {
public:
    using MaskType = typename std::underlying_type<BitType>::type;

    constexpr Flags() noexcept
        : m_mask(0)
    {
    }

    constexpr Flags(BitType bit) noexcept
        : m_mask(static_cast<MaskType>(bit))
    {
    }

    constexpr Flags(Flags<BitType> const& rhs) noexcept = default;

    constexpr explicit Flags(MaskType flags) noexcept
        : m_mask(flags)
    {
    }

    auto operator<=>(Flags<BitType> const&) const = default;

    constexpr bool operator!() const noexcept
    {
        return !m_mask;
    }

    constexpr Flags<BitType> operator&(Flags<BitType> const& rhs) const noexcept
    {
        return Flags<BitType>(m_mask & rhs.m_mask);
    }

    constexpr Flags<BitType> operator|(Flags<BitType> const& rhs) const noexcept
    {
        return Flags<BitType>(m_mask | rhs.m_mask);
    }

    constexpr Flags<BitType> operator^(Flags<BitType> const& rhs) const noexcept
    {
        return Flags<BitType>(m_mask ^ rhs.m_mask);
    }

    constexpr Flags<BitType> operator~() const noexcept
    {
        return Flags<BitType>(m_mask ^ FlagTraits<BitType>::allFlags);
    }

    constexpr Flags<BitType>& operator=(Flags<BitType> const& rhs) noexcept = default;

    constexpr Flags<BitType>& operator|=(Flags<BitType> const& rhs) noexcept
    {
        m_mask |= rhs.m_mask;
        return *this;
    }

    constexpr Flags<BitType>& operator&=(Flags<BitType> const& rhs) noexcept
    {
        m_mask &= rhs.m_mask;
        return *this;
    }

    constexpr Flags<BitType>& operator^=(Flags<BitType> const& rhs) noexcept
    {
        m_mask ^= rhs.m_mask;
        return *this;
    }

    // cast operators
    explicit constexpr operator bool() const noexcept
    {
        return !!m_mask;
    }

    explicit constexpr operator MaskType() const noexcept
    {
        return m_mask;
    }

    constexpr bool checkFlags(Flags<BitType> flags) const noexcept
    {
        return (flags & *this) == flags;
    }

private:
    MaskType m_mask;
};

template<typename BitType>
constexpr Flags<BitType> operator&(BitType bit, Flags<BitType> const& flags) noexcept
{
    return flags.operator&(bit);
}

template<typename BitType>
constexpr Flags<BitType> operator|(BitType bit, Flags<BitType> const& flags) noexcept
{
    return flags.operator|(bit);
}

template<typename BitType>
constexpr Flags<BitType> operator^(BitType bit, Flags<BitType> const& flags) noexcept
{
    return flags.operator^(bit);
}

#define FLAGS_CLASS_NAME(STRIPPED_FLAG_ENUM_CLASS_NAME) STRIPPED_FLAG_ENUM_CLASS_NAME##s
#define FLAG_BITS_CLASS_NAME(STRIPPED_FLAG_ENUM_CLASS_NAME) STRIPPED_FLAG_ENUM_CLASS_NAME##Bits
#define GENERATE_FLAG_OPERATOR_OVERLOADS(STRIPPED_NAME)                                           \
    inline constexpr FLAGS_CLASS_NAME(STRIPPED_NAME)                                              \
    operator|(FLAG_BITS_CLASS_NAME(STRIPPED_NAME) bit0, FLAG_BITS_CLASS_NAME(STRIPPED_NAME) bit1) \
    {                                                                                             \
        return FLAGS_CLASS_NAME(STRIPPED_NAME)(bit0) | bit1;                                      \
    }                                                                                             \
    inline constexpr FLAGS_CLASS_NAME(STRIPPED_NAME)                                              \
    operator&(FLAG_BITS_CLASS_NAME(STRIPPED_NAME) bit0, FLAG_BITS_CLASS_NAME(STRIPPED_NAME) bit1) \
    {                                                                                             \
        return FLAGS_CLASS_NAME(STRIPPED_NAME)(bit0) & bit1;                                      \
    }                                                                                             \
    inline constexpr FLAGS_CLASS_NAME(STRIPPED_NAME)                                              \
    operator^(FLAG_BITS_CLASS_NAME(STRIPPED_NAME) bit0, FLAG_BITS_CLASS_NAME(STRIPPED_NAME) bit1) \
    {                                                                                             \
        return FLAGS_CLASS_NAME(STRIPPED_NAME)(bit0) ^ bit1;                                      \
    }                                                                                             \
    inline constexpr FLAGS_CLASS_NAME(STRIPPED_NAME)                                              \
    operator~(FLAG_BITS_CLASS_NAME(STRIPPED_NAME) bits)                                           \
    {                                                                                             \
        return ~(FLAGS_CLASS_NAME(STRIPPED_NAME)(bits));                                          \
    }

}