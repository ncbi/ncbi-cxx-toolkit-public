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
#include <corelib/ncbiobj.hpp>
#include <map>


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
    enum EFlags {
        fTransient   = 0x1,     ///< Transient -- not saved by default
        fPersistent  = 0x100,   ///< Persistent -- saved when file is written
        fOverride    = 0x2,     ///< Existing value can be overriden
        fNoOverride  = 0x200,   ///< Cannot change existing value
        fTruncate    = 0x4,     ///< Leading, trailing blanks can be truncated
        fNoTruncate  = 0x400,   ///< Cannot truncate parameter value
        fJustCore    = 0x8,     ///< Ignore auxiliary subregistries
        fNotJustCore = 0x800,   ///< Include auxiliary subregistries
        fAllLayers   = fTransient | fPersistent | fNotJustCore
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
    ///   GetString()
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
    ///   Get()
    const string& GetString(const string& section,
                            const string& name,
                            const string& default_value,
                            TFlags        flags = 0) const;

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
    /// list in order.  Previous contents of the list are erased.  Enumerates
    /// sections rather than entries if section is empty.
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
    /// @sa
    ///   Write()
    void Read(CNcbiIstream& is, TFlags flags = 0);

    /// Set the configuration parameter value.
    ///
    /// Unset the parameter if specified "value" is empty.
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


protected:
    /// Called locked, like the virtual methods inherited from IRegistry.
    virtual void x_Clear(TFlags flags) = 0;
    virtual bool x_Set(const string& section, const string& name,
                       const string& value, TFlags flags,
                       const string& comment) = 0;
    virtual bool x_SetComment(const string& comment, const string& section,
                              const string& name, TFlags flags) = 0;

    /// Most implementations should not override this, but
    /// CNcbiRegistry must, to handle some special cases properly.
    virtual void x_Read(CNcbiIstream& is, TFlags flags);

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
    bool x_SetComment(const string& comment, const string& section,
                      const string& name, TFlags flags);

public: // WorkShop needs these exposed
    struct SEntry {
        string value, comment;
    };
    typedef map<string, SEntry, PNocase> TEntries;
    struct SSection {
        string   comment;
        TEntries entries;
    };
    typedef map<string, SSection, PNocase> TSections;

private:
    bool      m_IsModified;
    string    m_RegistryComment;
    TSections m_Sections;
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
    CCompoundRegistry(void) : m_CoreCutoff(ePriority_Default) { }

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
    TPriority GetCoreCutoff(void)           { return m_CoreCutoff; }
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

    friend class CNcbiRegistry;
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
    CTwoLayerRegistry(IRWRegistry* persistent = 0);

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
    bool x_SetComment(const string& comment, const string& section,
                      const string& name, TFlags flags);

private:
    typedef CRef<IRWRegistry> CRegRef;
    CRegRef m_Transient;
    CRegRef m_Persistent;
};


class CEnvironmentRegistry; // see <corelib/env_reg.hpp>



/////////////////////////////////////////////////////////////////////////////
///
/// CNcbiRegistry --
///
/// Define the Registry.
///
/// Load, access, modify and store runtime information (usually used
/// to work with configuration files).  Consists of a compound
/// registry whose top layer is a two-layer registry; all writes go to
/// the two-layer registry.

class NCBI_XNCBI_EXPORT CNcbiRegistry : public IRWRegistry
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
    CNcbiRegistry(void);

    /// Constructor.
    ///
    /// @param is
    ///   Input stream to load the Registry from.
    ///   NOTE:  if the stream is a file, it must be opened in binary mode!
    /// @param flags
    ///   How parameters are stored. The default is to store all parameters as
    ///   persistent unless the  "eTransient" flag is set in which case the
    ///   newly retrieved parameters are stored as transient.
    /// @sa
    ///   Read()
    CNcbiRegistry(CNcbiIstream& is, TFlags flags = 0);

    ~CNcbiRegistry();

    // The below interfaces provide access to the embedded compound registry.

    /// Priority for sub-registries; entries in higher-priority
    /// sub-registries take precedence over (identically named) entries
    /// in lower-priority ones.  Ties are broken arbitrarily.
    enum EPriority {
        ePriority_MinUser  = CCompoundRegistry::ePriority_Min,
        ePriority_Default  = CCompoundRegistry::ePriority_Default,
        ePriority_MaxUser  = CCompoundRegistry::ePriority_Max - 100,
        ePriority_Reserved ///< Everything greater is for internal use.
    };
    typedef int TPriority; ///< Not restricted to ePriority_*.

    /// Subregistries whose priority is less than the core cutoff
    /// (ePriority_Reserved by default) will be ignored for fJustCore
    /// operations, such as Write by default.
    TPriority GetCoreCutoff(void);
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

    /// Predefined subregistries' names.
    static const char* sm_MainRegName;
    static const char* sm_EnvRegName;
    static const char* sm_FileRegName;

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
    bool x_SetComment(const string& comment, const string& section,
                      const string& name, TFlags flags);
    void x_Read(CNcbiIstream& is, TFlags flags);

