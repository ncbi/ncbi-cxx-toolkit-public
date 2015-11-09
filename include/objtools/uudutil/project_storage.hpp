#ifndef OBJTOOLS_UUDUTIL__STORAGE_UTIL_HPP
#define OBJTOOLS_UUDUTIL__STORAGE_UTIL_HPP

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
 * Author:  Liangshou Wu, Victor Joukov
 *
 * File Description:
 *   The helper classes for saving and retrieving data/GBProject to and from 
 *   the NetCache. All saved blob will be prepended a header (unless it
 *   explicitly says no) that contains information about data compression
 *   and asn serialization format if it is a ASN object. When retrieving,
 *   if the blobs contains no header, then we assume the data is not 
 *   compressed and in ASN binary if it is a ASN object.
 *
 */

#include <corelib/ncbiobj.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <connect/services/netstorage.hpp>
#include <connect/services/netcache_api.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class IGBProject;
    class CSeq_annot;
END_SCOPE(objects)

class CPrjStorageException : public CException
{
public:
    enum EErrCode {
        eInvalidKey             = 100,
        eAsnObjectNotMatch      = 101,
        eUnsupportedSerialFmt   = 102,
        eUnsupportedCompression = 103
    };

    virtual const char* GetErrCodeString() const
        {
            switch (GetErrCode()) {
                case eInvalidKey:             return "eInvalidKey";
                case eAsnObjectNotMatch:      return "eAsnObjectNotMatch";
                case eUnsupportedSerialFmt:   return "eUnsupportedSerialFmt";
                case eUnsupportedCompression: return "eUnsupportedCompression";
                default:                      return CException::GetErrCodeString();
            }
        }

    NCBI_EXCEPTION_DEFAULT(CPrjStorageException, CException);
};


class NCBI_UUDUTIL_EXPORT CProjectStorage : public CObject
{
public:
  
    enum ENC_Compression {
        eNC_Uncompressed = 0,
        eNC_ZlibCompressed,
        eNC_Bzip2Compressed,
        eNC_LzoCompressed
    };

    typedef ENC_Compression TCompressionFormat;
    typedef ESerialDataFormat TDataFormat;

    typedef list< CRef<objects::CSeq_annot> > TAnnots;

    /// Constructor.
    /// This transient version use both NetCahceAPI and NetStorage services 
    /// according to the values of nc_service and password parameters
    /// if nc_service is NetStorage' init string (url-like string) then NetStorage is used for both reading and writing
    /// if nc_service is NetCache's service name then NetCache is used for writing and NetStorage is used for reading
    /// if password is not empty then NetCache is used for both reading/writing
    /// For retrieving a NC blob data, the service name is 
    /// not required. So it is optional for reading. We 
    /// always require a client name for tracking purpose even
    /// though an empty client name will still work fine. But
    /// that is not recommended. For saving data to NetCache,
    /// a valid NetCache service name is required. 

    /// @param password is needed only for reading and writing
    /// password-rotected data blobs.
    /// @param default_flags sets up CNetStorage default_flags
    CProjectStorage(const string& client,
                    const string& nc_service = "",
                    const string& password = "",
                    TNetStorageFlags default_flags = 0);

    /// Set netservice communication time out.
    /// If not set, whatever default timeout set inside CNetCacheAPI
    //  will be used.
    void SetCommTimeout(float sec);

    /// Save a GB project to NetCache.
    /// @param key if a key is given and valid, the existing project
    ///            will be updated using new data
    /// @return a NC key
    /// @throw CPrjStorageException
    ///        Thrown if either the compression format is not supported,
    ///        or data format is not ASN.
    string SaveProject(const objects::IGBProject& project,
                       const string& key = "",
                       TCompressionFormat compression_fmt = eNC_ZlibCompressed,
                       TDataFormat data_fmt = eSerial_AsnBinary, 
                       unsigned int time_to_live = 0,
                       TNetStorageFlags default_flags = 0);

    /// Save a serializable ASN object to NetCache.
    /// @param key if a key is given and valid, the existing project
    ///            will be updated using new data
    /// @return a NC key
    /// @throw CPrjStorageException
    ///        Thrown if either the compression format is not supported,
    ///        or data format is not supported.
    string SaveObject(const CSerialObject& obj,
                      const string& key = "", 
                      TCompressionFormat compression_fmt = eNC_ZlibCompressed, 
                      TDataFormat data_fmt = eSerial_AsnBinary, 
                      unsigned int time_to_live = 0,
                      TNetStorageFlags default_flags = 0);

    /// Save a string to NetCache.
    /// @param key if a is given and valid, the existing content will
    ///            be updated using the input string
    /// @return a NC key
    /// @throw CPrjStorageException
    ///        Thrown if the compression format is not supported.
    string SaveString(const string& str,
                      const string& key = "",
                      TCompressionFormat compression_fmt = eNC_ZlibCompressed,
                      unsigned int time_to_live = 0,
                      TNetStorageFlags default_flags = 0);

    /// Save data from an incoming stream to NetCache.
    /// @param key if a is given and valid, the existing content will
    ///            be updated using the input string
    /// @return a NC key
    /// @throw CPrjStorageException
    ///        Thrown if the compression format is not supported.
    string SaveStream(CNcbiIstream& istream,
                      const string& key,
                      TCompressionFormat compression_fmt = eNC_ZlibCompressed,
                      unsigned int time_to_live = 0,
                      TNetStorageFlags default_flags = 0);

