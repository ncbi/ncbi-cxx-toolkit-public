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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  1999/07/22 17:33:57  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.10  1999/07/21 20:02:57  vasilche
* Added embedding of ASN.1 binary output from ToolKit to our binary format.
* Fixed bugs with storing pointers into binary ASN.1
*
* Revision 1.9  1999/07/21 14:20:08  vasilche
* Added serialization of bool.
*
* Revision 1.8  1999/07/19 15:50:38  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.7  1999/07/09 20:27:08  vasilche
* Fixed some bugs
*
* Revision 1.6  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.5  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.4  1999/07/02 21:32:00  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.3  1999/07/01 17:55:35  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.2  1999/06/30 16:05:01  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.1  1999/06/24 14:44:59  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/memberid.hpp>
#include <asn.h>

BEGIN_NCBI_SCOPE

using namespace CObjectStreamAsnBinaryDefs;

CObjectOStreamAsnBinary::CObjectOStreamAsnBinary(CNcbiOstream& out)
    : m_Output(out)
{
}

CObjectOStreamAsnBinary::~CObjectOStreamAsnBinary(void)
{
}

inline
void CObjectOStreamAsnBinary::WriteByte(TByte byte)
{
    m_Output.put(byte);
}

inline
void CObjectOStreamAsnBinary::WriteBytes(const char* bytes, size_t size)
{
    m_Output.write(bytes, size);
}

template<typename T>
inline
void WriteBytesOf(CObjectOStreamAsnBinary& out, const T& value)
{
    for ( size_t shift = (sizeof(value) - 1) * 8; shift != 0; shift -= 8 ) {
        out.WriteByte(value >> shift);
    }
    out.WriteByte(value);
}

inline
void CObjectOStreamAsnBinary::WriteShortTag(EClass c, bool constructed,
                                            TTag tag)
{
    WriteByte((c << 6) | (constructed << 5) | tag);
}

inline
void CObjectOStreamAsnBinary::WriteSysTag(ETag tag)
{
    WriteShortTag(eUniversal, false, tag);
}

void CObjectOStreamAsnBinary::WriteTag(EClass c, bool constructed, TTag tag)
{
    if ( tag < 0 )
        THROW1_TRACE(runtime_error, "negative tag number");

    if ( tag < eLongTag ) {
        // short form
        WriteShortTag(c, constructed, tag);
    }
    else {
        // long form
        WriteShortTag(c, constructed, eLongTag);
        bool write = false;
        size_t shift;
        for ( shift = (sizeof(TTag) * 8 - 1) / 7 * 7; shift != 0; shift -= 7 ) {
            TByte bits = tag >> shift;
            if ( !write ) {
                if ( bits & 0x7f == 0 )
                    continue;
                write = true;
            }
            WriteByte(bits | 0x80);
        }
        WriteByte(tag & 0x7f);
    }
}

void CObjectOStreamAsnBinary::WriteClassTag(TTypeInfo typeInfo)
{
    const string& tag = typeInfo->GetName();
    if ( tag.empty() )
        THROW1_TRACE(runtime_error, "empty tag string");

    if ( tag[0] <= eLongTag )
        THROW1_TRACE(runtime_error, "bad tag string start");

    // long form
    WriteShortTag(eApplication, true, eLongTag);
    SIZE_TYPE last = tag.size() - 1;
    for ( SIZE_TYPE i = 0; i < last; ++i ) {
        char c = tag[i];
        if ( (c & 0x80) != 0 )
            THROW1_TRACE(runtime_error, "bad tag string char");
        WriteByte(c);
    }
    char c = tag[last];
    if ( (c & 0x80) != 0 )
        THROW1_TRACE(runtime_error, "bad tag string char");
    WriteByte(c | 0x80);
}

inline
void CObjectOStreamAsnBinary::WriteIndefiniteLength(void)
{
    WriteByte(0x80);
}

inline
void CObjectOStreamAsnBinary::WriteShortLength(size_t length)
{
    WriteByte(length);
}

void CObjectOStreamAsnBinary::WriteLength(size_t length)
{
    if ( length < 0 )
        THROW1_TRACE(runtime_error, "negative length");

    if ( length <= 127 ) {
        // short form
        WriteShortLength(length);
    }
    else if ( length <= 0xff ) {
        // long form 1 byte
        WriteByte(0x81);
        WriteByte(length);
    }
    else if ( length <= 0xffff ) {
        WriteByte(0x82);
        WriteByte(length >> 8);
        WriteByte(length);
    }
    else {
        WriteByte(0x80 | sizeof(length));
        WriteBytesOf(*this, length);
    }
}

void CObjectOStreamAsnBinary::WriteEndOfContent(void)
{
    WriteSysTag(eNone);
    WriteShortLength(0);
}

void CObjectOStreamAsnBinary::WriteNull(void)
{
    WriteSysTag(eNull);
    WriteShortLength(0);
}

void CObjectOStreamAsnBinary::WriteStd(const bool& data)
{
    WriteSysTag(eBoolean);
    WriteShortLength(1);
    WriteByte(data);
}

