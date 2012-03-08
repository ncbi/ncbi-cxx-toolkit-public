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
#include <corelib/ncbicfg.h>
#include <corelib/ncbifile.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup Dll
 *
 * @{
 */


// Forward declaration of struct containing OS-specific DLL handle.
struct SDllHandle;


#ifndef NCBI_PLUGIN_SUFFIX
#  ifdef NCBI_OS_MSWIN
#    define NCBI_PLUGIN_PREFIX ""
#    define NCBI_PLUGIN_MIN_SUFFIX ".dll"
#  elif defined(NCBI_OS_DARWIN)  &&  !defined(NCBI_USE_BUNDLES)
#    define NCBI_PLUGIN_PREFIX "lib"
#    define NCBI_PLUGIN_MIN_SUFFIX ".dylib"
#  else
#    define NCBI_PLUGIN_PREFIX "lib"
#    define NCBI_PLUGIN_MIN_SUFFIX ".so"
#  endif
#  if defined(NCBI_DLL_BUILD)  ||  defined(NCBI_OS_MSWIN)
#    define NCBI_PLUGIN_SUFFIX NCBI_PLUGIN_MIN_SUFFIX
#  else
#    define NCBI_PLUGIN_SUFFIX "-dll" NCBI_PLUGIN_MIN_SUFFIX
#  endif
#endif


/////////////////////////////////////////////////////////////////////////////
///
/// CDll --
///
/// Define class for portable Dll handling.
///
/// The DLL name is considered the basename if it does not contain embedded
/// '/', '\', or ':' symbols. Also, in this case, if the DLL name does not
/// start with NCBI_PLUGIN_PREFIX and contain NCBI_PLUGIN_MIN_SUFFIX (and if
/// eExactName flag not passed to the constructor), then it will be
/// automagically transformed according to the following rule:
///   <name>  --->  NCBI_PLUGIN_PREFIX + <name> + NCBI_PLUGIN_SUFFIX
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

class CDll
{
public:
    /// General flags.
    ///
    /// Default flag in each group have priority above non-default,
    /// if they are used together.
    enum EFlags {
        /// When to load DLL
        fLoadNow      = (1<<1),  ///< Load DLL immediately in the constructor
        fLoadLater    = (1<<2),  ///< Load DLL later, using method Load()
        /// Whether to unload DLL in the destructor
        fAutoUnload   = (1<<3),  ///< Unload DLL in the destructor
        fNoAutoUnload = (1<<4),  ///< Unload DLL later, using method Unload()
        /// Whether to transform the DLL basename
        fBaseName     = (1<<5),  ///< Treat the name as DLL basename
        fExactName    = (1<<6),  ///< Use the name "as is"
        /// Specify how to load symbols from DLL.
        /// UNIX specific (see 'man dlopen'), ignored on all other platforms.
        fGlobal       = (1<<7),  ///< Load as RTLD_GLOBAL
        fLocal        = (1<<8),  ///< Load as RTLD_LOCAL
        /// Default flags
        fDefault      = fLoadNow | fNoAutoUnload | fBaseName | fGlobal
    };
    typedef unsigned int TFlags;  ///< Binary OR of "EFlags"

    //
    // Enums, retained for backward compatibility
    //

    /// When to load DLL.
    enum ELoad {
        eLoadNow      = fLoadNow,
        eLoadLater    = fLoadLater
    };

    /// Whether to unload DLL in the destructor.
    enum EAutoUnload {
        eAutoUnload   = fAutoUnload,
        eNoAutoUnload = fNoAutoUnload
    };

    /// Whether to transform the DLL basename.
    ///
    /// Transformation is done according to the following:
    ///   <name>  --->  NCBI_PLUGIN_PREFIX + <name> + NCBI_PLUGIN_SUFFIX
    enum EBasename {
        eBasename     = fBaseName,
        eExactName    = fExactName
    };

    /// Constructor.
    ///
    /// @param name
    ///   Can be either DLL basename or an absolute file path.
    /// @param flags
    ///   Define how to load/unload DLL and interprete passed name.
    /// @sa
    ///   Basename discussion in CDll header, EFlags
    NCBI_XNCBI_EXPORT
    CDll(const string& name, TFlags flags);

    /// Constructor (for backward compatibility).
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
    ///   ELoad, EAutoUnload, EBasename definition.
    NCBI_XNCBI_EXPORT
    CDll(const string& name,
         ELoad         when_to_load = eLoadNow,
         EAutoUnload   auto_unload  = eNoAutoUnload,
         EBasename     treate_as    = eBasename);

    /// Constructor.
    ///
    /// The absolute file path to the DLL will be formed using the "path"
    /// and "name" parameters in the following way:
    /// - UNIX:   <path>/PFX<name>SFX ; <path>/<name> if "name" is not basename
    /// - MS-Win: <path>\PFX<name>SFX ; <path>\<name> if "name" is not basename
    /// where PFX is NCBI_PLUGIN_PREFIX and SFX is NCBI_PLUGIN_SUFFIX.
    ///
    /// @param path
    ///   Path to DLL.
    /// @param name
    ///   Name of DLL.
    /// @param flags
    ///   Define how to load/unload DLL and interprete passed name.
    /// @sa
    ///   Basename discussion in CDll header, EFlags
    NCBI_XNCBI_EXPORT
    CDll(const string& path, const string& name, TFlags flags);

    /// Constructor (for backward compatibility).
    ///
    /// The absolute file path to the DLL will be formed using the "path"
    /// and "name" parameters in the following way:
    /// - UNIX:   <path>/PFX<name>SFX ; <path>/<name> if "name" is not basename
    /// - MS-Win: <path>\PFX<name>SFX ; <path>\<name> if "name" is not basename
    /// where PFX is NCBI_PLUGIN_PREFIX and SFX is NCBI_PLUGIN_SUFFIX.
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
    ///   ELoad, EAutoUnload, EBasename definition.
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

    /// Get DLLs entry point (function).
    ///
    /// Get the entry point (a function) with name "name" in the DLL and
    /// return the entry point's address on success, or return NULL on error.
    /// If the DLL is not loaded yet, then this method will call Load(),
    /// which can result in throwing an exception if Load() fails.
    /// @sa
    ///   GetEntryPoint_Data
    template <class TFunc>
    TFunc GetEntryPoint_Func(const string& name, TFunc* func)
    {
        TEntryPoint ptr = GetEntryPoint(name);
        if ( func ) {
            *func = (TFunc)(void*)ptr.func; 
        }
        return (TFunc)(void*)ptr.func;
    }

    /// Get DLLs entry point (data).
    ///
    /// Get the entry point (a data) with name "name" in the DLL and
    /// return the entry point's address on success, or return NULL on error.
    /// If the DLL is not loaded yet, then this method will call Load(),
    /// which can result in throwing an exception if Load() fails.
    /// @sa
    ///   GetEntryPoint_Func
    template <class TData>
    TData GetEntryPoint_Data(const string& name, TData* data)
    {
        TEntryPoint ptr = GetEntryPoint(name);
        if ( data ) {
            *data = static_cast<TData> (ptr.data); 
        }
        return static_cast<TData> (ptr.data);
    }

    /// Fake, uncallable function pointer
    typedef void (*FEntryPoint)(char**** Do_Not_Call_This);

    /// Entry point -- pointer to either a function or a data
    union TEntryPoint {
        FEntryPoint func;  ///< Do not call this func without type cast!
        void*       data;
    };

    /// Helper find method for getting a DLLs entry point.
    ///
    /// Get the entry point (e.g. a function) with name "name" in the DLL.
    /// @param name
    ///   Name of DLL.
    /// @param pointer_size
    ///   Size of pointer.
    /// @return
    ///   The entry point's address on success, or return NULL on error.
    /// @sa
    ///   GetEntryPoint_Func, GetEntryPoint_Data
    NCBI_XNCBI_EXPORT
    TEntryPoint GetEntryPoint(const string& name);

    /// Get the name of the DLL file 
    NCBI_XNCBI_EXPORT
    const string& GetName() const { return m_Name; }

private:
    /// Helper method to throw exception with system-specific error message.
    NCBI_NORETURN
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
    ///   EFlags 
    void  x_Init(const string& path, const string& name, TFlags flags);

protected:
    /// Private copy constructor to prohibit copy.
    CDll(const CDll&);

    /// Private assignment operator to prohibit assignment.
    CDll& operator= (const CDll&);

private:
    string      m_Name;     ///< DLL name
    SDllHandle* m_Handle;   ///< DLL handle
    TFlags      m_Flags;    ///< Flags
};



/////////////////////////////////////////////////////////////////////////////
///
/// Class for entry point resolution when there are several DLL candidates.
///
/// If Dll resolver finds DLL with the specified entry point it is
/// stored in the internal list (provided by GetResolvedEntries method).
/// All DLL libraries are unloaded upon resolver's destruction
///
class CDllResolver
{
public:

    /// DLL entry point name -> function pair
    struct SNamedEntryPoint
    {
        string               name;          ///< Entry point name
        CDll::TEntryPoint    entry_point;   ///< DLL entry point

        SNamedEntryPoint(const string&       x_name,
                         CDll::TEntryPoint   x_entry_point)
        : name(x_name)
        {
            entry_point.data = x_entry_point.data;
        }
    };

    /// DLL resolution descriptor.
    struct SResolvedEntry
    {
        CDll*                     dll;           ///< Loaded DLL instance
        vector<SNamedEntryPoint>  entry_points;  ///< list of DLL entry points

        SResolvedEntry(CDll* dll_ptr = 0)
        : dll(dll_ptr)
        {}
    };

    /// Container, keeps list of all resolved entry points.
    typedef vector<SResolvedEntry>  TEntries;


    /// Constructor.
    ///
    /// @param entry_point_name
    ///   Name of the DLL entry point.
    /// @param unload
    ///   Whether to unload loaded DLLs in the destructor
    NCBI_XNCBI_EXPORT CDllResolver(const string& entry_point_name, 
                                   CDll::EAutoUnload unload = CDll::eAutoUnload);

    /// Constructor.
    ///
    /// @param entry_point_names
    ///   List of alternative DLL entry points.
    /// @param unload
    ///   Whether to unload loaded DLLs in the destructor
    NCBI_XNCBI_EXPORT CDllResolver(const vector<string>& entry_point_names,
                                   CDll::EAutoUnload unload = CDll::eAutoUnload); 

    /// Destructor.
    NCBI_XNCBI_EXPORT ~CDllResolver();

    /// Try to load DLL from the specified file and resolve the entry point.
    ///
    /// If DLL resolution successfull loaded entry point is registered in the
    /// internal list of resolved entries.
    ///
    /// @param file_name
    ///   Name of the DLL file. Can be full name with path of the base name.
    /// @param driver_name
    ///   Name of the driver (substitute for ${driver} macro)
    /// @return
    ///   TRUE if DLL is succesfully loaded and entry point resolved.
    /// @sa
    ///   GetResolvedEntries
    NCBI_XNCBI_EXPORT 
    bool TryCandidate(const string& file_name,
                      const string& driver_name = kEmptyStr);

    /// Try to resolve file candidates.
    ///
    /// @param candidates
    ///    Container with file names to try.
    /// @param driver_name
    ///    Driver name
    /// @sa
    ///   GetResolvedEntries
    template<class TClass>
    void Try(const TClass& candidates, const string& driver_name = kEmptyStr)
    {
        typename TClass::const_iterator it = candidates.begin();
        typename TClass::const_iterator it_end = candidates.end();
        for (; it != it_end; ++it) {
            TryCandidate(*it, driver_name);
        }
    }

    /// Various (usually system-dependent) standard paths to look for DLLs in.
    /// The fProgramPath flag works only inside CNcbiApplication framework.
    /// @sa
    ///   x_AddExtraDllPath, FindCandidates
    enum EExtraDllPath {
        fNoExtraDllPath = 0,        //< Do not add
        fProgramPath    = 1 << 0,   //< Path to executable file
        fToolkitDllPath = 1 << 1,   //< Toolkit paths
        fSystemDllPath  = 1 << 2,   //< System paths
        fDefaultDllPath = fProgramPath | fToolkitDllPath | fSystemDllPath
    };

    typedef int TExtraDllPath;      //<  bitwise OR of "EExtraDllPath"

    /// Try to resolve all files matching the specified masks in the
    /// specified directories.
    ///
    /// @param paths
    ///   Container with directory names.
    /// @param masks
    ///   Container with file candidate masks.
    /// @param extra_path
    ///   Extra "standard" paths to search the DLLs in
    /// @sa
    ///   GetResolvedEntries, x_AddExtraDllPath
    template<class TClass1, class TClass2>
    void FindCandidates(const TClass1& paths, const TClass2& masks,
                        TExtraDllPath extra_path = fDefaultDllPath,
                        const string& driver_name = kEmptyStr)
    {
        // search in the explicitly specified paths
        vector<string> x_path(paths);
        // search in "standard" paths, if any specified by 'extra_path' flag
        x_AddExtraDllPath(x_path, extra_path);
        // remove duplicate dirs
        vector<string> x_path_unique;
        x_path_unique.reserve(x_path.size());
#if defined(NCBI_OS_MSWIN)
        NStr::ECase use_case = NStr::eNocase;
#else
        NStr::ECase use_case = NStr::eCase;
#endif
        ITERATE(vector<string>, it, x_path) {
            bool found = false;
            ITERATE(vector<string>, i, x_path_unique) {
                if ( NStr::Compare(*i, *it, use_case) == 0 ) {
                    found = true;
                    break;
                }
            }
            if ( !found ) {
                x_path_unique.push_back(CDir::DeleteTrailingPathSeparator(*it));
            }
        }

        // find files
        vector<string> candidates;
        FindFiles(candidates,
                  x_path_unique.begin(), x_path_unique.end(),
                  masks.begin(), masks.end(),
                  fFF_File);
        // try to resolve entry points in the found DLLs
        Try(candidates, driver_name);
    }

    /// Get all resolved entry points.
    NCBI_XNCBI_EXPORT 
    const TEntries& GetResolvedEntries() const 
    { 
        return m_ResolvedEntries; 
    }

    /// Get all resolved entry points.
    NCBI_XNCBI_EXPORT
    TEntries& GetResolvedEntries() 
    { 
        return m_ResolvedEntries; 
    }

    /// Unload all resolved DLLs.
    NCBI_XNCBI_EXPORT void Unload();

private:
    CDllResolver(const CDllResolver&);
    CDllResolver& operator=(const CDllResolver&);

    /// Get the DLL search paths related to the given standard path group
    ///
    /// @param paths
    ///   Container to add the requested DLL search paths to.
    /// @param which
    ///   Which "standard" paths to add.
    /// @sa
    ///   FindCandidates
    NCBI_XNCBI_EXPORT
    void x_AddExtraDllPath(vector<string>& paths, TExtraDllPath which);

protected:
    vector<string>     m_EntryPoinNames;   ///< Candidate entry points
    TEntries           m_ResolvedEntries;
    CDll::EAutoUnload  m_AutoUnloadDll;
};

/* @} */


END_NCBI_SCOPE

#endif  /* CORELIB___NCBIDLL__HPP */