private:
    void x_Init(void);

    enum EReservedPriority {
        ePriority_File = ePriority_Reserved,
        ePriority_Environment,
        ePriority_Main
    };

    typedef map<string, TFlags> TClearedEntries;

    TClearedEntries            m_ClearedEntries;
    CRef<CTwoLayerRegistry>    m_MainRegistry;
    CRef<CEnvironmentRegistry> m_EnvRegistry;
    CRef<CTwoLayerRegistry>    m_FileRegistry;
    CRef<CCompoundRegistry>    m_AllRegistries;
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

class CRegistryException : public CParseTemplException<CCoreException>
{
public:
    /// Error types that the Registry can generate.
    enum EErrCode {
        eSection,   ///< Section error
        eEntry,     ///< Entry error
        eValue,     ///< Value error
        eErr        ///< Other error
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eSection: return "eSection";
        case eEntry:   return "eEntry";
        case eValue:   return "eValue";
        case eErr:     return "eErr";
        default:       return CException::GetErrCodeString();
        }
    }

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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.37  2005/03/14 15:52:09  ucko
 * Support taking settings from the environment.
 *
 * Revision 1.36  2004/12/21 16:20:58  ucko
 * IRegistry: comment out trivial x_* methods' unused arguments' names.
 *
 * Revision 1.35  2004/12/20 17:33:52  ucko
 * Expose CMemoryRegistry's internal types to make WorkShop happy.
 *
 * Revision 1.34  2004/12/20 15:28:26  ucko
 * Extensively refactor, and add support for subregistries.
 *
 * Revision 1.33  2004/08/19 13:01:51  dicuccio
 * Dropped unnecessary export specifier on exceptions
 *
 * Revision 1.32  2003/10/20 21:55:05  vakatov
 * CNcbiRegistry::GetComment() -- make it "const"
 *
 * Revision 1.31  2003/08/18 18:44:07  siyan
 * Minor comment changes.
 *
 * Revision 1.30  2003/08/14 12:25:28  siyan
 * Made previous documentation changes consistent.
 * Best not to mix the ///< style with @param style for parameter documentation
 * as Doxygen may not always render this correctly.
 *
 * Revision 1.29  2003/08/12 19:00:39  vakatov
 * Fixed comments and code layout
 *
 * Revision 1.27  2003/07/21 18:42:38  siyan
 * Documentation changes.
 *
 * Revision 1.26  2003/04/07 19:40:03  ivanov
 * Rollback to R1.24
 *
 * Revision 1.25  2003/04/07 16:08:41  ivanov
 * Added more thread-safety to CNcbiRegistry:: methods -- mutex protection.
 * Get() and GetComment() returns "string", not "string&".
 *
 * Revision 1.24  2003/04/01 14:20:28  siyan
 * Added doxygen support
 *
 * Revision 1.23  2003/02/28 19:24:42  vakatov
 * Get rid of redundant "const" in the return type of GetInt/Bool/Double()
 *
 * Revision 1.22  2003/02/24 19:54:51  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 1.21  2003/01/17 20:46:28  vakatov
 * Fixed/improved description of "EErrAction"
 *
 * Revision 1.20  2003/01/17 20:26:59  kuznets
 * CNcbiRegistry added ErrPost error action
 *
 * Revision 1.19  2003/01/17 17:31:20  vakatov
 * CNcbiRegistry::GetString() to return "string", not "string&" -- for safety
 *
 * Revision 1.18  2002/12/30 23:23:06  vakatov
 * + GetString(), GetInt(), GetBool(), GetDouble() -- with defaults,
 * conversions and error handling control (to extend Get()'s functionality).
 *
 * Revision 1.17  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.16  2002/04/11 20:39:18  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.15  2001/09/11 00:46:56  vakatov
 * Fixes to R1.14:
 *   Renamed HasChanged() to Modified(), refined and extended its functionality
 *   Made Write() be "const" again
 *
 * Revision 1.14  2001/09/10 16:35:02  ivanov
 * Added method HasChanged()
 *
 * Revision 1.13  2001/06/22 21:50:20  ivanov
 * Added (with Denis Vakatov) ability for read/write the registry file
 * with comments. Also added functions GetComment() and SetComment().
 *
 * Revision 1.12  2001/05/17 14:54:01  lavr
 * Typos corrected
 *
 * Revision 1.11  2001/04/09 17:39:20  grichenk
 * CNcbiRegistry::Get() return type reverted to "const string&"
 *
 * Revision 1.10  2001/04/06 15:46:29  grichenk
 * Added thread-safety to CNcbiRegistry:: methods
 *
 * Revision 1.9  1999/09/02 21:53:23  vakatov
 * Allow '-' and '.' in the section/entry name
 *
 * Revision 1.8  1999/07/07 14:17:05  vakatov
 * CNcbiRegistry::  made the section and entry names be case-insensitive
 *
 * Revision 1.7  1999/07/06 15:26:31  vakatov
 * CNcbiRegistry::
 *   - allow multi-line values
 *   - allow values starting and ending with space symbols
 *   - introduced EFlags/TFlags for optional parameters in the class
 *     member functions -- rather than former numerous boolean parameters
 *
 * Revision 1.6  1998/12/28 17:56:28  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.5  1998/12/10 22:59:46  vakatov
 * CNcbiRegistry:: API is ready(and by-and-large tested)
 * ===========================================================================
 */

#endif  /* CORELIB___NCBIREG__HPP */
