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

#include <corelib/ncbidll.hpp>
#include <corelib/ncbifile.hpp>


#if defined(NCBI_OS_MSWIN)
#  include <windows.h>
#elif defined(NCBI_OS_UNIX)
#  include <dlfcn.h>
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
        } catch(CException& _DEBUG_ARG(e)) {
            _TRACE(string("CDll::~CDll()  ") + e.what());
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
#if defined(NCBI_OS_MSWIN)
        !CDirEntry::MatchesMask(name.c_str(),"*.dll")
#elif defined(NCBI_OS_UNIX)
        !CDirEntry::MatchesMask(name.c_str(),"lib*.so") &&
        !CDirEntry::MatchesMask(name.c_str(),"lib*.so.*")
#endif
        ) {
        // "name" is basename
#if defined(NCBI_OS_MSWIN)
        x_name = x_name + ".dll";
#elif defined(NCBI_OS_UNIX)
        x_name = "lib" + x_name + ".so";
#endif
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
    void* handle = dlopen(m_Name.c_str(), RTLD_LAZY | RTLD_GLOBAL);
#endif
    if ( !handle ) {
        x_ThrowException("Load error");
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
    bool unloaded = dlclose(m_Handle->handle) == 0;
#endif
    if ( !unloaded ) {
        x_ThrowException("Unload error");
    }

    delete m_Handle;
    m_Handle = 0;
}


void* CDll::x_GetEntryPoint(const string& name, size_t pointer_size)
{
    // Check pointer size
    if (pointer_size != sizeof(void*)) {
        throw CException("CDll: GetEntryPoint():  incorrect entry ptr size");
    }

    // If DLL is not yet loaded
    if ( !m_Handle ) {
        Load();
    }

    // Return address of function
#if defined(NCBI_OS_MSWIN)
    return GetProcAddress(m_Handle->handle, name.c_str());
#elif defined(NCBI_OS_UNIX)
    return dlsym(m_Handle->handle, name.c_str());
#endif
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
    const char* errmsg = dlerror();
    if ( !errmsg ) {
        errmsg = "unknown reason";
    }
#endif

    // Throw exception
    throw CException("CDll: " + what + ":  " + errmsg);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
