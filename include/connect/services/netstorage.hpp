#ifndef CONNECT_SERVICES__NETSTORAGE__HPP
#define CONNECT_SERVICES__NETSTORAGE__HPP

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

#include "json_over_uttp.hpp"
#include "srv_connections.hpp"

#include <corelib/ncbitime.hpp>


BEGIN_NCBI_SCOPE


struct SNetStorageObjectImpl;           ///< @internal
struct SNetStorageImpl;                 ///< @internal
struct SNetStorageByKeyImpl;            ///< @internal
struct SNetStorageObjectInfoImpl;       ///< @internal
class  CNetStorageObjectLoc;            ///< @internal


/** @addtogroup NetStorage
 *
 * @{
 */

/// Exception class for use by CNetStorage, CNetStorageByKey,
/// and CNetStorageObject
///
class NCBI_XCONNECT_EXPORT CNetStorageException : public CException
{
public:
    enum EErrCode {
        eInvalidArg,    ///< Caller passed invalid arguments to the API
        eNotExists,     ///< Illegal op applied to non-existent object
        eAuthError,     ///< Authentication error (e.g. no FileTrack API key)
        eIOError,       ///< I/O error encountered while performing an op
        eServerError,   ///< NetStorage server error
        eTimeout        ///< Timeout encountered while performing an op
    };
    virtual const char* GetErrCodeString() const;
    NCBI_EXCEPTION_DEFAULT(CNetStorageException, CException);
};

/// Enumeration that indicates the current location of the object.
enum ENetStorageObjectLocation {
    eNFL_Unknown,
    eNFL_NotFound,
    eNFL_NetCache,
    eNFL_FileTrack
};

/// Detailed information about a CNetStorage object.
/// This structure is returned by CNetStorageObject::GetInfo().
class NCBI_XCONNECT_EXPORT CNetStorageObjectInfo
{
    NCBI_NET_COMPONENT(NetStorageObjectInfo);

    /// Construct a CNetStorageObjectInfo object.
    CNetStorageObjectInfo(const string& object_loc,
            ENetStorageObjectLocation location,
            const CNetStorageObjectLoc& object_loc_struct,
            Uint8 object_size,
            CJsonNode::TInstance storage_specific_info);

    /// Load NetStorage object information from a JSON object.
    CNetStorageObjectInfo(const string& object_loc,
            const CJsonNode& object_info_node);

    /// Return a ENetStorageObjectLocation constant that corresponds to the
    /// storage back-end where the object currently resides. If the
    /// object cannot be found in any of the predictable locations,
    /// eNFL_NotFound is returned.  If the object is found, the
    /// returned value reflects the storage back-end that supplied
    /// the rest of information about the object.
    ENetStorageObjectLocation GetLocation() const;

    /// Return a JSON object containing the fields of the object ID.
    /// A valid value is returned even if GetLocation() == eNFL_NotFound.
    CJsonNode GetObjectLocInfo() const;

    /// Return object creation time reported by the storage back-end.
    /// @note Valid only if GetLocation() != eNFL_NotFound.
    CTime GetCreationTime() const;

    /// Return object size in bytes.
    /// @note Valid only if GetLocation() != eNFL_NotFound.
    Uint8 GetSize();

    /// Return a JSON object containing storage-specific information
    /// about the object.
    /// @note Valid only if GetLocation() != eNFL_NotFound.
    CJsonNode GetStorageSpecificInfo() const;

    /// If the object is stored on a network file system,
    /// return the pathname of the file. Otherwise, throw
    /// an exception.
    string GetNFSPathname() const;

    /// Pack the whole structure in a single JSON object.
    CJsonNode ToJSON();
};

/// Blob storage allocation and access strategy
enum ENetStorageFlags {
    fNST_Fast       = (1 << 0), ///< E.g. use NetCache as the primary storage
    fNST_Persistent = (1 << 1), ///< E.g. use FileTrack as the primary storage
    fNST_Movable    = (1 << 2), ///< Allow the object to move between storages
    fNST_Cacheable  = (1 << 3), ///< Use NetCache for data caching
    fNST_NoMetaData = (1 << 4), ///< Do not use NetStorage relational database
                                ///< to track ownership & changes. Attributes
                                ///< and querying will also be disabled.
};
typedef int TNetStorageFlags;  ///< Bitwise OR of ENetStorageFlags



/////////////////////////////////////////////////////////////////////////////
///
/// Basic network-based data object I/O
///
/// Sequential I/O only
/// Can switch between reading and writing but only explicitly, using Close()
///

