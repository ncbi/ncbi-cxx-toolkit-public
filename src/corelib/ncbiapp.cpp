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
#include <corelib/ncbienv.hpp>
#include <corelib/metareg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbiapp.hpp>

#if defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#  include <corelib/ncbidll.hpp>
#endif

#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#endif
#if defined(NCBI_OS_DARWIN)
#  define __NOEXTENSIONS__
#  include <Carbon/Carbon.h>
#endif


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  Constants
//

static const char* s_ArgLogFile = "-logfile";
static const char* s_ArgCfgFile = "-conffile";
static const char* s_ArgVersion = "-version";


static void s_DiagToStdlog_Cleanup(void* data)
{
    // SetupDiag(eDS_ToStdlog)
    CNcbiOfstream* os_log = static_cast<CNcbiOfstream*> (data);
    delete os_log;
}



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
    m_DisableArgDesc = false;
    m_HideArgs = 0;
    m_StdioFlags = 0;
    m_CinBuffer = 0;

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
    m_Config = new CNcbiRegistry;
    m_OwnsConfig = true;

}


CNcbiApplication::~CNcbiApplication(void)
{
    m_Instance = 0;
    FlushDiag(0, true);
    if (m_CinBuffer) {
        delete [] m_CinBuffer;
    }
    if (m_OwnsConfig) {
        delete m_Config;
    }
}


void CNcbiApplication::Init(void)
{
    return;
}


void CNcbiApplication::Exit(void)
{
    return;
}


