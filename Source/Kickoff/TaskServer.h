#pragma once

#include "TaskDatabase.h"
#include "External/zmq.hpp"
#include "Crust/BlobStream.h"
#include "Crust/FormattedText.h"


// The "status" command (TaskRequestTyep::GetStatus) only works when there are fewer than these tasks tracked by the server
static const int MAX_STATUS_TASKS = 100;

// Minimum seconds between the server printing out basic stats (number of requests, etc.)
const int SERVER_STATS_MIN_INTERVAL_SECONDS = 10;

// If a running task hasn't received a heartbeat signal in over 5 minutes, consider the worker "dead" and time it out.
// The worker should always ping the server at least this frequently, so this will only timeout when something very wrong
// has happened to a worker owning a particular task (e.g. it was killed, machine lost power, etc.)
const int WORKER_HEARTBEAT_TIMEOUT_SECONDS = 60 * 5;

// Seconds between checking for and cleaning up running tasks that have timed out
const int SERVER_TASK_CLEANUP_INTERVAL_SECONDS = 60;


enum class TaskRequestType : uint8_t
{
    GetCommand, GetSchedule, GetStatus,
    GetStats, GetTasksByStates,
    Create, TakeToRun, HeartbeatAndCheckWasTaskCanceled,
    MarkFinished, MarkShouldCancel
};

enum class TaskReplyType : uint8_t
{
    BadRequest, Success, Failed
};


struct ServerStats
{
    ServerStats();
    ColoredString toColoredString() const;

    uint64_t succeededRequests;
    uint64_t failedRequests;
    uint64_t badRequests;
};


struct TaskBriefInfo
{
    TaskID id;
    TaskStatus status;

    void serialize(BlobStreamWriter& writer) const;
    bool deserialize(BlobStreamReader& reader);
};

inline BlobStreamWriter& operator<<(BlobStreamWriter& writer, const TaskBriefInfo& val) { val.serialize(writer); return writer; }
inline bool operator>>(BlobStreamReader& reader, TaskBriefInfo& val) { return val.deserialize(reader); }


struct TaskRunInfo
{
    TaskID id;
    PooledString command;

    void serialize(BlobStreamWriter& writer) const;
    bool deserialize(BlobStreamReader& reader);
};

inline BlobStreamWriter& operator<<(BlobStreamWriter& writer, const TaskRunInfo& val) { val.serialize(writer); return writer; }
inline bool operator>>(BlobStreamReader& reader, TaskRunInfo& val) { return val.deserialize(reader); }


class TaskServer
{
public:
    TaskServer(int port);
    void run();
    void shutdown();

private:
    void processRequest();
    BlobStreamWriter generateReply(ArrayView<uint8_t> request);

    TaskDatabase m_db;
    int m_port;
    zmq::context_t m_context;
    zmq::socket_t m_responder;
    ServerStats m_stats;
    volatile bool m_running;
};


class TaskClient
{
public:
    TaskClient(const std::string& ipStr, int port);
    TaskClient(TaskClient&& client);

    Optional<PooledString> getTaskCommand(TaskID id);
    Optional<TaskSchedule> getTaskSchedule(TaskID id);
    Optional<TaskStatus> getTaskStatus(TaskID id);
    Optional<bool> heartbeatAndCheckWasTaskCanceled(TaskID id);
    Optional<std::vector<TaskBriefInfo>> getTasksByStates(const std::set<TaskState>& states);
    Optional<TaskStats> getStats();

    Optional<TaskID> createTask(const TaskCreateInfo& startInfo);
    Optional<TaskRunInfo> takeTaskToRun(const std::vector<std::string>& haveResources);
    bool markTaskFinished(TaskID task); // this should be called whenever a running task finishes, whether or not it was canceled while it was running
    bool markTaskShouldCancel(TaskID task);

private:
    struct ReplyData
    {
        ReplyData() {}
        ReplyData(ReplyData&& other) 
        {
            data = std::move(other.data);
            type = other.type;
            reader = std::move(other.reader);
        }
        
        std::vector<uint8_t> data;
        TaskReplyType type;
        BlobStreamReader reader;

    private:
        ReplyData(const ReplyData& other) {}
    };

    ReplyData getReplyToRequest(const BlobStreamWriter& request);

    zmq::context_t m_context;
    zmq::socket_t m_requester;
};
