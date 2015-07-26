#pragma once

#include "TaskDatabase.h"
#include "External/zmq.hpp"
#include "Crust/BlobStream.h"
#include "Crust/FormattedText.h"

enum class TaskRequestType : uint8_t
{
    GetDescription, GetExecutable, GetSchedule,
    GetStatus, WasTaskCanceled, GetTasksByStates,
    Create, TakeToRun,
    MarkFinished, MarkShouldCancel, Delete
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
    static const int MAX_DESC_LEN = 128;
    TaskID id;
    TaskStatus status;
    std::string description; // truncated to MAX_DESC_LEN

    void serialize(BlobStreamWriter& writer) const;
    bool deserialize(BlobStreamReader& reader);
};

inline BlobStreamWriter& operator<<(BlobStreamWriter& writer, const TaskBriefInfo& val) { val.serialize(writer); return writer; }
inline bool operator>>(BlobStreamReader& reader, TaskBriefInfo& val) { return val.deserialize(reader); }

struct TaskRunInfo
{
    TaskID id;
    TaskExecutable executable;

    void serialize(BlobStreamWriter& writer) const;
    bool deserialize(BlobStreamReader& reader);
};

inline BlobStreamWriter& operator<<(BlobStreamWriter& writer, const TaskRunInfo& val) { val.serialize(writer); return writer; }
inline bool operator>>(BlobStreamReader& reader, TaskRunInfo& val) { return val.deserialize(reader); }

class TaskServer
{
public:
    TaskServer(int port);
    void processRequest();
    const ServerStats& getStats() const { return m_stats; }

private:
    BlobStreamWriter generateReply(ArrayView<uint8_t> request);

    TaskDatabase m_db;
    zmq::context_t m_context;
    zmq::socket_t m_responder;
    ServerStats m_stats;
};

class TaskClient
{
public:
    TaskClient(const std::string& ipStr, int port);
    TaskClient(TaskClient&& client);

    Optional<std::string> getTaskDescription(TaskID id);
    Optional<TaskExecutable> getTaskExecutable(TaskID id);
    Optional<TaskSchedule> getTaskSchedule(TaskID id);
    Optional<TaskStatus> getTaskStatus(TaskID id);
	Optional<bool> wasTaskCanceled(TaskID id);
    std::vector<TaskBriefInfo> getTasksByStates(const std::set<TaskState>& states);

    Optional<TaskID> createTask(const TaskCreateInfo& startInfo);
    Optional<TaskRunInfo> takeTaskToRun(const std::vector<std::string>& affinities);
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