SIZE_TYPE CNcbiApplication::FlushDiag(CNcbiOstream* os, bool close_diag)
{
    // dyn.cast to CNcbiOstrstream
    CNcbiOstrstream* ostr = dynamic_cast<CNcbiOstrstream*>(m_DiagStream.get());
    if ( !ostr ) {
        _ASSERT( !m_DiagStream.get() );
        return 0;
    }

    // dump all content to "os"
    SIZE_TYPE n_write = 0;
    if ( os )
        n_write = ostr->pcount();

    if ( n_write ) {
        os->write(ostr->str(), n_write);
        ostr->rdbuf()->freeze(0);
    }

    // reset output buffer or destroy
    if ( close_diag ) {
        if ( IsDiagStream(m_DiagStream.get()) ) {
            SetDiagStream(0);
        }
        m_DiagStream.reset(0);
    } else {
        ostr->rdbuf()->SEEKOFF(0, IOS_BASE::beg, IOS_BASE::out);
    }

    // return # of bytes dumped to "os"
    return (os  &&  os->good()) ? n_write : 0;
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
    string exepath = FindProgramExecutablePath(argc, argv);
    
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
            // Log file
            if ( NStr::strcmp(argv[i], s_ArgLogFile) == 0 ) {
                if ( !argv[i++] ) {
                    continue;
                }
                const char* log = argv[i];
                auto_ptr<CNcbiOfstream> os(new CNcbiOfstream(log));
                if ( !os->good() ) {
                    _TRACE("CNcbiApplication() -- cannot open log file: "
                           << log);
                    continue;
                }
                _TRACE("CNcbiApplication() -- opened log file: " << log);
                // (re)direct the global diagnostics to the log.file
                CNcbiOfstream* os_log = os.release();
                SetDiagStream(os_log, true, s_DiagToStdlog_Cleanup,
                              (void*) os_log);
                diag = eDS_ToStdlog;
                is_diag_setup = true;

                // Configuration file
            } else if ( NStr::strcmp(argv[i], s_ArgCfgFile) == 0 ) {
                if ( !argv[i++] ) {
                    continue;
                }
                conf = argv[i];

                // Version
            } else if ( NStr::strcmp(argv[i], s_ArgVersion) == 0 ) {
                if ( !argv[i++] ) {
                    continue;
                }
                // Print USAGE
                LOG_POST(appname + ": " + GetVersion().Print());
                return 0;

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
    m_Arguments->Reset(argc, argv, exepath);

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
        if ( !is_diag_setup  &&  !SetupDiag(diag) ) {
            ERR_POST("Application diagnostic stream's setup failed");
        }
    } catch (CException& e) {
        NCBI_RETHROW(e, CAppException, eSetupDiag,
                     "Application diagnostic stream's setup failed");
    } catch (exception& e) {
        NCBI_THROW(CAppException, eSetupDiag,
                   "Application diagnostic stream's setup failed: " +
                   string(e.what()));
    }

    // Load registry from the config file
    try {
        if ( conf ) {
            string x_conf(conf);
            LoadConfig(*m_Config, &x_conf);
        } else {
            LoadConfig(*m_Config, NULL);
        }
    } catch (CException& e) {
        NCBI_RETHROW(e, CAppException, eLoadConfig,
                     "Registry data cannot be loaded");
    } catch (exception& e) {
        NCBI_THROW(CAppException, eLoadConfig,
                   "Registry data cannot be loaded:  " + string(e.what()));
    }

    // Setup the debugging features from the config file.
    // Don't call till after LoadConfig()
    // NOTE: this will override environment variables, 
    // except DIAG_POST_LEVEL which is Set*Fixed*.
    
    HonorDebugSettings();
    
    // Call:  Init() + Run() + Exit()
    int exit_code = 1;

    try {
        // Init application
        try {
            Init();
            // If the app still has no arg description - provide default one
            if (!m_DisableArgDesc  &&  !m_ArgDesc.get()) {
                auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
                arg_desc->SetUsageContext
                    (GetArguments().GetProgramBasename(),
                     "This program has no mandatory arguments");
                SetupArgDescriptions(arg_desc.release());
            }
        }
        catch (CArgHelpException& ) {
            if ( !m_DisableArgDesc ) {
                if ((m_HideArgs & fHideHelp) != 0)
                    {
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
            }
            // Print USAGE
            string str;
            LOG_POST(m_ArgDesc->PrintUsage(str));
            exit_code = 0;
        }
        catch (CArgException& e) {
            NCBI_RETHROW_SAME(e, "Application's initialization failed");
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("Application's initialization failed", e);
            exit_code = -2;
        }
        catch (exception& e) {
            ERR_POST("Application's initialization failed: " << e.what());
            exit_code = -2;
        }

        // Run application
        if (exit_code == 1) {
            try {
                exit_code = Run();
            }
            catch (CArgException& e) {
                NCBI_RETHROW_SAME(e, "Application's execution failed");
            }
            catch (CException& e) {
                NCBI_REPORT_EXCEPTION("Application's execution failed", e);
                exit_code = -3;
            }
            catch (exception& e) {
                ERR_POST("Application's execution failed: " << e.what());
                exit_code = -3;
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
        }
        catch (exception& e) {
            ERR_POST("Application's cleanup failed: "<< e.what());
        }

    }
    catch (CArgException& e) {
        // Print USAGE and the exception error message
        string str;
        LOG_POST(string(72, '='));
        if ( m_ArgDesc.get() ) {
            LOG_POST(m_ArgDesc->PrintUsage(str) << string(72, '='));
        }
        NCBI_REPORT_EXCEPTION("", e);
        exit_code = -1;
    }
    catch (...) {
        // MSVC++ 6.0 in Debug mode does not call destructors when
        // unwinding the stack unless the exception is caught at least
        // somewhere.
        ERR_POST(Warning <<
                 "Application has thrown an exception of unknown type");
        throw;
    }

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


CVersionInfo CNcbiApplication::GetVersion(void)
{
    return *m_Version;
}


void CNcbiApplication::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    m_ArgDesc.reset(arg_desc);

    if ( arg_desc ) {
        m_Args.reset(arg_desc->CreateArgs(GetArguments()));
    } else {
        m_Args.reset();
    }
}


bool CNcbiApplication::SetupDiag(EAppDiagStream diag)
{
    // Setup diagnostic stream
    switch ( diag ) {
    case eDS_ToStdout: {
        SetDiagStream(&NcbiCout);
        break;
    }
    case eDS_ToStderr: {
        SetDiagStream(&NcbiCerr);
        break;
    }
    case eDS_ToStdlog: {
        // open log.file
        string log = m_Arguments->GetProgramName() + ".log";
        auto_ptr<CNcbiOfstream> os(new CNcbiOfstream(log.c_str()));
        if ( !os->good() ) {
            _TRACE("CNcbiApplication() -- cannot open log file: " << log);
            return false;
        }
        _TRACE("CNcbiApplication() -- opened log file: " << log);

        // (re)direct the global diagnostics to the log.file
        CNcbiOfstream* os_log = os.release();
        SetDiagStream(os_log, true, s_DiagToStdlog_Cleanup, (void*) os_log);
        break;
    }
    case eDS_ToMemory: {
        // direct global diagnostics to the memory-resident output stream
        if ( !m_DiagStream.get() ) {
            m_DiagStream.reset(new CNcbiOstrstream);
        }
        SetDiagStream(m_DiagStream.get());
        break;
    }
    case eDS_Disable: {
        SetDiagStream(0);
        break;
    }
    case eDS_User: {
        // dont change current diag.stream
        break;
    }
    case eDS_AppSpecific: {
        return SetupDiag_AppSpecific();
    }
    case eDS_Default: {
        if ( !IsSetDiagHandler() ) {
            return CNcbiApplication::SetupDiag(eDS_AppSpecific);
        }
        // else eDS_User -- dont change current diag.stream
        break;
    }
    default: {
        _ASSERT(0);
        break;
    }
    } // switch ( diag )

    return true;
}


bool CNcbiApplication::SetupDiag_AppSpecific(void)
{
    return SetupDiag(eDS_ToStderr);
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
        entry = CMetaRegistry::Load(basename, CMetaRegistry::eName_Ini,
                                    CMetaRegistry::fDontOwn, reg_flags, &reg);
        if ( !entry.registry  &&  basename2 != basename ) {
            entry = CMetaRegistry::Load(basename2, CMetaRegistry::eName_Ini,
                                        CMetaRegistry::fDontOwn, reg_flags,
                                        &reg);
        }
    } else {
        entry = CMetaRegistry::Load(*conf, CMetaRegistry::eName_AsIs,
                                    CMetaRegistry::fDontOwn, reg_flags, &reg);
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
        return false;
    } else if (entry.registry != &reg) {
        // should be impossible with new CMetaRegistry interface...
        if (&reg == m_Config  &&  reg.Empty()) {
            if (m_OwnsConfig) {
                delete m_Config;
            }
            m_Config     = entry.registry;
            m_OwnsConfig = false;
        } else {
            // copy into reg
            CNcbiStrstream str;
            entry.registry->Write(str);
            str.seekg(0);
            reg.Read(str);
        }
    }
    return true;
}


bool CNcbiApplication::LoadConfig(CNcbiRegistry& reg,
                                  const string*  conf)
{
    return LoadConfig(reg, conf, 0);
}


void CNcbiApplication::DisableArgDescriptions(void)
{
    m_DisableArgDesc = true;
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
        // when sync_with_stdio is true (default),
        // so we turn off sync_with_stdio here.
        IOS_BASE::sync_with_stdio(false);
    }

    if ((m_StdioFlags & fDefault_CinBufferSize) == 0
#ifdef NCBI_OS_UNIX
        &&  !isatty(0)
#endif
        ) {
#if defined(NCBI_COMPILER_GCC)
#  if NCBI_COMPILER_VERSION >= 300
        _ASSERT(!m_CinBuffer);
        // Ugly work around for G++/Solaris C RTL interaction
        const size_t kCinBufSize = 5120;
        m_CinBuffer = new char[kCinBufSize];
        cin.rdbuf()->pubsetbuf(m_CinBuffer, kCinBufSize);
#  endif
#endif
    }
}


