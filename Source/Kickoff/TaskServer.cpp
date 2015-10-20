#include "TaskServer.h"
#include "Crust/Array.h"
#include "Crust/Error.h"


static zmq::message_t toMessage(ArrayView<uint8_t> bytes)
{
    zmq::message_t replyMsg(bytes.size());
    memcpy(replyMsg.data(), (const void*)&bytes.first(), bytes.size());
    return std::move(replyMsg);
}

static zmq::message_t toMessage(const BlobStreamWriter& writer)
{
    return std::move(toMessage(writer.data()));
}

static ArrayView<uint8_t> viewMessage(const zmq::message_t& msg)
{
    return ArrayView<uint8_t>((const uint8_t*)msg.data(), (int)msg.size());
}


TaskServer::TaskServer(int port)
    : m_port(port)
    , m_context(1)
    , m_responder(m_context, ZMQ_REP)
    , m_running(false)
{
    try {
        m_responder.bind("tcp://*:" + std::to_string(m_port));
    }
    catch (zmq::error_t) {
        printError("Failed to start server on port " + std::to_string(m_port) + "!");
        exit(-1);
    }
}

void TaskServer::processRequest()
{
    // Get a request
    zmq::message_t request;
    m_responder.recv(&request);

    // Generate a reply, and track statistics about the reply type
    auto reply = generateReply(viewMessage(request));

    TaskReplyType type = *(TaskReplyType*)(&reply.data().first());
    if (type == TaskReplyType::Success) { m_stats.succeededRequests++; }
    else if (type == TaskReplyType::Failed) { m_stats.failedRequests++; }
    else if (type == TaskReplyType::BadRequest) { m_stats.badRequests++; }

    // Send the reply
    m_responder.send(toMessage(reply));
}

void TaskServer::run()
{
    ColoredString("Server running on port " + std::to_string(m_port) + "\n", TextColor::LightCyan).print();

    time_t serverStartTime = std::time(nullptr);
    time_t lastStatsPrint = 0, lastCleanup = 0;

    m_running = true;
    while (m_running) {
        processRequest();

        time_t now = std::time(nullptr);
        time_t serverAge = now - serverStartTime;
        time_t timeSinceLastPrint = now - lastStatsPrint;
        time_t timeSinceLastCleanup = now - lastCleanup;

        if (timeSinceLastPrint >= SERVER_STATS_MIN_INTERVAL_SECONDS) {
            if (lastStatsPrint == 0) { timeSinceLastPrint = now - serverStartTime; }
            ColoredString("\n[+" + std::to_string(timeSinceLastPrint) + "s] ", TextColor::Cyan).print();
            m_stats.toColoredString().print();
            lastStatsPrint = now;
        }

        if (timeSinceLastCleanup >= SERVER_TASK_CLEANUP_INTERVAL_SECONDS) {
            m_db.cleanupZombieTasks(WORKER_HEARTBEAT_TIMEOUT_SECONDS);
            lastCleanup = now;
        }
    }
}

void TaskServer::shutdown()
{
    ColoredString("Shutting down server\n", TextColor::LightYellow).print();
    m_running = true;
}

