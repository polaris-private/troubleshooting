#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ts::platform
{
    struct process_entry
    {
        std::uint32_t pid;
        std::string   exe_name;
    };

    [[nodiscard]] std::vector<process_entry> enumerate_processes();
    [[nodiscard]] bool kill_process(std::uint32_t pid) noexcept;
}
