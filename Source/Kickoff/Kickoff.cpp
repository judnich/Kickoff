// Kickoff.cpp : Defines the entry point for the console application.
//

#include "Precomp.h"

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
		"supporting arbitrary dependency graphs and mapping tasks to machines with matching capabilities. At its core, "
		"launching a task with Kickoff simply implies including one small script file (along with optional per-task "
		"command-line arguments to pass the script, which is then eventually executed on whatever compatible worker "
		"process dequeues the task."
		
		"\n\nThis means Kickoff does NOT manage the distribution of large or even payloads such as your task's executable "
		"content and input/output data (not even task stdout is stored by Kickoff). Instead, these are to be managed by "
		"a separate system of your choice, which can be invoked via the scripts you launch. This separation is intentional, "
		"keeping Kickoff focused on doing one task and only one task very well: dispatching tasks to workers."

		"\n\nWorker processes can be started anywhere and in any quantity, as long as they have network "
        "access to the central server. The \"heterogeneous\" part comes from Kickoff's \"affinity\" system, which effectively "
        "allows machine capabilities to be specified per-task, so they\'re mapped to appropriate machines. This affinity "
        "system is very simple and fully generic, allowing you to define your own capability groups ad-hoc (see below). "
    ));

    *doc += TextContainer::make(2, 1, TextBlock::make("Usage:\n\n", TextColor::White));

    *doc += usageMessage(
        "new <script file> [args] [-interpreter <executable>]\n"
		"  -affinity <affinity tags separated by space or comma>\n"
		"  [-depend <dependency list>] -server <database address>\n");
    *doc += usageMessage("cancel <task id> -server <database address");
    *doc += usageMessage("info <task id> -server <database address>");
    *doc += usageMessage("list -server <database address>");
    *doc += usageMessage("worker -server <database address> -affinity <affinity tags>");
    *doc += usageMessage("server [-port <portnum>]");

    *doc += TextContainer::make(2, 1, TextBlock::make(
        "Enqueueing a task is fairly straightforward; you are expected to provide a script file which will be executed on the "
        "worker that takes on this task, along with any optional command-line arguments to send the script. Your script is "
        "responsible for synchronizing and inputs/outputs of your task, including the full task executables or scripts required "
        "for this. Note that Kickoff auto-recognizes many script types by extension (e.g. .py, .sh, .cmd, .bat), but you may "
        "optionally manually specify the command name of the interpreter to use.\n"
    ));

    *doc += TextContainer::make(2, 1, TextBlock::make(
        "You can optionally provide a string label to associate with the task to easily identify it in a list. This has no "
        "functional effect. Setting dependencies and affinities do: A task won't run until all its dependencies have finished, "
        "and a task will only run on machines which are tagged with at least one of the affinity groups you specify.\n"
    ));

    *doc += TextContainer::make(2, 1, TextBlock::make(
        "When a worker is launched, one or more affinity tags (separated by space) must be given. Then, that worker will only "
        "run tasks with at least one intersecting affinity tags. In this way, you can customize how you manage which machine(s) to "
        "run tasks on. For example, you might have one tag for 'cuda' on machines with CUDA capable GPUs; then launching tasks which "
        "require CUDA acceleration with the tag 'cuda' will ensure they will only run on appropriate machines."
    ));

	*doc += TextContainer::make(2, 1, TextBlock::make(
		"Notes: When you cancel a task, all its dependencies will be immediately canceled as well. The status command will only "
		"work if the number of tasks is relatively small; otherwise the server will refuse to give a list; this is intentional "
		"as the \"status\" command is more of a debugging tool for small scale systems and not something to be used at scale."
		));

    return std::move(doc);
}

std::vector<std::string> parseAffinities(const CommandArgs& args)
{
    std::string str = args.expectOptionValue("affinity");
    std::vector<std::string> affinities = splitString(str, " ;,", false);
	return affinities;
}

std::vector<PooledString> toPooledStrings(std::vector<std::string>&& strings)
{
	std::vector<PooledString> pooledStrings;
	for (auto& str : strings) {
		pooledStrings.push_back(str);
	}

	return std::move(pooledStrings);
}

