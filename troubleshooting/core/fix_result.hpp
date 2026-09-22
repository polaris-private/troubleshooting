#pragma once

#include <string>
#include <vector>

namespace ts::core
{
    enum class action_outcome
    {
        done,
        not_present,
        failed,
    };

    struct fix_action
    {
        std::string    description;
        action_outcome outcome{ action_outcome::done };
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

        void record(std::string description, action_outcome outcome = action_outcome::done)
        {
            actions_taken.push_back(fix_action{ std::move(description), outcome });
        }
    };
}
