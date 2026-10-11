#include "tasks/task.h"
#include <stack>

namespace olagarro
{
namespace tasks
{

namespace
{

std::string n_times_tabs(int times)
{
    std::string result{""};

    for(auto i{0}; i < times; ++ i)
    {
        result += "  ";
    }

    return result;
}

} // namespace

Task::Task(const std::string& name)
#ifdef TASKS_USE_NAMES
    : m_name{name}
#endif
{
}

#ifdef TASKS_USE_NAMES
const std::string& Task::name() const
{
    return m_name;
}

std::string Task::tree() const
{
    struct TreeEntry
    {
        const Task* task;
        int level;
    };

    std::stack<TreeEntry> stack{};

    stack.push({this, 0});

    std::string resulting_tree{};

    while(not stack.empty())
    {
        const auto entry{stack.top()};
        stack.pop();

        resulting_tree += n_times_tabs(entry.level) + entry.task->name() + "\n";

        // Last first on the stack, so they come off it in their order.
        const auto children{entry.task->children()};
        for(auto child{children.rbegin()}; child != children.rend(); ++ child)
        {
            stack.push({*child, entry.level + 1});
        }
    }

    return resulting_tree;
}
#endif

void Task::on_about_to_start()
{
    task_on_about_to_start();
    m_was_started = true;
}

bool Task::was_started() const
{
    return m_was_started;
}

Task::State Task::update(float delta_time)
{
    if(task_update(delta_time) == State::finished)
    {
        m_was_started = false;
        return State::finished;
    }

    return State::working;
}

std::vector<const Task*> Task::children() const
{
    return {};
}

std::vector<std::unique_ptr<Task>> Task::grab_children()
{
    return {};
}

void destroy_task_tree(TaskPtr task)
{
    std::vector<TaskPtr> pending_tasks{};
    pending_tasks.emplace_back(std::move(task));

    while(!pending_tasks.empty())
    {
        TaskPtr current_task{std::move(pending_tasks.back())};
        pending_tasks.pop_back();

        for(auto& child : current_task->grab_children())
        {
            if(child)
            {
                pending_tasks.emplace_back(std::move(child));
            }
        }
    }
}

void Task::task_on_about_to_start()
{
}

} // namespace tasks
} // namespace olagarro
