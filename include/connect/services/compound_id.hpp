#ifndef CONNECT_SERVICES__COMPOUND_ID__HPP
#define CONNECT_SERVICES__COMPOUND_ID__HPP

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
 *   Compound IDs are Base64-encoded string that contain application-specific
 *   information to identify and/or locate objects.
 *
 */

/// @file compound_id.hpp
/// Declarations of CCompoundIDPool, CCompoundID, and CCompoundIDField.

#include "netcomponent.hpp"

BEGIN_NCBI_SCOPE

struct SCompoundIDPoolImpl;     ///< @internal
struct SCompoundIDImpl;         ///< @internal
struct SCompoundIDFieldImpl;    ///< @internal


// Registered CompoundID Classes
enum ECompoundIDClass {
    eCIC_GenericID,
    eCIC_NetCacheKey,
    eCIC_NetScheduleKey,
    eCIC_NetStorageObjectID,

    eCIC_NumberOfClasses
};


// Compound ID Field Types
enum ECompoundIDFieldType {
    eCIT_ID,            /* 64-bit natural number: counter, ID, size, offset   */
    eCIT_Integer,       /* General-purpose 64-bit signed integer              */
    eCIT_ServiceName,   /* LBSM service name                                  */
    eCIT_DatabaseName,  /* Database name (NetSchedule queue, cache name, etc) */
    eCIT_Timestamp,     /* POSIX time_t value (time in seconds since Epoch)   */
    eCIT_Random,        /* 32-bit random integer                              */
    eCIT_IPv4Address,   /* 32-bit IPv4 address                                */
    eCIT_Host,          /* Host name or literal IPv4 or IPv6 address          */
    eCIT_Port,          /* 16-bit port number                                 */
    eCIT_IPv4SockAddr,  /* 32-bit IPv4 address followed by 16-bit port number */
    eCIT_Path,          /* Pathname or URI                                    */
    eCIT_String,        /* Arbitrary single byte character string             */
    eCIT_Boolean,       /* Boolean value                                      */
    eCIT_Flags,         /* Combination of binary flags stored as a number     */
    eCIT_Label,         /* Application-specific field prefix, tag, label      */
    eCIT_Cue,           /* Application-specific numeric field prefix or tag   */
    eCIT_SeqID,         /* Sequence identifier (string)                       */
    eCIT_TaxID,         /* Taxon identifier (unsigned integer)                */
    eCIT_NestedCID,     /* Nested CompoundID                                  */

    eCIT_NumberOfTypes  /* Must be the last element of the enumeration.       */
};


/// Exception class for use by CCompoundIDPool, CCompoundID, and
/// CCompoundIDField.
///
class NCBI_XCONNECT_EXPORT CCompoundIDException : public CException
{
public:
    enum EErrCode {
        eInvalidType,       ///< Field type mismatch.
        eIDTooLong,         ///< ID exceeds length restrictions.
        eInvalidFormat,     ///< Format of the packed ID is not recognized.
        eInvalidDumpSyntax, ///< Dump parsing error.
    };
    virtual const char* GetErrCodeString() const;
    NCBI_EXCEPTION_DEFAULT(CCompoundIDException, CException);
};

class CCompoundID;

/// Compound ID field -- an element of the compound ID that
/// has a type and a value.
///
class NCBI_XCONNECT_EXPORT CCompoundIDField
{
    NCBI_NET_COMPONENT(CompoundIDField);

    /// Return the type of this field.
    ECompoundIDFieldType GetType();

    /// Return the next immediately adjacent field.
    CCompoundIDField GetNextNeighbor();

    /// Return the next field of the same type.
    CCompoundIDField GetNextHomogeneous();

    /// Remove this field from the compound ID that contains it.
    /// This will cause the GetNext*() methods to always return NULL.
    void Remove();

    /// Return the ID value that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_ID.
    Uint8 GetID() const;

    /// Return the integer value that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_Integer.
    Int8 GetInteger() const;

    /// Return the LBSM service name that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_ServiceName.
    string GetServiceName() const;

    /// Return the database name that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_DatabaseName.
    string GetDatabaseName() const;

    /// Return the UNIX timestamp that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_Timestamp.
    Int8 GetTimestamp() const;

    /// Return the random number value that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_Random
    Uint4 GetRandom() const;

    /// Return the 32-bit IP address that this field contains.
    /// @throw CCompoundIDException if GetType() is neither
    /// eCIT_IPv4Address nor eCIT_IPv4SockAddr.
    Uint4 GetIPv4Address() const;

    /// Return the host name or address that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_Host.
    string GetHost() const;

    /// Return the network port number that this field contains.
    /// @throw CCompoundIDException if GetType() is neither
    /// eCIT_Port nor eCIT_IPv4SockAddr.
    Uint2 GetPort() const;

    /// Return the path or URI that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_Path.
    string GetPath() const;

    /// Return the string value that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_String.
    string GetString() const;

    /// Return the Boolean value that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_Boolean.
    bool GetBoolean() const;

