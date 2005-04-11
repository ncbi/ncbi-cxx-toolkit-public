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

BEGIN_NCBI_SCOPE


template <typename T> class CSafeStaticPtr;

class NCBI_XNCBI_EXPORT CMetaRegistry
{
public:
    /// Relevant types

    /// General flags
    enum EFlags {
        fPrivate = 0x1 ///< Do not cache, or support automatic saving.
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

    /// m_ActualName is always an absolute path (or empty)
    struct SEntry {
        string            actual_name;
        TFlags            flags;
        TRegFlags         reg_flags;
        CRef<IRWRegistry> registry;
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

private:
    /// Private functions, mostly non-static implementations of the
    /// public interface.

    CMetaRegistry();
    ~CMetaRegistry();

    /// name0 and style0 are the originally requested name and style
    SEntry x_Load(const string& name,  ENameStyle style,
                  TFlags flags, TRegFlags reg_flags, IRWRegistry* reg,
                  const string& name0, ENameStyle style0);

    const TSearchPath& x_GetSearchPath(void) const { return m_SearchPath; }
    TSearchPath&       x_SetSearchPath(void)
        { CMutexGuard GUARD(m_Mutex); m_Index.clear(); return m_SearchPath; }

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
    typedef map<SKey, unsigned int> TIndex;

    vector<SEntry> m_Contents;
    TSearchPath    m_SearchPath;
    TIndex         m_Index;

    CMutex         m_Mutex;

    friend class CSafeStaticPtr<CMetaRegistry>;
};



/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


inline
CMetaRegistry::SEntry CMetaRegistry::Load(const string& name,
                                          CMetaRegistry::ENameStyle style,
                                          CMetaRegistry::TFlags flags,
                                          IRegistry::TFlags reg_flags,
                                          IRWRegistry* reg)
{
    return Instance().x_Load(name, style, flags, reg_flags, reg, name, style);
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
CMetaRegistry::CMetaRegistry()
{
    GetDefaultSearchPath(x_SetSearchPath());
}


END_NCBI_SCOPE



/*
* ===========================================================================
*
* $Log$
* Revision 1.12  2005/04/11 17:18:17  ucko
* Doxygen fix: associate the long comment describing the default search
* path with GetDefaultSearchPath, not TSearchPath.
*
* Revision 1.11  2005/01/25 19:56:34  ucko
* Hold the single instance with a CSafeStaticPtr rather than a static auto_ptr.
*
* Revision 1.10  2005/01/10 16:56:45  ucko
* Support working with arbitrary IRWRegistry objects, and take
* advantage of the fact that they're CObjects these days.
*
* Revision 1.9  2005/01/10 16:22:56  ucko
* Adjust default search path (now ordered like the C Toolkit's), and add
* support for NCBI_CONFIG_PATH and NCBI_DONT_USE_LOCAL_CONFIG.
*
* Revision 1.8  2003/12/08 18:40:13  ucko
* Use DECLARE_CLASS_STATIC_MUTEX to avoid possible premature locking.
*
* Revision 1.7  2003/09/30 21:05:56  ucko
* Refactored cache to allow flushing of path searches when the search
* path changes.
*
* Revision 1.6  2003/08/18 19:48:28  ucko
* Remove stray comma at end of enum list in EFlags.
*
* Revision 1.5  2003/08/15 03:44:57  ucko
* Add export specifier.
*
* Revision 1.4  2003/08/14 17:37:26  ucko
* +eName_RcOrIni (for .ncbirc / ncbi.ini)
*
* Revision 1.3  2003/08/12 19:01:55  vakatov
* Minor, formal code style amendments
*
* Revision 1.2  2003/08/06 20:25:55  ucko
* Allow Load to take an existing registry to reuse.
*
* Revision 1.1  2003/08/05 19:57:46  ucko
* CMetaRegistry: Singleton class for loading CRegistry data from files;
* keeps track of what it loaded from where, for potential reuse.
*
*
* ===========================================================================
*/

#endif  /* CORELIB___METAREG__HPP */
