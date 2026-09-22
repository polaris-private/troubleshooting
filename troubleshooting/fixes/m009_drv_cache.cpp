#include "m009_drv_cache.hpp"

#include <filesystem>
#include <format>
#include <system_error>

#include "../platform/paths.hpp"

namespace ts::fixes::m009
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

            const auto blob = dir / L"drv.bin";
            ctx.log(std::format("check {}", blob.string()));

            if (platform::has_reparse_point(blob))
            {
                ++errors;
                ctx.log("  refused: reparse point (symlink or junction)");
                result.record("refused (reparse point): " + blob.string(), false);
                continue;
            }

            std::error_code ec;
            const bool present = std::filesystem::exists(blob, ec);
            if (ec)
            {
                ++errors;
                ctx.log(std::format("  exists check failed: {}", ec.message()));
                result.record("check failed: " + blob.string(), false);
                continue;
            }
            if (!present)
            {
                ++not_present;
                result.record("not present: " + blob.string(), true);
                continue;
            }

            std::filesystem::remove(blob, ec);
            if (ec)
            {
                ++errors;
                ctx.log(std::format("  remove failed: {}", ec.message()));
                result.record("failed: " + blob.string(), false);
            }
            else
            {
                ++deleted;
                ctx.log("  deleted");
                result.record("deleted: " + blob.string(), true);
            }
        }

        result.ok      = errors == 0;
        result.message = std::format(
            "{} deleted, {} not present, {} errors - retry inject to re-download the driver",
            deleted, not_present, errors);
        return result;
    }
}
