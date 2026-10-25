module;

#include <concurrency/concurrency.h>

export module olagarro.concurrency;

export namespace olagarro::concurrency
{
using ::olagarro::concurrency::CancelToken;
using ::olagarro::concurrency::concurrent_for;
using ::olagarro::concurrency::LatestWins;
using ::olagarro::concurrency::Reductor;
using ::olagarro::concurrency::reductor;
using ::olagarro::concurrency::TaskCancelled;
using ::olagarro::concurrency::TaskHandle;
using ::olagarro::concurrency::TaskPool;
} // namespace olagarro::concurrency
