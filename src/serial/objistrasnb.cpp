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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.hpp>

#include <serial/objistrasnb.hpp>
#include <serial/impl/member.hpp>
#include <serial/impl/memberid.hpp>
#include <serial/enumvalues.hpp>
#include <serial/impl/memberlist.hpp>
#include <serial/objhook.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/impl/choice.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/impl/objistrimpl.hpp>
#include <serial/pack_string.hpp>

BEGIN_NCBI_SCOPE


CObjectIStream* CObjectIStream::CreateObjectIStreamAsnBinary(void)
{
    return new CObjectIStreamAsnBinary();
}


CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary), m_FixMethod(how)
{
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagStart;
#endif
#if CHECK_INSTREAM_LIMITS
    m_CurrentTagLimit = 0;
#endif
    m_CurrentTagLength = 0;
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CNcbiIstream& in,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary), m_FixMethod(how)
{
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagStart;
#endif
#if CHECK_INSTREAM_LIMITS
    m_CurrentTagLimit = 0;
#endif
    m_CurrentTagLength = 0;
    Open(in);
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CNcbiIstream& in,
                                                 bool deleteIn,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary), m_FixMethod(how)
{
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagStart;
#endif
#if CHECK_INSTREAM_LIMITS
    m_CurrentTagLimit = 0;
#endif
    m_CurrentTagLength = 0;
    Open(in, deleteIn ? eTakeOwnership : eNoOwnership);
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CNcbiIstream& in,
                                                 EOwnership deleteIn,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary), m_FixMethod(how)
{
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagStart;
#endif
#if CHECK_INSTREAM_LIMITS
    m_CurrentTagLimit = 0;
#endif
    m_CurrentTagLength = 0;
    Open(in, deleteIn);
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CByteSourceReader& reader,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary), m_FixMethod(how)
{
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagStart;
#endif
#if CHECK_INSTREAM_LIMITS
    m_CurrentTagLimit = 0;
#endif
    m_CurrentTagLength = 0;
    Open(reader);
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(const char* buffer,
                                                 size_t size,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary), m_FixMethod(how)
{
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagStart;
#endif
#if CHECK_INSTREAM_LIMITS
    m_CurrentTagLimit = 0;
#endif
    m_CurrentTagLength = 0;
    OpenFromBuffer(buffer, size);
}

CObjectIStreamAsnBinary::TLongTag
CObjectIStreamAsnBinary::PeekTag(TByte first_tag_byte)
{
    TByte byte = StartTag(first_tag_byte);
    ETagValue sysTag = GetTagValue(byte);
    if ( sysTag != eLongTag ) {
#if CHECK_INSTREAM_STATE
        m_CurrentTagState = eTagParsed;
#endif
        m_CurrentTagLength = 1;
        return sysTag;
    }
    TLongTag tag = 0;
    size_t i = 1;
    const size_t KBitsInByte = 8; // ?
    const size_t KTagBits = sizeof(tag) * KBitsInByte - 1;
    do {
        if ( tag >= (1 << (KTagBits - 7)) ) {
            ThrowError(fOverflow,
                       "tag number is too big: "+NStr::IntToString(tag));
        }
        byte = PeekTagByte(i++);
        tag = (tag << 7) | (byte & 0x7f);
    } while ( (byte & 0x80) != 0 );
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagParsed;
#endif
    m_CurrentTagLength = i;
    return tag;
}

void CObjectIStreamAsnBinary::UnexpectedTagClassByte(TByte first_tag_byte,
                                                     TByte expected_class_byte)
{
    ThrowError(fFormatError, "unexpected tag class/constructed: " +
                             TagToString(first_tag_byte) + ", should be " +
                             TagToString(expected_class_byte));
}

string CObjectIStreamAsnBinary::PeekClassTag(void)
{
    TByte byte = StartTag(PeekTagByte());
    if ( GetTagValue(byte) != eLongTag ) {
        ThrowError(fFormatError, "LongTag expected");
    }
    string name;
    size_t i = 1;
    TByte c;
    while ( ((c = PeekTagByte(i++)) & 0x80) != 0 ) {
        name += char(c & 0x7f);
        if ( i > 1024 ) {
            ThrowError(fOverflow, "tag number is too big (greater than 1024)");
        }
    }
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagParsed;
#endif
    m_CurrentTagLength = i;
    name += char(c & 0x7f);
    return name;
}

inline
CObjectIStreamAsnBinary::TByte
CObjectIStreamAsnBinary::PeekAnyTagFirstByte(void)
{
    TByte fByte = StartTag(PeekTagByte());
    if ( GetTagValue(fByte) != eLongTag ) {
#if CHECK_INSTREAM_STATE
        m_CurrentTagState = eTagParsed;
#endif
        m_CurrentTagLength = 1;
        return fByte;
    }
    size_t i = 1;
    TByte byte;
    do {
        if ( i > 1024 ) {
            ThrowError(fOverflow, "tag number is too big (greater than 1024)");
        }
        byte = PeekTagByte(i++);
    } while ( (byte & 0x80) != 0 );
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagParsed;
#endif
    m_CurrentTagLength = i;
    return fByte;
}
string CObjectIStreamAsnBinary::TagToString(TByte byte)
{
    const char *cls, *con, *v;

    switch (byte & eTagClassMask) {
    default:
    case eUniversal:       cls = "";                break;
    case eApplication:     cls = "application/";    break;
    case eContextSpecific: cls = "contextspecific/";break;
    case ePrivate:         cls = "private/";        break;
    }
    switch (byte & eTagConstructedMask) {
    default:
    case ePrimitive:         con = "";              break;
    case eConstructed:       con = "constructed/";  break;
    }
    switch (byte & eTagValueMask) {
    case eNone:              v= "None";             break;
    case eBoolean:           v= "Boolean";          break;
    case eInteger:           v= "Integer";          break;
    case eBitString:         v= "BitString";        break;
    case eOctetString:       v= "OctetString";      break;
    case eNull:              v= "Null";             break;
    case eObjectIdentifier:  v= "ObjectIdentifier"; break;
    case eObjectDescriptor:  v= "ObjectDescriptor"; break;
    case eExternal:          v= "External";         break;
    case eReal:              v= "Real";             break;
    case eEnumerated:        v= "Enumerated";       break;
    case eSequence:          v= "Sequence";         break;
    case eSet:               v= "Set";              break;
    case eNumericString:     v= "NumericString";    break;
    case ePrintableString:   v= "PrintableString";  break;
    case eTeletextString:    v= "TeletextString";   break;
    case eVideotextString:   v= "VideotextString";  break;
    case eIA5String:         v= "IA5String";        break;
    case eUTCTime:           v= "UTCTime";          break;
    case eGeneralizedTime:   v= "GeneralizedTime";  break;
    case eGraphicString:     v= "GraphicString";    break;
    case eVisibleString:     v= "VisibleString";    break;
    case eGeneralString:     v= "GeneralString";    break;
    case eMemberReference:   v= "MemberReference";  break;
    case eObjectReference:   v= "ObjectReference";  break;
    default:                 v= "unknown";          break;
    }
    return string(cls) + con + v + " (" + NStr::IntToString(byte) + ")";
}

void CObjectIStreamAsnBinary::UnexpectedSysTagByte(TByte tag_byte)
{
    TByte got = PeekTagByte();
    ThrowError(fFormatError, "unexpected tag: " + TagToString(got) +
                             ", should be: "    + TagToString(tag_byte));
}

void CObjectIStreamAsnBinary::UnexpectedByte(TByte byte)
{
    ThrowError(fFormatError,
               "byte " + NStr::IntToString(byte) + " expected");
}

inline
CObjectIStreamAsnBinary::TByte
CObjectIStreamAsnBinary::FlushTag(void)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eTagParsed || m_CurrentTagLength == 0 )
        ThrowError(fIllegalCall, "illegal FlushTag call");
    m_CurrentTagState = eLengthValue;
