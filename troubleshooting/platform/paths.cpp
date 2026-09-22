#include "paths.hpp"

#include <format>
#include <system_error>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ts::platform
{
    std::string path_to_utf8(const std::filesystem::path & p)
    {
        const std::u8string u8 = p.u8string();
        return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
    }

    std::string ec_message_utf8(const std::error_code & ec)
    {
        const int v = ec.value();
        if (v == 0) return {};

        wchar_t * buf = nullptr;
        const DWORD n = ::FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER
                | FORMAT_MESSAGE_FROM_SYSTEM
                | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            static_cast<DWORD>(v),
            0,
            reinterpret_cast<wchar_t*>(&buf),
            0,
            nullptr);
        if (n == 0 || buf == nullptr)
            return std::format("error {}", v);

        const int narrow_len = ::WideCharToMultiByte(
            CP_UTF8, 0, buf, static_cast<int>(n), nullptr, 0, nullptr, nullptr);
        std::string out;
        if (narrow_len > 0)
        {
            out.resize(static_cast<std::size_t>(narrow_len));
            ::WideCharToMultiByte(
                CP_UTF8, 0, buf, static_cast<int>(n),
                out.data(), narrow_len, nullptr, nullptr);
        }
        ::LocalFree(buf);

        while (!out.empty() &&
            (out.back() == '\n' || out.back() == '\r'
                || out.back() == '.' || out.back() == ' '))
        {
            out.pop_back();
        }
        return out;
    }

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
