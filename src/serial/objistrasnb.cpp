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
* Revision 1.7  1999/07/22 19:40:56  vasilche
* Fixed bug with complex object graphs (pointers to members of other objects).
*
* Revision 1.6  1999/07/22 17:33:52  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.5  1999/07/21 20:02:54  vasilche
* Added embedding of ASN.1 binary output from ToolKit to our binary format.
* Fixed bugs with storing pointers into binary ASN.1
*
* Revision 1.4  1999/07/21 14:20:05  vasilche
* Added serialization of bool.
*
* Revision 1.3  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.2  1999/07/07 21:15:02  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.1  1999/07/02 21:31:56  vasilche
* Implemented reading from ASN.1 binary format.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/classinfo.hpp>
#include <serial/member.hpp>
#include <serial/memberid.hpp>
#include <asn.h>

BEGIN_NCBI_SCOPE

using namespace CObjectStreamAsnBinaryDefs;

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CNcbiIstream& in)
    : m_Input(in), m_LastTagState(eNoTagRead)
{
}

unsigned char CObjectIStreamAsnBinary::ReadByte(void)
{
    char c;
    if ( !m_Input.get(c) ) {
        THROW1_TRACE(runtime_error, "unexpected EOF");
    }
    return c;
}

inline
signed char CObjectIStreamAsnBinary::ReadSByte(void)
{
    return ReadByte();
}

void CObjectIStreamAsnBinary::ReadBytes(char* bytes, size_t length)
{
    if ( !m_Input.read(bytes, length) ) {
        THROW1_TRACE(runtime_error, "unexpected EOF");
    }
}

void CObjectIStreamAsnBinary::ExpectByte(TByte byte)
{
    if ( ReadByte() != byte )
        THROW1_TRACE(runtime_error, "expected " + NStr::IntToString(byte));
}

ETag CObjectIStreamAsnBinary::ReadSysTag(void)
{
    _TRACE("ReadSysTag...");
    switch ( m_LastTagState ) {
    case eNoTagRead:
        m_LastTagState = eSysTagRead;
        m_LastTagByte = ReadByte();
        break;
    case eSysTagRead:
        THROW1_TRACE(runtime_error, "double tag read");
    case eSysTagBack:
        m_LastTagState = eSysTagRead;
        break;
    }
    _TRACE("ReadSysTag: " << (m_LastTagByte >> 6) << ", " <<
           ((m_LastTagByte & 0x20) != 0) << ", " << (m_LastTagByte & 0x1f)); 
    return ETag(m_LastTagByte & 0x1f);
}

inline
void CObjectIStreamAsnBinary::BackSysTag(void)
{
    _TRACE("BackSysTag...");
    switch ( m_LastTagState ) {
    case eNoTagRead:
        THROW1_TRACE(runtime_error, "tag back without read");
    case eSysTagBack:
        THROW1_TRACE(runtime_error, "double tag back");
    case eSysTagRead:
        m_LastTagState = eSysTagBack;
        break;
    }
}

inline
void CObjectIStreamAsnBinary::FlushSysTag(bool constructed)
{
    _TRACE("FlushSysTag...");
    switch ( m_LastTagState ) {
    case eNoTagRead:
        THROW1_TRACE(runtime_error, "length read without tag");
    case eSysTagRead:
        break;
    case eSysTagBack:
        THROW1_TRACE(runtime_error, "length read with tag back");
    }
    if ( constructed && (m_LastTagByte & 0x20) == 0 )
        THROW1_TRACE(runtime_error,
                     "non indefinite length on constructed value");
    if ( !constructed && (m_LastTagByte & 0x20) != 0 )
        THROW1_TRACE(runtime_error,
                     "indefinite length on primitive value");
    m_LastTagState = eNoTagRead;
}

TTag CObjectIStreamAsnBinary::ReadTag(void)
{
    ETag sysTag = ReadSysTag();
    if ( sysTag != eLongTag )
        return sysTag;
    TTag tag = 0;
    TByte byte;
    do {
        if ( tag >= (1 << (sizeof(tag) * 8 - 1 - 7)) )
            THROW1_TRACE(runtime_error, "tag number is too big");
        byte = ReadByte();
        tag = (tag << 7) | (byte & 0x7f);
    } while ( (byte & 0x80) == 0 );
    return tag;
}

