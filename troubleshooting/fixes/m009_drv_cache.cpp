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

            std::error_code ec;
            if (!std::filesystem::exists(blob, ec) || ec)
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
