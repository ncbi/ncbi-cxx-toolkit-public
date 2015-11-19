#ifndef CORELIB___NCBIREG__HPP
#define CORELIB___NCBIREG__HPP

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
 * Authors:  Denis Vakatov, Aaron Ucko
 *
 */

/// @file ncbireg.hpp
/// Process information in the NCBI Registry, including working with
/// configuration files.
///
/// Classes to perform NCBI Registry operations including:
/// - Read and parse configuration file
/// - Search, edit, etc. the retrieved configuration information
/// - Write information back to configuration file
///
/// The Registry is defined as a text file with sections and entries in the 
/// form of "name=value" strings in each section. 
///
/// For an explanation of the syntax of the Registry file, see the
/// C++ Toolkit documentation.


#include <corelib/ncbi_limits.h>
#include <corelib/ncbimtx.hpp>
#include <map>
#include <set>


/** @addtogroup Registry
 *
 * @{
 */


BEGIN_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
///
/// IRegistry --
///
/// Base class for organized configuration data.
///
/// Does not define a specific in-memory representation, just a
/// read-only API and some convenience methods.

class NCBI_XNCBI_EXPORT IRegistry : public CObject
{
public:
    /// Flags controlling the behavior of registry methods.  Please note:
    /// - Although CNcbiRegistry supports a full complement of layers, other
    ///   derived classes may ignore some or all level-related flags.
    /// - Most read-only operations consider all layers; the only exception
    ///   is Write, which defaults to fPersistent and fJustCore.
    /// - The presence or absence of fSectionCase and fEntryCase is relevant
    ///   ONLY when constructing new registry objects.
    enum EFlags {
        fTransient      = 0x1,   ///< Transient -- not saved by default
        fPersistent     = 0x100, ///< Persistent -- saved when file is written
        fOverride       = 0x2,   ///< Existing value can be overriden
        fNoOverride     = 0x200, ///< Cannot change existing value
        fTruncate       = 0x4,   ///< Leading, trailing blanks can be truncated
        fNoTruncate     = 0x400, ///< Cannot truncate parameter value
        fJustCore       = 0x8,   ///< Ignore auxiliary subregistries
        fNotJustCore    = 0x800, ///< Include auxiliary subregistries
        fIgnoreErrors   = 0x10,  ///< Continue Read()ing after parse errors
        fInternalSpaces = 0x20,  ///< Allow internal whitespace in names
        fWithNcbirc     = 0x40,  ///< Include .ncbirc (used only by CNcbiReg.)
        fCountCleared   = 0x80,  ///< Let explicitly cleared entries stand
        fSectionCase    = 0x1000,///< Create with case-sensitive section names
        fEntryCase      = 0x2000,///< Create with case-sensitive entry names
        fSectionlessEntries = 0x4000,///< Allow empty section names (for test_res framework)
        fSections       = 0x8000,///< Indicates that we want sections from x_Enumerate
        fPlaintextAllowed = 0x10000,///< @sa GetEncryptedString()
        fCoreLayers     = fTransient | fPersistent | fJustCore,
        fAllLayers      = fTransient | fPersistent | fNotJustCore,
        fCaseFlags      = fSectionCase | fEntryCase
    };
    typedef int TFlags;  ///< Binary OR of "EFlags"

    /// Verify if Registry is empty.
    /// @param flags
    ///   Layer(s) to check.
    /// @return
    ///   TRUE if the registry contains no entries.
    bool Empty(TFlags flags = fAllLayers) const;

    /// Verify if persistent values have been modified.
    /// @param flags
    ///   Layer(s) to check.
    /// @return
    ///   TRUE if the relevant part(s) of the registry were modified since the
    ///   last call to SetModifiedFlag(false).
    bool Modified(TFlags flags = fPersistent) const;

    /// Indicate whether any relevant values are out of sync with some
    /// external resource (typically a configuration file).  You
    /// should normally not need to call this explicitly.
    /// @param flags
    ///   Relevant layer(s).
    void SetModifiedFlag(bool modified, TFlags flags = fPersistent);

