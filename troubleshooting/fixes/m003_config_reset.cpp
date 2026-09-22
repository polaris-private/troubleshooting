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

            const auto cfg = dir / L"config.json";
            ctx.log(std::format("check {}", cfg.string()));

            if (platform::has_reparse_point(cfg))
            {
                ++errors;
                ctx.log("  refused: reparse point (symlink or junction)");
                result.record("refused (reparse point): " + cfg.string(), false);
                continue;
            }

            std::error_code ec;
            const bool present = std::filesystem::exists(cfg, ec);
            if (ec)
            {
                ++errors;
                ctx.log(std::format("  exists check failed: {}", ec.message()));
                result.record("check failed: " + cfg.string(), false);
                continue;
            }
            if (!present)
            {
                ++not_present;
                result.record("not present: " + cfg.string(), true);
                continue;
            }

            std::filesystem::remove(cfg, ec);
            if (ec)
            {
                ++errors;
                ctx.log(std::format("  remove failed: {}", ec.message()));
                result.record("failed: " + cfg.string(), false);
            }
            else
            {
                ++deleted;
                ctx.log("  deleted");
                result.record("deleted: " + cfg.string(), true);
            }
        }

        result.ok      = errors == 0;
        result.message = std::format(
            "{} deleted, {} not present, {} errors - relaunch loader to regenerate defaults",
            deleted, not_present, errors);
        return result;
    }
}
