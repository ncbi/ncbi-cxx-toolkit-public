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
 * Author:  Denis Vakatov, Vladimir Ivanov, Anatoliy Kuznetsov
 *
 *
 */

/// @file ncbidll.hpp
/// Define class Dll and for Portable DLL handling.


#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup Dll
 *
 * @{
 */


// Forward declaration of struct containing OS-specific DLL handle.
struct SDllHandle;


/////////////////////////////////////////////////////////////////////////////
///
/// CDll --
///
/// Define class for portable Dll handling.
///
/// The DLL name is considered the basename if it does not contain embedded
/// '/', '\', or ':' symbols. Also, in this case, if the DLL name does not
/// match pattern "lib*.so", "lib*.so.*", or "*.dll" (and if eExactName flag
/// not passed to the constructor), then it will be automagically transformed
/// according to the following rules:
/// - UNIX:        <name>  --->  lib<name>.so
/// - MS Windows:  <name>  --->  <name>.dll
///
///  If the DLL is specified by its basename, then it will be searched
///  (after the transformation described above) in the following locations:
///
///    UNIX:
///      1) the directories that are listed in the LD_LIBRARY_PATH environment
///         variable (analyzed once at the process startup);
///      2) the directory from which the application loaded;
///      3) hard-coded (e.g. with `ldconfig' on Linux) paths.
///
///    MS Windows:
///      1) the directory from which the application is loaded;
///      2) the current directory; 
///      3) the Windows system directory;
///      4) the Windows directory;
///      5) the directories that are listed in the PATH environment variable.
///
/// NOTE: All methods of this class except the destructor throw exception
/// CCoreException::eDll on error.

class CDll {

public:
    /// When to load DLL.
    enum ELoad {
        eLoadNow,      ///< Load DLL immediately in the constructor
        eLoadLater     ///< Load DLL later, using method Load()
    };

    /// Whether to unload DLL in the destructor.
    enum EAutoUnload {
        eAutoUnload,   ///< Do unload DLL in the destructor
        eNoAutoUnload  ///< Do not unload DLL in the destructor
    };

    /// Whether to transform the DLL basename.
    ///
    /// Transformation is done according to the following:
    ///
    ///    UNIX:        <name>  --->  lib<name>.so
    ///    MS Windows:  <name>  --->  <name>.dll
    enum EBasename {
        eBasename,  ///< Treat as basename (if it looks like one)
        eExactName  ///< Use the name "as is" (no prefix/suffix adding)
    };

    /// Constructor.
    ///
    /// @param name
    ///   Can be either DLL basename or an absolute file path.
    /// @param when_to_load
    ///   Choice to load now or later using Load().
    /// @param auto_unload
    ///   Choice to unload DLL in destructor.
    /// @param treat_as
    ///   Choice to transform the DLL base name.
    /// @sa
    ///   Basename discussion in CDll header,
    ///   Eload, EAutoUnload, EBasename definition.
    NCBI_XNCBI_EXPORT
    CDll(const string& name,
         ELoad         when_to_load = eLoadNow,
         EAutoUnload   auto_unload  = eNoAutoUnload,
         EBasename     treate_as    = eBasename);

    /// Constructor.
    ///
    /// The absolute file path to the DLL will be formed using the "path"
    /// and "name" parameters in the following way:
    /// - UNIX:   <path>/lib<name>.so ; <path>/<name> if "name" is not basename
    /// - MS-Win: <path>\<name>.dll   ; <path>\<name> if "name" is not basename
    ///
    /// @param path
    ///   Path to DLL.
    /// @param name
    ///   Name of DLL.
    /// @param when_to_load
    ///   Choice to load now or later using Load().
    /// @param auto_load
    ///   Choice to unload DLL in destructor.
    /// @param treat_as
    ///   Choice to transform the DLL base name.
    /// @sa
    ///   Basename discussion in CDll header,
    ///   Eload, EAutoUnload, EBasename definition.
    NCBI_XNCBI_EXPORT
    CDll(const string& path, const string& name,
         ELoad         when_to_load = eLoadNow,
         EAutoUnload   auto_unload  = eNoAutoUnload,
         EBasename     treate_as    = eBasename);

    /// Destructor.
    ///
    /// Unload DLL if constructor was passed "eAutoUnload".
    /// Destructor does not throw any exceptions.
    NCBI_XNCBI_EXPORT ~CDll(void);

    /// Load DLL.
    ///
    /// Load the DLL using the name specified in the constructor's DLL "name".
    /// If Load() is called more than once without calling Unload() in between,
    /// then it will do nothing.
    NCBI_XNCBI_EXPORT void Load(void);

    /// Unload DLL.
    ///
    /// Do nothing and do not generate errors if the DLL is not loaded.
    NCBI_XNCBI_EXPORT void Unload(void);

    /// Get DLLs entry point.
    ///
    /// Get the entry point (e.g. a function) with name "name" in the DLL and
    /// return the entry point's address on success, or return NULL on error.
    /// If the DLL is not loaded yet, then this method will call Load(),
    /// which can result in throwing an exception if Load() fails.
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

    /// Get the name of the DLL file 
    const string& GetName() const { return m_Name; }

    /// Get DLLs entry point.
    ///
    /// Same as GetEntryPoint, but returns a raw (void*) pointer on the 
    /// resolved entry function.
    /// @sa GetEntryPoint
    void* GetVoidEntryPoint(const string& name) 
    {
        return x_GetEntryPoint(name, sizeof(void*)); 
    }

private:
    /// Helper find method for getting a DLLs entry point.
    ///
    /// Get the entry point (e.g. a function) with name "name" in the DLL.
    /// @param name
    ///   Name of DLL.
    /// @param pointer_size
    ///   Size of pointer.
    /// @return
    ///   The entry point's address on success, or return NULL on error.
    NCBI_XNCBI_EXPORT
    void* x_GetEntryPoint(const string& name, size_t pointer_size);

    /// Helper method to throw exception with system-specific error message.
    void  x_ThrowException(const string& what);

    /// Helper method to initialize object.
    ///
    /// Called from constructor.
    /// @param path
    ///   Path to DLL.
    /// @param name
    ///   Name of DLL.
    /// @param when_to_load
    ///   Choice to load now or later using Load().
    /// @param auto_load
    ///   Choice to unload DLL in destructor.
    /// @param treat_as
    ///   Choice to transform the DLL base name.
    /// @sa
    ///   Eload, EAutoUnload, EBasename definition.
    void  x_Init(const string& path, const string& name, 
                 ELoad       when_to_load, 
                 EAutoUnload auto_unload, 
                 EBasename   treate_as);

protected:
    /// Private copy constructor to prohibit copy.
    CDll(const CDll&);

    /// Private assignment operator to prohibit assignment.
    CDll& operator= (const CDll&);

private:
    string      m_Name;       ///< DLL name
    SDllHandle* m_Handle;     ///< DLL handle
    bool        m_AutoUnload; ///< Whether to unload DLL in the destructor
};



/////////////////////////////////////////////////////////////////////////////
///
/// Class for entry point resolution when there are several DLL candidates.
///
/// If Dll resolver finds DLL with the specified entry point it is
/// stored in the internal list (provided by GetResolvedEntries method).
/// All DLL libraries are unloaded upon resolver's destruction
///
class NCBI_XNCBI_EXPORT CDllResolver
{
public:
    /// DLL resolution descriptor.
    struct SResolvedEntry
    {
        CDll*    dll;           ///! Loaded DLL instance
        void*    entry_point;   ///! Entry point pointer

        SResolvedEntry(CDll* dll_ptr, void* entry_point_ptr)
        : dll(dll_ptr), 
          entry_point(entry_point_ptr)
        {}
    };

    /// Container, keeps list of all resolved entry points
    typedef vector<SResolvedEntry>  TEntries;


    /// Constructor
    ///
    /// @param entry_point_name
    ///    - name of the DLL entry point
    CDllResolver(const string& entry_point_name);

    /// Constructor
    ///
    /// @param entry_point_names
    ///    - list of alternative DLL entry points
    CDllResolver(const vector<string>& entry_point_names); 

    
    ~CDllResolver();

    /// Try to load DLL from the specified file and resolve the entry point
    ///
    /// If DLL resolution successfull loaded entry point is registered in the
    /// internal list of resolved entries.
    ///
    /// @param file_name
    ///     Name of the DLL file. Can be full name with path of the base name
    /// @return
    ///     TRUE if DLL is succesfully loaded and entry point resolved
    /// @sa GetResolvedEntries
    bool TryCandidate(const string& file_name);

    /// Try to resolve file candidates
    ///
    /// @param candidates
    ///    container with file names to try
    /// @sa GetResolvedEntries
    template<class TClass>
    void Try(const TClass& candidates)
    {
        typename TClass::const_iterator it = candidates.begin();
        typename TClass::const_iterator it_end = candidates.end();
        for (; it != it_end; ++it) {
            TryCandidate(*it);
        }
    }

    /// Try to resolve all files matching the specified masks in the
    /// specified directories.
    ///
    /// @param paths
    ///    container with directory names
    /// @param masks
    ///    container with file candidate masks
    /// @sa GetResolvedEntries
    template<class TClass1, class TClass2>
    void FindCandidates(const TClass1& paths, const TClass2& masks)
    {
        vector<string> candidates;
        FindFiles(candidates, paths.begin(), paths.end(), 
                              masks.begin(), masks.end());
        Try(candidates);
    }

    /// Get all resolved entry points
    const TEntries& GetResolvedEntries() const 
    { 
        return m_ResolvedEntries; 
    }

    /// Get all resolved entry points
    TEntries& GetResolvedEntries() 
    { 
        return m_ResolvedEntries; 
    }

private:
    CDllResolver(const CDllResolver&);
    CDllResolver& operator=(const CDllResolver&);

protected:
    vector<string>  m_EntryPoinNames;   ///! Candidate entry points
    TEntries        m_ResolvedEntries;
};

/* @} */


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.16  2003/11/10 15:28:24  kuznets
 * Fixed misprint
 *
 * Revision 1.15  2003/11/10 15:04:35  kuznets
 * CDllResolver changed to inspect DLL candidate for several alternative
 * entry points
 *
 * Revision 1.14  2003/11/07 17:11:53  kuznets
 * Minor cleanup.
 *
 * Revision 1.13  2003/11/06 13:22:36  kuznets
 * Fixed minor compilation bug
 *
 * Revision 1.12  2003/11/06 12:59:15  kuznets
 * Added new class CDllResolver
 * (searches for DLLs with the specified entry point)
 *
 * Revision 1.11  2003/09/11 16:17:33  ivanov
 * Fixed lines wrapped at 79th column
 *
 * Revision 1.10  2003/07/28 19:07:04  siyan
 * Documentation changes.
 *
 * Revision 1.9  2003/03/31 15:44:33  siyan
 * Added doxygen support
 *
 * Revision 1.8  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.7  2002/10/29 15:59:06  ivanov
 * Prohibited copy constructor and assignment operator
 *
 * Revision 1.6  2002/07/15 18:17:51  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.5  2002/07/11 14:17:54  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
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