void CNcbiApplication::SetProgramDisplayName(const string& app_name)
{
    m_ProgramDisplayName = app_name;
}


string CNcbiApplication::FindProgramExecutablePath
(int              /*argc*/, 
 const char* const* argv)
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

    string app_path = argv[0];

#  if defined (NCBI_OS_MSWIN)
    // MS Windows: Try more accurate method of detection
    try {
        // Load PSAPI dynamic library -- it should exists on MS-Win NT/2000/XP
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
                if ( ncount ) {
                    buf[ncount] = 0;
                    return buf;
                }
            }
        }
    }
    catch (CException) {
        ; // Just catch an all exceptions from CDll
    }
    // This method don't work -- use standard method
#  endif

#  if defined(NCBI_OS_LINUX) && 0
    // Linux OS: Try more accurate method of detection
    {{
        char   buf[FILENAME_MAX + 1];
        string procfile = "/proc/" + NStr::IntToString(getpid()) + "/exe";
        int    ncount   = readlink((procfile).c_str(), buf, FILENAME_MAX);
        if ( ncount != -1 ) {
            buf[ncount] = 0;
            return buf;
        }
    }}
    // This method don't work -- use standard method
#  endif

    if ( !CDirEntry::IsAbsolutePath(app_path) ) {
#  if defined(NCBI_OS_MSWIN)
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
#  if defined(NCBI_OS_MSWIN)
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

#  error "Platform unrecognized"
#endif
    return ret_val;
}


