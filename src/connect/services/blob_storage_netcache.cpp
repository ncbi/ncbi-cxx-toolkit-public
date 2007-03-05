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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <connect/services/blob_storage_netcache.hpp>

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
//

const string CBlobStorage_NetCache::sm_InputBlobCachePrefix = ".nc_cache_input.";
const string CBlobStorage_NetCache::sm_OutputBlobCachePrefix = ".nc_cache_output.";


CBlobStorage_NetCache::CBlobStorage_NetCache(CNetCacheClient* nc_client,
                                       TCacheFlags flags,
                                       const string&  temp_dir)
    : m_NCClient(nc_client), 
      m_CacheFlags(flags),
      m_TempDir(temp_dir)
{
}

CBlobStorage_NetCache::CBlobStorage_NetCache()
{
    NCBI_THROW(CException, eInvalid,
               "Can not create an empty blob storage.");
}


CBlobStorage_NetCache::~CBlobStorage_NetCache() 
{
    try {
        Reset();
    } NCBI_CATCH_ALL("CBlobStorage_NetCache::~CBlobStorage_NetCache()");
}

void CBlobStorage_NetCache::x_Check(const string& where)
{
    if ( (m_IStream.get() && !(m_CacheFlags & eCacheInput)) || 
         (m_OStream.get() && !(m_CacheFlags & eCacheOutput)) )
        NCBI_THROW(CBlobStorageException,
                   eBusy, "Communication channel is already in use." + where);
}

auto_ptr<IReader> CBlobStorage_NetCache::x_GetReader(const string& key,
                                                  size_t& blob_size,
                                                  ELockMode lockMode)
{
    CNetCacheClient::ELockMode mode = lockMode == eLockNoWait ? 
        CNetCacheClient::eLockNoWait : CNetCacheClient::eLockWait;
    x_Check("GetReader");
    blob_size = 0;

    auto_ptr<IReader> reader;
    if (key.empty()) 
        NCBI_THROW(CBlobStorageException,
                   eBlobNotFound, "Requested blob is not found.");
    int try_count = 0;
    while(1) {
        try {
            reader.reset(m_NCClient->GetData(key, &blob_size, mode));
            break;
        } catch(CNetCacheException& ex1) {
            if(ex1.GetErrCode() != CNetCacheException::eBlobLocked) 
                throw;

            NCBI_RETHROW(ex1, CBlobStorageException, eBlocked, 
                       "Requested blob is blocked by another process.");

        } catch (CNetServiceException& ex) {
            if (ex.GetErrCode() != CNetServiceException::eTimeout) 
                throw;
            
            ERR_POST("Communication Error : " << ex.what());
            if (++try_count >= 2)
                throw;
            SleepMilliSec(1000 + try_count*2000);
        }
    }
    if (!reader.get()) {
        NCBI_THROW(CBlobStorageException,
                   eBlobNotFound, "Requested blob is not found.");
    }
    return reader;
}

bool CBlobStorage_NetCache::IsKeyValid(const string& str)
{
    try {
        CNetCache_Key key(str);
        return true;
    } catch(...) {
        return false;
    }
}

CNcbiIstream& CBlobStorage_NetCache::GetIStream(const string& key,
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

string CBlobStorage_NetCache::GetBlobAsString(const string& data_id)
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

CNcbiOstream& CBlobStorage_NetCache::CreateOStream(string& key,
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

                ERR_POST("Communication Error : " << ex.what());
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

string CBlobStorage_NetCache::CreateEmptyBlob()
{
    x_Check("CreateEmptyBlob");
    if (m_NCClient.get())
        return m_NCClient->PutData((const void*)NULL,0);
    return kEmptyStr;

}
void CBlobStorage_NetCache::DeleteBlob(const string& data_id)
{
    x_Check("DeleteBlob");
    if (!data_id.empty() && m_NCClient.get()) 
        m_NCClient->Remove(data_id);
}

void CBlobStorage_NetCache::Reset()
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
                    ERR_POST("Communication Error : " << ex.what());
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
                           eWriter, m_NCClient->GetCommErrMsg());
            }
        }
    }catch (...) {
        m_OStream.reset();
        throw;
    }
    m_OStream.reset();
}

//////////////////////////////////////////////////////////////////////////////
//


const char* kBlobStorageNetCacheDriverName = "netcache";


class CBlobStorageNetCacheCF 
    : public CSimpleClassFactoryImpl<IBlobStorage,CBlobStorage_NetCache>
{
public:
    typedef CSimpleClassFactoryImpl<IBlobStorage,
                                    CBlobStorage_NetCache> TParent;
public:
    CBlobStorageNetCacheCF() : TParent(kBlobStorageNetCacheDriverName, 0)
    {
    }
    virtual ~CBlobStorageNetCacheCF()
    {
    }

    virtual
    IBlobStorage* CreateInstance(
                   const string&    driver  = kEmptyStr,
                   CVersionInfo     version = NCBI_INTERFACE_VERSION(IBlobStorage),
                   const TPluginManagerParamTree* params = 0) const;
};

IBlobStorage* CBlobStorageNetCacheCF::CreateInstance(
           const string&                  driver,
           CVersionInfo                   version,
           const TPluginManagerParamTree* params) const
{
    if (driver.empty() || driver == m_DriverName) {
        if (version.Match(NCBI_INTERFACE_VERSION(IBlobStorage))
                            != CVersionInfo::eNonCompatible && params) {

            typedef CPluginManager<CNetCacheClient> TPMNetCache;
            TPMNetCache                      PM_NetCache;
            PM_NetCache.RegisterWithEntryPoint(NCBI_EntryPoint_xnetcache);
            auto_ptr<CNetCacheClient> nc_client (
                            PM_NetCache.CreateInstance(
                                            kNetCacheDriverName,
                                            TPMNetCache::GetDefaultDrvVers(),
                                            params)
                            );
            if( nc_client.get() ) {
                string temp_dir = 
                    GetParam(params, "tmp_dir", false, ".");
                vector<string> masks;
                masks.push_back( CBlobStorage_NetCache::sm_InputBlobCachePrefix + "*" );
                masks.push_back( CBlobStorage_NetCache::sm_OutputBlobCachePrefix + "*" );
                CDir curr_dir(temp_dir);
                CDir::TEntries dir_entries = curr_dir.GetEntries(masks, 
                                                                 CDir::eIgnoreRecursive);
                ITERATE( CDir::TEntries, it, dir_entries) {
                    (*it)->Remove(CDirEntry::eNonRecursive);
                }
                bool cache_input =
                    GetParamBool(params, "cache_input", false, false);
                bool cache_output =
                    GetParamBool(params, "cache_output", false, false);
 
                CBlobStorage_NetCache::TCacheFlags flags = 0;
                if (cache_input) flags |= CBlobStorage_NetCache::eCacheInput;
                if (cache_output) flags |= CBlobStorage_NetCache::eCacheOutput;

                return new CBlobStorage_NetCache(nc_client.release(),
                                                 flags, 
                                                 temp_dir);

            }
        }
    }
    return 0;
}

void NCBI_EntryPoint_xblobstorage_netcache(
     CPluginManager<IBlobStorage>::TDriverInfoList&   info_list,
     CPluginManager<IBlobStorage>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CBlobStorageNetCacheCF>
        ::NCBI_EntryPointImpl(info_list, method);
}

void BlobStorage_RegisterDriver_NetCache(void)
{
    RegisterEntryPoint<IBlobStorage>( NCBI_EntryPoint_xblobstorage_netcache );
}

END_NCBI_SCOPE
