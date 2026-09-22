#include "processes.hpp"

#include <cctype>
#include <cwchar>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

namespace ts::platform
{
    namespace
    {
        [[nodiscard]] std::string narrow_from_wide(const wchar_t* w)
        {
            if (w == nullptr) return {};
            const int len = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
            if (len <= 1) return {};
            std::string out(static_cast<std::size_t>(len - 1), '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), len, nullptr, nullptr);
            return out;
        }
    }

    std::vector<process_entry> enumerate_processes()
    {
        std::vector<process_entry> out;

        const HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return out;

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (!::Process32FirstW(snap, &pe))
        {
            ::CloseHandle(snap);
            return out;
        }

        do
        {
            out.push_back(process_entry{
                static_cast<std::uint32_t>(pe.th32ProcessID),
                narrow_from_wide(pe.szExeFile),
            });
        }
        while (::Process32NextW(snap, &pe));

        ::CloseHandle(snap);
        return out;
    }

    bool kill_process(std::uint32_t pid, std::string_view expected_name_lower) noexcept
    {
        if (pid == 0) return false;

        const HANDLE h = ::OpenProcess(
            PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (h == nullptr) return false;

        wchar_t path_buf[MAX_PATH]{};
        DWORD   path_size = MAX_PATH;
        if (!::QueryFullProcessImageNameW(h, 0, path_buf, &path_size))
        {
            ::CloseHandle(h);
            return false;
        }

        const wchar_t * sep    = std::wcsrchr(path_buf, L'\\');
        const wchar_t * name_w = sep ? sep + 1 : path_buf;

        const std::string current_narrow = narrow_from_wide(name_w);
        std::string current_lower;
        current_lower.reserve(current_narrow.size());
        for (char c : current_narrow)
        {
            current_lower.push_back(
                static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }

        if (current_lower != expected_name_lower)
        {
            ::CloseHandle(h);
            return false;
        }

        const BOOL ok = ::TerminateProcess(h, 1);
        ::CloseHandle(h);
        return ok != FALSE;
    }
}