std::vector<TaskID> parseDependencies(const CommandArgs& args)
{
    std::string str = args.getOptionValue("depend");
    std::vector<std::string> depStrs = splitString(str, " ;,", false);
    std::vector<TaskID> depIDs;

    for (auto& depStr : depStrs) {
        auto optVal = hexStringToUint64(depStr);
        TaskID taskID;
        if (optVal.tryGet(taskID)) {
            depIDs.push_back(taskID);
        }
        else {
            printError("Could not parse dependency task ID (should be hexadecimal): \"" + depStr + "\"");
        }
    }

    return depIDs;
}

std::string inferInterpreter(const std::string& scriptFilename)
{
    auto ext = getFileExtension(scriptFilename);
    if (ext == ".py") {
        return "python";
    }
    else if (ext == ".sh") {
        return "bash";
    }
    else if (ext == ".bat" || ext == ".cmd") {
        return "cmd";
    }
    else if (ext == ".js") {
        return "node";
    }
    return "";
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

const int defaultPort = 3355;

int main(int argc, char* argv[])
{
    CommandArgs args(argc, argv);

    if (args.getUnnamedArgCount() == 0) {
        helpMessage()->print();
        return 0;
    }

    auto command = args.popUnnamedArg();
    if (command == "new") {
        auto address = parseConnectionString(args.expectOptionValue("server"), defaultPort);

        ColoredString("Connecting\n", TextColor::Cyan).print();
        TaskClient client(address.ip, address.port);

        auto scriptFilename = args.popUnnamedArg();
        auto scriptFileData = readFileData(scriptFilename);
        if (!scriptFileData.hasValue()) {
            printError("Could not load script file \"" + scriptFilename + "\"");
            return -1;
        }

        std::string scriptArgs = "";
        while (args.getUnnamedArgCount() > 0) {
            if (scriptArgs != "") { scriptArgs += " "; }
            scriptArgs += args.popUnnamedArg();
        }

        TaskCreateInfo info;
        info.schedule.affinities = toPooledStrings(parseAffinities(args));
        info.schedule.dependencies = parseDependencies(args);
        info.executable.script = scriptFileData.orDefault(std::vector<uint8_t>());
        info.executable.args = scriptArgs;
        info.executable.interpreter = args.getOptionValue("interpreter", inferInterpreter(scriptFilename));

        if (info.executable.interpreter.get() == "") {
            printError("Could not infer interpreter for script extension \"" + getFileExtension(scriptFilename) + "\". Please specify via -interpreter.");
            return -1;
        }

        ColoredString("Creating task\n", TextColor::Cyan).print();
        auto result = client.createTask(info);
        TaskID taskID;
        if (!result.tryGet(taskID)) {
            printError("Failed to create task.");
            return -1;
        }

        (ColoredString("Success! Created task:\n", TextColor::Green) +
            ColoredString(toHexString(taskID), TextColor::LightGreen)).print();
        return 0;
    }
    else if (command == "cancel") {
        auto address = parseConnectionString(args.expectOptionValue("server"), defaultPort);
        auto taskIDStr = args.popUnnamedArg();
        TaskID taskID;
        if (!hexStringToUint64(taskIDStr).tryGet(taskID)) {
            printError("Failed to parse hexadecimal task ID: " + taskIDStr);
            return -1;
        }

		ColoredString("Connecting to task server\n", TextColor::Cyan).print();
		TaskClient client(address.ip, address.port);

        ColoredString("Canceling task\n", TextColor::Cyan).print();
        if (!client.markTaskShouldCancel(taskID)) {
            printError("Failed mark task for cancellation. Task may not exist (e.g. was already canceled, finished, or never started).");
            return -1;
        }

        (ColoredString("Success! Canceled task: ", TextColor::Green) +
            ColoredString(toHexString(taskID), TextColor::LightGreen)).print();
        return 0;
    }
    else if (command == "info") {
        auto address = parseConnectionString(args.expectOptionValue("server"), defaultPort);
        auto taskIDStr = args.popUnnamedArg();
        TaskID taskID;
        if (!hexStringToUint64(taskIDStr).tryGet(taskID)) {
            printError("Failed to parse hexadecimal task ID: " + taskIDStr);
            return -1;
        }

		ColoredString("Connecting to task server\n", TextColor::Cyan).print();
		TaskClient client(address.ip, address.port);

        ColoredString("Requesting task info\n", TextColor::Cyan).print();
        auto optStatus = client.getTaskStatus(taskID);
        auto optSchedule = client.getTaskSchedule(taskID);

        if (!optSchedule.hasValue() || !optStatus.hasValue()) {
            printError("Failed retrieve task info. Task may not exist (e.g. was canceled, finished, or never started).");
            return -1;
        }

        auto doc = TextContainer::make(2, 2, 1, 1);
        *doc += TextHeader::make("Task " + toHexString(taskID), '-', TextColor::LightMagenta, TextColor::Magenta);
        *doc += TextBlock::make(optStatus.orDefault().toString(), TextColor::White);
        *doc += TextBlock::make(optSchedule.orDefault().toString(), TextColor::Gray);

        doc->print();
        return 0;
    }
    else if (command == "list") {
        auto address = parseConnectionString(args.expectOptionValue("server"), defaultPort);

		ColoredString("Connecting to task server\n", TextColor::Cyan).print();
		TaskClient client(address.ip, address.port);

        std::set<TaskState> states;
        states.insert(TaskState::Waiting);
        states.insert(TaskState::Ready);
        states.insert(TaskState::Running);
        states.insert(TaskState::Canceling);
        
        ColoredString("Requesting task list\n", TextColor::Cyan).print();
        auto optTasks = client.getTasksByStates(states);

		std::vector<TaskBriefInfo> tasks;
		if (!optTasks.tryGet(tasks)) {
			printError("Task list is not available because the total number of tasks is too large. This command is meant "
				"to be used as a debugging tool for small-scale deployments, not large scale clusters.");
            return -1;
		}
        
        TextHeader::make("Tasks Status")->print();
		printWarning("The status command is meant to be used as a debugging tool for small-scale deployments, not large scale clusters. "
			"This command will (intentionally) fail to succeed when the task server has a large number of tasks.");

        for (auto& task : tasks) {
            auto state = task.status.getState();
            TextColor statusColor, statusColorBright;
            if (state == TaskState::Ready) { statusColorBright = TextColor::LightCyan; statusColor = TextColor::Cyan; }
            else if (state == TaskState::Waiting) { statusColorBright = TextColor::White; statusColor = TextColor::Gray; }
            else if (state == TaskState::Running) { statusColorBright = TextColor::LightGreen; statusColor = TextColor::Green; }
            else if (state == TaskState::Canceling) { statusColorBright = TextColor::LightRed; statusColor = TextColor::Red; }
            else { fail("Unexpected task state from server"); }

            auto str = ColoredString(toHexString(task.id), statusColorBright);
			str += ColoredString(": " + task.status.toString(), statusColor);

            str.print();
			printf("\n");
		}

		if (tasks.size() == 0) {
			ColoredString("No tasks.\n", TextColor::LightCyan).print();
		}
    }
    else if (command == "worker") {
		auto address = parseConnectionString(args.expectOptionValue("server"), defaultPort);
		auto affinities = parseAffinities(args);

		ColoredString("Connecting to task server\n", TextColor::Cyan).print();
		TaskClient client(address.ip, address.port);

		ColoredString("Starting worker\n", TextColor::Cyan).print();
		TaskWorker worker(std::move(client), std::move(affinities));
		worker.run();

		return 0;
    }
    else if (command == "server") {
        std::string portStr = args.getOptionValue("port", std::to_string(defaultPort));
        int port = parseInt(portStr);
        if (port == 0) {
            printError("Invalid port number.");
            return -1;
        }

        TaskServer server(port);
        ColoredString("Server started on port " + std::to_string(port) + "\n", TextColor::LightCyan).print();

        time_t serverStartTime = std::time(nullptr);
        time_t lastStatsPrint = 0;
        const time_t minStatsInterval = 10;

        for (;;) {
            server.processRequest();

            time_t now = std::time(nullptr);
            time_t serverAge = now - serverStartTime;
            time_t timeSinceLastPrint = now - lastStatsPrint;

            if (timeSinceLastPrint >= minStatsInterval) {
                if (lastStatsPrint == 0) { timeSinceLastPrint = now - serverStartTime; }
                ColoredString("\n[+" + std::to_string(timeSinceLastPrint) + "s] ", TextColor::Cyan).print();
                server.getStats().toColoredString().print();
                lastStatsPrint = now;
            }
        }
    }
    else {
        printWarning("Invalid command \"" + command + "\"");
        helpMessage()->print();
        return -1;
    }

    return 0;
}

