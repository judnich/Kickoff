#pragma once

#include <vector>
#include <cstdint>
#include <ctime>
#include <map>
#include <set>
#include "Crust/Array.h"
#include "Crust/Algebraic.h"
#include "Crust/PooledString.h"
#include "Crust/PooledBlob.h"
#include "Crust/BlobStream.h"
#include "Crust/FormattedText.h"


class Task;
typedef std::shared_ptr<Task> TaskPtr;
typedef std::weak_ptr<Task> TaskWeakPtr;
class TaskDatabase;


typedef uint64_t TaskID;


// When a task is run, the command: "<interpreter> <script_file> <args>" will be executed,
// where <script_file> is a file containing the provided script data blob.
struct TaskExecutable
{
    PooledString interpreter; // the executable name of the interpreter to run the script, e.g. python, sh, etc.
    PooledBlob script; // the contents of a text script to pass to the interpreter
    PooledString args; // arguments (e.g. usually to specify per-task variations) to pass to the script

    void serialize(BlobStreamWriter& writer) const;
    bool deserialize(BlobStreamReader& reader);
};

inline BlobStreamWriter& operator<<(BlobStreamWriter& writer, const TaskExecutable& val) { val.serialize(writer); return writer; }
inline bool operator>>(BlobStreamReader& reader, TaskExecutable& val) { return val.deserialize(reader); }

// This encapsulates all the information on when/where to run a task
struct TaskSchedule
{
    std::vector<PooledString> affinities; // "affinity" strings that describe workers compatible with this task
    std::vector<TaskID> dependencies; // a list of tasks that must complete before this task may run

    void serialize(BlobStreamWriter& writer) const;
    bool deserialize(BlobStreamReader& reader);

    std::string toString() const;
};

inline BlobStreamWriter& operator<<(BlobStreamWriter& writer, const TaskSchedule& val) { val.serialize(writer); return writer; }
inline bool operator>>(BlobStreamReader& reader, TaskSchedule& val) { return val.deserialize(reader); }

// These task states are simply conveniences for the user when inspecting a Task object. Internal state is NOT
// stored via a TaskState value, but with Optional<TaskRunStatus> data and Task::m_unsatisfiedDependencies
// (purely used to determine TaskState::Waiting vs TaskState::Ready).
enum class TaskState : uint8_t
{
    Waiting, Ready, Running, Canceling, Canceled, Completed
};

std::string toString(TaskState state);

// This struct describes status information for tasks that are NOT pending; e.g. either running, or finished.
struct TaskRunStatus
{
    // This identifies the worker that is running this task, and how to contact it (via an 'ip:port' connection string)
    PooledString workerName;
    // This does NOT mean the task has finished, just that it was marked for cancellation
    bool wasCanceled;
    // This marks the time the task started running on the worker that dequeued it
    std::time_t startTime;
    // If set, this marks the time when the task completed (either naturally or via cancellation). If not set, then the task is still running.
    Optional<std::time_t> endTime;

    void serialize(BlobStreamWriter& writer) const;
    bool deserialize(BlobStreamReader& reader);
};

inline BlobStreamWriter& operator<<(BlobStreamWriter& writer, const TaskRunStatus& val) { val.serialize(writer); return writer; }
inline bool operator>>(BlobStreamReader& reader, TaskRunStatus& val) { return val.deserialize(reader); }

// This struct describes the runtime status of a task, i.e. when it was enqueued, when it started running (if it has), etc.
struct TaskStatus
{
    std::time_t createTime; // has no functional effect on task execution
    Optional<TaskRunStatus> runStatus; // if no value exists, then the task is still pending
    int unsatisfiedDependencies; // counts how many of this task's dependency tasks are still unfinished
    
    bool areDependenciesSatisfied() const { return unsatisfiedDependencies == 0; }
    TaskState getState() const; // this classifies the task into several disjoint states; see TaskState

    void serialize(BlobStreamWriter& writer) const;
    bool deserialize(BlobStreamReader& reader);

    std::string toString() const;
};

inline BlobStreamWriter& operator<<(BlobStreamWriter& writer, const TaskStatus& val) { val.serialize(writer); return writer; }
inline bool operator>>(BlobStreamReader& reader, TaskStatus& val) { return val.deserialize(reader); }

// This is a simple structure to group together all the information needed to start a task
struct TaskCreateInfo
{
    TaskExecutable executable;
    TaskSchedule schedule;
    PooledString description;

    void serialize(BlobStreamWriter& writer) const;
    bool deserialize(BlobStreamReader& reader);
};

inline BlobStreamWriter& operator<<(BlobStreamWriter& writer, const TaskCreateInfo& val) { val.serialize(writer); return writer; }
inline bool operator>>(BlobStreamReader& reader, TaskCreateInfo& val) { return val.deserialize(reader); }

class TaskDB;

// Main task class not only provides (private, shared only with TaskDatabase) methods to change task run state 
// information, but tracks additional data about graph connectivity to enable knowing when a task becomes ready 
// in constant time.
class Task : public std::enable_shared_from_this<Task>
{
public:
    ~Task();
    Task() {} // do not use manually, use TaskDatabase::createTask() instead
    static TaskPtr create(TaskID id, const TaskCreateInfo& startInfo, const TaskDatabase& db); // called from TaskDatabase::createTask(). use that instead.
    
    TaskID getID() const { return m_id; }
    std::string getHexID() const;
    
    const TaskExecutable& getExecutable() const { return m_executable; }
    const TaskSchedule& getSchedule() const { return m_schedule; }
    const PooledString& getDescription() const { return m_description; }
    const TaskStatus& getStatus() const { return m_status; }

private:
    friend class TaskDatabase;

    TaskID m_id;
    TaskExecutable m_executable; // what to execute by the worker
    TaskSchedule m_schedule; // where and when to run the task
    PooledString m_description; // has no functional effect on task execution
    TaskStatus m_status;

    std::vector<TaskWeakPtr> m_descendants;
    std::vector<TaskPtr> m_dependencies; // cached strong pointers to children (vs just IDs, as stored in m_config.schedule.dependencies)

    void markStarted(const std::string& workerName);
    void markShouldCancel(TaskDatabase& callback);
    void markFinished(TaskDatabase& callback);
};


typedef std::map<TaskID, TaskPtr> TasksByID;

class TaskDatabase
{
public:
    TaskPtr getTaskByID(TaskID id) const;
    std::vector<TaskPtr> getTasksByStates(const std::set<TaskState>& states) const;

    TaskPtr createTask(const TaskCreateInfo& startInfo);
    TaskPtr takeTaskToRun(const std::string& workerName, const std::vector<std::string>& affinities);
    void markTaskFinished(TaskPtr task); // this should be called whenever a running task finishes, whether or not it was canceled while it was running
    void markTaskShouldCancel(TaskPtr task);

private:
    friend class Task;

    TaskID getUnusedTaskID() const;
    void notifyTaskReady(TaskPtr task);
    void notifyTaskCompleted(TaskPtr task);

    std::map<PooledString, std::set<TaskID>> m_readyTasksPerAffinity;
    TasksByID m_allTasks;
};

