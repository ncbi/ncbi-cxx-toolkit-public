#ifndef CORELIB___ENV_REG__HPP
#define CORELIB___ENV_REG__HPP

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

/// @file env_reg.hpp
/// Classes to support using environment variables as a backend for
/// the registry framework.

#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>


/** @addtogroup Registry
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// IEnvRegMapper --
///
/// Abstract policy class mediating conversions between environment
/// variable names and registry entry names.

class NCBI_XNCBI_EXPORT IEnvRegMapper : public CObject
{
public:
    /// Returns empty strings for unsupported (section, name) pairs.
    virtual string RegToEnv(const string& section, const string& name) const
        = 0;

    /// The return value indicates whether the environment variable was
    /// appropriately formatted.
    virtual bool   EnvToReg(const string& env, string& section, string& name)
        const = 0;

    /// Can be overriden to speed enumeration.
    virtual string GetPrefix(void) const { return kEmptyStr; }
};


/////////////////////////////////////////////////////////////////////////////
///
/// CEnvironmentRegistry --
///
/// Adapts CNcbiEnvironment to act like a registry.  Leaves all
/// caching of values to CNcbiEnvironment, and does not support
/// comments.  Always transient, though making it the persistent side
/// of a CTwoLayerRegistry can mask that.
///
/// Uses customizable mappers between environment variable names and
/// registry section/name pairs; see below for details.


class NCBI_XNCBI_EXPORT CEnvironmentRegistry : public IRWRegistry
{
public:
    /// Constructors.
    CEnvironmentRegistry(TFlags flags = 0);
    CEnvironmentRegistry(CNcbiEnvironment& env, EOwnership own = eNoOwnership,
                         TFlags flags = 0);

    /// Destructor.
    ~CEnvironmentRegistry();

    enum EPriority {
        ePriority_Min     = kMin_Int,
        ePriority_Default = 0,
        ePriority_Max     = kMax_Int
    };
    typedef int TPriority; ///< Not restricted to ePriority_*.

    void AddMapper(const IEnvRegMapper& mapper,
                   TPriority            prio = ePriority_Default);
    void RemoveMapper(const IEnvRegMapper& mapper);

protected:
    bool x_Empty(TFlags flags) const;
    bool x_Modified(TFlags flags) const;
    void x_SetModifiedFlag(bool modified, TFlags flags);
    const string& x_Get(const string& section, const string& name,
                        TFlags flags) const;
    const string& x_Get(const string& section, const string& name,
                        TFlags flags, bool& found) const;
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
    /// Copying prohibited.
    CEnvironmentRegistry(const CEnvironmentRegistry&) {}

    typedef multimap<TPriority, CConstRef<IEnvRegMapper> > TPriorityMap;

    AutoPtr<CNcbiEnvironment> m_Env;
    TPriorityMap      m_PriorityMap;
    bool              m_Modified; ///< only tracks mods made through this.
    TFlags            m_Flags;
};


/// CSimpleEnvRegMapper --
///
/// Treat environment variables named <prefix><name><suffix> as
/// registry entries with section <section> and key <section>.  Empty
/// prefixes are legal, but must be specified explicitly.  Each
/// section name is limited to a single prefix/suffix pair; however,
/// there are no obstacles to placing multiple such mappings in a
/// CEnvironmentRegistry.
///
/// Not used in the default configuration.

class NCBI_XNCBI_EXPORT CSimpleEnvRegMapper : public IEnvRegMapper
{
public:
    CSimpleEnvRegMapper(const string& section, const string& prefix,
                        const string& suffix = kEmptyStr);

    string RegToEnv (const string& section, const string& name) const;
    bool   EnvToReg (const string& env, string& section, string& name) const;
    string GetPrefix(void) const;
private:
    string m_Section, m_Prefix, m_Suffix;
};


/// CNcbiEnvRegMapper --
///
/// Somewhat more elaborate mapping used by default, with support for
/// tree conversion.  Special node names (starting with dots) get mapped as
///     [<section>].<name> <-> NCBI_CONFIG_<name>__<section> ;
/// all other names get mapped as
///     [<section>]<name> <-> NCBI_CONFIG__<section>__<name>
/// with _DOT_ corresponding to internal periods.

class NCBI_XNCBI_EXPORT CNcbiEnvRegMapper : public IEnvRegMapper
{
public:
    string RegToEnv (const string& section, const string& name) const;
    bool   EnvToReg (const string& env, string& section, string& name) const;
    string GetPrefix(void) const;

private:
    static const char* sm_Prefix;
};


END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB___ENV_REG__HPP */
