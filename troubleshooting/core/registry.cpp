#include "registry.hpp"

#include <algorithm>

#include "../fixes/m003_config_reset.hpp"
#include "../fixes/m009_drv_cache.hpp"
#include "../fixes/m014_ratchet.hpp"
#include "../fixes/m016_update_pending.hpp"
#include "../fixes/m020_dll_cache.hpp"

namespace ts::core
{
    namespace
    {
        std::vector<code_entry> build_entries()
        {
            std::vector<code_entry> v;
            v.reserve(16);

            v.push_back({ err_code::m_config_read, "m_config_read",
                "config read failed",
                { "delete %LOCALAPPDATA%\\Polaris-<hash>\\config.json",
                  "relaunch the loader; defaults get regenerated on start" },
                &fixes::m003::run, subsystem::main });

            v.push_back({ err_code::m_drv_binary_invalid, "m_drv_binary_invalid",
                "driver binary invalid or corrupt",
                { "delete %LOCALAPPDATA%\\Polaris-<hash>\\drv.bin",
                  "retry inject; loader will re-download a fresh driver" },
                &fixes::m009::run, subsystem::main });

            v.push_back({ err_code::m_drv_session_invalid, "m_drv_session_invalid",
                "injection session not authenticated",
                { "delete %LOCALAPPDATA%\\Polaris-<hex12(hwid)>\\ratchet.dat",
                  "sign out and sign back in from the loader" },
                &fixes::m014::run, subsystem::main });

            v.push_back({ err_code::m_dll_binary_invalid, "m_dll_binary_invalid",
                "module binary invalid or corrupt",
                { "delete %LOCALAPPDATA%\\Polaris-<hash>\\dll-<slug>.bin",
                  "retry inject; loader will re-download a fresh module" },
                &fixes::m020::run, subsystem::main });

            v.push_back({ err_code::m_update_swap_failed, "m_update_swap_failed",
                "update binary swap failed",
                { "close any polaris loader instance",
                  "delete %LOCALAPPDATA%\\Polaris-<hash>\\update-pending.*",
                  "relaunch the loader" },
                &fixes::m016::run, subsystem::main });

            v.push_back({ err_code::m_update_signature_failed, "m_update_signature_failed",
                "update signature verification failed",
                { "delete %LOCALAPPDATA%\\Polaris-<hash>\\update-pending.*",
                  "relaunch the loader to force a fresh download" },
                &fixes::m016::run, subsystem::main });

            return v;
        }
    }

    std::span<const code_entry> all_entries()
    {
        static const std::vector<code_entry> entries = build_entries();
        return std::span<const code_entry>{ entries.data(), entries.size() };
    }

    const code_entry * find(err_code c) noexcept
    {
        const auto es = all_entries();
        const auto it = std::ranges::find_if(es,
            [c](const code_entry & e) { return e.code == c; });
        return it != es.end() ? &*it : nullptr;
    }

    const code_entry * find_by_display(std::string_view display) noexcept
    {
        const auto es = all_entries();
        for (const auto & e : es)
        {
            if (format_code(e.code) == display) return &e;
        }
        return nullptr;
    }
}
