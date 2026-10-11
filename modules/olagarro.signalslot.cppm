module;

#include <signalslot/signalslot.h>

export module olagarro.signalslot;

export namespace olagarro::signal_slot
{
using ::olagarro::signal_slot::Connection;
using ::olagarro::signal_slot::ConnectionState;
using ::olagarro::signal_slot::FunctionSlot;
using ::olagarro::signal_slot::FunctorSlot;
using ::olagarro::signal_slot::MethodSlot;
using ::olagarro::signal_slot::ScopedConnection;
using ::olagarro::signal_slot::Signal;
using ::olagarro::signal_slot::Slot;
}
