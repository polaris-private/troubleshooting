#pragma once

#include <atomic>
#include <functional>
#include <string_view>

namespace ts::core
{
    class fix_context
    {
    public:
        using log_fn = std::function<void(std::string_view)>;

        explicit fix_context(log_fn on_log) noexcept;

        fix_context(const fix_context &) = delete;
        fix_context & operator=(const fix_context &) = delete;

        void log(std::string_view msg) const;

        [[nodiscard]] bool cancelled() const noexcept;
        void request_cancel() noexcept;

    private:
        log_fn on_log_;
        mutable std::atomic<bool> cancelled_{ false };
    };
}