inline
bool CObjectIStreamAsnBinary::LastTagWas(EClass c, bool constructed)
{
    return m_LastTagState == eSysTagRead &&
        (m_LastTagByte >> 5) == ((c << 1) | (constructed? 1: 0));
}

inline
bool CObjectIStreamAsnBinary::LastTagWas(EClass c, bool constructed, ETag tag)
{
    return m_LastTagState == eSysTagRead &&
        m_LastTagByte == ((c << 6) | (constructed? 0x20: 0) | tag);
}

inline
ETag CObjectIStreamAsnBinary::ReadSysTag(EClass c, bool constructed)
{
    ETag tag = ReadSysTag();
    if ( tag == eLongTag )
        THROW1_TRACE(runtime_error, "unexpected long tag");

    if ( !LastTagWas(c, constructed) )
        THROW1_TRACE(runtime_error,
                     "unexpected tag class/type: should be: " +
                     NStr::IntToString(c) +  (constructed? "C": "p"));
    return tag;
}

string CObjectIStreamAsnBinary::ReadClassTag(void)
{
    _ASSERT(LastTagWas(eApplication, true, eLongTag));
    string name;
    TByte c;
    while ( ((c = ReadByte()) & 0x80) == 0 ) {
        name += char(c);
    }
    return name + char(c & 0x7f);
}

inline
TTag CObjectIStreamAsnBinary::ReadTag(EClass c, bool constructed)
{
    TTag tag = ReadTag();
    if ( !LastTagWas(c, constructed) )
        THROW1_TRACE(runtime_error,
                     "unexpected tag class/type: should be: " +
                     NStr::IntToString(c) +  (constructed? "C": "p"));
    return tag;
}

inline
void CObjectIStreamAsnBinary::ExpectSysTag(EClass c, bool constructed,
                                           ETag tag)
{
    if ( ReadSysTag(c, constructed) != tag )
        THROW1_TRACE(runtime_error,
                     "unexpected tag: should be: " +
                     NStr::IntToString(tag));
}

inline
void CObjectIStreamAsnBinary::ExpectSysTag(ETag tag)
{
    ExpectSysTag(eUniversal, false, tag);
}

size_t CObjectIStreamAsnBinary::ReadShortLength(void)
{
    FlushSysTag();
    TByte byte = ReadByte();
    if ( byte >= 0x80 )
        THROW1_TRACE(runtime_error, "short length expected");
    return byte;
}

size_t CObjectIStreamAsnBinary::ReadLength(void)
{
    FlushSysTag();
    TByte byte = ReadByte();
    if ( byte < 0x80 )
        return byte;
    size_t lengthLength = byte - 0x80;
    if ( lengthLength == 0 )
        THROW1_TRACE(runtime_error, "unexpected indefinite length");
    if ( lengthLength > sizeof(size_t) )
        THROW1_TRACE(runtime_error, "length overflow");
    byte = ReadByte();
    if ( byte == 0 )
        THROW1_TRACE(runtime_error, "illegal length start");
    if ( size_t(-1) < size_t(0) ) {
        if ( lengthLength == sizeof(size_t) && (byte & 0x80) != 0 )
            THROW1_TRACE(runtime_error, "length overflow");
    }
    lengthLength--;
    size_t length = byte;
    while ( lengthLength-- > 0 )
        length = (length << 8) | ReadByte();
    return length;
}

inline
void CObjectIStreamAsnBinary::ExpectShortLength(size_t length)
{
    if ( ReadShortLength() != length )
        THROW1_TRACE(runtime_error, "length expected");
}

inline
void CObjectIStreamAsnBinary::ExpectIndefiniteLength(void)
{
    FlushSysTag(true);
    ExpectByte(0x80);
}

inline
void CObjectIStreamAsnBinary::ExpectEndOfContent(void)
{
    ExpectSysTag(eNone);
    ExpectShortLength(0);
}

