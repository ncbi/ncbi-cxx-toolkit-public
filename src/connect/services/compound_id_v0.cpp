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

/// @file compound_id_v0.cpp
/// A simple implementation of CompoundID packing/unpacking procedures.

#include <ncbi_pch.hpp>

#include "compound_id_impl.hpp"

#include <connect/ncbi_socket.hpp>

BEGIN_NCBI_SCOPE

#define CIT_ID_FIELD_CODE '!'
#define CIT_INTEGER_FIELD_CODE '+' or '-'
#define CIT_SERVICE_NAME_FIELD_CODE 'S'
#define CIT_DATABASE_NAME_FIELD_CODE 'D'
#define CIT_TIMESTAMP_FIELD_CODE '@'
#define CIT_RANDOM_FIELD_CODE 'R'
#define CIT_IPV4_ADDRESS_FIELD_CODE 'A'
#define CIT_HOST_FIELD_CODE 'H'
#define CIT_PORT_FIELD_CODE ':'
#define CIT_IPV4_SOCK_ADDR_FIELD_CODE '&'
#define CIT_OBJECTREF_FIELD_CODE '/'
#define CIT_STRING_FIELD_CODE '"'
#define CIT_BOOLEAN_FIELD_CODE 'Y' or 'N'
#define CIT_FLAGS_FIELD_CODE '|'
#define CIT_LABEL_FIELD_CODE '$'
#define CIT_CUE_FIELD_CODE '#'
#define CIT_SEQ_ID_FIELD_CODE 'Q'
#define CIT_TAX_ID_FIELD_CODE 'X'
#define CIT_NESTED_CID_FIELD_CODE '{' and '}'

static const char s_TypePrefix[eCIT_NumberOfTypes] = {
    /* eCIT_ID                  */  CIT_ID_FIELD_CODE,
    /* eCIT_Integer             */  0,
    /* eCIT_ServiceName         */  CIT_SERVICE_NAME_FIELD_CODE,
    /* eCIT_DatabaseName        */  CIT_DATABASE_NAME_FIELD_CODE,
    /* eCIT_Timestamp           */  CIT_TIMESTAMP_FIELD_CODE,
    /* eCIT_Random              */  CIT_RANDOM_FIELD_CODE,
    /* eCIT_IPv4Address         */  CIT_IPV4_ADDRESS_FIELD_CODE,
    /* eCIT_Host                */  CIT_HOST_FIELD_CODE,
    /* eCIT_Port                */  CIT_PORT_FIELD_CODE,
    /* eCIT_IPv4SockAddr        */  CIT_IPV4_SOCK_ADDR_FIELD_CODE,
    /* eCIT_ObjectRef           */  CIT_OBJECTREF_FIELD_CODE,
    /* eCIT_String              */  CIT_STRING_FIELD_CODE,
    /* eCIT_Boolean             */  0,
    /* eCIT_Flags               */  CIT_FLAGS_FIELD_CODE,
    /* eCIT_Label               */  CIT_LABEL_FIELD_CODE,
    /* eCIT_Cue                 */  CIT_CUE_FIELD_CODE,
    /* eCIT_SeqID               */  CIT_SEQ_ID_FIELD_CODE,
    /* eCIT_TaxID               */  CIT_TAX_ID_FIELD_CODE,
    /* eCIT_NestedCID           */  0
};

/// Save a 8-byte unsigned integer to a variable-length array of
/// up to 9 bytes.
///
/// @param dst
///     Pointer to the receiving buffer.
/// @param dst_size
///     Size of the receiving buffer in bytes. If the buffer is too
///     small to store a particular number, the buffer will not be
///     changed, but the function will return the value that would
///     have been returned if the buffer was big enough.  The caller
///     must check whether the returned value is greater than
///     'dst_size'.  This, of course, is not necessary if 'dst_size'
///     is guaranteed to be at least 9 bytes long, which is the
///     maximum possible packed length of an 8-byte number.
/// @param number
///     The integer to pack.
///
/// @return
///     The number of bytes stored in the 'dst' buffer. Or the
///     number of bytes that would have been stored if the buffer
///     was big enough (see the dscription of 'dst_size').
///
/// @see g_UnpackInteger
///
unsigned g_PackInteger(void* dst, size_t dst_size, Uint8 number)
{
    if ((signed char) number >= 0 && (unsigned char) number == number) {
        if (dst_size > 0)
            *(unsigned char*) dst = (unsigned char) number;
        return 1;
    }

    unsigned char buffer[sizeof(number)];
    unsigned char *ptr = buffer + sizeof(buffer) - 1;

    *ptr = (unsigned char) number;
    unsigned length = 1;

    Uint8 mask = 0x7F;
    Uint8 mask_shift;

    while ((number >>= 8) > (mask_shift = (mask >> 1))) {
        *--ptr = (unsigned char) number;
        ++length;
        mask = mask_shift;
    }

    if (dst_size > length) {
        *(unsigned char*) dst = (unsigned char) (~mask | number);

        memcpy(((unsigned char*) dst) + 1, ptr, length);
    }

    return length + 1;
}

