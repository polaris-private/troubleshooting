#include "app.hpp"

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "../core/err_code.hpp"
#include "../core/fix_context.hpp"
#include "../core/fix_result.hpp"
#include "../core/registry.hpp"

namespace ts::ui
{
    namespace
    {
        constexpr std::size_t log_capacity = 200;

        class status_log
        {
        public:
            void push(std::string line)
            {
                const std::scoped_lock lk{ mtx_ };
                lines_.push_back(std::move(line));
                while (lines_.size() > log_capacity) lines_.pop_front();
            }

            [[nodiscard]] std::vector<std::string> snapshot() const
            {
                const std::scoped_lock lk{ mtx_ };
                return { lines_.begin(), lines_.end() };
            }

            void clear()
            {
                const std::scoped_lock lk{ mtx_ };
                lines_.clear();
            }

        private:
            mutable std::mutex     mtx_;
            std::deque<std::string> lines_;
        };

        [[nodiscard]] ftxui::Color subsys_color(core::subsystem s) noexcept
        {
            switch (s)
            {
                case core::subsystem::auth:       return ftxui::Color::Cyan;
                case core::subsystem::license:    return ftxui::Color::Magenta;
                case core::subsystem::overlay:    return ftxui::Color::Blue;
                case core::subsystem::tamper:     return ftxui::Color::Yellow;
                case core::subsystem::protection: return ftxui::Color::Orange1;
                case core::subsystem::utilities:  return ftxui::Color::GrayLight;
                case core::subsystem::main:       return ftxui::Color::Green;
            }
            return ftxui::Color::Default;
        }

        [[nodiscard]] ftxui::Element render_detail(const core::code_entry & e)
        {
            using namespace ftxui;

            Elements steps;
            steps.reserve(e.manual_steps.size());
            for (std::size_t i = 0; i < e.manual_steps.size(); ++i)
            {
                steps.push_back(hbox({
                    text(std::to_string(i + 1) + ". ") | color(Color::GrayDark),
                    paragraph(e.manual_steps[i]),
                }));
            }

            Elements body;
            body.reserve(8 + steps.size());
            body.push_back(hbox({
                text(core::format_code(e.code)) | bold | color(subsys_color(e.sub)),
                text("  "),
                text(std::string(e.symbol)) | dim,
            }));
            body.push_back(text(std::string(core::subsys_name(e.sub))) | dim);
            body.push_back(separator());
            body.push_back(paragraph(std::string(e.summary)));
            body.push_back(text(""));
            body.push_back(text("manual steps:") | bold);
            for (auto & s : steps) body.push_back(std::move(s));
            body.push_back(text(""));
            body.push_back(text("auto fix available") | color(Color::Green));

            return vbox(std::move(body));
        }
    }

