/* $Id$
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
 * Author:  Andrei Gourianov
 *
 */

#include <ncbi_pch.hpp>
#include "ptb_registry.hpp"

BEGIN_NCBI_SCOPE

CPtbRegistry::CPtbRegistry(void)
    : m_IsEmpty(true)
{
    m_Registry.reset(new CMemoryRegistry);
}

CPtbRegistry::CPtbRegistry(const IRWRegistry& reg)
    : m_Registry(const_cast<IRWRegistry*>(&reg), eNoOwnership)
{
    m_IsEmpty = reg.Empty();
}

CPtbRegistry::~CPtbRegistry(void)
{
}

string CPtbRegistry::GetString(const string& section,
                               const string& name,
                               const string& default_value) const
{
    if (m_IsEmpty) {return default_value;}
    string key(section+name);
    map<string,string>::const_iterator i = m_Cache.find(key);
    if (i != m_Cache.end()) {
        return i->second;
    }
    return m_Cache[key] = m_Registry->GetString(section,name,default_value);
}

END_NCBI_SCOPE
