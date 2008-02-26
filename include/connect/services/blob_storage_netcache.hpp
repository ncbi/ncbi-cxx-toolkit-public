#ifndef CONNECT_SERVICES__BLOB_STORAGE_NETCACHE_HPP
#define CONNECT_SERVICES__BLOB_STORAGE_NETCACHE_HPP

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

#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/blob_storage.hpp>
#include <connect/services/netcache_api.hpp>
#include <connect/services/netcache_client.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */

///////////////////////////////////////////////////////////////////////////////
///
/// CBlobStorage_NetCache ---
/// Implementaion of IBlobStorage interface based on NetCache service
///
class NCBI_BLOBSTORAGE_NETCACHE_EXPORT CBlobStorage_NetCache : public IBlobStorage
{
public:

    /// Specifies if blobs should be cached on a local fs
    enum ECacheFlags {
        eCacheInput = 0x1,  ///< Cache input streams
        eCacheOutput = 0x2, ///< Cache ouput streams
        eCacheBoth = eCacheInput | eCacheOutput
    };
    typedef unsigned int TCacheFlags;

    CBlobStorage_NetCache();

    /// Create Blob Storage
    /// @param[in] nc_client
    ///  NetCache client. Session Storage will delete it when
    ///  it goes out of scope.
    /// @param[in] flags
    ///  Specifies if blobs should be cached on a local fs
    ///  before they are accessed for read/write.
    /// @param[in[ temp_dir
    ///  Specifies where on a local fs those blobs will be cached
    CBlobStorage_NetCache(CNetCacheAPI* nc_client,
                          TCacheFlags flags = 0x0,
                          const string& temp_dir = ".");
    CBlobStorage_NetCache(CNetCacheClient* nc_client,
                          TCacheFlags flags = 0x0,
                          const string& temp_dir = ".");

    virtual ~CBlobStorage_NetCache();


    virtual bool IsKeyValid(const string& str);

    /// Get a blob content as a string
    ///
    /// @param[in] blob_key
    ///    Blob key to read
    virtual string        GetBlobAsString(const string& data_id);

    /// Get an input stream to a blob
    ///
    /// @param[in] blob_key
    ///    Blob key to read
    /// @param[out] blob_size
    ///    if blob_size if not NULL the size of a blob is retured
    /// @param[in] lock_mode
    ///    Blob locking mode
    virtual CNcbiIstream& GetIStream(const string& data_id,
                                     size_t* blob_size = 0,
                                     ELockMode lock_mode = eLockWait);

    /// Get an output stream to a blob
    ///
    /// @param[in,out] blob_key
    ///    Blob key to read. If a blob with a given key does not exist
    ///    an key of a newly create blob will be assigned to blob_key
    /// @param[in] lock_mode
    ///    Blob locking mode
    virtual CNcbiOstream& CreateOStream(string& data_id,
                                        ELockMode lock_mode = eLockNoWait);

    /// Create an new blob
    ///
    /// @return
    ///     Newly create blob key
    virtual string CreateEmptyBlob();

    /// Delete a blob
    ///
    /// @param[in] blob_key
    ///    Blob key to read
    virtual void DeleteBlob(const string& data_id);

    /// Close all streams and connections.
    virtual void Reset();

private:
    auto_ptr<IBlobStorage> m_Impl;

    CBlobStorage_NetCache(const CBlobStorage_NetCache&);
    CBlobStorage_NetCache& operator=(CBlobStorage_NetCache&);
};

extern NCBI_BLOBSTORAGE_NETCACHE_EXPORT const char* kBlobStorageNetCacheDriverName;

extern "C"
{

NCBI_BLOBSTORAGE_NETCACHE_EXPORT
void NCBI_EntryPoint_xblobstorage_netcache(
     CPluginManager<IBlobStorage>::TDriverInfoList&   info_list,
     CPluginManager<IBlobStorage>::EEntryPointRequest method);

NCBI_BLOBSTORAGE_NETCACHE_EXPORT
void BlobStorage_RegisterDriver_NetCache(void);

} // extern C


END_NCBI_SCOPE

#endif // CONNECT_SERVICES__BLOB_STORAGE_NETCACHE_HPP
