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
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_config_value.hpp>

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
    m_Config.Reset(new CNcbiRegistry);

}


CNcbiApplication::~CNcbiApplication(void)
{
    m_Instance = 0;
    FlushDiag(0, true);
    if (m_CinBuffer) {
        delete [] m_CinBuffer;
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
            // Log file
            if ( NStr::strcmp(argv[i], s_ArgLogFile) == 0 ) {
                if ( !argv[i++] ) {
                    continue;
                }
                if (x_SetupLogFile(argv[i])) {
                    diag = eDS_ToStdlog;
                    is_diag_setup = true;
                }
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

    // Call:  Init() + Run() + Exit()
    int exit_code = 1;

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

            // Setup the standard features from the config file.
            // Don't call till after LoadConfig()
            // NOTE: this will override environment variables, 
            // except DIAG_POST_LEVEL which is Set*Fixed*.
            x_HonorStandardSettings();

            // Do init
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
            }
            // Print USAGE
            string str;
            LOG_POST(m_ArgDesc->PrintUsage(str,
                e.GetErrCode() == CArgHelpException::eHelpFull));
            exit_code = 0;
        }
        catch (CArgException& e) {
            NCBI_RETHROW_SAME(e, "Application's initialization failed");
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("Application's initialization failed", e);
            exit_code = 2;
        }
        catch (exception& e) {
            ERR_POST("Application's initialization failed: " << e.what());
            exit_code = 2;
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
                exit_code = 3;
            }
            catch (exception& e) {
                ERR_POST("Application's execution failed: " << e.what());
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
        }
        catch (exception& e) {
            ERR_POST("Application's cleanup failed: "<< e.what());
        }

    }
    catch (CArgException& e) {
        // Print USAGE and the exception error message
        if ( m_ArgDesc.get() ) {
            string str;
            LOG_POST(m_ArgDesc->PrintUsage(str) << string(72, '='));
        }
        NCBI_REPORT_EXCEPTION("", e);
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
        if (!x_SetupLogFile(log)) {
            return false;
        }
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
#if defined(NCBI_OS_MSWIN)
    if ((m_StdioFlags & fBinaryCin) != 0) {
        setmode( fileno( stdin ), O_BINARY );
    }
    if ((m_StdioFlags & fBinaryCout) != 0) {
        setmode( fileno( stdout ), O_BINARY );
    }
#endif
}


void CNcbiApplication::SetProgramDisplayName(const string& app_name)
{
    m_ProgramDisplayName = app_name;
}


string CNcbiApplication::FindProgramExecutablePath
(int                argc, 
 const char* const* argv,
 string* real_path)
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

#  if defined (NCBI_OS_MSWIN)
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

#  if defined(NCBI_OS_LINUX)
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

    // LOG settings
    if (m_LogFileName.empty() ||
        (!m_LogFileName.empty() && reg->GetBool("LOG","IgnoreEnvArg",false))) {
        string logname = reg->GetString("LOG","File",kEmptyStr);
        if (!logname.empty()) {
            bool truncate_log = reg->GetBool("LOG","Truncate",false);
            bool nocreate_log = reg->GetBool("LOG","NoCreate",false);
            CFile file_log(logname);
            if (!nocreate_log || file_log.Exists()) {
                string prevlog = GetLogFileName();
                ios::openmode mode = ios::out |
                                     (truncate_log ? ios::trunc : ios::app);
                if (x_SetupLogFile(logname,mode)) {
                    if (!prevlog.empty()) {
                        CDirEntry(prevlog).Remove();
                    }
                }
            }
        }
    }
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

bool CNcbiApplication::x_SetupLogFile(const string& name, ios::openmode mode)
{
    auto_ptr<CNcbiOfstream> os(new CNcbiOfstream(name.c_str(), mode));
    if ( os->good() ) {
        _TRACE("CNcbiApplication() -- opened log file: " << name);
        // (re)direct the global diagnostics to the log.file
        CNcbiOfstream* os_log = os.release();
        SetDiagStream(os_log, true, s_DiagToStdlog_Cleanup, (void*) os_log);
        m_LogFileName = name;
    } else {
        _TRACE("CNcbiApplication() -- cannot open log file: " << name);
        return false;
    }
    return true;
}

// GetConfigXxx() functions

namespace {
    bool StringToBool(const string& value)
    {
        try {
            return NStr::StringToBool(value);
        }
        catch ( ... ) {
            return NStr::StringToInt(value) != 0;
        }
    }

    const char* GetEnv(const char* section, const char* variable)
    {
        const char* value;
        if ( section ) {
            string env_var;
            env_var += section;
            env_var += '_';
            env_var += variable;
            value = getenv(env_var.c_str());
        }
        else {
            value = getenv(variable);
        }
        return value;
    }

#ifdef _DEBUG
    static const char* const CONFIG_DUMP_SECTION = "NCBI";
    static const char* const CONFIG_DUMP_VARIABLE = "CONFIG_DUMP_VARIABLES";
    static bool config_dump = GetConfigFlag(CONFIG_DUMP_SECTION,
                                            CONFIG_DUMP_VARIABLE);
#endif
}


