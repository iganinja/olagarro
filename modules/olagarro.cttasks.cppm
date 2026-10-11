module;

#include <cttasks/earlyexitparallel.h>
#include <cttasks/execute.h>
#include <cttasks/parallel.h>
#include <cttasks/repeat.h>
#include <cttasks/select.h>
#include <cttasks/serial.h>
#include <cttasks/taskexecutor.h>
#include <cttasks/taskscommon.h>
#include <cttasks/wait.h>

export module olagarro.cttasks;

export namespace olagarro::ct_tasks
{
using ::olagarro::ct_tasks::EarlyExitParallel;
using ::olagarro::ct_tasks::Execute;
using ::olagarro::ct_tasks::Parallel;
using ::olagarro::ct_tasks::Repeat;
using ::olagarro::ct_tasks::RepeatForever;
using ::olagarro::ct_tasks::RepeatWhile;
using ::olagarro::ct_tasks::Select;
using ::olagarro::ct_tasks::SelectEntry;
using ::olagarro::ct_tasks::Serial;
using ::olagarro::ct_tasks::State;
using ::olagarro::ct_tasks::TaskExecutor;
using ::olagarro::ct_tasks::Wait;
using ::olagarro::ct_tasks::WaitToPredicate;
using ::olagarro::ct_tasks::early_exit_parallel;
using ::olagarro::ct_tasks::execute;
using ::olagarro::ct_tasks::parallel;
using ::olagarro::ct_tasks::repeat;
using ::olagarro::ct_tasks::repeat_forever;
using ::olagarro::ct_tasks::repeat_while;
using ::olagarro::ct_tasks::select;
using ::olagarro::ct_tasks::serial;
using ::olagarro::ct_tasks::wait;
}