void CNcbiApplication::HonorDebugSettings(CNcbiRegistry* reg)
{
    if (reg == 0) {
        reg = m_Config;
        if (reg == 0)
            return;
    }
    
    // Setup the debugging features
    if ( !reg->Get("DEBUG", DIAG_TRACE).empty() ) {
        SetDiagTrace(eDT_Enable, eDT_Enable);
    }
    if ( !reg->Get("DEBUG", ABORT_ON_THROW).empty() ) {
        SetThrowTraceAbort(true);
    }
    string post_level = reg->Get("DEBUG", DIAG_POST_LEVEL);
    if ( !post_level.empty() ) {
        EDiagSev sev;
        if (CNcbiDiag::StrToSeverityLevel(post_level.c_str(), sev)) {
            SetDiagFixedPostLevel(sev);
        }
    }
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
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
 * Include <corelib/ncbidll.hpp> only on MSWin. + include <corelib/ncbi_os_mswin.hpp>
 *
 * Revision 1.69  2003/09/17 21:03:58  ivanov
 * FindProgramExecutablePath:  try more accurate method of detection on MS Windows
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
 *
 * Revision 1.63  2003/06/26 18:55:40  rsmith
 * call LoadConfig even if conf is null, child classes might do something
 * anyway. (gbench).
 *
 * Revision 1.62  2003/06/25 15:59:00  rsmith
 * factor out config file DEBUG settings into HonorDebugSettings
 *
 * Revision 1.61  2003/06/23 18:02:31  vakatov
 * CNcbiApplication::MacArgMunging() moved from header to the source file.
 * Heed warnings, formally reformat the code and comments.
 *
 * Revision 1.60  2003/06/18 20:41:23  ucko
 * FindProgramExecutablePath: conditionalize the Darwin-specific code on
 * Metrowerks, since no executable linked against Carbon can run remotely. :-/
 *
 * Revision 1.59  2003/06/16 13:52:27  rsmith
 * Add ProgramDisplayName member. Program name becomes real executable full
 * path. Handle Mac special arg handling better.
 *
 * Revision 1.58  2003/04/07 21:22:54  gouriano
 * correct App exit codes when CException was caught
 *
 * Revision 1.57  2003/03/19 19:37:10  gouriano
 * added optional adjustment of stdio streams
 *
 * Revision 1.56  2003/03/14 21:01:06  kans
 * commented out include Processes because Mac OS 10.2 libraries give an
 * undefined symbol error
 *
 * Revision 1.55  2003/03/14 20:35:24  rsmith
 * Comment out previous changes for Mac OSX since on 10.2 the header files
 * do not compile properly.
 *
 * Revision 1.54  2003/03/13 22:16:11  rsmith
 * on Metrowerks, Mac OSX, always use ncbi.args as the file name for arguments.
 *
 * Revision 1.53  2003/03/13 22:04:27  rsmith
 * Changes to AppMain so MacOSX can find out the appname and arguments.
 *
 * Revision 1.52  2003/02/17 06:30:05  vakatov
 * Get rid of an unused variable
 *
 * Revision 1.51  2002/12/26 17:13:03  ivanov
 * Added version info and Set/GetVersion functions into CNcbiApplication class
 *
 * Revision 1.50  2002/12/18 22:54:48  dicuccio
 * Shuffled some headers to avoid a potentially serious compiler warning
 * (deletion of incomplete type in Windows).
 *
 * Revision 1.49  2002/08/08 18:36:50  gouriano
 * added HideStdArgs function
 *
 * Revision 1.48  2002/08/08 13:39:06  gouriano
 * logfile & conffile-related correction
 *
 * Revision 1.47  2002/08/02 20:13:06  gouriano
 * added possibility to disable arg descriptions
 *
 * Revision 1.46  2002/08/01 19:02:17  ivanov
 * Added autoload of the verbose message file specified in the
 * applications configuration file.
 *
 * Revision 1.45  2002/07/31 18:33:44  gouriano
 * added default argument description,
 * added info about logfile and conffile optional arguments
 *
 * Revision 1.44  2002/07/22 19:33:28  ivanov
 * Fixed bug with internal processing parameters -conffile and -logfile
 *
 * Revision 1.43  2002/07/15 18:17:23  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.42  2002/07/11 14:18:25  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.41  2002/07/10 16:20:13  ivanov
 * Changes related with renaming
 * SetDiagFixedStrPostLevel()->SetDiagFixedPostLevel()
 *
 * Revision 1.40  2002/07/09 16:36:52  ivanov
 * s_SetFixedDiagPostLevel() moved to ncbidiag.cpp
 * and renamed to SetDiagFixedStrPostLevel()
 *
 * Revision 1.39  2002/07/02 18:31:38  ivanov
 * Added assignment the value of diagnostic post level from environment
 * variable and config file if set. Disable to change it from application
 * in this case.
 *
 * Revision 1.38  2002/06/19 16:57:56  ivanov
 * Added new default command line parameters "-logfile <file>" and
 * "-conffile <file>" into CNcbiApplication::AppMain()
 *
 * Revision 1.37  2002/04/11 21:08:00  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.36  2002/01/22 19:28:37  ivanov
 * Changed ConcatPath() -> ConcatPathEx() in LoadConfig()
 *
 * Revision 1.35  2002/01/20 05:53:59  vakatov
 * Get rid of a GCC warning;  formally rearrange some code.
 *
 * Revision 1.34  2002/01/10 16:52:20  ivanov
 * Changed LoadConfig() -- new method to search the config file
 *
 * Revision 1.33  2001/08/20 14:58:10  vakatov
 * Get rid of some compilation warnings
 *
 * Revision 1.32  2001/08/10 18:26:07  vakatov
 * Allow user to throw "CArgException" (and thus force USAGE printout
 * with user-provided explanation) not from any of Init(), Run() or Exit().
 *
 * Revision 1.31  2001/07/07 01:19:28  juran
 * Use "ncbi" for app name on Mac OS (if argv[0] is null).
 *
 * Revision 1.30  2001/03/26 20:07:40  vakatov
 * [NCBI_OS_MAC]  Use argv[0] (if available) as basename for ".args"
 *
 * Revision 1.29  2001/02/02 16:19:27  vasilche
 * Fixed reading program arguments on Mac
 *
 * Revision 1.28  2001/02/01 19:53:26  vasilche
 * Reading program arguments from file moved to CNcbiApplication::AppMain.
 *
 * Revision 1.27  2000/12/23 05:50:53  vakatov
 * AppMain() -- check m_ArgDesc for NULL
 *
 * Revision 1.26  2000/11/29 17:00:25  vakatov
 * Use LOG_POST instead of ERR_POST to print cmd.-line arg usage
 *
 * Revision 1.25  2000/11/24 23:33:12  vakatov
 * CNcbiApplication::  added SetupArgDescriptions() and GetArgs() to
 * setup cmd.-line argument description, and then to retrieve their
 * values, respectively. Also implements internal error handling and
 * printout of USAGE for the described arguments.
 *
 * Revision 1.24  2000/11/01 20:37:15  vasilche
 * Fixed detection of heap objects.
 * Removed ECanDelete enum and related constructors.
 * Disabled sync_with_stdio ad the beginning of AppMain.
 *
 * Revision 1.23  2000/06/09 18:41:04  vakatov
 * FlushDiag() -- check for empty diag.buffer
 *
 * Revision 1.22  2000/04/04 22:33:35  vakatov
 * Auto-set the tracing and the "abort-on-throw" debugging features
 * basing on the application environment and/or registry
 *
 * Revision 1.21  2000/01/20 17:51:18  vakatov
 * Major redesign and expansion of the "CNcbiApplication" class to 
 *  - embed application arguments   "CNcbiArguments"
 *  - embed application environment "CNcbiEnvironment"
 *  - allow auto-setup or "by choice" (see enum EAppDiagStream) of diagnostics
 *  - allow memory-resided "per application" temp. diagnostic buffer
 *  - allow one to specify exact name of the config.-file to load, or to
 *    ignore the config.file (via constructor's "conf" arg)
 *  - added detailed comments
 *
 * Revision 1.20  1999/12/29 21:20:18  vakatov
 * More intelligent lookup for the default config.file. -- try to strip off
 * file extensions if cannot find an exact match;  more comments and tracing
 *
 * Revision 1.19  1999/11/15 15:54:59  sandomir
 * Registry support moved from CCgiApplication to CNcbiApplication
 *
 * Revision 1.18  1999/06/11 20:30:37  vasilche
 * We should catch exception by reference, because catching by value
 * doesn't preserve comment string.
 *
 * Revision 1.17  1999/05/04 00:03:12  vakatov
 * Removed the redundant severity arg from macro ERR_POST()
 *
 * Revision 1.16  1999/04/30 19:21:03  vakatov
 * Added more details and more control on the diagnostics
 * See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
 *
 * Revision 1.15  1999/04/27 14:50:06  vasilche
 * Added FastCGI interface.
 * CNcbiContext renamed to CCgiContext.
 *
 * Revision 1.14  1999/02/22 21:12:38  sandomir
 * MsgRequest -> NcbiContext
 *
 * Revision 1.13  1998/12/28 23:29:06  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.12  1998/12/28 15:43:12  sandomir
 * minor fixed in CgiApp and Resource
 *
 * Revision 1.11  1998/12/14 15:30:07  sandomir
 * minor fixes in CNcbiApplication; command handling fixed
 *
 * Revision 1.10  1998/12/09 22:59:35  lewisg
 * use new cgiapp class
 *
 * Revision 1.7  1998/12/09 16:49:56  sandomir
 * CCgiApplication added
 *
 * Revision 1.4  1998/12/03 21:24:23  sandomir
 * NcbiApplication and CgiApplication updated
 *
 * Revision 1.3  1998/12/01 19:12:08  lewisg
 * added CCgiApplication
 *
 * Revision 1.2  1998/11/05 21:45:14  sandomir
 * std:: deleted
 *
 * Revision 1.1  1998/11/02 22:10:13  sandomir
 * CNcbiApplication added; netest sample updated
 *
 * ===========================================================================
 */
