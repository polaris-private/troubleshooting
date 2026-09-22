#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ts::platform
{
    struct process_entry
    {
        std::uint32_t pid;
        std::string   exe_name;
    };

    [[nodiscard]] std::vector<process_entry> enumerate_processes();
    [[nodiscard]] bool kill_process(std::uint32_t pid,
                                    std::string_view expected_name_lower) noexcept;
    [[nodiscard]] bool polaris_loader_running();
}
