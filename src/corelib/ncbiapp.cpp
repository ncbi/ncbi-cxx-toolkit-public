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
 * Authors:  Vsevolod Sandomirskiy, Denis Vakatov
 *
 * File Description:
 *   CNcbiApplication -- a generic NCBI application class
 *   CCgiApplication  -- a NCBI CGI-application class
 *
 */

#include <ncbi_pch.hpp>
#include <common/ncbi_source_ver.h>
#include <corelib/ncbiapp.hpp>
#undef CNcbiApplication
#undef CComponentVersionInfo
#undef CVersion

#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/syslog.hpp>
#include <corelib/error_codes.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/request_ctx.hpp>
#include "ncbisys.hpp"

#if defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#  include <corelib/ncbidll.hpp>
#  include <io.h>
#  include <fcntl.h>
#endif

#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#endif


#define NCBI_USE_ERRCODE_X   Corelib_App


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  Constants
//

extern const char* s_ArgLogFile;
extern const char* s_ArgCfgFile;
extern const char* s_ArgVersion;
extern const char* s_ArgFullVersion;
extern const char* s_ArgFullVersionXml;
extern const char* s_ArgFullVersionJson;
extern const char* s_ArgDryRun;


/////////////////////////////////////////////////////////////////////////////
//  Global variables
//

static bool s_IsApplicationStarted = false;


///////////////////////////////////////////////////////
// CNcbiApplication
//

CNcbiApplicationGuard::CNcbiApplicationGuard(CNcbiApplicationAPI* app) : m_App(app)
{
    if (m_App) {
        m_AppLock = make_shared<CReadLockGuard>(m_App->GetInstanceLock());
    }
}


CNcbiApplicationGuard::~CNcbiApplicationGuard(void)
{
}


DEFINE_STATIC_MUTEX(s_InstanceMutex);

SSystemMutex& CNcbiApplicationAPI::GetInstanceMutex(void)
{
    return s_InstanceMutex;
}


static CSafeStatic<CRWLock> s_InstanceRWLock(CSafeStaticLifeSpan(CSafeStaticLifeSpan::eLifeSpan_Long, 1));

CRWLock& CNcbiApplicationAPI::GetInstanceLock(void)
{
    return s_InstanceRWLock.Get();
}


CNcbiApplicationAPI* CNcbiApplicationAPI::m_Instance = nullptr;

CNcbiApplicationAPI* CNcbiApplicationAPI::Instance(void)
{
    return m_Instance;
}


CNcbiApplicationGuard CNcbiApplicationAPI::InstanceGuard(void)
{
    return CNcbiApplicationGuard(m_Instance);
}


CNcbiApplicationAPI::CNcbiApplicationAPI(const SBuildInfo& build_info)
    : m_ConfigLoaded(false),
      m_LogFile(0),
      m_LogOptions(0)
{
    CThread::InitializeMainThreadId();
    // Initialize UID and start timer
    GetDiagContext().GetUID();
    GetDiagContext().InitMessages(size_t(-1));
    GetDiagContext().SetGlobalAppState(eDiagAppState_AppBegin);

    // Verify CPU compatibility
    // First check. Print critical error only. See second check in x_TryInit() below.
    {{
        string err_message;
        if (!VerifyCpuCompatibility(&err_message)) {
            ERR_POST_X(22,  Critical << err_message);
        }
    }}

    m_DisableArgDesc = 0;
    m_HideArgs = 0;
    m_StdioFlags = 0;
    m_CinBuffer = 0;
    m_ExitCodeCond = eNoExits;

    {
        CWriteLockGuard guard(GetInstanceLock());
        // Register the app. instance
        if (m_Instance) {
            NCBI_THROW(CAppException, eSecond,
                "Second instance of CNcbiApplication is prohibited");
        }
        m_Instance = this;
    }

    // Create empty version info
    m_Version.Reset(new CVersionAPI(build_info));

    // Set version equal to package one if still empty (might have TeamCity build number)
    if (m_Version->GetVersionInfo().IsAny()) {
        auto package_info = m_Version->GetPackageVersion();
        m_Version->SetVersionInfo(new CVersionInfo(package_info));
    }

#if NCBI_SC_VERSION_PROXY != 0
    m_Version->AddComponentVersion("NCBI C++ Toolkit",
        NCBI_SC_VERSION_PROXY, 0, NCBI_SUBVERSION_REVISION_PROXY,
        NCBI_TEAMCITY_PROJECT_NAME_PROXY, NCBI_APP_SBUILDINFO_DEFAULT());
#endif
    // Create empty application arguments & name
    m_Arguments.reset(new CNcbiArguments(0,0));

    // Create empty application environment
    m_Environ.reset(new CNcbiEnvironment);

    // Create an empty registry
    m_Config.Reset(new CNcbiRegistry);

    m_DryRun = false;
}

void CNcbiApplicationAPI::ExecuteOnExitActions()
{
    m_OnExitActions.ExecuteActions();
}


CNcbiApplicationAPI::~CNcbiApplicationAPI(void)
{
    CThread::sm_IsExiting = true;

    // Execute exit actions before waiting for all threads to stop.
    // NOTE: The exit actions may already be executed by higher-level
    //       destructors. This is a final fail-safe place for this.
    ExecuteOnExitActions();

#if defined(NCBI_THREADS)
    CThread::WaitForAllThreads();
#endif

    {
        CWriteLockGuard guard(GetInstanceLock());
        m_Instance = 0;
    }
    FlushDiag(0, true);
    if (m_CinBuffer) {
        delete [] m_CinBuffer;
    }

#if defined(NCBI_COMPILER_WORKSHOP)
    // At least under these conditions:
    //  1) WorkShop 5.5 on Solaris 10/SPARC, Release64MT,    and
    //  2) when IOS_BASE::sync_with_stdio(false) is called,  and
    //  3) the contents of 'cout' is not flushed
    // some applications crash on exit() while apparently trying to
    // flush 'cout' and getting confused by its own guts, with error:
    //   "*** libc thread failure: _thread_setschedparam_main() fails"
    //
    // This forced pre-flush trick seems to fix the problem.
    NcbiCout.flush();
#endif
}


CNcbiApplication* CNcbiApplication::Instance(void)
{
    return dynamic_cast<CNcbiApplication*>(CNcbiApplicationAPI::Instance());
}


CNcbiApplication::CNcbiApplication(const SBuildInfo& build_info)
    : CNcbiApplicationAPI(build_info)
{
}


CNcbiApplication::~CNcbiApplication()
{
    // This earlier execution of the actions allows a safe use of
    // CNcbiApplication::Instance() from the exit action functions. Instance()
    // can return NULL pointer if called as part of CNcbiApplicationAPI dtor
    // when the CNcbiApplication dtor already finished.
    ExecuteOnExitActions();
}


void CNcbiApplicationAPI::Init(void)
{
    return;
}


int CNcbiApplicationAPI::DryRun(void)
{
    ERR_POST_X(1, Info << "DryRun: default implementation does nothing");
    return 0;
}


void CNcbiApplicationAPI::Exit(void)
{
    return;
}


const CArgs& CNcbiApplicationAPI::GetArgs(void) const
{
    if ( !m_Args.get() ) {
        NCBI_THROW(CAppException, eUnsetArgs,
                   "Command-line argument description is not found");
    }
    return *m_Args;
}