    /// Write the registry content to output stream.
    /// @param os
    ///   Output stream to write the registry to.
    ///   NOTE:  if the stream is a file, it must be opened in binary mode!
    /// @param flags
    ///   Layer(s) to write.  By default, only persistent entries are
    ///   written, and only entries from the core layer(s) are written.
    /// @return
    ///   TRUE if operation is successful.
    /// @sa
    ///   IRWRegistry::Read()
    bool Write(CNcbiOstream& os, TFlags flags = 0) const;

    /// Get the parameter value.
    ///
    /// Get the parameter with the specified "name" from the specified
    /// "section".  First, search for the transient parameter value, and if
    /// cannot find in there, then continue the search in the non-transient
    /// parameters. If "fPersistent" flag is set in "flags", then don't
    /// search in the transient parameters at all.
    /// @param section
    ///   Section name to search under (case-insensitive).
    /// @param name
    ///   Parameter name to search for (case-insensitive).
    /// @param flags
    ///   To control search.
    /// @return
    ///   The parameter value, or empty string if the parameter is not found.
    /// @sa
    ///   GetString(), GetEncryptedString()
    const string& Get(const string& section,
                      const string& name,
                      TFlags        flags = 0) const;

    bool HasEntry(const string& section,
                  const string& name = kEmptyStr,
                  TFlags        flags = 0) const;

    /// Get the parameter string value.
    ///
    /// Similar to the "Get()", but if the configuration parameter is not
    /// found, then return 'default_value' rather than empty string.
    /// @sa
    ///   Get(), GetEncryptedString()
    string GetString(const string& section,
                     const string& name,
                     const string& default_value,
                     TFlags        flags = 0) const;

    /// Get a value that was (potentially) stored encrypted.
    ///
    /// Similar to Get(), but if the value appears to have been
    /// encrypted, return the corresponding plaintext.  Otherwise, unless
    /// flags contains fPlaintextAllowed, throw an exception to let the
    /// user know of inappropriate plaintext in the configuration file.
    /// @param password
    ///   Explicitly supply a password.  If absent, will try standard keys,
    ///   honoring any key domain recorded in the value.
    /// @sa Get(), GetString(), CNcbiEncrypt
    string GetEncryptedString(const string& section,
                            const string& name,
                            TFlags        flags = 0,
                            const string& password = kEmptyStr) const;

    /// What to do if parameter value is present but cannot be converted into
    /// the requested type.
    enum EErrAction {
        eThrow,   ///< Throw an exception if an error occurs
        eErrPost, ///< Log the error message and return default value
        eReturn   ///< Return default value
    };

    /// Get integer value of specified parameter name.
    ///
    /// Like "GetString()", plus convert the value into integer.
    /// @param err_action
    ///   What to do if error encountered in converting parameter value.
    /// @sa
    ///   GetString()
    int GetInt(const string& section,
               const string& name,
               int           default_value,
               TFlags        flags      = 0,
               EErrAction    err_action = eThrow) const;

    /// Get boolean value of specified parameter name.
    ///
    /// Like "GetString()", plus convert the value into boolean.
    /// @param err_action
    ///   What to do if error encountered in converting parameter value.
    /// @sa
    ///   GetString()
    bool GetBool(const string& section,
                 const string& name,
                 bool          default_value,
                 TFlags        flags      = 0,
                 EErrAction    err_action = eThrow) const;

    /// Get double value of specified parameter name.
    ///
    /// Like "GetString()", plus convert the value into double.
    /// @param err_action
    ///   What to do if error encountered in converting parameter value.
    /// @sa
    ///   GetString()
    double GetDouble(const string& section,
                     const string& name,
                     double        default_value,
                     TFlags        flags = 0,
                     EErrAction    err_action = eThrow) const;