BlobStreamWriter TaskServer::generateReply(ArrayView<uint8_t> requestBytes)
{
    BlobStreamReader request(requestBytes);
    BlobStreamWriter reply;

    TaskRequestType type;
    if (!(request >> type)) {
        reply << TaskReplyType::BadRequest;
        return reply;
    }

    switch (type) {
        case TaskRequestType::GetCommand: {
            TaskID id;
            if (!(request >> id)) { break; }
            auto task = m_db.getTaskByID(id);

            if (!task) {
                reply << TaskReplyType::Failed;
            }
            else {
                reply << TaskReplyType::Success;
                reply << task->getCommand();
            }
            return reply;
        }

        case TaskRequestType::GetSchedule: {
            TaskID id;
            if (!(request >> id)) { break; }
            auto task = m_db.getTaskByID(id);

            if (!task) {
                reply << TaskReplyType::Failed;
            }
            else {
                reply << TaskReplyType::Success;
                reply << task->getSchedule();
            }
            return reply;
        }

        case TaskRequestType::GetStatus: {
            TaskID id;
            if (!(request >> id)) { break; }
            auto task = m_db.getTaskByID(id);

            if (!task) {
                reply << TaskReplyType::Failed;
            }
            else {
                reply << TaskReplyType::Success;
                reply << task->getStatus();
            }
            return reply;
        }

        case TaskRequestType::GetStats: {
            if (request.hasMore()) { break; }

            reply << TaskReplyType::Success;
            reply << m_db.getStats();

            return reply;
        }

        case TaskRequestType::HeartbeatAndCheckWasTaskCanceled: {
            TaskID id;
            if (!(request >> id)) { break; }
            auto task = m_db.getTaskByID(id);

            if (!task) {
                reply << TaskReplyType::Failed;
            }
            else {
                reply << TaskReplyType::Success;
                m_db.heartbeatTask(task);

                bool wasCanceled = false;
                if (const auto* runStatus = task->getStatus().runStatus.ptrOrNull()) {
                    if (runStatus->wasCanceled) { wasCanceled = true; }
                }

                reply << wasCanceled;
            }
            return reply;
        }

        case TaskRequestType::GetTasksByStates: {
            if (m_db.getTotalTaskCount() > MAX_STATUS_TASKS) {
                reply << TaskReplyType::Failed;
                return reply;
            }

            std::set<TaskState> states;
            TaskState state;
            while (request >> state) {
                states.insert(state);
            }

            auto tasks = m_db.getTasksByStates(states);

            reply << TaskReplyType::Success;
            for (auto task : tasks) {
                TaskBriefInfo info;
                info.id = task->getID();
                info.status = task->getStatus();
                reply << info;
            }

            return reply;
        }

        case TaskRequestType::Create: {
            TaskCreateInfo startInfo;
            if (!(request >> startInfo)) { break; }

            auto newTask = m_db.createTask(startInfo);
            if (!newTask) {
                reply << TaskReplyType::Failed;
            }
            else {
                reply << TaskReplyType::Success;
                reply << newTask->getID();
            }
            return reply;
        }

        case TaskRequestType::TakeToRun: {
            std::vector<std::string> haveResources;
            std::string resource;
            while (request >> resource) {
                haveResources.push_back(resource);
            }

            auto task = m_db.takeTaskToRun(haveResources);
            if (task) {
                TaskRunInfo info;
                info.id = task->getID();
                info.command = task->getCommand();

                reply << TaskReplyType::Success;
                reply << info;
            }
            else {
                reply << TaskReplyType::Failed;
            }
            return reply;
        }

        case TaskRequestType::MarkFinished: {
            TaskID id;
            if (!(request >> id)) { break; }

            auto task = m_db.getTaskByID(id);
            if (task) {
                m_db.markTaskFinished(task);
                reply << TaskReplyType::Success;
            }
            else {
                reply << TaskReplyType::Failed;
            }
            return reply;
        }

        case TaskRequestType::MarkShouldCancel: {
            TaskID id;
            if (!(request >> id)) { break; }

            auto task = m_db.getTaskByID(id);
            if (task) {
                m_db.markTaskShouldCancel(task);
                reply << TaskReplyType::Success;
            }
            else {
                reply << TaskReplyType::Failed;
            }
            return reply;
        }
    }

    reply << TaskReplyType::BadRequest;
    return reply;
}


TaskClient::TaskClient(const std::string& ipStr, int port)
    : m_context(1)
    , m_requester(m_context, ZMQ_REQ)
{
    std::string connStr = "tcp://" + ipStr + ":" + std::to_string(port);
    try {
        m_requester.connect(connStr.c_str());
    }
    catch (zmq::error_t) {
        printError("Failed to connect to task server: \"" + connStr + "\"");
        exit(-1);
    }
}

TaskClient::TaskClient(TaskClient&& client)
    : m_context(std::move(client.m_context))
    , m_requester(std::move(client.m_requester))
{

}

TaskClient::ReplyData TaskClient::getReplyToRequest(const BlobStreamWriter& request)
{
    m_requester.send(toMessage(request));
    zmq::message_t replyMsg;
    m_requester.recv(&replyMsg);

    ReplyData replyData;
    replyData.data.resize(replyMsg.size());
    MutableArrayView<uint8_t>(replyData.data).copyFrom(viewMessage(replyMsg));
    replyData.reader = BlobStreamReader(replyData.data);

    if (!(replyData.reader >> replyData.type)) {
        replyData.type = TaskReplyType::Failed;
    }
    return std::move(replyData);
}

Optional<PooledString> TaskClient::getTaskCommand(TaskID id)
{
    BlobStreamWriter request;
    request << TaskRequestType::GetCommand;
    request << id;

    ReplyData reply = getReplyToRequest(request);
    if (reply.type == TaskReplyType::Success) {
        PooledString val;
        if (reply.reader >> val) {
            return std::move(val);
        }
    }

    return Nothing();
}

Optional<TaskSchedule> TaskClient::getTaskSchedule(TaskID id)
{
    BlobStreamWriter request;
    request << TaskRequestType::GetSchedule;
    request << id;

    ReplyData reply = getReplyToRequest(request);
    if (reply.type == TaskReplyType::Success) {
        TaskSchedule val;
        if (reply.reader >> val) {
            return std::move(val);
        }
    }

    return Nothing();
}

