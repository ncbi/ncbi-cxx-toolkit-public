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
#include <corelib/ncbi_param.hpp>

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
#include <serial/error_codes.hpp>
#include <math.h>

#undef _TRACE
#define _TRACE(arg) ((void)0)

#define USE_OLD_TAGS 0

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   Serial_IStream

CObjectIStream* CObjectIStream::CreateObjectIStreamAsnBinary(void)
{
    return new CObjectIStreamAsnBinary();
}


CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary)
{
    FixNonPrint(how);
    ResetThisState();
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CNcbiIstream& in,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary)
{
    FixNonPrint(how);
    ResetThisState();
    Open(in);
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CNcbiIstream& in,
                                                 bool deleteIn,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary)
{
    FixNonPrint(how);
    ResetThisState();
    Open(in, deleteIn ? eTakeOwnership : eNoOwnership);
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CNcbiIstream& in,
                                                 EOwnership deleteIn,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary)
{
    FixNonPrint(how);
    ResetThisState();
    Open(in, deleteIn);
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CByteSourceReader& reader,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary)
{
    FixNonPrint(how);
    ResetThisState();
    Open(reader);
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(const char* buffer,
                                                 size_t size,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary)
{
    FixNonPrint(how);
    ResetThisState();
    OpenFromBuffer(buffer, size);
}

void CObjectIStreamAsnBinary::ResetThisState(void)
{
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagStart;
#endif
#if CHECK_INSTREAM_LIMITS
    m_CurrentTagLimit = 0;
    while (!m_Limits.empty()) {
        m_Limits.pop();
    }
#endif
    m_CurrentTagLength = 0;
    m_SkipNextTag = false;
#if USE_DEF_LEN
    m_CurrentDataLimit = 0;
    m_DataLimits.clear();
    m_DataLimits.reserve(16);
#endif
}

void CObjectIStreamAsnBinary::ResetState(void)
{
    CObjectIStream::ResetState();
    ResetThisState();
}

CObjectIStreamAsnBinary::TLongTag
CObjectIStreamAsnBinary::PeekLongTag(void)
{
    TLongTag tag = 0;
    size_t i = 1;
    const size_t KBitsInByte = 8; // ?
    const size_t KTagBits = sizeof(tag) * KBitsInByte - 1;
    TByte byte;
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

void CObjectIStreamAsnBinary::UnexpectedShortLength(size_t got_length,
                                                    size_t expected_length)
{
    ThrowError(fFormatError,
               "unexpected length: " + NStr::SizetToString(got_length) +
               ", should be: "       + NStr::SizetToString(expected_length));
}

void CObjectIStreamAsnBinary::UnexpectedFixedLength(void)
{
    ThrowError(fFormatError, "IndefiniteLengthByte is expected");
}

void CObjectIStreamAsnBinary::UnexpectedContinuation(void)
{
    ThrowError(eFormatError, "EndOfContentsByte expected");
}

void CObjectIStreamAsnBinary::UnexpectedLongLength(void)
{
    ThrowError(fFormatError, "ShortLength expected");
}

void CObjectIStreamAsnBinary::UnexpectedTagValue(ETagClass tag_class,
    TLongTag tag_got, TLongTag tag_expected)
{
    string s("Unexpected tag: ");
    if (tag_class == eApplication) {
        s += "Application ";
    } else if (tag_class == ePrivate) {
        s += "Private ";
    }
    s += NStr::NumericToString(tag_got) + " expected: " +
         NStr::NumericToString(tag_expected);
    ThrowError(fFormatError, s);
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

NCBI_PARAM_DECL(bool, SERIAL, READ_ANY_UTF8STRING_TAG);
NCBI_PARAM_DEF_EX(bool, SERIAL, READ_ANY_UTF8STRING_TAG, true,
                  eParam_NoThread, SERIAL_READ_ANY_UTF8STRING_TAG);

// int value meaning for READ_ANY_VISBLESTRING_TAG:
// 0 - disallow, throws an exception
// 1 - allow, but warn once (default)
// 2 - allow without warning
NCBI_PARAM_DECL(int, SERIAL, READ_ANY_VISIBLESTRING_TAG);
NCBI_PARAM_DEF_EX(int, SERIAL, READ_ANY_VISIBLESTRING_TAG, 1,
                  eParam_NoThread, SERIAL_READ_ANY_VISIBLESTRING_TAG);

void CObjectIStreamAsnBinary::ExpectStringTag(EStringType type)
{
    if (m_SkipNextTag) {
        m_SkipNextTag = false;
        return;
    }
    if ( type == eStringTypeUTF8 ) {
        static const bool sx_ReadAnyUtf8 =
            NCBI_PARAM_TYPE(SERIAL, READ_ANY_UTF8STRING_TAG)::GetDefault();
        if ( sx_ReadAnyUtf8 ) {
            // may be eVisibleString
            TByte alt_tag = MakeTagByte(eUniversal,ePrimitive,eVisibleString);
            if ( PeekTagByte() == alt_tag ) {
                ExpectSysTagByte(alt_tag);
                return;
            }
        }
        ExpectSysTag(eUniversal, ePrimitive, eUTF8String);
    }
    else {
        static const int sx_ReadAny =
            NCBI_PARAM_TYPE(SERIAL, READ_ANY_VISIBLESTRING_TAG)::GetDefault();
        if ( sx_ReadAny ) {
            // may be eUTF8String
            TByte alt_tag = MakeTagByte(eUniversal,ePrimitive,eUTF8String);
            if ( PeekTagByte() == alt_tag ) {
                // optionally issue a warning
                if ( sx_ReadAny == 1 ) {
                    ERR_POST_X_ONCE(10, Warning<<
                                    "CObjectIStreamAsnBinary: UTF8String data for VisibleString member "<<GetStackTraceASN()<<", ASN.1 specification may need an update");
                }
                ExpectSysTagByte(alt_tag);
                return;
            }
        }
        ExpectSysTag(eUniversal, ePrimitive, eVisibleString);
    }
}

string CObjectIStreamAsnBinary::TagToString(TByte byte)
{
    const char *cls, *con;

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
    if ((byte & eTagClassMask) == eUniversal) {
        const char *v;
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
        case eUTF8String:        v= "UTF8String";       break;
        case eGeneralString:     v= "GeneralString";    break;
        case eMemberReference:   v= "MemberReference";  break;
        case eObjectReference:   v= "ObjectReference";  break;
        default:                 v= "unknown";          break;
        }
        return string(cls) + con + v + " (" + NStr::IntToString(byte) + ")";
    }
    return string(cls) + con + NStr::NumericToString((byte & eTagValueMask));
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
    if (m_SkipNextTag) {
        m_SkipNextTag = false;
    } else {
        TByte next = PeekTagByte();
        if (next == MakeTagByte(eUniversal, ePrimitive, eInteger)) {
            ExpectSysTag(eInteger);
        } else {
            ExpectSysTag(eApplication, ePrimitive, eInteger);
        }
    }
    Uint8 data;
    ReadStdSigned(*this, data);
    return data;
}

Uint8 CObjectIStreamAsnBinary::ReadUint8(void)
{
    if (m_SkipNextTag) {
        m_SkipNextTag = false;
    } else {
        TByte next = PeekTagByte();
        if (next == MakeTagByte(eUniversal, ePrimitive, eInteger)) {
            ExpectSysTag(eInteger);
        } else {
            ExpectSysTag(eApplication, ePrimitive, eInteger);
        }
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
    if (NStr::strcasecmp(buffer,"PLUS-INFINITY") == 0) {
        return HUGE_VAL;
    } else if (NStr::strcasecmp(buffer,"MINUS-INFINITY") == 0) {
        return -HUGE_VAL;
    } else if (NStr::strcasecmp(buffer,"NOT-A-NUMBER") == 0) {
        return HUGE_VAL/HUGE_VAL; /* NCBI_FAKE_WARNING */
    }
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
        c = ReplaceVisibleChar(c, fix_method, 0, kEmptyStr);
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
    ExpectStringTag(type);
    size_t length = ReadLength();
    static const size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    if ( length > BUFFER_SIZE || length > pack_string.GetLengthLimit() ) {
        pack_string.Skipped();
        ReadStringValue(length, s,
                        type == eStringTypeVisible? x_FixCharsMethod(): eFNP_Allow);
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
                 FixVisibleChars(buffer, length, x_FixCharsMethod()) ) {
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
    ExpectStringTag(type);
    ReadStringValue(ReadLength(), s,
                    type == eStringTypeVisible? x_FixCharsMethod(): eFNP_Allow);
}


void CObjectIStreamAsnBinary::ReadStringStore(string& s)
{
    if (m_SkipNextTag) {
        m_SkipNextTag = false;
    } else {
        ExpectSysTagByte(MakeTagByte(eApplication, ePrimitive, eStringStore));
    }
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
    FixVisibleChars(s, length, x_FixCharsMethod());
    EndOfTag();
    return s;
}

void CObjectIStreamAsnBinary::BeginNamedType(TTypeInfo namedTypeInfo)
{
#if !USE_OLD_TAGS
    bool need_eoc = false;
    if (namedTypeInfo->HasTag()) {
        if (!m_SkipNextTag) {
            need_eoc = namedTypeInfo->IsTagConstructed();
            ExpectTag(namedTypeInfo->GetTagClass(),
                      namedTypeInfo->GetTagConstructed(),
                      namedTypeInfo->GetTag());
            if (need_eoc) {
                ExpectIndefiniteLength();
            }
        }
        m_SkipNextTag = namedTypeInfo->IsTagImplicit();
    }
    TopFrame().SetNoEOC(!need_eoc);
#endif
}

void CObjectIStreamAsnBinary::EndNamedType(void)
{
#if !USE_OLD_TAGS
    if (!TopFrame().GetNoEOC()) {
        ExpectEndOfContent();
    }
#endif
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamAsnBinary::ReadNamedType(
    TTypeInfo namedTypeInfo,TTypeInfo typeInfo, TObjectPtr object)
{
#if USE_OLD_TAGS
#ifndef VIRTUAL_MID_LEVEL_IO
    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);
    BeginNamedType(namedTypeInfo);
#endif
    ReadObject(object, typeInfo);
#ifndef VIRTUAL_MID_LEVEL_IO
    EndNamedType();
    END_OBJECT_FRAME();
#endif

#else
    bool need_eoc = false;
    if (namedTypeInfo->HasTag()) {
        if (!m_SkipNextTag) {
            need_eoc = namedTypeInfo->IsTagConstructed();
            ExpectTag(namedTypeInfo->GetTagClass(),
                      namedTypeInfo->GetTagConstructed(),
                      namedTypeInfo->GetTag());
            if (need_eoc) {
                ExpectIndefiniteLength();
            }
        }
        m_SkipNextTag = namedTypeInfo->IsTagImplicit();
    }

    ReadObject(object, typeInfo);
    if (need_eoc) {
        ExpectEndOfContent();
    }
#endif
}
void CObjectIStreamAsnBinary::SkipNamedType(
    TTypeInfo namedTypeInfo,TTypeInfo typeInfo)
{
    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);
    BeginNamedType(namedTypeInfo);

    SkipObject(typeInfo);

    EndNamedType();
    END_OBJECT_FRAME();
}

void CObjectIStreamAsnBinary::BeginContainer(const CContainerTypeInfo* cType)
{
#if USE_OLD_TAGS
    ExpectContainer(cType->RandomElementsOrder());
#else
    _ASSERT(cType->HasTag());
    _ASSERT(cType->IsTagConstructed());
    bool need_eoc = !m_SkipNextTag;
    if (!m_SkipNextTag) {
        ExpectTag(cType->GetTagClass(), eConstructed, cType->GetTag());
        ExpectIndefiniteLength();
    }
    m_SkipNextTag = cType->IsTagImplicit();
    TopFrame().SetNoEOC(!need_eoc);
#endif
}

void CObjectIStreamAsnBinary::EndContainer(void)
{
#if USE_OLD_TAGS
    ExpectEndOfContent();
#else
    if (!TopFrame().GetNoEOC()) {
        ExpectEndOfContent();
    }
#endif
}

bool CObjectIStreamAsnBinary::BeginContainerElement(TTypeInfo /*elementType*/)
{
    return HaveMoreElements();
}

void CObjectIStreamAsnBinary::ReadContainer(const CContainerTypeInfo* cType,
                                            TObjectPtr containerPtr)
{
#if USE_OLD_TAGS
    ExpectContainer(cType->RandomElementsOrder());
#else
    BEGIN_OBJECT_FRAME2(eFrameArray, cType);
    _ASSERT(cType->HasTag());
    _ASSERT(cType->IsTagConstructed());
    bool need_eoc = !m_SkipNextTag;
    if (!m_SkipNextTag) {
        ExpectTag(cType->GetTagClass(), eConstructed, cType->GetTag());
        ExpectIndefiniteLength();
    }
    m_SkipNextTag = cType->IsTagImplicit();
#endif

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

#if USE_OLD_TAGS
    ExpectEndOfContent();
#else
    if (need_eoc) {
        ExpectEndOfContent();
    }
    END_OBJECT_FRAME();
#endif
}

void CObjectIStreamAsnBinary::SkipContainer(const CContainerTypeInfo* cType)
{
#if USE_OLD_TAGS
    ExpectContainer(cType->RandomElementsOrder());
#else
    BEGIN_OBJECT_FRAME2(eFrameArray, cType);
    CObjectIStreamAsnBinary::BeginContainer(cType);
#endif

    TTypeInfo elementType = cType->GetElementType();
    BEGIN_OBJECT_FRAME(eFrameArrayElement);

    while ( HaveMoreElements() ) {
        SkipObject(elementType);
    }

    END_OBJECT_FRAME();

#if USE_OLD_TAGS
    ExpectEndOfContent();
#else
    CObjectIStreamAsnBinary::EndContainer();
    END_OBJECT_FRAME();
#endif
}
#endif

void CObjectIStreamAsnBinary::BeginClass(const CClassTypeInfo* classType)
{
#if USE_OLD_TAGS
    ExpectContainer(classType->RandomOrder());
#else
    _ASSERT(classType->HasTag());
    _ASSERT(classType->IsTagConstructed());
    bool need_eoc = !m_SkipNextTag;
    if (!m_SkipNextTag) {
        ExpectTag(classType->GetTagClass(), eConstructed, classType->GetTag());
        ExpectIndefiniteLength();
    }
    m_SkipNextTag = classType->IsTagImplicit();
    TopFrame().SetNoEOC(!need_eoc);
#endif
}

void CObjectIStreamAsnBinary::EndClass(void)
{
#if USE_OLD_TAGS
    ExpectEndOfContent();
#else
    if (!TopFrame().GetNoEOC()) {
        ExpectEndOfContent();
    }
#endif
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
#if USE_DEF_LEN
    if (!HaveMoreElements()) {
        return kInvalidMember;
    }
    TByte first_tag_byte = PeekTagByte();
#else
    TByte first_tag_byte = PeekTagByte();
    if ( first_tag_byte == eEndOfContentsByte )
        return kInvalidMember;
#endif

#if !USE_OLD_TAGS
    if (classType->GetTagType() != eAutomatic) {
        TLongTag tag = PeekTag(first_tag_byte);
        TMemberIndex index = classType->GetMembers().Find(tag, GetTagClass(first_tag_byte));
        if ( index == kInvalidMember ) {
            UnexpectedMember(tag, classType->GetItems());
        }
        if (!classType->GetMemberInfo(index)->GetId().HasTag()) {
            UndoPeekTag();
            TopFrame().SetNoEOC(true);
            m_SkipNextTag = false;
            return index;
        }
        bool need_eoc = GetTagConstructed(first_tag_byte) != ePrimitive;
        if (need_eoc) {
            ExpectIndefiniteLength();
        }
        TopFrame().SetNoEOC(!need_eoc);
        m_SkipNextTag = classType->GetMemberInfo(index)->GetId().IsTagImplicit();
        return index;
    }
#endif
    TLongTag tag = PeekTag(first_tag_byte, eContextSpecific, eConstructed);
    ExpectIndefiniteLength();
    TMemberIndex index = classType->GetMembers().Find(tag, eContextSpecific);
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
#if USE_DEF_LEN
    if (!HaveMoreElements()) {
        return kInvalidMember;
    }
    TByte first_tag_byte = PeekTagByte();
#else
    TByte first_tag_byte = PeekTagByte();
    if ( first_tag_byte == eEndOfContentsByte )
        return kInvalidMember;
#endif

#if !USE_OLD_TAGS
    if (classType->GetTagType() != eAutomatic) {
        TLongTag tag = PeekTag(first_tag_byte);
        TMemberIndex index = classType->GetMembers().Find(tag, GetTagClass(first_tag_byte), pos);
        if ( index == kInvalidMember ) {
            UnexpectedMember(tag, classType->GetItems());
        }
        if (!classType->GetMemberInfo(index)->GetId().HasTag()) {
            UndoPeekTag();
            TopFrame().SetNoEOC(true);
            m_SkipNextTag = false;
            return index;
        }
        bool need_eoc = GetTagConstructed(first_tag_byte) != ePrimitive;
        if (need_eoc) {
            ExpectIndefiniteLength();
        }
        TopFrame().SetNoEOC(!need_eoc);
        m_SkipNextTag = classType->GetMemberInfo(index)->GetId().IsTagImplicit();
        return index;
    }
#endif
    TLongTag tag = PeekTag(first_tag_byte, eContextSpecific, eConstructed);
    ExpectIndefiniteLength();
    TMemberIndex index = classType->GetMembers().Find(tag, eContextSpecific, pos);
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
    if (!TopFrame().GetNoEOC()) {
        ExpectEndOfContent();
    }
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamAsnBinary::ReadClassRandom(const CClassTypeInfo* classType,
                                              TObjectPtr classPtr)
{
    BEGIN_OBJECT_FRAME3(eFrameClass, classType, classPtr);
#if USE_OLD_TAGS
    ExpectContainer(classType->RandomOrder());
#else
    CObjectIStreamAsnBinary::BeginClass(classType);
#endif
    ReadClassRandomContentsBegin(classType);

    TMemberIndex index;
    while ( (index = CObjectIStreamAsnBinary::BeginClassMember(classType)) != kInvalidMember ) {
        ReadClassRandomContentsMember(classPtr);
//        ExpectEndOfContent();
        CObjectIStreamAsnBinary::EndClassMember();
    }

    ReadClassRandomContentsEnd();
//    ExpectEndOfContent();
    CObjectIStreamAsnBinary::EndClass();
    END_OBJECT_FRAME();
}

void
CObjectIStreamAsnBinary::ReadClassSequential(const CClassTypeInfo* classType,
                                             TObjectPtr classPtr)
{
    BEGIN_OBJECT_FRAME3(eFrameClass, classType, classPtr);
#if USE_OLD_TAGS
    ExpectContainer(classType->RandomOrder());
#else
    CObjectIStreamAsnBinary::BeginClass(classType);
#endif
    ReadClassSequentialContentsBegin(classType);

    TMemberIndex index;
    while ( (index = CObjectIStreamAsnBinary::BeginClassMember(classType,*pos)) != kInvalidMember ) {
        ReadClassSequentialContentsMember(classPtr);
#if USE_OLD_TAGS
        ExpectEndOfContent();
#else
        CObjectIStreamAsnBinary::EndClassMember();
#endif
    }

    ReadClassSequentialContentsEnd(classPtr);
#if USE_OLD_TAGS
    ExpectEndOfContent();
#else
    CObjectIStreamAsnBinary::EndClass();
#endif
    END_OBJECT_FRAME();
}

void CObjectIStreamAsnBinary::SkipClassRandom(const CClassTypeInfo* classType)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
#if USE_OLD_TAGS
    ExpectContainer(classType->RandomOrder());
#else
    CObjectIStreamAsnBinary::BeginClass(classType);
#endif
    SkipClassRandomContentsBegin(classType);

    TMemberIndex index;
    while ( (index = CObjectIStreamAsnBinary::BeginClassMember(classType)) != kInvalidMember ) {
        SkipClassRandomContentsMember();
#if USE_OLD_TAGS
        ExpectEndOfContent();
#else
        CObjectIStreamAsnBinary::EndClassMember();
#endif
    }

    SkipClassRandomContentsEnd();
#if USE_OLD_TAGS
    ExpectEndOfContent();
#else
    CObjectIStreamAsnBinary::EndClass();
#endif
    END_OBJECT_FRAME();
}

void
CObjectIStreamAsnBinary::SkipClassSequential(const CClassTypeInfo* classType)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
#if USE_OLD_TAGS
    ExpectContainer(classType->RandomOrder());
#else
    CObjectIStreamAsnBinary::BeginClass(classType);
#endif
    SkipClassSequentialContentsBegin(classType);

    TMemberIndex index;
    while ( (index = CObjectIStreamAsnBinary::BeginClassMember(classType,*pos)) != kInvalidMember ) {
        SkipClassSequentialContentsMember();
#if USE_OLD_TAGS
        ExpectEndOfContent();
#else
        CObjectIStreamAsnBinary::EndClassMember();
#endif
    }

    SkipClassSequentialContentsEnd();
#if USE_OLD_TAGS
    ExpectEndOfContent();
#else
    CObjectIStreamAsnBinary::EndClass();
#endif
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
#if !USE_OLD_TAGS
    if (choiceType->GetTagType() != eAutomatic) {
        TByte first_tag_byte = PeekTagByte();
        TLongTag tag = PeekTag(first_tag_byte);

        TMemberIndex index = choiceType->GetVariants().Find(tag, GetTagClass(first_tag_byte));
        if ( index == kInvalidMember ) {
            UnexpectedMember(tag, choiceType->GetItems());
        }
        if (!choiceType->GetVariantInfo(index)->GetId().HasTag()) {
            UndoPeekTag();
            TopFrame().SetNoEOC(true);
            m_SkipNextTag = false;
            return index;
        }
        bool need_eoc = GetTagConstructed(first_tag_byte) != ePrimitive;
        if (need_eoc) {
            ExpectIndefiniteLength();
        }
        TopFrame().SetNoEOC(!need_eoc);
        m_SkipNextTag = choiceType->GetVariantInfo(index)->GetId().IsTagImplicit();
        return index;
    }
#endif

    TLongTag tag = PeekTag(PeekTagByte(), eContextSpecific, eConstructed);
    ExpectIndefiniteLength();
    TMemberIndex index = choiceType->GetVariants().Find(tag, eContextSpecific);
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
        index = choiceType->GetVariants().Find(tag, eContextSpecific)+1;
    }
    return index;
}

void CObjectIStreamAsnBinary::EndChoiceVariant(void)
{
#if USE_OLD_TAGS
    ExpectEndOfContent();
#else
    if (!TopFrame().GetNoEOC()) {
        ExpectEndOfContent();
    }
#endif
}

#ifdef VIRTUAL_MID_LEVEL_IO

void CObjectIStreamAsnBinary::ReadChoiceSimple(const CChoiceTypeInfo* choiceType,
                                               TObjectPtr choicePtr)
{
    BEGIN_OBJECT_FRAME3(eFrameChoice, choiceType, choicePtr);
    BEGIN_OBJECT_FRAME(eFrameChoiceVariant);
    TMemberIndex index;
    if (choiceType->GetTagType() != eAutomatic) {
        index = CObjectIStreamAsnBinary::BeginChoiceVariant(choiceType);
    } else {
        TLongTag tag = PeekTag(PeekTagByte(), eContextSpecific, eConstructed);
        ExpectIndefiniteLength();
        index = choiceType->GetVariants().Find(tag, eContextSpecific);
        if ( index == kInvalidMember ) {
            if (!CanSkipUnknownVariants()) {
                UnexpectedMember(tag, choiceType->GetItems());
            }
            SetFailFlags(fUnknownValue);
            SkipAnyContent();
        }
    }
    if ( index != kInvalidMember ) {
        const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
        SetTopMemberId(variantInfo->GetId());
        variantInfo->ReadVariant(*this, choicePtr);
    }
    if (choiceType->GetTagType() != eAutomatic) {
        CObjectIStreamAsnBinary::EndChoiceVariant();
    } else {
        ExpectEndOfContent();
    }
    END_OBJECT_FRAME();
    END_OBJECT_FRAME();
}

void CObjectIStreamAsnBinary::SkipChoiceSimple(const CChoiceTypeInfo* choiceType)
{
    BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);
    BEGIN_OBJECT_FRAME(eFrameChoiceVariant);
    TMemberIndex index;
    if (choiceType->GetTagType() != eAutomatic) {
        index = CObjectIStreamAsnBinary::BeginChoiceVariant(choiceType);
    } else {
        TLongTag tag = PeekTag(PeekTagByte(), eContextSpecific, eConstructed);
        ExpectIndefiniteLength();
        index = choiceType->GetVariants().Find(tag, eContextSpecific);
        if ( index == kInvalidMember ) {
            if (!CanSkipUnknownVariants()) {
                UnexpectedMember(tag, choiceType->GetItems());
            }
            SetFailFlags(fUnknownValue);
            SkipAnyContent();
        }
    }
    if ( index != kInvalidMember ) {
        const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
        SetTopMemberId(variantInfo->GetId());
        variantInfo->SkipVariant(*this);
    }
    if (choiceType->GetTagType() != eAutomatic) {
        CObjectIStreamAsnBinary::EndChoiceVariant();
    } else {
        ExpectEndOfContent();
    }
    END_OBJECT_FRAME();
    END_OBJECT_FRAME();
}
#endif

