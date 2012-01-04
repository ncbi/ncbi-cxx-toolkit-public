#ifndef CORELIB___METAREG__HPP
#define CORELIB___METAREG__HPP

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
 * Authors:  Aaron Ucko
 *
 */

/// @file metareg.hpp
/// CMetaRegistry: Singleton class for loading CRegistry data from
/// files; keeps track of what it loaded from where, for potential
/// reuse.

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbitime.hpp>

BEGIN_NCBI_SCOPE


template <typename T> class CSafeStaticPtr;

class NCBI_XNCBI_EXPORT CMetaRegistry
{
public:
    /// Relevant types

    /// General flags
    enum EFlags {
        fPrivate         = 0x1, ///< Do not cache, or support automatic saving.
        fReloadIfChanged = 0x2, ///< Reload if time or size has changed.
        fAlwaysReload    = 0x6, ///< Reload unconditionally.
        fKeepContents    = 0x8  ///< Keep existing contents when reloading.
    };
    typedef int TFlags; ///< Binary OR of "EFlags"

    /// How to treat filenames
    enum ENameStyle {
        eName_AsIs,   ///< Take the specified filename as is
        eName_Ini,    ///< Add .ini, dropping existing extensions as needed
        eName_DotRc,  ///< Transform into .*rc
        /// C Toolkit style; mostly useful with name = "ncbi"
#ifdef NCBI_OS_MSWIN
        eName_RcOrIni = eName_Ini
#else
        eName_RcOrIni = eName_DotRc
#endif
    };

    typedef IRegistry::TFlags TRegFlags;

    struct NCBI_XNCBI_EXPORT SEntry {
        string            actual_name; ///< Either an absolute path or empty.
        TFlags            flags;
        TRegFlags         reg_flags;
        CRef<IRWRegistry> registry;
        CTime             timestamp; ///< For cache validation
        Int8              length;    ///< For cache validation

        /// Reload the configuration file.  By default, does nothing if
        /// the file has the same size and date as before.
        ///
        /// Note that this may lose other data stored in the registry!
        ///
        /// @param reload_flags
        ///   Controls how aggressively to reload.
        /// @return
        ///   TRUE if a reload actually occurred.
        bool Reload(TFlags reload_flags = fReloadIfChanged);
    };

    static CMetaRegistry& Instance(void);

    /// Load the configuration file "name".
    ///
    /// @param name
    ///   The name of the configuration file to look for.  If it does
    ///   not contain a path, Load() searches in the default path list.
    /// @param style
    ///   How, if at all, to modify "name".
    /// @param flags
    ///   Any relevant options from EFlags above.
    /// @param reg
    ///   If NULL, yield a new CNcbiRegistry.  Otherwise, populate the
    ///   supplied registry (and don't try to share it if it didn't
    ///   start out empty).
    /// @return
    ///   On success, .actual_name will contain the absolute path to
    ///   the file ultimately loaded, and .registry will point to an
    ///   IRWRegistry object containing its contents (owned by this
    ///   class unless fPrivate or fDontOwn was given).
    ///   On failure, .actual_name will be empty and .registry will be
    ///   NULL.
    static SEntry Load(const string&  name,
                       ENameStyle     style     = eName_AsIs,
                       TFlags         flags     = 0,
                       TRegFlags      reg_flags = 0,
                       IRWRegistry*   reg       = 0);

    /// Reload the configuration file "path".
    ///
    /// @param path
    ///   A path (ideally absolute) to the configuration file to read.
    /// @param reg
    ///   The registry to repopulate.
    /// @param flags
    ///   Any relevant options from EFlags above.
    /// @param reg_flags
    ///   Flags to use when parsing the registry; ignored if the registry
    ///   was already cached.
    /// @return
    ///   TRUE if a reload actually occurred.
    static bool Reload(const string& path,
                       IRWRegistry&  reg,
                       TFlags        flags = 0,
                       TRegFlags     reg_flags = 0);

    /// Search path for unqualified names.
    typedef vector<string> TSearchPath;
    static const TSearchPath& GetSearchPath(void);
    static       TSearchPath& SetSearchPath(void);

    /// Clears path and substitutes the default search path.  If the
    /// environment NCBI_CONFIG_PATH is set, the default is to look there
    /// exclusively; otherwise, the default list contains the following
    /// directories in order:
    ///    - The current working directory.
    ///    - The user's home directory.
    ///    - The directory, if any, given by the environment variable "NCBI".
    ///    - The standard system directory (/etc on Unix, and given by the
    ///      environment variable "SYSTEMROOT" on Windows).
    ///    - The directory containing the application, if known.
    ///      (Requires use of CNcbiApplication.)
    /// The first two directories are skipped if the environment variable
    /// NCBI_DONT_USE_LOCAL_CONFIG is set.
    static void GetDefaultSearchPath(TSearchPath& path);

    /// Yield the path to a registry with the given name if available,
    /// or the empty string otherwise.
    static string FindRegistry(const string& name,
                               ENameStyle style = eName_AsIs);

private:
    /// Private functions, mostly non-static implementations of the
    /// public interface.

    CMetaRegistry();
    ~CMetaRegistry();

    /// name0 and style0 are the originally requested name and style
    const SEntry& x_Load(const string& name,  ENameStyle style,
                         TFlags flags, TRegFlags reg_flags, IRWRegistry* reg,
                         const string& name0, ENameStyle style0,
                         SEntry& scratch_entry);

    bool x_Reload(const string& path, IRWRegistry&  reg, TFlags flags,
                  TRegFlags reg_flags);

    const TSearchPath& x_GetSearchPath(void) const { return m_SearchPath; }
    TSearchPath&       x_SetSearchPath(void)
        { CMutexGuard GUARD(m_Mutex); m_Index.clear(); return m_SearchPath; }

    string x_FindRegistry(const string& name, ENameStyle style);

    /// Members
    struct SKey {
        string     requested_name;
        ENameStyle style;
        TFlags     flags;
        TRegFlags  reg_flags;

        SKey(string n, ENameStyle s, TFlags f, TRegFlags rf)
            : requested_name(n), style(s), flags(f), reg_flags(rf) { }
        bool operator <(const SKey& k) const;
    };
    typedef map<SKey, size_t> TIndex;

    vector<SEntry> m_Contents;
    TSearchPath    m_SearchPath;
    TIndex         m_Index;

    CMutex         m_Mutex;

    friend class  CSafeStaticPtr<CMetaRegistry>;
    friend struct SEntry;
};



/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


inline
bool CMetaRegistry::Reload(const string& path,
                           IRWRegistry&  reg,
                           TFlags        flags,
                           TRegFlags     reg_flags)
{
    return Instance().x_Reload(path, reg, flags, reg_flags);
}

inline
const CMetaRegistry::TSearchPath& CMetaRegistry::GetSearchPath(void)
{
    return Instance().x_GetSearchPath();
}


inline
CMetaRegistry::TSearchPath& CMetaRegistry::SetSearchPath(void)
{
    return Instance().x_SetSearchPath();
}


inline
string CMetaRegistry::FindRegistry(const string& name, ENameStyle style)
{
    return Instance().x_FindRegistry(name, style);
}


inline
CMetaRegistry::CMetaRegistry()
{
    GetDefaultSearchPath(x_SetSearchPath());
}


END_NCBI_SCOPE

#endif  /* CORELIB___METAREG__HPP */
