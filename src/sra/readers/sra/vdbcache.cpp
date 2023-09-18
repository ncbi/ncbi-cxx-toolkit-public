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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to SRA files
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/sra/vdbcache.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);


CVDBCacheWithExpiration::CVDBCacheWithExpiration(size_t size_limit,
                                         unsigned force_reopen_seconds,
                                         unsigned rechech_seconds)
    : m_CacheMap(size_limit),
      m_ForceReopenSeconds(force_reopen_seconds),
      m_RecheckSeconds(rechech_seconds)
{
}


CVDBCacheWithExpiration::~CVDBCacheWithExpiration()
{
}


void CVDBCacheWithExpiration::set_size_limit(size_t limit)
{
    CMutexGuard guard(GetCacheMutex());
    m_CacheMap.set_size_limit(limit);
}


void CVDBCacheWithExpiration::SetForceReopenSeconds(unsigned seconds)
{
    m_ForceReopenSeconds = seconds;
}


void CVDBCacheWithExpiration::SetRecheckSeconds(unsigned seconds)
{
    m_RecheckSeconds = seconds;
}


CVDBCacheWithExpiration::CSlot::CSlot()
{
}


CVDBCacheWithExpiration::CSlot::~CSlot()
{
}


bool CVDBCacheWithExpiration::CSlot::IsExpired(const CVDBCacheWithExpiration& cache,
                                               const string& acc_or_path) const
{
    return !m_ExpirationInfo || m_ExpirationInfo->IsExpired(cache, acc_or_path);
}


void CVDBCacheWithExpiration::CSlot::UpdateExpiration(const CVDBCacheWithExpiration& cache,
                                                      const string& acc_or_path)
{
    m_ExpirationInfo = new CExpirationInfo(cache, acc_or_path);
}


CVDBCacheWithExpiration::CExpirationInfo::CExpirationInfo(const CVDBCacheWithExpiration& cache,
                                                          const string& acc_or_path)
    : m_ForceReopenDeadline(cache.m_ForceReopenSeconds),
      m_RecheckDeadline(cache.m_RecheckSeconds),
      m_DereferencedPath(DereferncePath(cache.m_Mgr, acc_or_path)),
      m_Timestamp(GetTimestamp(cache.m_Mgr, m_DereferencedPath))
{
    //LOG_POST("CVDBCacheWithExpiration: "<<acc_or_path<<" info: "
    //         << m_DereferencedPath << ", " << m_Timestamp);
}


CVDBCacheWithExpiration::CExpirationInfo::~CExpirationInfo()
{
}


string CVDBCacheWithExpiration::CExpirationInfo::DereferncePath(const CVDBMgr& mgr,
                                                                const string& acc_or_path)
{
    string path;
    try {
        path = mgr.FindDereferencedAccPath(acc_or_path);
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFoundDb ||
             exc.GetErrCode() == exc.eProtectedDb ) {
            // no such SNP NA accession, return empty string
            return string();
        }
    }
    catch ( exception& /*ignored*/ ) {
    }
    // in case of error assume original accession or path
    return path.empty()? acc_or_path: path;
}


CTime CVDBCacheWithExpiration::CExpirationInfo::GetTimestamp(const CVDBMgr& mgr,
                                                             const string& path)
{
    if ( !path.empty() ) {
        try {
            return mgr.GetTimestamp(path);
        }
        catch ( exception& /*ignored*/ ) {
        }
    }
    // in case of error return empty timestamp
    return CTime();
}


bool CVDBCacheWithExpiration::CExpirationInfo::IsExpired(const CVDBCacheWithExpiration& cache,
                                                         const string& acc_or_path) const
{
    if ( m_ForceReopenDeadline.IsExpired() ) {
        // forced expiration
        //LOG_POST("CVDBCacheWithExpiration: force reopen of "<<acc_or_path);
        return true;
    }
    if ( !m_RecheckDeadline.IsExpired() ) {
        // not re-checking yet
        return false;
    }
    // check if the file has changed
    string new_path = DereferncePath(cache.m_Mgr, acc_or_path);
    if ( new_path != m_DereferencedPath ) {
        // actual file path has changed
        //LOG_POST("CVDBCacheWithExpiration: path of "<<acc_or_path<<" has changed: "
        //         << m_DereferencedPath << " -> " << new_path);
        return true;
    }
    CTime new_timestamp = GetTimestamp(cache.m_Mgr, new_path);
    if ( new_timestamp != m_Timestamp ) {
        // file timestamp has changed
        //LOG_POST("CVDBCacheWithExpiration: timestamp of "<<acc_or_path<<" has changed: "
        //         << m_Timestamp << " -> " << new_timestamp);
        return true;
    }
    // everything is the same, set deadline for the next re-check
    //LOG_POST("CVDBCacheWithExpiration: "<<acc_or_path<<" is the same: "
    //         << m_DereferencedPath << ", " << m_Timestamp);
    m_RecheckDeadline = CDeadline(cache.m_RecheckSeconds);
    return false;
}


CRef<CVDBCacheWithExpiration::CSlot> CVDBCacheWithExpiration::GetSlot(const string& acc_or_path)
{
    CMutexGuard guard(GetCacheMutex());
    CRef<CSlot>& slot = m_CacheMap[acc_or_path];
    if ( !slot ) {
        slot = new CSlot();
    }
    return slot;
}
                  

END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
