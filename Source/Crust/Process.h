#pragma once
#include <string>
#include <windows.h>

struct ProcessStartInfo
{
    std::string commandStr;
    std::string workingDir;
};

class Process
{
public:
    Process(const ProcessStartInfo& info);
    ~Process();

    const ProcessStartInfo& getStartInfo() const { return m_startInfo; }
    void wait();
    void terminate();
    bool isRunning() const;

private:
    bool start();

    ProcessStartInfo m_startInfo;
    bool m_started;
    STARTUPINFO m_si;
    PROCESS_INFORMATION m_pi;
    HANDLE m_job;
};