    /// Get a object ostream to allow save data to NetCache.
    /// @param format the asn serialization format is required
    /// @param key if it is valid, the existing content will be
    ///            updated using the input string. If it is empty,
    ///            it will be updated with a new NC key.
    /// @return CObjectOStream
    /// @throw CPrjStorageException
    ///        Thrown if the compression format is not supported,
    ///        or data format is not supported.
    CObjectOStream* AsObjectOStream(TDataFormat data_fmt,
                                    string& key,
                                    TCompressionFormat compression_fmt = eNC_ZlibCompressed,
                                    unsigned int time_to_live = 0);
    /// Save any raw data to NetCache.
    /// No compression and no header
    /// @param key if a is given and valid, the existing content will
    ///            be updated using the input string.
    /// @return a NC key
    string SaveRawData(const void* buf, size_t size,
                       const string& key = "", 
                       unsigned int time_to_live = 0,
                       TNetStorageFlags default_flags = 0);

    /// Duplicate an existing data Blob.
    /// @return a new key
    string Clone(const string& key,
                 unsigned int time_to_live = 0,
                 TNetStorageFlags default_flags = 0);

    /// Retrieve a NC blob as a GB project.
    /// @throw CPrjStorageException
    ///        Thrown if the blob doesn't store a GB project,
    ///        or there is a problem to deserialize the data.
    CIRef<objects::IGBProject> GetProject(const string& key);

    /// Return a NC blob as a list of seq-annots.
    /// @throw CPrjStorageException
    ///        Thrown if there is a problem to deserialize the data.
    TAnnots GetAnnots(const string& key);

    /// Construct and return a serializable ASN object.
    /// When deserializing an ASN object, we need to know what
    /// kind of asn object is going to be serialized.
    /// Currently, only seq-annot and GB-project are supported.
    /// @throw CPrjStorageException
    ///        Thrown if the blob doesn't store compatibel data, or
    ///        if there is a problem to deserialize the data.
    CRef<CSerialObject> GetObject(const string& key);

    /// Create an input stream using the key.
    /// The function complete the following steps:
    ///   1) Connect the the netcache
    ///   2) Get the blob data
    ///   3) Extract the header info if any
    ///   4) Decompress the data if it is compressed
    ///   5) Return a NcbiIstream
    /// If raw is true, it does not check header, just returns
    /// the input stream to the blob.
    auto_ptr<CNcbiIstream> GetIstream(const string& key, bool raw=false);

    /// Create a CObjectIStream.
    /// It is caller's reponsibility to make sure the saved
    /// data indeed are in ASN format.
    auto_ptr<CObjectIStream> GetObjectIstream(const string& key);

    /// Download the data as a string from NetCache with a key
    void GetString(const string& key, string& str);

    /// Download the data as a blob from NetCache with a key
    void GetVector(const string& key, vector<char>& vec);

    /// Delete an existing Blob from NetCache.
    /// @throw CPrjStorageException
    ///        Thrown when the blob doesn't exist.
    void Delete(const string& key);

    /// Check existence of a NetCache blob
    /// @return status
    ///        It returns false if either blob does not exist, or
    ///        there is problem on accessing the data, such as
    ///        incorrect password for password-protected blobs.
    bool Exists(const string& key);


 
private:
    /// Create an output stream.
    auto_ptr<CNcbiOstream> x_GetOutputStream(string& key,
                                             unsigned int time_to_live, 
                                             TNetStorageFlags default_flags,
                                             CNetStorageObject nso);

    /// Get a object ostream to allow save data to NetCache.
    /// @param format the asn serialization format is required
    /// @param key if it is valid, the existing content will be
    ///            updated using the input string. If it is empty,
    ///            it will be updated with a new NC key.
    /// @return CObjectOStream
    /// @throw CPrjStorageException
    ///        Thrown if the compression format is not supported,
    ///        or data format is not supported.
    CObjectOStream* x_GetObjectOStream(TDataFormat data_fmt,
                                       CNetStorageObject nso,
                                       string& key,
                                       TCompressionFormat compression_fmt = eNC_ZlibCompressed,
                                       unsigned int time_to_live = 0,
                                       TNetStorageFlags default_flags = 0);

    /// Check if the object format is valid.
    /// @throw CPrjStorageException if no_throw is false
    ///        thrown when the specified object format is not supported.
    bool x_ValidateSerialFormat(TDataFormat fmt,
                                bool no_throw = false) const;

    /// Check if the object format is valid ASN format.
    /// @throw CPrjStorageException if no_throw is false
    ///        thrown when the specified object format is not ASN.
    bool x_ValidateAsnSerialFormat(TDataFormat fmt,
                                   bool no_throw = false) const;

    /// Check if the compression is valid.
    /// @throw CPrjStorageException if no_throw is false
    ///        thrown when the specified compression flag is not supported.
    bool x_ValidateCompressionFormat(TCompressionFormat fmt,
                                     bool no_throw = false) const;
  
private:
    Uint2              m_Magic;         ///< Data/application related magic number
    Uint2              m_Version;       ///< Version number for stored data blob
    TCompressionFormat m_CmprsFmt;      ///< Data compression format

    /// Data serialization format (one of ESerialDataFormat).
    /// It is relevant only when the blob stores serializable and
    /// deserializable objects such as ASN binary/text, xml and JSON.
    /// It must be one of these:
    ///  eSerial_None         = 0
    ///  eSerial_AsnText      = 1
    ///  eSerial_AsnBinary    = 2
    ///  eSerial_Xml          = 3
    ///  eSerial_Json         = 4
    TDataFormat        m_DataFmt;
    string             m_Password;      ///< For password-protected Blobs
    AutoPtr<CNetCacheAPI> m_NC;
    bool m_HasNetStorage;
    CNetStorage m_NS;
};


END_NCBI_SCOPE

#endif // OBJTOOLS_UUDUTIL__STORAGE_UTIL_HPP
