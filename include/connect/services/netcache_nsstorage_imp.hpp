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
 * Authors:  Maxim Didenko
 *
 */
#include <corelib/ncbimisc.hpp>
#include <connect/services/netschedule_storage.hpp>
#include <connect/services/netcache_client.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */


/// NetCache based job info storage for grid worker node
///
class NCBI_XCONNECT_EXPORT CNetCacheNSStorage : public INetScheduleStorage
{
public:
    CNetCacheNSStorage(CNetCacheClient* nc_client);
    virtual ~CNetCacheNSStorage(); 

    virtual string        GetBlobAsString( const string& data_id);

    virtual CNcbiIstream& GetIStream(const string& data_id,
                                     size_t* blob_size = 0);
    virtual CNcbiOstream& CreateOStream(string& data_id);

    virtual string CreateEmptyBlob();
    virtual void DeleteBlob(const string& data_id);
    
    virtual void Reset();

private:
    auto_ptr<CNetCacheClient> m_NCClient;
    auto_ptr<CNcbiIstream>    m_IStream;
    auto_ptr<CNcbiOstream>    m_OStream;

    auto_ptr<IReader> x_GetReader(const string& key,
                                  size_t& blob_size);
    void x_Check();
};

class CNetCacheNSStorageException : public CNetScheduleStorageException
{
public:
    enum EErrCode {
        eBlobNotFound,
        eBusy
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eBlobNotFound: return "eBlobNotFoundError";
        case eBusy:         return "eBusy";
        default:      return CNetScheduleStorageException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetCacheNSStorageException, CNetScheduleStorageException);
};

/* @} */

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/04/20 19:23:47  didenko
 * Added GetBlobAsString, GreateEmptyBlob methods
 * Remave RemoveData to DeleteBlob
 *
 * Revision 1.7  2005/04/12 15:11:12  didenko
 * Changed CRStream and CWStream to CNcbiIstream and CNcbiOstream
 *
 * Revision 1.6  2005/03/29 14:10:16  didenko
 * + removing a date from the storage
 *
 * Revision 1.5  2005/03/28 14:38:04  didenko
 * Cosmetics
 *
 * Revision 1.4  2005/03/23 13:10:32  kuznets
 * documented and doxygenized
 *
 * Revision 1.3  2005/03/22 21:42:50  didenko
 * Got rid of warnning on Sun WorkShop
 *
 * Revision 1.2  2005/03/22 20:35:56  didenko
 * Make it compile under CGG
 *
 * Revision 1.1  2005/03/22 20:17:55  didenko
 * Initial version
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES__NETCACHE_NSSTORAGE_IMP__HPP
