#ifndef CORELIB___NCBIDLL__HPP
#define CORELIB___NCBIDLL__HPP

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
 * Author:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:   Portable DLL handling.
 *
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////
//
//  CDll  --  portable DLL handling
//
//////////////////////////////////////////////////////////////////
//
//
//  The DLL name is considered basename if it does not contain
//  embedded '/', '\', or ':' symbols.
//  Also, in this case, if the DLL name does not match pattern
//  "lib*.so", "lib*.so.*", or "*.dll" (and if eExactName flag not passed
//  to the constructor), then it will be automagically transformed:
//    UNIX:        <name>  --->  lib<name>.so
//    MS Windows:  <name>  --->  <name>.dll
//
//  If the DLL is specified by its basename, then it will be searched
//  (after the transformation described above) in the following locations:
//    UNIX:
//      1) the directories that are listed in the LD_LIBRARY_PATH environment
//         variable (analyzed once at the process startup);
//      2) the directory from which the application loaded;
//      3) hard-coded (e.g. with `ldconfig' on Linux) paths.
//    MS Windows:
//      1) the directory from which the application is loaded;
//      2) the current directory; 
//      3) the Windows system directory;
//      4) the Windows directory;
//      5) the directories that are listed in the PATH environment variable.
//

// fwd-decl of struct containing OS-specific DLL handle
struct SDllHandle;


class CDll
{
public:
    // All methods of this class (but destructor) throw exception on error!
    // The exception specific for this class:
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
        eAutoUnload,   // do     unload DLL in the destructor
        eNoAutoUnload  // do not unload DLL in the destructor
    };
    // If to transform the DLL basename (like:  <name>  --->  lib<name>.so)
    enum EBasename {
        eBasename,  // treate as basename (if it looks like one)
        eExactName  // use the name "as is" (no prefix/suffix adding)
    };

    // The "name" can be either DLL basename (see explanations above) or
    // an absolute file path.
    CDll(const string& name,
         ELoad         when_to_load = eLoadNow,
         EAutoUnload   auto_unload  = eNoAutoUnload,
         EBasename     treate_as    = eBasename);

    // The absolute file path to the DLL will be formed of the
    // "path" and "name" in the following way:
    //  UNIX:    <path>/lib<name>.so ;  <path>/<name> if "name" is not basename
    //  MS-Win:  <path>\<name>.dll   ;  <path>\<name> if "name" is not basename
    CDll(const string& path, const string& name,
         ELoad         when_to_load = eLoadNow,
         EAutoUnload   auto_unload  = eNoAutoUnload,
         EBasename     treate_as    = eBasename);

    // Unload DLL if constructor was passed "eAutoUnload".
    // Destructor does not throw any exceptions.
    ~CDll(void);

    // Load DLL (name specified in the constructor's "dll_name").
    // If Load() is called more than once without calling Unload() in between,
    // then it will do nothing.
    void Load(void);

    // Unload DLL. Do nothing (and no error) if the DLL is not loaded.
    void Unload(void);

    // Find an entry point (e.g. a function) with name "name" in the DLL.
    // Return the entry point's address on success, NULL on error.
    // If the DLL is not loaded yet, then this method will call Load(),
    // which can result in throwing an exception (if Load() fails).
    template <class TPointer>
    TPointer GetEntryPoint(const string& name, TPointer* entry_ptr) {
        union {
            TPointer type_ptr;
            void*    void_ptr;
        } ptr;
        ptr.void_ptr = x_GetEntryPoint(name, sizeof(TPointer));
        if ( entry_ptr ) {
            *entry_ptr = ptr.type_ptr; 
        }
        return ptr.type_ptr;
    }

private:
    // Find an entry point (e.g. a function) with name "name" in the DLL.
    // Return the entry point's address on success, NULL on error.
    void* x_GetEntryPoint(const string& name, size_t pointer_size);

    // Throw exception ('CException' with system-specific error message)
    void  x_ThrowException(const string& what);

    // Initialize object (called from constructors)
    void  x_Init(const string& path, const string& name, 
                 ELoad       when_to_load, 
                 EAutoUnload auto_unload, 
                 EBasename   treate_as);

private:
    string      m_Name;        // DLL name
    SDllHandle* m_Handle;      // DLL handle
    bool        m_AutoUnload;  // if to unload the DLL in the destructor
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2002/04/11 20:39:17  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.3  2002/01/20 07:18:10  vakatov
 * CDll::GetEntryPoint() -- fool-proof cast of void ptr to func ptr
 *
 * Revision 1.2  2002/01/16 18:48:13  ivanov
 * Added new constructor and related "basename" rules for DLL names. 
 * Polished source code.
 *
 * Revision 1.1  2002/01/15 19:06:07  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CORELIB___NCBIDLL__HPP */
