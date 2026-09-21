#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ts::core
{
    enum class subsystem : std::uint8_t
    {
        auth       = 0,
        license    = 1,
        overlay    = 2,
        tamper     = 3,
        protection = 4,
        utilities  = 5,
        main       = 6,
    };

    enum class err_code : std::uint32_t
    {
        a_no_credentials         = 0x00000001,
        a_credentials_read       = 0x00000002,
        a_authenticate_failed    = 0x00000003,

        l_hwid_generate_failed   = 0x00010001,
        l_license_expired        = 0x00010002,
        l_license_invalid        = 0x00010003,
        l_license_hwid_mismatch  = 0x00010004,
        l_license_lookup_failed  = 0x00010005,
        l_no_product_license     = 0x00010006,

        o_target_not_found       = 0x00020001,
        o_d3d_device_failed      = 0x00020002,
        o_swapchain_failed       = 0x00020003,
        o_window_create_failed   = 0x00020004,
        o_imgui_init_failed      = 0x00020005,
        o_render_loop_exit       = 0x00020006,
        o_target_window_missing  = 0x00020007,
        o_initialize_failed      = 0x00020008,

        t_peb_being_debugged     = 0x00030001,
        t_nt_global_flag         = 0x00030002,
        t_hw_breakpoint          = 0x00030003,
        t_debug_port             = 0x00030004,
        t_debug_object_handle    = 0x00030005,

        p_delayload_reject       = 0x00040001,
        p_texture_decode         = 0x00040002,
        p_texture_d3d_create     = 0x00040003,
        p_tamper_mismatch        = 0x00040004,

        u_unicorn_open           = 0x00050001,
        u_unicorn_mem_map        = 0x00050002,
        u_unicorn_emu_abort      = 0x00050003,
        u_zydis_decoder_init     = 0x00050004,
        u_zydis_decode           = 0x00050005,

        m_dll_search_setup       = 0x00060001,
        m_sha256_failed          = 0x00060002,
        m_config_read            = 0x00060003,
        m_wsa_init               = 0x00060004,
        m_driver_open_failed     = 0x00060005,
        m_sign_in_failed         = 0x00060006,

        m_drv_download_failed    = 0x00060008,
        m_drv_binary_invalid     = 0x00060009,
        m_drv_provider_failed    = 0x0006000A,
        m_drv_map_failed         = 0x0006000B,
        m_drv_device_timeout     = 0x0006000C,
        m_drv_internal_error     = 0x0006000D,
        m_drv_session_invalid    = 0x0006000E,
        m_update_download_failed = 0x0006000F,
        m_update_swap_failed     = 0x00060010,
        m_update_signature_failed = 0x00060011,

        m_dll_no_product         = 0x00060012,
        m_dll_download_failed    = 0x00060013,
        m_dll_binary_invalid     = 0x00060014,
        m_dll_map_failed         = 0x00060015,
    };

    [[nodiscard]] subsystem subsys(err_code c) noexcept;
    [[nodiscard]] std::uint32_t ordinal(err_code c) noexcept;
    [[nodiscard]] char subsys_letter(subsystem s) noexcept;
    [[nodiscard]] std::string_view subsys_name(subsystem s) noexcept;
    [[nodiscard]] std::string format_code(err_code c);
}
