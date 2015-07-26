#pragma once

#include "TaskDatabase.h"
#include "TaskServer.h"

class TaskWorker
{
public:
    TaskWorker(TaskClient& client, std::vector<std::string>&& affinities);
    ~TaskWorker();

    void run();
    void shutdown();

private:
    bool tryRunOneTask();

    TaskClient& m_client;
    std::vector<std::string> m_affinities;
    volatile bool m_running;
};