    /// Get comment of the registry entry "section:name".
    ///
    /// @param section
    ///   Section name.
    ///   If passed empty string, then get the registry comment.
    /// @param name
    ///   Parameter name.
    ///   If empty string, then get the "section" comment.
    /// @param flags
    ///   To control search.
    /// @return
    ///   Comment string. If not found, return an empty string.
    const string& GetComment(const string& section = kEmptyStr,
                             const string& name    = kEmptyStr,
                             TFlags        flags   = 0) const;

    /// Enumerate section names.
    ///
    /// Write all section names to the "sections" list in
    /// (case-insensitive) order.  Previous contents of the list are
    /// erased.
    /// @param flags
    ///   To control search.
    void EnumerateSections(list<string>* sections,
                           TFlags        flags = fAllLayers) const;

    /// Enumerate parameter names for a specified section.
    ///
    /// Write all parameter names for specified "section" to the "entries"
    /// list in order.  Previous contents of the list are erased.  
    /// @deprecated Enumerates sections rather than entries if section is
    /// @deprecated empty and fSectionlessEntries is not set.
    /// @param flags
    ///   To control search.
    void EnumerateEntries(const string& section,
                          list<string>* entries,
                          TFlags        flags = fAllLayers) const;

    /// Support for locking.  Individual operations already use these
    /// to ensure atomicity, but the locking mechanism is recursive,
    /// so users can also make entire sequences of operations atomic.
    void ReadLock (void);
    void WriteLock(void);
    void Unlock   (void);

#ifdef NCBI_COMPILER_ICC
    /// Work around a compiler bug that can cause memory leaks.
    virtual ~IRegistry() { }
#endif

protected:
    enum EMasks {
        fLayerFlags = fAllLayers | fJustCore,
        fTPFlags    = fTransient | fPersistent
    };

    static void x_CheckFlags(const string& func, TFlags& flags,
                             TFlags allowed);
    /// Implementations of the fundamental operations above, to be run with
    /// the lock already acquired and some basic sanity checks performed.
    virtual bool x_Empty(TFlags flags) const = 0;
    virtual bool x_Modified(TFlags /* flags */) const { return false; }
    virtual void x_SetModifiedFlag(bool /* modified */, TFlags /* flags */) {}
    virtual const string& x_Get(const string& section, const string& name,
                                TFlags flags) const = 0;
    virtual bool x_HasEntry(const string& section, const string& name,
                            TFlags flags) const = 0;
    virtual const string& x_GetComment(const string& section,
                                       const string& name, TFlags flags)
        const = 0;
    // enumerate sections rather than entries if section is empty
    virtual void x_Enumerate(const string& section, list<string>& entries,
                             TFlags flags) const = 0;

    typedef void (IRegistry::*FLockAction)(void);
    virtual void x_ChildLockAction(FLockAction /* action */) {}

private:
    mutable CRWLock m_Lock;
};



/////////////////////////////////////////////////////////////////////////////
///
/// IRWRegistry --
///
/// Abstract subclass for modifiable registries.
///
/// To avoid confusion, all registry classes that support modification
/// should inherit from this class.

class NCBI_XNCBI_EXPORT IRWRegistry : public IRegistry
{
public:
    /// Categories of modifying operations
    enum EOperation {
        eClear,
        eRead,
        eSet
    };

    /// Indicate which portions of the registry the given operation
    /// would affect.
    static TFlags AssessImpact(TFlags flags, EOperation op);

    /// Reset the registry content.
    void Clear(TFlags flags = fAllLayers);

