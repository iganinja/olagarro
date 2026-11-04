#include <tasks/execute.h>
#include <tasks/serial.h>
#include <tasks/taskexecutor.h>
#include <bytestream/bytestream.h>
#include <concurrency/concurrency.h>
#include <effects/image.h>
#include <effects/sound.h>
#include <oui/layout.h>
#include <oui/script/script_engine.h>
#include <paint/canvas.h>
#include <paint/panels/tool_panel.h>
#include <signalslot/signalslot.h>

#include <vector>

using namespace olagarro;

int main()
{
    auto value{0};

    tasks::TaskExecutor executor;
    executor.add_task(tasks::serial(tasks::execute([&value] { value = 1; }), tasks::execute([&value] { value = 2; })));
    executor.update(0.016f);

    auto layout{oui::row()};

    ByteStream stream;
    stream.write(7);

    auto fired{0};
    signal_slot::Signal<int> signal;
    const signal_slot::ScopedConnection connection{signal.connect([&fired](int amount) { fired += amount; })};
    signal.fire(3);

    std::vector<int> values{1, 2, 3};
    concurrency::concurrent_for(values.begin(), values.end(), [](std::size_t, int& each) { each *= 2; }).get();

    const effects::Image image{2, 2, std::vector<float>(16, 1.0f)};
    const auto blurred{effects::gaussian_blur(concurrency::TaskPool::shared(), image, 1.0)};
    const auto quieter{effects::gain(effects::Sound{48000, std::size_t{4}}, -6.0)};

    const std::vector<std::uint8_t> red{255, 0, 0, 255};
    const auto canvas{paint::from_rgba8(1, 1, red.data())};
    paint::Icons icons;
    paint::ToolPanel tools{icons};
    tools.press_key(U'b');

    oui::script::ScriptEngine engine;
    engine.run("const answer = 6 * 7;", "consumer.js");

    return value == 2 && layout->child_count() == 0 && stream.read<int>() == 7 && fired == 3 && values[2] == 6 &&
                   blurred.width == 2 && quieter.frames() == 4 && paint::to_rgba8(canvas).bytes == red &&
                   tools.selected_tool() == paint::Tool::brush
               ? 0
               : 1;
}
