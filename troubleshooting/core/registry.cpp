#include "registry.hpp"

#include <algorithm>

#include "../fixes/m014_ratchet.hpp"

namespace ts::core
{
    namespace
    {
        std::vector<code_entry> build_entries()
        {
            std::vector<code_entry> v;
            v.reserve(48);

            v.push_back({ err_code::a_no_credentials, "a_no_credentials",
                "no polaris credentials found for this machine",
                { "sign in on the loader launch screen with your account",
                  "if sign-in loops back, check that your account has an active license" },
                {}, subsystem::auth });

            v.push_back({ err_code::a_credentials_read, "a_credentials_read",
                "credentials read failed",
                { "close the loader",
                  "sign in again from the loader; credentials get rewritten cleanly" },
                {}, subsystem::auth });

            v.push_back({ err_code::a_authenticate_failed, "a_authenticate_failed",
                "authenticate call failed",
                { "check your internet connection",
                  "confirm the polaris api endpoint is reachable",
                  "retry sign-in after 30 seconds" },
                {}, subsystem::auth });

            v.push_back({ err_code::l_hwid_generate_failed, "l_hwid_generate_failed",
                "hwid generation failed",
                { "reboot the machine",
                  "if it persists, open a support ticket - the wmi provider is misbehaving" },
                {}, subsystem::license });

            v.push_back({ err_code::l_license_expired, "l_license_expired",
                "license expired",
                { "renew your subscription from the polaris panel",
                  "restart the loader after renewal" },
                {}, subsystem::license });

            v.push_back({ err_code::l_license_invalid, "l_license_invalid",
                "license invalid signature",
                { "cached license state is corrupt",
                  "sign out and sign back in from the loader to force a fresh license fetch" },
                {}, subsystem::license });

            v.push_back({ err_code::l_license_hwid_mismatch, "l_license_hwid_mismatch",
                "license bound to different hwid",
                { "reset your hwid from the polaris panel (limited resets per month)",
                  "restart the loader" },
                {}, subsystem::license });

            v.push_back({ err_code::l_license_lookup_failed, "l_license_lookup_failed",
                "polaris license lookup failed",
                { "check your internet connection",
                  "retry sign-in after 30 seconds" },
                {}, subsystem::license });

            v.push_back({ err_code::l_no_product_license, "l_no_product_license",
                "account not entitled to this product",
                { "purchase the product from the polaris panel",
                  "confirm your account matches the one you signed in with" },
                {}, subsystem::license });

            v.push_back({ err_code::o_target_not_found, "o_target_not_found",
                "target process not found",
                { "start the game first, then press inject" },
                {}, subsystem::overlay });

            v.push_back({ err_code::o_d3d_device_failed, "o_d3d_device_failed",
                "d3d11 device create failed",
                { "update gpu drivers",
                  "close other d3d11 overlays (discord overlay, msi afterburner, etc.)" },
                {}, subsystem::overlay });

            v.push_back({ err_code::o_swapchain_failed, "o_swapchain_failed",
                "swapchain create failed",
                { "run the loader on the primary monitor",
                  "restart the display driver (win+ctrl+shift+b)" },
                {}, subsystem::overlay });

            v.push_back({ err_code::o_window_create_failed, "o_window_create_failed",
                "overlay window create failed",
                { "close conflicting overlays",
                  "reboot the machine" },
                {}, subsystem::overlay });

            v.push_back({ err_code::o_imgui_init_failed, "o_imgui_init_failed",
                "imgui init failed",
                { "reinstall the loader" },
                {}, subsystem::overlay });

            v.push_back({ err_code::o_render_loop_exit, "o_render_loop_exit",
                "render loop exit unexpected",
                { "check windows event viewer for gpu driver crashes",
                  "restart the loader" },
                {}, subsystem::overlay });

            v.push_back({ err_code::o_target_window_missing, "o_target_window_missing",
                "failed to find target window",
                { "make sure the game is in the foreground",
                  "avoid launching the loader before the game window appears" },
                {}, subsystem::overlay });

            v.push_back({ err_code::o_initialize_failed, "o_initialize_failed",
                "failed to initialize overlay",
                { "restart the loader",
                  "if it persists, open a support ticket with the loader log" },
                {}, subsystem::overlay });

            v.push_back({ err_code::t_peb_being_debugged, "t_peb_being_debugged",
                "debugger detected (peb.beingdebugged)",
                { "close any debugger (ida, x64dbg, windbg, cheat engine)",
                  "relaunch the loader" },
                {}, subsystem::tamper });

            v.push_back({ err_code::t_nt_global_flag, "t_nt_global_flag",
                "debugger detected (nt global flag)",
                { "close any tool that opens the game with debug heap flags",
                  "relaunch the loader" },
                {}, subsystem::tamper });

            v.push_back({ err_code::t_hw_breakpoint, "t_hw_breakpoint",
                "hardware breakpoint present",
                { "close x64dbg or any tool that sets dr0-dr3",
                  "relaunch the loader" },
                {}, subsystem::tamper });

            v.push_back({ err_code::t_debug_port, "t_debug_port",
                "debug port set",
                { "detach any debugger from the process",
                  "relaunch the loader" },
                {}, subsystem::tamper });

            v.push_back({ err_code::t_debug_object_handle, "t_debug_object_handle",
                "debug object handle set",
                { "close attach-based debuggers",
                  "relaunch the loader" },
                {}, subsystem::tamper });

            v.push_back({ err_code::p_delayload_reject, "p_delayload_reject",
                "delayload from non-system32 rejected",
                { "check the loader folder for stray .dll files that shadow system32",
                  "delete anything unfamiliar next to the loader and retry" },
                {}, subsystem::protection });

            v.push_back({ err_code::p_texture_decode, "p_texture_decode",
                "texture decode failed",
                { "reinstall the loader (asset bundle is corrupt)" },
                {}, subsystem::protection });

            v.push_back({ err_code::p_texture_d3d_create, "p_texture_d3d_create",
                "texture d3d create failed",
                { "update gpu drivers",
                  "reboot and retry" },
                {}, subsystem::protection });

            v.push_back({ err_code::p_tamper_mismatch, "p_tamper_mismatch",
                "tamper check mismatch",
                { "the loader binary has been modified (av quarantine, manual edit, cleaner)",
                  "delete the loader and re-download the latest from the panel" },
                {}, subsystem::protection });

            v.push_back({ err_code::u_unicorn_open, "u_unicorn_open",
                "unicorn engine open failed",
                { "reinstall the loader",
                  "if it persists, open a support ticket" },
                {}, subsystem::utilities });

            v.push_back({ err_code::u_unicorn_mem_map, "u_unicorn_mem_map",
                "unicorn memory map failed",
                { "close memory-heavy processes and retry" },
                {}, subsystem::utilities });

            v.push_back({ err_code::u_unicorn_emu_abort, "u_unicorn_emu_abort",
                "unicorn emulation aborted",
                { "open a support ticket with the loader log" },
                {}, subsystem::utilities });

            v.push_back({ err_code::u_zydis_decoder_init, "u_zydis_decoder_init",
                "zydis decoder init failed",
                { "reinstall the loader" },
                {}, subsystem::utilities });

            v.push_back({ err_code::u_zydis_decode, "u_zydis_decode",
                "zydis decode failed",
                { "open a support ticket with the loader log" },
                {}, subsystem::utilities });

            v.push_back({ err_code::m_dll_search_setup, "m_dll_search_setup",
                "dll search dirs setup failed",
                { "check that the loader lives in a writable folder",
                  "move the loader out of program files and retry" },
                {}, subsystem::main });

            v.push_back({ err_code::m_sha256_failed, "m_sha256_failed",
                "sha256 hash failed",
                { "reinstall the loader",
                  "if it persists, bcrypt.dll may be corrupt - run 'sfc /scannow' as admin" },
                {}, subsystem::main });

            v.push_back({ err_code::m_config_read, "m_config_read",
                "config read failed",
                { "delete %LOCALAPPDATA%\\Polaris-<hash>\\config.json and relaunch" },
                {}, subsystem::main });

            v.push_back({ err_code::m_wsa_init, "m_wsa_init",
                "wsa init failed",
                { "winsock stack is broken",
                  "open cmd as admin and run 'netsh winsock reset', then reboot" },
                {}, subsystem::main });

            v.push_back({ err_code::m_driver_open_failed, "m_driver_open_failed",
                "polaris driver could not be opened",
                { "the driver did not map or was unmapped",
                  "restart the loader; if it persists reboot the machine" },
                {}, subsystem::main });

            v.push_back({ err_code::m_sign_in_failed, "m_sign_in_failed",
                "polaris sign-in failed (from the injected module)",
                { "the injected module could not re-authenticate",
                  "check your internet connection and retry inject",
                  "if it persists, sign out from the loader and sign back in" },
                {}, subsystem::main });

            v.push_back({ err_code::m_drv_download_failed, "m_drv_download_failed",
                "driver binary download failed",
                { "check your internet connection",
                  "retry inject after 30 seconds" },
                {}, subsystem::main });

            v.push_back({ err_code::m_drv_binary_invalid, "m_drv_binary_invalid",
                "driver binary invalid or corrupt",
                { "delete %LOCALAPPDATA%\\Polaris-<hash>\\drv.bin and retry inject" },
                {}, subsystem::main });

            v.push_back({ err_code::m_drv_provider_failed, "m_drv_provider_failed",
                "driver service install failed",
                { "another mapper service is present or a prior run left one",
                  "open an elevated cmd and run 'sc query type= driver' to inspect",
                  "reboot and retry" },
                {}, subsystem::main });

            v.push_back({ err_code::m_drv_map_failed, "m_drv_map_failed",
                "driver map returned zero",
                { "kernel-side mapping failed",
                  "reboot the machine and retry inject",
                  "if it persists after reboot open a support ticket" },
                {}, subsystem::main });

            v.push_back({ err_code::m_drv_device_timeout, "m_drv_device_timeout",
                "driver device did not appear after map",
                { "wait a few seconds and retry inject",
                  "if it persists, reboot" },
                {}, subsystem::main });

            v.push_back({ err_code::m_drv_internal_error, "m_drv_internal_error",
                "driver injection internal error",
                { "open a support ticket with the loader log around the failure" },
                {}, subsystem::main });

            v.push_back({ err_code::m_drv_session_invalid, "m_drv_session_invalid",
                "injection session not authenticated",
                { "delete %LOCALAPPDATA%\\Polaris-<hex12(hwid)>\\ratchet.dat",
                  "sign out and sign back in from the loader" },
                &fixes::m014::run, subsystem::main });

            v.push_back({ err_code::m_update_download_failed, "m_update_download_failed",
                "update binary download failed",
                { "check your internet connection",
                  "wait 30 seconds and retry" },
                {}, subsystem::main });

            v.push_back({ err_code::m_update_swap_failed, "m_update_swap_failed",
                "update binary swap failed",
                { "close any polaris loader instance",
                  "delete %LOCALAPPDATA%\\Polaris-<hash>\\update-pending.* and retry" },
                {}, subsystem::main });

            v.push_back({ err_code::m_update_signature_failed, "m_update_signature_failed",
                "update signature verification failed",
                { "the downloaded update is corrupt or tampered",
                  "delete %LOCALAPPDATA%\\Polaris-<hash>\\update-pending.* and retry" },
                {}, subsystem::main });

            v.push_back({ err_code::m_dll_no_product, "m_dll_no_product",
                "no product selected for module download",
                { "pick a product on the loader main screen before pressing inject" },
                {}, subsystem::main });

            v.push_back({ err_code::m_dll_download_failed, "m_dll_download_failed",
                "module binary download failed",
                { "check your internet connection",
                  "retry inject after 30 seconds" },
                {}, subsystem::main });

            v.push_back({ err_code::m_dll_binary_invalid, "m_dll_binary_invalid",
                "module binary invalid or corrupt",
                { "delete %LOCALAPPDATA%\\Polaris-<hash>\\dll-<slug>.bin and retry inject" },
                {}, subsystem::main });

            v.push_back({ err_code::m_dll_map_failed, "m_dll_map_failed",
                "module manual map failed",
                { "check the loader log for the specific dllmap failure line (SEH / import / etc.)",
                  "restart the loader and retry",
                  "if it persists open a support ticket with the log" },
                {}, subsystem::main });

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
