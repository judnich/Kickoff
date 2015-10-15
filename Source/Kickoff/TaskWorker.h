#pragma once

#include "TaskDatabase.h"
#include "TaskServer.h"

class TaskWorker
{
public:
    TaskWorker(TaskClient& client, std::vector<std::string>&& resources);
    ~TaskWorker();

    void run();
    void shutdown();

private:
    bool tryRunOneTask();

    TaskClient& m_client;
    std::vector<std::string> m_resources;
    volatile bool m_running;
};

