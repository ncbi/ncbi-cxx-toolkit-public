#ifndef CONNECT_SERVICES__NETCACHE_NSSTORAGE_IMP__HPP
#define CONNECT_SERVICES__NETCACHE_NSSTORAGE_IMP__HPP

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
 * Authors:  Maxim Didneko
 *
 */
#include <corelib/ncbimisc.hpp>
#include <connect/services/netschedule_storage.hpp>
#include <connect/services/netcache_client.hpp>
#include <util/rwstream.hpp>

BEGIN_NCBI_SCOPE

class NCBI_XCONNECT_EXPORT CNetCacheNSStorage : public INetScheduleStorage
{
public:
    CNetCacheNSStorage(auto_ptr<CNetCacheClient> nc_client);

    virtual CNcbiIstream& GetIStream(const string& data_id,
                                     size_t* blob_size = 0);
    virtual CNcbiOstream& CreateOStream(string& data_id);
    virtual void Reset();

private:
    auto_ptr<CNetCacheClient> m_NCClient;
    auto_ptr<CRStream>        m_IStream;
    auto_ptr<CWStream>        m_OStream;
};

class CNetCacheNSStorageException : public CNetScheduleStorageException
{
public:
    enum EErrCode {
        eBlobNotFound
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eBlobNotFound: return "eBlobNotFoundError";
        default:      return CNetScheduleStorageException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetCacheNSStorageException, CNetScheduleStorageException);
};

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/03/22 20:17:55  didenko
 * Initial version
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES__NETCACHE_NSSTORAGE_IMP__HPP
