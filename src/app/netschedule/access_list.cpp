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


bool CNetSchedule_AccessList::IsRestrictionSet() const
{
    CReadLockGuard guard(m_Lock);
    return x_IsRestrictionSet();
}


/// is host allowed to connect
bool CNetSchedule_AccessList::IsAllowed(unsigned ha) const
{
    CReadLockGuard guard(m_Lock);

    if (!x_IsRestrictionSet())
        return true;

    return m_Hosts[ha];
}


/// Delimited lists of hosts allowed into the system
void CNetSchedule_AccessList::SetHosts(const string& host_names)
{
    vector<string>      hosts;
    NStr::Tokenize(host_names, ";, \n\r", hosts, NStr::eMergeDelims);

    CWriteLockGuard guard(m_Lock);
    m_Hosts.clear();

    ITERATE(vector<string>, it, hosts) {
        const string &      hn = *it;

        if (NStr::CompareNocase(hn, "localhost") == 0) {
            string          my_name = CSocketAPI::gethostname();
            unsigned int    ha = CSocketAPI::gethostbyname(my_name);

            if (ha != 0)
                m_Hosts.set(ha);
        }

        unsigned int        ha = CSocketAPI::gethostbyname(*it);
        if (ha != 0)
            m_Hosts.set(ha);
        else
            LOG_POST(Error << "'" << *it << "'"
                           << " is not a valid host name. Ignored.");
    }
}


void CNetSchedule_AccessList::PrintHosts(CNetScheduleHandler &  handler) const
{
    CReadLockGuard          guard(m_Lock);
    THostVector::enumerator en(m_Hosts.first());

    for(; en.valid(); ++en) {
        handler.WriteMessage("OK:", CSocketAPI::gethostbyaddr(*en));
    }
}


string CNetSchedule_AccessList::Print(const char* sep) const
{
    string      s;
    for(THostVector::enumerator en(m_Hosts.first()); en.valid(); ++en) {
        if (s.size()) s += sep;
        s += CSocketAPI::gethostbyaddr(*en);
    }
    return s;
}


END_NCBI_SCOPE