class NCBI_XCONNECT_EXPORT CNetStorageObject
{
    NCBI_NET_COMPONENT(NetStorageObject);

    /// Return object locator
    string GetLoc(void);

    /// Read no more than 'buf_size' bytes of the object contents
    /// (starting at the current position)
    ///
    /// @param buffer
    ///  Pointer to the receiving buffer. NULL means to skip bytes (advance
    ///  the current object reading position by 'buf_size' bytes).
    /// @param buf_size
    ///  Number of bytes to attempt to read (or to skip)
    /// @return
    ///  Number of bytes actually read into the receiving buffer (or skipped)
    ///
    /// @throw CNetStorageException
    ///  On any error -- and only if no data at all has been actually read.
    ///  Also, if 'CNetStorageObject' is in writing mode.
    ///
    size_t Read(void* buffer, size_t buf_size);

    /// Read object (starting at the current position) and put the read data
    /// into a string
    ///
    /// @param data
    ///  The string in which to store the read data
    ///
    /// @throw CNetStorageException
    ///  If unable to read ALL of the data, or if 'CNetStorageObject'
    ///  is in writing mode
    ///
    void Read(string* data);

    /// Check if the last Read() has hit EOF
    ///
    /// @return
    ///  TRUE if if the last Read() has hit EOF;  FALSE otherwise
    ///
    /// @throw CNetStorageException
    ///  If the object doesn't exist, or if 'CNetStorageObject'
    ///  is in writing mode
    ///
    bool Eof(void);

    /// Write data to the object (starting at the current position)
    ///
    /// @param buffer
    ///  Pointer to the data to write
    /// @param buf_size
    ///  Data length in bytes
    ///
    /// @throw CNetStorageException
    ///  If unable to write ALL of the data, or if CNetStorageObject
    ///  is in reading mode
    ///
    void Write(const void* buffer, size_t buf_size);

    /// Write string to the object (starting at the current position)
    ///
    /// @param data
    ///  Data to write to the object
    ///
    /// @throw CNetStorageException
    ///  If unable to write ALL of the data, or if CNetStorageObject
    ///  is in reading mode
    ///
    void Write(const string& data);

    /// Finalize and close the current object stream.
    /// If the operation is successful, then the state (including the current
    /// position, if applicable) of the 'CNetStorageObject' is reset, and
    /// you can start reading from (or writing to) the object all
    /// anew, as if the 'CNetStorageObject' object had just been
    /// created by 'CNetStorage'.
    ///
    /// @throw CNetStorageException
    ///  If cannot finalize writing operations
    void Close(void);

    /// Return size of the object
    ///
    /// @return
    ///  Size of the object in bytes
    ///
    /// @throw CNetStorageException
    ///  On any error (including if the object does not yet exist)
    ///
    Uint8 GetSize(void);

    /// Get the current value of the specified attribute. Attribute
    /// values are not cached by this method.
    ///
    /// @throw If the underlying storage does not support attributes,
    /// a CNetStorageException will be thrown.
    ///
    string GetAttribute(const string& attr_name);

    /// Set the new value for the specified attribute.
    ///
    /// @throw If the underlying storage does not support attributes,
    /// a CNetStorageException will be thrown.
    ///
    void SetAttribute(const string& attr_name, const string& attr_value);

    /// Return detailed information about the object.
    ///
    /// @return
    ///  A CNetStorageObjectInfo object. If the object is not found, a
    ///  valid object is returned, which returns eNFL_NotFound for
    ///  GetLocation().
    ///
    /// @see CNetStorageObjectInfo
    ///
    CNetStorageObjectInfo GetInfo(void);
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
    /// @param init_string
    ///  Initialization string that contains client identification,
    ///  network service locations, etc.
    ///  The string must be a sequence of apersand-separated pairs of
    ///  attribute names and their values. Within each pair, the name
    ///  and the value of the attribute must be separated by the equality
    ///  sign, and the value must be URL-encoded.
    ///  The following attributes are recoginzed:
    ///  * client     - Application name.
    ///  * nst        - NetStorage server address or LBSM service name
    ///                 pointing to a group of NetStorage servers.
    ///  * nc         - NetCache service name or server address.
    ///  * cache      - Application-specific NetCache cache name.
    ///  Example: "clent=MyApp&nst=NST_Test&nc=NC_MyApp_TEST&cache=myapp"
    ///
    /// @param default_flags
    ///  Default storage preferences for the created objects.
    ///
    explicit CNetStorage(const string& init_string,
            TNetStorageFlags default_flags = 0);