    /// Read and parse the stream "is", and merge its content with current
    /// Registry entries.
    ///
    /// Once the Registry has been initialized by the constructor, it is 
    /// possible to load other parameters from other files using this method.
    /// @param is
    ///   Input stream to read and parse.
    ///   NOTE:  if the stream is a file, it must be opened in binary mode!
    /// @param flags
    ///   How parameters are stored. The default is for all values to be read
    ///   as persistent with the capability of overriding any previously
    ///   loaded value associated with the same name. The default can be
    ///   modified by specifying "fTransient", "fNoOverride" or 
    ///   "fTransient | fNoOverride". If there is a conflict between the old
    ///   and the new (loaded) entry value and if "fNoOverride" flag is set,
    ///   then just ignore the new value; otherwise, replace the old value by
    ///   the new one. If "fTransient" flag is set, then store the newly
    ///   retrieved parameters as transient;  otherwise, store them as
    ///   persistent.
    /// @param path
    ///   Where to look for base registries listed with relative paths.
    /// @return
    ///   A pointer to a newly created subregistry, if any, directly
    ///   containing the entries loaded from is.
    /// @sa
    ///   Write()
    IRWRegistry* Read(CNcbiIstream& is, TFlags flags = 0,
                      const string& path = kEmptyStr);

    /// Set the configuration parameter value.
    ///
    /// @param value
    ///   Value that the parameter is set to.
    /// @param flags
    ///   To control search.
    ///   Valid flags := { fPersistent, fNoOverride, fTruncate }
    ///   If there was already an entry with the same <section,name> key:
    ///     if "fNoOverride" flag is set then do not override old value
    ///     and return FALSE;  else override the old value and return TRUE.
    ///   If "fPersistent" flag is set then store the entry as persistent;
    ///     else store it as transient.
    ///   If "fTruncate" flag is set then truncate the leading and trailing
    ///     spaces -- " \r\t\v" (NOTE:  '\n' is not considered a space!).
    /// @param comment
    ///   Optional comment string describing parameter.
    /// @return
    ///   TRUE if successful (including replacing a value with itself)
    bool Set(const string& section,
             const string& name,
             const string& value,
             TFlags        flags   = 0,
             const string& comment = kEmptyStr);

    /// Fully unset the configuration parameter value, so that
    /// HasEntry returns false.
    /// @return
    ///   TRUE if there had been an existing value.
    bool Unset(const string& section, const string& name, TFlags flags = 0);
    
    /// Set comment "comment" for the registry entry "section:name".
    ///
    /// @param comment
    ///   Comment string value.
    ///   Set to kEmptyStr to delete the comment.
    /// @param section
    ///   Section name.
    ///   If "section" is empty string, then set as the registry comment.
    /// @param name
    ///   Parameter name.
    ///   If "name" is empty string, then set as the "section" comment.
    /// @param flags
    ///   How the comment is stored. The default is for comments to be
    ///   stored as persistent with the capability of overriding any
    ///   previously loaded value associated with the same name. The
    ///   default can be modified by specifying "fTransient", "fNoOverride"
    ///   or "fTransient | fNoOverride". If there is a conflict between the
    ///   old and the new comment and if "fNoOverride" flag is set, then
    ///   just ignore the new comment; otherwise, replace the old comment
    ///   by the new one. If "fTransient" flag is set, then store the new
    ///   comment as transient (generally not desired); otherwise, store it
    ///   as persistent.
    /// @return
    ///   FALSE if "section" and/or "name" do not exist in registry.
    bool SetComment(const string& comment,
                    const string& section = kEmptyStr,
                    const string& name    = kEmptyStr,
                    TFlags        flags   = 0);

#ifdef NCBI_COMPILER_ICC
    /// Work around a compiler bug that can cause memory leaks.
    virtual ~IRWRegistry() { }
#endif

protected:
    /// Called locked, like the virtual methods inherited from IRegistry.
    virtual void x_Clear(TFlags flags) = 0;
    virtual bool x_Set(const string& section, const string& name,
                       const string& value, TFlags flags,
                       const string& comment) = 0;
    virtual bool x_Unset(const string& section, const string& name,
                         TFlags flags) = 0;
    virtual bool x_SetComment(const string& comment, const string& section,
                              const string& name, TFlags flags) = 0;

    /// Most implementations should not override this, but
    /// CNcbiRegistry must, to handle some special cases properly.
    virtual IRWRegistry* x_Read(CNcbiIstream& is, TFlags flags,
                                const string& path);

