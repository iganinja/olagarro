#include <paint/tool.h>

#include <utility>

namespace olagarro::paint
{

namespace
{

const std::vector<std::pair<Tool, std::string_view>>& tool_ids()
{
    static const std::vector<std::pair<Tool, std::string_view>> ids{
        {Tool::rectangle_select, "rectangle_select"},
        {Tool::move_pixels, "move_pixels"},
        {Tool::lasso_select, "lasso_select"},
        {Tool::move_selection, "move_selection"},
        {Tool::ellipse_select, "ellipse_select"},
        {Tool::magic_wand, "magic_wand"},
        {Tool::fill, "bucket"},
        {Tool::gradient, "gradient"},
        {Tool::brush, "brush"},
        {Tool::eraser, "eraser"},
        {Tool::pencil, "pencil"},
        {Tool::color_picker, "color_picker"},
        {Tool::line, "line"},
        {Tool::shapes, "shapes"},
        {Tool::arc, "arc"},
    };
    return ids;
}

} // namespace

const std::vector<Tool>& all_tools()
{
    static const auto tools{[]
                            {
                                std::vector<Tool> result;
                                for(const auto& [tool, id] : tool_ids())
                                {
                                    result.push_back(tool);
                                }
                                return result;
                            }()};
    return tools;
}

std::string_view tool_id(Tool tool)
{
    for(const auto& [each, id] : tool_ids())
    {
        if(each == tool)
        {
            return id;
        }
    }
    return {};
}

std::optional<Tool> tool_from_id(std::string_view id)
{
    for(const auto& [tool, each] : tool_ids())
    {
        if(each == id)
        {
            return tool;
        }
    }
    return std::nullopt;
}

} // namespace olagarro::paint
