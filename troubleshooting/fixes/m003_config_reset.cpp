#include "m003_config_reset.hpp"

#include <filesystem>
#include <format>
#include <system_error>

#include "../platform/paths.hpp"

namespace ts::fixes::m003
{
    core::fix_result run(core::fix_context & ctx)
    {
        const auto dirs = platform::polaris_stage_dirs();
        if (dirs.empty())
        {
            return core::fix_result::failure(
                "no Polaris-<hash> directory found under %LOCALAPPDATA%");
        }

        core::fix_result result = core::fix_result::success("");

        int deleted     = 0;
        int not_present = 0;
        int errors      = 0;

        for (const auto & dir : dirs)
        {
            if (ctx.cancelled()) break;

            const auto cfg     = dir / L"config.json";
            const auto cfg_str = platform::path_to_utf8(cfg);
            ctx.log(std::format("check {}", cfg_str));

            if (platform::has_reparse_point(cfg))
            {
                ++errors;
                ctx.log("  refused: reparse point (symlink or junction)");
                result.record("refused (reparse point): " + cfg_str, false);
                continue;
            }

            std::error_code ec;
            const bool present = std::filesystem::exists(cfg, ec);
            if (ec)
            {
                ++errors;
                ctx.log(std::format("  exists check failed: {}",
                    platform::ec_message_utf8(ec)));
                result.record("check failed: " + cfg_str, false);
                continue;
            }
            if (!present)
            {
                ++not_present;
                result.record("not present: " + cfg_str, true);
                continue;
            }

            std::filesystem::remove(cfg, ec);
            if (ec)
            {
                ++errors;
                ctx.log(std::format("  remove failed: {}",
                    platform::ec_message_utf8(ec)));
                result.record("failed: " + cfg_str, false);
            }
            else
            {
                ++deleted;
                ctx.log("  deleted");
                result.record("deleted: " + cfg_str, true);
            }
        }

        result.ok      = errors == 0;
        result.message = std::format(
            "{} deleted, {} not present, {} errors - relaunch loader to regenerate defaults",
            deleted, not_present, errors);
        return result;
    }
}
