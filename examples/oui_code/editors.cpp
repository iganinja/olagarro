#include "editors.h"

#include "dialogs.h"
#include "task_list.h"

#include <oui/oui.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <utility>

using namespace olagarro;

namespace
{

struct LayoutNode
{
    // 'h' or 'v' for a split, 'g' for a group.
    char kind{'g'};
    double ratio{0.5};
    std::size_t group{};
    std::unique_ptr<LayoutNode> first;
    std::unique_ptr<LayoutNode> second;
};

// A split's ratio as the layout keeps it, "0.25", to four decimals. By hand: std::to_chars and from_chars for
// floating point bring in more than a hundred kilobytes of tables, and strtod reads by the user's locale.
[[nodiscard]] std::string ratio_text(double ratio)
{
    const auto scaled{std::lround(std::clamp(ratio, 0.0, 1.0) * 10000.0)};
    if(scaled == 10000)
    {
        return "1";
    }
    auto decimals{std::to_string(10000 + scaled).substr(1)};
    while(!decimals.empty() && decimals.back() == '0')
    {
        decimals.pop_back();
    }
    return decimals.empty() ? "0" : "0." + decimals;
}

// Reads a ratio at `at`, digits with a point among them, and leaves `at` after it.
[[nodiscard]] std::optional<double> parse_ratio(std::string_view text, std::size_t& at)
{
    const auto start{at};
    double ratio{};
    for(; at < text.size() && std::isdigit(static_cast<unsigned char>(text[at])) != 0; ++ at)
    {
        ratio = ratio * 10.0 + (text[at] - '0');
    }
    if(at < text.size() && text[at] == '.')
    {
        ++ at;
        for(auto scale{0.1}; at < text.size() && std::isdigit(static_cast<unsigned char>(text[at])) != 0;
            ++ at, scale /= 10.0)
        {
            ratio += (text[at] - '0') * scale;
        }
    }
    if(at == start)
    {
        return std::nullopt;
    }
    return ratio;
}

[[nodiscard]] std::unique_ptr<LayoutNode> parse_layout(std::string_view text, std::size_t& at)
{
    if(at >= text.size())
    {
        return nullptr;
    }
    auto node{std::make_unique<LayoutNode>()};
    if(std::isdigit(static_cast<unsigned char>(text[at])) != 0)
    {
        const auto parsed{std::from_chars(text.data() + at, text.data() + text.size(), node->group)};
        at = static_cast<std::size_t>(parsed.ptr - text.data());
        return node;
    }
    if(text[at] != 'h' && text[at] != 'v')
    {
        return nullptr;
    }
    node->kind = text[at++];
    const auto ratio{parse_ratio(text, at)};
    if(!ratio)
    {
        return nullptr;
    }
    node->ratio = *ratio;
    if(at >= text.size() || text[at++] != '(')
    {
        return nullptr;
    }
    node->first = parse_layout(text, at);
    if(node->first == nullptr || at >= text.size() || text[at++] != ',')
    {
        return nullptr;
    }
    node->second = parse_layout(text, at);
    if(node->second == nullptr || at >= text.size() || text[at++] != ')')
    {
        return nullptr;
    }
    return node;
}

void build(Editors& editors, const LayoutNode& node, EditorGroup& group, std::vector<EditorGroup*>& numbered)
{
    if(node.kind == 'g')
    {
        if(numbered.size() <= node.group)
        {
            numbered.resize(node.group + 1);
        }
        numbered[node.group] = &group;
        return;
    }
    auto& second{editors.split(group, node.kind == 'h' ? Side::right : Side::bottom)};
    dynamic_cast<oui::SplitView&>(*second.tabs.parent()).set_ratio(node.ratio);
    build(editors, *node.first, group, numbered);
    build(editors, *node.second, second, numbered);
}

} // namespace

