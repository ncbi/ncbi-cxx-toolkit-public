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
 * Author: Vladimir Ivanov, Denis Vakatov
 *
 * File Description:
 *   Portable DLL handling
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidll.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbiapp.hpp>


#if defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#elif defined(NCBI_OS_UNIX)
#  ifdef HAVE_DLFCN_H
#    include <dlfcn.h>
#  endif
#else
#  error "Class CDll defined only for MS Windows and UNIX platforms"
#endif


BEGIN_NCBI_SCOPE


// Platform-dependent DLL handle type definition
struct SDllHandle {
#if defined(NCBI_OS_MSWIN)
    HMODULE handle;
#elif defined(NCBI_OS_UNIX)
    void*   handle;
#endif
};


CDll::CDll(const string& name, ELoad when_to_load, EAutoUnload auto_unload,
           EBasename treate_as)
{
    x_Init(kEmptyStr, name, when_to_load, auto_unload, treate_as);
}


CDll::CDll(const string& path, const string& name, ELoad when_to_load,
           EAutoUnload auto_unload, EBasename treate_as)
{
    x_Init(path, name, when_to_load, auto_unload, treate_as);
}


CDll::~CDll() 
{
    // Unload DLL automaticaly
    if ( m_AutoUnload ) {
        try {
            Unload();
        } catch(CException& e) {
            NCBI_REPORT_EXCEPTION("CDll destructor", e);
        }
    }
    delete m_Handle;
}


void CDll::x_Init(const string& path, const string& name, ELoad when_to_load, 
           EAutoUnload  auto_unload, EBasename treate_as)
{
    // Init members
    m_Handle = 0;
    m_AutoUnload = auto_unload == eAutoUnload;

    string x_name = name;
#if defined(NCBI_OS_MSWIN)
    NStr::ToLower(x_name);
#endif
    // Process DLL name
    if (treate_as == eBasename  &&  
        name.find_first_of(":/\\") == NPOS &&
        !CDirEntry::MatchesMask(name.c_str(),
                                NCBI_PLUGIN_PREFIX "*" NCBI_PLUGIN_SUFFIX "*")
        ) {
        // "name" is basename
        x_name = NCBI_PLUGIN_PREFIX + x_name + NCBI_PLUGIN_SUFFIX;
    }
    m_Name = CDirEntry::ConcatPath(path, x_name);  
    // Load DLL now if indicated
    if (when_to_load == eLoadNow) {
        Load();
    }
}


void CDll::Load(void)
{
    // DLL is already loaded
    if ( m_Handle ) {
        return;
    }
    // Load DLL
#if defined(NCBI_OS_MSWIN)
    HMODULE handle = LoadLibrary(m_Name.c_str());
#elif defined(NCBI_OS_UNIX)
#  ifdef HAVE_DLFCN_H
    void* handle = dlopen(m_Name.c_str(), RTLD_LAZY | RTLD_GLOBAL);
#  else
    void* handle = 0;
#  endif
#endif
    if ( !handle ) {
        x_ThrowException("CDll::Load");
    }
    m_Handle = new SDllHandle;
    m_Handle->handle = handle;
}


void CDll::Unload(void)
{
    // DLL is not loaded
    if ( !m_Handle ) {
        return;
    }
    // Unload DLL
#if defined(NCBI_OS_MSWIN)
    BOOL unloaded = FreeLibrary(m_Handle->handle);
#elif defined(NCBI_OS_UNIX)
#  ifdef HAVE_DLFCN_H
    bool unloaded = dlclose(m_Handle->handle) == 0;
#  else
    bool unloaded = false;
#  endif
#endif
    if ( !unloaded ) {
        x_ThrowException("CDll::Unload");
    }

    delete m_Handle;
    m_Handle = 0;
}


CDll::TEntryPoint CDll::GetEntryPoint(const string& name)
{
    // If DLL is not yet loaded
    if ( !m_Handle ) {
        Load();
    }
    // Add leading underscore on Darwin platform
#if defined(NCBI_OS_DARWIN)
    const string entry_name = "_" + name;
#else
    const string entry_name = name;
#endif
    TEntryPoint entry;
    // Return address of entry (function or data)
#if defined(NCBI_OS_MSWIN)
    FARPROC ptr = GetProcAddress(m_Handle->handle, entry_name.c_str());
#elif defined(NCBI_OS_UNIX)
    void* ptr = 0;
#  if defined(HAVE_DLFCN_H)
    ptr = dlsym(m_Handle->handle, entry_name.c_str());
#    if defined(NCBI_OS_DARWIN)
    // Try again without the underscore, since 10.3 and up don't need it.
    if ( !ptr ) {
        ptr = dlsym(m_Handle->handle, entry_name.c_str() + 1);
    }
#    endif
#  endif
#else
    void* ptr = 0;
#endif
    entry.func = (FEntryPoint)ptr;
    entry.data = ptr;
    return entry;
}


