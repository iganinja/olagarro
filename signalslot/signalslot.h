#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace olagarro::signal_slot
{

template <typename... Parameters> class Signal;

class ConnectionState
{
  public:
    virtual ~ConnectionState() = default;
    virtual void disconnect(std::size_t id) = 0;
    [[nodiscard]] virtual bool is_connected(std::size_t id) const = 0;
};

class Connection
{
  public:
    Connection() = default;

    void disconnect() const
    {
        if(const auto state{m_state.lock()})
        {
            state->disconnect(m_id);
        }
    }

    [[nodiscard]] bool is_connected() const
    {
        if(const auto state{m_state.lock()})
        {
            return state->is_connected(m_id);
        }

        return false;
    }

  private:
    template <typename... Parameters> friend class Signal;

    Connection(std::weak_ptr<ConnectionState> state, std::size_t id) : m_state{std::move(state)}, m_id{id} {}

    std::weak_ptr<ConnectionState> m_state;
    std::size_t m_id{0};
};

class ScopedConnection
{
  public:
    ScopedConnection() = default;

    explicit ScopedConnection(Connection connection) : m_connection{std::move(connection)} {}

    ~ScopedConnection() { m_connection.disconnect(); }

    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;
    ScopedConnection(ScopedConnection&& other) noexcept : m_connection{std::exchange(other.m_connection, {})} {}
    // Disconnects the connection it had before taking the other's.
    ScopedConnection& operator=(ScopedConnection&& other) noexcept
    {
        if(this != &other)
        {
            m_connection.disconnect();
            m_connection = std::exchange(other.m_connection, {});
        }
        return *this;
    }

    void disconnect() const { m_connection.disconnect(); }

  private:
    Connection m_connection;
};

namespace detail
{

// A callback a signal keeps, its type forgotten, with the function made for its type that destroys it. No virtual
// functions: a class with them brings a table and the type's name for every lambda, which signals have hundreds of.
struct Callable
{
    explicit Callable(void (*destroy)(Callable*)) : destroy{destroy} {}
    Callable(const Callable&) = delete;
    Callable& operator=(const Callable&) = delete;

    void (*destroy)(Callable*);
};

// What unique_ptr destroys a callback with: the function made for its type.
struct Destroy
{
    void operator()(Callable* callable) const { callable->destroy(callable); }
};

using Owned = std::unique_ptr<Callable, Destroy>;

// A callback for a signal of these parameters, with the function made for its type that calls it.
template <typename... Parameters> struct Invocable : Callable
{
    Invocable(void (*destroy)(Callable*), void (*call)(Invocable*, Parameters...)) : Callable{destroy}, call{call} {}

    void (*call)(Invocable*, Parameters...);
};

// The callback itself, as it was given, and the two functions made for its type: the one that calls it with the
// signal's parameters, and the one that destroys it, which the unique_ptr holding it calls.
template <typename Function, typename... Parameters> struct Held final : Invocable<Parameters...>
{
    template <typename Given>
    explicit Held(Given&& given) : Invocable<Parameters...>{&destroy_it, &call_it}, function{std::forward<Given>(given)}
    {
    }

    static void call_it(Invocable<Parameters...>* callable, Parameters... parameters)
    {
        static_cast<Held*>(callable)->function(parameters...);
    }

    static void destroy_it(Callable* callable) { delete static_cast<Held*>(callable); }

    Function function;
};

// What every signal keeps, whatever it carries, written once for them all: its callbacks and its slots, both in the
// order they were connected.
//
// A callback disconnected while the signal fires is not destroyed then, as it may be the one running: it is only no
// longer called, and goes once the fire is over. A fire goes through those that were connected when it started.
class SignalCore final : public ConnectionState
{
  public:
    struct Entry
    {
        std::size_t id;
        Owned callable;
        bool connected{true};
    };

    // While a fire lasts, nested or not: disconnected callbacks wait for the last one to end, even when a callback
    // throws, and go then.
    class Firing
    {
      public:
        explicit Firing(SignalCore& core) : m_core{core} { ++ m_core.m_firing; }

        ~Firing()
        {
            if(-- m_core.m_firing == 0)
            {
                m_core.sweep();
            }
        }

        Firing(const Firing&) = delete;
        Firing& operator=(const Firing&) = delete;

      private:
        SignalCore& m_core;
    };

    SignalCore() = default;
    SignalCore(const SignalCore&) = delete;
    SignalCore& operator=(const SignalCore&) = delete;

    [[nodiscard]] std::size_t add(Owned callable)
    {
        const auto id{m_next_id++};
        m_entries.push_back({id, std::move(callable)});
        return id;
    }

    void disconnect(std::size_t id) override
    {
        const auto found{std::find_if(m_entries.begin(), m_entries.end(),
                                      [id](const Entry& entry) { return entry.id == id && entry.connected; })};
        if(found == m_entries.end())
        {
            return;
        }
        if(m_firing > 0)
        {
            found->connected = false;
        }
        else
        {
            m_entries.erase(found);
        }
    }

    [[nodiscard]] bool is_connected(std::size_t id) const override
    {
        return std::any_of(m_entries.begin(), m_entries.end(),
                           [id](const Entry& entry) { return entry.id == id && entry.connected; });
    }

    void clear()
    {
        if(m_firing > 0)
        {
            for(auto& entry : m_entries)
            {
                entry.connected = false;
            }
        }
        else
        {
            m_entries.clear();
        }
    }

    // The callbacks, which only grow at their end while a fire goes through them, so a place in them stays put.
    [[nodiscard]] std::size_t count() const { return m_entries.size(); }
    // The callback at a place, if it is still connected.
    [[nodiscard]] Callable* connected_at(std::size_t index) const
    {
        const auto& entry{m_entries[index]};
        return entry.connected ? entry.callable.get() : nullptr;
    }

    // The slots, as untyped places: Signal and Slot know what they are.
    void add_slot(void* slot)
    {
        if(!has_slot(slot))
        {
            m_slots.push_back(slot);
        }
    }

    void remove_slot(void* slot) { m_slots.erase(std::remove(m_slots.begin(), m_slots.end(), slot), m_slots.end()); }

    [[nodiscard]] bool has_slot(void* slot) const
    {
        return std::find(m_slots.begin(), m_slots.end(), slot) != m_slots.end();
    }

    [[nodiscard]] const std::vector<void*>& slots() const { return m_slots; }

  private:
    void sweep()
    {
        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(), [](const Entry& entry) { return !entry.connected; }),
            m_entries.end());
    }

    std::vector<Entry> m_entries;
    std::vector<void*> m_slots;
    std::size_t m_next_id{1};
    // How many fires are going on, one inside another.
    std::size_t m_firing{0};
};