Editors::Editors(oui::Application& application, oui::SplitView& area) : m_application{application}, m_area{area}
{
    auto& tabs{area.add(oui::tab_view())};
    m_groups.push_back(EditorGroup{tabs, {}});
}

void Editors::prepare_tabs(std::function<void(oui::TabView&)> setup)
{
    for(auto& group : m_groups)
    {
        setup(group.tabs);
    }
    m_setups.push_back(std::move(setup));
}

EditorGroup& Editors::active()
{
    if(const auto* const focused{m_application.focused()})
    {
        for(auto& group : m_groups)
        {
            if(&group.tabs == focused || group.tabs.contains(focused))
            {
                m_active = &group;
            }
        }
    }
    if(m_active == nullptr)
    {
        m_active = &m_groups.front();
    }
    return *m_active;
}

void Editors::set_languages(const Languages* languages)
{
    m_languages = languages;
}

void Editors::apply_languages(bool again)
{
    for(auto& group : m_groups)
    {
        for(std::size_t index{}; index < group.documents.size(); ++ index)
        {
            auto& document{group.documents[index]};
            if(!again && document.language_path == document.path)
            {
                continue;
            }
            document.language_path = document.path;
            if(m_languages == nullptr)
            {
                document.language = nullptr;
            }
            else if(document.chosen_language)
            {
                document.language = m_languages->with_id(*document.chosen_language);
            }
            else
            {
                document.language = document.path ? m_languages->for_path(*document.path) : nullptr;
            }
            if(auto* const editor{document_editor(group.tabs, index)})
            {
                editor->set_syntax_highlighter(document.language ? document.language->highlighter : nullptr);
                editor->set_bracket_pairs(document.language
                                              ? document.language->brackets
                                              : std::vector<std::pair<char, char>>{{'(', ')'}, {'[', ']'}, {'{', '}'}});
            }
        }
    }
}

std::string Editors::layout(std::vector<const EditorGroup*>& order) const
{
    return describe(m_area, order);
}

std::vector<EditorGroup*> Editors::restore_layout(std::string_view text)
{
    std::size_t at{};
    const auto tree{parse_layout(text, at)};
    if(tree == nullptr || at != text.size() || m_groups.size() != 1)
    {
        return {&m_groups.front()};
    }
    std::vector<EditorGroup*> numbered;
    build(*this, *tree, m_groups.front(), numbered);
    return numbered;
}

void Editors::move_line(const EditorGroup& group, Side side)
{
    const auto across{side == Side::left || side == Side::right};
    const auto wanted{across ? oui::SplitView::Orientation::horizontal : oui::SplitView::Orientation::vertical};
    for(auto* widget{group.tabs.parent()}; widget != nullptr; widget = widget->parent())
    {
        auto* const split{dynamic_cast<oui::SplitView*>(widget)};
        if(split != nullptr && split->child_count() == 2 && split->orientation() == wanted)
        {
            split->move_divider(side == Side::left || side == Side::top ? -1 : 1);
            return;
        }
    }
}

void Editors::set_active(EditorGroup& group)
{
    m_active = &group;
}
std::list<EditorGroup>& Editors::groups()
{
    return m_groups;
}
const std::list<EditorGroup>& Editors::groups() const
{
    return m_groups;
}
std::size_t Editors::count() const
{
    return m_groups.size();
}

EditorGroup& Editors::split(EditorGroup& group, Side side)
{
    auto* const parent{dynamic_cast<oui::SplitView*>(group.tabs.parent())};
    auto node{oui::split_view(side == Side::left || side == Side::right ? oui::SplitView::Orientation::horizontal
                                                                        : oui::SplitView::Orientation::vertical)};
    auto& node_reference{*node};
    auto old{parent->replace(group.tabs, std::move(node))};
    auto tabs{oui::tab_view()};
    auto& tabs_reference{*tabs};
    if(side == Side::left || side == Side::top)
    {
        node_reference.add(std::move(tabs));
        node_reference.add(std::move(old));
    }
    else
    {
        node_reference.add(std::move(old));
        node_reference.add(std::move(tabs));
    }
    auto& added{m_groups.emplace_back(EditorGroup{tabs_reference, {}})};
    for(const auto& setup : m_setups)
    {
        setup(tabs_reference);
    }
    return added;
}

