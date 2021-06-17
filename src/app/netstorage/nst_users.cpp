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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   NetStorage users facilities
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/impl/netstorage_impl.hpp>
#include "nst_users.hpp"
#include "nst_exception.hpp"


USING_NCBI_SCOPE;




void CNSTUserID::Validate(void)
{
    // Note:
    //  both fields could be empty. This is a 'special case': when the
    //  user namespace and user name were not provided at the time of creation


    // The following will throw an exception if the limit is broken
    SNetStorage::SLimits::
                    Check<SNetStorage::SLimits::SUserName>(name);
    SNetStorage::SLimits::
                    Check<SNetStorage::SLimits::SUserNamespace>(name_space);
}


CNSTUserCache::CNSTUserCache()
{}


Int8  CNSTUserCache::GetDBUserID(const CNSTUserID &  user) const
{
    CMutexGuard                 guard(m_Lock);
    TUsers::const_iterator      found = m_Users.find(user);
    if (found != m_Users.end())
        return found->second.GetDBUserID();
    return k_UndefinedUserID;
}


void  CNSTUserCache::SetDBUserID(const CNSTUserID &  user, Int8  id)
{
    CMutexGuard             guard(m_Lock);
    TUsers::iterator        found = m_Users.find(user);
    if (found != m_Users.end())
        found->second.SetDBUserID(id);
    else {
        CNSTUserData    user_data;
        user_data.SetDBUserID(id);
        m_Users[user] = user_data;
    }
}


size_t  CNSTUserCache::Size(void) const
{
    CMutexGuard     guard(m_Lock);
    return m_Users.size();
}