SIZE_TYPE CNcbiApplicationAPI::FlushDiag(CNcbiOstream* os, bool /*close_diag*/)
{
    if ( os ) {
        SetDiagStream(os, true, 0, 0, "STREAM");
    }
    GetDiagContext().FlushMessages(*GetDiagHandler());
    GetDiagContext().DiscardMessages();
    return 0;
}


#if defined(NCBI_OS_DARWIN)
static void s_MacArgMunging(CNcbiApplicationAPI&   app,
                            int*                argcPtr,
                            const char* const** argvPtr,
                            const string&       exepath)
{

    // Sometimes on Mac there will be an argument -psn which
    // will be followed by the Process Serial Number, e.g. -psn_0_13107201
    // this is in situations where the application could have no other
    // arguments like when it is double clicked.
    // This will mess up argument processing later, so get rid of it.
    static const char* s_ArgMacPsn = "-psn_";

    if (*argcPtr == 2  &&
        NStr::strncmp((*argvPtr)[1], s_ArgMacPsn, strlen(s_ArgMacPsn)) == 0) {
        --*argcPtr;
    }

    if (*argcPtr > 1)
        return;

    // Have no arguments from the operating system -- so use the '.args' file

    // Open the args file.
    string exedir;
    CDir::SplitPath(exepath, &exedir);
    string args_fname = exedir + app.GetProgramDisplayName() + ".args";
    CNcbiIfstream in(args_fname.c_str());

    if ( !in.good() ) {
        ERR_POST_X(2, Info << "Mac arguments file not found: " << args_fname);
        return;
    }

    vector<string> v;

    // remember or fake the executable name.
    if (*argcPtr > 0) {
        v.push_back((*argvPtr)[0]); // preserve the original argv[0].
    } else {
        v.push_back(exepath);
    }

    // grab the rest of the arguments from the file.
    // arguments are separated by whitespace. Can be on
    // more than one line.
    string arg;
    while (in >> arg) {
        v.push_back(arg);
    }

    // stash them away in the standard argc and argv places.
    *argcPtr = v.size();

    char** argv =  new char*[v.size()];
    int c = 0;
    ITERATE(vector<string>, vp, v) {
        argv[c++] = strdup(vp->c_str());
    }
    *argvPtr = argv;
}
#endif  /* NCBI_OS_DARWIN */


NCBI_PARAM_DECL(bool, Debug, Catch_Unhandled_Exceptions);
NCBI_PARAM_DEF_EX(bool, Debug, Catch_Unhandled_Exceptions, true,
                  eParam_NoThread, DEBUG_CATCH_UNHANDLED_EXCEPTIONS);
typedef NCBI_PARAM_TYPE(Debug, Catch_Unhandled_Exceptions) TParamCatchExceptions;

bool s_HandleExceptions(void)
{
    return TParamCatchExceptions::GetDefault();
}


NCBI_PARAM_DECL(bool, NCBI, TerminateOnCpuIncompatibility); 
NCBI_PARAM_DEF_EX(bool, NCBI, TerminateOnCpuIncompatibility, false,
                  eParam_NoThread, NCBI_CONFIG__TERMINATE_ON_CPU_INCOMPATIBILITY);


void CNcbiApplicationAPI::x_TryInit(EAppDiagStream diag, const char* conf)
{
    // Load registry from the config file
    if ( conf ) {
        string x_conf(conf);
        LoadConfig(*m_Config, &x_conf);
    } else {
        LoadConfig(*m_Config, NULL);
    }
    m_ConfigLoaded = true;

    CDiagContext::SetupDiag(diag, m_Config, eDCM_Flush, m_LogFile);
    CDiagContext::x_FinalizeSetupDiag();

    // Setup the standard features from the config file.
    // Don't call till after LoadConfig()
    // NOTE: this will override environment variables,
    // except DIAG_POST_LEVEL which is Set*Fixed*.
    x_HonorStandardSettings();

    // Application start
    AppStart();

    // Verify CPU compatibility
    // Second check. Print error message and allow to terminate program depends on configuration parameters.
    // Also, see first check in CNcbiApplicationAPI() constructor.
    {{
        string err_message;
        if (!VerifyCpuCompatibility(&err_message)) {
            bool fatal = NCBI_PARAM_TYPE(NCBI, TerminateOnCpuIncompatibility)::GetDefault();
            ERR_POST_X(22, (fatal ? Fatal : Critical) << err_message);
        }
    }}

    // Do init
#if (defined(NCBI_COMPILER_ICC) && NCBI_COMPILER_VERSION < 900)
    // ICC 8.0 have an optimization bug in exceptions handling,
    // so workaround it here
    try {
        Init();
    }
    catch (const CArgHelpException&) {
        throw;
    }
#else
    Init();
#endif

    // If the app still has no arg description - provide default one
    if (!m_DisableArgDesc  &&  !m_ArgDesc.get()) {
        unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
        arg_desc->SetUsageContext
            (GetArguments().GetProgramBasename(),
             "This program has no mandatory arguments");
        SetupArgDescriptions(arg_desc.release());
    }
}

