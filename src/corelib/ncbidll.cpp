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
 * Author: Vladimir Ivanov
 *
 * File Description:
 *   Portable DLL handling
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2002/01/15 19:05:28  ivanov
 * Initial revision
 *
 *
 * ===========================================================================
 */

#include <corelib/ncbidll.hpp>


BEGIN_NCBI_SCOPE


CDll::CDll(const string& dll_name, ELoad when_to_load, EAutoUnload auto_unload)
{
    // Members initialization
    m_Name = dll_name;
    m_Handle = NULL;
    m_AutoUnload = auto_unload;

    // Load DLL now if indicated
    if ( when_to_load == eLoadNow ) {
        Load();
    }
}


CDll::~CDll() 
{
    // Unload DLL automaticaly
    if ( m_AutoUnload == eAutoUnload ) {
        try {
            Unload();
        } catch( CException& ) {
            // not process
        }
    }
}


void CDll::Load(void)
{
    // DLL is already loaded
    if ( m_Handle ) return;
    // Load DLL
#if defined NCBI_OS_MSWIN
    if ( (m_Handle = LoadLibrary(m_Name.c_str())) != NULL ) {
        return;
    }
#elif defined NCBI_OS_UNIX
    if ( (m_Handle = dlopen(m_Name.c_str(), RTLD_LAZY | RTLD_GLOBAL)) != NULL ) {
        return;
    }
#endif
    x_ThrowException("Load error");
}


void CDll::Unload(void)
{
    // DLL is not loaded
    if ( !m_Handle ) return;
    // Unload DLL
#if defined NCBI_OS_MSWIN
    if ( FreeLibrary(m_Handle) ) {
        return;
    }
#elif defined NCBI_OS_UNIX
    if ( dlclose(m_Handle) == 0 ) {
        return;
    }
#endif
    x_ThrowException("Unload error");
}


void* CDll::x_GetEntryPoint(const string& name, size_t pointer_size)
{
    // Check pointer size
    if ( pointer_size != sizeof(void*)) {
        throw CException("CDll: GetEntryPoint(): incorrect entry pointer size");
    }
    // If DLL is not yet loaded
    if ( !m_Handle ) {
        Load();
    }
    // Return address of function
#if defined NCBI_OS_MSWIN
    return GetProcAddress(m_Handle, name.c_str());
#elif defined NCBI_OS_UNIX
    return dlsym(m_Handle, name.c_str());
#endif
}


void CDll::x_ThrowException(const string& what)
{
#if defined NCBI_OS_MSWIN
    char* ptr = NULL;
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                   FORMAT_MESSAGE_FROM_SYSTEM | 
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, GetLastError(), 
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPTSTR) &ptr, 0, NULL);
    string errmsg = ptr ? ptr : "unknown reason";
    LocalFree(ptr);

#elif defined NCBI_OS_UNIX
    char* errmsg = dlerror();
    if ( !errmsg ) {
        errmsg = "unknown reason";
    }
#endif
    // Throw exception
    throw CException("CDll: " + what + ": "+ errmsg);
}


END_NCBI_SCOPE
