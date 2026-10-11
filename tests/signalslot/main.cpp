#include <signalslot/signalslot.h>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <vector>

namespace
{

using namespace olagarro;
using namespace olagarro::signal_slot;

class Counter final
{
  public:
    void add(int value) { total += value; }

    int total{0};
};

class DisconnectingSlot final : public Slot<>
{
  public:
    DisconnectingSlot(Signal<>& signal, int& calls) : m_signal{signal}, m_calls{calls} {}

    void call() override
    {
        ++ m_calls;
        disconnect(m_signal);
    }

  private:
    Signal<>& m_signal;
    int& m_calls;
};

// Counts its calls.
class CountingSlot final : public Slot<>
{
  public:
    explicit CountingSlot(int& calls) : m_calls{calls} {}

    void call() override { ++ m_calls; }

  private:
    int& m_calls;
};

// Destroys the signal it is called by, as a window closed by its own button does.
class DestroyingSlot final : public Slot<>
{
  public:
    DestroyingSlot(std::unique_ptr<Signal<>>& signal, int& calls) : m_signal{signal}, m_calls{calls} {}

    void call() override
    {
        ++ m_calls;
        m_signal.reset();
    }

  private:
    std::unique_ptr<Signal<>>& m_signal;
    int& m_calls;
};

} // namespace

TEST(SignalSlotTests, CallsMethodAndFunctorSlots)
{
    Signal<int> signal{};
    Counter counter{};
    MethodSlot<Counter, int> methodSlot{counter, &Counter::add, signal};
    auto functorSlot{FunctorSlot{[&counter](int value) { counter.total += value * 10; }, signal}};

    signal.fire(2);

    EXPECT_EQ(counter.total, 22);
}

TEST(SignalSlotTests, AllowsSlotsToDisconnectWhileFiring)
{
    Signal<> signal{};
    auto calls{0};
    DisconnectingSlot slot{signal, calls};
    slot.connect(signal);

    signal.fire();
    signal.fire();

    EXPECT_EQ(calls, 1);
}

TEST(SignalSlotTests, DisconnectsWhenEitherEndpointIsDestroyed)
{
    auto calls{0};
    Signal<> signal{};
    {
        FunctorSlot slot{[&calls]() { ++ calls; }, signal};
        signal.fire();
    }

    signal.fire();
    EXPECT_EQ(calls, 1);
}

TEST(SignalSlotTests, KeepsCallbacksConnectedWhenTheirConnectionIsIgnored)
{
    Signal<int> signal{};
    auto total{0};

    signal.connect([&total](int value) { total += value; });
    signal.fire(3);

    EXPECT_EQ(total, 3);
}

TEST(SignalSlotTests, DisconnectsCallbacksThroughConnection)
{
    Signal<> signal{};
    auto calls{0};
    const auto connection{signal.connect([&calls] { ++ calls; })};

    signal.fire();
    connection.disconnect();
    signal.fire();

    EXPECT_EQ(calls, 1);
}

// A scoped connection given another let go of the one it had without disconnecting it: that slot was called for
// as long as the signal lived.
TEST(SignalSlotTests, AScopedConnectionGivenAnotherDisconnectsTheOneItHad)
{
    Signal<> signal;
    int first_calls{0};
    int second_calls{0};
    ScopedConnection connection{signal.connect([&first_calls] { ++ first_calls; })};
    connection = ScopedConnection{signal.connect([&second_calls] { ++ second_calls; })};
    signal.fire();
    EXPECT_EQ(first_calls, 0);
    EXPECT_EQ(second_calls, 1);

    ScopedConnection moved{std::move(connection)};
    connection = std::move(moved);
    signal.fire();
    EXPECT_EQ(second_calls, 2);
}

// A slot object that destroyed the signal firing it left the firing on freed memory, which callbacks were kept from.
TEST(SignalSlotTests, ASlotMayDestroyTheSignalThatCallsIt)
{
    auto signal{std::make_unique<Signal<>>()};
    int destroying_calls{0};
    int other_calls{0};
    int callback_calls{0};
    DestroyingSlot destroying{signal, destroying_calls};
    CountingSlot other{other_calls};
    destroying.connect(*signal);
    other.connect(*signal);
    signal->connect([&callback_calls] { ++ callback_calls; });
    signal->fire();
    EXPECT_EQ(destroying_calls, 1);
    EXPECT_EQ(other_calls, 0) << "the signal was gone before its turn";
    EXPECT_EQ(callback_calls, 0);
    EXPECT_EQ(signal, nullptr);
}

TEST(SignalSlotTests, ScopedConnectionDisconnectsAtEndOfScope)
{
    Signal<> signal{};
    auto calls{0};
    {
        ScopedConnection connection{signal.connect([&calls] { ++ calls; })};
        signal.fire();
    }

    signal.fire();

    EXPECT_EQ(calls, 1);
}