#define REC_DATA \
{ \
    REC(2, 0x00), REC(2, 0x01), REC(2, 0x02), REC(2, 0x03), REC(2, 0x04), \
    REC(2, 0x05), REC(2, 0x06), REC(2, 0x07), REC(2, 0x08), REC(2, 0x09), \
    REC(2, 0x0A), REC(2, 0x0B), REC(2, 0x0C), REC(2, 0x0D), REC(2, 0x0E), \
    REC(2, 0x0F), REC(2, 0x10), REC(2, 0x11), REC(2, 0x12), REC(2, 0x13), \
    REC(2, 0x14), REC(2, 0x15), REC(2, 0x16), REC(2, 0x17), REC(2, 0x18), \
    REC(2, 0x19), REC(2, 0x1A), REC(2, 0x1B), REC(2, 0x1C), REC(2, 0x1D), \
    REC(2, 0x1E), REC(2, 0x1F), REC(2, 0x20), REC(2, 0x21), REC(2, 0x22), \
    REC(2, 0x23), REC(2, 0x24), REC(2, 0x25), REC(2, 0x26), REC(2, 0x27), \
    REC(2, 0x28), REC(2, 0x29), REC(2, 0x2A), REC(2, 0x2B), REC(2, 0x2C), \
    REC(2, 0x2D), REC(2, 0x2E), REC(2, 0x2F), REC(2, 0x30), REC(2, 0x31), \
    REC(2, 0x32), REC(2, 0x33), REC(2, 0x34), REC(2, 0x35), REC(2, 0x36), \
    REC(2, 0x37), REC(2, 0x38), REC(2, 0x39), REC(2, 0x3A), REC(2, 0x3B), \
    REC(2, 0x3C), REC(2, 0x3D), REC(2, 0x3E), REC(2, 0x3F), REC(3, 0x00), \
    REC(3, 0x01), REC(3, 0x02), REC(3, 0x03), REC(3, 0x04), REC(3, 0x05), \
    REC(3, 0x06), REC(3, 0x07), REC(3, 0x08), REC(3, 0x09), REC(3, 0x0A), \
    REC(3, 0x0B), REC(3, 0x0C), REC(3, 0x0D), REC(3, 0x0E), REC(3, 0x0F), \
    REC(3, 0x10), REC(3, 0x11), REC(3, 0x12), REC(3, 0x13), REC(3, 0x14), \
    REC(3, 0x15), REC(3, 0x16), REC(3, 0x17), REC(3, 0x18), REC(3, 0x19), \
    REC(3, 0x1A), REC(3, 0x1B), REC(3, 0x1C), REC(3, 0x1D), REC(3, 0x1E), \
    REC(3, 0x1F), REC(4, 0x00), REC(4, 0x01), REC(4, 0x02), REC(4, 0x03), \
    REC(4, 0x04), REC(4, 0x05), REC(4, 0x06), REC(4, 0x07), REC(4, 0x08), \
    REC(4, 0x09), REC(4, 0x0A), REC(4, 0x0B), REC(4, 0x0C), REC(4, 0x0D), \
    REC(4, 0x0E), REC(4, 0x0F), REC(5, 0x00), REC(5, 0x01), REC(5, 0x02), \
    REC(5, 0x03), REC(5, 0x04), REC(5, 0x05), REC(5, 0x06), REC(5, 0x07), \
    REC(6, 0x00), REC(6, 0x01), REC(6, 0x02), REC(6, 0x03), REC(7, 0x00), \
    REC(7, 0x01), REC(8, 0x00), REC(9, 0x00) \
}