template<typename T>
void ReadStdSigned(CObjectIStreamAsnBinary& in, T& data)
{
    size_t length = in.ReadShortLength();
    if ( length == 0 )
        THROW1_TRACE(runtime_error, "zero length of number");
    T n;
    if ( length > sizeof(data) ) {
        // skip
        signed char c = in.ReadSByte();
        if ( c != 0 || c != -1 )
            THROW1_TRACE(runtime_error, "overflow error");
        length--;
        while ( length-- > sizeof(data) )
            in.ExpectByte(c);
        length--;
        n = in.ReadSByte();
        if ( ((n ^ c) & 0x80) != 0 )
            THROW1_TRACE(runtime_error, "overflow error");
    }
    else {
        length--;
        n = in.ReadSByte();
    }
    while ( length-- > 0 ) {
        n = (n << 8) | in.ReadByte();
    }
    data = n;
}

template<typename T>
void ReadStdUnsigned(CObjectIStreamAsnBinary& in, T& data)
{
    size_t length = in.ReadShortLength();
    if ( length == 0 )
        THROW1_TRACE(runtime_error, "zero length of number");
    T n;
    if ( length > sizeof(data) ) {
        // skip
        while ( length-- > sizeof(data) )
            in.ExpectByte(0);
        length--;
        n = in.ReadByte();
        if ( (n & 0x80) != 0 )
            THROW1_TRACE(runtime_error, "overflow error");
    }
    else if ( length == sizeof(data) ) {
        length--;
        n = in.ReadByte();
        if ( (n & 0x80) != 0 )
            THROW1_TRACE(runtime_error, "overflow error");
    }
    else {
        n = 0;
    }
    while ( length-- > 0 ) {
        n = (n << 8) | in.ReadByte();
    }
    data = n;
}

template<typename T>
inline
void ReadStdNumberValue(CObjectIStreamAsnBinary& in, T& data)
{
    if ( T(-1) < T(0) )
        ReadStdSigned(in, data);
    else
        ReadStdUnsigned(in, data);
}

template<typename T>
inline
void ReadStdNumber(CObjectIStreamAsnBinary& in, T& data)
{
    in.ExpectSysTag(eInteger);
    ReadStdNumberValue(in, data);
}

void CObjectIStreamAsnBinary::ReadStd(bool& data)
{
    ExpectSysTag(eBoolean);
    ExpectShortLength(1);
    data = ReadByte() != 0;
}

void CObjectIStreamAsnBinary::ReadStd(char& data)
{
    ExpectSysTag(eGeneralString);
    ExpectShortLength(1);
    data = ReadByte();
}

