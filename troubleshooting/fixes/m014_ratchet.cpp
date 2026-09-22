#include "m014_ratchet.hpp"

#include <filesystem>
#include <format>
#include <system_error>

#include "../platform/paths.hpp"

namespace ts::fixes::m014
{
    core::fix_result run(core::fix_context & ctx)
    {
        const auto dirs = platform::polaris_stage_dirs();
        if (dirs.empty())
        {
            return core::fix_result::failure(
                "no Polaris-<hash> directory found under %LOCALAPPDATA% (nothing to delete)");
        }

        core::fix_result result = core::fix_result::success("");

        int deleted     = 0;
        int not_present = 0;
        int errors      = 0;

        for (const auto & dir : dirs)
        {
            if (ctx.cancelled()) break;

            const auto ratchet = dir / L"ratchet.dat";
            ctx.log(std::format("check {}", ratchet.string()));

            if (platform::has_reparse_point(ratchet))
            {
                ++errors;
                ctx.log("  refused: reparse point (symlink or junction)");
                result.record("refused (reparse point): " + ratchet.string(), false);
                continue;
            }

            std::error_code ec;
            if (!std::filesystem::exists(ratchet, ec) || ec)
            {
                ++not_present;
                result.record("not present: " + ratchet.string(), true);
                continue;
            }

            std::filesystem::remove(ratchet, ec);
            if (ec)
            {
                ++errors;
                ctx.log(std::format("  remove failed: {}", ec.message()));
                result.record("failed: " + ratchet.string(), false);
            }
            else
            {
                ++deleted;
                ctx.log("  deleted");
                result.record("deleted: " + ratchet.string(), true);
            }
        }

        result.ok      = errors == 0;
        result.message = std::format(
            "{} deleted, {} not present, {} errors - sign out and sign in again from the loader",
            deleted, not_present, errors);
        return result;
    }
}
