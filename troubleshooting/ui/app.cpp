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

        std::mutex        last_result_mtx;
        core::fix_result  last_result{};
        std::string       last_result_code;
        bool              show_result = false;

        auto detail_renderer = Renderer([&]
        {
            if (selected < 0 || selected >= static_cast<int>(entries.size()))
                return text("no code selected");
            return render_detail(entries[selected]);
        });

        auto spawn_fix = [&]
        {
            if (fix_running.load(std::memory_order_acquire)) return;
            if (selected < 0 || selected >= static_cast<int>(entries.size())) return;
            const auto & entry = entries[selected];
            if (!entry.has_auto_fix()) return;

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

                {
                    const std::scoped_lock lk{ last_result_mtx };
                    last_result      = std::move(r);
                    last_result_code = code_str;
                    show_result      = true;
                }

                fix_running.store(false, std::memory_order_release);
                screen.PostEvent(Event::Custom);
            }).detach();
        };

        bool show_confirm = false;

        auto request_fix = [&]
        {
            if (fix_running.load(std::memory_order_acquire)) return;
            if (selected < 0 || selected >= static_cast<int>(entries.size())) return;
            if (!entries[selected].has_auto_fix()) return;
            show_confirm = true;
        };

        auto auto_fix_button = Button("[ auto fix ]", request_fix, ButtonOption::Ascii());
        auto quit_button = Button("[ quit ]", screen.ExitLoopClosure(), ButtonOption::Ascii());

        auto action_row = Container::Horizontal({ auto_fix_button, quit_button });

        auto right_pane = Container::Vertical({ detail_renderer, action_row });

        auto layout = Container::Horizontal({ menu, right_pane });

        auto confirm_yes = Button(" [ yes, run ] ", [&]
        {
            show_confirm = false;
            spawn_fix();
        }, ButtonOption::Ascii());

        auto confirm_no = Button(" [ cancel ] ", [&]
        {
            show_confirm = false;
        }, ButtonOption::Ascii());

        auto confirm_buttons = Container::Horizontal({ confirm_yes, confirm_no });

        auto confirm_modal = Renderer(confirm_buttons, [&]
        {
            std::string code_str = (selected >= 0 && selected < static_cast<int>(entries.size()))
                ? core::format_code(entries[selected].code)
                : std::string{};
            std::string symbol_str = (selected >= 0 && selected < static_cast<int>(entries.size()))
                ? std::string(entries[selected].symbol)
                : std::string{};

            return vbox({
                text(" confirm auto fix ") | bold | center,
                separator(),
                text(""),
                hbox({
                    text("code:   ") | dim,
                    text(code_str) | bold,
                }),
                hbox({
                    text("action: ") | dim,
                    text(symbol_str),
                }),
                text(""),
                paragraph("this will modify state on disk or terminate processes. "
                          "make sure the polaris loader is closed before continuing."),
                text(""),
                hbox({
                    filler(),
                    confirm_yes->Render(),
                    text("  "),
                    confirm_no->Render(),
                    filler(),
                }),
                text(""),
            }) | border | size(WIDTH, GREATER_THAN, 56) | bgcolor(Color::Black);
        });

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

        auto result_close = Button(" [ close ] ", [&]
        {
            const std::scoped_lock lk{ last_result_mtx };
            show_result = false;
        }, ButtonOption::Ascii());

        auto result_buttons = Container::Horizontal({ result_close });

        auto result_modal = Renderer(result_buttons, [&]
        {
            core::fix_result snapshot;
            std::string code_str;
            {
                const std::scoped_lock lk{ last_result_mtx };
                snapshot = last_result;
                code_str = last_result_code;
            }

            Elements action_rows;
            action_rows.reserve(snapshot.actions_taken.size());
            for (const auto & a : snapshot.actions_taken)
            {
                Element bullet = a.ok
                    ? text(" ok  ") | color(Color::Green)
                    : text(" fail") | color(Color::Red);
                action_rows.push_back(hbox({
                    bullet,
                    text("  "),
                    paragraph(a.description),
                }));
            }
            if (action_rows.empty())
                action_rows.push_back(text("(no actions recorded)") | dim);

            Element verdict = snapshot.ok
                ? text(" success ") | color(Color::Black) | bgcolor(Color::Green) | bold
                : text(" failed  ") | color(Color::White) | bgcolor(Color::Red)   | bold;

            return vbox({
                hbox({
                    verdict,
                    text("  "),
                    text(code_str) | bold,
                }),
                separator(),
                text("actions taken:") | dim,
                vbox(std::move(action_rows)) | vscroll_indicator | yframe
                    | size(HEIGHT, LESS_THAN, 12),
                separator(),
                text("summary:") | dim,
                paragraph(snapshot.message),
                text(""),
                hbox({
                    filler(),
                    result_close->Render(),
                    filler(),
                }),
                text(""),
            }) | border | size(WIDTH, GREATER_THAN, 64) | bgcolor(Color::Black);
        });

        auto ui_with_modal = ui
            | Modal(confirm_modal, &show_confirm)
            | Modal(result_modal,  &show_result);

        auto app = CatchEvent(ui_with_modal, [&](const Event & e) -> bool
        {
            if (e == Event::Escape)
            {
                if (show_result)
                {
                    const std::scoped_lock lk{ last_result_mtx };
                    show_result = false;
                    return true;
                }
                if (show_confirm)
                {
                    show_confirm = false;
                    return true;
                }
                if (!fix_running.load(std::memory_order_acquire))
                {
                    screen.ExitLoopClosure()();
                    return true;
                }
            }
            if (e == Event::Character('q'))
            {
                if (!show_confirm && !show_result
                    && !fix_running.load(std::memory_order_acquire))
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