void Editors::remove(EditorGroup& group)
{
    if(m_groups.size() < 2)
    {
        return;
    }
    auto* const node{dynamic_cast<oui::SplitView*>(group.tabs.parent())};
    auto* const above{dynamic_cast<oui::SplitView*>(node->parent())};
    auto sibling{node->remove_at(&node->child_at(0) == &group.tabs ? 1 : 0)};
    if(m_active == &group)
    {
        m_active = nullptr;
    }
    m_groups.remove_if([&group](const EditorGroup& each) { return &each == &group; });
    // The split, with the group's tabs, goes as the sibling takes its place.
    static_cast<void>(above->replace(*node, std::move(sibling)));
}

EditorGroup* Editors::neighbour(const EditorGroup& group, Side side)
{
    const auto from{group.tabs.bounds()};
    EditorGroup* best{};
    std::pair<int, int> best_score{};
    for(auto& other : m_groups)
    {
        if(&other == &group)
        {
            continue;
        }
        const auto to{other.tabs.bounds()};
        const auto across{side == Side::left || side == Side::right};
        // How far it is on that side, and how much of it runs alongside.
        const auto gap{side == Side::right    ? to.x - (from.x + from.width)
                       : side == Side::left   ? from.x - (to.x + to.width)
                       : side == Side::bottom ? to.y - (from.y + from.height)
                                              : from.y - (to.y + to.height)};
        const auto overlap{across ? std::min(from.y + from.height, to.y + to.height) - std::max(from.y, to.y)
                                  : std::min(from.x + from.width, to.x + to.width) - std::max(from.x, to.x)};
        if(gap < 0 || overlap <= 0)
        {
            continue;
        }
        const std::pair score{gap, -overlap};
        if(best == nullptr || score < best_score)
        {
            best = &other;
            best_score = score;
        }
    }
    return best;
}

std::string Editors::describe(const oui::Widget& widget, std::vector<const EditorGroup*>& order) const
{
    for(const auto& group : m_groups)
    {
        if(&group.tabs == &widget)
        {
            order.push_back(&group);
            return std::to_string(order.size() - 1);
        }
    }
    const auto& split{dynamic_cast<const oui::SplitView&>(widget)};
    if(split.child_count() == 1)
    {
        return describe(split.child_at(0), order);
    }
    auto text{std::string{split.orientation() == oui::SplitView::Orientation::horizontal ? "h" : "v"} +
              ratio_text(split.ratio()) + "("};
    text += describe(split.child_at(0), order) + ",";
    text += describe(split.child_at(1), order) + ")";
    return text;
}

void focus_current_tab(oui::Application& application, oui::TabView& tabs)
{
    // A calculator is written on at its line, an interactive tab in its code.
    if(auto* const calculator{tabs.selected_content<oui::CalculatorView>()})
    {
        application.focus(calculator->prompt());
    }
    else if(auto* const editor{document_editor(tabs, tabs.selected_index())})
    {
        application.focus(*editor);
    }
    else if(auto* const content{tabs.selected_content<oui::Widget>()}; content != nullptr && content->is_focusable())
    {
        application.focus(*content);
    }
}

void drop_lone_empty_tab(oui::TabView& tabs, std::vector<Document>& documents)
{
    if(tabs.child_count() == 1 && documents.size() == 1 && !documents[0].path.has_value() &&
       tabs.title(0) == oui::translate("Files", "Untitled"))
    {
        if(auto* const editor{document_editor(tabs, 0)})
        {
            if(editor->text().empty())
            {
                documents.clear();
                tabs.close_tab(0);
            }
        }
    }
}

