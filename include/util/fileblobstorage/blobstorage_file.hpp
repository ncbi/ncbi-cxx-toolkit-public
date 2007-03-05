#ifndef CONNECT_SERVICES__BLOB_STORAGE_FILE_HPP
#define CONNECT_SERVICES__BLOB_STORAGE_FILE_HPP

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

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
///
/// CBlobStorage_File ---
/// Implementaion of IBlobStorage interface based on files service
///
class NCBI_BLOBSTORAGE_FILE_EXPORT CBlobStorage_File : public IBlobStorage
{
public:

    CBlobStorage_File();

    /// Create Blob Storage 
    /// @param[in[ storage_dir
    ///  Specifies where on a local fs those blobs will be stored
    explicit CBlobStorage_File(const string& storage_dir);

    virtual ~CBlobStorage_File(); 


    virtual bool IsKeyValid(const string& str);

    /// Get a blob content as a string
    ///
    /// @param[in] blob_key
    ///    Blob key to read from
    virtual string        GetBlobAsString(const string& data_id);

    /// Get an input stream to a blob
    ///
    /// @param[in] blob_key
    ///    Blob key to read from
    /// @param[out] blob_size
    ///    if blob_size if not NULL the size of a blob is retured
    /// @param[in] lock_mode
    ///    Blob locking mode
    virtual CNcbiIstream& GetIStream(const string& data_id,
                                     size_t* blob_size = 0,
                                     ELockMode lock_mode = eLockWait);

    /// Get an output stream to a blob
    ///
    /// @param blob_key
    ///    Blob key to write to. If a blob with a given key does not exist
    ///    a new blob will be created and its key will be assigned to blob_key
    /// @param lock_mode
    ///    Blob locking mode
    virtual CNcbiOstream& CreateOStream(string& data_id,
                                        ELockMode lock_mode = eLockNoWait);

    /// Create a new empty blob
    /// 
    /// @return 
    ///     Newly create blob key
    virtual string CreateEmptyBlob();

    /// Delete a blob
    ///
    /// @param[in] blob_key
    ///    Blob key to delete
    virtual void DeleteBlob(const string& data_id);
    
    /// Close all streams and connections.
    virtual void Reset();


    /// Delete the storage with all its data
    virtual void DeleteStorage();

private:
    auto_ptr<CNcbiIstream>    m_IStream;
    auto_ptr<CNcbiOstream>    m_OStream;

    string   m_StorageDir;

    CBlobStorage_File(const CBlobStorage_File&);
    CBlobStorage_File& operator=(CBlobStorage_File&);
};

extern NCBI_BLOBSTORAGE_FILE_EXPORT const char* kBlobStorageFileDriverName;

extern "C"
{

NCBI_BLOBSTORAGE_FILE_EXPORT
void NCBI_EntryPoint_xblobstorage_file(
     CPluginManager<IBlobStorage>::TDriverInfoList&   info_list,
     CPluginManager<IBlobStorage>::EEntryPointRequest method);

NCBI_BLOBSTORAGE_FILE_EXPORT
void BlobStorage_RegisterDriver_File(void);

} // extern C


END_NCBI_SCOPE

#endif // CONNECT_SERVICES__BLOB_STORAGE_FILE_HPP
