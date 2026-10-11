module;

#include <tasks/earlyexitparallel.h>
#include <tasks/execute.h>
#include <tasks/interpolate.h>
#include <tasks/neverfinish.h>
#include <tasks/parallel.h>
#include <tasks/priorityselector.h>
#include <tasks/repeat.h>
#include <tasks/select.h>
#include <tasks/serial.h>
#include <tasks/task.h>
#include <tasks/taskexecutor.h>
#include <tasks/trigger.h>
#include <tasks/wait.h>
#include <tasks/waitsignal.h>

export module olagarro.tasks;

export namespace olagarro::tasks
{
using ::olagarro::tasks::ExecuteCallable;
using ::olagarro::tasks::ExecuteEveryUpdate;
using ::olagarro::tasks::ExecuteUntil;
using ::olagarro::tasks::Interpolate;
using ::olagarro::tasks::PriorityEntry;
using ::olagarro::tasks::Progress;
using ::olagarro::tasks::RepeatWhile;
using ::olagarro::tasks::SelectEntry;
using ::olagarro::tasks::SignalArguments;
using ::olagarro::tasks::Task;
using ::olagarro::tasks::TaskExecutor;
using ::olagarro::tasks::TaskPtr;
using ::olagarro::tasks::Trigger;
using ::olagarro::tasks::WaitHeld;
using ::olagarro::tasks::WaitPredicate;
using ::olagarro::tasks::WaitSignal;
using ::olagarro::tasks::destroy_task_tree;
using ::olagarro::tasks::early_exit_parallel;
using ::olagarro::tasks::early_exit_parallel_with_sidecars;
using ::olagarro::tasks::execute;
using ::olagarro::tasks::execute_every_update;
using ::olagarro::tasks::execute_until;
using ::olagarro::tasks::interpolate;
using ::olagarro::tasks::interpolate_value;
using ::olagarro::tasks::make_early_exit_parallel;
using ::olagarro::tasks::make_parallel;
using ::olagarro::tasks::make_priority_selector;
using ::olagarro::tasks::make_serial;
using ::olagarro::tasks::make_trigger;
using ::olagarro::tasks::never_finish;
using ::olagarro::tasks::otherwise;
using ::olagarro::tasks::parallel;
using ::olagarro::tasks::priority_selector;
using ::olagarro::tasks::progress_monitor;
using ::olagarro::tasks::repeat;
using ::olagarro::tasks::repeat_forever;
using ::olagarro::tasks::repeat_while;
using ::olagarro::tasks::select;
using ::olagarro::tasks::serial;
using ::olagarro::tasks::task_list;
using ::olagarro::tasks::wait;
using ::olagarro::tasks::wait_for;
using ::olagarro::tasks::wait_held;
using ::olagarro::tasks::wait_random;
using ::olagarro::tasks::wait_signal;
}

export namespace olagarro::tasks::curves
{
using ::olagarro::tasks::curves::ease_in;
using ::olagarro::tasks::curves::ease_in_out;
using ::olagarro::tasks::curves::ease_out;
using ::olagarro::tasks::curves::linear;
using ::olagarro::tasks::curves::smoothstep;
}