void move_tab(EditorGroup& from, std::size_t index, EditorGroup& to)
{
    const std::string title{from.tabs.title(index)};
    auto content{from.tabs.take_tab(index)};
    auto document{std::move(from.documents.at(index))};
    from.documents.erase(from.documents.begin() + static_cast<std::ptrdiff_t>(index));
    to.tabs.set_visible(true);
    drop_lone_empty_tab(to.tabs, to.documents);
    to.documents.push_back(std::move(document));
    to.tabs.add_tab(title, std::move(content));
    to.tabs.select(to.tabs.child_count() - 1);
}

void reorder_tab(EditorGroup& group, std::size_t from, std::size_t to)
{
    if(from >= group.documents.size() || to >= group.documents.size() || from == to)
    {
        return;
    }
    group.tabs.move_tab(from, to);
    auto document{std::move(group.documents[from])};
    group.documents.erase(group.documents.begin() + static_cast<std::ptrdiff_t>(from));
    group.documents.insert(group.documents.begin() + static_cast<std::ptrdiff_t>(to), std::move(document));
}

void settle_group(Editors& editors, EditorGroup& group, oui::Settings& settings)
{
    if(group.tabs.child_count() > 0)
    {
        return;
    }
    if(editors.count() > 1)
    {
        editors.remove(group);
        return;
    }
    auto blank{make_editor()};
    apply_tab_settings(*blank, settings);
    group.tabs.add_tab(oui::translate("Files", "Untitled"), std::move(blank));
    group.documents.push_back({});
}

EditorGroup* any_neighbour(Editors& editors, const EditorGroup& group)
{
    for(const auto side : {Side::right, Side::left, Side::bottom, Side::top})
    {
        if(auto* const found{editors.neighbour(group, side)})
        {
            return found;
        }
    }
    return nullptr;
}

void open_task_list(oui::Application& application, Editors& editors, const std::filesystem::path& file,
                    EditorGroup* into)
{
    for(auto& group : editors.groups())
    {
        for(std::size_t index{}; index < group.tabs.child_count(); ++ index)
        {
            if(auto* const view{group.tabs.content_at<TasksView>(index)}; view != nullptr && view->file() == file)
            {
                group.tabs.select(index);
                view->focus_list();
                return;
            }
        }
    }
    auto view{tasks_view(application, file)};
    auto& view_reference{*view};
    auto& group{into != nullptr ? *into : editors.active()};
    drop_lone_empty_tab(group.tabs, group.documents);
    group.documents.push_back({});
    group.tabs.add_tab(view_reference.title(), std::move(view));
    group.tabs.select(group.tabs.child_count() - 1);
    view_reference.on_context_requested(
        [&editors, &view_reference](oui::Point at)
        {
            if(editors.open_context_menu)
            {
                editors.open_context_menu(view_reference.context_items(), at);
            }
        });
    // The title counts what is left to do, and names the file, wherever the tab has been moved.
    view_reference.on_changed(
        [&editors, &view_reference]
        {
            for(auto& each : editors.groups())
            {
                for(std::size_t index{}; index < each.tabs.child_count(); ++ index)
                {
                    if(each.tabs.content_at<TasksView>(index) == &view_reference)
                    {
                        each.tabs.set_title(index, view_reference.title());
                    }
                }
            }
        });
    view_reference.focus_list();
}

bool could_be_task_list(const std::filesystem::path& file)
{
    oui::FileError error;
    const auto loaded{oui::load_text_file(file, error)};
    return loaded && holds_only_tasks(loaded->content.text);
}

