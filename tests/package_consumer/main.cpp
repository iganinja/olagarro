#include <tasks/execute.h>
#include <tasks/serial.h>
#include <tasks/taskexecutor.h>
#include <bytestream/bytestream.h>
#include <concurrency/concurrency.h>
#include <oui/layout.h>
#include <oui/script/script_engine.h>
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

    oui::script::ScriptEngine engine;
    engine.run("const answer = 6 * 7;", "consumer.js");

    return value == 2 && layout->child_count() == 0 && stream.read<int>() == 7 && fired == 3 && values[2] == 6 ? 0 : 1;
}
