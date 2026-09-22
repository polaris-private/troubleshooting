#include "err_code.hpp"

#include <array>
#include <format>

namespace ts::core
{
    namespace
    {
        constexpr std::array<char, 7> letters =
        {
            'A', 'L', 'O', 'T', 'P', 'U', 'M'
        };

        constexpr std::array<std::string_view, 7> names =
        {
            "auth", "license", "overlay", "tamper", "protection", "utilities", "main"
        };

        static_assert(letters.size() == names.size(),
            "letters and names must match subsystem count");
        static_assert(letters.size() == static_cast<std::size_t>(subsystem::main) + 1,
            "letters/names must cover every subsystem enumerator");
    }

    subsystem subsys(err_code c) noexcept
    {
        return static_cast<subsystem>((static_cast<std::uint32_t>(c) >> 16) & 0xFFu);
    }

    std::uint32_t ordinal(err_code c) noexcept
    {
        return static_cast<std::uint32_t>(c) & 0xFFFFu;
    }

    char subsys_letter(subsystem s) noexcept
    {
        const auto i = static_cast<std::size_t>(s);
        return i < letters.size() ? letters[i] : '?';
    }

    std::string_view subsys_name(subsystem s) noexcept
    {
        const auto i = static_cast<std::size_t>(s);
        return i < names.size() ? names[i] : std::string_view{ "unknown" };
    }

    std::string format_code(err_code c)
    {
        return std::format("{}{:03}", subsys_letter(subsys(c)), ordinal(c));
    }
}