template<typename T>
static inline
void WriteNumberValue(CObjectOStreamAsnBinary& out, const T& data)
{
    if ( data >= -0x80 && data < 0x80 ) {
        // one byte
        out.WriteShortLength(1);
        out.WriteByte(data);
    }
    else if ( data >= -0x8000 && data < 0x8000 ) {
        // two bytes
        out.WriteShortLength(2);
        out.WriteByte(data >> 8);
        out.WriteByte(data);
    }
    else if ( data >= -0x800000 && data < 0x800000 ) {
        // three bytes
        out.WriteShortLength(3);
        out.WriteByte(data >> 16);
        out.WriteByte(data >> 8);
        out.WriteByte(data);
    }
    else if ( T(-1) >= 0 && (data & (1 << (sizeof(T) * 8 - 1))) != 0 ) {
        // full length unsigned - and doesn't fit in signed place
        out.WriteShortLength(sizeof(data) + 1);
        out.WriteByte(0);
        WriteBytesOf(out, data);
    }
    else {
        // full length signed
        out.WriteShortLength(sizeof(data));
        WriteBytesOf(out, data);
    }
}

template<typename T>
static inline
void WriteStdNumber(CObjectOStreamAsnBinary& out, const T& data)
{
    out.WriteSysTag(eInteger);
    WriteNumberValue(out, data);
}

void CObjectOStreamAsnBinary::WriteStd(const char& data)
{
    WriteSysTag(eGeneralString);
    WriteShortLength(1);
    WriteByte(data);
}

void CObjectOStreamAsnBinary::WriteStd(const signed char& data)
{
    WriteStdNumber(*this, data);
}

void CObjectOStreamAsnBinary::WriteStd(const unsigned char& data)
{
    WriteStdNumber(*this, data);
}

void CObjectOStreamAsnBinary::WriteStd(const short& data)
{
    WriteStdNumber(*this, data);
}

void CObjectOStreamAsnBinary::WriteStd(const unsigned short& data)
{
    WriteStdNumber(*this, data);
}

void CObjectOStreamAsnBinary::WriteStd(const int& data)
{
    WriteStdNumber(*this, data);
}

void CObjectOStreamAsnBinary::WriteStd(const unsigned& data)
{
    WriteStdNumber(*this, data);
}

void CObjectOStreamAsnBinary::WriteStd(const long& data)
{
    WriteStdNumber(*this, data);
}

void CObjectOStreamAsnBinary::WriteStd(const unsigned long& data)
{
    WriteStdNumber(*this, data);
}

void CObjectOStreamAsnBinary::WriteStd(const float& data)
{
    WriteSysTag(eReal);
    char buffer[128];
    sprintf(buffer, "%.7g", data);
    size_t length = strlen(buffer);
    WriteShortLength(length + 1);
    WriteByte(eDecimal);
    WriteBytes(buffer, length);
}

void CObjectOStreamAsnBinary::WriteStd(const double& data)
{
    WriteSysTag(eReal);
    char buffer[128];
    sprintf(buffer, "%.15g", data);
    size_t length = strlen(buffer);
    WriteShortLength(length + 1);
    WriteByte(eDecimal);
    WriteBytes(buffer, length);
}

void CObjectOStreamAsnBinary::WriteString(const string& str)
{
    WriteSysTag(eVisibleString);
    size_t length = str.size();
    WriteLength(length);
    WriteBytes(str.data(), length);
}

void CObjectOStreamAsnBinary::WriteCString(const char* str)
{
	if ( str == 0 ) {
		WriteSysTag(eNull);
		WriteShortLength(0);
	}
	else {
		WriteSysTag(eVisibleString);
	    size_t length = strlen(str);
		WriteLength(length);
		WriteBytes(str, length);
	}
}

void CObjectOStreamAsnBinary::WriteMemberPrefix(const CMemberId& id)
{
    WriteTag(eApplication, true, eMemberReference);
    WriteIndefiniteLength();
    WriteString(id.GetName());
}

void CObjectOStreamAsnBinary::WriteMemberSuffix(const CMemberId& )
{
    WriteEndOfContent();
}

void CObjectOStreamAsnBinary::WriteObjectReference(TIndex index)
{
    WriteTag(eApplication, false, eObjectReference);
    WriteNumberValue(*this, index);
}

void CObjectOStreamAsnBinary::WriteNullPointer(void)
{
    WriteSysTag(eNull);
    WriteShortLength(0);
}

void CObjectOStreamAsnBinary::WriteOther(TConstObjectPtr object,
                                         TTypeInfo typeInfo)
{
    WriteClassTag(typeInfo);
    WriteIndefiniteLength();
    WriteString(typeInfo->GetName());
    WriteExternalObject(object, typeInfo);
    WriteEndOfContent();
}

void CObjectOStreamAsnBinary::VBegin(Block& block)
{
    WriteShortTag(eUniversal, true, block.RandomOrder()? eSet: eSequence);
    WriteIndefiniteLength();
}

void CObjectOStreamAsnBinary::VEnd(const Block& )
{
    WriteEndOfContent();
}

void CObjectOStreamAsnBinary::StartMember(Member& , const CMemberId& id)
{
    WriteTag(eContextSpecific, true, id.GetTag());
    WriteIndefiniteLength();
}

void CObjectOStreamAsnBinary::EndMember(const Member& )
{
    WriteEndOfContent();
}

void CObjectOStreamAsnBinary::Begin(const ByteBlock& block)
{
	WriteSysTag(eOctetString);
	WriteLength(block.GetLength());
}

void CObjectOStreamAsnBinary::WriteBytes(const ByteBlock& ,
                                         const char* bytes, size_t length)
{
	WriteBytes(bytes, length);
}

unsigned CObjectOStreamAsnBinary::GetAsnFlags(void)
{
    return ASNIO_BIN;
}

void CObjectOStreamAsnBinary::AsnWrite(AsnIo& , const char* data, size_t length)
{
    WriteBytes(data, length);
}

END_NCBI_SCOPE
