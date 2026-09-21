#pragma once

#include <string>
#include <vector>

namespace ts::core
{
    struct fix_action
    {
        std::string description;
        bool ok{ true };
    };

    struct fix_result
    {
        bool ok{ false };
        std::string message;
        std::vector<fix_action> actions_taken;

        [[nodiscard]] static fix_result success(std::string msg)
        {
            return fix_result{ true, std::move(msg), {} };
        }

        [[nodiscard]] static fix_result failure(std::string msg)
        {
            return fix_result{ false, std::move(msg), {} };
        }

        void record(std::string description, bool ok = true)
        {
            actions_taken.push_back(fix_action{ std::move(description), ok });
        }
    };
}