#endif
    m_Input.SkipChars(m_CurrentTagLength);
    return TByte(m_Input.GetChar());
}

inline
bool CObjectIStreamAsnBinary::PeekIndefiniteLength(void)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eTagParsed )
        ThrowError(fIllegalCall, "illegal PeekIndefiniteLength call");
#endif
    return TByte(m_Input.PeekChar(m_CurrentTagLength)) == eIndefiniteLengthByte;
}

inline
void CObjectIStreamAsnBinary::ExpectIndefiniteLength(void)
{
    // indefinite length allowed only for constructed tags
    if ( !GetTagConstructed(m_Input.PeekChar()) )
        ThrowError(fIllegalCall, "illegal ExpectIndefiniteLength call");
#if CHECK_INSTREAM_LIMITS
    _ASSERT(m_CurrentTagLimit == 0);
    // save tag limit
    // tag limit is not changed
    if ( m_CurrentTagLimit != 0 ) {
        m_Limits.push(m_CurrentTagLimit);
    }
#endif
    if ( FlushTag() != eIndefiniteLengthByte ) {
        ThrowError(fFormatError, "IndefiniteLengthByte is expected");
    }
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagStart;
#endif
    m_CurrentTagLength = 0;
}

inline
size_t CObjectIStreamAsnBinary::StartTagData(size_t length)
{
#if CHECK_INSTREAM_LIMITS
    Int8 cur_pos = m_Input.GetStreamPosAsInt8();
    Int8 newLimit = cur_pos + length;
    _ASSERT(newLimit >= cur_pos);
    _ASSERT(newLimit != 0);
    Int8 currentLimit = m_CurrentTagLimit;
    if ( currentLimit != 0 ) {
        if ( newLimit > currentLimit ) {
            ThrowError(fOverflow, "nested data length overflow");
        }
        m_Limits.push(currentLimit);
    }
    m_CurrentTagLimit = newLimit;
#endif
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eData;
#endif
    return length;
}

inline
size_t CObjectIStreamAsnBinary::ReadShortLength(void)
{
    TByte byte = FlushTag();
    if ( byte >= 0x80 ) {
        ThrowError(fFormatError, "ShortLength expected");
    }
    return StartTagData(byte);
}

inline
size_t CObjectIStreamAsnBinary::ReadLengthInlined(void)
{
    TByte byte = FlushTag();
    if ( byte >= 0x80 ) {
        return ReadLengthLong(byte);
    }
    return StartTagData(byte);
}

