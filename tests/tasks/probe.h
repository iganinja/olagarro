#ifndef PROBE_H
#define PROBE_H

#include <tasks/task.h>

#include <memory>

namespace Probes
{

// What a probe saw, and what it answers: kept by the test, so it can be read after the probe is gone.
struct ProbeLog
{
    int starts{0};
    int updates{0};
    // The probe finishes on every update while this is set.
    bool finishes{false};
};

// A task that counts its starts and updates, and finishes when its log says so.
class Probe : public olagarro::tasks::Task
{
  public:
    explicit Probe(std::shared_ptr<ProbeLog> log) : Task{"Probe"}, m_log{std::move(log)} {}

  protected:
    void task_on_about_to_start() override { ++ m_log->starts; }

    State task_update(float) override
    {
        ++ m_log->updates;
        return m_log->finishes ? State::finished : State::working;
    }

  private:
    std::shared_ptr<ProbeLog> m_log;
};

inline olagarro::tasks::TaskPtr probe(const std::shared_ptr<ProbeLog>& log)
{
    return std::make_unique<Probe>(log);
}

} // namespace Probes

#endif // PROBE_H
