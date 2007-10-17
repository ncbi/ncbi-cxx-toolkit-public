#ifndef __BLOB_STORAGE_NETCACHE_IMPL__HPP
#define __BLOB_STORAGE_NETCACHE_IMPL__HPP

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
#include <connect/services/error_codes.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */

///////////////////////////////////////////////////////////////////////////////
///
/// CBlobStorage_NetCache_Impl ---
/// Implementaion of IBlobStorage interface based on NetCache service
//
template <class T>
class CBlobStorage_NetCache_Impl : public IBlobStorage
{
public:

    /// Specifies if blobs should be cached on a local fs
    enum ECacheFlags {
        eCacheInput = 0x1,  ///< Cache input streams
        eCacheOutput = 0x2, ///< Cache ouput streams
        eCacheBoth = eCacheInput | eCacheOutput
    };
    typedef unsigned int TCacheFlags;

    CBlobStorage_NetCache_Impl();

    /// Create Blob Storage 
    /// @param[in] nc_client
    ///  NetCache client. Session Storage will delete it when
    ///  it goes out of scope.
    /// @param[in] flags
    ///  Specifies if blobs should be cached on a local fs
    ///  before they are accessed for read/write.
    /// @param[in[ temp_dir
    ///  Specifies where on a local fs those blobs will be cached
    CBlobStorage_NetCache_Impl(T* nc_client, 
                          TCacheFlags flags = 0x0,
                          const string& temp_dir = ".");

    virtual ~CBlobStorage_NetCache_Impl(); 


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

public:
    static const string sm_InputBlobCachePrefix;
    static const string sm_OutputBlobCachePrefix;
private:
    auto_ptr<T> m_NCClient;
    auto_ptr<CNcbiIstream>    m_IStream;
    auto_ptr<CNcbiOstream>    m_OStream;

    auto_ptr<IReader> x_GetReader(const string& key,
                                  size_t& blob_size,
                                  ELockMode lockMode);
    void x_Check(const string& where);

    TCacheFlags m_CacheFlags;
    string   m_CreatedBlobId;
    string   m_TempDir;

    CBlobStorage_NetCache_Impl(const CBlobStorage_NetCache&);
    CBlobStorage_NetCache_Impl& operator=(CBlobStorage_NetCache&);
};

////////////////////////////////////////////////////////////////////////////////////
//


template <class T>
const string CBlobStorage_NetCache_Impl<T>::sm_InputBlobCachePrefix = ".nc_cache_input.";

template <class T>
const string CBlobStorage_NetCache_Impl<T>::sm_OutputBlobCachePrefix = ".nc_cache_output.";


template <class T>
CBlobStorage_NetCache_Impl<T>::CBlobStorage_NetCache_Impl(T* nc_client,
                                                    TCacheFlags flags,
                                                    const string&  temp_dir)
    : m_NCClient(nc_client), 
      m_CacheFlags(flags),
      m_TempDir(temp_dir)
{
}

template <class T>
CBlobStorage_NetCache_Impl<T>::CBlobStorage_NetCache_Impl()
{
    NCBI_THROW(CException, eInvalid,
               "Can not create an empty blob storage.");
}


template <class T>
CBlobStorage_NetCache_Impl<T>::~CBlobStorage_NetCache_Impl() 
{
    try {
        Reset();
    } NCBI_CATCH_ALL("CBlobStorage_NetCache_Impl::~CBlobStorage_NetCache()");
}

template <class T>
void CBlobStorage_NetCache_Impl<T>::x_Check(const string& where)
{
    if ( (m_IStream.get() && !(m_CacheFlags & eCacheInput)) || 
         (m_OStream.get() && !(m_CacheFlags & eCacheOutput)) )
        NCBI_THROW(CBlobStorageException,
                   eBusy, "Communication channel is already in use." + where);
}

