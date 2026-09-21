#pragma once

#include <filesystem>
#include <vector>

namespace ts::platform
{
    [[nodiscard]] std::filesystem::path local_appdata();
    [[nodiscard]] std::vector<std::filesystem::path> polaris_stage_dirs();
}