size_t CObjectIStreamAsnBinary::ReadLength(void)
{
    return ReadLengthInlined();
}

#define ReadLength ReadLengthInlined

size_t CObjectIStreamAsnBinary::ReadLengthLong(TByte byte)
{
    size_t lengthLength = byte - 0x80;
    if ( lengthLength == 0 ) {
        ThrowError(fFormatError, "unexpected indefinite length");
    }
    if ( lengthLength > sizeof(size_t) ) {
        ThrowError(fOverflow, "length overflow");
    }
    size_t length = m_Input.GetChar() & 0xff;
    if ( length == 0 ) {
        ThrowError(fFormatError, "illegal length start");
    }
    while ( --lengthLength > 0 ) {
        length = (length << 8) | (m_Input.GetChar() & 0xff);
    }
    return StartTagData(length);
}

inline
void CObjectIStreamAsnBinary::ExpectShortLength(size_t length)
{
    size_t got = ReadShortLength();
    if ( got != length ) {
        ThrowError(fFormatError,
            "unexpected length: " + NStr::SizetToString(got) +
            ", should be: "       + NStr::SizetToString(length));
    }
}

inline
void CObjectIStreamAsnBinary::ExpectEndOfContent(void)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eTagStart || m_CurrentTagLength != 0 )
        ThrowError(fIllegalCall, "illegal ExpectEndOfContent call");
#endif
    if ( !m_Input.SkipExpectedChars(char(eEndOfContentsByte),
                                    char(eZeroLengthByte)) ) {
        ThrowError(eFormatError, "EndOfContentsByte expected");
    }
#if CHECK_INSTREAM_LIMITS
    // restore tag limit from stack
    _ASSERT(m_CurrentTagLimit == 0);
    if ( m_CurrentTagLimit != 0 ) {
        if ( m_Limits.empty() ) {
            m_CurrentTagLimit = 0;
        }
        else {
            m_CurrentTagLimit = m_Limits.top();
            m_Limits.pop();
        }
        _ASSERT(m_CurrentTagLimit == 0);
    }
#endif
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagStart;
#endif
    m_CurrentTagLength = 0;
}

void CObjectIStreamAsnBinary::ReadBytes(char* buffer, size_t count)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eData ) {
        ThrowError(fIllegalCall, "illegal ReadBytes call");
    }
#endif
    if ( count == 0 )
        return;
#if CHECK_INSTREAM_LIMITS
    Int8 cur_pos = m_Input.GetStreamPosAsInt8();
    Int8 end_pos = cur_pos + count;
    if ( end_pos < cur_pos ||
        (m_CurrentTagLimit != 0 && end_pos > m_CurrentTagLimit) )
        ThrowError(fOverflow, "tag size overflow");
#endif
    m_Input.GetChars(buffer, count);
}

void CObjectIStreamAsnBinary::ReadBytes(string& str, size_t count)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eData ) {
        ThrowError(fIllegalCall, "illegal ReadBytes call");
    }
#endif
    if ( count == 0 )
        return;
#if CHECK_INSTREAM_LIMITS
    Int8 cur_pos = m_Input.GetStreamPosAsInt8();
    Int8 end_pos = cur_pos + count;
    if ( end_pos < cur_pos ||
        (m_CurrentTagLimit != 0 && end_pos > m_CurrentTagLimit) )
        ThrowError(fOverflow, "tag size overflow");
#endif
    m_Input.GetChars(str, count);
}

inline
void CObjectIStreamAsnBinary::SkipBytes(size_t count)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eData ) {
        ThrowError(fIllegalCall, "illegal ReadBytes call");
    }
#endif
    if ( count == 0 )
        return;
#if CHECK_INSTREAM_LIMITS
    Int8 cur_pos = m_Input.GetStreamPosAsInt8();
    Int8 end_pos = cur_pos + count;
    if ( end_pos < cur_pos ||
        (m_CurrentTagLimit != 0 && end_pos > m_CurrentTagLimit) )
        ThrowError(fOverflow, "tag size overflow");
#endif
    m_Input.GetChars(count);
}

template<typename T>
void ReadStdSigned(CObjectIStreamAsnBinary& in, T& data)
{
    size_t length = in.ReadShortLength();
    if ( length == 0 ) {
        in.ThrowError(in.fFormatError, "zero length of number");
    }
    T n;
    if ( length > sizeof(data) ) {
        // skip
        --length;
        Int1 c = in.ReadSByte();
        if ( c != 0 && c != -1 ) {
            in.ThrowError(in.fOverflow, "overflow error");
        }
        while ( length > sizeof(data) ) {
            --length;
            if ( in.ReadSByte() != c ) {
                in.ThrowError(in.fOverflow, "overflow error");
            }
        }
        --length;
        n = in.ReadSByte();
        if ( ((n ^ c) & 0x80) != 0 ) {
            in.ThrowError(in.fOverflow, "overflow error");
        }
    }
    else {
        --length;
        n = in.ReadSByte();
    }
    while ( length > 0 ) {
        --length;
        n = (n << 8) | in.ReadByte();
    }
    data = n;
    in.EndOfTag();
}

