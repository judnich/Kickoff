#include "TaskWorker.h"
#include "Process.h"
#include "Crust/Util.h"
#include "Crust/Error.h"
#include <thread>
#include <chrono>

static const int MIN_PROCESS_POLL_INTERVAL_MS = 100;
static const int MIN_SERVER_POLL_MS = 1000;
static const int MAX_WAITING_POLL_INTERVAL_MS = 60 * 1000;
static const int MAX_RUNNING_POLL_INTERVAL_MS = clamp<int>(MAX_WAITING_POLL_INTERVAL_MS, MIN_PROCESS_POLL_INTERVAL_MS, 1000 * WORKER_HEARTBEAT_TIMEOUT_SECONDS / 2);


TaskWorker::TaskWorker(TaskClient& client, std::vector<std::string>&& resources)
    : m_client(client), m_resources(std::move(resources)), m_running(false)
{
}


TaskWorker::~TaskWorker()
{
}

void TaskWorker::run()
{
    ColoredString("Starting worker.\n", TextColor::Cyan).print();
    m_running = true;

    int pollIntervalMS = 0;
    while (m_running)
    {
        if (tryRunOneTask())
        {
            pollIntervalMS = 0;
            ColoredString("Requesting next task\n", TextColor::Cyan).print();
        }
        else
        {
            ColoredString("Waiting for task (" + std::to_string(pollIntervalMS / 1000) + "s)\r", TextColor::Cyan).print();
            
            // While no tasks are ready, sleep for a little bit before checking again (at slowly increasing intervals)
            pollIntervalMS = clamp(pollIntervalMS, MIN_SERVER_POLL_MS, MAX_WAITING_POLL_INTERVAL_MS);
            std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMS));
            pollIntervalMS = (pollIntervalMS + 1) + (pollIntervalMS / 4); // slow exponential slowdown
        }
    }
}

void TaskWorker::shutdown()
{
    ColoredString("Shutting down worker (will wait for running tasks to complete)\n", TextColor::LightYellow).print();
    m_running = false;
}

bool TaskWorker::tryRunOneTask()
{
    auto optRunInfo = m_client.takeTaskToRun(m_resources);
    if (!optRunInfo.hasValue())
    {
        return false;
    }
    auto& runInfo = optRunInfo.refOrFail("Assert fail");

    ProcessStartInfo startInfo;
    startInfo.commandStr = runInfo.command.get();
    startInfo.workingDir = ".";

    ColoredString("Starting task " + toHexString(runInfo.id) + "\n", TextColor::Green).print();
    Process proc(startInfo);

    int pollIntervalSecondsMS = 0;
    int timeSleptBetweenHeartbeatMS = 0;

    while (proc.isRunning())
    {
        // While the process is running, sleep for a little bit before checking again (at slowly increasing intervals)
        pollIntervalSecondsMS = clamp(pollIntervalSecondsMS, MIN_PROCESS_POLL_INTERVAL_MS, MAX_RUNNING_POLL_INTERVAL_MS);
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalSecondsMS));
        timeSleptBetweenHeartbeatMS += pollIntervalSecondsMS;
        pollIntervalSecondsMS = (pollIntervalSecondsMS + 1) + (pollIntervalSecondsMS / 2);

        // If enough time has passed, send a heartbeat signal while and check if the task was canceled
        if (timeSleptBetweenHeartbeatMS >= MIN_SERVER_POLL_MS) {
            timeSleptBetweenHeartbeatMS = 0;

            auto optWasCanceled = m_client.heartbeatAndCheckWasTaskCanceled(runInfo.id);
            if (optWasCanceled.orDefault(false) == true) {
                ColoredString("Killing task " + toHexString(runInfo.id) + "\n", TextColor::Red).print();
                proc.terminate();
                break;
            }
        }
    }
    proc.wait();

    ColoredString("Finished task " + toHexString(runInfo.id) + "\n", TextColor::LightGreen).print();
    if (!m_client.markTaskFinished(runInfo.id)) {
        printWarning("Failed to mark task " + toHexString(runInfo.id) + " as finished!");
    }

    return true;
}