Optional<TaskStatus> TaskClient::getTaskStatus(TaskID id)
{
    BlobStreamWriter request;
    request << TaskRequestType::GetStatus;
    request << id;

    ReplyData reply = getReplyToRequest(request);
    if (reply.type == TaskReplyType::Success) {
        TaskStatus status;
        if (reply.reader >> status) {
            return std::move(status);
        }
    }

    return Nothing();
}

Optional<bool> TaskClient::heartbeatAndCheckWasTaskCanceled(TaskID id)
{
    BlobStreamWriter request;
    request << TaskRequestType::HeartbeatAndCheckWasTaskCanceled;
    request << id;

    ReplyData reply = getReplyToRequest(request);
    if (reply.type == TaskReplyType::Success) {
        bool wasCanceled;
        if (reply.reader >> wasCanceled) {
            return std::move(wasCanceled);
        }
    }

    return Nothing();
}

Optional<std::vector<TaskBriefInfo>> TaskClient::getTasksByStates(const std::set<TaskState>& states)
{
    BlobStreamWriter request;
    request << TaskRequestType::GetTasksByStates;
    for (auto state : states) {
        request << state;
    }

    std::vector<TaskBriefInfo> tasks;

    ReplyData reply = getReplyToRequest(request);
    if (reply.type == TaskReplyType::Success) {
        TaskBriefInfo info;
        while (reply.reader >> info) {
            tasks.push_back(info);
        }
    }
    else {
        return Nothing();
    }

    return tasks;
}

Optional<TaskStats> TaskClient::getStats()
{
    BlobStreamWriter request;
    request << TaskRequestType::GetStats;

    ReplyData reply = getReplyToRequest(request);
    if (reply.type == TaskReplyType::Success) {
        TaskStats stats;
        if (reply.reader >> stats) {
            return stats;
        }
    }
    return Nothing();
}

Optional<TaskID> TaskClient::createTask(const TaskCreateInfo& startInfo)
{
    BlobStreamWriter request;
    request << TaskRequestType::Create;
    request << startInfo;

    ReplyData reply = getReplyToRequest(request);
    if (reply.type == TaskReplyType::Success) {
        TaskID id;
        if (reply.reader >> id) {
            return id;
        }
    }

    return Nothing();
}

Optional<TaskRunInfo> TaskClient::takeTaskToRun(const std::vector<std::string>& haveResources)
{
    BlobStreamWriter request;
    request << TaskRequestType::TakeToRun;
    for (const auto& resource : haveResources) {
        request << resource;
    }

    ReplyData reply = getReplyToRequest(request);
    if (reply.type == TaskReplyType::Success) {
        TaskRunInfo info;
        if (reply.reader >> info) {
            return info;
        }
    }

    return Nothing();
}

bool TaskClient::markTaskFinished(TaskID task)
{
    BlobStreamWriter request;
    request << TaskRequestType::MarkFinished;
    request << task;

    ReplyData reply = getReplyToRequest(request);
    return (reply.type == TaskReplyType::Success);
}

bool TaskClient::markTaskShouldCancel(TaskID task)
{
    BlobStreamWriter request;
    request << TaskRequestType::MarkShouldCancel;
    request << task;

    ReplyData reply = getReplyToRequest(request);
    return (reply.type == TaskReplyType::Success);
}

ServerStats::ServerStats()
    : succeededRequests(0)
    , failedRequests(0)
    , badRequests(0)
{
}

ColoredString ServerStats::toColoredString() const
{
    return ColoredString(std::to_string(succeededRequests), TextColor::LightGreen) +
        ColoredString(" requests successfully served; ", TextColor::Green) +
        ColoredString(std::to_string(failedRequests), TextColor::LightYellow) +
        ColoredString(" failed; ", TextColor::Yellow) +
        ColoredString(std::to_string(badRequests), TextColor::LightRed) +
        ColoredString(" bad/corrupt.", TextColor::Red);
}

void TaskBriefInfo::serialize(BlobStreamWriter& writer) const
{
    writer << id;
    writer << status;
}

bool TaskBriefInfo::deserialize(BlobStreamReader& reader)
{
    if (!(reader >> id)) { return false; }
    if (!(reader >> status)) { return false; }
    return true;
}

void TaskRunInfo::serialize(BlobStreamWriter& writer) const
{
    writer << id;
    writer << command;
}

bool TaskRunInfo::deserialize(BlobStreamReader& reader)
{
    if (!(reader >> id)) { return false; }
    if (!(reader >> command)) { return false; }
    return true;
}