template <class T>
auto_ptr<IReader> CBlobStorage_NetCache_Impl<T>::x_GetReader(const string& key,
                                                  size_t& blob_size,
                                                  ELockMode lockMode)
{
    typename T::ELockMode mode = lockMode == eLockNoWait ? 
        T::eLockNoWait : T::eLockWait;
    x_Check("GetReader");
    blob_size = 0;

    auto_ptr<IReader> reader;
    if (key.empty()) 
        NCBI_THROW(CBlobStorageException,
                   eBlobNotFound, "Requested blob is not found: " + key);
    int try_count = 0;
    while(1) {
        try {
            reader.reset(m_NCClient->GetData(key, &blob_size, mode));
            break;
        } catch(CNetCacheException& ex1) {
            if(ex1.GetErrCode() != CNetCacheException::eBlobLocked) 
                throw;

            NCBI_RETHROW(ex1, CBlobStorageException, eBlocked, 
                       "Requested blob(" + key + ") is blocked by another process.");

        } catch (CNetServiceException& ex) {
            if (ex.GetErrCode() != CNetServiceException::eTimeout) 
                throw;
            
            ERR_POST_XX(ConnServ_NetCache, 4,
                        "Communication Error : " << ex.what());
            if (++try_count >= 2)
                throw;
            SleepMilliSec(1000 + try_count*2000);
        }
    }
    if (!reader.get()) {
        NCBI_THROW(CBlobStorageException,
                   eBlobNotFound, "Requested blob is not found: " + key);
    }
    return reader;
}

template <class T>
bool CBlobStorage_NetCache_Impl<T>::IsKeyValid(const string& str)
{
    try {
        CNetCacheKey key(str);
        return true;
    } catch(...) {
        return false;
    }
}

template <class T>
CNcbiIstream& CBlobStorage_NetCache_Impl<T>::GetIStream(const string& key,
                                             size_t* blob_size,
                                             ELockMode lockMode)
{
    if (blob_size) *blob_size = 0;
    size_t b_size = 0;
    auto_ptr<IReader> reader = x_GetReader(key, b_size, lockMode);

    if (blob_size) *blob_size = b_size;
    if (m_CacheFlags & eCacheInput) {
        auto_ptr<fstream> fstr(CFile::CreateTmpFileEx(m_TempDir,sm_InputBlobCachePrefix));
        if( !fstr.get() || !fstr->good()) {
            fstr.reset();
            NCBI_THROW(CBlobStorageException, eReader, 
                       "Reader couldn't create a temporary file. BlobKey: " + key);
        }

        char buf[1024];
        size_t bytes_read = 0;
        while( reader->Read(buf, sizeof(buf), &bytes_read) == eRW_Success ) {
            fstr->write(buf, bytes_read);
        }
        fstr->flush();
        fstr->seekg(0);
        m_IStream.reset(fstr.release());
    } else {    
        m_IStream.reset(new CRStream(reader.release(), 0,0, 
                                     CRWStreambuf::fOwnReader));
        if( !m_IStream.get() || !m_IStream->good()) {
            m_IStream.reset();
            NCBI_THROW(CBlobStorageException,
                       eReader, "Reader couldn't create a input stream. BlobKey: " + key);
        }

    }
    return *m_IStream;
}

template <class T>
string CBlobStorage_NetCache_Impl<T>::GetBlobAsString(const string& data_id)
{
    size_t b_size = 0;
    auto_ptr<IReader> reader = x_GetReader(data_id, b_size, eLockWait);
    string buf(b_size,0);

    size_t idx = 0;
    ERW_Result res = eRW_Success;
    while (res != eRW_Eof) {
        size_t bytes_read = 0;
        res = reader->Read(&buf[idx], b_size, &bytes_read);
        switch (res) {
        case eRW_Success:
        case eRW_Eof:
            b_size -= bytes_read;
            idx += bytes_read;
            break;
        default:
            NCBI_THROW(CBlobStorageException,
                       eReader, "Reader couldn't read a blob. BlobKey: " + data_id);
        } // switch
    } // for
   
    return buf;
}

template <class T>
CNcbiOstream& CBlobStorage_NetCache_Impl<T>::CreateOStream(string& key,
                                                   ELockMode)

