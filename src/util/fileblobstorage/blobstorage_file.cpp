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
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbifile.hpp>

#include <util/fileblobstorage/blobstorage_file.hpp>
#include <util/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Util_BlobStore

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
//


CBlobStorage_File::CBlobStorage_File(const string& storage_dir)
{
    if (CDirEntry::IsAbsolutePath(storage_dir))
        m_StorageDir = storage_dir;
    else {
        m_StorageDir = CDir::GetCwd() 
            + CDirEntry::GetPathSeparator() 
            + storage_dir;
    }
    m_StorageDir = CDirEntry::NormalizePath(m_StorageDir);

    CDir dir(m_StorageDir);
    if (!dir.Exists())
        if (!dir.CreatePath())
            NCBI_THROW(CBlobStorageException,
                       eBusy, "Could not create a storage directory: " + m_StorageDir);
}

CBlobStorage_File::CBlobStorage_File()
{
    NCBI_THROW(CException, eInvalid,
               "Can not create an empty blob storage.");
}


CBlobStorage_File::~CBlobStorage_File() 
{
    try {
        Reset();
    } catch(exception& ex) {
        ERR_POST_X(1, "An exception caught in ~CBlobStorage_File() :" << 
                      ex.what());
    } catch(...) {
        ERR_POST_X(2, "An unknown exception caught in ~CBlobStorage_File() :");
    }
}

bool CBlobStorage_File::IsKeyValid(const string& str)
{
    return true;
}

CNcbiIstream& CBlobStorage_File::GetIStream(const string& key,
                                            size_t* blob_size,
                                            ELockMode lockMode)
{
    if (m_IStream.get())
        NCBI_THROW(CBlobStorageException,
                   eBusy, "The input stream is in use.");

    CFile blob(m_StorageDir + CDirEntry::GetPathSeparator() + key);
    if (!blob.Exists()) 
        NCBI_THROW(CBlobStorageException,
                   eBlobNotFound, "Requested blob is not found. " + key);

    Int8 fsize = blob.GetLength();
    m_IStream.reset(new CNcbiIfstream(blob.GetPath().c_str(), IOS_BASE::in | IOS_BASE::binary));
    if (!m_IStream->good() || fsize == -1 )
        NCBI_THROW(CBlobStorageException, eReader, 
                   "Reader couldn't create a temporary file. BlobKey: " + key);


    if (blob_size) *blob_size = (size_t)fsize;
    return *m_IStream;
}

string CBlobStorage_File::GetBlobAsString(const string& data_id)
{
    size_t bsize;
    CNcbiIstream& is = GetIStream(data_id, &bsize);
    string buf(bsize,0);
    is.read(&buf[0], bsize);
    m_IStream.reset();  
    return buf;
}

CNcbiOstream& CBlobStorage_File::CreateOStream(string& key,
                                               ELockMode)

{
    if (m_OStream.get())
        NCBI_THROW(CBlobStorageException,
                   eBusy, "The output stream is in use.");

    string fpath = CFile::GetTmpNameEx(m_StorageDir, kEmptyStr, CFile::eTmpFileCreate);
    m_OStream.reset(new CNcbiOfstream(fpath.c_str(), IOS_BASE::out | IOS_BASE::binary));
    if (!m_OStream->good())
            NCBI_THROW(CBlobStorageException,
                       eWriter, "Writer couldn't create an ouput stream.");
    string dir, base, ext;
    CDirEntry::SplitPath(fpath, &dir, &base, &ext);
    key = base;
    if (!ext.empty())
        key += '.' + ext;

    return *m_OStream;
}

string CBlobStorage_File::CreateEmptyBlob()
{
    string key;
    CreateOStream(key);
    m_OStream.reset();
    return key;
}

void CBlobStorage_File::DeleteBlob(const string& data_id)
{
    CFile blob(m_StorageDir + CDirEntry::GetPathSeparator() + data_id);
    if (blob.Exists()) 
        blob.Remove();
}

void CBlobStorage_File::Reset()
{
    m_IStream.reset();
    m_OStream.reset();
}

void CBlobStorage_File::DeleteStorage()
{
    CDir(m_StorageDir).Remove();
}

//////////////////////////////////////////////////////////////////////////////
//


const char* kBlobStorageFileDriverName = "fileblobstroage";


class CBlobStorageFileCF 
    : public CSimpleClassFactoryImpl<IBlobStorage,CBlobStorage_File>
{
public:
    typedef CSimpleClassFactoryImpl<IBlobStorage,
                                    CBlobStorage_File> TParent;
public:
    CBlobStorageFileCF() : TParent(kBlobStorageFileDriverName, 0)
    {
    }
    virtual ~CBlobStorageFileCF()
    {
    }

    virtual
    IBlobStorage* CreateInstance(
                   const string&    driver  = kEmptyStr,
                   CVersionInfo     version = NCBI_INTERFACE_VERSION(IBlobStorage),
                   const TPluginManagerParamTree* params = 0) const;
};

IBlobStorage* CBlobStorageFileCF::CreateInstance(
           const string&                  driver,
           CVersionInfo                   version,
           const TPluginManagerParamTree* params) const
{
    if (driver.empty() || driver == m_DriverName) {
        if (version.Match(NCBI_INTERFACE_VERSION(IBlobStorage))
                            != CVersionInfo::eNonCompatible && params) {

            string storage_dir = 
                GetParam(params, "storage_dir", false, ".");
            return new CBlobStorage_File(storage_dir);
        }
    }
    return 0;
}

void NCBI_EntryPoint_xblobstorage_file(
     CPluginManager<IBlobStorage>::TDriverInfoList&   info_list,
     CPluginManager<IBlobStorage>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CBlobStorageFileCF>
        ::NCBI_EntryPointImpl(info_list, method);
}

void BlobStorage_RegisterDriver_File(void)
{
    RegisterEntryPoint<IBlobStorage>( NCBI_EntryPoint_xblobstorage_file );
}

END_NCBI_SCOPE
