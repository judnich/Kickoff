#include "TaskDatabase.h"
#include "Crust/Util.h"
#include "Crust/Error.h"


TaskStats::TaskStats()
    : numPending(0)
    , numRunning(0)
    , numFinished(0)
{}


Task::Task(TaskID id, const TaskCreateInfo& startInfo)
    : m_id(id)
    , m_executable(startInfo.executable)
    , m_schedule(startInfo.schedule)
{
    m_status.createTime = std::time(nullptr);
}

std::string Task::getHexID() const
{
    return toHexString(ArrayView<uint8_t>(reinterpret_cast<const uint8_t*>(&m_id), sizeof(m_id)));
}

void Task::markStarted()
{
    if (!m_status.runStatus.hasValue()) {
        TaskRunStatus runStatus;
        runStatus.startTime = std::time(nullptr);
        runStatus.heartbeatTime = std::time(nullptr);
        runStatus.wasCanceled = false;
        m_status.runStatus = runStatus;
    }
}

bool Task::markShouldCancel()
{
    if (auto* runStatus = m_status.runStatus.tryGet()) {
        runStatus->wasCanceled = true;
        return true;
    }
    return false;
}

void Task::heartbeat()
{
    // Update a running task's heartbeat timestamp
    if (auto* runStatus = m_status.runStatus.tryGet()) {
        runStatus->heartbeatTime = std::time(nullptr);
    }
}

TaskPtr TaskDatabase::getTaskByID(TaskID id) const
{
    auto it = m_allTasks.find(id);
    if (it != m_allTasks.end()) {
        return it->second;
    }
    return TaskPtr();
}

std::vector<TaskPtr> TaskDatabase::getTasksByStates(const std::set<TaskState>& states) const
{
    std::vector<TaskPtr> results;
    for (const auto& entry : m_allTasks) {
        TaskPtr task = entry.second;
        if (states.find(task->getStatus().getState()) != states.end()) {
            results.push_back(task);
        }
    }
    return std::move(results);
}

int TaskDatabase::getTotalTaskCount() const
{
    return (int)m_allTasks.size();
}

static uint64_t fastRand64()
{
    uint64_t val = static_cast<uint64_t>(clock());
    for (int i = 0; i < 16; ++i) {
        val ^= static_cast<uint64_t>(rand() & 0xFFFF) << (i * 16);
    }
    return val;
}

TaskID TaskDatabase::getUnusedTaskID() const
{
    TaskID randID = fastRand64();
    
    int sanityCount = 0;
    while (getTaskByID(randID)) {
        randID = fastRand64();

        sanityCount++;
        if (sanityCount > 10) {
            printWarning("TaskDatabase::getUnusedTaskID is taking unusually long to find an empty slot!");
        }
        else if (sanityCount > 1000) {
            fail("TaskDatabase::getUnusedTaskID failed to find an empty slot after 1000 iterations!");
        }
    }
    return randID;
}

TaskPtr TaskDatabase::createTask(const TaskCreateInfo& info)
{
    TaskID id = getUnusedTaskID();
    TaskPtr task = std::make_shared<Task>(id, info);
    m_allTasks[id] = task;

    for (const auto& affinity : task->getSchedule().affinities) {
        m_readyTasksPerAffinity[affinity].insert(task->getID());
    }

    m_stats.numPending++;
    return task;
}

TaskPtr TaskDatabase::takeTaskToRun(const std::vector<std::string>& affinities)
{
    // Try each affinity that could match one by one, starting at a random one. This randomness is necessary so
    // that tasks don't end up being dequeued with a preference for alphabetical order in their affinities.
    Optional<TaskID> foundID;
    int offset = rand() % (int)affinities.size();
    for (int i = 0; i < (int)affinities.size(); ++i) {
        const auto& affinity = affinities[(i + offset) % (int)affinities.size()];
        auto& readyTasks = m_readyTasksPerAffinity[affinity];
        if (!readyTasks.empty()) {
            foundID = *readyTasks.begin();
            break;
        }

        i = (i + 1) % (int)affinities.size();
    }

    // If a ready task was found, update its state
    TaskPtr readyTask;
    TaskID readyTaskID;
    if (foundID.tryGet(readyTaskID)) {
        readyTask = getTaskByID(readyTaskID);
        assert(readyTask);

        m_stats.numPending--;
        m_stats.numRunning++;

        // Now that a ready task was found, remove it from the "ready tasks" sets
        for (auto& readyTasks : m_readyTasksPerAffinity) {
            readyTasks.second.erase(readyTask->getID());
        }

        // Then finally set it as running
        readyTask->markStarted();
    }

    return readyTask;
}

void TaskDatabase::heartbeatTask(TaskPtr task)
{
    return task->heartbeat();
}

void TaskDatabase::markTaskFinished(TaskPtr task)
{
    if (auto* runStatus = task->getStatus().runStatus.tryGet()) {
        if (runStatus->wasCanceled) {
            m_stats.numCanceling--;
        }
        else {
            m_stats.numRunning--;
        }
        m_stats.numFinished++;
    }

    for (const auto& affinity : task->getSchedule().affinities) {
        m_readyTasksPerAffinity[affinity].erase(task->getID());
    }
    m_allTasks.erase(task->getID());
}

void TaskDatabase::markTaskShouldCancel(TaskPtr task)
{
    if (task->markShouldCancel()) {
        m_stats.numRunning--;
        m_stats.numCanceling--;
    }
}