void CDll::x_ThrowException(const string& what)
{
#if defined(NCBI_OS_MSWIN)
    char* ptr = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                  FORMAT_MESSAGE_FROM_SYSTEM | 
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, GetLastError(), 
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR) &ptr, 0, NULL);
    string errmsg = ptr ? ptr : "unknown reason";
    LocalFree(ptr);
#elif defined(NCBI_OS_UNIX)
#  ifdef HAVE_DLFCN_H
    const char* errmsg = dlerror();
    if ( !errmsg ) {
        errmsg = "unknown reason";
    }
#  else
    const char* errmsg = "No DLL support on this platform.";
#  endif
#endif

    NCBI_THROW(CCoreException, eDll, what + " [" + m_Name +"]: " + errmsg);
}


CDllResolver::CDllResolver(const string& entry_point_name, 
                           CDll::EAutoUnload unload)
    : m_AutoUnloadDll( unload )
{    
    m_EntryPoinNames.push_back(entry_point_name);
}

CDllResolver::CDllResolver(const vector<string>& entry_point_names, 
                           CDll::EAutoUnload unload)
    : m_AutoUnloadDll( unload )
{
    m_EntryPoinNames = entry_point_names;
}

CDllResolver::~CDllResolver()
{
    Unload();
}

bool CDllResolver::TryCandidate(const string& file_name,
                                const string& driver_name)
{
    try {
        CDll* dll = new CDll(file_name, CDll::eLoadNow, CDll::eNoAutoUnload);
        CDll::TEntryPoint p;

        SResolvedEntry entry_point(dll);

        ITERATE(vector<string>, it, m_EntryPoinNames) {
            string entry_point_name;
            
            const string& dll_name = dll->GetName();
            
            if ( !dll_name.empty() ) {
                string base_name;
                CDirEntry::SplitPath(entry_point_name, 0, &base_name, 0);
                NStr::Replace(*it, 
                              "${basename}", base_name, entry_point_name);

                if (!driver_name.empty()) {
                    NStr::Replace(*it, 
                            "${driver}", driver_name, entry_point_name);
                }
            }
            
            // Check for the BASE library name macro
            
            if ( entry_point_name.empty() )
                continue;
            p = dll->GetEntryPoint(entry_point_name);
            if ( p.data ) { 
                entry_point.entry_points.push_back(SNamedEntryPoint(entry_point_name, p));
            }
        } // ITERATE

        if ( entry_point.entry_points.empty() ) {
            dll->Unload();
            delete dll;
            return false;
        }

        m_ResolvedEntries.push_back(entry_point);
    } 
    catch (CCoreException& ex)
    {
        if (ex.GetErrCode() != CCoreException::eDll)
            throw;
        return false;
    }

    return true;
}

void CDllResolver::AddExtraDllPath(vector<string>& paths, TExtraDllPath which)
{
    // Nothing to do

    if (which == fNoExtraDllPath) {
        return;
    }

    // Add program executable path

    if ((which & fProgramPath) != 0) {
        CNcbiApplication* app = CNcbiApplication::Instance();
        if ( app ) {
            string exe = app->GetProgramExecutablePath();
            if ( !exe.empty() ) {
                string dir;
                CDirEntry::SplitPath(exe, &dir);
                if ( !dir.empty() ) {
                    paths.push_back(dir);
                }
            }
        }
    }

    // Add hardcoded runpath

#if !defined(NCBI_COMPILER_METROWERKS)
    if ((which & fToolkitDllPath) != 0) {
        const char* runpath = NCBI_GetRunpath();
        if (runpath  &&  *runpath) {
#  if defined(NCBI_OS_MSWIN)
            NStr::Tokenize(runpath, ";", paths);
#  elif defined(NCBI_OS_UNIX)
            NStr::Tokenize(runpath, ":", paths);
#  else
            paths.push_back(runpath);
#  endif
        }
    }
#endif

    // Add systems directories

    if ((which & fSystemDllPath) != 0) {
#if defined(NCBI_OS_MSWIN)
        // Get Windows system directories
        char buf[MAX_PATH+1];
        UINT len = GetSystemDirectory(buf, MAX_PATH+1);
        if (len>0  &&  len<=MAX_PATH) {
            paths.push_back(buf);
        }
        len = GetWindowsDirectory(buf, MAX_PATH+1);
        if (len>0  &&  len<=MAX_PATH) {
            paths.push_back(buf);
        }
        // Parse PATH environment variable 
        const char* env = getenv("PATH");
        if (env  &&  *env) {
            NStr::Tokenize(env, ";", paths);
        }

#elif defined(NCBI_OS_UNIX)
        // From LD_LIBRARY_PATH environment variable 
        const char* env = getenv("LD_LIBRARY_PATH");            
        if (env  &&  *env) {
            NStr::Tokenize(env, ":", paths);
        }
#endif
    }
    return;
}

