#include <oui/button.h>
#include <oui/combo_box.h>
#include <oui/dialogs.h>
#include <oui/file_dialog.h>
#include <oui/file_system.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/spacer.h>
#include <oui/table.h>
#include <oui/text_metrics.h>
#include <oui/text_layout.h>
#include <oui/tree_view.h>
#include <oui/utf8.h>
#include <oui/window.h>

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <string>

namespace olagarro::oui
{

namespace
{

constexpr int VISIBLE_FILE_ROWS{10};

[[nodiscard]] int text_columns(std::string_view text)
{
    auto columns{0};
    for(std::size_t index{}; index < text.size();)
    {
        columns += std::max(0, terminal_cell_width(decode_utf8(text, index)));
    }
    return columns;
}

std::string format_size(std::uintmax_t size)
{
    if(size < 1024)
    {
        return std::to_string(size) + " B";
    }
    if(size < 1024 * 1024)
    {
        return std::to_string(size / 1024) + " KB";
    }
    if(size < 1024ull * 1024 * 1024)
    {
        return std::to_string(size / (1024 * 1024)) + " MB";
    }
    return std::to_string(size / (1024ull * 1024 * 1024)) + " GB";
}

std::string extension_lower(const std::filesystem::path& path)
{
    auto ext{utf8_of(path.extension())};
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

std::string trim_unquoted(const std::string& text)
{
    const auto first{text.find_first_not_of(" \t\"")};
    if(first == std::string::npos)
    {
        return {};
    }
    const auto last{text.find_last_not_of(" \t\"")};
    return text.substr(first, last - first + 1);
}

bool matches_filter(const std::string& name, const std::string& filter)
{
    if(filter.empty() || filter == "*" || filter == "*.*")
    {
        return true;
    }
    const auto extension{extension_lower(path_from_utf8(name))};
    std::size_t start{};
    while(start <= filter.size())
    {
        auto end{filter.find(';', start)};
        if(end == std::string::npos)
        {
            end = filter.size();
        }
        std::string token{filter.substr(start, end - start)};
        std::transform(token.begin(), token.end(), token.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if(!token.empty())
        {
            if(token == "*")
            {
                return true;
            }
            auto token_extension{token};
            if(token_extension.compare(0, 2, "*.") == 0)
            {
                token_extension.erase(0, 1);
            }
            else if(token_extension.empty() || token_extension.front() != '.')
            {
                token_extension.insert(0, ".");
            }
            if(extension == token_extension)
            {
                return true;
            }
        }
        if(end == filter.size())
        {
            break;
        }
        start = end + 1;
    }
    return false;
}

struct FileEntry
{
    std::string name;
    // None for a folder, and for what has no size to give, as a named pipe.
    std::optional<std::uintmax_t> size;
    std::string type;
    bool is_directory{false};
    std::filesystem::path full_path;
};

class FileListWidget final : public Table
{
  public:
    using ActivateCallback = std::function<void(const std::filesystem::path&)>;
    using SelectCallback = std::function<void(const std::filesystem::path&)>;
    using NavigateParentCallback = std::function<void()>;

    explicit FileListWidget(const FileDialogTexts& texts) : m_texts{texts}
    {
        on_row_activated(
            [this](std::size_t row)
            {
                if(m_on_activate)
                {
                    m_on_activate(entry_at(row).full_path);
                }
            });
        // Typing finds an entry by its name, without the marks the rows show.
        set_type_ahead([this](std::size_t row) { return entry_at(row).name; });
        // The name takes the width the size and type leave.
        set_stretch_column(0);
        // The headers sort the entries their own way: ".." first, then folders, then files.
        set_headers_clickable(true);
        set_sorted_by_owner(true);
        set_columns_resizable(true);
        on_column_click([this](std::size_t column, SortOrder)
                        { sort_by(static_cast<SortColumn>(std::min(column, file_column_count - 1))); });
    }

    void set_entries(std::vector<FileEntry>* entries)
    {
        m_entries = entries;
        rebuild_rows();
        if(!m_entries->empty())
        {
            Table::select_row(0);
        }
    }

    void set_on_activate(ActivateCallback callback) { m_on_activate = std::move(callback); }
    void set_on_select(SelectCallback callback) { m_on_select = std::move(callback); }
    void set_on_navigate_parent(NavigateParentCallback callback) { m_on_navigate_parent = std::move(callback); }

    // Every choice of a row, by keys, clicks or typing, comes through here.
    void select_row(std::size_t row) override
    {
        Table::select_row(row);
        if(m_on_select && row < m_order.size())
        {
            m_on_select(entry_at(row).full_path);
        }
    }

    [[nodiscard]] bool is_focusable() const override { return true; }

    [[nodiscard]] Widget* hit_test(int x, int y) override
    {
        return x >= bounds().x && x < bounds().x + bounds().width && y >= bounds().y && y < bounds().y + bounds().height
                   ? this
                   : nullptr;
    }

    bool handle_event(const Event& event) override
    {
        if(!m_entries || m_entries->empty())
        {
            return false;
        }
        if(const auto* const key{std::get_if<KeyEvent>(&event)}; key != nullptr && key->key == Key::backspace)
        {
            if(m_on_navigate_parent)
            {
                m_on_navigate_parent();
            }
            return true;
        }
        return Table::handle_event(event);
    }

    [[nodiscard]] Size measure(const Constraints& constraints) const override
    {
        const int h{std::max(VISIBLE_FILE_ROWS, std::min(constraints.max_height, 22))};
        return constraints.constrain({constraints.max_width, h});
    }

  private:
    static constexpr std::size_t file_column_count{3};

    enum class SortColumn : std::size_t
    {
        name,
        size,
        type
    };

    [[nodiscard]] const FileEntry& entry_at(std::size_t row) const { return m_entries->at(m_order.at(row)); }

    [[nodiscard]] static std::string lowercase(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return text;
    }

    // ".." stays on top and folders before files whatever the order; within each group the chosen column
    // decides, and the name breaks ties in ascending order so equal sizes or types stay readable.
    void sort_rows()
    {
        m_order.resize(m_entries->size());
        for(std::size_t index{}; index < m_order.size(); ++ index)
        {
            m_order.at(index) = index;
        }
        const auto group{[](const FileEntry& entry) { return entry.name == ".." ? 0 : entry.is_directory ? 1 : 2; }};
        const auto compare_names{[](const FileEntry& first, const FileEntry& second)
                                 {
                                     const auto first_name{lowercase(first.name)};
                                     const auto second_name{lowercase(second.name)};
                                     if(first_name != second_name)
                                     {
                                         return first_name < second_name ? -1 : 1;
                                     }
                                     return first.name < second.name ? -1 : first.name == second.name ? 0 : 1;
                                 }};
        std::stable_sort(m_order.begin(), m_order.end(),
                         [this, &group, &compare_names](std::size_t first_index, std::size_t second_index)
                         {
                             const auto& first{m_entries->at(first_index)};
                             const auto& second{m_entries->at(second_index)};
                             if(group(first) != group(second))
                             {
                                 return group(first) < group(second);
                             }
                             auto order{0};
                             if(m_sort_column == SortColumn::size && first.size != second.size)
                             {
                                 order = first.size < second.size ? -1 : 1;
                             }
                             else if(m_sort_column == SortColumn::type)
                             {
                                 const auto first_type{lowercase(first.type)};
                                 const auto second_type{lowercase(second.type)};
                                 order = first_type == second_type ? 0 : first_type < second_type ? -1 : 1;
                             }
                             else if(m_sort_column == SortColumn::name)
                             {
                                 order = compare_names(first, second);
                             }
                             if(order != 0)
                             {
                                 return m_sort_descending ? order > 0 : order < 0;
                             }
                             return compare_names(first, second) < 0;
                         });
    }

    void rebuild_rows()
    {
        sort_rows();
        set_column_width(0, 35);
        set_column_width(1, 9);
        set_column_width(2, 9);
        set_headers({m_texts.name_column, m_texts.size_column, m_texts.type_column});
        set_horizontal_scroll_enabled(false);
        clear();
        for(std::size_t column{}; column < file_column_count; ++ column)
        {
            const auto sorted{static_cast<SortColumn>(column) == m_sort_column};
            set_column_sort(column, !sorted             ? SortOrder::none
                                    : m_sort_descending ? SortOrder::descending
                                                        : SortOrder::ascending);
        }
        for(std::size_t row{}; row < m_order.size(); ++ row)
        {
            const auto& entry{entry_at(row)};
            const auto marker_columns{text_width(m_texts.folder_marker)};
            const std::string size_text{entry.size ? format_size(*entry.size) : "       "};
            const std::string type_text{entry.is_directory ? "       " : entry.type};
            add_line({entry.name == ".." ? std::string_view{} : std::string_view{entry.name}, size_text, type_text});
            // The name says what the entry is by its mark: a folder's, or as wide a blank, so the names line up.
            auto& name{dynamic_cast<Label&>(child_at(child_count() - 3))};
            if(entry.name == "..")
            {
                name.set_marker(m_texts.parent_folder, text_width(m_texts.parent_folder), Icon::parent_folder);
            }
            else
            {
                name.set_marker(entry.is_directory ? m_texts.folder_marker : "", marker_columns,
                                entry.is_directory ? Icon::folder : Icon::file);
            }
        }
    }

    // Clicking the sorted column again reverses it; the selected entry stays selected after the reorder.
    void sort_by(SortColumn column)
    {
        m_sort_descending = column == m_sort_column && !m_sort_descending;
        m_sort_column = column;
        const auto row{selected_row()};
        const auto selected{m_order.empty() || !row ? std::nullopt : std::optional<std::size_t>{m_order.at(*row)}};
        rebuild_rows();
        if(selected)
        {
            Table::select_row(static_cast<std::size_t>(
                std::distance(m_order.begin(), std::find(m_order.begin(), m_order.end(), *selected))));
        }
    }

    const FileDialogTexts& m_texts;
    const std::vector<FileEntry>* m_entries{};
    // Display order: row -> index into m_entries.
    std::vector<std::size_t> m_order;
    SortColumn m_sort_column{SortColumn::name};
    bool m_sort_descending{false};
    ActivateCallback m_on_activate;
    SelectCallback m_on_select;
    NavigateParentCallback m_on_navigate_parent;
};

// The entries of `folder`: ".." first, then folders, then the files `patterns` matches; only folders in
// folder mode.
std::vector<FileEntry> list_folder(const std::filesystem::path& folder, const std::string& patterns, bool folders_only)
{
    std::vector<FileEntry> entries;
    if(folder.has_parent_path() && folder.parent_path() != folder)
    {
        entries.push_back({"..", std::nullopt, "", true, folder.parent_path()});
    }
    for(const auto& found : directory_entries(folder))
    {
        if(!found.is_directory && (folders_only || !matches_filter(utf8_of(found.path.filename()), patterns)))
        {
            continue;
        }
        FileEntry entry;
        entry.name = utf8_of(found.path.filename());
        entry.is_directory = found.is_directory;
        entry.full_path = found.path;
        if(!found.is_directory)
        {
            std::error_code unknown;
            if(const auto size{std::filesystem::file_size(found.path, unknown)}; !unknown)
            {
                entry.size = size;
            }
            entry.type = extension_lower(found.path);
            if(entry.type.size() > 1)
            {
                entry.type = entry.type.substr(1);
            }
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

// A place in the side pane: a label and where it leads; an empty path is a blank separator row.
struct Place
{
    std::string label;
    std::filesystem::path path;
};

std::vector<Place> standard_places(const FileDialogTexts& texts)
{
    std::vector<Place> places;
    const auto add_known{[&places](const std::string& label, const std::string& folder)
                         {
                             const auto path{known_folder(folder)};
                             std::error_code ignored;
                             if(!path.empty() && std::filesystem::is_directory(path, ignored))
                             {
                                 places.push_back({label, path});
                             }
                         }};
    add_known(texts.home, "home");
    add_known(texts.desktop, "desktop");
    add_known(texts.documents, "documents");
    add_known(texts.downloads, "downloads");
    for(const auto& root : root_directories())
    {
        places.push_back({utf8_of(root), root});
    }
    return places;
}

} // namespace

std::optional<std::filesystem::path> file_dialog(Application& application, const FileDialogOptions& options)
{
    const auto& texts{options.texts};
    const auto folders_only{options.mode == FileDialogMode::open_folder};
    const auto saving{options.mode == FileDialogMode::save_file};
    auto filters{options.filters};
    if(filters.empty())
    {
        filters.push_back({texts.all_files, "*"});
    }
    const auto first_filter{std::min(options.selected_filter, filters.size() - 1)};

    // Everything the dialog's callbacks share. The filter lives here, so every way of changing folder
    // keeps listing with the filter the user picked last.
    struct State
    {
        std::filesystem::path current;
        std::vector<FileEntry> entries;
        std::string patterns;
        bool overwrite_armed{false};
        std::optional<std::filesystem::path> result;
    };
    State state;
    std::error_code ignored;
    state.current = options.directory.empty() ? std::filesystem::current_path(ignored) : options.directory;
    state.patterns = filters.at(first_filter).patterns;

    const auto title{!options.title.empty() ? options.title
                     : folders_only         ? texts.open_folder_title
                     : saving               ? texts.save_title
                                            : texts.open_file_title};
    auto dialog{window(title)};
    auto& dialog_reference{*dialog};
    if(!options.placement_key.empty())
    {
        dialog->set_placement_key(options.placement_key);
    }
    dialog->set_maximum_size(options.preferred_size);
    dialog->set_preferred_size(options.preferred_size);
    dialog->set_minimum_size(options.minimum_size);
    dialog->set_resizable(true);

    const auto places{standard_places(texts)};
    auto places_width{std::max(12, text_columns(texts.places) + 2)};
    for(const auto& place : places)
    {
        places_width = std::max(places_width, text_columns(place.label) + 4);
    }
    const auto label_width{
        std::max(shown_width(strip_mnemonic(texts.file_name)), shown_width(strip_mnemonic(texts.file_type)))};

    auto content{grid()};
    content->set_columns({Length::flex()});
    content->set_rows({Length::content(), Length::cells(1), Length::flex(), Length::cells(1), Length::content()});
    content->set_gap(Spacing::none);

    auto& location_row{content->add(grid())};
    location_row.set_columns({Length::cells(places_width), Length::flex()});
    // Parted as the places and the files below, so the field lines up with the files.
    location_row.set_gap(text_metrics().panel_gap() > 0 ? Spacing::tight : Spacing::none);
    // Down the left side, as the labels at the bottom are.
    auto& location_label{location_row.add(label(texts.location))};
    auto& location_edit{location_row.add(line_edit())};
    location_label.set_buddy(location_edit);
    content->add(vspacer(1));

    auto& main_row{content->add(grid())};
    main_row.set_columns({Length::cells(places_width), Length::flex()});
    // The places and the files take the whole height the dialog gives the row, however tall it is resized.
    main_row.set_rows({Length::flex()});
    main_row.set_gap(text_metrics().panel_gap() > 0 ? Spacing::tight : Spacing::none);
    auto& places_view{main_row.add(tree_view())};
    places_view.set_show_expanders(false);
    // Its title in the row the files' column headers take, the places under it.
    places_view.set_top_padding(1);
    places_view.set_header(texts.places);
    for(const auto& place : places)
    {
        places_view.add(place.label);
    }
    auto& file_list{main_row.add(std::make_unique<FileListWidget>(texts))};
    content->add(vspacer(1));

    auto& footer{content->add(grid())};
    footer.set_columns({Length::flex()});
    footer.set_gap(Spacing::none);
    auto& name_row{footer.add(grid())};
    name_row.set_columns({Length::cells(label_width), Length::fill()});
    name_row.set_gap(Spacing::tight);
    auto& name_label{name_row.add(label(texts.file_name))};
    auto& name_edit{name_row.add(line_edit())};
    name_label.set_buddy(name_edit);
    name_row.set_visible(!folders_only);
    if(saving)
    {
        // A name ready to save under, its part before the extension selected so typing replaces just that.
        auto name{options.file_name};
        if(name.empty())
        {
            name = options.new_file_extension.empty()
                       ? texts.new_file_name
                       : texts.new_file_name.substr(0, texts.new_file_name.rfind('.')) + options.new_file_extension;
        }
        name_edit.set_text(name);
        const auto dot{name.rfind('.')};
        name_edit.select_range(0, dot == std::string::npos || dot == 0 ? name.size() : dot);
    }
    ComboBox* filter_box{};
    if(!folders_only)
    {
        auto& type_row{footer.add(grid())};
        type_row.set_columns({Length::cells(label_width), Length::fill()});
        type_row.set_gap(Spacing::tight);
        auto& type_label{type_row.add(label(texts.file_type))};
        std::vector<std::string> filter_names;
        for(const auto& filter : filters)
        {
            filter_names.push_back(filter.name);
        }
        filter_box = &type_row.add(combo_box(std::move(filter_names), first_filter));
        type_label.set_buddy(*filter_box);
    }
    auto& hint{footer.add(label(""))};
    hint.set_color(&Theme::error_foreground);
    auto& buttons_row{footer.add(grid())};
    buttons_row.set_columns({Length::fill(), Length::content()});
    buttons_row.set_gap(Spacing::none);
    // On the left, where saving or choosing a folder, a button that makes a folder; filled once the folder shown can be
    // changed.
    auto& making{row(buttons_row)};
    auto& buttons{row(buttons_row)};
    buttons.set_gap(Spacing::normal);

    const auto accept_text{folders_only ? texts.select_folder : saving ? texts.save : texts.open};
    Button* accept_button{};
    const auto disarm{[&state, &accept_button, &hint, accept_text]
                      {
                          if(state.overwrite_armed && accept_button != nullptr)
                          {
                              accept_button->set_text(accept_text);
                          }
                          state.overwrite_armed = false;
                          hint.set_text("");
                      }};
    // `folder` is a copy: the path usually comes from an entry, and showing the folder replaces the entries.
    const auto show{[&state, &file_list, &location_edit, &disarm, folders_only](std::filesystem::path folder)
                    {
                        std::error_code ignored;
                        if(!std::filesystem::is_directory(folder, ignored))
                        {
                            return;
                        }
                        // Whole, so a folder given as ".." still has a parent to go up to.
                        folder = whole_path(folder);
                        state.current = folder;
                        state.entries = list_folder(folder, state.patterns, folders_only);
                        file_list.set_entries(&state.entries);
                        location_edit.set_text(utf8_of(folder));
                        disarm();
                    }};
    const auto accept{
        [&state, &dialog_reference, &name_edit, &hint, &accept_button, &options, &texts, folders_only, saving]
        {
            if(folders_only)
            {
                state.result = state.current;
                dialog_reference.close(DialogResult::accepted);
                return;
            }
            const auto name{trim_unquoted(std::string{name_edit.text()})};
            if(name.empty())
            {
                hint.set_text(texts.enter_a_name);
                return;
            }
            const auto target{state.current / path_from_utf8(name)};
            std::error_code ignored;
            if(!saving && !std::filesystem::is_regular_file(target, ignored))
            {
                hint.set_text(format_text(texts.file_not_found, {name}));
                return;
            }
            if(saving && options.confirm_overwrite && !state.overwrite_armed &&
               std::filesystem::exists(target, ignored))
            {
                state.overwrite_armed = true;
                if(accept_button != nullptr)
                {
                    accept_button->set_text(texts.overwrite);
                }
                hint.set_text(texts.overwrite_hint);
                return;
            }
            state.result = target;
            dialog_reference.close(DialogResult::accepted);
        }};
    accept_button = &button(accept_text, buttons, [&accept] { accept(); });
    accept_button->set_default(true);
    button(texts.cancel, buttons, [&dialog_reference] { dialog_reference.close(DialogResult::cancelled); });
    // New Folder asks its name, makes it where the dialog is, and goes into it, so what is saved goes there.
    if(saving || folders_only)
    {
        button(texts.new_folder, making,
               [&application, &state, &hint, &texts, &show, &name_edit, &file_list, saving]
               {
                   PromptOptions asking;
                   asking.title = texts.new_folder_title;
                   asking.label = texts.folder_name;
                   const auto name{prompt(application, asking)};
                   if(!name || trim_unquoted(*name).empty())
                   {
                       return;
                   }
                   const auto folder{state.current / path_from_utf8(trim_unquoted(*name))};
                   std::error_code error;
                   std::filesystem::create_directory(folder, error);
                   if(error)
                   {
                       hint.set_text(format_text(texts.folder_not_made, {trim_unquoted(*name), error.message()}));
                       return;
                   }
                   show(folder);
                   // On to what comes next there: the file's name to save under, or the folders in it.
                   application.focus(saving ? static_cast<Widget&>(name_edit) : static_cast<Widget&>(file_list));
               });
    }
    dialog->add(std::move(content));

    places_view.on_activated(
        [&places, &show](std::size_t index)
        {
            if(index < places.size() && !places.at(index).path.empty())
            {
                show(places.at(index).path);
            }
        });
    if(filter_box != nullptr)
    {
        static_cast<void>(filter_box->on_selected(
            [&state, &filters, &show](std::size_t index)
            {
                state.patterns = filters.at(index).patterns;
                show(state.current);
            }));
    }
    static_cast<void>(name_edit.on_submitted([&accept](std::string_view) { accept(); }));
    static_cast<void>(name_edit.on_changed([&disarm](std::string_view) { disarm(); }));
    static_cast<void>(location_edit.on_submitted(
        [&location_edit, &hint, &texts, &show](std::string_view)
        {
            const auto text{trim_unquoted(std::string{location_edit.text()})};
            std::error_code ignored;
            if(text.empty())
            {
                return;
            }
            if(!std::filesystem::is_directory(path_from_utf8(text), ignored))
            {
                hint.set_text(format_text(texts.folder_not_found, {text}));
                return;
            }
            show(path_from_utf8(text));
        }));
    file_list.set_on_select(
        [&name_edit, folders_only](const std::filesystem::path& path)
        {
            std::error_code ignored;
            if(!folders_only && std::filesystem::is_regular_file(path, ignored))
            {
                name_edit.set_text(utf8_of(path.filename()));
            }
        });
    file_list.set_on_activate(
        [&name_edit, &accept, &show, saving](const std::filesystem::path& path)
        {
            std::error_code ignored;
            if(std::filesystem::is_directory(path, ignored))
            {
                show(path);
                return;
            }
            name_edit.set_text(utf8_of(path.filename()));
            if(!saving)
            {
                accept();
            }
        });
    file_list.set_on_navigate_parent(
        [&state, &show]
        {
            if(state.current.has_parent_path() && state.current.parent_path() != state.current)
            {
                show(state.current.parent_path());
            }
        });

    show(state.current);
    // Focus moves once the dialog is showing: show_modal starts from its first focusable widget.
    Widget& first_focus{saving ? static_cast<Widget&>(name_edit) : static_cast<Widget&>(file_list)};
    application.post([&application, &first_focus] { application.focus(first_focus); });
    static_cast<void>(application.show_modal(std::move(dialog)));
    return state.result;
}

} // namespace olagarro::oui
