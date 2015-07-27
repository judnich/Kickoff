#include "TaskDatabase.h"
#include "Crust/Util.h"
#include "Crust/Error.h"


TaskStats::TaskStats()
    : numWaiting(0)
    , numReady(0)
    , numRunning(0)
    , numFinished(0)
{}


TaskPtr Task::create(TaskID id, const TaskCreateInfo& startInfo, const TaskDatabase& db)
{
    TaskPtr newTask = std::make_shared<Task>();
    newTask->m_id = id;
    newTask->m_executable = startInfo.executable;
    newTask->m_schedule = startInfo.schedule;
    newTask->m_status.createTime = std::time(nullptr);

    // For each task this task depends on, keep track of dependencies (parents) and descendants (children)
    newTask->m_status.unsatisfiedDependencies = 0;
    for (auto depTaskID : newTask->m_schedule.dependencies) {
        auto depTask = db.getTaskByID(depTaskID);
        if (depTask) {
            newTask->m_dependencies.push_back(depTask);

            // Register the new task as a "descendant" of this dependency (parent) task.
            // This information is used later when a task is marked as finished, so nodes know when
            // all their dependencies are satisfied without having to constantly poll all of them.
            // It's also used when a task is canceled so all tasks depending on it are also canceled.
            depTask->m_descendants.push_back(newTask);

            // Also, if this dependency has not yet completed, count it
            TaskRunStatus runStatus;
            if (depTask->m_status.runStatus.tryGet(runStatus)) {
                if (!runStatus.endTime.hasValue()) {
                    newTask->m_status.unsatisfiedDependencies++;
                }
            }
        }
    }

    return newTask;
}

Task::~Task()
{
    // Un-register this task as a descendant for each of its dependencies
    for (auto depTask : m_dependencies) {
        // Find this task ID in the dependency's list of descendants
        int index = -1;
        for (int i = 0; i < (int)depTask->m_descendants.size(); ++i) {
            auto descendant = depTask->m_descendants[i].lock();
            if (descendant && descendant->getID() == getID()) {
                index = i;
                break;
            }
        }

        // Delete it (if it exists; otherwise assert fail)
        if (index >= 0) {
            depTask->m_descendants[index] = depTask->m_descendants.back();
            depTask->m_descendants.pop_back();
        }
        else {
            assert(false); // forward vs backwards DAG links became inconsistent
        }
    }
}

std::string Task::getHexID() const
{
    return toHexString(ArrayView<uint8_t>(reinterpret_cast<const uint8_t*>(&m_id), sizeof(m_id)));
}

bool Task::markStarted(const std::string& workerName)
{
    if (m_status.runStatus.hasValue()) {
        return false; // already started
    }

    TaskRunStatus runStatus;
    runStatus.workerName = workerName;
    runStatus.startTime = std::time(nullptr);
    runStatus.heartbeatTime = std::time(nullptr);
    runStatus.wasCanceled = false;
    m_status.runStatus = runStatus;

    return true;
}

bool Task::markShouldCancel(TaskDatabase& callback)
{
    TaskRunStatus runStatus;
    if (m_status.runStatus.tryGet(runStatus)) {
        // If the task is already canceled, no need to do anything here
        if (runStatus.wasCanceled) { return false; }
    }
    else {
        // If this is a pending task, mark it as finished immediately
        runStatus.workerName = "";
        runStatus.startTime = 0;
        runStatus.heartbeatTime = 0;
        runStatus.endTime = 0;
        callback.notifyTaskCompleted(shared_from_this());
    }

    // Set canceled flag, which indicates that this task was canceled (or if it's running, that it needs to be killed)
    runStatus.wasCanceled = true;

    // Save run status changes
    m_status.runStatus = runStatus;

    // Also cancel any descendants, since their dependencies can no longer be satisfied
    for (auto weakPtr : m_descendants) {
        TaskPtr task = weakPtr.lock();
        if (task) {
            task->markShouldCancel(callback);
        }
    }

    return true;
}

bool Task::heartbeat()
{
    // Update a running task's heartbeat timestamp
    bool success = true;
    success &= m_status.runStatus.tryUnwrap(
        [&success](TaskRunStatus& runStatus) {
        if (runStatus.endTime.hasValue()) {
            success = false; // already finished
        }
        else {
            runStatus.heartbeatTime = std::time(nullptr);
        }
    });

    return success;
}