void CDllResolver::Unload()
{
    NON_CONST_ITERATE(TEntries, it, m_ResolvedEntries) {
        if ( m_AutoUnloadDll == CDll::eAutoUnload ) {
            it->dll->Unload();
        }
        delete it->dll;
    }
    m_ResolvedEntries.resize(0);    
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.27  2005/03/03 19:03:43  ssikorsk
 * Pass an 'auto_unload' parameter into CDll and CDllResolver constructors
 *
 * Revision 1.26  2005/02/18 14:29:19  ivanov
 * CDllResolver::AddExtraDllPath() -- added support of multiply pathes in
 * the NCBI runpath.
 *
 * Revision 1.25  2004/10/12 19:59:23  ivanov
 * Do not call NCBI_GetRunpath() when compiling with Codewarrior only --
 * MS Windows have NCBI_GetRunpath().
 *
 * Revision 1.24  2004/10/12 19:46:08  rsmith
 * Do not call NCBI_GetRunpath when compiling with Codewarrior.
 *
 * Revision 1.23  2004/08/09 15:36:56  kuznets
 * CDllResolver added support of driver name
 *
 * Revision 1.22  2004/08/06 11:26:07  ivanov
 * Extend CDllResolver to make it also look in "standard" places.
 * Added TExtraDllPath enum and AddExtraDllPath() method.
 * Added accessory parameter to FindCandidates().
 *
 * Revision 1.21  2004/06/23 17:13:56  ucko
 * Centralize plugin naming in ncbidll.hpp.
 *
 * Revision 1.20  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.19  2004/02/05 18:41:25  ucko
 * GetEntryPoint: Darwin may or may not need a leading underscore, so try
 * it both ways.
 *
 * Revision 1.18  2003/12/09 13:06:44  kuznets
 * Supported dll base name in entry point resolution (CDllResolver)
 *
 * Revision 1.17  2003/12/01 16:39:15  kuznets
 * CDllResolver changed to try all entry points
 * (prev. version stoped on first successfull).
 *
 * Revision 1.16  2003/11/19 13:50:42  ivanov
 * GetEntryPoint() revamp: added GetEntryPoin_[Func|Data]()
 *
 * Revision 1.15  2003/11/12 17:40:54  kuznets
 * + CDllResolver::Unload()
 *
 * Revision 1.14  2003/11/10 15:28:46  kuznets
 * Fixed misprint
 *
 * Revision 1.13  2003/11/10 15:05:04  kuznets
 * Reflecting changes in hpp file
 *
 * Revision 1.12  2003/11/06 13:00:12  kuznets
 * +CDllResolver
 *
 * Revision 1.11  2002/12/19 20:27:09  ivanov
 * Added DLL name to an exception description in the x_ThrowException()
 *
 * Revision 1.10  2002/09/19 20:05:42  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.9  2002/07/15 18:17:24  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.8  2002/07/11 14:18:26  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.7  2002/07/01 16:44:14  ivanov
 * Added Darwin specific: use leading underscores in entry names
 *
 * Revision 1.6  2002/05/28 20:01:20  vakatov
 * Typo fixed
 *
 * Revision 1.5  2002/05/23 22:24:07  ucko
 * Handle the absence of <dlfcn.h> better.
 *
 * Revision 1.4  2002/04/11 21:08:02  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.3  2002/03/25 18:10:00  ucko
 * Make errmsg const to accommodate platforms (FreeBSD at least) where
 * dlerror returns const char*.
 *
 * Revision 1.2  2002/01/16 18:48:57  ivanov
 * Added new constructor and related "basename" rules for DLL names. 
 * Polished source code.
 *
 * Revision 1.1  2002/01/15 19:05:28  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