#define REC(length, bits) {length, (bits << 8)}

struct SCodeRec
{
    unsigned length;
    Uint8 bits;
} static const s_CodeRec[128] = REC_DATA;

/// Unpack an unsigned integer from a byte array prepared by
/// g_PackInteger().
///
/// @param src
///     The source byte array.
/// @param src_size
///     Number of bytes available in 'src'.
/// @param number
///     Pointer to a variable where the integer originally passed
///     to g_PackInteger() will be stored. If 'src_size' bytes is
///     not enough to reconstitute the integer, the variable will
///     not be changed.
///
/// @return
///     The length of the packed integer stored in 'src' regardless
///     of whether the integer was actually unpacked or not (in case
///     if 'src_size' was less that the value that g_PackInteger()
///     originally returned, see the description of the 'number'
///     argument).
///
/// @see g_PackInteger
///
unsigned g_UnpackInteger(const void* src, size_t src_size, Uint8* number)
{
    const unsigned char* ptr = (const unsigned char*) src;

    if (src_size == 0)
        return 0;

    _ASSERT(number);

    if ((signed char) *ptr >= 0) {
        *number = (Uint8) *ptr;
        return 1;
    }

    SCodeRec acc = s_CodeRec[*ptr - 0x80];

    if (src_size >= acc.length) {
        unsigned count = acc.length - 1;

        for (;;) {
            acc.bits += *++ptr;
            if (--count == 0) {
                *number = acc.bits;
                break;
            }
            acc.bits <<= 8;
        }
    }

    return acc.length;
}

struct SIDPackingBuffer
{
    char m_Buffer[1024];
    char* m_Ptr;
    size_t m_FreeSpace;

    void Overflow();
    void PackCode(char code);
    void PackTypePrefix(ECompoundIDFieldType field_type)
    {
        PackCode(s_TypePrefix[field_type]);
    }
    void PackNumber(Uint8 number);
    void PackUint4(Uint4 number);
    void PackPort(Uint2 port);
    void PackCompoundID(SCompoundIDImpl* cid_impl);

    SIDPackingBuffer() : m_Ptr(m_Buffer), m_FreeSpace(sizeof(m_Buffer)) {}
};

void SIDPackingBuffer::Overflow()
{
    NCBI_THROW(CCompoundIDException, eIDTooLong,
            "Cannot create CompoundID: buffer overflow");
}

void SIDPackingBuffer::PackCode(char code)
{
    if (m_FreeSpace == 0)
        Overflow();
    *m_Ptr++ = code;
    --m_FreeSpace;
}

void SIDPackingBuffer::PackNumber(Uint8 number)
{
    unsigned packed_size = g_PackInteger(m_Ptr, m_FreeSpace, number);
    if (packed_size > m_FreeSpace)
        Overflow();
    m_Ptr += packed_size;
    m_FreeSpace -= packed_size;
}

void SIDPackingBuffer::PackUint4(Uint4 number)
{
    if (m_FreeSpace < 4)
        Overflow();
    memcpy(m_Ptr, &number, 4);
    m_Ptr += 4;
    m_FreeSpace -= 4;
}

void SIDPackingBuffer::PackPort(Uint2 port)
{
    if (m_FreeSpace < 2)
        Overflow();
    port = CSocketAPI::HostToNetShort(port);
    memcpy(m_Ptr, &port, 2);
    m_Ptr += 2;
    m_FreeSpace -= 2;
}

