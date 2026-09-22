#pragma once

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace ts::platform
{
    [[nodiscard]] std::filesystem::path local_appdata();
    [[nodiscard]] std::vector<std::filesystem::path> polaris_stage_dirs();
    [[nodiscard]] bool has_reparse_point(const std::filesystem::path & p) noexcept;
    [[nodiscard]] std::string path_to_utf8(const std::filesystem::path & p);
    [[nodiscard]] std::string ec_message_utf8(const std::error_code & ec);
}
