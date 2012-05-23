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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description:
 *   NetSchedule access list.
 *
 */

/// @file access_list.hpp
/// NetSchedule access lists, configures server-client access rights
///
/// @internal

#include <corelib/ncbidbg.hpp>
#include "ns_types.hpp"
#include <corelib/ncbimtx.hpp>


BEGIN_NCBI_SCOPE


/// List of hosts allowed
///
/// @internal
class CNetScheduleAccessList
{
public:
    CNetScheduleAccessList()
    {}

    /// is host allowed to connect
    bool IsAllowed(unsigned int  ha) const;

    /// Delimited lists of hosts allowed into the system
    void SetHosts(const string &  host_names);

    string Print(const string &  prefix,
                 const string &  separator) const;

private:
    mutable CRWLock             m_Lock;
    TNSBitVector                m_Hosts;
};


END_NCBI_SCOPE

#endif

