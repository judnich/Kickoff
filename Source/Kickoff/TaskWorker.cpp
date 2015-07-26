#include "TaskWorker.h"
#include "Crust/Util.h"
#include "Crust/Process.h"
#include "Crust/Error.h"
#include <thread>
#include <chrono>
#include "External\rlutil.h"

static const int MAX_POLL_INTERVAL_SECONDS = 60;


TaskWorker::TaskWorker(TaskClient& client, std::vector<std::string>&& affinities)
    : m_client(client), m_affinities(std::move(affinities))
{
}


TaskWorker::~TaskWorker()
{
}

void TaskWorker::run()
{
	int pollIntervalSeconds = 0;
	for (;;)
	{
        auto ch = rlutil::nb_getch();
        if (ch != 0) {
            ColoredString("Keypress detected, shutting down worker\n", TextColor::LightRed).print();
            return;
        }

		if (tryRunOneTask())
		{
			pollIntervalSeconds = 0;
			ColoredString("Requesting next task\n", TextColor::Cyan).print();
		}
		else
		{
			ColoredString("Waiting for task (" + std::to_string(pollIntervalSeconds) + "s)\r", TextColor::Cyan).print();
			std::this_thread::sleep_for(std::chrono::seconds(pollIntervalSeconds));
			pollIntervalSeconds = (pollIntervalSeconds + 1) + (pollIntervalSeconds / 10); // slow exponential + linear slowdown
			if (pollIntervalSeconds > MAX_POLL_INTERVAL_SECONDS) { pollIntervalSeconds = MAX_POLL_INTERVAL_SECONDS; }
		}
	}
}

bool TaskWorker::tryRunOneTask()
{
    auto optRunInfo = m_client.takeTaskToRun(m_affinities);
    TaskRunInfo runInfo;
    if (!optRunInfo.tryGet(runInfo)) {
        return false;
    }

    std::string taskFolder = "./Task_" + toHexString(runInfo.id);
    makeDirectory(taskFolder);

    std::string taskScriptFilePath = taskFolder + "/Task_" + toHexString(runInfo.id) + ".script";
    writeFileData(runInfo.executable.script.get(), taskScriptFilePath);

	ProcessStartInfo startInfo;
	startInfo.commandStr = runInfo.executable.interpreter.get() + " " + taskScriptFilePath + " " + runInfo.executable.args.get();
	startInfo.workingDir = ".";

	ColoredString("Starting task " + toHexString(runInfo.id) + "\n", TextColor::Green).print();
	Process proc(startInfo);

	int pollIntervalSeconds = 1;
	while (proc.isRunning())
	{
		std::this_thread::sleep_for(std::chrono::seconds(pollIntervalSeconds));
		pollIntervalSeconds = (pollIntervalSeconds + 1) + (pollIntervalSeconds / 2); // slow exponential + linear slowdown
		if (pollIntervalSeconds > MAX_POLL_INTERVAL_SECONDS) { pollIntervalSeconds = MAX_POLL_INTERVAL_SECONDS; }

		auto optWasCanceled = m_client.wasTaskCanceled(runInfo.id);
		if (optWasCanceled.orDefault(false) == true) {
			ColoredString("Killing task " + toHexString(runInfo.id) + "\n", TextColor::Red).print();
			proc.terminate();
			break;
		}
	}
	proc.wait();

	ColoredString("Finished task " + toHexString(runInfo.id) + "\n", TextColor::LightGreen).print();
	pollIntervalSeconds = 1;
	while (!m_client.markTaskFinished(runInfo.id)) {
		std::this_thread::sleep_for(std::chrono::seconds(pollIntervalSeconds));
		pollIntervalSeconds = (pollIntervalSeconds + 1) + (pollIntervalSeconds / 2); // slow exponential + linear slowdown
		if (pollIntervalSeconds > MAX_POLL_INTERVAL_SECONDS) { pollIntervalSeconds = MAX_POLL_INTERVAL_SECONDS; }

		printWarning("Failed to mark task " + toHexString(runInfo.id) + " as finished; retrying.");
	}

	ColoredString("Deleting workspace folder for task " + toHexString(runInfo.id) + "\n", TextColor::Cyan).print();

	bool success = false;
	for (int tries = 0; tries < 10; ++tries) {
		if (deleteDirectory(taskFolder, true)) {
			success = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	if (!success) {
		ColoredString("Failed to delete workspace folder \"" + taskFolder + "\"!\n", TextColor::LightRed).print();
	}

	return true;
}
