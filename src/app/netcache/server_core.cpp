/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author: Pavel Ivanov
 *
 */

#include "task_server_pch.hpp"

#include <corelib/metareg.hpp>
#include <corelib/ncbifile.hpp>

#include "server_core.hpp"
#include "threads_man.hpp"
#include "sockets_man.hpp"
#include "timers.hpp"
#include "logging.hpp"
#include "time_man.hpp"
#include "memory_man.hpp"

#include <string.h>
#ifdef NCBI_OS_LINUX
# include <signal.h>
#endif



namespace boost
{

void
assertion_failed(char const* expr, char const* func, char const* file, long line)
{
    ncbi::CSrvDiagMsg().StartSrvLog(ncbi::CSrvDiagMsg::Fatal, file, line, func)
        << "assertion_failed: " << expr;
    abort();
}

}



BEGIN_NCBI_SCOPE;


EServerState s_SrvState = eSrvNotInitialized;
static EServerState s_SeenSDState = eSrvRunning;
static CSrvTime s_ShutdownStartTime;
static int s_ShutdownTO = 0;
static int s_SlowShutdownTO = 10;
static int s_FastShutdownTO = 2;
static int s_AbortShutdownTO = 0;
static CNcbiRegistry* s_Registry = NULL;
static string s_ConfName;
string s_AppBaseName;
static CMiniMutex s_SDListLock;
static TShutdownList s_ShutdownList;
static const char kSection[] = "task_server";



void
CTaskServer::AddShutdownCallback(CSrvShutdownCallback* callback)
{
    s_SDListLock.Lock();
    s_ShutdownList.push_front(*callback);
    s_SDListLock.Unlock();
}

static bool
s_IsReadyForShutdown(void)
{
    bool result = true;
    s_SDListLock.Lock();
    NON_CONST_ITERATE(TShutdownList, it, s_ShutdownList) {
        result &= it->ReadyForShutdown();
    }
    s_SDListLock.Unlock();
    return result;
}

void
TrackShuttingDown(void)
{
    if (s_SeenSDState != s_SrvState) {
        if (s_SeenSDState == eSrvRunning) {
            SRV_LOG(Warning, "Server is starting shutdown procedures.");
            RequestStopListening();
        }
        s_ShutdownStartTime = s_LastJiffyTime;
        s_SeenSDState = s_SrvState;
        FireAllTimers();
    }

    CSrvTime diff_time = CSrvTime::Current();
    diff_time -= s_ShutdownStartTime;
    if (s_SrvState == eSrvShuttingDownSoft  &&  diff_time.Sec() >= s_ShutdownTO) {
        SRV_LOG(Error, "Soft shutdown timeout has expired. Trying harder.");
        s_SeenSDState = s_SrvState = eSrvShuttingDownHard;
        FireAllTimers();
    }

    if (SchedIsAllIdle()  &&  s_IsReadyForShutdown()  /*&&  s_TotalSockets == 0*/)
        s_SrvState = eSrvStopping;

    if (s_SrvState == eSrvShuttingDownHard  &&
        s_AbortShutdownTO &&  diff_time.Sec() >= s_AbortShutdownTO)
    {
        SRV_FATAL("TrackShuttingDown forced abort: s_SrvState: " << s_SrvState);
    }
}

static void
s_TermHandler(int)
{
    CTaskServer::RequestShutdown(eSrvFastShutdown);
}

static void
s_InitSignals(void)
{
#ifdef NCBI_OS_LINUX
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &s_TermHandler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
#endif
}

