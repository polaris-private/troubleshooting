#pragma once

#include <filesystem>
#include <vector>

namespace ts::platform
{
    [[nodiscard]] std::filesystem::path local_appdata();
    [[nodiscard]] std::vector<std::filesystem::path> polaris_stage_dirs();
    [[nodiscard]] bool has_reparse_point(const std::filesystem::path & p) noexcept;
}
