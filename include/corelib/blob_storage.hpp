#ifndef __BLOB_STORAGE_IFACE__HPP
#define __BLOB_STORAGE_IFACE__HPP
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
 * Author: Maxim Didenko
 *
 * File Description:
 *   Blob Storage Interface
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiexpt.hpp>


#include <string>

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
///
/// Blob Storage interface
///
class NCBI_XNCBI_EXPORT IBlobStorage
{
public:
    enum ELockMode {
        eLockWait,   ///< waits for BLOB to become available
        eLockNoWait  ///< throws an exception immediately if BLOB locked
    };

    virtual ~IBlobStorage();

    /// Get a blob content as a string
    ///
    /// @param blob_key
    ///    Blob key to read
    virtual string        GetBlobAsString(const string& blob_key) = 0;

    /// Get an input stream to a blob
    ///
    /// @param blob_key
    ///    Blob key to read
    /// @param blob_size
    ///    if blob_size if not NULL the size of a blob is retured
    /// @param lock_mode
    ///    Blob locking mode
    virtual CNcbiIstream& GetIStream(const string& blob_key, 
                                     size_t* blob_size = NULL,
                                     ELockMode lock_mode = eLockWait) = 0;

    /// Get an output stream to a blob
    ///
    /// @param blob_key
    ///    Blob key to read. If a blob with a given key does not exist
    ///    an key of a newly create blob will be assigned to blob_key
    /// @param lock_mode
    ///    Blob locking mode
    virtual CNcbiOstream& CreateOStream(string& blob_key, 
                                        ELockMode lock_mode = eLockNoWait) = 0;

    /// Create an new blob
    /// 
    /// @return 
    ///     Newly create blob key
    virtual string CreateEmptyBlob() = 0;

    /// Delete a blob
    ///
    /// @param blob_key
    ///    Blob key to read
    virtual void DeleteBlob(const string& data_id) = 0;

    /// Reset a stroge
    ///
    /// @note
    ///    The implementatioin of this method should close 
    ///    all opened streams and connections
    virtual void Reset() = 0;

};


///////////////////////////////////////////////////////////////////////////////
///
/// Blob Storage Factory interafce
///
/// @sa IBlobStorage
///
class NCBI_XNCBI_EXPORT IBlobStorageFactory
{
public:
    virtual ~IBlobStorageFactory();

    /// Create an instance of Blob Storage
    ///
    virtual IBlobStorage* CreateInstance(void) = 0;
};

///////////////////////////////////////////////////////////////////////////////
///
/// Blob Storage Exception
///
class CBlobStorageException : public CException
{
public:
    enum EErrCode {
        eReader,    ///< A problem arised while reading from a stroage
        eWriter,    ///< A problem arised while writting to a stroage
        eBlocked    ///< A blob is blocked by another reader/writer
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eReader:  return "eReaderError";
        case eWriter:  return "eWriterError";
        case eBlocked: return "eBlocked";
        default:       return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CBlobStorageException, CException);
};


///////////////////////////////////////////////////////////////////////////////
///
/// An Empty implemention of Blob Storage Interface
///
class CBlobStorage_Null : public IBlobStorage
{
public:
    virtual ~CBlobStorage_Null() {}

    virtual string        GetBlobAsString( const string& blob_key)
    {
        return "";
    }

    virtual CNcbiIstream& GetIStream(const string&,
                                     size_t* blob_size = 0,
                                     ELockMode lock_mode = eLockWait)
    {
        if (blob_size) *blob_size = 0;
        NCBI_THROW(CBlobStorageException,
                   eReader, "Empty Storage reader.");
    }
    virtual CNcbiOstream& CreateOStream(string&, 
                                        ELockMode lock_mode = eLockNoWait)
    {
        NCBI_THROW(CBlobStorageException,
                   eWriter, "Empty Storage writer.");
    }

    virtual string CreateEmptyBlob() { return kEmptyStr; };
    virtual void DeleteBlob(const string& data_id) {}
    virtual void Reset() {};
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/12/20 17:13:34  didenko
 * Added new IBlobStorage interface
 *
 * ===========================================================================
 */

#endif /* __BLOB_STORAGE_IFACE__HPP */