// Macro to define a logging parameter
#define NCBI_LOG_PARAM(type,Name,NAME)             \
    NCBI_PARAM_DECL  (type, Log, LogApp ## Name);  \
    NCBI_PARAM_DEF_EX(type, Log, LogApp ## Name, false, eParam_NoThread, DIAG_LOG_APP_ ## NAME); \
    typedef NCBI_PARAM_TYPE(Log, LogApp ## Name) TLogApp ## Name;

NCBI_LOG_PARAM(bool, Environment,        ENVIRONMENT)
NCBI_LOG_PARAM(bool, EnvironmentOnStop,  ENVIRONMENT_ON_STOP)
NCBI_LOG_PARAM(bool, Registry,           REGISTRY)
NCBI_LOG_PARAM(bool, RegistryOnStop,     REGISTRY_ON_STOP)
NCBI_LOG_PARAM(bool, Arguments,          ARGUMENTS)
NCBI_LOG_PARAM(bool, Path,               PATH)
NCBI_LOG_PARAM(bool, RunContext,         RUN_CONTEXT)
NCBI_LOG_PARAM(bool, ResUsageOnStop,     RESUSAGE_ON_STOP)


enum ELogOptionsEvent {
    eStartEvent = 0x01, ///< right before AppMain()
    eStopEvent  = 0x02, ///< right after AppMain()
    eOtherEvent = 0x03  ///< any case is fine
};


/// Flags to switch what to log
enum ELogOptions {
    fLogAppEnvironment      = 0x01, ///< log app environment on app start
    fLogAppEnvironmentStop  = 0x02, ///< log app environment on app stop
    fLogAppRegistry         = 0x04, ///< log app registry on app start
    fLogAppRegistryStop     = 0x08, ///< log app registry on app stop
    fLogAppArguments        = 0x10, ///< log app arguments
    fLogAppPath             = 0x20, ///< log app executable path
    fLogAppResUsageStop     = 0x40, ///< log resource usage on app stop
};


void CNcbiApplicationAPI::x_ReadLogOptions()
{
    // Log all
    if ( TLogAppRunContext::GetDefault() ) {
        m_LogOptions = 0x7f; // all on
        return;
    }

    // Log registry
    m_LogOptions |= TLogAppRegistry::GetDefault() ? fLogAppRegistry : 0;
    m_LogOptions |= TLogAppRegistryOnStop::GetDefault() ? fLogAppRegistryStop : 0;

    // Log environment
    m_LogOptions |= TLogAppEnvironment::GetDefault() ? fLogAppEnvironment : 0;
    m_LogOptions |= TLogAppEnvironmentOnStop::GetDefault() ? fLogAppEnvironmentStop : 0;

    // Log arguments
    m_LogOptions |= TLogAppArguments::GetDefault() ? fLogAppArguments : 0;

    // Log path
    m_LogOptions |= TLogAppPath::GetDefault() ? fLogAppPath : 0;

    // Log resources usage
    m_LogOptions |= TLogAppResUsageOnStop::GetDefault() ? fLogAppResUsageStop : 0;
}


void s_RoundResUsageSize(Uint8 value_in_bytes, string& suffix, Uint8& value)
{
    const Uint8 limit = 1000;

    // KB by default
    suffix = "_KB";
    value = value_in_bytes / 1024;

    // Round to MB if value is too big
    if (value / 1024 > limit) {
        suffix = "_MB";
        value /= 1024;
    }
}

#define RES_SIZE_USAGE(name, value_in_bytes) \
    { \
        string suffix; \
        Uint8  value; \
        s_RoundResUsageSize(value_in_bytes, suffix, value); \
        extra.Print(name + suffix, value); \
    }

#define RES_TIME_USAGE(name, value) \
    if (value >= 0 ) \
        extra.Print(name, (Uint8)value)


void CNcbiApplicationAPI::x_LogOptions(int /*ELogOptionsEvent*/ event)
{
    const bool start = (event & eStartEvent) != 0;
    const bool stop  = (event & eStopEvent)  != 0;

    // Print environment values
    if ( (m_LogOptions & fLogAppEnvironment      &&  start) ||
         (m_LogOptions & fLogAppEnvironmentStop  &&  stop) ) {
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("LogAppEnvironment", "true");
        list<string> env_keys;
        const CNcbiEnvironment& env = GetEnvironment();
        env.Enumerate(env_keys);
        ITERATE(list<string>, it, env_keys) {
            const string& val = env.Get(*it);
            extra.Print(*it, val);
        }
    }

    // Print registry values
    if ( (m_LogOptions & fLogAppRegistry      &&  start) ||
         (m_LogOptions & fLogAppRegistryStop  &&  stop) ) {
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("LogAppRegistry", "true");
        list<string> reg_sections;
        const CNcbiRegistry& reg = GetConfig();
        reg.EnumerateSections(&reg_sections);
        ITERATE(list<string>, it, reg_sections) {
            string section, name;
            list<string> section_entries;
            reg.EnumerateEntries(*it, &section_entries);
            ITERATE(list<string>, it_entry, section_entries) {
                const string& val = reg.Get(*it, *it_entry);
                string path = "[" + *it + "]" + *it_entry;
                extra.Print(path, val);
            }
        }
    }

    // Print arguments
    if ( m_LogOptions & fLogAppArguments  &&  start) {
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("LogAppArguments", "true");
        string args_str;
        extra.Print("Arguments", GetArgs().Print(args_str));
    }

    // Print app path
    if ( m_LogOptions & fLogAppPath  &&  start) {
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("LogAppPath", "true");
        extra.Print("Path", GetProgramExecutablePath());
    }

    // Print resource usage
    if ( m_LogOptions & fLogAppResUsageStop  &&  stop) {
        CDiagContext_Extra extra = GetDiagContext().Extra();
        extra.Print("LogAppResUsage", "true");
        // Memory usage
        CProcess::SMemoryUsage mem_usage;
        if ( CCurrentProcess::GetMemoryUsage(mem_usage) ) {
            RES_SIZE_USAGE("mem_total",      mem_usage.total        );
            RES_SIZE_USAGE("mem_total_peak", mem_usage.total_peak   );
            RES_SIZE_USAGE("rss_mem",        mem_usage.resident     );
            RES_SIZE_USAGE("rss_peak_mem",   mem_usage.resident_peak);
            RES_SIZE_USAGE("shared.mem",     mem_usage.shared       );
            RES_SIZE_USAGE("data.mem",       mem_usage.data         );
            RES_SIZE_USAGE("stack.mem",      mem_usage.stack        );
        }
        // CPU time usage
        double real, user, sys;
        if ( CCurrentProcess::GetTimes(&real, &user, &sys, CProcess::eProcess) ) {
            RES_TIME_USAGE("real.proc.cpu", real);
            RES_TIME_USAGE("user.proc.cpu", user);
            RES_TIME_USAGE("sys.proc.cpu",  sys);
        }
        if ( CCurrentProcess::GetTimes(&real, &user, &sys, CProcess::eChildren) ) {
            RES_TIME_USAGE("user.child.cpu", user);
            RES_TIME_USAGE("sys.child.cpu",  sys);
        }
        if ( CCurrentProcess::GetTimes(&real, &user, &sys, CProcess::eThread) ) {
            RES_TIME_USAGE("user.thread.cpu", user);
            RES_TIME_USAGE("sys.thread.cpu",  sys);
        }
    }
}


void CNcbiApplicationAPI::x_TryMain(EAppDiagStream diag,
                                 const char*    conf,
                                 int*           exit_code,
                                 bool*          got_exception)
{
    // Initialize the application
    try {
        if ( s_HandleExceptions() ) {
            try {
                x_TryInit(diag, conf);
            }
            catch (const CArgHelpException&) {
                // This exceptions will be caught later regardless of the
                // handle-exceptions flag.
                throw;
            }
            catch (const CArgException&) {
//                NCBI_RETHROW_SAME(e, "Application's initialization failed");
                throw;
            }
            catch (const CException& e) {
                NCBI_REPORT_EXCEPTION_X(15,
                                        "Application's initialization failed", e);
                *got_exception = true;
                *exit_code = 2;
            }
            catch (const exception& e) {
                ERR_POST_X(6, "Application's initialization failed: " << e.what());
                *got_exception = true;
                *exit_code = 2;
            }
        }
        else {
            x_TryInit(diag, conf);
        }
    }
    catch (const CArgHelpException& e) {
        x_AddDefaultArgs();
        // Print USAGE
        if (e.GetErrCode() == CArgHelpException::eHelpXml) {
            m_ArgDesc->PrintUsageXml(cout);
        } else {
            CArgHelpException::TErrCode code =  e.GetErrCode();
            string str;
            m_ArgDesc->ShowAllArguments(code == CArgHelpException::eHelpShowAll)->PrintUsage
                (str, code == CArgHelpException::eHelpFull || code == CArgHelpException::eHelpShowAll);
            cout << str;
        }
        *exit_code = e.GetErrCode() == CArgHelpException::eHelpErr ? 2 : 0;
    }
    x_ReadLogOptions();
    x_LogOptions(eStartEvent);
    // Run application
    if (*exit_code == 1) {
        GetDiagContext().SetGlobalAppState(eDiagAppState_AppRun);
        if ( s_HandleExceptions() ) {
            try {
                *exit_code = m_DryRun ? DryRun() : Run();
            }
            catch (CArgException& e) {
                NCBI_RETHROW_SAME(e, "Application's execution failed");
            }
            catch (const CException& e) {
                CRef<CRequestContext> cur_ctx(&GetDiagContext().GetRequestContext());
                if (cur_ctx != &e.GetRequestContext()) {
                    GetDiagContext().SetRequestContext(&e.GetRequestContext());
                    NCBI_REPORT_EXCEPTION_X(16,
                                            "CException thrown", e);
                    GetDiagContext().SetRequestContext(cur_ctx);
                }
                NCBI_REPORT_EXCEPTION_X(16,
                                        "Application's execution failed", e);
                *got_exception = true;
                *exit_code = 3;
            }
            catch (const exception& e) {
                ERR_POST_X(7, "Application's execution failed: " << e.what());
                *got_exception = true;
                *exit_code = 3;
            }
        }
        else {
            *exit_code = m_DryRun ? DryRun() : Run();
        }
    }
    x_LogOptions(eStopEvent);
    GetDiagContext().SetGlobalAppState(eDiagAppState_AppEnd);

    // Close application
    if ( s_HandleExceptions() ) {
        try {
            Exit();
        }
        catch (CArgException& e) {
            NCBI_RETHROW_SAME(e, "Application's cleanup failed");
        }
        catch (const CException& e) {
            NCBI_REPORT_EXCEPTION_X(17, "Application's cleanup failed", e);
            *got_exception = true;
        }
        catch (const exception& e) {
            ERR_POST_X(8, "Application's cleanup failed: "<< e.what());
            *got_exception = true;
        }
    }
    else {
        Exit();
    }
}

#if defined(NCBI_OS_MSWIN) && defined(_UNICODE)
static
void s_Create_ArgsOrEnvW(
    vector<string>& storage,
    AutoArray<const char*>& pointers,
    const TXChar* const* begin)
{
    const TXChar* const* arg = begin;
    size_t count = 0;
    while( *(arg++) )
        ++count;

    const char** args = new const char*[count+1];
    if ( !args ) {
        NCBI_THROW(CCoreException, eNullPtr, kEmptyStr);
    }
    pointers = args;

    arg = begin;
    size_t i=0;
    for (i=0; i<count; ++i) {
        storage.push_back( _T_STDSTRING( *(arg++) ) );
    }

    for (i=0; i < storage.size(); ++i) {
        args[i] = storage[i].c_str();
    }
    args[i] = NULL;
}

int CNcbiApplicationAPI::AppMain
(int                  argc,
 const TXChar* const* argv,
 const TXChar* const* envp,
 EAppDiagStream       diag,
 const TXChar*        conf,
 const TXString&      name)
{
    vector< string> argv_storage;
    AutoArray<const char*> argv_pointers;
    if (argv) {
        s_Create_ArgsOrEnvW(argv_storage, argv_pointers, argv);
    }

    vector< string> envp_storage;
    AutoArray<const char*> envp_pointers;
    if (envp) {
        s_Create_ArgsOrEnvW(envp_storage, envp_pointers, envp);
    }

    return AppMain(argc,
        argv == NULL ? NULL : argv_pointers.get(),
        envp == NULL ? NULL : envp_pointers.get(),
        diag,
        conf == NULL ? NULL : (conf == NcbiEmptyXCStr ? NcbiEmptyCStr : _T_CSTRING(conf)),
        name == NcbiEmptyXString ? NcbiEmptyString : _T_STDSTRING(name));
}
#endif

int CNcbiApplicationAPI::AppMain
(int                argc,
 const char* const* argv,
 const char* const* envp,
 EAppDiagStream     diag,
 const char*        conf,
 const string&      name)
{
    if (conf) {
        m_DefaultConfig = conf;
    }
    x_SetupStdio();

    // Check if logfile is set in the args.
    m_LogFile = 0;
    if (!m_DisableArgDesc && argc > 1  &&  argv  &&  diag != eDS_User) {
        for (int i = 1;  i < argc;  i++) {
            if ( !argv[i] ) {
                continue;
            }
            if ( NStr::strcmp(argv[i], s_ArgLogFile) == 0 ) {
                if (!argv[++i]) {
                    continue;
                }
                m_LogFile = argv[i];
            } else if (NStr::StartsWith(argv[i], s_ArgLogFile)) {
                const char *a = argv[i] + strlen(s_ArgLogFile);
                if (*a == '=') {
                    m_LogFile = ++a;
                }
            }
        }
    }
    // Setup logging as soon as possible.
    // Setup for diagnostics
    try {
        CDiagContext::SetupDiag(diag, 0, eDCM_NoChange, m_LogFile);
    } catch (const CException& e) {
        NCBI_RETHROW(e, CAppException, eSetupDiag,
                     "Application diagnostic stream's setup failed");
    } catch (const exception& e) {
        NCBI_THROW(CAppException, eSetupDiag,
                   "Application diagnostic stream's setup failed: " +
                   string(e.what()));
    }

    // Get program executable's name & path.
    string exepath = FindProgramExecutablePath(argc, argv, &m_RealExePath);
    m_ExePath = exepath;

    // Get program display name
    string appname = name;
    if (appname.empty()) {
        if (!exepath.empty()) {
            CDirEntry::SplitPath(exepath, NULL, &appname);
        } else if (argc > 0  &&  argv[0] != NULL  &&  *argv[0] != '\0') {
            CDirEntry::SplitPath(argv[0], NULL, &appname);
        } else {
            appname = "ncbi";
        }
    }
    if ( m_ProgramDisplayName.empty() ) {
        SetProgramDisplayName(appname);
    }

    // Make sure we have something as our 'real' executable's name.
    // though if it does not contain a full path it won't be much use.
    if ( exepath.empty() ) {
        ERR_POST_X(3, Warning
                      << "Warning:  Could not determine this application's "
                      "file name and location.  Using \""
                      << appname << "\" instead.\n"
                      "Please fix FindProgramExecutablePath() on this platform.");
        exepath = appname;
    }

#if defined(NCBI_OS_DARWIN)
    // We do not know standard way of passing arguments to C++ program on Mac,
    // so we will read arguments from special file having extension ".args"
    // and name equal to display name of program (name argument of AppMain).
    s_MacArgMunging(*this, &argc, &argv, exepath);
#endif

    CDiagContext& diag_context = GetDiagContext();

    // Preparse command line
    if (PreparseArgs(argc, argv) == ePreparse_Exit) {
        diag_context.DiscardMessages();
        return 0;
    }

    // Check command line for presence special arguments
    // "-logfile", "-conffile", "-version"
    const char* conf_arg = nullptr;
    if (!m_DisableArgDesc && argc > 1  &&  argv) {
        const char** v = new const char*[argc];
        v[0] = argv[0];
        int real_arg_index = 1;
        for (int i = 1;  i < argc;  i++) {
            if ( !argv[i] ) {
                continue;
            }
            // Log file - ignore if diag is eDS_User - the user wants to
            // take care about logging.
            if ( diag != eDS_User  &&
                NStr::strcmp(argv[i], s_ArgLogFile) == 0 ) {
                if (!argv[++i]) {
                    continue;
                }
                v[real_arg_index++] = argv[i - 1];
                v[real_arg_index++] = argv[i];
                // Configuration file
            } else if ( NStr::strcmp(argv[i], s_ArgCfgFile) == 0 ) {
                if (!argv[++i]) {
                    continue;
                }
                v[real_arg_index++] = argv[i - 1];
                v[real_arg_index++] = argv[i];
                conf_arg = argv[i];

            }
            else if (NStr::StartsWith(argv[i], s_ArgCfgFile)) {
                v[real_arg_index++] = argv[i];
                const char* a = argv[i] + strlen(s_ArgCfgFile);
                if (*a == '=') {
                    conf_arg = ++a;
                }

                // Version
            } else if ( NStr::strcmp(argv[i], s_ArgVersion) == 0 ) {
                delete[] v;
                // Print VERSION
                cout << GetFullVersion().Print( appname, CVersionAPI::fVersionInfo | CVersionAPI::fPackageShort );
                diag_context.DiscardMessages();
                return 0;

                // Full version
            } else if ( NStr::strcmp(argv[i], s_ArgFullVersion) == 0 ) {
                delete[] v;
                // Print full VERSION
                cout << GetFullVersion().Print( appname );
                diag_context.DiscardMessages();
                return 0;
            } else if ( NStr::strcmp(argv[i], s_ArgFullVersionXml) == 0 ) {
                delete[] v;
                // Print full VERSION in XML format
                cout << GetFullVersion().PrintXml( appname );
                diag_context.DiscardMessages();
                return 0;
            } else if ( NStr::strcmp(argv[i], s_ArgFullVersionJson) == 0 ) {
                delete[] v;
                // Print full VERSION in JSON format
                cout << GetFullVersion().PrintJson( appname );
                diag_context.DiscardMessages();
                return 0;

                // Dry run
            } else if ( NStr::strcmp(argv[i], s_ArgDryRun) == 0 ) {
                m_DryRun = true;

                // Save real argument
            } else {
                v[real_arg_index++] = argv[i];
            }
        }
        if (real_arg_index == argc ) {
            delete[] v;
        } else {
            argc = real_arg_index;
            argv = v;
        }
        if (conf_arg) {
            if (CFile(conf_arg).Exists()) {
                conf = conf_arg;
            }
            else {
                ERR_POST_X(23, Critical << "Registry file \"" << conf_arg << "\" not found");
            }
        }
    }

    // Reset command-line args and application name
    m_Arguments->Reset(argc, argv, exepath, m_RealExePath);

    // Reset application environment
    m_Environ->Reset(envp);

    // Setup some debugging features from environment variables.
    if ( !m_Environ->Get(DIAG_TRACE).empty() ) {
        SetDiagTrace(eDT_Enable, eDT_Enable);
    }
    string post_level = m_Environ->Get(DIAG_POST_LEVEL);
    if ( !post_level.empty() ) {
        EDiagSev sev;
        if (CNcbiDiag::StrToSeverityLevel(post_level.c_str(), sev)) {
            SetDiagFixedPostLevel(sev);
        }
    }
    if ( !m_Environ->Get(ABORT_ON_THROW).empty() ) {
        SetThrowTraceAbort(true);
    }

    // Clear registry content
    m_Config->Clear();

    // Call:  Init() + Run() + Exit()
    int exit_code = 1;
    bool got_exception = false;
    s_IsApplicationStarted = true;

    try {
        if ( s_HandleExceptions() ) {
            try {
                x_TryMain(diag, conf, &exit_code, &got_exception);
            }
            catch (const CArgException&) {
                // This exceptions will be caught later regardless of the
                // handle-exceptions flag.
                throw;
            }
#if defined(NCBI_COMPILER_MSVC)  &&  defined(_DEBUG)
            // Microsoft promotes many common application errors to exceptions.
            // This includes occurrences such as dereference of a NULL pointer and
            // walking off of a dangling pointer.  The catch-all is lifted only in
            // debug mode to permit easy inspection of such error conditions, while
            // maintaining safety of production, release-mode applications.
            catch (...) {
                ERR_POST_X(10, Warning <<
                               "Application has thrown an exception of unknown type");
                throw;
            }
#endif
        }
        else {
#ifdef NCBI_OS_MSWIN
            if ( !IsDebuggerPresent() ) {
                SuppressSystemMessageBox(fSuppress_Exception);
            }
#endif
            x_TryMain(diag, conf, &exit_code, &got_exception);
        }
    }
    catch (const CArgException& e) {
        // Print USAGE and the exception error message
        if ( e.GetErrCode() != CArgException::eNoValue &&  m_ArgDesc.get() ) {
            x_AddDefaultArgs();
            string str;
            if ( !m_ArgDesc->IsSetMiscFlag(CArgDescriptions::fNoUsage) ) {
                m_ArgDesc->PrintUsage(str);
                cerr << str;
            }
            if ( !m_ArgDesc->IsSetMiscFlag(CArgDescriptions::fDupErrToCerr) ) {
                CStreamDiagHandler* errh =
                    dynamic_cast<CStreamDiagHandler*>(GetDiagHandler());
                if (!errh  ||  errh->GetStream() != &cerr) {
                    cerr << "Error in command-line arguments. "
                        "See error logs for more details." << endl;
                }
            }
            else {
                cerr << "Error in command-line arguments." << endl;
                cerr << e.what() << endl;
            }
            cerr << string(72, '=') << endl << endl;
        }
        SetDiagPostAllFlags(eDPF_Severity);
        NCBI_REPORT_EXCEPTION_X(18, "", e);
        got_exception = true;
        exit_code = 1;
    }

    if (!diag_context.IsSetExitCode()) {
        diag_context.SetExitCode(exit_code);
    }

    if (m_ExitCodeCond == eAllExits
        ||  (got_exception  &&  m_ExitCodeCond == eExceptionalExits)) {
        _TRACE("Overriding exit code from " << exit_code
               << " to " << m_ExitCode);
        exit_code = m_ExitCode;
    }

    // Application stop
    AppStop(exit_code);

    if ((m_AppFlags & fSkipSafeStaticDestroy) == 0) {
        // Destroy short-lived statics
        CSafeStaticGuard::Destroy(CSafeStaticLifeSpan::eLifeLevel_AppMain);
    }

    // Exit
    return exit_code;
}


void CNcbiApplicationAPI::SetEnvironment(const string& name, const string& value)
{
    SetEnvironment().Set(name, value);
}

#if 0
void CNcbiApplicationAPI::SetVersionByBuild(int major)
{
    SetVersion(major, NStr::StringToInt(m_Version->GetBuildInfo().GetExtraValue(SBuildInfo::eStableComponentsVersion, "0")));
}

void CNcbiApplicationAPI::SetVersion(int major, int minor)
{
    SetVersion(major, minor, NStr::StringToInt(m_Version->GetBuildInfo().GetExtraValue(SBuildInfo::eTeamCityBuildNumber, "0")));
}

void CNcbiApplicationAPI::SetVersion(int major, int minor, int patch)
{
    m_Version->SetVersionInfo(major, minor, patch,
        m_Version->GetBuildInfo().GetExtraValue(SBuildInfo::eTeamCityProjectName));
}
#else
void CNcbiApplicationAPI::SetVersionByBuild(int major)
{
    m_Version->SetVersionInfo(major, NCBI_SC_VERSION_PROXY, NCBI_TEAMCITY_BUILD_NUMBER_PROXY);
}
#endif


void CNcbiApplicationAPI::SetVersion(const CVersionInfo& version)
{
    if ( s_IsApplicationStarted ) {
        ERR_POST_X(19, "SetVersion() should be used from constructor of " \
                       "CNcbiApplication derived class, see description");
    }
    m_Version->SetVersionInfo(new CVersionInfo(version));
}

void CNcbiApplicationAPI::SetVersion(const CVersionInfo& version,
        const SBuildInfo& build_info)
{
    if ( s_IsApplicationStarted ) {
        ERR_POST_X(19, "SetVersion() should be used from constructor of " \
                       "CNcbiApplication derived class, see description");
    }
    m_Version->SetVersionInfo(new CVersionInfo(version), build_info);
}

void CNcbiApplicationAPI::SetFullVersion( CRef<CVersionAPI> version)
{
    if ( s_IsApplicationStarted ) {
        ERR_POST_X(19, "SetFullVersion() should be used from constructor of "\
                       "CNcbiApplication derived class, see description");
    }
    m_Version.Reset( version );
}


CVersionInfo CNcbiApplicationAPI::GetVersion(void) const
{
    return m_Version->GetVersionInfo();
}

const CVersionAPI& CNcbiApplicationAPI::GetFullVersion(void) const
{
    return *m_Version;
}


void CNcbiApplicationAPI::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    m_ArgDesc.reset(arg_desc);

    if ( arg_desc ) {
        if ( !m_DisableArgDesc ) {
            for(CArgDescriptions* desc : m_ArgDesc->GetAllDescriptions()) {
                desc->AddDefaultFileArguments(m_DefaultConfig);
            }
        }
        m_Args.reset(arg_desc->CreateArgs(GetArguments()));
    } else {
        m_Args.reset();
    }
}


bool CNcbiApplicationAPI::SetupDiag(EAppDiagStream diag)
{
    CDiagContext::SetupDiag(diag, 0, eDCM_Flush, m_LogFile);
    return true;
}


bool CNcbiApplicationAPI::SetupDiag_AppSpecific(void)
{
    CDiagContext::SetupDiag(eDS_ToStderr, 0, eDCM_Flush, m_LogFile);
    return true;
}


bool CNcbiApplicationAPI::LoadConfig(CNcbiRegistry&        reg,
                                  const string*         conf,
                                  CNcbiRegistry::TFlags reg_flags)
{
    string basename (m_Arguments->GetProgramBasename(eIgnoreLinks));
    string basename2(m_Arguments->GetProgramBasename(eFollowLinks));
    CMetaRegistry::SEntry entry;

    if ( !conf ) {
        if (reg.IncludeNcbircIfAllowed(reg_flags)) {
            m_ConfigPath = CMetaRegistry::FindRegistry
                ("ncbi", CMetaRegistry::eName_RcOrIni);
        }
        m_ConfigLoaded = true;
        return false;
    } else if (conf->empty()) {
        entry = CMetaRegistry::Load(basename, CMetaRegistry::eName_Ini, 0,
                                    reg_flags, &reg);
        if ( !entry.registry  &&  basename2 != basename ) {
            entry = CMetaRegistry::Load(basename2, CMetaRegistry::eName_Ini, 0,
                                        reg_flags, &reg);
        }
        m_DefaultConfig = CDirEntry(entry.actual_name).GetName();
    } else {
        entry = CMetaRegistry::Load(*conf, CMetaRegistry::eName_AsIs, 0,
                                    reg_flags, &reg);
    }
    if ( !entry.registry ) {
        // failed; complain as appropriate
        string dir;
        CDirEntry::SplitPath(*conf, &dir, 0, 0);
        if (dir.empty()) {
            ERR_POST_X(11, Info <<
                           "Registry file of application \"" << basename
                           << "\" is not found");
        } else {
            NCBI_THROW(CAppException, eNoRegistry,
                       "Registry file \"" + *conf + "\" cannot be opened");
        }
        // still consider pulling in defaults from .ncbirc
        if (reg.IncludeNcbircIfAllowed(reg_flags)) {
            m_ConfigPath = CMetaRegistry::FindRegistry
                ("ncbi", CMetaRegistry::eName_RcOrIni);
        }
        m_ConfigLoaded = true;
        return false;
    } else if (entry.registry != static_cast<IRWRegistry*>(&reg)) {
        // should be impossible with new CMetaRegistry interface...
        if (&reg == m_Config  &&  reg.Empty()) {
            m_Config.Reset(dynamic_cast<CNcbiRegistry*>
                           (entry.registry.GetPointer()));
        } else {
            // copy into reg
            CNcbiStrstream str;
            entry.registry->Write(str);
            str.seekg(0);
            reg.Read(str);
        }
    }
    m_ConfigPath = entry.actual_name;
    m_ConfigLoaded = true;
    return true;
}


bool CNcbiApplicationAPI::LoadConfig(CNcbiRegistry& reg,
                                  const string*  conf)
{
    return LoadConfig(reg, conf, IRegistry::fWithNcbirc);
}


CNcbiApplicationAPI::EPreparseArgs
CNcbiApplicationAPI::PreparseArgs(int                /*argc*/,
                               const char* const* /*argv*/)
{
    return ePreparse_Continue;
}


void CNcbiApplicationAPI::DisableArgDescriptions(TDisableArgDesc disable)
{
    m_DisableArgDesc = disable;
}


void CNcbiApplicationAPI::HideStdArgs(THideStdArgs hide_mask)
{
    m_HideArgs = hide_mask;
}


void CNcbiApplicationAPI::SetStdioFlags(TStdioSetupFlags stdio_flags)
{
    // do not call this function more than once
    // and from places other than App constructor
    _ASSERT(m_StdioFlags == 0);
    m_StdioFlags = stdio_flags;
}


void CNcbiApplicationAPI::x_SetupStdio(void)
{
    if ((m_StdioFlags & fNoSyncWithStdio) != 0) {
        IOS_BASE::sync_with_stdio(false);
    }

    if ((m_StdioFlags & fDefault_CinBufferSize) == 0
#ifdef NCBI_OS_UNIX
        &&  !isatty(0)
#endif
        ) {
#if defined(NCBI_COMPILER_GCC)  &&  defined(NCBI_OS_SOLARIS)
        _ASSERT(!m_CinBuffer);
        // Ugly workaround for ugly interaction between g++ and Solaris C RTL
        const size_t kCinBufSize = 5120;
        m_CinBuffer = new char[kCinBufSize];
        cin.rdbuf()->pubsetbuf(m_CinBuffer, kCinBufSize);
#endif
    }
#ifdef NCBI_OS_MSWIN
    if ((m_StdioFlags & fBinaryCin) != 0) {
        NcbiSys_setmode(NcbiSys_fileno(stdin), O_BINARY);
    }
    if ((m_StdioFlags & fBinaryCout) != 0) {
        NcbiSys_setmode(NcbiSys_fileno(stdout), O_BINARY);
    }
#endif
}

void CNcbiApplicationAPI::x_AddDefaultArgs(void)
{
    THideStdArgs mask = m_DisableArgDesc ? (fHideAll & ~fHideFullHelp & ~fHideHelp) : m_HideArgs;
    for(CArgDescriptions* desc : m_ArgDesc->GetAllDescriptions())
    {
        desc->AddStdArguments(mask);
        mask = mask | fHideFullVersion | fHideVersion;
    }
}

void CNcbiApplicationAPI::SetProgramDisplayName(const string& app_name)
{
    if (app_name.empty()) return;
    m_ProgramDisplayName = app_name;
    // Also set app_name in the diag context
    if ( GetDiagContext().GetAppName().empty() ) {
        GetDiagContext().SetAppName(app_name);
    }
}


string CNcbiApplicationAPI::GetAppName(EAppNameType name_type, int argc,
                                    const char* const* argv)
{
    CNcbiApplicationGuard instance = InstanceGuard();
    string app_name;

    switch (name_type) {
    case eBaseName:
        if (instance) {
            app_name = instance->GetProgramDisplayName();
        } else {
            string exe_path = FindProgramExecutablePath(argc, argv);
            CDirEntry::SplitPath(exe_path, NULL, &app_name);
        }
        break;

    case eFullName:
        if (instance) {
            app_name = instance->GetProgramExecutablePath(eIgnoreLinks);
        } else {
            app_name = FindProgramExecutablePath(argc, argv);
        }
        break;

    case eRealName:
        if (instance) {
            app_name = instance->GetProgramExecutablePath(eFollowLinks);
        } else {
            FindProgramExecutablePath(argc, argv, &app_name);
        }
        break;
    }

    return app_name;
}


string CNcbiApplicationAPI::FindProgramExecutablePath
(int                           argc,
 const char* const*            argv,
 string*                       real_path)
{
    CNcbiApplicationGuard instance = InstanceGuard();
    string ret_val;
    _ASSERT(argc >= 0); // formally signed for historical reasons
    _ASSERT(argv != NULL  ||  argc == 0);
    if (argc > 0  &&  argv[0] != NULL  &&  argv[0][0] != '\0') {
        ret_val = argv[0];
    } else if (instance) {
        ret_val = instance->GetArguments().GetProgramName();
    }

#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)

#  ifdef NCBI_OS_MSWIN
    // MS Windows: Try more accurate method of detection
    // XXX - use this only for real_path?
    try {
        // Load PSAPI dynamic library -- it should exist on MS-Win NT/2000/XP
        CDll dll_psapi("psapi.dll", CDll::eLoadNow, CDll::eAutoUnload);

        // Get function entry-point from DLL
        BOOL  (STDMETHODCALLTYPE FAR * dllEnumProcessModules)
                (HANDLE  hProcess,     // handle to process
                 HMODULE *lphModule,   // array of module handles
                 DWORD   cb,           // size of array
                 LPDWORD lpcbNeeded    // number of bytes required
                 ) = NULL;

        dllEnumProcessModules =
            dll_psapi.GetEntryPoint_Func("EnumProcessModules",
                                         &dllEnumProcessModules);
        if ( !dllEnumProcessModules ) {
            NCBI_THROW(CException, eUnknown, kEmptyStr);
        }

        // Find executable file in the midst of all loaded modules
        HANDLE  process = GetCurrentProcess();
        HMODULE module  = 0;
        DWORD   needed  = 0;

        // Get first module of current process (it should be .exe file)
        if ( dllEnumProcessModules(process,
                                   &module, sizeof(HMODULE), &needed) ) {
            if ( needed  &&  module ) {
                TXChar buf[MAX_PATH + 1];
                DWORD ncount = GetModuleFileName(module, buf, MAX_PATH);
                if (ncount > 0) {
                    ret_val = _T_STDSTRING(buf);
                    if (real_path) {
                        *real_path = CDirEntry::NormalizePath(ret_val,
                                                              eFollowLinks);
                    }
                    return ret_val;
                }
            }
        }
    }
    catch (const CException&) {
        ; // Just catch an all exceptions from CDll
    }
    // This method didn't work -- use standard method
#  endif

#  ifdef NCBI_OS_LINUX
    // Linux OS: Try more accurate method of detection for real_path
    if (ret_val.empty()  &&  !real_path) {
        real_path = &ret_val;
    }
    if (real_path) {
        char   buf[PATH_MAX + 1];
        string procfile = "/proc/" + NStr::IntToString(getpid()) + "/exe";
        int    ncount   = (int)readlink((procfile).c_str(), buf, PATH_MAX);
        if (ncount > 0) {
            real_path->assign(buf, ncount);
            if (real_path == &ret_val  ||  ret_val.empty()) {
                // XXX - could also parse /proc/self/cmdline.
                return *real_path;
            }
            real_path = 0;
        }
    }
#  endif

    if (ret_val.empty()) {
        // nothing to go on :-/
        if (real_path) {
            real_path->erase();
        }
        return kEmptyStr;
    }
    string app_path = ret_val;

    if ( !CDirEntry::IsAbsolutePath(app_path) ) {
#  ifdef NCBI_OS_MSWIN
        // Add default ".exe" extention to the name of executable file
        // if it running without extension
        string dir, title, ext;
        CDirEntry::SplitPath(app_path, &dir, &title, &ext);
        if ( ext.empty() ) {
            app_path = CDirEntry::MakePath(dir, title, "exe");
        }
#  endif
        if ( CFile(app_path).Exists() ) {
            // Relative path from the the current directory
            app_path = CDir::GetCwd() + CDirEntry::GetPathSeparator()+app_path;
            if ( !CFile(app_path).Exists() ) {
                app_path = kEmptyStr;
            }
        } else {
            // Running from some path from PATH environment variable.
            // Try to determine that path.
            string env_path;
            if (instance) {
                env_path = instance->GetEnvironment().Get("PATH");
            } else {
                env_path = _T_STDSTRING(NcbiSys_getenv(_TX("PATH")));
            }
            list<string> split_path;
#  ifdef NCBI_OS_MSWIN
            NStr::Split(env_path, ";", split_path,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
#  else
            NStr::Split(env_path, ":", split_path,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
#  endif
            string base_name = CDirEntry(app_path).GetBase();
            ITERATE(list<string>, it, split_path) {
                app_path = CDirEntry::MakePath(*it, base_name);
                if ( CFile(app_path).Exists() ) {
                    break;
                }
                app_path = kEmptyStr;
            }
        }
    }
    ret_val = CDirEntry::NormalizePath
        ((app_path.empty() && argv != NULL && argv[0] != NULL) ? argv[0]
         : app_path);

#else  // defined (NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)

#  error "Unsupported platform, sorry -- please contact NCBI"
#endif
    if (real_path) {
        *real_path = CDirEntry::NormalizePath(ret_val, eFollowLinks);
    }
    return ret_val;
}


void CNcbiApplicationAPI::x_HonorStandardSettings( IRegistry* reg)
{
    if (reg == 0) {
        reg = m_Config.GetPointer();
        if (reg == 0)
            return;
    }

    CStackTrace::s_HonorSignalHandlingConfiguration();
    
    // [NCBI.MEMORY_FILL]
    CObject::SetAllocFillMode(reg->Get("NCBI", "MEMORY_FILL"));

    {{
        CSysLog* syslog = dynamic_cast<CSysLog*>(GetDiagHandler());
        if (syslog) {
            syslog->HonorRegistrySettings(reg);
        }
    }}

    // Debugging features

    // [DEBUG.DIAG_TRACE]
    if ( !reg->Get("DEBUG", DIAG_TRACE).empty() ) {
        SetDiagTrace(eDT_Enable, eDT_Enable);
    }

    // [DEBUG.ABORT_ON_THROW]
    if ( !reg->Get("DEBUG", ABORT_ON_THROW).empty() ) {
        SetThrowTraceAbort(true);
    }

    // [DEBUG.DIAG_POST_LEVEL]
    string post_level = reg->Get("DEBUG", DIAG_POST_LEVEL);
    if ( !post_level.empty() ) {
        EDiagSev sev;
        if (CNcbiDiag::StrToSeverityLevel(post_level.c_str(), sev)) {
            SetDiagFixedPostLevel(sev);
        }
    }

    // [DEBUG.MessageFile]
    string msg_file = reg->Get("DEBUG", DIAG_MESSAGE_FILE);
    if ( !msg_file.empty() ) {
        CDiagErrCodeInfo* info = new CDiagErrCodeInfo();
        if ( !info  ||  !info->Read(msg_file) ) {
            if ( info ) {
                delete info;
            }
            ERR_POST_X(12, Warning << "Applications message file \""
                           << msg_file
                           << "\" is not found");
        } else {
            SetDiagErrCodeInfo(info);
        }
    }

    // [DEBUG.GuardAgainstThreadsOnStaticDataDestruction]
    if ( !reg->GetBool("DEBUG", "GuardAgainstThreadsOnStaticDataDestruction", true, 0, IRegistry::eErrPost) ) {
        CSafeStaticGuard::DisableChildThreadsCheck();
    }

    // CPU and memory limitations

    // [NCBI.HeapSizeLimit]
    if ( !reg->Get("NCBI", "HeapSizeLimit").empty() ) {
        ERR_POST_X(13, Warning
                       << "Config param [NCBI.HeapSizeLimit] is deprecated,"
                       << "please use [NCBI.MemorySizeLimit] instead.");
        int mem_size_limit = reg->GetInt("NCBI", "HeapSizeLimit", 0);
        if (mem_size_limit < 0) {
            NCBI_THROW(CAppException, eLoadConfig,
                       "Configuration file error:  [NCBI.HeapSizeLimit] < 0");
        }
        SetMemoryLimit(size_t(mem_size_limit) * 1024 * 1024);
    }
    // [NCBI.MemorySizeLimit]
    if ( !reg->Get("NCBI", "MemorySizeLimit").empty() ) {
        size_t mem_size_limit = 0;
        string s = reg->GetString("NCBI", "MemorySizeLimit", kEmptyStr);
        size_t pos = s.find("%");
        if (pos != NPOS) {
            // Size in percents of total memory
            size_t percents = NStr::StringToUInt(CTempString(s, 0, pos));
            if (percents > 100) {
                NCBI_THROW(CAppException, eLoadConfig,
                           "Configuration file error:  [NCBI.HeapSizeLimit] > 100%");
            }
            mem_size_limit = (size_t)(CSystemInfo::GetTotalPhysicalMemorySize() * percents / 100);
        } else {
            try {
                // Size is specified in MiB by default if no suffixes
                // (converted without exception)
                mem_size_limit = NStr::StringToSizet(s) * 1024 * 1024;
            }
            catch (const CStringException&) {
                // Otherwise, size have suffix (MiB, G, GB, etc)
                Uint8 bytes = NStr::StringToUInt8_DataSize(s);
                if ( bytes > get_limits(mem_size_limit).max() ) {
                    NCBI_THROW(CAppException, eLoadConfig,
                               "Configuration file error:  [NCBI.MemorySizeLimit] is too big");
                }
                mem_size_limit = (size_t)bytes;
            }
        }
        SetMemoryLimit(mem_size_limit);
    }

    // [NCBI.CpuTimeLimit]
    if ( !reg->Get("NCBI", "CpuTimeLimit").empty() ) {
        int cpu_time_limit = reg->GetInt("NCBI", "CpuTimeLimit", 0);
        if (cpu_time_limit < 0) {
            NCBI_THROW(CAppException, eLoadConfig,
                       "Configuration file error:  [NCBI.CpuTimeLimit] < 0");
        }
        SetCpuTimeLimit((unsigned int)cpu_time_limit, 5, NULL, NULL);
    }

    // TRACE and POST filters

    try {
        // [DIAG.TRACE_FILTER]
        string trace_filter = reg->Get("DIAG", "TRACE_FILTER");
        if ( !trace_filter.empty() )
            SetDiagFilter(eDiagFilter_Trace, trace_filter.c_str());
    } NCBI_CATCH_X(20,
                   "Failed to load and set diag. filter for traces");

    try {
        // [DIAG.POST_FILTER]
        string post_filter = reg->Get("DIAG", "POST_FILTER");
        if ( !post_filter.empty() )
            SetDiagFilter(eDiagFilter_Post, post_filter.c_str());
    } NCBI_CATCH_X(21,
                   "Failed to load and set diag. filter for regular errors");
}


void CNcbiApplicationAPI::AppStart(void)
{
    string cmd_line = GetProgramExecutablePath();
    if ( m_Arguments.get() ) {
        if ( cmd_line.empty() ) {
            cmd_line = (*m_Arguments)[0];
        }
        for (SIZE_TYPE arg = 1; arg < m_Arguments->Size(); ++arg) {
            cmd_line += " ";
            cmd_line += NStr::ShellEncode((*m_Arguments)[arg]);
        }
    }

    // Print application start message
    if ( !CDiagContext::IsSetOldPostFormat() ) {
        GetDiagContext().PrintStart(cmd_line);
    }
}


void CNcbiApplicationAPI::AppStop(int exit_code)
{
    CDiagContext& ctx = GetDiagContext();
    if ( !ctx.IsSetExitCode() ) {
        ctx.SetExitCode(exit_code);
    }
}


void CNcbiApplicationAPI::SetExitCode(int exit_code, EExitMode when)
{
    m_ExitCode = exit_code;
    m_ExitCodeCond = when;
}

const char* CAppException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eUnsetArgs:  return "eUnsetArgs";
    case eSetupDiag:  return "eSetupDiag";
    case eLoadConfig: return "eLoadConfig";
    case eSecond:     return "eSecond";
    case eNoRegistry: return "eNoRegistry";
    default:    return CException::GetErrCodeString();
    }
}


void CDefaultIdler::Idle(void)
{
    DiagHandler_Reopen();
}


class CIdlerWrapper
{
public:
    CIdlerWrapper(void) : m_Idler(new CDefaultIdler()) {}
    ~CIdlerWrapper(void) {}

    INcbiIdler* GetIdler(EOwnership own);
    void SetIdler(INcbiIdler* idler, EOwnership own);
    void RunIdler(void);

private:
    CMutex              m_Mutex;
    AutoPtr<INcbiIdler> m_Idler;
};


inline
INcbiIdler* CIdlerWrapper::GetIdler(EOwnership own)
{
    CMutexGuard guard(m_Mutex);
    m_Idler.reset(m_Idler.release(), own);
    return m_Idler.get();
}


inline
void CIdlerWrapper::SetIdler(INcbiIdler* idler, EOwnership own)
{
    CMutexGuard guard(m_Mutex);
    m_Idler.reset(idler, own);
}


inline
void CIdlerWrapper::RunIdler(void)
{
    if ( m_Idler.get() ) {
        CMutexGuard guard(m_Mutex);
        if ( m_Idler.get() ) {
            m_Idler->Idle();
        }
    }
}


CSafeStatic<CIdlerWrapper> s_IdlerWrapper;

INcbiIdler* GetIdler(EOwnership ownership)
{
    return s_IdlerWrapper.Get().GetIdler(ownership);
}


void SetIdler(INcbiIdler* idler, EOwnership ownership)
{
    s_IdlerWrapper.Get().SetIdler(idler, ownership);
}


void RunIdler(void)
{
    s_IdlerWrapper.Get().RunIdler();
}


END_NCBI_SCOPE