// The signals a slot is connected to, as untyped places, written once for every kind of slot.
class SlotCore
{
  public:
    [[nodiscard]] bool add(void* signal)
    {
        if(std::find(m_signals.begin(), m_signals.end(), signal) != m_signals.end())
        {
            return false;
        }
        m_signals.push_back(signal);
        return true;
    }

    [[nodiscard]] bool remove(void* signal)
    {
        const auto found{std::find(m_signals.begin(), m_signals.end(), signal)};
        if(found == m_signals.end())
        {
            return false;
        }
        m_signals.erase(found);
        return true;
    }

    [[nodiscard]] const std::vector<void*>& signals() const { return m_signals; }

  private:
    std::vector<void*> m_signals;
};

} // namespace detail

template <typename... Parameters> class Slot
{
  public:
    Slot() = default;

    explicit Slot(Signal<Parameters...>& signal) { connect(signal); }

    virtual ~Slot() { disconnect_all(); }

    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;

    virtual void call(Parameters... parameters) = 0;

    void connect(Signal<Parameters...>& signal);
    void disconnect(Signal<Parameters...>& signal);
    void disconnect_all();

  private:
    detail::SlotCore m_signals;
};

template <typename... Parameters> class Signal
{
  public:
    Signal() = default;

    ~Signal() { disconnect_all(); }

    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;

    void connect(Slot<Parameters...>* const slot) { m_state->add_slot(slot); }

    void disconnect(Slot<Parameters...>* const slot) { m_state->remove_slot(slot); }

    void disconnect_all()
    {
        const auto slots{m_state->slots()};
        for(void* const slot : slots)
        {
            if(m_state->has_slot(slot))
            {
                static_cast<Slot<Parameters...>*>(slot)->disconnect(*this);
            }
        }

        m_state->clear();
    }

    // Any callable that takes the signal's parameters, copied or moved in, kept until it is disconnected or the
    // signal goes.
    template <typename Callback> Connection connect(Callback&& callback)
    {
        const auto id{m_state->add(
            detail::Owned{new detail::Held<std::decay_t<Callback>, Parameters...>{std::forward<Callback>(callback)}})};
        return {m_state, id};
    }

    // Calls the slots and the callbacks connected when it starts that are still connected when their turn comes.
    void fire(Parameters... parameters)
    {
        // The state held while it goes through them, should a slot or a callback destroy the signal: its slots and
        // callbacks are then gone from it, and the rest are not called.
        const auto state{m_state};
        const auto slots{state->slots()};
        for(void* const slot : slots)
        {
            if(state->has_slot(slot))
            {
                static_cast<Slot<Parameters...>*>(slot)->call(parameters...);
            }
        }

        const detail::SignalCore::Firing firing{*state};
        const auto count{state->count()};
        for(std::size_t index{}; index < count; ++ index)
        {
            // Every callback of this signal is one of its parameters', as connect made it.
            if(auto* const callable{state->connected_at(index)})
            {
                auto* const invocable{static_cast<detail::Invocable<Parameters...>*>(callable)};
                invocable->call(invocable, parameters...);
            }
        }
    }

  private:
    std::shared_ptr<detail::SignalCore> m_state{std::make_shared<detail::SignalCore>()};
};