    // for use by implementations
    static bool MaybeSet(string& target, const string& value, TFlags flags);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CMemoryRegistry --
///
/// Straightforward monolithic modifiable registry.

class NCBI_XNCBI_EXPORT CMemoryRegistry : public IRWRegistry
{
public:
    CMemoryRegistry(TFlags flags = 0)
        : m_IsModified(false),
          m_Sections((flags & fSectionCase) == 0 ? NStr::eNocase : NStr::eCase),
          m_Flags(flags)
        {}

#ifdef NCBI_COMPILER_ICC
    /// Work around a compiler bug that can cause memory leaks.
    virtual ~CMemoryRegistry() { }
#endif

protected:
    bool x_Empty(TFlags flags) const;
    bool x_Modified(TFlags) const { return m_IsModified; }
    void x_SetModifiedFlag(bool modified, TFlags) { m_IsModified = modified; }
    const string& x_Get(const string& section, const string& name,
                        TFlags flags) const;
    bool x_HasEntry(const string& section, const string& name, TFlags flags)
        const;
    const string& x_GetComment(const string& section, const string& name,
                               TFlags flags) const;
    void x_Enumerate(const string& section, list<string>& entries,
                     TFlags flags) const;

    void x_Clear(TFlags flags);
    bool x_Set(const string& section, const string& name,
               const string& value, TFlags flags,
               const string& comment);
    bool x_Unset(const string& section, const string& name, TFlags flags);
    bool x_SetComment(const string& comment, const string& section,
                      const string& name, TFlags flags);

public: // WorkShop needs these exposed
    struct SEntry {
        string value, comment;
    };
    typedef map<string, SEntry, PNocase_Conditional> TEntries;
    struct SSection {
        SSection(TFlags flags)
            : entries((flags & fEntryCase) == 0 ? NStr::eNocase : NStr::eCase),
              cleared(false)
            { }
        string   comment;
        TEntries entries;
        bool     cleared;
    };
    typedef map<string, SSection, PNocase_Conditional> TSections;

private:
    bool      m_IsModified;
    string    m_RegistryComment;
    TSections m_Sections;
    TFlags    m_Flags;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CCompoundRegistry --
///
/// Prioritized read-only collection of sub-registries.
///
/// @sa
///  CTwoLayerRegistry

class NCBI_XNCBI_EXPORT CCompoundRegistry : public IRegistry
{
public:
    CCompoundRegistry() : m_CoreCutoff(ePriority_Default) { }

#ifdef NCBI_COMPILER_ICC
    /// Work around a compiler bug that can cause memory leaks.
    virtual ~CCompoundRegistry() { }
#endif

    /// Priority for sub-registries; entries in higher-priority
    /// sub-registries take precedence over (identically named) entries
    /// in lower-priority ones.  Ties are broken arbitrarily.
    enum EPriority {
        ePriority_Min     = kMin_Int,
        ePriority_Default = 0,
        ePriority_Max     = kMax_Int
    };
    typedef int TPriority; ///< Not restricted to ePriority_*.

    /// Non-empty names must be unique within each compound registry,
    /// but there is no limit to the number of anonymous sub-registries.
    /// Sub-registries themselves may not (directly) appear more than once.
    void Add(const IRegistry& reg,
             TPriority        prio = ePriority_Default,
             const string&    name = kEmptyStr);

    /// Remove sub-registry "reg".
    /// Throw an exception if "reg" is not a (direct) sub-registry.
    void Remove(const IRegistry& reg);

    /// Subregistries whose priority is less than the core cutoff
    /// (ePriority_Default by default) will be ignored for fJustCore
    /// operations, such as Write by default.
    TPriority GetCoreCutoff(void) const     { return m_CoreCutoff; }
    void      SetCoreCutoff(TPriority prio) { m_CoreCutoff = prio; }

    /// Return a pointer to the sub-registry with the given name, or
    /// NULL if not found.
    CConstRef<IRegistry> FindByName(const string& name) const;

