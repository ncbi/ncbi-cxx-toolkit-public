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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/syslog.hpp>

#if defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#  include <corelib/ncbidll.hpp>
#  include <io.h> 
#  include <fcntl.h> 
#endif

#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#endif
#if defined(NCBI_OS_DARWIN)
#  ifdef NCBI_COMPILER_METROWERKS
#    define __NOEXTENSIONS__
#  endif
#  include <Carbon/Carbon.h>
#endif


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  Constants
//

static const char* s_ArgLogFile = "-logfile";
static const char* s_ArgCfgFile = "-conffile";
static const char* s_ArgVersion = "-version";
static const char* s_ArgDryRun  = "-dryrun";


///////////////////////////////////////////////////////
// CNcbiApplication
//

CNcbiApplication* CNcbiApplication::m_Instance;


CNcbiApplication* CNcbiApplication::Instance(void)
{
    return m_Instance;
}


CNcbiApplication::CNcbiApplication(void)
{
    // Initialize UID and start timer
    GetDiagContext().GetUID();
    GetDiagContext().InitMessages(size_t(-1));

    m_DisableArgDesc = false;
    m_HideArgs = 0;
    m_StdioFlags = 0;
    m_CinBuffer = 0;
    m_ExitCodeCond = eNoExits;

    // Register the app. instance
    if ( m_Instance ) {
        NCBI_THROW(CAppException, eSecond,
                   "Second instance of CNcbiApplication is prohibited");
    }
    m_Instance = this;

    // Create empty version info
    m_Version.reset(new CVersionInfo(0,0));

    // Create empty application arguments & name
    m_Arguments.reset(new CNcbiArguments(0,0));

    // Create empty application environment
    m_Environ.reset(new CNcbiEnvironment);

    // Create an empty registry
    m_Config.Reset(new CNcbiRegistry);
    
    m_DryRun = false;
}


CNcbiApplication::~CNcbiApplication(void)
{
    m_Instance = 0;
    FlushDiag(0, true);
    if (m_CinBuffer) {
        delete [] m_CinBuffer;
    }

#if defined(NCBI_COMPILER_WORKSHOP)
    // At least under these conditions:
    //  1) WorkShop 5.5 on Solaris 10/SPARC, Release64MT,    and
    //  2) when IOS_BASE::sync_with_stdio(false) is called,  and
    //  3) the contents of 'cout' is not flashed
    // some applications crash on exit() while apparently trying to
    // flush 'cout' and getting confused by its own guts, with error:
    //   "*** libc thread failure: _thread_setschedparam_main() fails"
    //
    // This forced pre-flush trick seems to fix the problem.
    NcbiCout.flush();
#endif
}


void CNcbiApplication::Init(void)
{
    return;
}


int CNcbiApplication::DryRun(void)
{
    ERR_POST(Info << "DryRun: default implementation does nothing");
    return 0;
}


void CNcbiApplication::Exit(void)
{
    return;
}


const CArgs& CNcbiApplication::GetArgs(void) const
{
    if ( !m_Args.get() ) {
        NCBI_THROW(CAppException, eUnsetArgs,
                   "Command-line argument description is not found");
    }
    return *m_Args;
}


SIZE_TYPE CNcbiApplication::FlushDiag(CNcbiOstream* os, bool close_diag)
{
    if ( os ) {
        SetDiagStream(os, true, 0, 0, "STREAM");
    }
    GetDiagContext().FlushMessages(*GetDiagHandler());
    GetDiagContext().DiscardMessages();
    return 0;
}


