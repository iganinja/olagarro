#include <oui/command_palette.h>

#include <oui/command.h>
#include <oui/quick_pick.h>

#include <vector>

namespace olagarro::oui
{

void show_command_palette(Application& application, const CommandPaletteOptions& options)
{
    const auto commands{application.commands().commands()};
    std::vector<QuickPickItem> items;
    for(const auto& command : commands)
    {
        QuickPickItem item{strip_mnemonic(command.title), {}};
        if(command.shortcut)
        {
            item.detail = format_shortcut(*command.shortcut);
        }
        if(command.checked)
        {
            item.detail += (item.detail.empty() ? "" : "  ") + (*command.checked ? options.checked : options.unchecked);
        }
        items.push_back(std::move(item));
    }
    QuickPickOptions pick_options;
    pick_options.title = options.title;
    pick_options.placeholder = options.placeholder;
    pick_options.height_share = options.height_share;
    pick_options.placement_key = options.placement_key;
    if(const auto picked{quick_pick(application, items, pick_options)})
    {
        static_cast<void>(application.commands().execute(commands.at(*picked).id));
    }
}

} // namespace olagarro::oui
