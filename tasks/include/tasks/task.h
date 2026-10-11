#ifndef TASKS_H
#define TASKS_H

#include <memory>
#include <vector>
#include <string>
#include <utility>

#define TASKS_USE_NAMES

namespace olagarro
{
namespace tasks
{

class Task
{
  public:
    Task(const std::string& name = "Task");

    virtual ~Task() = default;

#ifdef TASKS_USE_NAMES
    const std::string& name() const;

    std::string tree() const;
#endif

    virtual void on_about_to_start();

    virtual bool was_started() const;

    enum class State
    {
        working,
        finished
    };

    virtual State update(float delta_time);

    virtual std::vector<const Task*> children() const;

    virtual std::vector<std::unique_ptr<Task>> grab_children();

  protected:
    // Usually a task needs to override those methods
    virtual void task_on_about_to_start();
    virtual State task_update(float delta_time) = 0;

#ifdef TASKS_USE_NAMES
    std::string m_name;
#endif

  private:
    bool m_was_started{false};
};

using TaskPtr = std::unique_ptr<Task>;

// The given tasks, moved into a vector, for the functions that take lists of tasks: a braced list cannot hold
// unique_ptrs, as it copies them.
template <typename... Tasks> std::vector<TaskPtr> task_list(Tasks&&... tasks)
{
    std::vector<TaskPtr> list;
    list.reserve(sizeof...(tasks));
    (list.emplace_back(std::forward<Tasks>(tasks)), ...);
    return list;
}

void destroy_task_tree(TaskPtr task);

} // namespace tasks
} // namespace olagarro

#endif // TASKS_H