bool NCBI_XNCBI_EXPORT GetConfigFlag(const char* section,
                                     const char* variable,
                                     bool default_value)
{
#ifdef _DEBUG
    bool dump = variable != CONFIG_DUMP_VARIABLE && config_dump;
#endif
    if ( section ) {
        CNcbiApplication* app = CNcbiApplication::Instance();
        if ( app ) {
            const string& str = app->GetConfig().Get(section, variable);
            if ( !str.empty() ) {
                try {
                    bool value = StringToBool(str);
#ifdef _DEBUG
                    if ( dump ) {
                        LOG_POST("NCBI_CONFIG: bool variable"
                                 " [" << section << "]"
                                 " " << variable <<
                                 " = " << value <<
                                 " from registry");
                    }
#endif
                    return value;
                }
                catch ( ... ) {
                    // ignored
                }
            }
        }
    }
    const char* str = GetEnv(section, variable);
    if ( str ) {
        try {
            bool value = StringToBool(str);
#ifdef _DEBUG
            if ( dump ) {
                if ( section ) {
                    LOG_POST("NCBI_CONFIG: bool variable"
                             " [" << section << "]"
                             " " << variable <<
                             " = " << value <<
                             " from env var " << section << '_' << variable);
                }
                else {
                    LOG_POST("NCBI_CONFIG: bool variable "
                             " " << variable <<
                             " = " << value <<
                             " from env var");
                }
            }
#endif
            return value;
        }
        catch ( ... ) {
            // ignored
        }
    }
    bool value = default_value;
#ifdef _DEBUG
    if ( dump ) {
        if ( section ) {
            LOG_POST("NCBI_CONFIG: bool variable"
                     " [" << section << "]"
                     " " << variable <<
                     " = " << value <<
                     " by default");
        }
        else {
            LOG_POST("NCBI_CONFIG: bool variable"
                     " " << variable <<
                     " = " << value <<
                     " by default");
        }
    }
#endif
    return value;
}


int NCBI_XNCBI_EXPORT GetConfigInt(const char* section,
                                   const char* variable,
                                   int default_value)
{
    if ( section ) {
        CNcbiApplication* app = CNcbiApplication::Instance();
        if ( app ) {
            const string& str = app->GetConfig().Get(section, variable);
            if ( !str.empty() ) {
                try {
                    int value = NStr::StringToInt(str);
#ifdef _DEBUG
                    if ( config_dump ) {
                        LOG_POST("NCBI_CONFIG: int variable"
                                 " [" << section << "]"
                                 " " << variable <<
                                 " = " << value <<
                                 " from registry");
                    }
#endif
                    return value;
                }
                catch ( ... ) {
                    // ignored
                }
            }
        }
    }
    const char* str = GetEnv(section, variable);
    if ( str ) {
        try {
            int value = NStr::StringToInt(str);
#ifdef _DEBUG
            if ( config_dump ) {
                if ( section ) {
                    LOG_POST("NCBI_CONFIG: int variable"
                             " [" << section << "]"
                             " " << variable <<
                             " = " << value <<
                             " from env var " << section << '_' << variable);
                }
                else {
                    LOG_POST("NCBI_CONFIG: int variable "
                             " " << variable <<
                             " = " << value <<
                             " from env var");
                }
            }
#endif
            return value;
        }
        catch ( ... ) {
            // ignored
        }
    }
    int value = default_value;
#ifdef _DEBUG
    if ( config_dump ) {
        if ( section ) {
            LOG_POST("NCBI_CONFIG: int variable"
                     " [" << section << "]"
                     " " << variable <<
                     " = " << value <<
                     " by default");
        }
        else {
            LOG_POST("NCBI_CONFIG: int variable"
                     " " << variable <<
                     " = " << value <<
                     " by default");
        }
    }
#endif
    return value;
}


string NCBI_XNCBI_EXPORT GetConfigString(const char* section,
                                         const char* variable,
                                         const char* default_value)
{
    if ( section ) {
        CNcbiApplication* app = CNcbiApplication::Instance();
        if ( app ) {
            const string& value = app->GetConfig().Get(section, variable);
            if ( !value.empty() ) {
#ifdef _DEBUG
                if ( config_dump ) {
                    LOG_POST("NCBI_CONFIG: str variable"
                             " [" << section << "]"
                             " " << variable <<
                             " = \"" << value << "\""
                             " from registry");
                }
#endif
                return value;
            }
        }
    }
    const char* value = GetEnv(section, variable);
    if ( value ) {
#ifdef _DEBUG
        if ( config_dump ) {
            if ( section ) {
                LOG_POST("NCBI_CONFIG: str variable"
                         " [" << section << "]"
                         " " << variable <<
                         " = \"" << value << "\""
                         " from env var " << section << '_' << variable);
            }
            else {
                LOG_POST("NCBI_CONFIG: str variable"
                         " " << variable <<
                         " = \"" << value << "\""
                         " from env var");
            }
        }
#endif
        return value;
    }
    value = default_value? default_value: "";
#ifdef _DEBUG
    if ( config_dump ) {
        if ( section ) {
            LOG_POST("NCBI_CONFIG: str variable"
                     " [" << section << "]"
                     " " << variable <<
                     " = \"" << value << "\""
                     " by default");
        }
        else {
            LOG_POST("NCBI_CONFIG: str variable"
                     " " << variable <<
                     " = \"" << value << "\""
                     " by default");
        }
    }
#endif
    return value;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
