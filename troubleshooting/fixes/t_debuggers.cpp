#include "t_debuggers.hpp"

#include <array>
#include <cctype>
#include <format>
#include <string>
#include <string_view>

#include "../platform/processes.hpp"

namespace ts::fixes::t_debuggers
{
    namespace
    {
        constexpr std::array<std::string_view, 21> exact_names =
        {
            "ida.exe",
            "ida64.exe",
            "idag.exe",
            "idag64.exe",
            "idaq.exe",
            "idaq64.exe",
            "x64dbg.exe",
            "x32dbg.exe",
            "x96dbg.exe",
            "windbg.exe",
            "windbgx.exe",
            "cdb.exe",
            "ntsd.exe",
            "ollydbg.exe",
            "processhacker.exe",
            "systeminformer.exe",
            "httpdebuggerui.exe",
            "httpdebuggersvc.exe",
            "dnspy.exe",
            "dnspy-x86.exe",
            "dnspy.console.exe",
        };

        constexpr std::array<std::string_view, 2> prefixes =
        {
            "cheatengine-",
            "cheatengine.",
        };

        [[nodiscard]] std::string to_lower(std::string_view s)
        {
            std::string out;
            out.reserve(s.size());
            for (char c : s)
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            return out;
        }

        [[nodiscard]] bool matches_debugger(std::string_view name)
        {
            const std::string lower = to_lower(name);
            for (auto e : exact_names)
                if (lower == e) return true;
            for (auto p : prefixes)
                if (lower.starts_with(p)) return true;
            return false;
        }
    }

    core::fix_result run(core::fix_context & ctx)
    {
        const auto processes = platform::enumerate_processes();
        if (processes.empty())
        {
            return core::fix_result::failure(
                "failed to enumerate processes (Toolhelp snapshot returned nothing)");
        }

        core::fix_result result = core::fix_result::success("");

        int killed = 0;
        int failed = 0;

        for (const auto & p : processes)
        {
            if (ctx.cancelled()) break;
            if (!matches_debugger(p.exe_name)) continue;

            const std::string exe_lower = to_lower(p.exe_name);
            ctx.log(std::format("terminate {} (pid {})", p.exe_name, p.pid));
            if (platform::kill_process(p.pid, exe_lower))
            {
                ++killed;
                result.record(
                    std::format("killed: {} (pid {})", p.exe_name, p.pid),
                    core::action_outcome::done);
            }
            else
            {
                ++failed;
                ctx.log("  terminate failed (protected, pid reused, or insufficient rights)");
                result.record(
                    std::format("failed: {} (pid {})", p.exe_name, p.pid),
                    core::action_outcome::failed);
            }
        }

        result.ok = (failed == 0) || (killed > 0);
        if (killed == 0 && failed == 0)
            result.message = "no known debugger was running - relaunch the loader anyway";
        else if (failed == 0)
            result.message = std::format(
                "{} killed - relaunch the loader", killed);
        else if (killed == 0)
            result.message = std::format(
                "{} could not be terminated (protected or pid reused) - "
                "close manually and relaunch the loader", failed);
        else
            result.message = std::format(
                "{} killed, {} could not be terminated - "
                "close the remaining ones manually and relaunch the loader",
                killed, failed);
        return result;
    }
}
