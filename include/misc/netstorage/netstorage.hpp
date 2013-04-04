#ifndef MISC_NETSTORAGE__NETSTORAGE__HPP
#define MISC_NETSTORAGE__NETSTORAGE__HPP

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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *   A generic API for accessing heterogeneous storage services.
 *
 */

/// @file netstorage_api.hpp
///

#include <connect/services/neticache_client.hpp>

BEGIN_NCBI_SCOPE


struct SNetFileImpl;            ///< @internal
struct SNetStorageImpl;         ///< @internal
struct SNetStorageByKeyImpl;    ///< @internal


/** @addtogroup NetStorage
 *
 * @{
 */

/// Exception class for use by CNetStorage, CNetStorageByKey, and CNetFile
///
class CNetStorageException : public CException
{
public:
    enum EErrCode {
        eInvalidArg,    ///< Caller passed invalid arguments to the API
        eNotExist,      ///< Illegal op applied to non-existent file
        eIOError,       ///< I/O error encountered while performing an op
        eTimeout        ///< Timeout encountered while performing an op
    };
    virtual const char* GetErrCodeString() const;
    NCBI_EXCEPTION_DEFAULT(CNetStorageException, CException);
};


/// Blob storage allocation and access strategy
enum ENetStorageFlags {
    fNST_Fast       = (1 << 0), ///< E.g. use NetCache as the primary storage
    fNST_Persistent = (1 << 1), ///< E.g. use FileTrack as the primary storage
    fNST_Movable    = (1 << 2), ///< Allow the file to move between storages
    fNST_Cacheable  = (1 << 3)  ///< Use NetCache for data caching
};
typedef int TNetStorageFlags;  ///< Bitwise OR of ENetStorageFlags



/////////////////////////////////////////////////////////////////////////////
///
/// Basic network-based file I/O
///
/// Sequiential I/O only
/// Can switch between reading and writing but only explicitly, using Close()
///

class NCBI_XCONNECT_EXPORT CNetFile
{
    NCBI_NET_COMPONENT(NetFile);

    /// Return unique ID of the file
    string GetID(void);

    /// Read no more than 'buf_size' bytes of the file contents
    /// (starting at the current position)
    ///
    /// @param buffer
    ///  Pointer to the receiving buffer. NULL means to skip bytes (advance
    ///  the current file reading position by 'buf_size' bytes).
    /// @param buf_size
    ///  Number of bytes to attempt to read (or to skip)
    /// @return
    ///  Number of bytes actually read into the receiving buffer (or skipped)
    ///
    /// @throw CNetStorageException
    ///  On any error -- and only if no data at all has been actually read.
    ///  Also, if 'CNetFile' is in writing mode.
    ///
    size_t Read(void* buffer, size_t buf_size);

    /// Read file (starting at the current position) and put the read data
    /// into a string
    ///
    /// @param data
    ///  The string in which to store the read data
    ///
    /// @throw CNetStorageException
    ///  If unable to read ALL of the data, or if 'CNetFile' is in writing mode
    ///
    void Read(string* data);

    /// Check if the last Read() has hit EOF
    ///
    /// @return
    ///  TRUE if if the last Read() has hit EOF;  FALSE otherwise
    ///
    /// @throw CNetStorageException
    ///  If file doesn't exist, or if 'CNetFile' is in writing mode
    ///
    bool Eof(void);

    /// Write data to the file (starting at the current position)
    ///
    /// @param buffer
    ///  Pointer to the data to write
    /// @param buf_size
    ///  Data length in bytes
    ///
    /// @throw CNetStorageException
    ///  If unable to write ALL of the data, or if CNetFile is in reading mode
    ///
    void Write(const void* buffer, size_t buf_size);

    /// Write string to the file (starting at the current position)
    ///
    /// @param data
    ///  Data to write to the file
    ///
    /// @throw CNetStorageException
    ///  If unable to write ALL of the data, or if CNetFile is in reading mode
    ///
    void Write(const string& data);

    /// Return size of the file
    ///
    /// @return
    ///  Size of the file in bytes
    ///
    /// @throw CNetStorageException
    ///  On any error (including if the file does not yet exist)
    ///
    Uint8 GetSize(void);

    /// Finalize and close the current file stream.
    /// If the operation is successful, then the state (including the current
    /// position, if applicable) of the 'CNetFile' is reset, and you can start
    /// reading from (or writing to) the file all anew, as if the 'CNetFile'
    /// object had just been created by 'CNetStorage'.
    ///
    /// @throw CNetStorageException
    ///  If cannot finalize writing operations
    void Close(void);
};




/////////////////////////////////////////////////////////////////////////////
///
/// Network BLOB storage API
///

class NCBI_XCONNECT_EXPORT CNetStorage
{
    NCBI_NET_COMPONENT(NetStorage);

    /// Construct a CNetStorage object
    ///
    /// @param icache_client
    ///  NetCache service used for storing movable blobs and for caching
    /// @param default_flags
    ///  Default storage preferences for files created by this object.
    ///
    explicit CNetStorage
    (CNetICacheClient::TInstance icache_client,
     TNetStorageFlags            default_flags = 0);

