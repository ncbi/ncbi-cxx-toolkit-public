#ifndef _GRID_RW_IMPL_HPP_
#define _GRID_RW_IMPL_HPP_


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
 */

#include <corelib/blob_storage.hpp>
#include <corelib/reader_writer.hpp>

#include <connect/connect_export.h>

BEGIN_NCBI_SCOPE

/// String or Blob Storage Writer
///
/// An implementation of the IWriter interface with a dual behavior.
/// It writes data into the "data_or_key" paramter until
/// the total number of written bytes reaches "max_string_size" parameter.
/// After that all data from "data_or_key" is stored into the Blob Storage
/// and all next calls to Write method will write data to the Blob Storage.
/// In this case "data_or_key" paramter holds a blob key for the written data.
///
class NCBI_XCONNECT_EXPORT CStringOrBlobStorageWriter : public IWriter
{
public:
    CStringOrBlobStorageWriter(size_t max_string_size,
                               IBlobStorage& storage,
                               string& data_or_key);
    CStringOrBlobStorageWriter(size_t max_string_size,
                               IBlobStorage* storage,
                               string& data_or_key);

    virtual ~CStringOrBlobStorageWriter();

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void);

private:

    ERW_Result x_WriteToStream(const void* buf,
                               size_t      count,
                               size_t*     bytes_written = 0);

    void x_Init(size_t max_string_size);

    size_t        m_MaxBuffSize;
    IBlobStorage& m_Storage;
    auto_ptr<IBlobStorage> m_StorageGuard;
    CNcbiOstream* m_BlobOstr;
    string&       m_Data;

private:
    CStringOrBlobStorageWriter(const CStringOrBlobStorageWriter&);
    CStringOrBlobStorageWriter& operator=(const CStringOrBlobStorageWriter&);
};


/// String or Blob Strorage Reader
///
/// An implementaion of the IReader interface with a dual behavior.
/// If "data_or_key" parameter can be interpered as Blob Storage key and
/// a blob with given key is found in the storage, then the storage is
/// used as data source. Otherwise "data_or_key" is a data source.
///
class NCBI_XCONNECT_EXPORT CStringOrBlobStorageReader : public IReader
{
public:
    CStringOrBlobStorageReader(const string& data_or_key,
                               IBlobStorage& storage,
                               size_t* data_size = NULL,
                               IBlobStorage::ELockMode lock_mode
                                         = IBlobStorage::eLockWait);
    CStringOrBlobStorageReader(const string& data_or_key,
                               IBlobStorage* storage,
                               size_t* data_size = NULL,
                               IBlobStorage::ELockMode lock_mode
                                         = IBlobStorage::eLockWait);

    virtual ~CStringOrBlobStorageReader();

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);

    virtual ERW_Result PendingCount(size_t* count);

private:

    const string& m_Data;
    IBlobStorage& m_Storage;
    auto_ptr<IBlobStorage> m_StorageGuard;
    CNcbiIstream* m_BlobIstr;
    string::const_iterator m_CurPos;

    void x_Init(size_t* data_size,
                IBlobStorage::ELockMode lock_mode);
private:
    CStringOrBlobStorageReader(const CStringOrBlobStorageReader&);
    CStringOrBlobStorageReader& operator=(const CStringOrBlobStorageReader&);
};


class CStringOrBlobStorageRWException : public CException
{
public:
    enum EErrCode {
        eInvalidFlag
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eInvalidFlag: return "eInvalidFlag";
        default:           return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CStringOrBlobStorageRWException, CException);
};

END_NCBI_SCOPE

#endif // __GRID_RW_IMPL_HPP_
