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
* Revision 1.20  1999/09/23 21:16:09  vasilche
* Removed dependance on asn.h
*
* Revision 1.19  1999/09/23 18:57:01  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.18  1999/09/22 20:11:55  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.17  1999/08/16 16:08:31  vasilche
* Added ENUMERATED type.
*
* Revision 1.16  1999/08/13 20:22:58  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.15  1999/08/13 15:53:52  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.14  1999/07/26 18:31:39  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.13  1999/07/22 20:36:38  vasilche
* Fixed 'using namespace' declaration for MSVC.
*
* Revision 1.12  1999/07/22 19:40:59  vasilche
* Fixed bug with complex object graphs (pointers to members of other objects).
*
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
#if HAVE_NCBI_C
# include <asn.h>
#endif

using namespace NCBI_NS_NCBI::CObjectStreamAsnBinaryDefs;

BEGIN_NCBI_SCOPE

CObjectOStreamAsnBinary::CObjectOStreamAsnBinary(CNcbiOstream& out)
    : m_Output(out)
{
#if CHECK_STREAM_INTEGRITY
    m_CurrentPosition = 0;
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = INT_MAX;
#endif
}

CObjectOStreamAsnBinary::~CObjectOStreamAsnBinary(void)
{
#if CHECK_STREAM_INTEGRITY
    if ( !m_Limits.empty() || m_CurrentTagState != eTagStart )
        THROW1_TRACE(runtime_error, "CObjectOStreamAsnBinary not finished");
#endif
}

#if CHECK_STREAM_INTEGRITY
inline
void CObjectOStreamAsnBinary::StartTag(TByte code)
{
    m_Limits.push(m_CurrentTagLimit);
    m_CurrentTagCode = code;
    m_CurrentTagPosition = m_CurrentPosition;
    m_CurrentTagState = (code & 0x1f) == eLongTag? eTagValue: eLengthStart;
}

inline
void CObjectOStreamAsnBinary::EndTag(void)
{
    if ( m_Limits.empty() )
        THROW1_TRACE(runtime_error, "too many tag ends");
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = m_Limits.top();
    m_Limits.pop();
}

inline
void CObjectOStreamAsnBinary::SetTagLength(size_t length)
{
    size_t limit = m_CurrentPosition + 1 + length;
    if ( limit > m_CurrentTagLimit )
        THROW1_TRACE(runtime_error, "tag will overflow enclosing tag");
    else
        m_CurrentTagLimit = limit;
    if ( m_CurrentTagCode & 0x20 ) // constructed
        m_CurrentTagState = eTagStart;
    else
        m_CurrentTagState = eData;
    if ( length == 0 )
        EndTag();
}
#endif

#if !CHECK_STREAM_INTEGRITY
inline
#endif
void CObjectOStreamAsnBinary::WriteByte(TByte byte)
{
#if CHECK_STREAM_INTEGRITY
    //_TRACE("WriteByte: " << unsigned(byte));
    if ( m_CurrentPosition >= m_CurrentTagLimit )
        THROW1_TRACE(runtime_error, "tag size overflow");
    switch ( m_CurrentTagState ) {
    case eTagStart:
        StartTag(byte);
        break;
    case eTagValue:
        if ( byte & 0x80 )
            m_CurrentTagState = eLengthStart;
        break;
    case eLengthStart:
        if ( byte == 0 ) {
            SetTagLength(0);
            if ( m_CurrentTagCode == 0 )
                EndTag();
        }
        else if ( byte == 0x80 ) {
            if ( !(m_CurrentTagCode & 0x20) ) {
                THROW1_TRACE(runtime_error,
                             "cannot use indefinite form for primitive tag");
            }
            m_CurrentTagState = eTagStart;
        }
        else if ( byte < 0x80 ) {
            SetTagLength(byte);
        }
        else {
            m_CurrentTagLengthSize = byte - 0x80;
            if ( m_CurrentTagLengthSize > sizeof(size_t) )
                THROW1_TRACE(runtime_error, "too big length");
            m_CurrentTagState = eLengthValueFirst;
        }
        break;
    case eLengthValueFirst:
        if ( byte == 0 )
            THROW1_TRACE(runtime_error, "first byte of length is zero");
        if ( --m_CurrentTagLengthSize == 0 ) {
            SetTagLength(byte);
		}
		else {
	        m_CurrentTagLength = byte;
			m_CurrentTagState = eLengthValue;
		}
        break;
        // fall down to next case (no break needed)
    case eLengthValue:
        m_CurrentTagLength = (m_CurrentTagLength << 8) | byte;
        if ( --m_CurrentTagLengthSize == 0 )
            SetTagLength(m_CurrentTagLength);
        break;
    case eData:
        if ( m_CurrentPosition + 1 == m_CurrentTagLimit )
            EndTag();
        break;
    }
    m_CurrentPosition++;
#endif
    m_Output.put(byte);
}

