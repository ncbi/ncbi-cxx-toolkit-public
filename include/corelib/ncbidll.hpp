#ifndef NCBIDLL__HPP
#define NCBIDLL__HPP

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
 *    Portable DLL handling
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2002/01/15 19:06:07  ivanov
 * Initial revision
 *
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>

#if defined NCBI_OS_MSWIN
#   include <windows.h>
#elif defined NCBI_OS_UNIX
#   include <dlfcn.h>
#else
#   pragma message("Class CDll defined only for MS Windows and UNIX platforms.")
#endif


BEGIN_NCBI_SCOPE


// Dll handle type definition
#if defined NCBI_OS_MSWIN
    typedef HMODULE TDllHandle;
#elif defined NCBI_OS_UNIX
    typedef void * TDllHandle;
#endif


//////////////////////////////////////////////////////////////////
//
//  CDll  --  portable DLL handling
//
//////////////////////////////////////////////////////////////////
//
//  Order to search DLL:
//
//  These search rules will only be applied to path names that do not contain an
//  embedded '/' or '\' symbols.
//
//  WINDOWS:
//     If no file name extension is specified for DLL name, the default library extension 
//     ".dll" is appended. 
//     1) The directory from which the application loaded;
//     2) The current directory; 
//     3) The Windows system directory;
//     4) The Windows directory;
//     5) The directories that are listed in the PATH environment variable. 
//  UNIX:
//     1) The directories that are listed in the LD_LIBRARY_PATH environment variable
//        (analyzed once at process startup);
//     2) The directory from which the application loaded.
//

class CDll
{
public:
    // The exception to be thrown by this class
    class CException : public runtime_error {
    public:
        CException(const string& message) : runtime_error(message) {}
    };

    // When to load DLL
    enum ELoad {
        eLoadNow,   // immediately in the constructor
        eLoadLater  // later, using method Load()
    };

    // If to unload DLL in the destructor
    enum EAutoUnload {
        eAutoUnload,   //  do     unload DLL in the destructor
        eNoAutoUnload  //  do not unload DLL in the destructor
    };


    // On error, an exception will be thrown
    CDll(const string& dll_name,
         ELoad         when_to_load = eLoadNow,
         EAutoUnload   auto_unload  = eNoAutoUnload);

    // Unload DLL if constructor was passed "eAutoUnload".
    // Destructor does not throw any exceptions.
    ~CDll(void);

    // Load DLL (name specified in the constructor's "dll_name").
    // If Load() is called more than once without calling Unload() in between,
    // then it will do nothing.
    // On error, an exception will be thrown.
    void Load(void);

    // Unload DLL. Do nothing (and no error) if the DLL is not loaded.
    // On error, an exception will be thrown.
    void Unload(void);

    // Find an entry point (e.g. a function) with name "name" in the DLL.
    // Return the entry point's address on success, NULL on error.
    // If the DLL is not loaded yet, then this method will call Load(),
    // which can result in throwing an exception (if Load() fails).
    template <class TPointer>
    TPointer GetEntryPoint(const string& name, TPointer* entry_ptr) {
        TPointer x_ptr = (TPointer) x_GetEntryPoint(name, sizeof(TPointer));
        if ( entry_ptr ) {
            *entry_ptr = x_ptr; 
        }
        return x_ptr;
    }

private:
    // Find an entry point (e.g. a function) with name "name" in the DLL.
    // Return the entry point's address on success, NULL on error.
    void* x_GetEntryPoint(const string& name, size_t pointer_size);

    // Throw exception if error accrued
    void  x_ThrowException(const string& what);

private:
    string      m_Name;         // DLL name
    TDllHandle  m_Handle;       // DLL handle
    EAutoUnload m_AutoUnload;   // Flag of automaticaly DLL unload in the destructor
};



END_NCBI_SCOPE

#endif  /* NCBIDLL__HPP */
