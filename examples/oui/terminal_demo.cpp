#include <oui/oui.h>

#include <memory>
#include <string_view>
#include <utility>

using namespace olagarro;

namespace
{

class SmokeBackend final : public oui::Backend
{
  public:
    [[nodiscard]] oui::Size size() const override { return {80, 25}; }
    std::optional<oui::Event> poll_event() override { return std::nullopt; }
    void present(const oui::CellBuffer&) override {}
};

} // namespace

int main(int argument_count, char* const* const arguments)
{
    const bool smoke_test{argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test"};
    std::unique_ptr<oui::Backend> backend;
    if(smoke_test)
    {
        backend = std::make_unique<SmokeBackend>();
    }
    else
    {
        backend = oui::terminal_backend();
    }
    oui::Application application{std::move(backend)};
    oui::Theme theme{};
    theme.background = {0, 0, 170};
    theme.panel_background = {0, 0, 170};
    theme.control_background = {0, 0, 85};
    theme.text = {255, 255, 255};
    theme.muted_text = {85, 255, 255};
    theme.focus_background = {85, 255, 255};
    theme.focus_text = {0, 0, 85};
    application.set_theme(theme);

    auto window{oui::window("OUI terminal demo")};
    auto& window_reference{*window};
    auto content{oui::column()};
    content->set_gap(oui::Spacing::tight);
    content->add(oui::label("Type Unicode text. Tab changes focus. Escape closes."));

    auto& form{oui::grid(*content)};
    form.set_columns({oui::Length::content(), oui::Length::fill()});
    form.set_gap(oui::Spacing::tight);
    form.add(oui::label("Project"));
    auto project{oui::line_edit()};
    project->set_placeholder("Project name");
    form.add(std::move(project));
    form.add(oui::label("Description"));
    auto description{oui::line_edit()};
    description->set_placeholder("A short project description");
    form.add(std::move(description));

    auto& browsers{oui::row(*content)};
    browsers.set_gap(oui::Spacing::loose);
    auto tree{std::make_unique<oui::TreeView>()};
    auto& source{tree->add("source")};
    source.add("main.cpp");
    source.add("ui.cpp");
    tree->add("README.md");
    browsers.add(std::move(tree));

    auto items{oui::list()};
    items->add(oui::label("List item: selectable with Up and Down"));
    items->add(oui::label("List item: arbitrary child widgets"));
    browsers.add(std::move(items));

    auto& actions{oui::row(*content)};
    actions.set_gap(oui::Spacing::tight);
    actions.set_centered(true);
    oui::button("Quit", actions, [&window_reference] { window_reference.close(oui::DialogResult::cancelled); });

    window->add(std::move(content));
    application.set_window(std::move(window));
    if(smoke_test)
    {
        application.post([&application] { application.quit(); });
    }
    return application.exec();
}