    /// Return the combination of binary flags stored in this field.
    /// @throw CCompoundIDException if GetType() != eCIT_Flags.
    Uint8 GetFlags() const;

    /// Return the application-specific tag that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_Label.
    string GetLabel() const;

    /// Return the application-specific numeric tag value
    /// that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_Cue.
    Uint8 GetCue() const;

    /// Return the Sequence ID that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_SeqID.
    string GetSeqID() const;

    /// Return the Taxonomy ID that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_TaxID.
    Uint8 GetTaxID() const;

    /// Return the nested compound ID that this field contains.
    /// @throw CCompoundIDException if GetType() != eCIT_NestedCID.
    const CCompoundID& GetNestedCID() const;
};


/// Base64-encoded ID string that contains extractable typed fields.
///
class NCBI_XCONNECT_EXPORT CCompoundID
{
    NCBI_NET_COMPONENT(CompoundID);

    /// One of the registered ID classes.
    ECompoundIDClass GetClass() const;

    /// Return TRUE if this compound ID contains no fields.
    bool IsEmpty() const;

    /// Return the number of fields this ID contains.
    unsigned GetLength() const;

    /// Return the first field or NULL if this ID is empty.
    CCompoundIDField GetFirstField();

    /// Return the first field of the specified type or NULL
    /// if this compound ID contains no fields of such type.
    CCompoundIDField GetFirst(ECompoundIDFieldType field_type);

    /// Append an eCIT_ID field at the end of this compound ID.
    void AppendID(Uint8 id);

    /// Append an eCIT_Integer field at the end of this compound ID.
    void AppendInteger(Int8 number);

    /// Append an eCIT_ServiceName field at the end of this compound ID.
    void AppendServiceName(const string& service_name);

    /// Append an eCIT_DatabaseName field at the end of this compound ID.
    void AppendDatabaseName(const string& db_name);

    /// Append an eCIT_Timestamp field at the end of this compound ID.
    void AppendTimestamp(Int8 timestamp);

    /// Get the current time and append it as an eCIT_Timestamp field
    /// at the end of this compound ID.
    void AppendCurrentTime();

    /// Append an eCIT_Random field at the end of this compound ID.
    void AppendRandom(Uint4 random_number);

    /// Generate a 32-bit pseudo-random number and append it as an
    /// eCIT_Random field at the end of this compound ID.
    void AppendRandom();

    /// Append an eCIT_IPv4Address field at the end of this compound ID.
    void AppendIPv4Address(Uint4 ipv4_address);

    /// Append an eCIT_Host field at the end of this compound ID.
    void AppendHost(const string& host);

    /// Append an eCIT_Port field at the end of this compound ID.
    void AppendPort(Uint2 port_number);

    /// Append an eCIT_IPv4SockAddr field at the end of this compound ID.
    void AppendIPv4SockAddr(Uint4 ipv4_address, Uint2 port_number);

    /// Append an eCIT_Path field at the end of this compound ID.
    void AppendPath(const string& path);

    /// Append an eCIT_String field at the end of this compound ID.
    void AppendString(const string& string_value);

    /// Append an eCIT_Boolean field at the end of this compound ID.
    void AppendBoolean(bool boolean_value);

    /// Append an eCIT_Flags field at the end of this compound ID.
    void AppendFlags(Uint8 flags);

    /// Append an eCIT_Label field at the end of this compound ID.
    void AppendLabel(const string& tag);

    /// Append an eCIT_Cue field at the end of this compound ID.
    void AppendCue(Uint8 tag);

    /// Append an eCIT_SeqID field at the end of this compound ID.
    void AppendSeqID(const string& seq_id);

    /// Append an eCIT_TaxID field at the end of this compound ID.
    void AppendTaxID(Uint8 tax_id);

    /// Append an eCIT_NestedCID field at the end of this compound ID.
    void AppendNestedCID(const CCompoundID& cid);

    /// Get the field that was added last.
    CCompoundIDField GetLastField();

    /// Pack the ID and return its string representation.
    string ToString();

    /// Dump the contents of the ID in the human-readable format that
    /// can also be parsed by CCompoundIDPool::FromDump().
    string Dump();
};


/// Pool of recycled CCompoundID objects. On some systems, this pool
/// also contains the shared pseudo-random number generator used by
/// CCompoundID::AppendRandom().
///
class NCBI_XCONNECT_EXPORT CCompoundIDPool
{
    NCBI_NET_COMPONENT_WITH_DEFAULT_CTOR(CompoundIDPool);

    /// Construct a new pool of CompoundID objects.
    CCompoundIDPool();

    /// Create and return a new CCompoundID objects.
    CCompoundID NewID(ECompoundIDClass new_id_class);

    /// Unpack the base64-encoded ID and return a CCompoundID
    /// object for field extraction.
    CCompoundID FromString(const string& cid);

    /// Restore the compound ID from its textual representation
    /// created by CCompoundID::Dump().
    CCompoundID FromDump(const string& cid_dump);
};


END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES__COMPOUND_ID__HPP */
