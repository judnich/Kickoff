// Kickoff.cpp : Defines the entry point for the console application.
//

#include "Precomp.h"

const int DEFAULT_TASK_SERVER_PORT = 3355;


static TextContainer::Ptr usageMessage(const std::string& args)
{
    return TextContainer::make(2, 0, 
        TextBlock::make(ColoredString("Kickoff ", TextColor::LightGreen) + ColoredString(args, TextColor::Green))
    );
}

static TextContainer::Ptr helpMessage()
{
    auto doc = TextContainer::make();
    *doc += TextHeader::make("Kickoff");

    *doc += TextContainer::make(2, 1, TextBlock::make(
        "\"Kickoff\" is a minimalistic, highly efficient task dispatch system for \"heterogeneous\" compute clusters, "
        "supporting mapping tasks to machines with matching capabilities. At its core, launching a task with Kickoff "
        "simply implies including one small script file (along with optional per-task command-line arguments to pass "
        "the script, which is then eventually executed on whatever compatible worker process dequeues the task."
        
        "\n\nThis means Kickoff does NOT manage the distribution of large or even payloads such as your task's executable "
        "content and input/output data (not even task stdout is stored by Kickoff). Instead, these are to be managed by "
        "a separate system of your choice, which can be invoked via the scripts you launch. This separation is intentional, "
        "keeping Kickoff focused on doing one task and only one task very well: dispatching tasks to workers."

        "\n\nWorker processes can be started anywhere and in any quantity, as long as they have network "
        "access to the central server. The \"heterogeneous\" part comes from Kickoff's \"resource tag\" system, which effectively "
        "allows desired machine capabilities and resources to be specified per-task, so they\'re mapped to appropriate machines. "
        "This resource tag system is very simple and fully generic, allowing you to define your own capability groups ad-hoc (see below). "
    ));

    *doc += TextContainer::make(2, 1, TextBlock::make("Usage:\n\n", TextColor::White));

    *doc += usageMessage(
        "new <command to execute> [args] -server <database address>\n"
        "  -require <required resource tags separated by space or comma>\n"
        "  -want <optional resource tags separated by space or comma>\n");
    *doc += usageMessage("cancel <task id> -server <database address");
    *doc += usageMessage("info <task id> -server <database address>");
    *doc += usageMessage("list -server <database address>");
    *doc += usageMessage("stats -server <database address>");
    *doc += usageMessage("worker -server <database address> [-have <resource tags>]");
    *doc += usageMessage("server [-port <portnum>]");

    *doc += TextContainer::make(2, 1, TextBlock::make(
        "Enqueueing a task is fairly straightforward; you are expected to provide a script file which will be executed on the "
        "worker that takes on this task, along with any optional command-line arguments to send the script. Your script is "
        "responsible for synchronizing and inputs/outputs of your task, including the full task executables or scripts required "
        "for this. Note that Kickoff auto-recognizes many script types by extension (e.g. .py, .sh, .cmd, .bat), but you may "
        "optionally manually specify the command name of the interpreter to use.\n"
    ));

    *doc += TextContainer::make(2, 1, TextBlock::make(
        "When a worker is launched, one or more resource tags (separated by space) may be given."
    ));

    return std::move(doc);
}

std::vector<std::string> parseResourceTags(const std::string& listStr)
{
    std::vector<std::string> resourceTags = splitString(listStr, " ;,", false);
    return resourceTags;
}

std::vector<PooledString> toPooledStrings(std::vector<std::string>&& strings)
{
    std::vector<PooledString> pooledStrings;
    for (auto& str : strings) {
        pooledStrings.push_back(str);
    }

    return std::move(pooledStrings);
}

struct ServerAddress
{
    std::string ip;
    int port;
};

ServerAddress parseConnectionString(const std::string& connectionStr, int defaultPort)
{
    auto parts = splitString(connectionStr, ":");
    if (parts.size() > 2) {
        printError("Failed to parse connection string (too many colons): \"" + connectionStr + "\"");
        exit(-1);
    }
    else if (parts.size() < 1) {
        printError("Failed to parse connection string (no ip)");
    }

    ServerAddress addr;
    addr.ip = parts[0];
    addr.port = parts.size() > 1 ? parseInt(parts[1]) : defaultPort;
    return addr;
}