bool Task::markFinished(TaskDatabase& callback)
{
    TaskRunStatus runStatus;
    if (m_status.runStatus.tryGet(runStatus)) {
        // Marking a running task as finished
        if (runStatus.endTime.hasValue()) {
            return false;
        }
        auto nowTime = std::time(nullptr);
        runStatus.endTime = nowTime;
        runStatus.heartbeatTime = nowTime;
    }
    else {
        // Marking a pending task as finished
        runStatus.workerName = "";
        runStatus.startTime = 0;
        runStatus.heartbeatTime = 0;
        runStatus.endTime = 0;
    }
    m_status.runStatus = runStatus;

    // Decrement the count of unsatisfied dependencies for each descendant, since this task is now done.
    // Note that even if this is called as the result of a task having been canceled, this behavior is
    // still okay because canceling a task also recursively cancels all of its dependent tasks.
    bool success = true;
    for (auto weakPtr : m_descendants) {
        auto descendant = weakPtr.lock();
        if (descendant) {
            descendant->m_status.unsatisfiedDependencies--;
            runtimeAssert(descendant->m_status.unsatisfiedDependencies >= 0, "Unsatisfied dependency count is negative!");

            // If the unsatisfied dependency count for this descendant reaches 0, the task is ready to be executed
            if (descendant->m_status.unsatisfiedDependencies == 0) {
                callback.notifyTaskReady(descendant);
            }
        }
    }

    callback.notifyTaskCompleted(shared_from_this());
    return true;
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
    TaskPtr task = Task::create(id, info, *this);
    m_stats.numWaiting++;

    if (m_allTasks.find(id) != m_allTasks.end()) {
        fail("Cannot creat task -- task ID already exists.");
    }
    m_allTasks[id] = task;

    if (task->getStatus().areDependenciesSatisfied()) {
        notifyTaskReady(task);
    }

    return task;
}

TaskPtr TaskDatabase::takeTaskToRun(const std::string& workerName, const std::vector<std::string>& affinities)
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

        m_stats.numReady--;
        m_stats.numRunning++;

        // Now that a ready task was found, remove it from the "ready tasks" sets
        for (auto& readyTasks : m_readyTasksPerAffinity) {
            readyTasks.second.erase(readyTask->getID());
        }

        // Then finally set it as running
        readyTask->markStarted(workerName);
    }

    return readyTask;
}

bool TaskDatabase::heartbeatTask(TaskPtr task)
{
    return task->heartbeat();
}

bool TaskDatabase::markTaskFinished(TaskPtr task)
{
    return task->markFinished(*this);
}

bool TaskDatabase::markTaskShouldCancel(TaskPtr task)
{
    return task->markShouldCancel(*this);
}

void TaskDatabase::cleanupZombieTasks(std::time_t heartbeatTimeoutSeconds)
{
    for (auto item : m_allTasks) {
        auto task = item.second;
        cleanupIfZombieTask(task, heartbeatTimeoutSeconds);
    }
}

void TaskDatabase::notifyTaskCompleted(TaskPtr task)
{
    for (const auto& affinity : task->getSchedule().affinities) {
        m_readyTasksPerAffinity[affinity].erase(task->getID());
    }
    m_allTasks.erase(task->getID());

    m_stats.numRunning--;
    m_stats.numFinished++;
}

bool TaskDatabase::cleanupIfZombieTask(TaskPtr task, std::time_t heartbeatTimeoutSeconds)
{
    bool died = false;

    task->getStatus().runStatus.tryUnwrap([&](const TaskRunStatus& runStatusVal) {
        if (!runStatusVal.endTime.hasValue()) {
            std::time_t diff = std::time(nullptr) - runStatusVal.heartbeatTime;
            died = (diff >= heartbeatTimeoutSeconds);
        }
    });

    if (died) {
        markTaskFinished(task);
    }
    return died;
}