template<typename T>
void ReadStdUnsigned(CObjectIStreamAsnBinary& in, T& data)
{
    size_t length = in.ReadShortLength();
    if ( length == 0 ) {
        in.ThrowError(in.fFormatError, "zero length of number");
    }
    T n;
    if ( length > sizeof(data) ) {
        // skip
        while ( length > sizeof(data) ) {
            --length;
            if ( in.ReadSByte() != 0 ) {
                in.ThrowError(in.fOverflow, "overflow error");
            }
        }
        --length;
        n = in.ReadByte();
    }
    else if ( length == sizeof(data) ) {
        --length;
        n = in.ReadByte();
        if ( (n & 0x80) != 0 ) {
            in.ThrowError(in.fOverflow, "overflow error");
        }
    }
    else {
        n = 0;
    }
    while ( length > 0 ) {
        --length;
        n = (n << 8) | in.ReadByte();
    }
    data = n;
    in.EndOfTag();
}

bool CObjectIStreamAsnBinary::ReadBool(void)
{
    ExpectSysTag(eBoolean);
    ExpectShortLength(1);
    bool ret = ReadByte() != 0;
    EndOfTag();
    return ret;
}

char CObjectIStreamAsnBinary::ReadChar(void)
{
    ExpectSysTag(eGeneralString);
    ExpectShortLength(1);
    char ret = ReadByte();
    EndOfTag();
    return ret;
}

Int4 CObjectIStreamAsnBinary::ReadInt4(void)
{
    ExpectSysTag(eInteger);
    Int4 data;
    ReadStdSigned(*this, data);
    return data;
}

Uint4 CObjectIStreamAsnBinary::ReadUint4(void)
{
    ExpectSysTag(eInteger);
    Uint4 data;
    ReadStdUnsigned(*this, data);
    return data;
}

Int8 CObjectIStreamAsnBinary::ReadInt8(void)
{
    TByte next = PeekTagByte();
    if (next == MakeTagByte(eUniversal, ePrimitive, eInteger)) {
        ExpectSysTag(eInteger);
    } else {
        ExpectSysTag(eApplication, ePrimitive, eInteger);
    }
    Uint8 data;
    ReadStdSigned(*this, data);
    return data;
}

Uint8 CObjectIStreamAsnBinary::ReadUint8(void)
{
    TByte next = PeekTagByte();
    if (next == MakeTagByte(eUniversal, ePrimitive, eInteger)) {
        ExpectSysTag(eInteger);
    } else {
        ExpectSysTag(eApplication, ePrimitive, eInteger);
    }
    Uint8 data;
    ReadStdUnsigned(*this, data);
    return data;
}

static const size_t kMaxDoubleLength = 256;

double CObjectIStreamAsnBinary::ReadDouble(void)
{
    ExpectSysTag(eReal);
    size_t length = ReadLength();
    if ( length < 2 ) {
        ThrowError(fFormatError, "too short REAL data: length < 2");
    }
    if ( length > kMaxDoubleLength ) {
        ThrowError(fFormatError, "too long REAL data: length > "
            + NStr::SizetToString(kMaxDoubleLength));
    }

    ExpectByte(eDecimal);
    length--;
    char buffer[kMaxDoubleLength + 2];
    ReadBytes(buffer, length);
    EndOfTag();
    buffer[length] = 0;
    char* endptr;
    double data = NStr::StringToDoublePosix(buffer, &endptr);
    if ( *endptr != 0 ) {
        ThrowError(fFormatError, "bad REAL data string");
    }
    return data;
}

namespace {
    inline
    bool BadVisibleChar(char c)
    {
        return Uint1(c-' ') > Uint1('~' - ' ');
    }

    inline
    void ReplaceVisibleCharMethod(char& c, EFixNonPrint fix_method)
    {
        c = ReplaceVisibleChar(c, fix_method, 0);
    }

    inline
    void FixVisibleCharAlways(char& c)
    {
        if ( BadVisibleChar(c) ) {
            c = '#';
        }
    }
    inline
    void FixVisibleCharMethod(char& c, EFixNonPrint fix_method)
    {
        if ( BadVisibleChar(c) ) {
            ReplaceVisibleCharMethod(c, fix_method);
        }
    }

#define FIND_BAD_CHAR(ptr)                      \
        do {                                    \
            if ( !count ) {                     \
                return false;                   \
            }                                   \
            --count;                            \
        } while ( !BadVisibleChar(*ptr++) );    \
        --ptr

#define REPLACE_BAD_CHARS_ALWAYS(ptr)           \
        *--ptr = '#';                           \
        while ( count-- ) {                     \
            FixVisibleCharAlways(*++ptr);       \
        }                                       \
        return true
    
#define REPLACE_BAD_CHARS_METHOD(ptr, fix_method)          \
        ReplaceVisibleCharMethod(*--ptr, fix_method);      \
        while ( count-- ) {                                \
            FixVisibleCharMethod(*++ptr, fix_method);      \
        }                                                  \
        return true
    
    bool FixVisibleCharsAlways(char* ptr, size_t count)
    {
        FIND_BAD_CHAR(ptr);
        REPLACE_BAD_CHARS_ALWAYS(ptr);
    }