TaskWorker* gWorkerForInterruptHandler = nullptr;

static void interruptHandler(int sig)
{
    if (gWorkerForInterruptHandler) {
        signal(sig, SIG_IGN);

        printWarning("Control-C was detected while the worker is running; shutting down gracefully now. "
            "Trying Control-C again will immediately terminate the worker and the task running within.");

        gWorkerForInterruptHandler->shutdown();
        gWorkerForInterruptHandler = nullptr;


        signal(SIGINT, interruptHandler);
    }
    else {
        printError("Control-C was detected again while the worker is running. Terminating immediately!");
        exit(-2);
    }
}


int main(int argc, char* argv[])
{
    CommandArgs args(argc, argv);

    if (args.getUnnamedArgCount() == 0) {
        helpMessage()->print();
        return 0;
    }

    auto command = args.popUnnamedArg();
    if (command == "new") {
        auto address = parseConnectionString(args.expectOptionValue("server"), DEFAULT_TASK_SERVER_PORT);
        TaskClient client(address.ip, address.port);

        std::string command = args.popUnnamedArg();
        while (args.getUnnamedArgCount() > 0) {
            if (command != "") { command += " "; }
            command += args.popUnnamedArg();
        }

        TaskCreateInfo info;
        info.schedule.requiredResources = toPooledStrings(parseResourceTags(args.getOptionValue("require")));
        info.schedule.optionalResources = toPooledStrings(parseResourceTags(args.getOptionValue("want")));
        info.executable.command = command;

        ColoredString("Creating task\n", TextColor::Cyan).print();
        auto result = client.createTask(info);
        TaskID taskID = result.orFail("Failed to create task.");

        (ColoredString("Success! Created task:\n", TextColor::Green) +
            ColoredString(toHexString(taskID), TextColor::LightGreen)).print();
        return 0;
    }
    else if (command == "cancel") {
        auto address = parseConnectionString(args.expectOptionValue("server"), DEFAULT_TASK_SERVER_PORT);
        auto taskIDStr = args.popUnnamedArg();
        TaskID taskID = hexStringToUint64(taskIDStr).orFail("Failed to parse hexadecimal task ID: " + taskIDStr);

        TaskClient client(address.ip, address.port);
        if (!client.markTaskShouldCancel(taskID)) {
            printError("Failed mark task for cancellation. Task may not exist (e.g. was already canceled, finished, or never started).");
            return -1;
        }

        (ColoredString("Success! Canceled task: ", TextColor::Green) +
            ColoredString(toHexString(taskID), TextColor::LightGreen)).print();
        return 0;
    }
    else if (command == "info") {
        auto address = parseConnectionString(args.expectOptionValue("server"), DEFAULT_TASK_SERVER_PORT);
        auto taskIDStr = args.popUnnamedArg();
        TaskID taskID = hexStringToUint64(taskIDStr).orFail("Failed to parse hexadecimal task ID: " + taskIDStr);

        TaskClient client(address.ip, address.port);

        auto optStatus = client.getTaskStatus(taskID);
        const auto& status = optStatus.refOrFail("Failed to retrieve task info. Task may not exist (e.g. was canceled, finished, or never started)");

        auto optSchedule = client.getTaskSchedule(taskID);
        const auto& schedule = optSchedule.refOrFail("Failed to retrieve task info. Internal error: Retrieved status but not schedule.");

        auto state = status.getState();
        TextColor statusColor, statusColorBright;
        if (state == TaskState::Pending) { statusColorBright = TextColor::LightCyan; statusColor = TextColor::Cyan; }
        else if (state == TaskState::Running) { statusColorBright = TextColor::LightGreen; statusColor = TextColor::Green; }
        else if (state == TaskState::Canceling) { statusColorBright = TextColor::LightRed; statusColor = TextColor::Red; }
        else { fail("Unexpected task state from server"); }

        (ColoredString(toHexString(taskID), statusColorBright)
            + ColoredString(": " + status.toString(), statusColor)
            + ColoredString("\n" + schedule.toString() + "\n", statusColor)
        ).print();

        return 0;
    }
    else if (command == "list") {
        auto address = parseConnectionString(args.expectOptionValue("server"), DEFAULT_TASK_SERVER_PORT);

        TaskClient client(address.ip, address.port);

        std::set<TaskState> states;
        states.insert(TaskState::Pending);
        states.insert(TaskState::Running);
        states.insert(TaskState::Canceling);

        const auto& tasks = client.getTasksByStates(states).refOrFail(
                "Task list is not available because the total number of tasks is too large. This command is meant "
                "to be used as a debugging tool for small-scale deployments, not large scale clusters.");

        TextHeader::make("Tasks Status")->print();
        printWarning("The status command is meant to be used as a debugging tool for small-scale deployments, not large scale clusters. "
            "This command will (intentionally) fail to succeed when the task server has a large number of tasks.");

        for (auto& task : tasks) {
            auto state = task.status.getState();
            TextColor statusColor, statusColorBright;
            if (state == TaskState::Pending) { statusColorBright = TextColor::LightCyan; statusColor = TextColor::Cyan; }
            else if (state == TaskState::Running) { statusColorBright = TextColor::LightGreen; statusColor = TextColor::Green; }
            else if (state == TaskState::Canceling) { statusColorBright = TextColor::LightRed; statusColor = TextColor::Red; }
            else { fail("Unexpected task state from server"); }

            (ColoredString(toHexString(task.id), statusColorBright) + ColoredString(": " + task.status.toString(), statusColor)).print();
            printf("\n");
        }

        if (tasks.size() == 0) {
            ColoredString("No tasks.\n", TextColor::LightCyan).print();
        }
    }
    else if (command == "stats") {
        auto address = parseConnectionString(args.expectOptionValue("server"), DEFAULT_TASK_SERVER_PORT);

        TaskClient client(address.ip, address.port);
        auto optStats = client.getStats();

        const auto& stats = client.getStats().refOrFail("Failed retrieve task server stats. Server may not be responding.");

        (ColoredString(std::to_string(stats.numPending), TextColor::LightCyan) + ColoredString(" tasks pending\n", TextColor::Cyan)).print();
        (ColoredString(std::to_string(stats.numRunning), TextColor::LightGreen) + ColoredString(" tasks running\n", TextColor::Green)).print();
        (ColoredString(std::to_string(stats.numCanceling), TextColor::LightRed) + ColoredString(" tasks running\n", TextColor::Red)).print();
        (ColoredString(std::to_string(stats.numFinished), TextColor::LightMagenta) + ColoredString(" tasks finished.\n", TextColor::Magenta)).print();
    }
    else if (command == "worker") {
        auto address = parseConnectionString(args.expectOptionValue("server"), DEFAULT_TASK_SERVER_PORT);
        auto affinities = parseResourceTags(args.getOptionValue("have"));

        TaskClient client(address.ip, address.port);
        TaskWorker worker(std::move(client), std::move(affinities));

        gWorkerForInterruptHandler = &worker;
        signal(SIGINT, interruptHandler);
 
        worker.run();

        ColoredString("Worker was gracefully shut down!\n", TextColor::LightGreen).print();
        return 0;
    }
    else if (command == "server") {
        std::string portStr = args.getOptionValue("port", std::to_string(DEFAULT_TASK_SERVER_PORT));
        int port = parseInt(portStr);
        if (port == 0) {
            printError("Invalid port number.");
            return -1;
        }

        TaskServer server(port);
        server.run();

        ColoredString("Server was gracefully shut down!\n", TextColor::LightGreen).print();
        return 0;
    }
    else {
        printWarning("Invalid command \"" + command + "\"");
        helpMessage()->print();
        return -1;
    }

    return 0;
}