    int run()
    {
        using namespace ftxui;

        auto screen = ScreenInteractive::Fullscreen();

        const auto entries = core::all_entries();

        std::vector<std::string> code_column;
        std::vector<std::string> symbol_column;
        code_column.reserve(entries.size());
        symbol_column.reserve(entries.size());
        for (const auto & e : entries)
        {
            code_column.push_back(core::format_code(e.code));
            symbol_column.emplace_back(e.symbol);
        }

        std::vector<std::string> menu_labels(entries.size(), std::string{});
        int selected = 0;

        MenuOption menu_opt = MenuOption::Vertical();
        menu_opt.entries_option.transform = [&](const EntryState & s) -> Element
        {
            const std::size_t i = static_cast<std::size_t>(s.index);
            if (i >= entries.size()) return text(s.label);

            const auto & entry = entries[i];
            const auto sub_col  = subsys_color(entry.sub);

            Element code_e   = text(code_column[i])   | bold | color(sub_col);
            Element sym_e    = text(symbol_column[i]) | dim;
            Element marker   = text(s.active ? " > " : "   ");
            if (s.focused) marker = text(" > ") | color(Color::White) | bold;

            Element row = hbox({ marker, code_e, text("  "), sym_e });
            if (s.focused) row = row | inverted;
            return row;
        };
        auto menu = Menu(&menu_labels, &selected, menu_opt);

        status_log log;
        std::atomic<bool> fix_running{ false };

        auto detail_renderer = Renderer([&]
        {
            if (selected < 0 || selected >= static_cast<int>(entries.size()))
                return text("no code selected");
            return render_detail(entries[selected]);
        });

        auto run_auto_fix = [&]
        {
            if (fix_running.load(std::memory_order_acquire)) return;
            if (selected < 0 || selected >= static_cast<int>(entries.size())) return;
            const auto & entry = entries[selected];
            if (!entry.has_auto_fix())
            {
                log.push(std::string("[") + core::format_code(entry.code) + "] no auto fix");
                screen.PostEvent(Event::Custom);
                return;
            }

            fix_running.store(true, std::memory_order_release);
            log.push(std::string("[") + core::format_code(entry.code) + "] running auto fix...");
            screen.PostEvent(Event::Custom);

            std::thread([&, code_str = core::format_code(entry.code), fixer = entry.fixer]() mutable
            {
                core::fix_context ctx{ [&](std::string_view msg)
                {
                    log.push(std::string("  ") + std::string(msg));
                    screen.PostEvent(Event::Custom);
                } };

                core::fix_result r = fixer(ctx);
                std::string tail = std::string("[") + code_str + "] "
                    + (r.ok ? "ok: " : "failed: ") + r.message;
                log.push(std::move(tail));
                fix_running.store(false, std::memory_order_release);
                screen.PostEvent(Event::Custom);
            }).detach();
        };

        auto auto_fix_button = Button("[ auto fix ]", run_auto_fix, ButtonOption::Ascii());
        auto quit_button = Button("[ quit ]", screen.ExitLoopClosure(), ButtonOption::Ascii());

        auto action_row = Container::Horizontal({ auto_fix_button, quit_button });

        auto right_pane = Container::Vertical({ detail_renderer, action_row });

        auto layout = Container::Horizontal({ menu, right_pane });

        auto ui = Renderer(layout, [&]
        {
            auto detail_view = detail_renderer->Render()
                | vscroll_indicator | yframe | flex;

            auto buttons = hbox({
                auto_fix_button->Render(),
                text("  "),
                quit_button->Render(),
            });

            auto right = vbox({
                detail_view,
                separator(),
                buttons,
            }) | flex;

            auto left = menu->Render()
                | vscroll_indicator | yframe
                | size(WIDTH, EQUAL, 34);

            auto body = hbox({
                left | border,
                right | border,
            }) | flex;

            const auto log_lines = log.snapshot();
            Elements log_elems;
            log_elems.reserve(log_lines.size());
            const std::size_t start = log_lines.size() > 6 ? log_lines.size() - 6 : 0;
            for (std::size_t i = start; i < log_lines.size(); ++i)
                log_elems.push_back(text(log_lines[i]));
            if (log_elems.empty())
                log_elems.push_back(text("(no actions yet)") | dim);

            return vbox({
                hbox({
                    text("polaris troubleshooting") | bold,
                    filler(),
                    text(fix_running.load() ? "working..." : "idle") | dim,
                    text("  "),
                    text("q to quit") | dim,
                }) | border,
                body,
                vbox({
                    text(" status log ") | bold,
                    vbox(std::move(log_elems)),
                }) | border,
            });
        });

        auto app = CatchEvent(ui, [&](const Event & e) -> bool
        {
            if (e == Event::Character('q') || e == Event::Escape)
            {
                if (!fix_running.load(std::memory_order_acquire))
                {
                    screen.ExitLoopClosure()();
                    return true;
                }
            }
            return false;
        });

        screen.Loop(app);
        return 0;
    }
}