#if defined(NCBI_OS_DARWIN)
static void s_MacArgMunging(CNcbiApplication&   app,
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
        ERR_POST(Info << "Mac arguments file not found: " << args_fname);
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


int CNcbiApplication::AppMain
(int                argc,
 const char* const* argv,
 const char* const* envp,
 EAppDiagStream     diag,
 const char*        conf,
 const string&      name)
{
    x_SetupStdio();

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
    SetProgramDisplayName(appname);

    // Make sure we have something as our 'real' executable's name.
    // though if it does not contain a full path it won't be much use.
    if ( exepath.empty() ) {
        ERR_POST(Warning
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

    // Check command line for presence special arguments
    // "-logfile", "-conffile", "-version"
    bool is_diag_setup = false;
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
                if ( !argv[i++] ) {
                    continue;
                }
                v[real_arg_index++] = argv[i - 1];
                v[real_arg_index++] = argv[i];
                if (SetLogFile(argv[i], eDiagFile_All, true)) {
                    diag = eDS_User;
                    is_diag_setup = true;
                }
                // Configuration file
            } else if ( NStr::strcmp(argv[i], s_ArgCfgFile) == 0 ) {
                if ( !argv[i++] ) {
                    continue;
                }
                v[real_arg_index++] = argv[i - 1];
                v[real_arg_index++] = argv[i];
                conf = argv[i];

                // Version
            } else if ( NStr::strcmp(argv[i], s_ArgVersion) == 0 ) {
                if ( !argv[i++] ) {
                    continue;
                }
                // Print USAGE
                LOG_POST(appname + ": " + GetVersion().Print());
                GetDiagContext().DiscardMessages();
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

    // Setup for diagnostics
    try {
        if ( !is_diag_setup ) {
            CDiagContext::SetupDiag(diag);
        }
    } catch (CException& e) {
        NCBI_RETHROW(e, CAppException, eSetupDiag,
                     "Application diagnostic stream's setup failed");
    } catch (exception& e) {
        NCBI_THROW(CAppException, eSetupDiag,
                   "Application diagnostic stream's setup failed: " +
                   string(e.what()));
    }

    // Call:  Init() + Run() + Exit()
    int exit_code = 1;
    bool got_exception = false;

    try {
        // Initialize the application
        try {
            // Load registry from the config file
            if ( conf ) {
                string x_conf(conf);
                LoadConfig(*m_Config, &x_conf);
            } else {
                LoadConfig(*m_Config, NULL);
            }

            CDiagContext::SetupDiag(diag, m_Config, eDCM_Flush);

            // Setup the standard features from the config file.
            // Don't call till after LoadConfig()
            // NOTE: this will override environment variables, 
            // except DIAG_POST_LEVEL which is Set*Fixed*.
            x_HonorStandardSettings();

            // Application start
            AppStart();

            // Do init
#if (defined(NCBI_COMPILER_ICC) && NCBI_COMPILER_VERSION < 900)
            // ICC 8.0 have an optimization bug in exceptions handling,
            // so workaround it here
            try {            
                Init();
            }
            catch (CArgHelpException& ) {
                throw;
            }            
#else
            Init();
#endif

            // If the app still has no arg description - provide default one
            if (!m_DisableArgDesc  &&  !m_ArgDesc.get()) {
                auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
                arg_desc->SetUsageContext
                    (GetArguments().GetProgramBasename(),
                     "This program has no mandatory arguments");
                SetupArgDescriptions(arg_desc.release());
            }
        }
        catch (CArgHelpException& e) {
            if ( !m_DisableArgDesc ) {
                if ((m_HideArgs & fHideHelp) != 0) {
                    if (m_ArgDesc->Exist("h")) {
                        m_ArgDesc->Delete("h");
                    }
                }
                if ((m_HideArgs & fHideLogfile) == 0  &&
                    !m_ArgDesc->Exist(s_ArgLogFile + 1)) {
                    m_ArgDesc->AddOptionalKey
                        (s_ArgLogFile+1, "File_Name",
                         "File to which the program log should be redirected",
                         CArgDescriptions::eOutputFile);
                }
                if ((m_HideArgs & fHideConffile) == 0  &&
                    !m_ArgDesc->Exist(s_ArgCfgFile + 1)) {
                    m_ArgDesc->AddOptionalKey
                        (s_ArgCfgFile + 1, "File_Name",
                         "Program's configuration (registry) data file",
                         CArgDescriptions::eInputFile);
                }
                if ((m_HideArgs & fHideVersion) == 0  &&
                    !m_ArgDesc->Exist(s_ArgVersion + 1)) {
                    m_ArgDesc->AddFlag
                        (s_ArgVersion + 1,
                         "Print version number;  ignore other arguments");
                }
                if ((m_HideArgs & fHideDryRun) == 0  &&
                    !m_ArgDesc->Exist(s_ArgDryRun + 1)) {
                    m_ArgDesc->AddFlag
                        (s_ArgDryRun + 1,
                         "Dry run the application: do nothing, only test all preconditions");
                }
            }
            // Print USAGE
            string str;
            m_ArgDesc->PrintUsage
                (str, e.GetErrCode() == CArgHelpException::eHelpFull);
            // LOG_POST(str);
            cout << str;
            exit_code = 0;
        }
        catch (CArgException& e) {
            NCBI_RETHROW_SAME(e, "Application's initialization failed");
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("Application's initialization failed", e);
            got_exception = true;
            exit_code = 2;
        }
        catch (exception& e) {
            ERR_POST("Application's initialization failed: " << e.what());
            got_exception = true;
            exit_code = 2;
        }

        // Run application
        if (exit_code == 1) {
            try {
                exit_code = m_DryRun ? DryRun() : Run();
            }
            catch (CArgException& e) {
                NCBI_RETHROW_SAME(e, "Application's execution failed");
            }
            catch (CException& e) {
                NCBI_REPORT_EXCEPTION("Application's execution failed", e);
                got_exception = true;
                exit_code = 3;
            }
            catch (exception& e) {
                ERR_POST("Application's execution failed: " << e.what());
                got_exception = true;
                exit_code = 3;
            }
        }

        // Close application
        try {
            Exit();
        }
        catch (CArgException& e) {
            NCBI_RETHROW_SAME(e, "Application's cleanup failed");
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("Application's cleanup failed", e);
            got_exception = true;
        }
        catch (exception& e) {
            ERR_POST("Application's cleanup failed: "<< e.what());
            got_exception = true;
        }

    }
    catch (CArgException& e) {
        // Print USAGE and the exception error message
        if ( m_ArgDesc.get() ) {
            string str;
            LOG_POST(m_ArgDesc->PrintUsage(str) << string(72, '='));
        }
        NCBI_REPORT_EXCEPTION("", e);
        got_exception = true;
        exit_code = 1;
    }
    catch (...) {
        // MSVC++ 6.0 in Debug mode does not call destructors when
        // unwinding the stack unless the exception is caught at least
        // somewhere.
        ERR_POST(Warning <<
                 "Application has thrown an exception of unknown type");
        throw;
    }

    if (m_ExitCodeCond == eAllExits
        ||  (got_exception  &&  m_ExitCodeCond == eExceptionalExits)) {
        _TRACE("Overriding exit code from " << exit_code
               << " to " << m_ExitCodeCond);
        exit_code = m_ExitCode;
    }

    // Application stop
    AppStop(exit_code);

    // Exit
    return exit_code;
}


void CNcbiApplication::SetEnvironment(const string& name, const string& value)
{
    SetEnvironment().Set(name, value);
}


void CNcbiApplication::SetVersion(const CVersionInfo& version)
{
    m_Version.reset(new CVersionInfo(version));
}


CVersionInfo CNcbiApplication::GetVersion(void) const
{
    return *m_Version;
}


void CNcbiApplication::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    m_ArgDesc.reset(arg_desc);

    if ( arg_desc ) {
        if ( !m_DisableArgDesc ) {
            // Add logfile and conffile arguments
            if ((m_HideArgs & fHideLogfile) == 0  &&
                !m_ArgDesc->Exist(s_ArgLogFile + 1) ) {
                m_ArgDesc->AddOptionalKey
                    (s_ArgLogFile+1, "File_Name",
                        "File to which the program log should be redirected",
                        CArgDescriptions::eOutputFile);
            }
            if ((m_HideArgs & fHideConffile) == 0  &&
                !m_ArgDesc->Exist(s_ArgCfgFile + 1) ) {
                m_ArgDesc->AddOptionalKey
                    (s_ArgCfgFile + 1, "File_Name",
                        "Program's configuration (registry) data file",
                        CArgDescriptions::eInputFile);
            }
        }
        m_Args.reset(arg_desc->CreateArgs(GetArguments()));
    } else {
        m_Args.reset();
    }
}


bool CNcbiApplication::SetupDiag(EAppDiagStream diag)
{
    CDiagContext::SetupDiag(diag, 0, eDCM_Flush);
    return true;
}


bool CNcbiApplication::SetupDiag_AppSpecific(void)
{
    CDiagContext::SetupDiag(eDS_ToStderr, 0, eDCM_Flush);
    return true;
}


bool CNcbiApplication::LoadConfig(CNcbiRegistry&        reg,
                                  const string*         conf,
                                  CNcbiRegistry::TFlags reg_flags)
{
    string basename (m_Arguments->GetProgramBasename(eIgnoreLinks));
    string basename2(m_Arguments->GetProgramBasename(eFollowLinks));
    CMetaRegistry::SEntry entry;

    if ( !conf ) {
        return false;
    } else if (conf->empty()) {
        entry = CMetaRegistry::Load(basename, CMetaRegistry::eName_Ini, 0,
                                    reg_flags, &reg);
        if ( !entry.registry  &&  basename2 != basename ) {
            entry = CMetaRegistry::Load(basename2, CMetaRegistry::eName_Ini, 0,
                                        reg_flags, &reg);
        }
    } else {
        entry = CMetaRegistry::Load(*conf, CMetaRegistry::eName_AsIs, 0,
                                    reg_flags, &reg);
    }
    if ( !entry.registry ) {
        // failed; complain as appropriate
        string dir;
        CDirEntry::SplitPath(*conf, &dir, 0, 0);
        if (dir.empty()) {
            ERR_POST(Warning <<
                     "Registry file of application \"" << basename
                     << "\" is not found");
        } else {
            NCBI_THROW(CAppException, eNoRegistry,
                       "Registry file \"" + *conf + "\" cannot be opened");
        }
        // still consider pulling in defaults from .ncbirc
        reg.IncludeNcbircIfAllowed(reg_flags);
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
    return true;
}


bool CNcbiApplication::LoadConfig(CNcbiRegistry& reg,
                                  const string*  conf)
{
    return LoadConfig(reg, conf, IRegistry::fWithNcbirc);
}


void CNcbiApplication::DisableArgDescriptions(TDisableArgDesc disable)
{
    m_DisableArgDesc = disable;
}


void CNcbiApplication::HideStdArgs(THideStdArgs hide_mask)
{
    m_HideArgs = hide_mask;
}


void CNcbiApplication::SetStdioFlags(TStdioSetupFlags stdio_flags)
{
    // do not call this function more than once
    // and from places other than App constructor
    _ASSERT(m_StdioFlags == 0);
    m_StdioFlags = stdio_flags;
}


void CNcbiApplication::x_SetupStdio(void)
{
    if ((m_StdioFlags & fDefault_SyncWithStdio) == 0) {
        // SUN WorkShop STL stream library has significant performance loss
        // (due to the multiple gratuitous lseeks() in std i/o)
        // when sync_with_stdio is TRUE (default),
        // so we turn off sync_with_stdio here.
        IOS_BASE::sync_with_stdio(false);
    }

    if ((m_StdioFlags & fDefault_CinBufferSize) == 0
#ifdef NCBI_OS_UNIX
        &&  !isatty(0)
#endif
        ) {
#if defined(NCBI_COMPILER_GCC)  &&  defined(NCBI_OS_SOLARIS)
#  if NCBI_COMPILER_VERSION >= 300
        _ASSERT(!m_CinBuffer);
        // Ugly workaround for ugly interaction between g++ and Solaris C RTL
        const size_t kCinBufSize = 5120;
        m_CinBuffer = new char[kCinBufSize];
        cin.rdbuf()->pubsetbuf(m_CinBuffer, kCinBufSize);
#  endif
#endif
    }
#ifdef NCBI_OS_MSWIN
    if ((m_StdioFlags & fBinaryCin) != 0) {
        setmode(fileno(stdin), O_BINARY);
    }
    if ((m_StdioFlags & fBinaryCout) != 0) {
        setmode(fileno(stdout), O_BINARY);
    }
#endif
}


void CNcbiApplication::SetProgramDisplayName(const string& app_name)
{
    m_ProgramDisplayName = app_name;
    // Also set app_name property in the diag context
    GetDiagContext().SetProperty(
        CDiagContext::kProperty_AppName, app_name);
}


string CNcbiApplication::FindProgramExecutablePath
(int                _DEBUG_ARG(argc), 
 const char* const*            argv,
 string*                       real_path)
{
    string ret_val;
#if defined (NCBI_OS_DARWIN)  &&  defined (NCBI_COMPILER_METROWERKS)
    // We don't want to impose a dependency on Carbon when building
    // with GCC, since linking the framework *at all* makes remote
    // execution impossible. :-/
    OSErr               err;
    ProcessSerialNumber psn;
    FSRef               fsRef;
    char                filePath[1024];

    err = GetCurrentProcess(&psn);
    if (err == noErr) {
        err = GetProcessBundleLocation(&psn, &fsRef);
        if (err == noErr) {
            err = FSRefMakePath(&fsRef, (UInt8*) filePath, sizeof(filePath));
        }
    }    
    ret_val = filePath;

#elif defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)

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
                char buf[MAX_PATH + 1];
                DWORD ncount = GetModuleFileName(module, buf, MAX_PATH);
                if (ncount > 0) {
                    ret_val.assign(buf, ncount);
                    if (real_path) {
                        *real_path = CDirEntry::NormalizePath(ret_val,
                                                              eFollowLinks);
                    }
                    return ret_val;
                }
            }
        }
    }
    catch (CException) {
        ; // Just catch an all exceptions from CDll
    }
    // This method didn't work -- use standard method
#  endif

#  ifdef NCBI_OS_LINUX
    // Linux OS: Try more accurate method of detection for real_path
    if (real_path) {
        char   buf[PATH_MAX + 1];
        string procfile = "/proc/" + NStr::IntToString(getpid()) + "/exe";
        int    ncount   = readlink((procfile).c_str(), buf, PATH_MAX);
        if (ncount > 0) {
            real_path->assign(buf, ncount);
            real_path = 0;
        }
    }
#  endif

    _ASSERT(argc  &&  argv);
    string app_path = argv[0];

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
            string env_path = GetEnvironment().Get("PATH");
            list<string> split_path;
#  ifdef NCBI_OS_MSWIN
            NStr::Split(env_path, ";", split_path);
#  else
            NStr::Split(env_path, ":", split_path);
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
    ret_val = CDirEntry::NormalizePath(app_path.empty() ? argv[0] : app_path);

#else  // defined (NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)

#  error "Unsupported platform, sorry -- please contact NCBI"
#endif
    if (real_path) {
        *real_path = CDirEntry::NormalizePath(ret_val, eFollowLinks);
    }
    return ret_val;
}


