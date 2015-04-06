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
 * Authors:  Victor Joukov
 *
 * File Description:
 *   NetSchedule access list.
 */
#include <ncbi_pch.hpp>

#include "access_list.hpp"
#include "ns_handler.hpp"

#include <connect/ncbi_socket.hpp>

BEGIN_NCBI_SCOPE


// is host allowed to connect
bool CNetScheduleAccessList::IsAllowed(unsigned ha) const
{
    CReadLockGuard guard(m_Lock);

    if (m_Hosts.count() == 0)
        return true;

    return m_Hosts[ha];
}


// Delimited lists of hosts allowed into the system
CJsonNode CNetScheduleAccessList::SetHosts(const string &  host_names)
{
    CJsonNode           diff = CJsonNode::NewArrayNode();
    vector<string>      hosts;
    NStr::Tokenize(host_names, ";, \n\r", hosts, NStr::eMergeDelims);

    CWriteLockGuard guard(m_Lock);
    TNSBitVector        old_hosts = m_Hosts;
    vector<string>      old_as_from_config = m_AsFromConfig;

    m_Hosts.clear();
    m_AsFromConfig.clear();

    ITERATE(vector<string>, it, hosts) {
        const string &      hn = *it;

        if (NStr::CompareNocase(hn, "localhost") == 0) {
            string          my_name = CSocketAPI::gethostname();
            unsigned int    ha = CSocketAPI::gethostbyname(my_name);

            if (ha != 0) {
                m_Hosts.set_bit(ha, true);
                m_AsFromConfig.push_back(hn);
                continue;
            }
        }

        unsigned int        ha = CSocketAPI::gethostbyname(hn);
        if (ha != 0) {
            m_Hosts.set_bit(ha, true);
            m_AsFromConfig.push_back(hn);
        }
        else
            ERR_POST("'" << hn << "' is not a valid host name. Ignored.");
    }

    if (old_hosts != m_Hosts) {
        CJsonNode       old_vals = CJsonNode::NewArrayNode();
        CJsonNode       new_vals = CJsonNode::NewArrayNode();

        for (vector<string>::const_iterator  k = old_as_from_config.begin();
                k != old_as_from_config.end(); ++k)
            old_vals.AppendString(*k);
        for (vector<string>::const_iterator  k = m_AsFromConfig.begin();
                k != m_AsFromConfig.end(); ++k)
            new_vals.AppendString(*k);

        diff.Append(old_vals);
        diff.Append(new_vals);
    }
    return diff;
}


// Used to print as a comma separated string - part of output
// or as output in the socket with leading 'OK:'
string CNetScheduleAccessList::Print(const string &  prefix,
                                     const string &  separator) const
{
    string                      s;
    CReadLockGuard              guard(m_Lock);
    TNSBitVector::enumerator    en(m_Hosts.first());

    for(; en.valid(); ++en) {
        if (!s.empty())
            s += separator;
        s += prefix + CSocketAPI::gethostbyaddr(*en);
    }
    return s;
}


string CNetScheduleAccessList::GetAsFromConfig(void) const
{
    string                      ret;
    CReadLockGuard              guard(m_Lock);

    for (vector<string>::const_iterator  k = m_AsFromConfig.begin();
            k != m_AsFromConfig.end(); ++k) {
        if (!ret.empty())
            ret += ", ";
        ret += *k;
    }
    return ret;
}


END_NCBI_SCOPE