void TaskDatabase::cleanupZombieTasks(std::time_t heartbeatTimeoutSeconds)
{
    for (auto item : m_allTasks) {
        auto task = item.second;
        cleanupIfZombieTask(task, heartbeatTimeoutSeconds);
    }
}

bool TaskDatabase::cleanupIfZombieTask(TaskPtr task, std::time_t heartbeatTimeoutSeconds)
{
    bool died = false;

    if (auto* runStatus = task->getStatus().runStatus.tryGet()) {
        std::time_t diff = std::time(nullptr) - runStatus->heartbeatTime;
        died = (diff >= heartbeatTimeoutSeconds);
    }

    if (died) {
        markTaskFinished(task);
    }
    return died;
}


TaskState TaskStatus::getState() const
{
    TaskRunStatus runStatusVal;
    if (runStatus.tryGet(runStatusVal)) {
        return runStatusVal.wasCanceled ? TaskState::Canceling : TaskState::Running;
    }
    else {
        return TaskState::Pending;
    }
}


void TaskExecutable::serialize(BlobStreamWriter& writer) const
{
    writer << interpreter;
    writer << script;
    writer << args;
}

bool TaskExecutable::deserialize(BlobStreamReader& reader)
{
    if (!(reader >> interpreter)) { return false; }
    if (!(reader >> script)) { return false; }
    if (!(reader >> args)) { return false; }
    return true;
}


void TaskSchedule::serialize(BlobStreamWriter& writer) const
{
    writer << affinities.size();
    for (auto& affin : affinities) {
        writer << affin;
    }
}

bool TaskSchedule::deserialize(BlobStreamReader& reader)
{
    size_t count;

    if (!(reader >> count)) { return false; }
    affinities.resize(count);
    for (size_t i = 0; i < count; ++i) {
        if (!(reader >> affinities[i])) { return false; }
    }

    return true;
}


std::string TaskSchedule::toString() const
{
    std::string str = "Affinities = {";
    for (size_t i = 0; i < affinities.size(); ++i) {
        str += affinities[i].get();
        if (i != affinities.size() - 1) {
            str += ", ";
        }
    }
    str += "}";
    return str;
}

void TaskCreateInfo::serialize(BlobStreamWriter& writer) const
{
    executable.serialize(writer);
    schedule.serialize(writer);
}

bool TaskCreateInfo::deserialize(BlobStreamReader& reader)
{
    if (!executable.deserialize(reader)) { return false; }
    if (!schedule.deserialize(reader)) { return false; }
    return true;
}


void TaskRunStatus::serialize(BlobStreamWriter& writer) const
{
    writer << wasCanceled;
    writer << startTime;
    writer << heartbeatTime;
}

bool TaskRunStatus::deserialize(BlobStreamReader& reader)
{
    if (!(reader >> wasCanceled)) { return false; }
    if (!(reader >> startTime)) { return false; }
    if (!(reader >> heartbeatTime)) { return false; }
    return true;
}


void TaskStatus::serialize(BlobStreamWriter& writer) const
{
    writer << createTime;

    TaskRunStatus runStatusVal;
    if (runStatus.tryGet(runStatusVal)) {
        writer << true;
        runStatusVal.serialize(writer);
    }
    else {
        writer << false;
    }
}

bool TaskStatus::deserialize(BlobStreamReader& reader)
{
    if (!(reader >> createTime)) { return false; }

    bool hasRunStatus;
    if (!(reader >> hasRunStatus)) { return false; }
    if (hasRunStatus) {
        TaskRunStatus runStatusVal;
        if (!runStatusVal.deserialize(reader)) { return false; }
        runStatus = runStatusVal;
    }
    else {
        runStatus = Nothing();
    }

    return true;
}

static std::string intervalToString(time_t interval)
{
    int seconds = interval % 60;
    interval /= 60;

    int minutes = interval % 60;
    interval /= 60;
    
    int hours = interval % 24;
    interval /= 24;

    int days = (int)interval;

    return
        (days > 0 ? (std::to_string(days) + "d") : "") +
        (hours > 0 ? (std::to_string(hours) + "h") : "") +
        (minutes > 0 ? (std::to_string(minutes) + "m") : "") +
        (std::to_string(seconds) + "s");
}

std::string toString(TaskState state)
{
    if (state == TaskState::Pending) { return "Pending"; }
    else if (state == TaskState::Running) { return "Running"; }
    else if (state == TaskState::Canceling) { return "Canceling"; }
    return "<Invalid TaskState>";
}

std::string TaskStatus::toString() const
{
    TaskRunStatus runStatusVal = runStatus.orDefault();
    std::time_t nowTime = std::time(nullptr);

    std::string str;
    switch (getState()) {
        case TaskState::Pending:
            str += "Pending (so far waited " + intervalToString(nowTime - createTime) + ")";
            break;
        case TaskState::Running:
            str += "Running (current runtime " + intervalToString(nowTime - runStatusVal.startTime) + "; worker heartbeat " + intervalToString(nowTime - runStatusVal.heartbeatTime) + ")";
            break;
        case TaskState::Canceling:
            str += "Canceling (current runtime " + intervalToString(nowTime - runStatusVal.startTime) + "; worker heartbeat " + intervalToString(nowTime - runStatusVal.heartbeatTime) + ")";
            break;
    }

    return str;
}
