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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko, Anatoliy Kuznetsov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/services/netcache_nsstorage_imp.hpp>


BEGIN_NCBI_SCOPE


CNetCacheNSStorage::CNetCacheNSStorage(CNetCacheClient* nc_client)
: m_NCClient(nc_client)
{
}


CNetCacheNSStorage::~CNetCacheNSStorage() 
{
}

CNcbiIstream& CNetCacheNSStorage::GetIStream(const string& key,
                                             size_t* blob_size)
{
    size_t b_size = 0;
    auto_ptr<IReader> reader;
    int try_count = 0;
    while(1) {
        try {
            reader.reset(m_NCClient->GetData(key, &b_size));
            break;
        }
        catch (CNetServiceException& ex) {
            LOG_POST(Error << "Communication Error : " 
                            << ex.what());
            if (++try_count >= 2)
                throw;
            SleepMilliSec(1000 + try_count*2000);
        }
    }

    if (blob_size) *blob_size = b_size;
    if (!reader.get()) {
        NCBI_THROW(CNetCacheNSStorageException,
                   eBlobNotFound, "Requested blob is not found.");
        //return *(CNcbiIstream*)NULL;
    }

    m_IStream.reset(new CRStream(reader.release(), 0,0, 
                                 CRWStreambuf::fOwnReader));
    return *m_IStream;
}


CNcbiOstream& CNetCacheNSStorage::CreateOStream(string& key)
{
    auto_ptr<IWriter> writer;
    int try_count = 0;
    while(1) {
        try {
            writer.reset(m_NCClient->PutData(&key));
            break;
        }
        catch (CNetServiceException& ex) {
            LOG_POST(Error << "Communication Error : " 
                            << ex.what());
            if (++try_count >= 2)
                throw;
            SleepMilliSec(1000 + try_count*2000);
        }
    }
    if (!writer.get()) {
        NCBI_THROW(CNetScheduleStorageException,
                   eWriter, "Writer couldn't be created.");
        //return *(CNcbiOstream*)NULL;
    }
    m_OStream.reset( new CWStream(writer.release(), 0,0, 
                                  CRWStreambuf::fOwnWriter));
    return *m_OStream;
}

void CNetCacheNSStorage::Reset()
{
    m_IStream.reset();
    m_OStream.reset();
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/03/28 14:40:39  didenko
 * Cosmetics
 *
 * Revision 1.2  2005/03/22 20:45:13  didenko
 * Got ride from warning on ICC
 *
 * Revision 1.1  2005/03/22 20:18:25  didenko
 * Initial version
 *
 * ===========================================================================
 */
 