    bool FixVisibleCharsMethod(char* ptr, size_t count,
                               EFixNonPrint fix_method)
    {
        FIND_BAD_CHAR(ptr);
        REPLACE_BAD_CHARS_METHOD(ptr, fix_method);
    }

    bool FixVisibleCharsAlways(string& s)
    {
        size_t count = s.size();
        const char* ptr = s.data();
        FIND_BAD_CHAR(ptr);
        string::iterator it = s.begin()+(ptr-s.data());
        REPLACE_BAD_CHARS_ALWAYS(it);
    }

    bool FixVisibleCharsMethod(string& s, EFixNonPrint fix_method)
    {
        size_t count = s.size();
        const char* ptr = s.data();
        FIND_BAD_CHAR(ptr);
        string::iterator it = s.begin()+(ptr-s.data());
        REPLACE_BAD_CHARS_METHOD(it, fix_method);
    }

    inline
    bool FixVisibleChars(char* ptr, size_t count, EFixNonPrint fix_method)
    {
        return fix_method == eFNP_Allow? false:
            fix_method == eFNP_Replace? FixVisibleCharsAlways(ptr, count):
            FixVisibleCharsMethod(ptr, count, fix_method);
    }

    inline
    bool FixVisibleChars(string& s, EFixNonPrint fix_method)
    {
        return fix_method == eFNP_Allow? false:
            fix_method == eFNP_Replace? FixVisibleCharsAlways(s):
            FixVisibleCharsMethod(s, fix_method);
    }
}

void CObjectIStreamAsnBinary::ReadPackedString(string& s,
                                               CPackString& pack_string,
                                               EStringType type)
{
    ExpectSysTag(eVisibleString);
    size_t length = ReadLength();
    static const size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    if ( length > BUFFER_SIZE || length > pack_string.GetLengthLimit() ) {
        pack_string.Skipped();
        ReadStringValue(length, s,
                        type == eStringTypeVisible? m_FixMethod: eFNP_Allow);
    }
    else {
        ReadBytes(buffer, length);
        EndOfTag();
        pair<CPackString::iterator, bool> found =
            pack_string.Locate(buffer, length);
        if ( found.second ) {
            pack_string.AddOld(s, found.first);
        }
        else {
            if ( type == eStringTypeVisible &&
                 FixVisibleChars(buffer, length, m_FixMethod) ) {
                // do not remember fixed strings
                pack_string.Pack(s, buffer, length);
                return;
            }
            pack_string.AddNew(s, buffer, length, found.first);
        }
    }
}

void CObjectIStreamAsnBinary::ReadString(string& s, EStringType type)
{
    ExpectSysTag(eVisibleString);
    ReadStringValue(ReadLength(), s,
                    type == eStringTypeVisible? m_FixMethod: eFNP_Allow);
}


void CObjectIStreamAsnBinary::ReadStringStore(string& s)
{
    ExpectSysTagByte(MakeTagByte(eApplication, ePrimitive, eStringStore));
    ReadStringValue(ReadLength(), s, eFNP_Allow);
}

void CObjectIStreamAsnBinary::ReadStringValue(size_t length,
                                              string& s,
                                              EFixNonPrint fix_method)
{
    static const size_t BUFFER_SIZE = 1024;
    if ( length != s.size() || length > BUFFER_SIZE ) {
        // new string
        ReadBytes(s, length);
        FixVisibleChars(s, fix_method);
    }
    else {
        char buffer[BUFFER_SIZE];
        // try to reuse old value
        ReadBytes(buffer, length);
        FixVisibleChars(buffer, length, fix_method);
        if ( memcmp(s.data(), buffer, length) != 0 ) {
            s.assign(buffer, length);
        }
    }
    EndOfTag();
}

char* CObjectIStreamAsnBinary::ReadCString(void)
{
    ExpectSysTag(eVisibleString);
    size_t length = ReadLength();
    char* s = static_cast<char*>(malloc(length + 1));
    ReadBytes(s, length);
    s[length] = 0;
    FixVisibleChars(s, length, m_FixMethod);
    EndOfTag();
    return s;
}

void CObjectIStreamAsnBinary::BeginContainer(const CContainerTypeInfo* contType)
{
    ExpectContainer(contType->RandomElementsOrder());
}

void CObjectIStreamAsnBinary::EndContainer(void)
{
    ExpectEndOfContent();
}

