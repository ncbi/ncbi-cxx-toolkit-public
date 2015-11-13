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
#include <corelib/error_codes.hpp>
#include "ncbisys.hpp"


#if defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#elif defined(NCBI_OS_UNIX)
#  ifdef NCBI_OS_DARWIN
#    include <mach-o/dyld.h>
#  endif
#  ifdef HAVE_DLFCN_H
#    include <dlfcn.h>
#    ifndef RTLD_LOCAL /* missing on Cygwin? */
#      define RTLD_LOCAL 0
#    endif
#  endif
#else
#  error "Class CDll defined only for MS Windows and UNIX platforms"
#endif

#if defined(NCBI_OS_MSWIN)
#  pragma warning (disable : 4191)
#endif


#define NCBI_USE_ERRCODE_X   Corelib_Dll


BEGIN_NCBI_SCOPE


// Platform-dependent DLL handle type definition
struct SDllHandle {
#if defined(NCBI_OS_MSWIN)
    HMODULE handle;
#elif defined(NCBI_OS_UNIX)
    void*   handle;
#endif
};

// Check flag bits
#define F_ISSET(mask) ((m_Flags & (mask)) == (mask))
// Clean up an all non-default bits in group if all bits are set
#define F_CLEAN_REDUNDANT(group) \
    if (F_ISSET(group)) m_Flags &= ~unsigned((group) & ~unsigned(fDefault))


CDll::CDll(const string& name, TFlags flags)
{
    x_Init(kEmptyStr, name, flags);
}

CDll::CDll(const string& path, const string& name, TFlags flags)
{
    x_Init(path, name, flags);
}

CDll::CDll(const string& name, ELoad when_to_load, EAutoUnload auto_unload,
           EBasename treate_as)
{
    x_Init(kEmptyStr, name,
           TFlags(when_to_load) | TFlags(auto_unload) | TFlags(treate_as));
}


CDll::CDll(const string& path, const string& name, ELoad when_to_load,
           EAutoUnload auto_unload, EBasename treate_as)
{
    x_Init(path, name,
           TFlags(when_to_load) | TFlags(auto_unload) | TFlags(treate_as));
}


CDll::~CDll()
{
    // Unload DLL automaticaly
    if ( F_ISSET(fAutoUnload) ) {
        try {
            Unload();
        } catch(CException& e) {
            NCBI_REPORT_EXCEPTION_X(1, "CDll destructor", e);
        }
    }
    delete m_Handle;
}