    /// Return a pointer to the highest-priority sub-registry with a
    /// section named SECTION containing (if ENTRY is non-empty) an entry
    /// named ENTRY, or NULL if not found.
    CConstRef<IRegistry> FindByContents(const string& section,
                                        const string& entry = kEmptyStr,
                                        TFlags        flags = 0) const;

    // allow enumerating sub-registries?

protected:    
    // virtual methods of IRegistry

    /// True iff all sub-registries are empty
    bool x_Empty(TFlags flags) const;

    /// True iff any sub-registry is modified
    bool x_Modified(TFlags flags) const;
    void x_SetModifiedFlag(bool modified, TFlags flags);
    const string& x_Get(const string& section, const string& name,
                        TFlags flags) const;
    bool x_HasEntry(const string& section, const string& name, TFlags flags)
        const;
    const string& x_GetComment(const string& section, const string& name,
                               TFlags flags) const;
    void x_Enumerate(const string& section, list<string>& entries,
                     TFlags flags) const;
    void x_ChildLockAction(FLockAction action);

private:
    typedef multimap<TPriority, CRef<IRegistry> > TPriorityMap;
    typedef map<string, CRef<IRegistry> >         TNameMap;

    TPriorityMap m_PriorityMap; 
    TNameMap     m_NameMap;     ///< excludes anonymous sub-registries
    TPriority    m_CoreCutoff;

    friend class CCompoundRWRegistry;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CTwoLayerRegistry --
///
/// Limited to two direct layers (transient above persistent), but
/// supports modification.
///
/// @sa
///  CCompoundRegistry

class NCBI_XNCBI_EXPORT CTwoLayerRegistry : public IRWRegistry
{
public:
    /// Constructor.  The transient layer is always a new memory registry,
    /// and so is the persistent layer by default.
    CTwoLayerRegistry(IRWRegistry* persistent = 0, TFlags flags = 0);

#ifdef NCBI_COMPILER_ICC
    /// Work around a compiler bug that can cause memory leaks.
    virtual ~CTwoLayerRegistry() { }
#endif

protected:
    bool x_Empty(TFlags flags) const;
    bool x_Modified(TFlags flags) const;
    void x_SetModifiedFlag(bool modified, TFlags flags);
    const string& x_Get(const string& section, const string& name,
                        TFlags flags) const;
    bool x_HasEntry(const string& section, const string& name,
                    TFlags flags) const;
    const string& x_GetComment(const string& section, const string& name,
                               TFlags flags) const;
    void x_Enumerate(const string& section, list<string>& entries,
                     TFlags flags) const;
    void x_ChildLockAction(FLockAction action);

    void x_Clear(TFlags flags);
    bool x_Set(const string& section, const string& name,
               const string& value, TFlags flags,
               const string& comment);
    bool x_Unset(const string& section, const string& name, TFlags flags);
    bool x_SetComment(const string& comment, const string& section,
                      const string& name, TFlags flags);

private:
    typedef CRef<IRWRegistry> CRegRef;
    CRegRef m_Transient;
    CRegRef m_Persistent;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CCompoundRWRegistry --
///
/// Writeable compound registry.
///
/// Compound registry whose top layer is a two-layer registry; all
/// writes go to the two-layer registry.

class NCBI_XNCBI_EXPORT CCompoundRWRegistry : public IRWRegistry
{
public:
    /// Constructor.
    CCompoundRWRegistry(TFlags m_Flags = 0);

    /// Destructor.
    ~CCompoundRWRegistry();

    /// Priority for sub-registries; entries in higher-priority
    /// sub-registries take precedence over (identically named) entries
    /// in lower-priority ones.  Ties are broken arbitrarily.
    enum EPriority {
        ePriority_MinUser  = CCompoundRegistry::ePriority_Min,
        ePriority_Default  = CCompoundRegistry::ePriority_Default,
        ePriority_MaxUser  = CCompoundRegistry::ePriority_Max - 0x10000,
        ePriority_Reserved ///< Everything greater is for internal use.
    };
    typedef int TPriority; ///< Not restricted to ePriority_*.