bool open_file_at_path(oui::Application& application, Editors& editors, oui::FolderView& explorer,
                       std::filesystem::path& current_folder, const std::filesystem::path& file,
                       oui::Settings& settings, EditorGroup* into, OpenAs as)
{
    // A task list opens as one, not as its text, unless its text is what is asked for.
    if(as == OpenAs::task_list || (as == OpenAs::natural && is_task_list_file(file)))
    {
        open_task_list(application, editors, file, into);
        return true;
    }
    for(auto& group : editors.groups())
    {
        for(std::size_t i{}; i < group.documents.size(); ++ i)
        {
            if(group.documents[i].path.has_value() && *group.documents[i].path == file)
            {
                group.tabs.select(i);
                if(auto* const editor{document_editor(group.tabs, i)})
                {
                    application.focus(*editor);
                }
                return true;
            }
        }
    }
    auto& [tabs, documents]{into != nullptr ? *into : editors.active()};
    oui::FileError error;
    const auto loaded{oui::load_text_file(file, error)};
    if(!loaded)
    {
        show_error(application, oui::translate("Files", "Open File"),
                   oui::format_text(oui::translate("Files", "Could not open {0}:\n{1}"),
                                    {oui::utf8_of(file), oui::describe(error)}));
        return false;
    }
    drop_lone_empty_tab(tabs, documents);
    auto document{make_editor()};
    apply_tab_settings(*document, settings);
    document->set_text(loaded->content.text);
    auto& document_reference{*document};
    documents.push_back(Document{file, loaded->content.format, loaded->stamp, nullptr, {}});
    tabs.add_tab(title_for(file), std::move(document));
    tabs.select(tabs.child_count() - 1);
    editors.apply_languages();
    application.focus(document_reference);
    current_folder = file.parent_path();
    explorer.refresh();
    return true;
}

void apply_split_choice(oui::Application& application, Editors& editors, oui::Settings& settings,
                        const SplitChoice& choice)
{
    auto& group{editors.active()};
    if(choice.kind == SplitChoice::Kind::join)
    {
        auto* const beside{any_neighbour(editors, group)};
        if(beside == nullptr)
        {
            return;
        }
        while(group.tabs.child_count() > 0)
        {
            move_tab(group, 0, *beside);
        }
        editors.remove(group);
        focus_current_tab(application, beside->tabs);
        return;
    }
    const auto side{choice.side};
    if(choice.kind == SplitChoice::Kind::focus || choice.kind == SplitChoice::Kind::move)
    {
        auto* const beside{editors.neighbour(group, side)};
        if(beside == nullptr)
        {
            return;
        }
        if(choice.kind == SplitChoice::Kind::move)
        {
            move_tab(group, group.tabs.selected_index(), *beside);
            settle_group(editors, group, settings);
        }
        focus_current_tab(application, beside->tabs);
        return;
    }
    auto& added{editors.split(group, side)};
    if(group.tabs.child_count() > 1)
    {
        move_tab(group, group.tabs.selected_index(), added);
    }
    else
    {
        auto blank{make_editor()};
        apply_tab_settings(*blank, settings);
        added.tabs.add_tab(oui::translate("Files", "Untitled"), std::move(blank));
        added.documents.push_back({});
    }
    focus_current_tab(application, added.tabs);
}

std::optional<SplitChoice> split_choice_of(const oui::KeyEvent& key, bool several)
{
    using oui::Key;
    const auto side{key.key == Key::left    ? std::optional{Side::left}
                    : key.key == Key::right ? std::optional{Side::right}
                    : key.key == Key::up    ? std::optional{Side::top}
                    : key.key == Key::down  ? std::optional{Side::bottom}
                                            : std::nullopt};
    if(side)
    {
        if(!several && (key.control || key.shift))
        {
            return std::nullopt;
        }
        return SplitChoice{key.control ? SplitChoice::Kind::focus
                           : key.shift ? SplitChoice::Kind::move
                                       : SplitChoice::Kind::split,
                           *side};
    }
    if(key.key == Key::backspace && several)
    {
        return SplitChoice{SplitChoice::Kind::join, Side::right};
    }
    return std::nullopt;
}