bool CObjectIStreamAsnBinary::BeginContainerElement(TTypeInfo /*elementType*/)
{
    return HaveMoreElements();
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamAsnBinary::ReadContainer(const CContainerTypeInfo* cType,
                                            TObjectPtr containerPtr)
{
    ExpectContainer(cType->RandomElementsOrder());

    BEGIN_OBJECT_FRAME(eFrameArrayElement);

    CContainerTypeInfo::CIterator iter;
    bool old_element = cType->InitIterator(iter, containerPtr);
    TTypeInfo elementType = cType->GetElementType();
    while ( HaveMoreElements() ) {
        if ( old_element ) {
            elementType->ReadData(*this, cType->GetElementPtr(iter));
            old_element = cType->NextElement(iter);
        }
        else {
            cType->AddElement(containerPtr, *this);
        }
    }
    if ( old_element ) {
        cType->EraseAllElements(iter);
    }

    END_OBJECT_FRAME();

    ExpectEndOfContent();
}

void CObjectIStreamAsnBinary::SkipContainer(const CContainerTypeInfo* cType)
{
    ExpectContainer(cType->RandomElementsOrder());

    TTypeInfo elementType = cType->GetElementType();
    BEGIN_OBJECT_FRAME(eFrameArrayElement);

    while ( HaveMoreElements() ) {
        SkipObject(elementType);
    }

    END_OBJECT_FRAME();

    ExpectEndOfContent();
}
#endif

void CObjectIStreamAsnBinary::BeginClass(const CClassTypeInfo* classInfo)
{
    ExpectContainer(classInfo->RandomOrder());
}

void CObjectIStreamAsnBinary::EndClass(void)
{
    ExpectEndOfContent();
}

void CObjectIStreamAsnBinary::UnexpectedMember(TLongTag tag, const CItemsInfo& items)
{
    string message ="unexpected member: ["+NStr::IntToString(tag)+
                    "], should be one of: ";
    for ( CItemsInfo::CIterator i(items); i.Valid(); ++i ) {
        message += items.GetItemInfo(i)->GetId().GetName()+ "[" +
            NStr::IntToString(items.GetItemInfo(i)->GetId().GetTag()) + "] ";
    }
    ThrowError(fFormatError, message);
}

TMemberIndex
CObjectIStreamAsnBinary::BeginClassMember(const CClassTypeInfo* classType)
{
    TByte first_tag_byte = PeekTagByte();
    if ( first_tag_byte == eEndOfContentsByte )
        return kInvalidMember;

    TLongTag tag = PeekTag(first_tag_byte, eContextSpecific, eConstructed);
    ExpectIndefiniteLength();
    TMemberIndex index = classType->GetMembers().Find(tag);
    if ( index == kInvalidMember ) {
        if (CanSkipUnknownMembers()) {
            SetFailFlags(fUnknownValue);
            SkipAnyContent();
            ExpectEndOfContent();
            return BeginClassMember(classType);
        }
        else {
            UnexpectedMember(tag, classType->GetItems());
        }
    }
    return index;
}

TMemberIndex
CObjectIStreamAsnBinary::BeginClassMember(const CClassTypeInfo* classType,
                                          TMemberIndex pos)
{
    TByte first_tag_byte = PeekTagByte();
    if ( first_tag_byte == eEndOfContentsByte )
        return kInvalidMember;

    TLongTag tag = PeekTag(first_tag_byte, eContextSpecific, eConstructed);
    ExpectIndefiniteLength();
    TMemberIndex index = classType->GetMembers().Find(tag, pos);
    if ( index == kInvalidMember ) {
        if (CanSkipUnknownMembers()) {
            SetFailFlags(fUnknownValue);
            SkipAnyContent();
            ExpectEndOfContent();
            return BeginClassMember(classType, pos);
        }
        else {
            UnexpectedMember(tag, classType->GetItems());
        }
    }
    return index;
}

void CObjectIStreamAsnBinary::EndClassMember(void)
{
    ExpectEndOfContent();
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamAsnBinary::ReadClassRandom(const CClassTypeInfo* classType,
                                              TObjectPtr classPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    ExpectContainer(classType->RandomOrder());

    ReadClassRandomContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType)) != kInvalidMember ) {
        ReadClassRandomContentsMember(classPtr);
        EndClassMember();
    }

    ReadClassRandomContentsEnd();

    EndClass();
    END_OBJECT_FRAME();
}

void
CObjectIStreamAsnBinary::ReadClassSequential(const CClassTypeInfo* classType,
                                             TObjectPtr classPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    ExpectContainer(classType->RandomOrder());
    ReadClassSequentialContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType,*pos)) != kInvalidMember ) {
        ReadClassSequentialContentsMember(classPtr);
        EndClassMember();
    }

    ReadClassSequentialContentsEnd(classPtr);
    EndClass();
    END_OBJECT_FRAME();
}

void CObjectIStreamAsnBinary::SkipClassRandom(const CClassTypeInfo* classType)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    ExpectContainer(classType->RandomOrder());
    SkipClassRandomContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType)) != kInvalidMember ) {
        SkipClassRandomContentsMember();
        EndClassMember();
    }

    SkipClassRandomContentsEnd();
    EndClass();
    END_OBJECT_FRAME();
}

void
CObjectIStreamAsnBinary::SkipClassSequential(const CClassTypeInfo* classType)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    ExpectContainer(classType->RandomOrder());
    SkipClassSequentialContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType,*pos)) != kInvalidMember ) {
        SkipClassSequentialContentsMember();
        EndClassMember();
    }

    SkipClassSequentialContentsEnd();
    EndClass();
    END_OBJECT_FRAME();
}
#endif

void CObjectIStreamAsnBinary::BeginChoice(const CChoiceTypeInfo* choiceType)
{
    if (choiceType->GetVariantInfo(kFirstMemberIndex)->GetId().IsAttlist()) {
        TopFrame().SetNotag();
        ExpectContainer(false);
    }
}

