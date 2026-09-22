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

            const auto blob     = dir / L"drv.bin";
            const auto blob_str = platform::path_to_utf8(blob);
            ctx.log(std::format("check {}", blob_str));

            if (platform::has_reparse_point(blob))
            {
                ++errors;
                ctx.log("  refused: reparse point (symlink or junction)");
                result.record("refused (reparse point): " + blob_str, false);
                continue;
            }

            std::error_code ec;
            const bool present = std::filesystem::exists(blob, ec);
            if (ec)
            {
                ++errors;
                ctx.log(std::format("  exists check failed: {}",
                    platform::ec_message_utf8(ec)));
                result.record("check failed: " + blob_str, false);
                continue;
            }
            if (!present)
            {
                ++not_present;
                result.record("not present: " + blob_str, true);
                continue;
            }

            std::filesystem::remove(blob, ec);
            if (ec)
            {
                ++errors;
                ctx.log(std::format("  remove failed: {}",
                    platform::ec_message_utf8(ec)));
                result.record("failed: " + blob_str, false);
            }
            else
            {
                ++deleted;
                ctx.log("  deleted");
                result.record("deleted: " + blob_str, true);
            }
        }

        result.ok      = errors == 0;
        result.message = std::format(
            "{} deleted, {} not present, {} errors - retry inject to re-download the driver",
            deleted, not_present, errors);
        return result;
    }
}
