import olagarro;

int main()
{
    const olagarro::oui::Size size{80, 25};
    const olagarro::ByteStream stream;
    const olagarro::oui::Theme theme{};
    const olagarro::oui::ColorRef panel{&olagarro::oui::Theme::panel_background};
    const olagarro::oui::FileDialogOptions options{};
    return size.width == 80 && size.height == 25 && stream.size() == 0u &&
                   panel.resolve(theme) == theme.panel_background &&
                   options.mode == olagarro::oui::FileDialogMode::open_file &&
                   olagarro::oui::translate("Dialogs", "OK").size() == 2u
               ? 0
               : 1;
}
