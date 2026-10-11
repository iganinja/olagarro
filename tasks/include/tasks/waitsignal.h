#ifndef WAITSIGNAL_H
#define WAITSIGNAL_H

// Apart from the rest of the tasks, as it needs olagarro::signal_slot: who includes it links that too.

#include "tasks/task.h"

#include <signalslot/signalslot.h>

#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>

namespace olagarro
{
namespace tasks
{

// What a signal carried when it fired, kept for whoever reads it. It is shared with the task that waits for it, so a
// copy stays valid after the task is gone.
template <typename... Parameters> class SignalArguments
{
  public:
    using Values = std::tuple<std::decay_t<Parameters>...>;

    SignalArguments() : m_values{std::make_shared<std::optional<Values>>()} {}

    // Whether the signal fired since the task last started.
    bool received() const { return m_values->has_value(); }

    // The arguments of the fire that ended the wait; only when received().
    const Values& get() const { return **m_values; }

  private:
    template <typename... Others> friend class WaitSignal;

    std::shared_ptr<std::optional<Values>> m_values;
};

// Finishes on the first update after the signal fires, and keeps what it carried. It connects to the signal when it
// starts, and lets go when it finishes, starts again or is destroyed. The signal is not owned: it must outlive the
// task, or at least its last start; once connected, a signal that goes first only leaves the task waiting forever.
template <typename... Parameters> class WaitSignal : public Task
{
  public:
    explicit WaitSignal(signal_slot::Signal<Parameters...>& signal) : Task{"WaitSignal"}, m_signal{signal} {}

    SignalArguments<Parameters...> arguments() const { return m_arguments; }

  protected:
    void task_on_about_to_start() override
    {
        m_arguments.m_values->reset();

        m_connection = signal_slot::ScopedConnection{m_signal.connect(
            [values = m_arguments.m_values](Parameters... parameters)
            {
                if(not values->has_value())
                {
                    values->emplace(parameters...);
                }
            })};
    }

    State task_update(float) override
    {
        if(not m_arguments.received())
        {
            return State::working;
        }

        m_connection.disconnect();
        return State::finished;
    }

  private:
    signal_slot::Signal<Parameters...>& m_signal;
    SignalArguments<Parameters...> m_arguments;
    signal_slot::ScopedConnection m_connection;
};

// A task that waits for the signal to fire; arguments() on it gives what the signal carried, before it is moved into a
// container or an executor.
template <typename... Parameters>
std::unique_ptr<WaitSignal<Parameters...>> wait_signal(signal_slot::Signal<Parameters...>& signal)
{
    return std::make_unique<WaitSignal<Parameters...>>(signal);
}

} // namespace tasks
} // namespace olagarro

#endif // WAITSIGNAL_H