void CObjectIStreamAsnBinary::EndChoice(void)
{
    if (TopFrame().GetNotag()) {
        ExpectEndOfContent();
        ExpectEndOfContent();
    }
}

TMemberIndex
CObjectIStreamAsnBinary::BeginChoiceVariant(const CChoiceTypeInfo* choiceType)
{
    TLongTag tag = PeekTag(PeekTagByte(), eContextSpecific, eConstructed);
    ExpectIndefiniteLength();
    TMemberIndex index = choiceType->GetVariants().Find(tag);
    if ( index == kInvalidMember ) {
        if (CanSkipUnknownVariants()) {
            SetFailFlags(fUnknownValue);
        } else {
            UnexpectedMember(tag, choiceType->GetItems());
        }
        return index;
    }
    if (index != kFirstMemberIndex && FetchFrameFromTop(1).GetNotag()) {
        if (index != kFirstMemberIndex+1) {
            UnexpectedMember(tag, choiceType->GetItems());
        }
        tag = PeekTag(PeekTagByte(), eContextSpecific, eConstructed);
        ExpectIndefiniteLength();
        index = choiceType->GetVariants().Find(tag)+1;
    }
    return index;
}

void CObjectIStreamAsnBinary::EndChoiceVariant(void)
{
    ExpectEndOfContent();
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamAsnBinary::ReadChoice(const CChoiceTypeInfo* choiceType,
                                         TObjectPtr choicePtr)
{
    TMemberIndex index = BeginChoiceVariant(choiceType);

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());

    variantInfo->ReadVariant(*this, choicePtr);

    END_OBJECT_FRAME();

    EndChoiceVariant();
}

void CObjectIStreamAsnBinary::SkipChoice(const CChoiceTypeInfo* choiceType)
{
    TMemberIndex index = BeginChoiceVariant(choiceType);

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());

    variantInfo->SkipVariant(*this);

    END_OBJECT_FRAME();

    EndChoiceVariant();
}
#endif

void CObjectIStreamAsnBinary::BeginBytes(ByteBlock& block)
{
    if (PeekTagByte() == MakeTagByte(eUniversal, ePrimitive, eOctetString)) {
        ExpectSysTag(eOctetString);
        block.SetLength(ReadLength());
    } else {
        ExpectSysTag(eBitString);
        block.SetLength(ReadLength()-1);
        ReadByte();
    }
}

size_t CObjectIStreamAsnBinary::ReadBytes(ByteBlock& ,
                                          char* dst, size_t length)
{
    ReadBytes(dst, length);
    return length;
}

void CObjectIStreamAsnBinary::EndBytes(const ByteBlock& )
{
    EndOfTag();
}

void CObjectIStreamAsnBinary::BeginChars(CharBlock& block)
{
    ExpectSysTag(eVisibleString);
    block.SetLength(ReadLength());
}

size_t CObjectIStreamAsnBinary::ReadChars(CharBlock& ,
                                          char* dst, size_t length)
{
    ReadBytes(dst, length);
    return length;
}

void CObjectIStreamAsnBinary::EndChars(const CharBlock& )
{
    EndOfTag();
}

void CObjectIStreamAsnBinary::ReadNull(void)
{
    ExpectSysTag(eNull);
    ExpectShortLength(0);
    EndOfTag();
}

void CObjectIStreamAsnBinary::ReadAnyContentObject(CAnyContentObject& )
{
    ThrowError(fNotImplemented,
        "CObjectIStreamAsnBinary::ReadAnyContentObject: "
        "unable to read AnyContent object in ASN binary");
}

void CObjectIStreamAsnBinary::SkipAnyContent(void)
{
    TByte byte = PeekAnyTagFirstByte();
    if ( GetTagConstructed(byte) && PeekIndefiniteLength() ) {
        ExpectIndefiniteLength();
    }
    else {
        size_t length = ReadLength();
        if (length) {
            SkipBytes(length);
        }
        EndOfTag();
        return;
    }
    int depth = 1;
    for ( ;; ) {
        if ( !HaveMoreElements() ) {
            ExpectEndOfContent();
            if ( --depth == 0 ) {
                break;
            }
        }
        else {
            TByte byte = PeekAnyTagFirstByte();
            if ( GetTagConstructed(byte) && PeekIndefiniteLength() ) {
                ExpectIndefiniteLength();
                ++depth;
            }
            else {
                size_t length = ReadLength();
                if (length) {
                    SkipBytes(length);
                }
                EndOfTag();
            }
        }
    }
}

void CObjectIStreamAsnBinary::SkipAnyContentObject(void)
{
    SkipAnyContent();
}

void CObjectIStreamAsnBinary::SkipAnyContentVariant(void)
{
    SkipAnyContent();
    ExpectEndOfContent();
}