void SIDPackingBuffer::PackCompoundID(SCompoundIDImpl* cid_impl)
{
    PackNumber(cid_impl->m_Class);

    SCompoundIDFieldImpl* field = cid_impl->m_FieldList.m_Head;

    while (field != NULL) {
        switch (field->m_Type) {
        case eCIT_ID:
        case eCIT_Flags:
        case eCIT_Cue:
        case eCIT_TaxID:
            PackTypePrefix(field->m_Type);
            PackNumber(field->m_Uint8Value);
            break;
        case eCIT_Integer:
            if (field->m_Int8Value >= 0) {
                PackCode('+');
                PackNumber(field->m_Int8Value);
            } else {
                PackCode('-');
                PackNumber(-field->m_Int8Value);
            }
            break;
        case eCIT_ServiceName:
        case eCIT_DatabaseName:
        case eCIT_Host:
        case eCIT_ObjectRef:
        case eCIT_String:
        case eCIT_Label:
        case eCIT_SeqID:
            PackTypePrefix(field->m_Type);
            PackNumber(field->m_StringValue.length());
            if (m_FreeSpace < field->m_StringValue.length())
                Overflow();
            memcpy(m_Ptr, field->m_StringValue.data(),
                    field->m_StringValue.length());
            m_Ptr += field->m_StringValue.length();
            m_FreeSpace -= field->m_StringValue.length();
            break;
        case eCIT_Timestamp:
            PackCode(CIT_TIMESTAMP_FIELD_CODE);
            PackNumber(field->m_Int8Value);
            break;
        case eCIT_Random:
            PackCode(CIT_RANDOM_FIELD_CODE);
            PackUint4(CSocketAPI::HostToNetLong(field->m_Uint4Value));
            break;
        case eCIT_IPv4Address:
            PackCode(CIT_IPV4_ADDRESS_FIELD_CODE);
            PackUint4(field->m_IPv4SockAddr.m_IPv4Addr);
            break;
        case eCIT_Port:
            PackCode(CIT_PORT_FIELD_CODE);
            PackPort(field->m_IPv4SockAddr.m_Port);
            break;
        case eCIT_IPv4SockAddr:
            PackCode(CIT_IPV4_SOCK_ADDR_FIELD_CODE);
            PackUint4(field->m_IPv4SockAddr.m_IPv4Addr);
            PackPort(field->m_IPv4SockAddr.m_Port);
            break;
        case eCIT_Boolean:
            PackCode(field->m_BoolValue ? 'Y' : 'N');
            break;
        case eCIT_NestedCID:
            PackCode('{');
            PackCompoundID(field->m_NestedCID);
            PackCode('}');
            break;

        case eCIT_NumberOfTypes:
            _ASSERT(0);
            break;
        }
        field = cid_impl->m_FieldList.GetNext(field);
    }
}

string SCompoundIDImpl::PackV0()
{
    if (m_Dirty) {
        SIDPackingBuffer buffer;

        buffer.PackCompoundID(this);

        g_PackID(buffer.m_Buffer, buffer.m_Ptr - buffer.m_Buffer, m_PackedID);

        m_Dirty = false;
    }

    return m_PackedID;
}

struct SIDUnpacking
{
    SIDUnpacking(const string& packed_id) : m_PackedID(packed_id)
    {
        if (!g_UnpackID(packed_id, m_BinaryID)) {
            NCBI_THROW_FMT(CCompoundIDException, eInvalidFormat,
                    "Invalid CompoundID format: " << packed_id);
        }
        m_Ptr = m_BinaryID.data();
        m_RemainingBytes = m_BinaryID.length();
    }

    Uint8 ExtractNumber();
    char ExtractCode();
    string ExtractString();
    Uint4 ExtractUint4();
    Uint2 ExtractPort();
    CCompoundID ExtractCID(SCompoundIDPoolImpl* pool_impl);

    string m_PackedID;
    string m_BinaryID;
    const char* m_Ptr;
    size_t m_RemainingBytes;
};

Uint8 SIDUnpacking::ExtractNumber()
{
    Uint8 number = 0;

    unsigned number_len = g_UnpackInteger(m_Ptr, m_RemainingBytes, &number);

    if (number_len > m_RemainingBytes) {
        NCBI_THROW_FMT(CCompoundIDException, eInvalidFormat,
                "Invalid CompoundID format: " << m_PackedID);
    }

    m_Ptr += number_len;
    m_RemainingBytes -= number_len;

    return number;
}

char SIDUnpacking::ExtractCode()
{
    --m_RemainingBytes;
    return *m_Ptr++;
}

string SIDUnpacking::ExtractString()
{
    Uint8 string_len = ExtractNumber();

    if (m_RemainingBytes < string_len) {
        NCBI_THROW_FMT(CCompoundIDException, eInvalidFormat,
                "Invalid CompoundID format: " << m_PackedID);
    }

    string result(m_Ptr, (string::size_type) string_len);

    m_Ptr += string_len;
    m_RemainingBytes -= (string::size_type) string_len;

    return result;
}