void CNcbiApplication::x_HonorStandardSettings( IRegistry* reg)
{
    if (reg == 0) {
        reg = m_Config.GetPointer();
        if (reg == 0)
            return;
    }

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
            ERR_POST(Warning << "Applications message file \""
                     << msg_file
                     << "\" is not found");
        } else {
            SetDiagErrCodeInfo(info);
        }
    }

    // CPU and heap limitations

    // [NCBI.HeapSizeLimit]
    if ( !reg->Get("NCBI", "HeapSizeLimit").empty() ) {
        int heap_size_limit = reg->GetInt("NCBI", "HeapSizeLimit", 0);
        if (heap_size_limit < 0) {
            NCBI_THROW(CAppException, eLoadConfig,
                       "Configuration file error:  [NCBI.HeapSizeLimit] < 0");
        }
        heap_size_limit *= 1024 * 1024;
        if ( !SetHeapLimit(heap_size_limit) ) {
            ERR_POST(Warning
                     << "Failed to set the heap size limit to "
                     << heap_size_limit
                     << "Mb (as per the config param [NCBI.HeapSizeLimit])");
        }
    }
    
    // [NCBI.CpuTimeLimit]
    if ( !reg->Get("NCBI", "CpuTimeLimit").empty() ) {
        int cpu_time_limit = reg->GetInt("NCBI", "CpuTimeLimit", 0);
        if (cpu_time_limit < 0) {
            NCBI_THROW(CAppException, eLoadConfig,
                       "Configuration file error:  [NCBI.CpuTimeLimit] < 0");
        }
        if ( !SetCpuTimeLimit(cpu_time_limit) ) {
            ERR_POST(Warning
                     << "Failed to set the CPU time limit to "
                     << cpu_time_limit
                     << " sec (as per the config param [NCBI.CpuTimeLimit])");
        }
    }

    // TRACE and POST filters

    // [DIAG.TRACE_FILTER]
    string trace_filter = reg->Get("DIAG", "TRACE_FILTER");
    if ( !trace_filter.empty() )
        SetDiagFilter(eDiagFilter_Trace, trace_filter.c_str());

    // [DIAG.POST_FILTER]
    string post_filter = reg->Get("DIAG", "POST_FILTER");
    if ( !post_filter.empty() )
        SetDiagFilter(eDiagFilter_Post, post_filter.c_str());
}


