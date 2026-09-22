#include "m020_dll_cache.hpp"

#include <filesystem>
#include <format>
#include <system_error>

#include "../platform/paths.hpp"

namespace ts::fixes::m020
{
    namespace
    {
        [[nodiscard]] bool matches_dll_cache(const std::filesystem::path & p) noexcept
        {
            const auto name = p.filename().wstring();
            if (!name.starts_with(L"dll-")) return false;
            if (!name.ends_with(L".bin"))  return false;
            return true;
        }
    }

    core::fix_result run(core::fix_context & ctx)
    {
        const auto dirs = platform::polaris_stage_dirs();
        if (dirs.empty())
        {
            return core::fix_result::failure(
                "no Polaris-<hash> directory found under %LOCALAPPDATA%");
        }

        core::fix_result result = core::fix_result::success("");

        int deleted = 0;
        int errors  = 0;

        for (const auto & dir : dirs)
        {
            if (ctx.cancelled()) break;

            std::error_code ec;
            for (auto it = std::filesystem::directory_iterator(dir, ec);
                 !ec && it != std::filesystem::directory_iterator{};
                 it.increment(ec))
            {
                if (ctx.cancelled()) break;
                const auto & entry = *it;
                if (!matches_dll_cache(entry.path())) continue;

                if (platform::has_reparse_point(entry.path()))
                {
                    ++errors;
                    ctx.log(std::format("  refused reparse point: {}",
                        entry.path().string()));
                    result.record("refused (reparse point): "
                        + entry.path().string(), false);
                    continue;
                }

                std::error_code type_ec;
                if (!entry.is_regular_file(type_ec) || type_ec) continue;

                ctx.log(std::format("delete {}", entry.path().string()));
                std::error_code rm_ec;
                std::filesystem::remove(entry.path(), rm_ec);
                if (rm_ec)
                {
                    ++errors;
                    ctx.log(std::format("  remove failed: {}", rm_ec.message()));
                    result.record("failed: " + entry.path().string(), false);
                }
                else
                {
                    ++deleted;
                    result.record("deleted: " + entry.path().string(), true);
                }
            }
        }

        result.ok      = errors == 0;
        result.message = std::format(
            "{} deleted, {} errors - retry inject to re-download the module",
            deleted, errors);
        return result;
    }
}
