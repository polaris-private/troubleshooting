#include "fix_context.hpp"

namespace ts::core
{
    fix_context::fix_context(log_fn on_log) noexcept
        : on_log_{ std::move(on_log) }
    {
    }

    void fix_context::log(std::string_view msg) const
    {
        if (on_log_) on_log_(msg);
    }

    bool fix_context::cancelled() const noexcept
    {
        return cancelled_.load(std::memory_order_acquire);
    }

    void fix_context::request_cancel() noexcept
    {
        cancelled_.store(true, std::memory_order_release);
    }
}
