#include "Process.h"
#include "Util.h"
#include "Error.h"
#include <windows.h>


Process::Process(const ProcessStartInfo& info)
    : m_startInfo(info)
    , m_started(false)
{
    start();
}

Process::~Process()
{
    terminate();
}

bool Process::start()
{
    if (m_started) { return false; }

    std::wstring workingDir = stringToWstring(m_startInfo.workingDir);

    std::vector<wchar_t> commandStrVec(m_startInfo.commandStr.size() + 1);
    for (size_t i = 0; i < m_startInfo.commandStr.size(); ++i) {
        commandStrVec[i] = (wchar_t)m_startInfo.commandStr[i];
    }
    commandStrVec[commandStrVec.size() - 1] = 0;

    m_job = CreateJobObject(NULL, NULL);
    if (m_job == NULL)
    {
        fail("Could not create job object");
    }
    else
    {
        // Configure all child processes associated with the job to terminate when the
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if( 0 == SetInformationJobObject(m_job, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)))
        {
            fail("Could not SetInformationJobObject");
        }
    }

    ZeroMemory( &m_si, sizeof(m_si) );
    m_si.cb = sizeof(m_si);
    ZeroMemory( &m_pi, sizeof(m_pi) );

    if (!CreateProcess(
        NULL, // No module name (use command line)
        commandStrVec.data(), // Command line
        NULL, // Process handle not inheritable
        NULL, // Thread handle not inheritable
        FALSE, // Set handle inheritance to FALSE
        0, // No creation flags
        NULL, // Use parent's environment block
        workingDir.c_str(),
        &m_si,
        &m_pi))
    {
        m_started = false;
        return false;
    }

    if (0 == AssignProcessToJobObject(m_job, m_pi.hProcess))
    {
        fail("Could not AssignProcessToJobObject");
    }

    m_started = true;
    return true;
}

void Process::wait()
{
    if (m_started) {
        // Wait until child process exits.
        WaitForSingleObject(m_pi.hProcess, INFINITE);

        m_started = false;
    }
}

void Process::terminate()
{
    if (m_started) {
        TerminateProcess(m_pi.hProcess, 0);
        CloseHandle(m_pi.hProcess);
        CloseHandle(m_pi.hThread);
        CloseHandle(m_job);
        m_started = false;
    }
}

bool Process::isRunning() const
{
    if (!m_started) {
        return false;
    }
    DWORD exitCode;
    if (!GetExitCodeProcess(m_pi.hProcess, &exitCode)) {
        return false;
    }

    return (exitCode == STILL_ACTIVE);
}