void CNcbiApplication::AppStart(void)
{
    string args;
    if ( m_Arguments.get() ) {
        for (SIZE_TYPE arg = 0; arg < m_Arguments->Size(); ++arg) {
            if (arg > 0) {
                args += " ";
            }
            args += (*m_Arguments)[arg];
        }
    }

    // Print application start message
    if ( !CDiagContext::IsSetOldPostFormat() ) {
        GetDiagContext().PrintStart(NStr::PrintableString(args));
    }
}


void CNcbiApplication::AppStop(int exit_code)
{
    GetDiagContext().SetProperty(CDiagContext::kProperty_ExitCode,
        NStr::IntToString(exit_code));
}


void CNcbiApplication::SetExitCode(int exit_code, EExitMode when)
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

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.140  2006/12/05 15:23:41  grichenk
 * Check fHideLogfile when adding logfile and conffile args.
 * Discard log messages after printing version.
 *
 * Revision 1.139  2006/11/30 21:32:17  grichenk
 * Automatically add logfile and conffile arguments if they do not exist.
 *
 * Revision 1.138  2006/11/29 13:56:29  gouriano
 * Moved GetErrorCodeString method into cpp
 *
 * Revision 1.137  2006/11/16 20:16:55  grichenk
 * Log open mode controlled by CParam.
 * Report switching handlers only if messages have been printed.
 * Disable diagnostics if log file is /dev/null.
 *
 * Revision 1.136  2006/11/15 19:20:42  ivanov
 * Added workaround for ICC 8.0 optimization bug in exceptions handling
 *
 * Revision 1.135  2006/10/31 18:41:17  grichenk
 * Redesigned diagnostics setup.
 * Moved the setup function to ncbidiag.cpp.
 *
 * Revision 1.134  2006/09/27 17:49:56  grichenk
 * Fixed 'unused variable' warning.
 *
 * Revision 1.133  2006/09/18 18:12:06  grichenk
 * Reset log file name if failed to open logs with default names.
 *
 * Revision 1.132  2006/09/18 15:01:56  grichenk
 * Fixed log file creation. Check if log dir exists.
 *
 * Revision 1.131  2006/09/12 20:38:55  grichenk
 * More warnings from SetLogFile().
 * Fixed log file path.
 *
 * Revision 1.130  2006/09/12 15:02:04  grichenk
 * Fixed log file name extensions.
 * Added GetDiagStream().
 *
 * Revision 1.129  2006/09/08 15:33:41  grichenk
 * Flush data from memory stream when switching to log file.
 *
 * Revision 1.128  2006/09/07 20:10:32  grichenk
 * Do not add '.log' to the log file name.
 *
 * Revision 1.127  2006/08/30 18:08:13  ucko
 * Load global registry settings in addition to program-specific ones by
 * default (which can be overridden in various ways).
 * Ask CSysLog to honor registry settings (for the default facility).
 *
 * Revision 1.126  2006/08/29 15:29:39  gouriano
 * Added dryrun option
 *
 * Revision 1.125  2006/06/29 16:02:21  grichenk
 * Added constants for setting CDiagContext properties.
 *
 * Revision 1.124  2006/06/09 15:46:06  ucko
 * Add a protected SetExitCode method that can be used to force AppMain
 * to return a specified value, either unconditionally or only on
 * uncaught exceptions.
 *
 * Revision 1.123  2006/06/06 20:57:37  grichenk
 * Removed s_DiagToStdlog_Cleanup().
 *
 * Revision 1.122  2006/05/23 18:49:41  grichenk
 * Fixed warning
 *
 * Revision 1.121  2006/05/18 19:07:27  grichenk
 * Added output to log file(s), application access log, new cgi log formatting.
 *
 * Revision 1.120  2006/02/27 15:14:59  ucko
 * Drop explicit inclusion of <math.h> on Darwin, which should be
 * unnecessary now that __NOEXTENSIONS__ is defined only for CodeWarrior.
 *
 * Revision 1.119  2006/02/27 13:53:15  rsmith
 * Only define _NOEXTENSIONS_ on Mac Codewarrior builds.
 *
 * Revision 1.118  2006/02/24 21:57:44  ucko
 * Darwin: include <math.h> before __NOEXTENSIONS__ is defined, as the
 * subsequent inclusion of Carbon.h can otherwise yield broken math inlines.
 *
 * Revision 1.117  2006/02/16 15:50:18  lavr
 * Use ostrstream's freeze(), not strstreambuf's one
 *
 * Revision 1.116  2006/02/16 15:18:18  lavr
 * Replace use of PUBSEEKOFF macro with equiv. stream positioning method
 *
 * Revision 1.115  2006/02/16 13:18:50  lavr
 * SEEKOFF -> PUBSEEKOFF (SEEKOFF has been made obsolescent a long ago)
 *
 * Revision 1.114  2006/01/09 15:59:52  vakatov
 * CNcbiApplication::x_SetupStdio() -- work around the STDIO destruction
 * glitch that appeared on WorkShop 5.5 on Solaris 10/SPARC
 *
 * Revision 1.113  2005/12/21 18:17:04  grichenk
 * Fixed output of module/class/function by adding GetRef().
 * Fixed width for UID.
 * Use source directory as module name if no module is set at compile time.
 *
 * Revision 1.112  2005/12/15 20:25:07  grichenk
 * Set AppName diag context property.
 *
 * Revision 1.111  2005/11/17 18:47:18  grichenk
 * Replaced GetConfigXXX with CParam<>.
 *
 * Revision 1.110  2005/07/27 15:28:08  ucko
 * New EAppDiagStream option: eDS_ToSyslog (tunable via openlog())
 *
 * Revision 1.109  2005/07/27 14:25:52  didenko
 * Changed the signature of DisableArgDescriptions method
 *
 * Revision 1.108  2005/07/05 15:53:30  ucko
 * If the user explicitly requested help, print it on standard output
 * rather than logging it (to standard error, typically).
 *
 * Revision 1.107  2005/05/12 15:15:32  ucko
 * Fix some (meta)registry buglets and add support for reloading.
 *
 * Revision 1.106  2005/05/04 14:20:22  kapustin
 * Make GetVersion() const
 *
 * Revision 1.105  2005/03/10 18:01:53  vakatov
 * Moved CNcbiApplication::GetArgs() implementation to here from the header
 *
 * Revision 1.104  2005/02/11 16:04:11  gouriano
 * Distinguish short and detailed help message
 * print detailed one only when requested
 *
 * Revision 1.103  2005/01/10 21:44:21  ucko
 * Add one more tweak required by WorkShop.
 *
 * Revision 1.102  2005/01/10 16:58:38  ucko
 * Reflect recent changes to CMetaRegistry's interface to take advantage
 * of CNcbiRegistry's refactoring.
 *
 * Revision 1.101  2005/01/05 18:45:29  vasilche
 * Added GetConfigXxx() functions.
 *
 * Revision 1.100  2004/12/20 16:44:14  ucko
 * Take advantage of the fact that CNcbiRegistry is now a CObject, and
 * generalize x_HonorStandardSettings to accept any IRegistry.
 *
 * Revision 1.99  2004/10/20 14:16:24  gouriano
 * Give access to logfile name
 *
 * Revision 1.98  2004/10/18 18:59:19  gouriano
 * Allow to turn the logging on from the config.file
 *
 * Revision 1.97  2004/09/29 13:40:39  ivanov
 * Changed standard exit code to positive values.
 * Exit codes must be in range 0..255.
 *
 * Revision 1.96  2004/09/24 17:47:46  gouriano
 * Enable treating standard input and output as binary
 *
 * Revision 1.95  2004/09/22 13:32:17  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 *  CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.94  2004/08/09 20:05:32  ucko
 * FindProgramExecutablePath: On Linux, consult PATH_MAX, not FILENAME_MAX,
 * which is less relevant and might not have been defined (via stdio.h).
 *
 * Revision 1.93  2004/08/09 19:12:19  ucko
 * {Find,Get}ProgramExecutablePath: make fully resolved paths available too,
 * using the /proc method on Linux.
 * AppMain: cache them here and in CNcbiArguments.
 *
 * Revision 1.92  2004/08/06 11:19:47  ivanov
 * + CNcbiApplication::GetProgramExecutablePath()
 *
 * Revision 1.91  2004/07/08 14:10:11  lavr
 * Some comment spellings
 *
 * Revision 1.90  2004/07/04 18:34:30  vakatov
 * HonorDebugSettings() --> x_HonorStandardSettings()
 * the latter also allows to limit max CPU usage and max heap size
 *
 * Revision 1.89  2004/06/18 18:44:42  vakatov
 * CNcbiApplication::AppMain() not to printout line of '=' before USAGE
 * (in case of invalid args)
 *
 * Revision 1.88  2004/06/15 17:54:15  vakatov
 * Indentation
 *
 * Revision 1.87  2004/06/15 16:12:47  ivanov
 * Moved loading registry from the config file into general try/catch block
 *
 * Revision 1.86  2004/06/15 14:20:31  lavr
 * Spelling fix for "caught"
 *
 * Revision 1.85  2004/06/02 20:45:27  vakatov
 * CNcbiApplication::AppMain() not to printout line of '=' before USAGE
 *
 * Revision 1.84  2004/05/14 13:59:26  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.83  2004/03/02 15:57:34  ucko
 * Downgrade severity of missing Mac arguments file to Info, since Darwin
 * allows actual command lines.
 *
 * Revision 1.82  2004/01/06 18:17:49  dicuccio
 * Added APIs for setting environment variables
 *
 * Revision 1.81  2003/12/17 20:25:46  ucko
 * x_SetupStdio: for the sake of interactive applications, don't buffer
 * cin if it's a terminal.
 *
 * Revision 1.80  2003/11/21 21:03:37  vakatov
 * Cosmetics
 *
 * Revision 1.79  2003/11/21 20:12:20  kuznets
 * Minor clean-up
 *
 * Revision 1.78  2003/11/21 19:53:58  kuznets
 * Added catch(...) to intercept all exceptions and give compiler
 * (MSVC) a chance to call destructors even if this exception
 * will never be handled and causes crash.
 *
 * Revision 1.77  2003/11/19 13:54:19  ivanov
 * FindProgramExecutablePath: Replaced GetEntryPoint with GetEntryPoint_Func
 *
 * Revision 1.76  2003/10/10 19:37:13  lavr
 * Ugly workaround for G++/Solaris C RTL (FILE*) interactions that affect
 * stdio constructed on pipes (sockets).  To be fully investigated later...
 *
 * Revision 1.75  2003/10/01 14:32:09  ucko
 * +EFollowLinks
 *
 * Revision 1.74  2003/09/30 21:12:30  ucko
 * CNcbiApplication::LoadConfig: if run through a symlink and there's no
 * configuration file for the link's source, try its (ultimate) target.
 *
 * Revision 1.73  2003/09/29 20:28:00  vakatov
 * + LoadConfig(...., reg_flags)
 *
 * Revision 1.72  2003/09/25 19:34:51  ucko
 * FindProgramExecutablePath: disable Linux-specific logic, since it
 * loses the ability to detect having been run through a symlink.
 *
 * Revision 1.71  2003/09/25 13:33:58  rsmith
 * NCBI_OS_DARWIN and NCBI_OS_UNIX are not mutually exclusive.
 *
 * Revision 1.70  2003/09/22 13:43:41  ivanov
 * Include <corelib/ncbidll.hpp> only on MSWin.
 * + include <corelib/ncbi_os_mswin.hpp>
 *
 * Revision 1.69  2003/09/17 21:03:58  ivanov
 * FindProgramExecutablePath:  try more accurate method of detection on
 * MS Windows
 *
 * Revision 1.68  2003/09/16 20:55:33  ivanov
 * FindProgramExecutablePath:  try more accurate method of detection on Linux.
 *
 * Revision 1.67  2003/09/16 16:35:38  ivanov
 * FindProgramExecutablePath(): added implementation for UNIX and MS-Windows
 *
 * Revision 1.66  2003/08/06 20:27:17  ucko
 * LoadConfig: take advantage of the latest changes to CMetaRegistry to
 * reuse reg.
 *
 * Revision 1.65  2003/08/06 14:31:55  ucko
 * LoadConfig: Only replace m_Config if it was empty, and remember to
 * delete the old object if we owned it.
 *
 * Revision 1.64  2003/08/05 20:00:36  ucko
 * Completely rewrite LoadConfig to use CMetaRegistry; properly handle
 * only sometimes owning m_Config.
 * ===========================================================================
 */