namespace
{

// Counts how many of it there are, to see each callback destroyed once, when its time comes.
struct Tracked
{
    explicit Tracked(int& alive) : m_alive{&alive} { ++*m_alive; }
    Tracked(const Tracked& other) : m_alive{other.m_alive} { ++*m_alive; }
    Tracked(Tracked&& other) noexcept : m_alive{other.m_alive} { ++*m_alive; }
    Tracked& operator=(const Tracked&) = delete;
    Tracked& operator=(Tracked&&) = delete;
    ~Tracked() { --*m_alive; }

    int* m_alive;
};

} // namespace

TEST(SignalSlotTests, ACallbackMayDisconnectItselfWhileItRuns)
{
    Signal<> signal{};
    auto calls{0};
    Connection connection;
    connection = signal.connect(
        [&calls, &connection]
        {
            ++ calls;
            // What runs is kept until it returns, though it is no longer connected.
            connection.disconnect();
            ++ calls;
        });
    signal.fire();
    signal.fire();
    EXPECT_EQ(calls, 2);
}

TEST(SignalSlotTests, ACallbackConnectedWhileFiringIsCalledFromTheNextFire)
{
    Signal<> signal{};
    auto late{0};
    std::vector<ScopedConnection> kept;
    kept.emplace_back(signal.connect(
        [&signal, &late, &kept]
        {
            if(kept.size() == 1)
            {
                kept.emplace_back(signal.connect([&late] { ++ late; }));
            }
        }));
    signal.fire();
    EXPECT_EQ(late, 0);
    signal.fire();
    EXPECT_EQ(late, 1);
}

TEST(SignalSlotTests, TakesCallbacksThatCanOnlyBeMoved)
{
    Signal<int> signal{};
    auto total{0};
    auto owned{std::make_unique<int>(10)};
    signal.connect([&total, owned = std::move(owned)](int value) { total += value * *owned; });
    signal.fire(2);
    EXPECT_EQ(total, 20);
}

TEST(SignalSlotTests, EachCallbackIsDestroyedOnceWhenDisconnectedOrWithTheSignal)
{
    auto alive{0};
    {
        Signal<> signal{};
        const auto first{signal.connect([tracked = Tracked{alive}] {})};
        signal.connect([tracked = Tracked{alive}] {});
        EXPECT_EQ(alive, 2);
        signal.fire();
        EXPECT_EQ(alive, 2) << "firing makes no copies";
        first.disconnect();
        EXPECT_EQ(alive, 1);
    }
    EXPECT_EQ(alive, 0);
}

TEST(SignalSlotTests, ACallbackThatThrowsLeavesNothingBehind)
{
    auto alive{0};
    {
        Signal<> signal{};
        signal.connect([tracked = Tracked{alive}] { throw std::runtime_error{"stop"}; });
        EXPECT_THROW(signal.fire(), std::runtime_error);
        EXPECT_EQ(alive, 1);
    }
    EXPECT_EQ(alive, 0);
}

TEST(SignalSlotTests, ASignalMayBeDestroyedByOneOfItsCallbacks)
{
    auto alive{0};
    auto signal{std::make_unique<Signal<>>()};
    auto calls{0};
    signal->connect(
        [&signal, &calls, tracked = Tracked{alive}]
        {
            ++ calls;
            signal.reset();
        });
    signal->connect([&calls] { ++ calls; });
    signal->fire();
    // The callback that destroyed it finished; the one after it is no longer connected.
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(alive, 0);
}

TEST(SignalSlotTests, AFireInsideAnotherWaitsForTheOuterToLetGo)
{
    auto alive{0};
    Signal<int> signal{};
    Connection inner;
    auto calls{0};
    signal.connect(
        [&signal, &inner](int depth)
        {
            if(depth == 0)
            {
                signal.fire(1);
                // The inner fire disconnected it, but this one is still going: it is not gone yet.
                EXPECT_FALSE(inner.is_connected());
            }
        });
    inner = signal.connect(
        [&inner, &calls, tracked = Tracked{alive}](int)
        {
            ++ calls;
            inner.disconnect();
        });
    signal.fire(0);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(alive, 0);
}

TEST(SignalSlotTests, DisconnectingAllWhileFiringCallsNoMore)
{
    auto alive{0};
    Signal<> signal{};
    auto calls{0};
    signal.connect(
        [&signal, &calls]
        {
            ++ calls;
            signal.disconnect_all();
        });
    signal.connect([&calls, tracked = Tracked{alive}] { ++ calls; });
    signal.fire();
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(alive, 0);
    signal.fire();
    EXPECT_EQ(calls, 1);
}