    /// Create new NetStorage object.
    /// The physical storage is allocated during the first
    /// CNetStorageObject::Write() operation.
    ///
    /// @param flags
    ///  Combination of flags that defines object location (storage) and caching
    /// @return
    ///  New CNetStorageObject
    ///
    CNetStorageObject Create(TNetStorageFlags flags = 0);

    /// Open an existing NetStorage object for reading.
    /// @note
    ///  The object is not checked for existence until
    ///  CNetStorageObject::Read() is called
    ///
    /// @param object_loc
    ///  File to open
    /// @param flags
    ///  Combination of flags that hints at the current object location
    ///  (storage).
    ///  and caching. If this combination differs from that embedded in
    ///  'object_loc', a new object ID will be generated for this object.
    /// @return
    ///  New CNetStorageObject
    ///
    CNetStorageObject Open(const string& object_loc,
            TNetStorageFlags flags = 0);

    /// Relocate a object according to the specified combination of flags
    ///
    /// @param object_loc
    ///  An existing object to relocate
    /// @param flags
    ///  Combination of flags that defines the new object location (storage)
    ///
    /// @return
    ///  New object ID that fully reflects the new object location.
    ///  If possible, this new ID should be used for further access to the
    ///  object. Note however that its original ID still can be used as well.
    ///
    string Relocate(const string& object_loc, TNetStorageFlags flags);

    /// Check if the object addressed by 'object_loc' exists.
    ///
    /// @param object_loc
    ///  File to check for existence
    /// @return
    ///  TRUE if the object exists; FALSE otherwise
    ///
    bool Exists(const string& object_loc);

    /// Remove the object addressed by 'object_loc'. If the object is
    /// cached, an attempt is made to purge it from the cache as well.
    ///
    /// @param object_loc
    ///  File to remove
    ///
    void Remove(const string& object_loc);
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
    /// @param init_string
    ///  Initialization string that contains client identification,
    ///  network service locations, etc.
    ///  The string must be a sequence of apersand-separated pairs of
    ///  attribute names and their values. Within each pair, the name
    ///  and the value of the attribute must be separated by the equality
    ///  sign, and the value must be URL-encoded.
    ///  The following attributes are recoginzed:
    ///  * domain     - Namespace name, within which the keys passed
    ///                 to the methods of this class must be unique.
    ///  * client     - Application name.
    ///  * nst        - NetStorage server address or LBSM service name
    ///                 pointing to a group of NetStorage servers.
    ///  * nc         - NetCache service name or server address.
    ///  * cache      - Application-specific NetCache cache name.
    ///  Example: "clent=MyApp&nst=NST_Test&nc=NC_MyApp_TEST&cache=myapp"
    ///
    /// @param default_flags
    ///  Default storage preferences for objects created by this object.
    ///
    explicit CNetStorageByKey(const string& init_string,
            TNetStorageFlags default_flags = 0);

    /// Create a new object or open an existing object using the supplied
    /// unique key. The returned object object can be either written
    /// or read.
    ///
    /// @param key
    ///  User-defined unique key that, in combination with the domain name
    ///  specified in the constructor, can be used to address the object
    /// @param flags
    ///  Combination of flags that defines object location and caching.
    /// @return
    ///  New CNetStorageObject.
    ///
    CNetStorageObject Open(const string& unique_key,
            TNetStorageFlags flags = 0);

    /// Relocate a object according to the specified combination of flags
    ///
    /// @param flags
    ///  Combination of flags that defines the new object location
    /// @param old_flags
    ///  Combination of flags that defines the current object location
    /// @return
    ///  A unique full object ID that reflects the new object location (storage)
    ///  and which can be used with CNetStorage::Open(). Note however that the
    ///  original ID 'unique_key' still can be used as well.
    ///
    string Relocate(const string&    unique_key,
                    TNetStorageFlags flags,
                    TNetStorageFlags old_flags = 0);

    /// Check if a object with the specified key exists in the storage
    /// hinted by 'flags'
    ///
    /// @return TRUE if the object exists; FALSE otherwise
    ///
    bool Exists(const string& key, TNetStorageFlags flags = 0);

    /// Remove a object addressed by a key and a set of flags
    ///
    /// @param key
    ///  User-defined unique key that, in combination with the domain name
    ///  specified in the constructor, addresses the object
    /// @param flags
    ///  Combination of flags that hints on the current object location
    ///
    void Remove(const string& key, TNetStorageFlags flags = 0);
};


/* @} */

END_NCBI_SCOPE

#include "netstorage_impl.hpp"

#endif  /* CONNECT_SERVICES__NETSTORAGE__HPP */