    /// Subregistries whose priority is less than the core cutoff
    /// (ePriority_Reserved by default) will be ignored for fJustCore
    /// operations, such as Write by default.
    TPriority GetCoreCutoff(void) const;
    void      SetCoreCutoff(TPriority prio);

    /// Non-empty names must be unique within each compound registry,
    /// but there is no limit to the number of anonymous sub-registries.
    /// Sub-registries themselves may not (directly) appear more than once.
    void Add(const IRegistry& reg,
             TPriority        prio = ePriority_Default,
             const string&    name = kEmptyStr);

    /// Remove sub-registry "reg".
    /// Throw an exception if "reg" is not a (direct) sub-registry.
    void Remove(const IRegistry& reg);

    /// Return a pointer to the sub-registry with the given name, or
    /// NULL if not found.
    CConstRef<IRegistry> FindByName(const string& name) const;

    /// Return a pointer to the highest-priority sub-registry with a
    /// section named SECTION containing (if ENTRY is non-empty) an entry
    /// named ENTRY, or NULL if not found.
    CConstRef<IRegistry> FindByContents(const string& section,
                                        const string& entry = kEmptyStr,
                                        TFlags        flags = 0) const;

    /// Load any base registries listed in [NCBI].Inherits; returns
    /// true if able to load at least one, false otherwise.
    /// @param flags
    ///   Registry flags to apply.
    /// @param metareg_flags
    ///   Metaregistry flags to apply.
    /// @param path
    ///   Where to look for base registries listed with relative paths.
    bool LoadBaseRegistries(TFlags flags = 0,
                            int /* CMetaRegistry::TFlags */ metareg_flags = 0,
                            const string& path = kEmptyStr);

    /// Predefined subregistry's name.
    static const char* sm_MainRegName;
    /// Prefix for any base registries' names.
    static const char* sm_BaseRegNamePrefix;

protected:
    bool x_Empty(TFlags flags) const;
    bool x_Modified(TFlags flags) const;
    void x_SetModifiedFlag(bool modified, TFlags flags);
    const string& x_Get(const string& section, const string& name,
                        TFlags flags) const;
    bool x_HasEntry(const string& section, const string& name,
                    TFlags flags) const;
    const string& x_GetComment(const string& section, const string& name,
                               TFlags flags) const;
    void x_Enumerate(const string& section, list<string>& entries,
                     TFlags flags) const;
    void x_ChildLockAction(FLockAction action);

    void x_Clear(TFlags flags);
    bool x_Set(const string& section, const string& name,
               const string& value, TFlags flags,
               const string& comment);
    bool x_Unset(const string& section, const string& name, TFlags flags);
    bool x_SetComment(const string& comment, const string& section,
                      const string& name, TFlags flags);
    IRWRegistry* x_Read(CNcbiIstream& is, TFlags flags, const string& path);

    /// Add an internal high-priority subregistry.
    void x_Add(const IRegistry& reg,
               TPriority        prio = ePriority_Default,
               const string&    name = kEmptyStr);

private:
    typedef map<string, TFlags> TClearedEntries;

    TClearedEntries         m_ClearedEntries;
    CRef<CTwoLayerRegistry> m_MainRegistry;
    CRef<CCompoundRegistry> m_AllRegistries;
    set<string>             m_BaseRegNames;
    TFlags                  m_Flags;
};


class CEnvironmentRegistry; // see <corelib/env_reg.hpp>



/////////////////////////////////////////////////////////////////////////////
///
/// CNcbiRegistry --
///
/// Define the Registry.
///
/// Load, access, modify and store runtime information (usually used
/// to work with configuration files).

class NCBI_XNCBI_EXPORT CNcbiRegistry : public CCompoundRWRegistry
{
public:
    enum ECompatFlags {
        eTransient   = fTransient,
        ePersistent  = fPersistent,
        eOverride    = fOverride,
        eNoOverride  = fNoOverride,
        eTruncate    = fTruncate,
        eNoTruncate  = fNoTruncate
    };