void CObjectIStreamAsnBinary::BeginBytes(ByteBlock& block)
{
    CAsnBinaryDefs::ETagValue type = CAsnBinaryDefs::eNone;
    TByte bt = PeekByte();
    if (bt == MakeTagByte(eUniversal, ePrimitive, eOctetString)) {
        type = eOctetString;
    } else if (bt == MakeTagByte(eUniversal, ePrimitive, eBitString)) {
        type = eBitString;
    } else if (m_SkipNextTag) {
        const CChoiceTypeInfo* choiceType =
            dynamic_cast<const CChoiceTypeInfo*>(FetchFrameFromTop(1).GetTypeInfo());
        type = (CAsnBinaryDefs::ETagValue)choiceType->GetVariantInfo(
            TopFrame().GetMemberId().GetName())->GetTypeInfo()->GetTag();
    }
    if (type == eOctetString) {
        ExpectSysTag(eOctetString);
        block.SetLength(ReadLength());
    } else if (type == eBitString) {
        ExpectSysTag(eBitString);
        block.SetLength(ReadLength()-1);
        ReadByte();
    } else {
        ThrowError(fUnknownValue,
            "Unable to identify the type of byte block");
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

set<TTypeInfo>
CObjectIStreamAsnBinary::GuessDataType(set<TTypeInfo>& known_types,
                                       size_t max_length,
                                       size_t max_bytes)
{
    set<TTypeInfo> matching_types;
    vector<int> pattern;

    // save state
    size_t pos0 = m_Input.SetBufferLock(max_bytes);
#if CHECK_INSTREAM_STATE
    ETagState state = m_CurrentTagState;
#endif
#if CHECK_INSTREAM_LIMITS
    Int8 lim = m_CurrentTagLimit;
#endif

    try {
        GetTagPattern(pattern, max_length*3);
    }
    catch ( CIOException& exc ) {
        if ( exc.GetErrCode() != CIOException::eOverflow ) {
            // restore state
            m_Input.ResetBufferLock(pos0);
#if CHECK_INSTREAM_STATE
            m_CurrentTagState = state;
#endif
#if CHECK_INSTREAM_LIMITS
            m_CurrentTagLimit = lim;
#endif
            m_CurrentTagLength = 0;
            throw;
        }
    }
    catch ( ... ) {
        // restore state
        m_Input.ResetBufferLock(pos0);
#if CHECK_INSTREAM_STATE
        m_CurrentTagState = state;
#endif
#if CHECK_INSTREAM_LIMITS
        m_CurrentTagLimit = lim;
#endif
        m_CurrentTagLength = 0;
        throw;
    }
    // restore state
    m_Input.ResetBufferLock(pos0);
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = state;
#endif
#if CHECK_INSTREAM_LIMITS
    m_CurrentTagLimit = lim;
#endif
    m_CurrentTagLength = 0;

    if (pattern.size() != 0) {
        ITERATE( set<TTypeInfo>, t, known_types) {
            size_t pos = 0;
            CObjectTypeInfo ti(*t);
            if (ti.MatchPattern(pattern,pos,0) && pos == pattern.size()) {
                matching_types.insert(*t);
            }
        }
    }
    return matching_types;
}

// based on SkipAnyContent() method
void CObjectIStreamAsnBinary::GetTagPattern(vector<int>& pattern, size_t max_length)
{
    int counter = 0;
    TByte memtag = 0;
    pattern.clear();
    
    TByte prevbyte = 0, byte = PeekAnyTagFirstByte();
    pattern.push_back(0);
    pattern.push_back(0);
    if (byte & CAsnBinaryDefs::eContextSpecific) {
        pattern.push_back(0);
        prevbyte = byte;
    } else {
        pattern.push_back((int)(byte & CAsnBinaryDefs::eTagValueMask));
    }

    if ( GetTagConstructed(byte) && PeekIndefiniteLength() ) {
        ExpectIndefiniteLength();
    }
    else {
        pattern.clear();
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
            byte = PeekAnyTagFirstByte();
            ++counter;
            if ((counter%2 != 0 && (byte & CAsnBinaryDefs::eContextSpecific) == 0) ||
                (prevbyte & CAsnBinaryDefs::eContextSpecific) != 0) {
                memtag = prevbyte;
                ++counter;
            }
            if (counter%2 == 0) {
                pattern.push_back(depth);
                pattern.push_back((int)(memtag & CAsnBinaryDefs::eTagValueMask));
                if (byte & CAsnBinaryDefs::eContextSpecific) {
                    pattern.push_back(0);
                    prevbyte = byte;
                } else {
                    pattern.push_back((int)(byte & CAsnBinaryDefs::eTagValueMask));
                    prevbyte = 0;
                }
                if (pattern.size() >= max_length) {
                    return;
                }
            } else {
                memtag = byte;
            }
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

static inline
TTypeInfo MapType(const string& name)
{
    return CClassTypeInfoBase::GetClassInfoByName(name);
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

static unsigned cnt[256];
struct SPrint {
    ~SPrint() {
        for ( int i = 0; i < 256; ++i ) {
            if ( cnt[i] ) {
                cout << i << ": " << cnt[i] << endl;
            }
        }
    }
} s_print;

pair<TObjectPtr, TTypeInfo>
CObjectIStreamAsnBinary::ReadPointer(TTypeInfo declaredType)
{
    _TRACE("CObjectIStream::ReadPointer("<<declaredType->GetName()<<")");
    TObjectPtr objectPtr = 0;
    TTypeInfo objectType = 0;
    TByte byte = PeekTagByte();
    //cnt[byte] += 1;
    // variants:
    //    eUniversal,   !constructed, eNull             -> NULL
    //    eApplication,  constructed, eMemberReference  -> member reference
    //    eApplication,  constructed, eLongTag          -> other class
    //    eApplication, !constructed, eObjectReference  -> object reference
    // any other -> this class
    switch ( byte ) {
    case ASN_BINARY_MAKE_TAG_BYTE(eUniversal, ePrimitive, eNull):
        {
            _TRACE("CObjectIStream::ReadPointer: null");
#if CHECK_INSTREAM_STATE
            m_CurrentTagState = eTagParsed;
#endif
            m_CurrentTagLength = 1;
            ExpectShortLength(0);
            EndOfTag();
            return pair<TObjectPtr, TTypeInfo>((TObjectPtr)0, declaredType);
        }
    case ASN_BINARY_MAKE_TAG_BYTE(eApplication, ePrimitive, eObjectReference):
        {
            _TRACE("CObjectIStream::ReadPointer: @...");
            TObjectIndex index = ReadObjectPointer();
            _TRACE("CObjectIStream::ReadPointer: @" << index);
            const CReadObjectInfo& info = GetRegisteredObject(index);
            objectType = info.GetTypeInfo();
            objectPtr = info.GetObjectPtr();
            if ( !objectPtr ) {
                ThrowError(fFormatError,
                    "invalid reference to skipped object: object ptr is NULL");
            }
            break;
        }
    case ASN_BINARY_MAKE_TAG_BYTE(eApplication, eConstructed, eLongTag):
        {
            _TRACE("CObjectIStream::ReadPointer: new...");
            string className = ReadOtherPointer();
            _TRACE("CObjectIStream::ReadPointer: new " << className);
            objectType = MapType(className);

            BEGIN_OBJECT_FRAME2(eFrameNamed, objectType);
                
            CRef<CObject> ref;
            if ( objectType->IsCObject() ) {
                objectPtr = objectType->Create(GetMemoryPool());
                ref.Reset(static_cast<CObject*>(objectPtr));
            }
            else {
                objectPtr = objectType->Create();
            }
            RegisterObject(objectPtr, objectType);
            ReadObject(objectPtr, objectType);
            if ( objectType->IsCObject() )
                ref.Release();
                
            END_OBJECT_FRAME();

            ReadOtherPointerEnd();
            break;
        }
    case eContainterTagByte:
    case eContainterTagByte+1:
    default:
        {
            _TRACE("CObjectIStream::ReadPointer: new");
            CRef<CObject> ref;
            if ( declaredType->IsCObject() ) {
                objectPtr = declaredType->Create(GetMemoryPool());
                ref.Reset(static_cast<CObject*>(objectPtr));
            }
            else {
                objectPtr = declaredType->Create();
            }
            RegisterObject(objectPtr, declaredType);
            ReadObject(objectPtr, declaredType);
            if ( declaredType->IsCObject() )
                ref.Release();
            return make_pair(objectPtr, declaredType);
        }
    }
    while ( objectType != declaredType ) {
        // try to check parent class pointer
        if ( objectType->GetTypeFamily() != eTypeFamilyClass ) {
            ThrowError(fFormatError,"incompatible member type");
        }
        const CClassTypeInfo* parentClass =
            CTypeConverter<CClassTypeInfo>::SafeCast(objectType)->GetParentClassInfo();
        if ( parentClass ) {
            objectType = parentClass;
        }
        else {
            ThrowError(fFormatError,"incompatible member type");
        }
    }
    return make_pair(objectPtr, objectType);
}

void CObjectIStreamAsnBinary::SkipPointer(TTypeInfo declaredType)
{
    _TRACE("CObjectIStream::SkipPointer("<<declaredType->GetName()<<")");
    TByte byte = PeekTagByte();
    // variants:
    //    eUniversal,   !constructed, eNull             -> NULL
    //    eApplication,  constructed, eMemberReference  -> member reference
    //    eApplication,  constructed, eLongTag          -> other class
    //    eApplication, !constructed, eObjectReference  -> object reference
    // any other -> this class
    switch ( byte ) {
    case ASN_BINARY_MAKE_TAG_BYTE(eUniversal, ePrimitive, eNull):
        {
            _TRACE("CObjectIStream::SkipPointer: null");
#if CHECK_INSTREAM_STATE
            m_CurrentTagState = eTagParsed;
#endif
            m_CurrentTagLength = 1;
            ExpectShortLength(0);
            EndOfTag();
            return;
        }
        return;
    case ASN_BINARY_MAKE_TAG_BYTE(eApplication, ePrimitive, eObjectReference):
        {
            _TRACE("CObjectIStream::SkipPointer: @...");
            TObjectIndex index = ReadObjectPointer();
            _TRACE("CObjectIStream::SkipPointer: @" << index);
            GetRegisteredObject(index);
            break;
        }
    case ASN_BINARY_MAKE_TAG_BYTE(eApplication, eConstructed, eLongTag):
        {
            _TRACE("CObjectIStream::ReadPointer: new...");
            string className = ReadOtherPointer();
            _TRACE("CObjectIStream::ReadPointer: new " << className);
            TTypeInfo typeInfo = MapType(className);
            BEGIN_OBJECT_FRAME2(eFrameNamed, typeInfo);
                
            RegisterObject(typeInfo);
            SkipObject(typeInfo);

            END_OBJECT_FRAME();
            ReadOtherPointerEnd();
            break;
        }
    case eContainterTagByte:
    case eContainterTagByte+1:
    default:
        {
            _TRACE("CObjectIStream::ReadPointer: new");
            RegisterObject(declaredType);
            SkipObject(declaredType);
            break;
        }
    }
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

void CObjectIStreamAsnBinary::SkipString(EStringType type)
{
    ExpectStringTag(type);
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
