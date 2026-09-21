#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "err_code.hpp"
#include "fix_context.hpp"
#include "fix_result.hpp"

namespace ts::core
{
    using fix_fn = std::function<fix_result(fix_context &)>;

    struct code_entry
    {
        err_code                 code;
        std::string_view         symbol;
        std::string_view         summary;
        std::vector<std::string> manual_steps;
        fix_fn                   fixer;
        subsystem                sub;

        [[nodiscard]] bool has_auto_fix() const noexcept { return static_cast<bool>(fixer); }
    };

    [[nodiscard]] std::span<const code_entry> all_entries();
    [[nodiscard]] const code_entry * find(err_code c) noexcept;
    [[nodiscard]] const code_entry * find_by_display(std::string_view display) noexcept;
}
