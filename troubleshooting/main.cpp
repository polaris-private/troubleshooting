#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

int main()
{
    auto screen = ftxui::ScreenInteractive::Fullscreen();

    auto root = ftxui::Renderer([]
    {
        return ftxui::vbox({
            ftxui::text("polaris troubleshooting") | ftxui::bold | ftxui::center,
            ftxui::separator(),
            ftxui::text("press q to quit") | ftxui::dim | ftxui::center,
        }) | ftxui::border;
    });

    auto app = ftxui::CatchEvent(root, [&](const ftxui::Event & e) -> bool
    {
        if (e == ftxui::Event::Character('q') || e == ftxui::Event::Escape)
        {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(app);
    return 0;
}