void CObjectIStreamAsnBinary::ReadBitString(CBitString& obj)
{
    obj.clear();
#if !BITSTRING_AS_VECTOR
    if (TopFrame().HasMemberId() && TopFrame().GetMemberId().IsCompressed()) {
        ReadCompressedBitString(obj);
        return;
    }
#endif
    ExpectSysTag(eBitString);
    size_t length = ReadLength();
    if (length == 0) {
        return;
    }
    Uint1 unused = ReadByte();

#if BITSTRING_AS_VECTOR
    obj.reserve((--length)*8);
#else
    obj.resize((--length)*8);
    CBitString::size_type len = 0;
#endif

    size_t count;
    const size_t step = 128;
    char bytes[step];
    while ( length != 0 ) {
        ReadBytes(bytes, count= min(length,step));
        length -= count;
        for (size_t i=0; i<count; ++i) {
            Uint1 byte = bytes[i];
#if BITSTRING_AS_VECTOR
            for (Uint1 mask= 0x80; mask != 0; mask >>= 1) {
                obj.push_back( (byte & mask) != 0 );
            }
#else
            if (byte) {
                for (Uint1 mask= 0x80; mask != 0; mask >>= 1, ++len) {
                    if ((byte & mask) != 0 ) {
                        obj.set_bit(len);
                    }
                }
            } else {
                len += 8;
            }
#endif
        }
    }
    obj.resize( obj.size() - unused);
    EndOfTag();
}

inline
void CObjectIStreamAsnBinary::SkipTagData(void)
{
    SkipBytes(ReadLength());
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipBitString(void)
{
    ExpectSysTag(eBitString);
    SkipTagData();
}

CObjectIStream::EPointerType CObjectIStreamAsnBinary::ReadPointerType(void)
{
    TByte byte = PeekTagByte();
    // variants:
    //    eUniversal,   !constructed, eNull             -> NULL
    //    eApplication,  constructed, eMemberReference  -> member reference
    //    eApplication,  constructed, eLongTag          -> other class
    //    eApplication, !constructed, eObjectReference  -> object reference
    // any other -> this class
    if ( byte == MakeTagByte(eUniversal, ePrimitive, eNull) ) {
#if CHECK_INSTREAM_STATE
        m_CurrentTagState = eTagParsed;
#endif
        m_CurrentTagLength = 1;
        ExpectShortLength(0);
        EndOfTag();
        return eNullPointer;
    }
    else if ( byte == MakeTagByte(eApplication, eConstructed, eLongTag) ) {
        return eOtherPointer;
    }
    else if ( byte == MakeTagByte(eApplication, ePrimitive, eObjectReference) ) {
        return eObjectPointer;
    }
    // by default: try this class
    return eThisPointer;
}

TEnumValueType
CObjectIStreamAsnBinary::ReadEnum(const CEnumeratedTypeValues& values)
{
    TEnumValueType value;
    if ( values.IsInteger() ) {
        // allow any integer
        ExpectSysTag(eInteger);
        ReadStdSigned(*this, value);
    }
    else {
        // enum element by value
        ExpectSysTag(eEnumerated);
        ReadStdSigned(*this, value);
        values.FindName(value, false); // check value
    }
    return value;
}

CObjectIStream::TObjectIndex CObjectIStreamAsnBinary::ReadObjectPointer(void)
{
    TObjectIndex data;
    ReadStdSigned(*this, data);
    return data;
}

string CObjectIStreamAsnBinary::ReadOtherPointer(void)
{
    string className = PeekClassTag();
    ExpectIndefiniteLength();
    return className;
}

void CObjectIStreamAsnBinary::ReadOtherPointerEnd(void)
{
    ExpectEndOfContent();
}

bool CObjectIStreamAsnBinary::SkipRealValue(void)
{
    if ( PeekTagByte() == eEndOfContentsByte &&
         PeekTagByte(1) == eZeroLengthByte )
        return false;
    TByte byte = PeekAnyTagFirstByte();
    if ( GetTagConstructed(byte) ) {
        // constructed
        ExpectIndefiniteLength();
        while ( SkipRealValue() )
            ;
        ExpectEndOfContent();
    }
    else {
        SkipTagData();
    }
    return true;
}


void CObjectIStreamAsnBinary::SkipBool(void)
{
    ExpectSysTag(eBoolean);
    ExpectShortLength(1);
    ReadByte();
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipChar(void)
{
    ExpectSysTag(eGeneralString);
    ExpectShortLength(1);
    ReadByte();
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipSNumber(void)
{
    ExpectSysTag(eInteger);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipUNumber(void)
{
    ExpectSysTag(eInteger);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipFNumber(void)
{
    ExpectSysTag(eReal);
    size_t length = ReadLength();
    if ( length < 2 )
        ThrowError(fFormatError, "too short REAL data: length < 2");
    if ( length > kMaxDoubleLength )
        ThrowError(fFormatError, "too long REAL data: length > "
            + NStr::SizetToString(kMaxDoubleLength));

    ExpectByte(eDecimal);
    length--;
    SkipBytes(length);
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipString(EStringType /*type*/)
{
    ExpectSysTag(eVisibleString);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipStringStore(void)
{
    ExpectSysTag(eApplication, ePrimitive, eStringStore);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipNull(void)
{
    ExpectSysTag(eNull);
    ExpectShortLength(0);
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipByteBlock(void)
{
    ExpectSysTag(eOctetString);
    SkipTagData();
}

END_NCBI_SCOPE
