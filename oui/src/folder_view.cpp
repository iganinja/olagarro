#include <oui/folder_view.h>
#include <oui/file_system.h>

#include <oui/label.h>
#include <oui/painter.h>
#include <oui/text_layout.h>
#include <oui/utf8.h>

#include <algorithm>

namespace olagarro::oui
{

namespace
{

// Whether `folder` is `scope` or lies somewhere inside it.
[[nodiscard]] bool inside(const std::filesystem::path& folder, const std::filesystem::path& scope)
{
    const auto relative{relative_to(normal_path(folder), normal_path(scope))};
    return !relative.empty() && utf8_of(*relative.begin()) != "..";
}

[[nodiscard]] std::string name_of(const std::filesystem::path& path)
{
    const auto name{utf8_of(path.filename())};
    return name.empty() ? utf8_of(path) : name;
}

// The text cut to `width` columns from the start, so the end of a long location stays in sight.
[[nodiscard]] std::string fit_from_end(const std::string& text, int width)
{
    if(text_width(text) <= width)
    {
        return text;
    }
    std::vector<std::size_t> starts;
    for(std::size_t index{}; index < text.size();)
    {
        starts.push_back(index);
        static_cast<void>(decode_utf8(text, index));
    }
    for(const auto start : starts)
    {
        const auto tail{text.substr(start)};
        if(text_width(tail) + 1 <= width)
        {
            return "~" + tail;
        }
    }
    return {};
}

} // namespace

FolderView::FolderView()
{
    set_colors(&Theme::panel_background, &Theme::panel_foreground);
    set_type_ahead(
        [this](std::size_t row) -> std::string
        {
            const auto offset{has_parent_row() ? 1u : 0u};
            if(row < offset)
            {
                return "..";
            }
            return row - offset < m_entries.size() ? name_of(m_entries[row - offset].path) : std::string{};
        });
    on_row_activated([this](std::size_t row) { activate(row); });
}

void FolderView::set_scope(std::optional<std::filesystem::path> folder)
{
    m_scope = folder ? std::optional<std::filesystem::path>{whole_path(*folder)} : std::nullopt;
    if(!m_current.empty())
    {
        show_folder(m_current);
    }
}

const std::optional<std::filesystem::path>& FolderView::scope() const
{
    return m_scope;
}

void FolderView::show_folder(const std::filesystem::path& folder)
{
    const auto whole{whole_path(folder)};
    m_current = m_scope && !inside(whole, *m_scope) ? *m_scope : whole;
    populate(std::nullopt);
}

const std::filesystem::path& FolderView::current_path() const
{
    return m_current;
}

void FolderView::refresh(const std::optional<std::filesystem::path>& select)
{
    populate(select ? select : selected_path());
}

std::optional<std::filesystem::path> FolderView::selected_path() const
{
    const auto row{selected_row()};
    const auto offset{has_parent_row() ? 1u : 0u};
    if(!row || *row < offset || *row - offset >= m_entries.size())
    {
        return std::nullopt;
    }
    return m_entries[*row - offset].path;
}

void FolderView::set_texts(FolderViewTexts texts)
{
    m_texts = std::move(texts);
    populate(selected_path());
}

bool FolderView::can_go_up() const
{
    return !m_current.empty() && m_current.has_parent_path() && m_current.parent_path() != m_current &&
           (!m_scope || m_current != *m_scope);
}

bool FolderView::has_parent_row() const
{
    return can_go_up();
}

const std::string& FolderView::name() const
{
    return m_scope ? m_texts.project : m_texts.explorer;
}

std::string FolderView::header_label() const
{
    return name() + ": ";
}

std::string FolderView::header_location() const
{
    if(!m_scope)
    {
        return utf8_of(m_current);
    }
    auto location{name_of(*m_scope)};
    for(const auto& part : relative_to(m_current, *m_scope))
    {
        if(utf8_of(part) != ".")
        {
            location += " › " + utf8_of(part);
        }
    }
    return location;
}

void FolderView::go_up()
{
    if(!can_go_up())
    {
        return;
    }
    const auto previous{m_current};
    m_current = m_current.parent_path();
    // Back in the parent, the folder just left stays selected.
    populate(previous);
}

void FolderView::activate(std::size_t row)
{
    const auto offset{has_parent_row() ? 1u : 0u};
    if(row < offset)
    {
        go_up();
        return;
    }
    if(row - offset >= m_entries.size())
    {
        return;
    }
    const auto entry{m_entries[row - offset]};
    if(entry.is_directory)
    {
        m_current = entry.path;
        populate(std::nullopt);
        return;
    }
    m_activated.fire(entry.path);
}

void FolderView::populate(const std::optional<std::filesystem::path>& select)
{
    clear();
    m_entries.clear();
    if(m_current.empty())
    {
        set_headers_visible(false);
        return;
    }
    set_headers({header_label() + header_location()});
    if(can_go_up())
    {
        add_item("").set_marker("..", 2, Icon::parent_folder);
    }
    m_entries = directory_entries(m_current);
    std::optional<std::size_t> selected;
    for(const auto& entry : m_entries)
    {
        if(select && entry.path == *select)
        {
            selected = row_count();
        }
        // Folders are marked; files take as wide a blank, so the names line up.
        add_item(name_of(entry.path))
            .set_marker(entry.is_directory ? "+ " : "", 2, entry.is_directory ? Icon::folder : Icon::file);
    }
    if(row_count() != 0)
    {
        select_row(selected.value_or(0));
    }
}

bool FolderView::claims_key(const Event& event) const
{
    const auto* const key{std::get_if<KeyEvent>(&event)};
    if(key == nullptr || key->control || key->alt || key->shift)
    {
        return false;
    }
    switch(key->key)
    {
    case Key::f2:
    case Key::f5:
    case Key::f7:
    case Key::insert_:
    case Key::delete_:
        return true;
    default:
        return false;
    }
}

bool FolderView::handle_event(const Event& event)
{
    if(const auto* const mouse{std::get_if<MouseEvent>(&event)}; mouse != nullptr && mouse->wheel_delta == 0)
    {
        // A right click selects what it falls on and asks for the menu of what can be done there.
        if(mouse->pressed && mouse->right)
        {
            auto left{*mouse};
            left.right = false;
            left.clicks = 1;
            static_cast<void>(List::handle_event(left));
            m_pressed_row.reset();
            m_context_requested.fire(Point{mouse->x, mouse->y});
            return true;
        }
        // One click opens, as in most side panels: the press selects, and the release on the same row opens.
        if(mouse->pressed)
        {
            auto single{*mouse};
            single.clicks = 1;
            const auto handled{List::handle_event(single)};
            m_pressed_row = handled ? selected_row() : std::nullopt;
            return handled;
        }
        if(mouse->released)
        {
            const auto pressed{m_pressed_row};
            m_pressed_row.reset();
            if(pressed && pressed == selected_row())
            {
                activate(*pressed);
            }
            return pressed.has_value();
        }
    }
    if(const auto* const key{std::get_if<KeyEvent>(&event)}; key != nullptr && !m_current.empty())
    {
        switch(key->key)
        {
        case Key::backspace:
            if(!can_go_up())
            {
                return false;
            }
            go_up();
            return true;
        case Key::f2:
            if(const auto path{selected_path()})
            {
                m_rename_requested.fire(*path);
            }
            return true;
        case Key::delete_:
            if(const auto path{selected_path()})
            {
                m_delete_requested.fire(*path);
            }
            return true;
        case Key::f5:
            if(const auto path{selected_path()})
            {
                m_copy_requested.fire(*path);
            }
            return true;
        case Key::insert_:
            m_create_requested.fire(m_current, false);
            return true;
        case Key::f7:
            m_create_requested.fire(m_current, true);
            return true;
        default:
            break;
        }
    }
    return List::handle_event(event);
}

void FolderView::paint(Painter& painter, const Theme& theme) const
{
    List::paint(painter, theme);
    if(m_current.empty())
    {
        const auto x{bounds().x + std::max(0, (bounds().width - text_width(m_texts.empty)) / 2)};
        const auto y{bounds().y + std::max(0, bounds().height / 2 - 1)};
        painter.draw_label({x, y}, m_texts.empty, {theme.panel_foreground, background_color(theme)}, Align::center);
        return;
    }
    // The header says where the panel is, and whether it is held to a project: outside one it stands out.
    // With the focus the header takes the focus colours, so it is plain which panel keys act on.
    const auto focused{is_focused()};
    const auto header_background{focused ? theme.focus_background : theme.table_header_background};
    painter.fill_rect({bounds().x, bounds().y, bounds().width, 1}, header_background);
    // The label stays whole; a long location loses its start, as its end says where the panel is.
    const auto label{header_label()};
    const TextStyle style{focused   ? theme.focus_text
                          : m_scope ? theme.table_header_foreground
                                    : theme.warning_foreground,
                          header_background};
    painter.draw_label({bounds().x, bounds().y}, label, style);
    const auto label_width{shown_width(label)};
    painter.draw_label({bounds().x + label_width, bounds().y},
                       fit_from_end(header_location(), std::max(0, bounds().width - label_width)), style);
}

std::unique_ptr<FolderView> folder_view()
{
    return std::make_unique<FolderView>();
}

FolderView& folder_view(Container& parent)
{
    return parent.add(folder_view());
}

} // namespace olagarro::oui