void CObjectIStreamAsnBinary::ReadStd(signed char& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsnBinary::ReadStd(unsigned char& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsnBinary::ReadStd(short& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsnBinary::ReadStd(unsigned short& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsnBinary::ReadStd(int& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsnBinary::ReadStd(unsigned int& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsnBinary::ReadStd(long& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsnBinary::ReadStd(unsigned long& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsnBinary::ReadStd(float& data)
{
    ExpectSysTag(eReal);
    size_t length = ReadShortLength();
    if ( length < 2 )
        THROW1_TRACE(runtime_error, "too short REAL data");

    ExpectByte(eDecimal);
    length--;
    char buffer[128];
    ReadBytes(buffer, length);
    buffer[length] = 0;
    if ( sscanf(buffer, "%hg", &data) != 1 )
        THROW1_TRACE(runtime_error, "bad REAL data string");
}

void CObjectIStreamAsnBinary::ReadStd(double& data)
{
    ExpectSysTag(eReal);
    size_t length = ReadShortLength();
    if ( length < 2 )
        THROW1_TRACE(runtime_error, "too short REAL data");

    ExpectByte(eDecimal);
    length--;
    char buffer[128];
    ReadBytes(buffer, length);
    buffer[length] = 0;
    if ( sscanf(buffer, "%g", &data) != 1 )
        THROW1_TRACE(runtime_error, "bad REAL data string");
}

string CObjectIStreamAsnBinary::ReadString(void)
{
    ExpectSysTag(eVisibleString);
    size_t length = ReadLength();
    string s;
    s.reserve(length);
    for ( size_t i = 0; i < length; ++i ) {
        s += char(ReadByte());
    }
    return s;
}

char* CObjectIStreamAsnBinary::ReadCString(void)
{
    ExpectSysTag(eVisibleString);
    size_t length = ReadLength();
	char* s = static_cast<char*>(malloc(length + 1));
	ReadBytes(s, length);
	s[length] = 0;
    return s;
}

void CObjectIStreamAsnBinary::VBegin(Block& block)
{
    ExpectSysTag(eUniversal, true, block.RandomOrder()? eSet: eSequence);
    ExpectIndefiniteLength();
}

bool CObjectIStreamAsnBinary::VNext(const Block& )
{
    if ( ReadSysTag() == eNone &&
         LastTagWas(eUniversal, false) ) {
        ExpectShortLength(0);
        return false;
    }
    else {
        BackSysTag();
        return true;
    }
}

void CObjectIStreamAsnBinary::StartMember(Member& member)
{
    member.SetTag(ReadTag(eContextSpecific, true));
    ExpectIndefiniteLength();
}

void CObjectIStreamAsnBinary::EndMember(const Member& )
{
    ExpectEndOfContent();
}

void CObjectIStreamAsnBinary::Begin(ByteBlock& block)
{
	ExpectSysTag(eOctetString);
	SetBlockLength(block, ReadLength());
}

size_t CObjectIStreamAsnBinary::ReadBytes(const ByteBlock& , char* dst, size_t length)
{
	ReadBytes(dst, length);
	return length;
}

CObjectIStream::EPointerType CObjectIStreamAsnBinary::ReadPointerType(void)
{
    ETag tag = ReadSysTag();
    if ( LastTagWas(eUniversal, false) ) {
        switch ( tag ) {
        case eNull:
            ExpectShortLength(0);
            return eNullPointer;
        }
    }
    else if ( LastTagWas(eApplication, true) ) {
        switch ( tag ) {
        case eMemberReference:
            return eMemberPointer;
        case eLongTag:
            return eOtherPointer;
        }
    }
    else if ( LastTagWas(eApplication, false) ) {
        switch ( tag) {
        case eObjectReference:
            return eObjectPointer;
        }
    }
    // by default: try this class
    BackSysTag();
    return eThisPointer;
}

string CObjectIStreamAsnBinary::ReadMemberPointer(void)
{
    ExpectIndefiniteLength();
    return ReadString();
}

void CObjectIStreamAsnBinary::ReadMemberPointerEnd(void)
{
    ExpectEndOfContent();
}

CObjectIStream::TIndex CObjectIStreamAsnBinary::ReadObjectPointer(void)
{
    TIndex index;
    ReadStdNumberValue(*this, index);
    return index;
}

string CObjectIStreamAsnBinary::ReadOtherPointer(void)
{
    string className = ReadClassTag();
    ExpectIndefiniteLength();
    return className;
}

void CObjectIStreamAsnBinary::ReadOtherPointerEnd(void)
{
    ExpectEndOfContent();
}

void CObjectIStreamAsnBinary::SkipValue()
{
    THROW1_TRACE(runtime_error, "not implemented");
}

void CObjectIStreamAsnBinary::SkipObjectPointer(void)
{
    THROW1_TRACE(runtime_error, "not implemented");
}

unsigned CObjectIStreamAsnBinary::GetAsnFlags(void)
{
    return ASNIO_BIN;
}

void CObjectIStreamAsnBinary::AsnOpen(AsnIo& )
{
    if ( m_LastTagByte == eSysTagRead )
        THROW1_TRACE(runtime_error, "double tag read");
}

size_t CObjectIStreamAsnBinary::AsnRead(AsnIo& , char* data, size_t )
{
    if ( m_LastTagState == eSysTagBack ) {
        m_LastTagByte = eNoTagRead;
        *data = m_LastTagByte;
        return 1;
    }
    *data = ReadByte();
    return 1;
}

END_NCBI_SCOPE
