#include "app.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
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
        const ftxui::Color polaris_purple     = ftxui::Color::RGB(0x88, 0x6F, 0xFF);
        const ftxui::Color polaris_purple_dim = ftxui::Color::RGB(0x4C, 0x3F, 0x8F);
        const ftxui::Color row_focus_bg       = ftxui::Color::RGB(0x22, 0x1F, 0x30);

        constexpr std::size_t log_tail_capacity = 200;
        constexpr std::size_t log_tail_visible  = 8;

        enum class screen : int
        {
            pick    = 0,
            confirm = 1,
            running = 2,
            result  = 3,
        };

        class status_log
        {
        public:
            void push(std::string line)
            {
                const std::scoped_lock lk{ mtx_ };
                lines_.push_back(std::move(line));
                while (lines_.size() > log_tail_capacity) lines_.pop_front();
            }

            [[nodiscard]] std::vector<std::string> tail(std::size_t n) const
            {
                const std::scoped_lock lk{ mtx_ };
                if (lines_.size() <= n)
                    return { lines_.begin(), lines_.end() };
                return { std::prev(lines_.end(), static_cast<std::ptrdiff_t>(n)), lines_.end() };
            }

            void clear()
            {
                const std::scoped_lock lk{ mtx_ };
                lines_.clear();
            }

        private:
            mutable std::mutex      mtx_;
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
                case core::subsystem::main:       return polaris_purple;
            }
            return ftxui::Color::Default;
        }

        [[nodiscard]] std::string to_lower_copy(std::string_view s)
        {
            std::string out;
            out.reserve(s.size());
            for (char c : s)
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            return out;
        }

        [[nodiscard]] ftxui::Element brand_header()
        {
            using namespace ftxui;
            return vbox({
                hbox({
                    text("  "),
                    text("polaris") | bold | color(polaris_purple),
                    text("  troubleshooting") | bold,
                    filler(),
                    text("v0.1.0") | dim,
                    text("  "),
                }),
                hbox({
                    text("  "),
                    text("-----") | color(polaris_purple),
                }),
                text(""),
            });
        }

        [[nodiscard]] ftxui::Element hint(std::string_view s)
        {
            using namespace ftxui;
            return hbox({ filler(), text(std::string(s)) | dim, filler() });
        }
    }

    int run()
    {
        using namespace ftxui;

        auto screen_h      = ScreenInteractive::Fullscreen();
        const auto entries = core::all_entries();
        const int total    = static_cast<int>(entries.size());

        std::vector<std::string> code_strs;
        std::vector<std::string> code_strs_lower;
        std::vector<std::string> symbol_strs;
        std::vector<std::string> symbol_strs_lower;
        code_strs.reserve(entries.size());
        symbol_strs.reserve(entries.size());
        code_strs_lower.reserve(entries.size());
        symbol_strs_lower.reserve(entries.size());
        for (const auto & e : entries)
        {
            auto c = core::format_code(e.code);
            code_strs_lower.push_back(to_lower_copy(c));
            code_strs.push_back(std::move(c));
            symbol_strs.emplace_back(e.symbol);
            symbol_strs_lower.push_back(to_lower_copy(e.symbol));
        }

        int              current_tab        = static_cast<int>(screen::pick);
        std::string      search_text;
        std::vector<int> filtered;
        int              filtered_selection = 0;
        int              confirm_focus      = 0;
        int              result_focus       = 0;

        std::atomic<bool> fix_running{ false };
        std::atomic<int>  spinner_frame{ 0 };

        std::mutex        last_result_mtx;
        core::fix_result  last_result{};
        std::string       last_result_code;
        std::string       running_code;

        status_log log;

        std::vector<std::string> menu_dummy;

        auto recompute_filter = [&]
        {
            const std::string q = to_lower_copy(search_text);
            filtered.clear();
            for (int i = 0; i < total; ++i)
            {
                if (q.empty()
                    || code_strs_lower[i].find(q) != std::string::npos
                    || symbol_strs_lower[i].find(q) != std::string::npos)
                {
                    filtered.push_back(i);
                }
            }
            menu_dummy.assign(filtered.size(), std::string{});
            if (filtered.empty())
            {
                filtered_selection = 0;
                return;
            }
            if (filtered_selection >= static_cast<int>(filtered.size()))
                filtered_selection = static_cast<int>(filtered.size()) - 1;
            if (filtered_selection < 0) filtered_selection = 0;
        };
        recompute_filter();

        auto spawn_fix = [&](int entry_idx)
        {
            if (fix_running.load(std::memory_order_acquire)) return;
            if (entry_idx < 0 || entry_idx >= total) return;
            const auto & entry = entries[entry_idx];
            if (!entry.has_auto_fix()) return;

            log.clear();
            running_code = code_strs[entry_idx];
            fix_running.store(true, std::memory_order_release);
            spinner_frame.store(0, std::memory_order_release);
            current_tab = static_cast<int>(screen::running);
            screen_h.PostEvent(Event::Custom);

            std::thread([&]
            {
                while (fix_running.load(std::memory_order_acquire))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    spinner_frame.fetch_add(1, std::memory_order_relaxed);
                    screen_h.PostEvent(Event::Custom);
                }
            }).detach();

            std::thread([&, code_str = code_strs[entry_idx], fixer = entry.fixer]() mutable
            {
                core::fix_context ctx{ [&](std::string_view msg)
                {
                    log.push(std::string(msg));
                    screen_h.PostEvent(Event::Custom);
                } };

                core::fix_result r = fixer(ctx);
                {
                    const std::scoped_lock lk{ last_result_mtx };
                    last_result      = std::move(r);
                    last_result_code = code_str;
                }
                fix_running.store(false, std::memory_order_release);
                current_tab  = static_cast<int>(screen::result);
                result_focus = 0;
                screen_h.PostEvent(Event::Custom);
            }).detach();
        };

        InputOption search_opt = InputOption::Default();
        search_opt.placeholder = "type a code (e.g. M014) or a keyword";
        search_opt.on_change   = [&] { recompute_filter(); };
        search_opt.on_enter    = [&]
        {
            if (filtered.empty()) return;
            confirm_focus = 0;
            current_tab   = static_cast<int>(screen::confirm);
        };
        auto search_input = Input(&search_text, search_opt);

        MenuOption picker_opt = MenuOption::Vertical();
        picker_opt.entries_option.transform = [&](const EntryState & s) -> Element
        {
            const int fi = s.index;
            if (fi < 0 || fi >= static_cast<int>(filtered.size()))
                return text("");
            const int real = filtered[fi];
            const auto & e = entries[real];
            const auto  sub_col = subsys_color(e.sub);

            Element arrow = s.focused
                ? text("  > ") | color(polaris_purple) | bold
                : text("    ");
            Element code_e = text(code_strs[real])   | bold | color(sub_col);
            Element sym_e  = text(symbol_strs[real]) | dim;
            Element chip   = s.focused
                ? text(" [ auto fix ] ") | color(Color::White) | bgcolor(polaris_purple) | bold
                : text(" [ auto fix ] ") | color(polaris_purple);

            Element row = hbox({
                arrow, code_e, text("   "), sym_e, filler(), chip, text("  "),
            });
            if (s.focused) row = row | bgcolor(row_focus_bg);
            return row;
        };
        picker_opt.on_enter = [&]
        {
            if (filtered.empty()) return;
            confirm_focus = 0;
            current_tab   = static_cast<int>(screen::confirm);
        };
        auto picker = Menu(&menu_dummy, &filtered_selection, picker_opt);

        auto pick_container = Container::Vertical({ search_input, picker });

        auto primary_button = [&](std::string label, std::function<void()> on_click)
        {
            ButtonOption opt = ButtonOption::Ascii();
            opt.transform = [](const EntryState & s) -> Element
            {
                Element l = text(" " + s.label + " ") | bold;
                if (s.focused || s.active)
                    return l | color(Color::White) | bgcolor(polaris_purple);
                return l | color(polaris_purple) | bgcolor(row_focus_bg);
            };
            return Button(label, std::move(on_click), opt);
        };

        auto secondary_button = [&](std::string label, std::function<void()> on_click)
        {
            ButtonOption opt = ButtonOption::Ascii();
            opt.transform = [](const EntryState & s) -> Element
            {
                Element l = text(" " + s.label + " ");
                if (s.focused || s.active)
                    return l | color(Color::White) | bgcolor(polaris_purple_dim);
                return l | dim;
            };
            return Button(label, std::move(on_click), opt);
        };

        auto run_btn = primary_button("run the fix", [&]
        {
            if (filtered.empty()) return;
            if (filtered_selection < 0
                || filtered_selection >= static_cast<int>(filtered.size())) return;
            spawn_fix(filtered[filtered_selection]);
        });

        auto back_btn = secondary_button("<- back", [&]
        {
            current_tab = static_cast<int>(screen::pick);
        });

        auto confirm_container = Container::Horizontal({ run_btn, back_btn }, &confirm_focus);

        auto another_btn = primary_button("fix another", [&]
        {
            search_text.clear();
            recompute_filter();
            current_tab = static_cast<int>(screen::pick);
        });

        auto done_btn = secondary_button("done", [&]
        {
            screen_h.ExitLoopClosure()();
        });

        auto result_container = Container::Horizontal({ another_btn, done_btn }, &result_focus);

        auto running_container = Renderer([] { return text(""); });

        auto root_tab = Container::Tab({
            pick_container,
            confirm_container,
            running_container,
            result_container,
        }, &current_tab);

        auto ui = Renderer(root_tab, [&]() -> Element
        {
            Element body;
            const auto s = static_cast<screen>(current_tab);

            if (s == screen::pick)
            {
                Element list_e;
                if (filtered.empty())
                    list_e = hbox({ filler(),
                        text("no matches for '" + search_text + "'") | dim,
                        filler() });
                else
                    list_e = picker->Render();

                body = vbox({
                    text(""),
                    hbox({ filler(),
                        text("what error code did the loader show you?") | bold,
                        filler() }),
                    text(""),
                    hbox({ filler(),
                        search_input->Render() | borderRounded
                            | size(WIDTH, EQUAL, 66),
                        filler() }),
                    text(""),
                    list_e | vscroll_indicator | yframe
                        | size(HEIGHT, LESS_THAN, 14),
                    filler(),
                    hint("up/down pick    enter select    esc quit"),
                    text(""),
                });
            }
            else if (s == screen::confirm)
            {
                const int real = (filtered_selection >= 0
                                  && filtered_selection < static_cast<int>(filtered.size()))
                    ? filtered[filtered_selection] : -1;
                if (real < 0 || real >= total)
                {
                    body = text("no code selected");
                }
                else
                {
                    const auto & e = entries[real];
                    Elements will_do;
                    for (const auto & step : e.manual_steps)
                    {
                        will_do.push_back(hbox({
                            text("     > ") | color(polaris_purple),
                            paragraph(step) | dim,
                        }));
                    }

                    Element code_card = window(
                        text(""),
                        text(" " + code_strs[real] + " ") | bold
                            | color(Color::White) | bgcolor(polaris_purple));

                    body = vbox({
                        filler(),
                        hbox({ filler(), code_card, filler() }),
                        text(""),
                        hbox({ filler(),
                            text(std::string(e.summary)) | bold,
                            filler() }),
                        text(""),
                        text(""),
                        text("   this fix will:") | dim,
                        text(""),
                        vbox(std::move(will_do)),
                        text(""),
                        text("   close the polaris loader before running.") | dim,
                        filler(),
                        hbox({ filler(),
                            run_btn->Render(),
                            text("    "),
                            back_btn->Render(),
                            filler() }),
                        text(""),
                        hint("tab / left-right switch    enter run    esc back"),
                        text(""),
                    });
                }
            }
            else if (s == screen::running)
            {
                const auto tail = log.tail(log_tail_visible);
                Elements tail_e;
                tail_e.reserve(tail.size() + 2);
                if (tail.empty())
                    tail_e.push_back(hbox({ filler(),
                        text("(waiting for the fix to start...)") | dim,
                        filler() }));
                for (const auto & t : tail)
                    tail_e.push_back(hbox({
                        text("     "),
                        text(t) | dim,
                    }));

                body = vbox({
                    filler(),
                    hbox({ filler(),
                        spinner(4, spinner_frame.load(std::memory_order_relaxed))
                            | color(polaris_purple),
                        text("   running fix  ") | bold | color(polaris_purple),
                        text(running_code) | bold,
                        filler() }),
                    text(""),
                    text(""),
                    hbox({ filler(),
                        text("---------------------------------------------")
                            | color(polaris_purple_dim),
                        filler() }),
                    text(""),
                    vbox(std::move(tail_e)),
                    text(""),
                    hbox({ filler(),
                        text("---------------------------------------------")
                            | color(polaris_purple_dim),
                        filler() }),
                    filler(),
                    text(""),
                });
            }
            else
            {
                core::fix_result snap;
                std::string      code;
                {
                    const std::scoped_lock lk{ last_result_mtx };
                    snap = last_result;
                    code = last_result_code;
                }

                const std::string verdict_label = snap.ok ? "   success   " : "   failed    ";
                const Color       verdict_bg    = snap.ok
                    ? Color::RGB(0x1F, 0x8A, 0x3D)
                    : Color::RGB(0xC0, 0x2A, 0x2A);
                const Color       verdict_fg    = Color::White;

                int ok_deleted     = 0;
                int not_present    = 0;
                int failed_actions = 0;

                Elements action_rows;
                action_rows.reserve(snap.actions_taken.size());
                for (const auto & a : snap.actions_taken)
                {
                    Element mark;
                    if (!a.ok)
                    {
                        mark = text("  x  ") | color(Color::Red) | bold;
                        ++failed_actions;
                    }
                    else if (a.description.starts_with("not present"))
                    {
                        mark = text("  .  ") | dim;
                        ++not_present;
                    }
                    else
                    {
                        mark = text("  +  ") | color(Color::Green) | bold;
                        ++ok_deleted;
                    }
                    action_rows.push_back(hbox({
                        text("   "),
                        mark,
                        text(" "),
                        text(a.description) | dim,
                    }));
                }
                if (action_rows.empty())
                    action_rows.push_back(hbox({ filler(),
                        text("(no actions recorded)") | dim,
                        filler() }));

                std::string counters = std::to_string(ok_deleted) + " done  "
                    "  " + std::to_string(not_present) + " not present  "
                    "  " + std::to_string(failed_actions) + " errors";

                body = vbox({
                    filler(),
                    hbox({ filler(),
                        text(verdict_label) | bold | color(verdict_fg) | bgcolor(verdict_bg),
                        text("   -   "),
                        text(code) | bold,
                        filler() }),
                    text(""),
                    text(""),
                    hbox({ filler(), text(counters) | dim, filler() }),
                    text(""),
                    hbox({ filler(),
                        paragraph(snap.message),
                        filler() }),
                    text(""),
                    text("   actions") | dim,
                    vbox(std::move(action_rows)) | vscroll_indicator | yframe
                        | size(HEIGHT, LESS_THAN, 10),
                    filler(),
                    hbox({ filler(),
                        another_btn->Render(),
                        text("    "),
                        done_btn->Render(),
                        filler() }),
                    text(""),
                    hint("tab / left-right switch    enter select    esc done"),
                    text(""),
                });
            }

            return vbox({
                brand_header(),
                body | flex,
            }) | border;
        });

        auto app = CatchEvent(ui, [&](const Event & e) -> bool
        {
            if (fix_running.load(std::memory_order_acquire))
                return false;

            if (e == Event::Escape)
            {
                switch (static_cast<screen>(current_tab))
                {
                    case screen::pick:
                        screen_h.ExitLoopClosure()();
                        return true;
                    case screen::confirm:
                        current_tab = static_cast<int>(screen::pick);
                        return true;
                    case screen::running:
                        return true;
                    case screen::result:
                        screen_h.ExitLoopClosure()();
                        return true;
                }
            }
            return false;
        });

        screen_h.Loop(app);
        return 0;
    }
}