void TaskDatabase::notifyTaskReady(TaskPtr task)
{
    runtimeAssert(task->getStatus().areDependenciesSatisfied(), "TaskDatabase::notifyTaskReady was called before task dependencies finished");
    for (const auto& affinity : task->getSchedule().affinities) {
        m_readyTasksPerAffinity[affinity].insert(task->getID());
    }
    m_stats.numWaiting--;
    m_stats.numReady++;
}


TaskState TaskStatus::getState() const
{
    TaskRunStatus runStatusVal;
    if (runStatus.tryGet(runStatusVal)) {
        if (runStatusVal.endTime.hasValue()) {
            return runStatusVal.wasCanceled ? TaskState::Canceled : TaskState::Completed;
        }
        else {
            return runStatusVal.wasCanceled ? TaskState::Canceling : TaskState::Running;
        }
    }
    else {
        return areDependenciesSatisfied() ? TaskState::Ready : TaskState::Waiting;
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

    writer << dependencies.size();
    for (auto& dep : dependencies) {
        writer << dep;
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

    if (!(reader >> count)) { return false; }
    dependencies.resize(count);
    for (size_t i = 0; i < count; ++i) {
        if (!(reader >> dependencies[i])) { return false; }
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
    str += "}, Dependencies = {";
    for (size_t i = 0; i < dependencies.size(); ++i) {
        str += toHexString(dependencies[i]);
        if (i != dependencies.size() - 1) {
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
    writer << workerName;
    writer << wasCanceled;
    writer << startTime;
    writer << heartbeatTime;

    std::time_t endTimeVal;
    if (endTime.tryGet(endTimeVal)) {
        writer << true;
        writer << endTimeVal;
    }
    else {
        writer << false;
    }
}

bool TaskRunStatus::deserialize(BlobStreamReader& reader)
{
    if (!(reader >> workerName)) { return false; }
    if (!(reader >> wasCanceled)) { return false; }
    if (!(reader >> startTime)) { return false; }
    if (!(reader >> heartbeatTime)) { return false; }

    bool hasEndTime;
    if (!(reader >> hasEndTime)) { return false; }
    if (hasEndTime) {
        std::time_t endTimeVal;
        if (!(reader >> endTimeVal)) { return false; }
        endTime = endTimeVal;
    }
    else {
        endTime = Nothing();
    }

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

    writer << unsatisfiedDependencies;
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

    if (!(reader >> unsatisfiedDependencies)) { return false; }
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
    if (state == TaskState::Waiting) { return "Waiting"; }
    else if (state == TaskState::Ready) { return "Ready"; }
    else if (state == TaskState::Running) { return "Running"; }
    else if (state == TaskState::Canceling) { return "Canceling"; }
    else if (state == TaskState::Canceled) { return "Canceled"; }
    else if (state == TaskState::Completed) { return "Completed"; }
    return "<Invalid TaskState>";
}

std::string TaskStatus::toString() const
{
    TaskRunStatus runStatusVal = runStatus.orDefault();
    std::time_t nowTime = std::time(nullptr);

    std::string str;
    switch (getState()) {
        case TaskState::Waiting:
            str += "Waiting (" + std::to_string(unsatisfiedDependencies) + " unsatisfied dependencies; so far waited " + intervalToString(nowTime - createTime) + ")";
            break;
        case TaskState::Ready:
            str += "Ready (dependencies satisfied; so far waited " + intervalToString(nowTime - createTime) + ")";
            break;
        case TaskState::Running:
            str += "Running (current runtime " + intervalToString(nowTime - runStatusVal.startTime) + "; worker heartbeat " + intervalToString(nowTime - runStatusVal.heartbeatTime) + ")";
            break;
        case TaskState::Canceling:
            str += "Canceling (current runtime " + intervalToString(nowTime - runStatusVal.startTime) + "; worker heartbeat " + intervalToString(nowTime - runStatusVal.heartbeatTime) + ")";
            break;
        case TaskState::Canceled:
            if (runStatusVal.startTime == time_t(0)) {
                str += "Canceled (never ran)";
            }
            else {
                str += "Canceled (finished after " + intervalToString(runStatusVal.endTime.orDefault() - runStatusVal.startTime) + ")";
            }
            break;
        case TaskState::Completed:
            str += "Completed (finished after " + intervalToString(runStatusVal.endTime.orDefault() - runStatusVal.startTime) + ")";
            break;
    }

    return str;
}