#if !CHECK_STREAM_INTEGRITY
inline
#endif
void CObjectOStreamAsnBinary::WriteBytes(const char* bytes, size_t size)
{
    if ( size == 0 )
        return;
#if CHECK_STREAM_INTEGRITY
    //_TRACE("WriteBytes: " << size);
    if ( m_CurrentTagState != eData )
        THROW1_TRACE(runtime_error, "WriteBytes only allowed in DATA");
    if ( m_CurrentPosition + size > m_CurrentTagLimit )
        THROW1_TRACE(runtime_error, "tag DATA overflow");
    if ( (m_CurrentPosition += size) == m_CurrentTagLimit )
        EndTag();
#endif
    if ( !m_Output.write(bytes, size) )
        THROW1_TRACE(runtime_error, "write error");
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
    else if ( length < 0xffffff ) {
        WriteByte(0x83);
        WriteByte(length >> 16);
        WriteByte(length >> 8);
        WriteByte(length);
    }
    else if ( length < 0xffffffff ) {
        WriteByte(0x84);
        WriteByte(length >> 24);
        WriteByte(length >> 16);
        WriteByte(length >> 8);
        WriteByte(length);
    }
    else {
        THROW1_TRACE(runtime_error, "too big length");
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

void CObjectOStreamAsnBinary::WriteBool(bool data)
{
    WriteSysTag(eBoolean);
    WriteShortLength(1);
    WriteByte(data);
}

void CObjectOStreamAsnBinary::WriteChar(char data)
{
    WriteSysTag(eGeneralString);
    WriteShortLength(1);
    WriteByte(data);
}

void CObjectOStreamAsnBinary::WriteInt(int data)
{
    WriteStdNumber(*this, data);
}

void CObjectOStreamAsnBinary::WriteUInt(unsigned data)
{
    WriteStdNumber(*this, data);
}

void CObjectOStreamAsnBinary::WriteLong(long data)
{
#if LONG_MIN == INT_MIN && LONG_MAX == INT_MAX
    WriteStdNumber(*this, int(data));
#else
    WriteStdNumber(*this, data);
#endif
}

void CObjectOStreamAsnBinary::WriteULong(unsigned long data)
{
#if ULONG_MAX == UINT_MAX
    WriteStdNumber(*this, unsigned(data));
#else
    WriteStdNumber(*this, data);
#endif
}

void CObjectOStreamAsnBinary::WriteDouble(double data)
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

void CObjectOStreamAsnBinary::WriteEnumValue(int value)
{
    WriteSysTag(eEnumerated);
    WriteNumberValue(*this, value);
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
#if HAVE_NCBI_C
    return ASNIO_BIN;
#else
    return 0;
#endif
}

void CObjectOStreamAsnBinary::AsnWrite(AsnIo& , const char* data, size_t length)
{
    if ( length == 0 )
        return;
#if CHECK_STREAM_INTEGRITY
    _TRACE("WriteBytes: " << length);
    if ( m_CurrentTagState != eTagStart )
        THROW1_TRACE(runtime_error, "AsnWrite only allowed at tag start");
    if ( m_CurrentPosition + length > m_CurrentTagLimit )
        THROW1_TRACE(runtime_error, "tag DATA overflow");
    m_CurrentPosition += length;
#endif
    if ( !m_Output.write(data, length) )
        THROW1_TRACE(runtime_error, "write error");
}

END_NCBI_SCOPE