void CDll::x_Init(const string& path, const string& name, TFlags flags)
{
    // Save flags
    m_Flags = flags;

    // Reset redundant flags
    F_CLEAN_REDUNDANT(fLoadNow    | fLoadLater);
    F_CLEAN_REDUNDANT(fAutoUnload | fNoAutoUnload);
    F_CLEAN_REDUNDANT(fBaseName   | fExactName);
    F_CLEAN_REDUNDANT(fGlobal     | fLocal);

    // Init members
    m_Handle = 0;
    string x_name = name;
#if defined(NCBI_OS_MSWIN)
    NStr::ToLower(x_name);
#endif
    // Process DLL name
    if (F_ISSET(fBaseName)  &&
        name.find_first_of(":/\\") == NPOS  &&
        !CDirEntry::MatchesMask(name.c_str(),
                                NCBI_PLUGIN_PREFIX "*" NCBI_PLUGIN_MIN_SUFFIX
                                "*")
        ) {
        // "name" is basename
        x_name = NCBI_PLUGIN_PREFIX + x_name + NCBI_PLUGIN_SUFFIX;
    }
    m_Name = CDirEntry::ConcatPath(path, x_name);
    // Load DLL now if indicated
    if (F_ISSET(fLoadNow)) {
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
    _TRACE("Loading dll: "<<m_Name);
#if defined(NCBI_OS_MSWIN)
    UINT errMode = SetErrorMode(SEM_FAILCRITICALERRORS);
    HMODULE handle = LoadLibrary(_T_XCSTRING(m_Name));
    SetErrorMode(errMode);
#elif defined(NCBI_OS_UNIX)
#  ifdef HAVE_DLFCN_H
    int flags = RTLD_LAZY | (F_ISSET(fLocal) ? RTLD_LOCAL : RTLD_GLOBAL);
    void* handle = dlopen(m_Name.c_str(), flags);
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
    _TRACE("Unloading dll: "<<m_Name);
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
    _TRACE("Getting entry point: "<<name);
    TEntryPoint entry;

    // Return address of entry (function or data)
#if defined(NCBI_OS_MSWIN)
    FARPROC ptr = GetProcAddress(m_Handle->handle, name.c_str());
#elif defined(NCBI_OS_DARWIN)
    NSModule module = (NSModule)m_Handle->handle;
    NSSymbol nssymbol = NSLookupSymbolInModule(module, name.c_str());
    void* ptr = 0;
    ptr = NSAddressOfSymbol(nssymbol);
    if (ptr == NULL) {
        ptr = dlsym (m_Handle->handle, name.c_str());
    }
#elif defined(NCBI_OS_UNIX)  &&  defined(HAVE_DLFCN_H)
    void* ptr = 0;
    ptr = dlsym(m_Handle->handle, name.c_str());
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
    TXChar* ptr = NULL;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (TXChar*) &ptr, 0, NULL);
    string errmsg = ptr ? _T_CSTRING(ptr) : "unknown reason";
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
    : m_AutoUnloadDll(unload)
{
    m_EntryPoinNames.push_back(entry_point_name);
}

CDllResolver::CDllResolver(const vector<string>& entry_point_names,
                           CDll::EAutoUnload unload)
    : m_AutoUnloadDll(unload)
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
        CDll* dll = new CDll(file_name, CDll::fLoadNow | CDll::fNoAutoUnload);
        CDll::TEntryPoint p;

        SResolvedEntry entry_point(dll);

        ITERATE(vector<string>, it, m_EntryPoinNames) {
            string entry_point_name;

            const string& dll_name = dll->GetName();

            if ( !dll_name.empty() ) {
                string base_name;
                CDirEntry::SplitPath(dll_name, 0, &base_name, 0);
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
        if (ex.GetErrCode() != CCoreException::eDll) {
            throw;
        }
        NCBI_REPORT_EXCEPTION_X(2, "CDllResolver::TryCandidate() failed", ex);
        return false;
    }

    return true;
}

static inline
string s_GetProgramPath(void)
{
    string dir;
    CDirEntry::SplitPath
        (CNcbiApplication::GetAppName(CNcbiApplication::eFullName), &dir);
    return dir;
}

void CDllResolver::x_AddExtraDllPath(vector<string>& paths, TExtraDllPath which)
{
    // Nothing to do

    if (which == fNoExtraDllPath) {
        return;
    }

    // Add program executable path

    if ((which & fProgramPath) != 0) {
        string dir = s_GetProgramPath();
        if ( !dir.empty() ) {
            paths.push_back(dir);
        }
    }

    // Add systems directories

    if ((which & fSystemDllPath) != 0) {
#if defined(NCBI_OS_MSWIN)
        // Get Windows system directories
        TXChar buf[MAX_PATH+1];
        UINT len = GetSystemDirectory(buf, MAX_PATH+1);
        if (len>0  &&  len<=MAX_PATH) {
            paths.push_back(_T_STDSTRING(buf));
        }
        len = GetWindowsDirectory(buf, MAX_PATH+1);
        if (len>0  &&  len<=MAX_PATH) {
            paths.push_back(_T_STDSTRING(buf));
        }
        // Parse PATH environment variable
        const TXChar* env = NcbiSys_getenv(_TX("PATH"));
        if (env  &&  *env) {
            NStr::Split(_T_STDSTRING(env), ";", paths, NStr::fSplit_NoMergeDelims);
        }

#elif defined(NCBI_OS_UNIX)
        // From LD_LIBRARY_PATH environment variable
        const char* env = getenv("LD_LIBRARY_PATH");
        if (env  &&  *env) {
            NStr::Split(env, ":", paths, NStr::fSplit_NoMergeDelims);
        }
#endif
    }

    // Add hardcoded runpath

    if ((which & fToolkitDllPath) != 0) {
        const char* runpath = NCBI_GetRunpath();
        if (runpath  &&  *runpath) {
#  if defined(NCBI_OS_MSWIN)
            NStr::Split(runpath, ";", paths, NStr::fSplit_NoMergeDelims);
#  elif defined(NCBI_OS_UNIX)
            vector<string> tokenized;
            NStr::Split(runpath, ":", tokenized, NStr::fSplit_NoMergeDelims);
            ITERATE(vector<string>, i, tokenized) {
                if (i->find("$ORIGIN") == NPOS) {
                    paths.push_back(*i);
                } else {
                    string dir = s_GetProgramPath();
                    if ( !dir.empty() ) {
                        // Need to know the $ORIGIN else discard path.
                        paths.push_back(NStr::Replace(*i, "$ORIGIN", dir));
                    }
                }
            }
#  else
            paths.push_back(runpath);
#  endif
        }
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
