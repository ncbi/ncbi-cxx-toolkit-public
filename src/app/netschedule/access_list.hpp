#ifndef NETSCHEDULE_ACCESS_LIST__HPP
#define NETSCHEDULE_ACCESS_LIST__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:
 *   NetSchedule access list.
 *
 */

/// @file access_list.hpp
/// NetSchedule access lists, configures server-client access rights
///
/// @internal


#include <util/bitset/ncbi_bitset.hpp>
#include <corelib/ncbimtx.hpp>


BEGIN_NCBI_SCOPE

/// List of hosts allowed
///
/// @internal
class CNetSchedule_AccessList
{
public:
    CNetSchedule_AccessList()
    {
    }

    /// @return TRUE if restriction list set
    bool IsRestrictionSet() const
    {
        CReadLockGuard guard(m_Lock);
        return x_IsRestrictionSet();
    }

    /// is host allowed to connect
    bool IsAllowed(unsigned ha) const
    {
        CReadLockGuard guard(m_Lock);

        if (!x_IsRestrictionSet()) return true;

        return m_Hosts[ha];
    }

    /// Delimited lists of hosts allowed into the system
    void SetHosts(const string& host_names)
    {
        vector<string> hosts;
        NStr::Tokenize(host_names, ";, ", hosts, NStr::eMergeDelims);

        CWriteLockGuard guard(m_Lock);
        m_Hosts.clear();

        ITERATE(vector<string>, it, hosts) {
            unsigned int ha = CSocketAPI::gethostbyname(*it);
            if (ha != 0) {
                m_Hosts.set(ha);
            } else {
                LOG_POST(Error << "'" << *it << "'" 
                               << " is not a valid host name. Ignored.");
            }
        }        
    }

    void PrintHosts(CNcbiOstream & out) const
    {
        CReadLockGuard guard(m_Lock);

        THostVector::enumerator en(m_Hosts.first());
        for(; en.valid(); ++en) {
            unsigned ha = *en;
            string host = CSocketAPI::gethostbyaddr(ha);
            out << host << "\n";
        }
    }
    
private:
    bool x_IsRestrictionSet() const 
    {
        return m_Hosts.any();
    }
private:
    typedef bm::bvector<>       THostVector;

private:
    mutable CRWLock             m_Lock;
    THostVector                 m_Hosts;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/06/27 15:53:03  kuznets
 * Print host name not the address
 *
 * Revision 1.1  2005/06/20 13:31:08  kuznets
 * Added access control for job submitters and worker nodes
 *
 *
 * ===========================================================================
 */

#endif 