Uint4 SIDUnpacking::ExtractUint4()
{
    if (m_RemainingBytes < 4) {
        NCBI_THROW_FMT(CCompoundIDException, eInvalidFormat,
                "Invalid CompoundID format: " << m_PackedID);
    }

    Uint4 number;

    memcpy(&number, m_Ptr, 4);

    m_Ptr += 4;
    m_RemainingBytes -= 4;

    return number;
}

Uint2 SIDUnpacking::ExtractPort()
{
    if (m_RemainingBytes < 2) {
        NCBI_THROW_FMT(CCompoundIDException, eInvalidFormat,
                "Invalid CompoundID format: " << m_PackedID);
    }

    Uint2 port;

    memcpy(&port, m_Ptr, 2);

    m_Ptr += 2;
    m_RemainingBytes -= 2;

    return CSocketAPI::NetToHostShort(port);
}

CCompoundID SIDUnpacking::ExtractCID(SCompoundIDPoolImpl* pool_impl)
{
    Uint8 id_class = ExtractNumber();

    if (id_class >= eCIC_NumberOfClasses) {
        NCBI_THROW_FMT(CCompoundIDException, eInvalidFormat,
                "Unknown CompoundID class: " << m_PackedID);
    }

    CCompoundID new_cid(pool_impl->m_CompoundIDPool.Alloc());
    new_cid->Reset(pool_impl, (ECompoundIDClass) id_class);

    while (m_RemainingBytes > 0) {
        switch (ExtractCode()) {
        case CIT_ID_FIELD_CODE:
            new_cid.AppendID(ExtractNumber());
            break;
        case '+':
            new_cid.AppendInteger(ExtractNumber());
            break;
        case '-':
            new_cid.AppendInteger(-(Int8) ExtractNumber());
            break;
        case CIT_SERVICE_NAME_FIELD_CODE:
            new_cid.AppendServiceName(ExtractString());
            break;
        case CIT_DATABASE_NAME_FIELD_CODE:
            new_cid.AppendDatabaseName(ExtractString());
            break;
        case CIT_TIMESTAMP_FIELD_CODE:
            new_cid.AppendTimestamp(ExtractNumber());
            break;
        case CIT_RANDOM_FIELD_CODE:
            new_cid.AppendRandom(CSocketAPI::NetToHostLong(ExtractUint4()));
            break;
        case CIT_IPV4_ADDRESS_FIELD_CODE:
            new_cid.AppendIPv4Address(ExtractUint4());
            break;
        case CIT_HOST_FIELD_CODE:
            new_cid.AppendHost(ExtractString());
            break;
        case CIT_PORT_FIELD_CODE:
            new_cid.AppendPort(ExtractPort());
            break;
        case CIT_IPV4_SOCK_ADDR_FIELD_CODE:
            {
                Uint4 host = ExtractUint4();
                Uint2 port = ExtractPort();
                new_cid.AppendIPv4SockAddr(host, port);
            }
            break;
        case CIT_OBJECTREF_FIELD_CODE:
            new_cid.AppendObjectRef(ExtractString());
            break;
        case CIT_STRING_FIELD_CODE:
            new_cid.AppendString(ExtractString());
            break;
        case 'Y':
            new_cid.AppendBoolean(true);
            break;
        case 'N':
            new_cid.AppendBoolean(false);
            break;
        case CIT_FLAGS_FIELD_CODE:
            new_cid.AppendFlags(ExtractNumber());
            break;
        case CIT_LABEL_FIELD_CODE:
            new_cid.AppendLabel(ExtractString());
            break;
        case CIT_CUE_FIELD_CODE:
            new_cid.AppendCue(ExtractNumber());
            break;
        case CIT_SEQ_ID_FIELD_CODE:
            new_cid.AppendSeqID(ExtractString());
            break;
        case CIT_TAX_ID_FIELD_CODE:
            new_cid.AppendTaxID(ExtractNumber());
            break;
        case '{':
            new_cid.AppendNestedCID(ExtractCID(pool_impl));
            break;
        case '}':
            return new_cid;
        }
    }

    return new_cid;
}

CCompoundID SCompoundIDPoolImpl::UnpackV0(const string& packed_id)
{
    SIDUnpacking unpacking(packed_id);

    CCompoundID new_cid(unpacking.ExtractCID(this));

    new_cid->m_PackedID = packed_id;
    new_cid->m_Dirty = false;

    return new_cid;
}

END_NCBI_SCOPE