static bool 
s_LoadConfFile(CNcbiRegistry*& reg)
{
    reg = new CNcbiRegistry();
#if 0
    CMetaRegistry::SEntry entry;
    if (s_ConfName.empty()) {
        entry = CMetaRegistry::Load(s_AppBaseName, CMetaRegistry::eName_Ini,
                                    0, 0, reg);
    }
    else {
        entry = CMetaRegistry::Load(s_ConfName, CMetaRegistry::eName_AsIs,
                                    0, 0, reg);
    }
    if (!entry.registry  &&  !s_ConfName.empty()) {
        SRV_LOG(Critical, "Configuration file \"" << s_ConfName
                            << "\" cannot be opened");
        return false;
    }
    return true;
#else
    CNcbiIfstream ifs;
    string ini( GetConfName());
    ifs.open( ini.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if (!ifs.is_open()) {
        return false;
    }
    reg->Read(ifs);
    return true;
#endif
}

static void s_ConfigureTaskServer(const CNcbiRegistry* reg, CTempString section)
{
    s_SlowShutdownTO = reg->GetInt(kSection, "slow_shutdown_timeout", 10);
    s_FastShutdownTO = reg->GetInt(kSection, "fast_shutdown_timeout", 2);
    s_AbortShutdownTO = reg->GetInt(kSection, "max_shutdown_time", 0);
}

static bool
s_ReadConfiguration(void)
{
    try {
        if (!s_LoadConfFile(s_Registry))
            return false;

        s_ConfigureTaskServer(s_Registry,kSection);
        ConfigureTimeMan(s_Registry, kSection);
        ConfigureScheduler(s_Registry, kSection);
        ConfigureThreads(s_Registry, kSection);
        ConfigureSockets(s_Registry, kSection);
        ConfigureLogging(s_Registry, kSection);
    }
    catch (CException& ex) {
        SRV_LOG(Critical, "Bad configuration of CTaskServer: " << ex);
        return false;
    }
    return true;
}

bool
CTaskServer::ReadConfiguration( CNcbiRegistry*& reg)
{
    try {
        if (!s_LoadConfFile(reg))
            return false;
    }
    catch (CException&) {
        return false;
    }
    return true;
}

void CTaskServer::WriteSetup(CSrvSocketTask& task)
{
    string is("\": "), eol(",\n\"");
    task.WriteText(eol).WriteText("slow_shutdown_timeout").WriteText(is ).WriteNumber( s_SlowShutdownTO);
    task.WriteText(eol).WriteText("fast_shutdown_timeout").WriteText(is ).WriteNumber( s_FastShutdownTO);
    task.WriteText(eol).WriteText("max_shutdown_time").WriteText(is ).WriteNumber( s_AbortShutdownTO);
    WriteSetup_TimeMan(task);
    WriteSetup_Scheduler(task);
    WriteSetup_Threads(task);
    WriteSetup_Sockets(task);
    WriteSetup_Logging(task);
}

bool CTaskServer::ReConfig(const CNcbiRegistry& new_reg, string& err_message)
{
    s_ConfigureTaskServer(&new_reg,kSection);
    return 
        ReConfig_TimeMan(kSection, new_reg, err_message) &&
        ReConfig_Scheduler(kSection, new_reg, err_message) &&
        ReConfig_Threads(kSection, new_reg, err_message) &&
        ReConfig_Sockets(kSection, new_reg, err_message) &&
        ReConfig_Logging(kSection, new_reg, err_message);
}

string
GetConfName(void)
{
    string ini(s_AppBaseName + ".ini");
    if (!s_ConfName.empty()) {
        ini = s_ConfName;
    }
    return ini;
}

void
ExtractFileName(const char* file, const char*& file_name, size_t& name_size)
{
    file_name = file + strlen(file);
    name_size = 0;
    while (file_name > file) {
        --file_name;
        if (*file_name == '/') {
            ++file_name;
            break;
        }
        ++name_size;
    }
}

static bool
s_ProcessParameters(int& argc, const char** argv)
{
    string cmd_line;
    for (int i = 0; i < argc; ++i) {
        cmd_line += argv[i];
        cmd_line.append(1, ' ');
    }
    cmd_line.resize(cmd_line.size() - 1);
    SaveAppCmdLine(cmd_line);

    const char* file_name;
    size_t name_size;
    ExtractFileName(argv[0], file_name, name_size);
    s_AppBaseName.assign(file_name, name_size);

    for (int i = 1; i < argc; ++i) {
        string param(argv[i]);
        if (param == "-conffile") {
            if (i + 1 < argc) {
                s_ConfName = argv[i + 1];
                if (!CDirEntry::IsAbsolutePath(s_ConfName)) {
                    s_ConfName = CDirEntry::NormalizePath(CDirEntry::ConcatPath(CDir::GetCwd(), s_ConfName));
                }
            }
            else {
                cerr << "Parameter -conffile misses file name" << endl;
                SRV_LOG(Critical, "Parameter -conffile misses file name");
                return false;
            }
        }
        else if (param == "-logfile") {
            if (i + 1 < argc) {
                SetLogFileName(argv[i + 1]);
            }
            else {
                cerr << "Parameter -logfile misses file name" << endl;
                SRV_LOG(Critical, "Parameter -logfile misses file name");
                return false;
            }
        }
        else {
            continue;
        }
        if (i + 2 < argc)
            memmove(&argv[i], &argv[i + 2], (argc - (i + 2)) * sizeof(argv[i]));
        argc -= 2;
        --i;
    }
    return true;
}


bool
CTaskServer::Initialize(int& argc, const char** argv)
{
    InitTime();
    bool result = true;
    if (result  &&  !s_ProcessParameters(argc, argv))
        result = false;
    if (result  &&  !s_ReadConfiguration())
        result = false;
    InitLogging();
    if (!result)
        return false;
    s_InitSignals();
    if (!InitSocketsMan())
        return false;
    if (!InitThreadsMan())
        return false;

    InitMemoryMan();
    InitTimeMan();
    InitTimers();

    s_SrvState = eSrvInitialized;
    return true;
}

void
CTaskServer::Finalize(void)
{
    FinalizeThreadsMan();
    FinalizeSocketsMan();
    FinalizeLogging();
    FinalizeMemoryMan();
}

void
CTaskServer::Run(void)
{
    s_SrvState = eSrvRunning;

    if (!StartSocketsMan())
        return;
    RunMainThread();

    s_SrvState = eSrvStopped;
}

const CNcbiRegistry&
CTaskServer::GetConfRegistry(void)
{
    return *s_Registry;
}

CNcbiRegistry&
CTaskServer::SetConfRegistry(void)
{
    return *s_Registry;
}

void
CTaskServer::RequestShutdown(ESrvShutdownType shutdown_type)
{
    switch (shutdown_type) {
    case eSrvSlowShutdown:
        s_ShutdownTO = s_SlowShutdownTO;
        break;
    case eSrvFastShutdown:
        s_ShutdownTO = s_FastShutdownTO;
        break;
    }
    AtomicCAS(s_SrvState, eSrvRunning, eSrvShuttingDownSoft);
}


CSrvShutdownCallback::CSrvShutdownCallback(void)
{}

CSrvShutdownCallback::~CSrvShutdownCallback(void)
{}

END_NCBI_SCOPE;