template <typename... Parameters> void Slot<Parameters...>::connect(Signal<Parameters...>& signal)
{
    if(m_signals.add(&signal))
    {
        signal.connect(this);
    }
}

template <typename... Parameters> void Slot<Parameters...>::disconnect(Signal<Parameters...>& signal)
{
    if(m_signals.remove(&signal))
    {
        signal.disconnect(this);
    }
}

template <typename... Parameters> void Slot<Parameters...>::disconnect_all()
{
    const auto signals{m_signals.signals()};
    for(void* const signal : signals)
    {
        static_cast<Signal<Parameters...>*>(signal)->disconnect(this);
    }
}

template <typename Instance, typename... Parameters> class MethodSlot final : public Slot<Parameters...>
{
  public:
    using Method = void (Instance::*)(Parameters...);

    MethodSlot(Instance& instance, Method method) : m_instance{instance}, m_method{method} {}

    MethodSlot(Instance& instance, Method method, Signal<Parameters...>& signal)
        : Slot<Parameters...>{signal}, m_instance{instance}, m_method{method}
    {
    }

    void call(Parameters... parameters) override { (m_instance.*m_method)(parameters...); }

  private:
    Instance& m_instance;
    Method m_method;
};

template <typename... Parameters> class FunctionSlot final : public Slot<Parameters...>
{
  public:
    using Function = void (*)(Parameters...);

    explicit FunctionSlot(Function function) : m_function{function} {}

    FunctionSlot(Function function, Signal<Parameters...>& signal) : Slot<Parameters...>{signal}, m_function{function}
    {
    }

    void call(Parameters... parameters) override { m_function(parameters...); }

  private:
    Function m_function;
};

template <typename Function, typename... Parameters> class FunctorSlot final : public Slot<Parameters...>
{
  public:
    explicit FunctorSlot(Function function) : m_function{std::move(function)} {}

    FunctorSlot(Function function, Signal<Parameters...>& signal)
        : Slot<Parameters...>{signal}, m_function{std::move(function)}
    {
    }

    void call(Parameters... parameters) override { m_function(parameters...); }

  private:
    Function m_function;
};

} // namespace olagarro::signal_slot
