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
 * Author: Andrei Gourianov
 *
 */

#ifndef __PTB_REGISTRY__
#define __PTB_REGISTRY__

#include <corelib/ncbireg.hpp>
#include <map>
   
BEGIN_NCBI_SCOPE


class CPtbRegistry
{
public:
    CPtbRegistry(void);
    CPtbRegistry(const IRWRegistry& reg);
    ~CPtbRegistry(void);

    string GetString(const string& section,
                     const string& name,
                     const string& default_value = kEmptyStr) const;

    string Get(const string& section,
               const string& name) const
    {
        return m_IsEmpty ? kEmptyStr : GetString(section,name);
    }
    bool HasEntry(const string& section, const string& name = kEmptyStr) const
    {
        return m_IsEmpty ? false : m_Registry->HasEntry(section, name, IRegistry::fCountCleared);
    }
    void Read(CNcbiIstream& is)
    {
        m_Registry->Read(is);
        m_IsEmpty = m_Registry->Empty();
    }
    bool Empty(void) const
    {
        return m_IsEmpty;
    }
    void EnumerateSections(list<string>* sections) const
    {
        if (!m_IsEmpty) {m_Registry->EnumerateSections(sections);}
    }

    void EnumerateEntries(const string& section,
                          list<string>* entries) const
    {
        if (!m_IsEmpty) {m_Registry->EnumerateEntries(section,entries);}
    }

private:
    mutable map<string,string> m_Cache;
    AutoPtr<IRWRegistry> m_Registry;
    bool m_IsEmpty;

    /// forbidden
    CPtbRegistry(const CPtbRegistry&);
    CPtbRegistry& operator=(const CPtbRegistry&);
};

END_NCBI_SCOPE

#endif // __PTB_REGISTRY__