    /// Construct a CNetStorage object for use with FileTrack only.
    ///
    /// @param default_flags
    ///  Default storage preferences for files created by this object.
    ///
    explicit CNetStorage(TNetStorageFlags default_flags = 0);

    /// Create new NetStorage file.
    /// The physical storage is allocated during the first CNetFile::Write() op
    ///
    /// @param flags
    ///  Combination of flags that defines file location (storage) and caching
    /// @return
    ///  New CNetFile object
    ///
    CNetFile Create(TNetStorageFlags flags = 0);

    /// Open an existing NetStorage file for reading.
    /// @note
    ///  The file is not checked for existence until CNetFile::Read() is called

    /// @param file_id
    ///  File to open
    /// @param flags
    ///  Combination of flags that hints at the current file location (storage)
    ///  and caching. If this combination differs from that embedded in
    ///  'file_id', a new file ID will be generated for this file.
    /// @return
    ///  New CNetFile object
    ///
    CNetFile Open(const string& file_id, TNetStorageFlags flags = 0);

    /// Relocate a file according to the specified combination of flags
    ///
    /// @param file_id
    ///  An existing file to relocate
    /// @param flags
    ///  Combination of flags that defines the new file location (storage)
    ///
    /// @return
    ///  New file ID that fully reflects the new file location.
    ///  If possible, this new ID should be used for further access to the
    ///  file. Note however that its original ID still can be used as well.
    ///
    string Relocate(const string& file_id, TNetStorageFlags flags);

    /// Check if the file addressed by 'file_id' exists.
    ///
    /// @param file_id
    ///  File to check for existence
    /// @return
    ///  TRUE if the file exists; FALSE otherwise
    ///
    bool Exists(const string& file_id);

    /// Remove the file addressed by 'file_id'. If the file is
    /// cached, an attempt is made to purge it from the cache as well.
    ///
    /// @param file_id
    ///  File to remove
    ///
    void Remove(const string& file_id);
};



/////////////////////////////////////////////////////////////////////////////
///
/// Network BLOB storage API -- with access by user-defined keys
///

class NCBI_XCONNECT_EXPORT CNetStorageByKey
{
    NCBI_NET_COMPONENT(NetStorageByKey);

    /// Construct a CNetStorageByKey object.
    ///
    /// @param icache_client
    ///  NetCache service used for storing movable blobs and for caching
    /// @param domain_name
    ///  Namespace for the keys that will be created by this object.
    ///  If not specified, then the cache name from the 'icache_client' object
    ///  will be used.
    /// @param default_flags
    ///  Default storage preferences for files created by this object.
    ///
    explicit CNetStorageByKey
    (CNetICacheClient::TInstance icache_client,
     const string&               domain_name   = kEmptyStr,
     TNetStorageFlags            default_flags = 0);

    /// Construct a CNetStorageByKey object for use with FileTrack only.
    ///
    /// @param domain_name
    ///  Namespace for the keys that will be created by this object
    /// @param default_flags
    ///  Default storage preferences for files created by this object.
    ///
    explicit CNetStorageByKey
    (const string&    domain_name,
     TNetStorageFlags default_flags = 0);

    /// Create a new file or open an existing file using the supplied
    /// unique key. The returned file object can be either written
    /// or read.
    ///
    /// @param key
    ///  User-defined unique key that, in combination with the domain name
    ///  specified in the constructor, can be used to address the file
    /// @param flags
    ///  Combination of flags that defines file location and caching.
    /// @return
    ///  New CNetFile object.
    ///
    CNetFile Open(const string& unique_key, TNetStorageFlags flags = 0);

    /// Relocate a file according to the specified combination of flags
    ///
    /// @param flags
    ///  Combination of flags that defines the new file location
    /// @param old_flags
    ///  Combination of flags that defines the current file location
    /// @return
    ///  A unique full file ID that reflects the new file location (storage)
    ///  and which can be used with CNetStorage::Open(). Note however that the
    ///  original ID 'unique_key' still can be used as well.
    ///
    string Relocate(const string&    unique_key,
                    TNetStorageFlags flags,
                    TNetStorageFlags old_flags = 0);

    /// Check if a file with the specified key exists in the storage
    /// hinted by 'flags'
    ///
    /// @return TRUE if the file exists; FALSE otherwise
    ///
    bool Exists(const string& key, TNetStorageFlags flags = 0);

    /// Remove a file addressed by a key and a set of flags
    ///
    /// @param key
    ///  User-defined unique key that, in combination with the domain name
    ///  specified in the constructor, addresses the file
    /// @param flags
    ///  Combination of flags that hints on the current file location
    ///
    void Remove(const string& key, TNetStorageFlags flags = 0);
};


/* @} */


inline void CNetFile::Write(const string& data)
{
    Write(data.data(), data.length());
}


END_NCBI_SCOPE

#endif  /* MISC_NETSTORAGE__NETSTORAGE__HPP */