    /// Constructor.
    CNcbiRegistry(TFlags flags = 0);

    /// Constructor.
    ///
    /// @param is
    ///   Input stream to load the Registry from.
    ///   NOTE:  if the stream is a file, it must be opened in binary mode!
    /// @param flags
    ///   How parameters are stored. The default is to store all parameters as
    ///   persistent unless the  "eTransient" flag is set in which case the
    ///   newly retrieved parameters are stored as transient.
    /// @param path
    ///   Where to look for base registries listed with relative paths.
    /// @sa
    ///   Read()
    CNcbiRegistry(CNcbiIstream& is, TFlags flags = 0,
                  const string& path = kEmptyStr);

    ~CNcbiRegistry();

    /// Attempt to load a systemwide configuration file (.ncbirc on
    /// Unix, ncbi.ini on Windows) as a low-priority registry, as long
    /// as the following conditions all hold:
    /// - fWithNcbirc is set in FLAGS.
    /// - The environment variable NCBI_DONT_USE_NCBIRC is NOT set.
    /// - The registry's existing contents do NOT contain a setting of
    ///   [NCBI]DONT_USE_NCBIRC (case-insensitive).
    /// @param flags
    ///   Registry flags to be applied when reading the system
    ///   configuration file.  Must also contain fWithNcbirc (which
    ///   will be filtered out before calling any other methods) for
    ///   the call to have any effect.
    /// @return
    ///   TRUE if the system configuration file was successfully read
    ///   and parsed; FALSE otherwise.
    bool IncludeNcbircIfAllowed(TFlags flags = fWithNcbirc);

    /// Predefined subregistries' names.
    static const char* sm_EnvRegName;
    static const char* sm_FileRegName;
    static const char* sm_OverrideRegName;
    static const char* sm_SysRegName;

protected:
    void x_Clear(TFlags flags);
    IRWRegistry* x_Read(CNcbiIstream& is, TFlags flags, const string& path);

private:
    void x_Init(void);

    enum EReservedPriority {
        ePriority_File = ePriority_Reserved,
        ePriority_Overrides,
        ePriority_Environment,
        ePriority_RuntimeOverrides
    };

    CRef<CEnvironmentRegistry> m_EnvRegistry;
    CRef<CTwoLayerRegistry>    m_FileRegistry;
    CRef<IRWRegistry>          m_OverrideRegistry;
    CRef<IRWRegistry>          m_SysRegistry;
    unsigned int               m_RuntimeOverrideCount;
    TFlags                     m_Flags;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CRegistryException --
///
/// Define exceptions generated by IRegistry and derived classes.
///
/// CRegistryException inherits its basic functionality from
/// CCParseTemplException<CCoreException> and defines additional error codes
/// for the Registry.

class NCBI_XNCBI_EXPORT CRegistryException : public CParseTemplException<CCoreException>
{
public:
    /// Error types that the Registry can generate.
    enum EErrCode {
        eSection,          ///< Section name format error
        eEntry,            ///< Entry name format error
        eValue,            ///< General value format error
        eUnencrypted,      ///< Value should have been encrypted, but wasn't
        eDecryptionFailed, ///< Value looked encrypted, but decryption failed
        eErr               ///< Other error
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code
    NCBI_EXCEPTION_DEFAULT2(CRegistryException,
                            CParseTemplException<CCoreException>,
                            std::string::size_type);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CRegistry{Read,Write}Guard --
///
/// Guard classes to ensure one-thread-at-a-time access to registries.

class NCBI_XNCBI_EXPORT CRegistryReadGuard
    : public CGuard<IRegistry, SSimpleReadLock<IRegistry> >
{
public:
    typedef CGuard<IRegistry, SSimpleReadLock<IRegistry> > TParent;
    CRegistryReadGuard(const IRegistry& reg)
        : TParent(const_cast<IRegistry&>(reg))
        { }
};

typedef CGuard<IRegistry, SSimpleWriteLock<IRegistry> > CRegistryWriteGuard;

END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB___NCBIREG__HPP */