{
    x_Check("CreateOStream");
    if (!(m_CacheFlags & eCacheOutput)) {
        auto_ptr<IWriter> writer;
        int try_count = 0;
        while(1) {
            try {
                writer.reset(m_NCClient->PutData(&key));
                break;
            }
            catch (CNetServiceException& ex) {
                if (ex.GetErrCode() != CNetServiceException::eTimeout) 
                    throw;

                ERR_POST_XX(ConnServ_NetCache, 5,
                            "Communication Error : " << ex.what());
                if (++try_count >= 2)
                    throw;
                SleepMilliSec(1000 + try_count*2000);
            }
        }
        if (!writer.get()) {
            NCBI_THROW(CBlobStorageException,
                       eWriter, "Writer couldn't be created. BlobKey: " + key);
        }
        m_OStream.reset( new CWStream(writer.release(), 0,0, 
                                      CRWStreambuf::fOwnWriter));
        if( !m_OStream.get() || !m_OStream->good()) {
            m_OStream.reset();
            NCBI_THROW(CBlobStorageException,
                       eWriter, "Writer couldn't create an ouput stream. BlobKey: " + key);
        }

    } else {
        if( key.empty() || !IsKeyValid(key) ) key = CreateEmptyBlob();
        m_CreatedBlobId = key;
        m_OStream.reset(CFile::CreateTmpFileEx(m_TempDir,sm_OutputBlobCachePrefix));
        if( !m_OStream.get() || !m_OStream->good()) {
            m_OStream.reset();
            NCBI_THROW(CBlobStorageException,
                       eWriter, 
                       "Writer couldn't create a temporary file in \"" + m_TempDir + "\" dir. "
                       "Check \"tmp_dir\" parameter in the registry for netcache client.");
        }
    }
    return *m_OStream;
}

template <class T>
string CBlobStorage_NetCache_Impl<T>::CreateEmptyBlob()
{
    x_Check("CreateEmptyBlob");
    if (m_NCClient.get())
        return m_NCClient->PutData((const void*)NULL,0);
    return kEmptyStr;

}
template <class T>
void CBlobStorage_NetCache_Impl<T>::DeleteBlob(const string& data_id)
{
    x_Check("DeleteBlob");
    if (!data_id.empty() && m_NCClient.get()) 
        m_NCClient->Remove(data_id);
}

template <class T>
void CBlobStorage_NetCache_Impl<T>::Reset()
{
    m_IStream.reset();
    try {
        if ((m_CacheFlags & eCacheOutput) && m_OStream.get() && !m_CreatedBlobId.empty() ) {
            auto_ptr<IWriter> writer;
            int try_count = 0;
            while(1) {
                try {
                    writer.reset(m_NCClient->PutData(&m_CreatedBlobId));
                    break;
                }
                catch (CNetServiceException& ex) {
                    if (ex.GetErrCode() != CNetServiceException::eTimeout) 
                        throw;
                    ERR_POST_XX(ConnServ_NetCache, 6,
                                "Communication Error : " << ex.what());
                    if (++try_count >= 2)
                        throw;
                    SleepMilliSec(1000 + try_count*2000);
                }
            }
            if (!writer.get()) { 
                NCBI_THROW(CBlobStorageException,
                           eWriter, "Writer couldn't be created.");
            }
            fstream* fstr = dynamic_cast<fstream*>(m_OStream.get());
            if (fstr) {
                fstr->flush();
                fstr->seekg(0);
                char buf[1024];
                while( fstr->good() ) {
                    fstr->read(buf, sizeof(buf));
                    if( writer->Write(buf, fstr->gcount()) != eRW_Success)
                        NCBI_THROW(CBlobStorageException,
                                   eWriter, "Couldn't write to Writer.");
                }
            } else {
                NCBI_THROW(CBlobStorageException,
                           eWriter, "Wrong cast.");
            }
            m_CreatedBlobId = "";
            
        }
        if (m_OStream.get() && !(m_CacheFlags & eCacheOutput)) {
            m_OStream->flush();
            if (!m_OStream->good()) {
                NCBI_THROW(CBlobStorageException,
                           eWriter, " " ); //m_NCClient->GetCommErrMsg());
            }
        }
    }catch (...) {
        m_OStream.reset();
        throw;
    }
    m_OStream.reset();
}


END_NCBI_SCOPE

#endif // __BLOB_STORAGE_NETCACHE_IMPL__HPP
