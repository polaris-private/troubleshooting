#include "paths.hpp"

#include <system_error>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ts::platform
{
    bool has_reparse_point(const std::filesystem::path & p) noexcept
    {
        const DWORD attrs = ::GetFileAttributesW(p.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) return false;
        return (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    }

    std::filesystem::path local_appdata()
    {
        wchar_t buf[MAX_PATH]{};
        const DWORD n = ::GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) return {};
        return std::filesystem::path{ buf };
    }

    std::vector<std::filesystem::path> polaris_stage_dirs()
    {
        std::vector<std::filesystem::path> out;

        const auto la = local_appdata();
        if (la.empty()) return out;

        std::error_code ec;
        if (!std::filesystem::exists(la, ec) || ec) return out;

        for (auto it = std::filesystem::directory_iterator(la, ec);
             !ec && it != std::filesystem::directory_iterator{};
             it.increment(ec))
        {
            const auto & entry = *it;

            const auto name = entry.path().filename().wstring();
            if (!name.starts_with(L"Polaris-")) continue;

            if (has_reparse_point(entry.path())) continue;

            std::error_code ec2;
            if (!entry.is_directory(ec2) || ec2) continue;

            out.push_back(entry.path());
        }

        return out;
    }
}
