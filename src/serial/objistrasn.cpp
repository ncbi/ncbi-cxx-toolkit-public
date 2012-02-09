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
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/objistrasn.hpp>
#include <serial/impl/member.hpp>
#include <serial/enumvalues.hpp>
#include <serial/impl/memberlist.hpp>
#include <serial/objhook.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/impl/choice.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/impl/objistrimpl.hpp>
#include <math.h>
#if !defined(DBL_MAX_10_EXP) || !defined(FLT_MAX)
# include <float.h>
#endif

BEGIN_NCBI_SCOPE

CObjectIStream* CObjectIStream::CreateObjectIStreamAsn(void)
{
    return new CObjectIStreamAsn();
}

CObjectIStreamAsn::CObjectIStreamAsn(EFixNonPrint how)
    : CObjectIStream(eSerial_AsnText), m_FixMethod(how)
{
}

CObjectIStreamAsn::CObjectIStreamAsn(CNcbiIstream& in,
                                     EFixNonPrint how)
    : CObjectIStream(eSerial_AsnText), m_FixMethod(how)
{
    Open(in);
}

CObjectIStreamAsn::CObjectIStreamAsn(CNcbiIstream& in,
                                     bool deleteIn,
                                     EFixNonPrint how)
    : CObjectIStream(eSerial_AsnText), m_FixMethod(how)
{
    Open(in, deleteIn ? eTakeOwnership : eNoOwnership);
}

CObjectIStreamAsn::CObjectIStreamAsn(CNcbiIstream& in,
                                     EOwnership deleteIn,
                                     EFixNonPrint how)
    : CObjectIStream(eSerial_AsnText), m_FixMethod(how)
{
    Open(in, deleteIn);
}

CObjectIStreamAsn::CObjectIStreamAsn(const char* buffer,
                                     size_t size,
                                     EFixNonPrint how)
    : CObjectIStream(eSerial_AsnText), m_FixMethod(how)
{
    OpenFromBuffer(buffer, size);
}

bool CObjectIStreamAsn::EndOfData(void)
{
    if (CObjectIStream::EndOfData()) {
        return true;
    }
    try {
        SkipWhiteSpace();
    } catch (...) {
        return true;
    }
    return false;
}

string CObjectIStreamAsn::GetPosition(void) const
{
    return "line "+NStr::SizetToString(m_Input.GetLine());
}

inline
bool CObjectIStreamAsn::FirstIdChar(char c)
{
    return isalpha((unsigned char) c) || c == '_';
}

inline
bool CObjectIStreamAsn::IdChar(char c)
{
    return isalnum((unsigned char) c) || c == '_' || c == '.';
}

inline
char CObjectIStreamAsn::GetChar(void)
{
    return m_Input.GetChar();
}

inline
char CObjectIStreamAsn::PeekChar(void)
{
    return m_Input.PeekChar();
}

inline
void CObjectIStreamAsn::SkipEndOfLine(char c)
{
    m_Input.SkipEndOfLine(c);
}

inline
char CObjectIStreamAsn::SkipWhiteSpaceAndGetChar(void)
{
    char c = SkipWhiteSpace();
    m_Input.SkipChar();
    return c;
}

inline
char CObjectIStreamAsn::GetChar(bool skipWhiteSpace)
{
    return skipWhiteSpace? SkipWhiteSpaceAndGetChar(): m_Input.GetChar();
}

inline
char CObjectIStreamAsn::PeekChar(bool skipWhiteSpace)
{
    return skipWhiteSpace? SkipWhiteSpace(): m_Input.PeekChar();
}

inline
bool CObjectIStreamAsn::GetChar(char expect, bool skipWhiteSpace)
{
    if ( PeekChar(skipWhiteSpace) != expect ) {
        return false;
    }
    m_Input.SkipChar();
    return true;
}

void CObjectIStreamAsn::Expect(char expect, bool skipWhiteSpace)
{
    if ( !GetChar(expect, skipWhiteSpace) ) {
        string msg("\'");
        msg += expect;
        msg += "' expected";
        ThrowError(fFormatError, msg);
    }
}

bool CObjectIStreamAsn::Expect(char choiceTrue, char choiceFalse,
                               bool skipWhiteSpace)
{
    char c = GetChar(skipWhiteSpace);
    if ( c == choiceTrue ) {
        return true;
    }
    else if ( c == choiceFalse ) {
        return false;
    }
    m_Input.UngetChar(c);
    string msg("\'");
    msg += choiceTrue;
    msg += "' or '";
    msg += choiceFalse;
    msg += "' expected";
    ThrowError(fFormatError, msg);
    return false;
}

char CObjectIStreamAsn::SkipWhiteSpace(void)
{
    try { // catch CEofException
        for ( ;; ) {
            char c = m_Input.SkipSpaces();
            switch ( c ) {
            case '\t':
                m_Input.SkipChar();
                continue;
            case '\r':
            case '\n':
                m_Input.SkipChar();
                SkipEndOfLine(c);
                continue;
            case '-':
                // check for comments
                if ( m_Input.PeekChar(1) != '-' ) {
                    return '-';
                }
                m_Input.SkipChars(2);
                // skip comments
                SkipComments();
                continue;
            default:
                return c;
            }
        }
    } catch (CEofException& e) {
        if (GetStackDepth() <= 2) {
            throw;
        } else {
            // There should be no eof here, report as error
            ThrowError(fEOF, e.what());
        }
    }
    return '\0';
}

void CObjectIStreamAsn::SkipComments(void)
{
    try {
        for ( ;; ) {
            char c = GetChar();
            switch ( c ) {
            case '\r':
            case '\n':
                SkipEndOfLine(c);
                return;
            case '-':
                c = GetChar();
                switch ( c ) {
                case '\r':
                case '\n':
                    SkipEndOfLine(c);
                    return;
                case '-':
                    return;
                }
                continue;
            default:
                continue;
            }
        }
    }
    catch ( CEofException& /* ignored */ ) {
        return;
    }
}

CTempString CObjectIStreamAsn::ScanEndOfId(bool isId)
{
    if ( isId ) {
        for ( size_t i = 1; ; ++i ) {
            char c = m_Input.PeekCharNoEOF(i);
            if ( !IdChar(c) &&
                 (c != '-' || !IdChar(m_Input.PeekChar(i + 1))) ) {
                const char* ptr = m_Input.GetCurrentPos();
                m_Input.SkipChars(i);
                return CTempString(ptr, i);
            }
        }
    }
    return CTempString();
}

CTempString CObjectIStreamAsn::ReadTypeId(char c)
{
    if ( c == '[' ) {
        for ( size_t i = 1; ; ++i ) {
            switch ( m_Input.PeekChar(i) ) {
            case '\r':
            case '\n':
                ThrowError(fFormatError, "end of line: expected ']'");
                break;
            case ']':
                {
                    const char* ptr = m_Input.GetCurrentPos();
                    m_Input.SkipChars(i);
                    return CTempString(ptr + 1, i - 2);
                }
            }
        }
    }
    else {
        return ScanEndOfId(FirstIdChar(c));
    }
}

CTempString CObjectIStreamAsn::ReadNumber(void)
{
    char c = SkipWhiteSpace();
    if ( c != '-' && c != '+' && !isdigit((unsigned char) c) )
        ThrowError(fFormatError, "invalid number");
    for ( size_t i = 1; ; ++i ) {
        c = m_Input.PeekChar(i);
        if ( !isdigit((unsigned char) c) ) {
            const char* ptr = m_Input.GetCurrentPos();
            m_Input.SkipChars(i);
            return CTempString(ptr, i);
        }
    }
}

inline
CTempString CObjectIStreamAsn::ReadUCaseId(char c)
{
    return ScanEndOfId(isupper((unsigned char) c) != 0);
}

inline
CTempString CObjectIStreamAsn::ReadLCaseId(char c)
{
    return ScanEndOfId(islower((unsigned char) c) != 0);
}

inline
CTempString CObjectIStreamAsn::ReadMemberId(char c)
{
    if ( c == '[' ) {
        for ( size_t i = 1; ; ++i ) {
            switch ( m_Input.PeekChar(i) ) {
            case '\r':
            case '\n':
                ThrowError(fFormatError, "end of line: expected ']'");
                break;
            case ']':
                {
                    const char* ptr = m_Input.GetCurrentPos();
                    m_Input.SkipChars(++i);
                    return CTempString(ptr + 1, i - 2);
                }
            }
        }
    }
    else {
        return ScanEndOfId(islower((unsigned char) c) != 0);
    }
}

TMemberIndex CObjectIStreamAsn::GetAltItemIndex(
    const CClassTypeInfoBase* classType,
    const CTempString& id,
    const TMemberIndex pos /*= kInvalidMember*/)
{
    TMemberIndex idx = kInvalidMember;
    if (!id.empty()) {
        const CItemsInfo& info = classType->GetItems();
        string id_alt = string(id);
        id_alt[0] = toupper((unsigned char)id_alt[0]);
        if (pos != kInvalidMember) {
            idx = info.Find(CTempString(id_alt),pos);
        } else {
            idx = info.Find(CTempString(id_alt));
        }
        if (idx != kInvalidMember &&
            !info.GetItemInfo(idx)->GetId().HaveNoPrefix()) {
            idx = kInvalidMember;
        }
    }
    return idx;
}

TMemberIndex CObjectIStreamAsn::GetMemberIndex
    (const CClassTypeInfo* classType,
     const CTempString& id)
{
    TMemberIndex idx;
    if (!id.empty()  &&  isdigit((unsigned char) id[0])) {
        idx = classType->GetMembers().Find
            (CMemberId::TTag(NStr::StringToInt(id)));
    }
    else {
        idx = classType->GetMembers().Find(id);
        if (idx == kInvalidMember) {
            idx = GetAltItemIndex(classType,id);
        }
    }
    return idx;
}

TMemberIndex CObjectIStreamAsn::GetMemberIndex
    (const CClassTypeInfo* classType,
     const CTempString& id,
     const TMemberIndex pos)
{
    TMemberIndex idx;
    if (!id.empty()  &&  isdigit((unsigned char) id[0])) {
        idx = classType->GetMembers().Find
            (CMemberId::TTag(NStr::StringToInt(id)), pos);
    }
    else {
        idx = classType->GetMembers().Find(id, pos);
        if (idx == kInvalidMember) {
            idx = GetAltItemIndex(classType,id,pos);
        }
    }
    return idx;
}

TMemberIndex CObjectIStreamAsn::GetChoiceIndex
    (const CChoiceTypeInfo* choiceType,
     const CTempString& id)
{
    TMemberIndex idx;
    if (!id.empty()  &&  isdigit((unsigned char) id[0])) {
        idx = choiceType->GetVariants().Find
            (CMemberId::TTag(NStr::StringToInt(id)));
    }
    else {
        idx = choiceType->GetVariants().Find(id);
        if (idx == kInvalidMember) {
            idx = GetAltItemIndex(choiceType,id);
        }
    }
    return idx;
}

void CObjectIStreamAsn::ReadNull(void)
{
    if ( SkipWhiteSpace() == 'N' && 
         m_Input.PeekCharNoEOF(1) == 'U' &&
         m_Input.PeekCharNoEOF(2) == 'L' &&
         m_Input.PeekCharNoEOF(3) == 'L' &&
         !IdChar(m_Input.PeekCharNoEOF(4)) ) {
        m_Input.SkipChars(4);
    }
    else
        ThrowError(fFormatError, "'NULL' expected");
}

void CObjectIStreamAsn::ReadAnyContent(string& value)
{
    char buf[128];
    size_t pos=0;
    const size_t maxpos=128;

    char to = GetChar(true);
    buf[pos++] = to;
    if (to == '{') {
        to = '}';
    } else if (to == '\"') {
    } else {
        to = '\0';
    }

    bool space = false;
    for (char c = m_Input.PeekChar(); ; c = m_Input.PeekChar()) {
        if (to != '\"') {
            if (to != '}' && c == '\n') {
                value.append(buf,pos);
                return;
            }
            if (isspace((unsigned char) c)) {
                if (space) {
                    m_Input.SkipChar();
                    continue;
                }
                c = ' ';
                space = true;
            } else {
                space = false;;
            }
            if (to != '}' && (c == ',' || c == '}')) {
                value.append(buf,pos);
                return;
            } else if (c == '\"' || c == '{') {
                value.append(buf,pos);
                ReadAnyContent(value);
                pos = 0;
                continue;
            }
        }
        if (c == to) {
            if (pos >= maxpos) {
                value.append(buf,pos);
                pos = 0;
            }
            buf[pos++] = c;
            value.append(buf,pos);
            m_Input.SkipChar();
            return;
        }
        if (c == '\"' || c == '{') {
            value.append(buf,pos);
            ReadAnyContent(value);
            pos = 0;
            continue;
        }
        if (pos >= maxpos) {
            value.append(buf,pos);
            pos = 0;
        }
        buf[pos++] = c;
        m_Input.SkipChar();
    }
}

void CObjectIStreamAsn::ReadAnyContentObject(CAnyContentObject& obj)
{
    string value;
    ReadAnyContent(value);
    obj.SetValue(value);
}

void CObjectIStreamAsn::SkipAnyContent(void)
{
    char to = GetChar(true);
    if (to == '{') {
        to = '}';
    } else if (to == '\"') {
    } else {
        to = '\0';
    }
    for (char c = m_Input.PeekChar(); ; c = m_Input.PeekChar()) {
        if (to != '\"') {
            if (to != '}' && (c == '\n' || c == ',' || c == '}')) {
                return;
            } else if (c == '\"' || c == '{') {
                SkipAnyContent();
                continue;
            }
        }
        if (c == to) {
            m_Input.SkipChar();
            return;
        }
        if (c == '\"' || (c == '{' && to != '\"')) {
            SkipAnyContent();
            continue;
        }
        m_Input.SkipChar();
    }
}

void CObjectIStreamAsn::SkipAnyContentObject(void)
{
    SkipAnyContent();
}

void CObjectIStreamAsn::ReadBitString(CBitString& obj)
{
    obj.clear();
#if BITSTRING_AS_VECTOR
// CBitString is vector<bool>
    Expect('\'', true);
    string data;
    size_t reserve;
    const size_t step=128;
    data.reserve(reserve=step);
    bool hex=false;
    int c;
    for ( ; !hex; hex= c > 0x1) {
        c = GetHexChar();
        if (c < 0) {
            break;
        }
        data.append(1, char(c));
        if (--reserve == 0) {
            data.reserve(data.size() + (reserve=step));
        }
    }
    if (c<0 && !hex) {
        hex = m_Input.PeekChar() == 'H';
    }
    if (hex) {
        obj.reserve( data.size() * 4 );
        Uint1 byte;
        ITERATE( string, i, data) {
            byte = *i;
            for (Uint1 mask= 0x8; mask != 0; mask >>= 1) {
                obj.push_back( (byte & mask) != 0 );
            }
        }
        if (c > 0) {
            obj.reserve(obj.size() + (reserve=step));
            for (c= GetHexChar(); c >= 0; c= GetHexChar()) {
                byte = c;
                for (Uint1 mask= 0x8; mask != 0; mask >>= 1) {
                    obj.push_back( (byte & mask) != 0 );
                    if (--reserve == 0) {
                        obj.reserve(obj.size() + (reserve=step));
                    }
                }
            }
        }
        Expect('H');
    } else {
        obj.reserve( data.size() );
        ITERATE( string, i, data) {
            obj.push_back( *i != 0 );
        }
        Expect('B');
    }
    obj.reserve(obj.size());
#else
    obj.resize(0);
    if (TopFrame().HasMemberId() && TopFrame().GetMemberId().IsCompressed()) {
        ReadCompressedBitString(obj);
        return;
    }
    Expect('\'', true);
    string data;
    size_t reserve;
    const size_t step=128;
    data.reserve(reserve=step);
    bool hex=false;
    int c;
    for ( ; !hex; hex= c > 0x1) {
        c = GetHexChar();
        if (c < 0) {
            break;
        }
        data.append(1, char(c));
        if (--reserve == 0) {
            data.reserve(data.size() + (reserve=step));
        }
    }
    if (c<0 && !hex) {
        hex = m_Input.PeekChar() == 'H';
    }
    CBitString::size_type len = 0;
    if (hex) {
        Uint1 byte;
        obj.resize(4*data.size());
        ITERATE( string, i, data) {
            byte = *i;
            if (byte) {
                for (Uint1 mask= 0x8; mask != 0; mask >>= 1, ++len) {
                    if ((byte & mask) != 0) {
                        obj.set_bit(len);
                    }
                }
            } else {
                len += 4;
            }
        }
        if (c > 0) {
            for (c= GetHexChar(); c >= 0; c= GetHexChar()) {
                obj.resize( 4 + obj.size());
                byte = c;
                if (byte) {
                    for (Uint1 mask= 0x8; mask != 0; mask >>= 1, ++len) {
                        if ((byte & mask) != 0) {
                            obj.set_bit(len);
                        }
                    }
                } else {
                    len += 4;
                }
            }
        }
        Expect('H');
    } else {
        obj.resize(data.size());
        ITERATE( string, i, data) {
            if ( *i != 0 ) {
                obj.set_bit(len);
            }
            ++len;
        }
        Expect('B');
    }
    obj.resize(len);
#endif
}

void CObjectIStreamAsn::SkipBitString(void)
{
    SkipByteBlock();
}

string CObjectIStreamAsn::ReadFileHeader()
{
    CTempString id = ReadTypeId(SkipWhiteSpace());
    string s(id);
    if ( SkipWhiteSpace() == ':' && 
         m_Input.PeekCharNoEOF(1) == ':' &&
         m_Input.PeekCharNoEOF(2) == '=' ) {
        m_Input.SkipChars(3);
    }
    else
        ThrowError(fFormatError, "'::=' expected");
    return s;
}

TEnumValueType CObjectIStreamAsn::ReadEnum(const CEnumeratedTypeValues& values)
{
    // not integer
    CTempString id = ReadLCaseId(SkipWhiteSpace());
    if ( !id.empty() ) {
        // enum element by name
        return values.FindValue(id);
    }
    // enum element by value
    TEnumValueType value = m_Input.GetInt4();
    if ( !values.IsInteger() ) // check value
        values.FindName(value, false);
    
    return value;
}

bool CObjectIStreamAsn::ReadBool(void)
{
    switch ( SkipWhiteSpace() ) {
    case 'T':
        if ( m_Input.PeekCharNoEOF(1) == 'R' &&
             m_Input.PeekCharNoEOF(2) == 'U' &&
             m_Input.PeekCharNoEOF(3) == 'E' &&
             !IdChar(m_Input.PeekCharNoEOF(4)) ) {
            m_Input.SkipChars(4);
            return true;
        }
        break;
    case 'F':
        if ( m_Input.PeekCharNoEOF(1) == 'A' &&
             m_Input.PeekCharNoEOF(2) == 'L' &&
             m_Input.PeekCharNoEOF(3) == 'S' &&
             m_Input.PeekCharNoEOF(4) == 'E' &&
             !IdChar(m_Input.PeekCharNoEOF(5)) ) {
            m_Input.SkipChars(5);
            return false;
        }
        break;
    }
    ThrowError(fFormatError, "TRUE or FALSE expected");
    return false;
}

char CObjectIStreamAsn::ReadChar(void)
{
    string s;
    ReadString(s);
    if ( s.size() != 1 ) {
        ThrowError(fFormatError,
                   "\"" + s + "\": one char string expected");
    }
    return s[0];
}

Int4 CObjectIStreamAsn::ReadInt4(void)
{
    SkipWhiteSpace();
    return m_Input.GetInt4();
}

Uint4 CObjectIStreamAsn::ReadUint4(void)
{
    SkipWhiteSpace();
    return m_Input.GetUint4();
}

Int8 CObjectIStreamAsn::ReadInt8(void)
{
    SkipWhiteSpace();
    return m_Input.GetInt8();
}

Uint8 CObjectIStreamAsn::ReadUint8(void)
{
    SkipWhiteSpace();
    return m_Input.GetUint8();
}

double CObjectIStreamAsn::ReadDouble(void)
{
    if (PeekChar(true) != '{') {
        return NStr::StringToDouble( ScanEndOfId(true), NStr::fDecimalPosix );
    }
    Expect('{', true);
    CTempString mantissaStr = ReadNumber();
    size_t mantissaLength = mantissaStr.size();
    char buffer[128];
    if ( mantissaLength >= sizeof(buffer) - 1 )
        ThrowError(fOverflow, "buffer overflow");
    memcpy(buffer, mantissaStr.data(), mantissaLength);
    buffer[mantissaLength] = '\0';
    char* endptr;
    double mantissa = NStr::StringToDoublePosix(buffer, &endptr);
    if ( *endptr != 0 )
        ThrowError(fFormatError, "bad double in line "
            + NStr::SizetToString(m_Input.GetLine()));
    Expect(',', true);
    unsigned base = ReadUint4();
    Expect(',', true);
    int exp = ReadInt4();
    Expect('}', true);
    if ( base != 2 && base != 10 )
        ThrowError(fFormatError, "illegal REAL base (must be 2 or 10)");

    if ( base == 10 ) {     /* range checking only on base 10, for doubles */
        if ( exp > DBL_MAX_10_EXP )   /* exponent too big */
            ThrowError(fOverflow, "double overflow");
        else if ( exp < DBL_MIN_10_EXP )  /* exponent too small */
            return 0;
    }

    return mantissa * pow(double(base), exp);
}

void CObjectIStreamAsn::BadStringChar(size_t startLine, char c)
{
    ThrowError(fFormatError,
               "bad char in string starting at line "+
               NStr::SizetToString(startLine)+": "+
               NStr::IntToString(c));
}

void CObjectIStreamAsn::UnendedString(size_t startLine)
{
    ThrowError(fFormatError,
               "unclosed string starts at line "+
               NStr::SizetToString(startLine));
}


inline
void CObjectIStreamAsn::AppendStringData(string& s,
                                         size_t count,
                                         EFixNonPrint fix_method,
                                         size_t line)
{
    const char* data = m_Input.GetCurrentPos();
    if ( fix_method != eFNP_Allow ) {
        size_t done = 0;
        for ( size_t i = 0; i < count; ++i ) {
            char c = data[i];
            if ( !GoodVisibleChar(c) ) {
                if ( i > done ) {
                    s.append(data + done, i - done);
                }
                FixVisibleChar(c, fix_method, line);
                s += c;
                done = i + 1;
            }
        }
        if ( done < count ) {
            s.append(data + done, count - done);
        }
    }
    else {
        s.append(data, count);
    }
    if ( count > 0 ) {
        m_Input.SkipChars(count);
    }
}


void CObjectIStreamAsn::AppendLongStringData(string& s,
                                             size_t count,
                                             EFixNonPrint fix_method,
                                             size_t line)
{
    // Reserve extra-space to reduce heap reallocation
    if ( s.empty() ) {
        s.reserve(count*2);
    }
    else if ( s.capacity() < (s.size()+1)*1.1 ) {
        s.reserve(s.size()*2);
    }
    AppendStringData(s, count, fix_method, line);
}


void CObjectIStreamAsn::ReadStringValue(string& s, EFixNonPrint fix_method)
{
    Expect('\"', true);
    size_t startLine = m_Input.GetLine();
    size_t i = 0;
    s.erase();
    try {
        for (;;) {
            char c = m_Input.PeekChar(i);
            switch ( c ) {
            case '\r':
            case '\n':
                // flush string
                AppendLongStringData(s, i, fix_method, startLine);
                m_Input.SkipChar(); // '\r' or '\n'
                i = 0;
                // skip end of line
                SkipEndOfLine(c);
                break;
            case '\"':
                s.reserve(s.size() + i);
                AppendStringData(s, i, fix_method, startLine);
                m_Input.SkipChar(); // quote
                if ( m_Input.PeekCharNoEOF() != '\"' ) {
                    // end of string
                    return;
                }
                else {
                    // double quote -> one quote
                    i = 1;
                }
                break;
            default:
                // ok: append char
                if ( ++i == 128 ) {
                    // too long string -> flush it
                    AppendLongStringData(s, i, fix_method, startLine);
                    i = 0;
                }
                break;
            }
        }
    }
    catch ( CEofException& ) {
        SetFailFlags(fEOF);
        UnendedString(startLine);
        throw;
    }
}

void CObjectIStreamAsn::ReadString(string& s, EStringType type)
{
    ReadStringValue(s, type == eStringTypeUTF8? eFNP_Allow: m_FixMethod);
}

void CObjectIStreamAsn::SkipBool(void)
{
    switch ( SkipWhiteSpace() ) {
    case 'T':
        if ( m_Input.PeekCharNoEOF(1) == 'R' &&
             m_Input.PeekCharNoEOF(2) == 'U' &&
             m_Input.PeekCharNoEOF(3) == 'E' &&
             !IdChar(m_Input.PeekCharNoEOF(4)) ) {
            m_Input.SkipChars(4);
            return;
        }
        break;
    case 'F':
        if ( m_Input.PeekCharNoEOF(1) == 'A' &&
             m_Input.PeekCharNoEOF(2) == 'L' &&
             m_Input.PeekCharNoEOF(3) == 'S' &&
             m_Input.PeekCharNoEOF(4) == 'E' &&
             !IdChar(m_Input.PeekCharNoEOF(5)) ) {
            m_Input.SkipChars(5);
            return;
        }
        break;
    }
    ThrowError(fFormatError, "TRUE or FALSE expected");
}

void CObjectIStreamAsn::SkipChar(void)
{
    // TODO: check string length to be 1
    SkipString();
}

void CObjectIStreamAsn::SkipSNumber(void)
{
    size_t i;
    char c = SkipWhiteSpace();
    switch ( c ) {
    case '-':
    case '+':
        c = m_Input.PeekChar(1);
        // next char
        i = 2;
        break;
    default:
        // next char
        i = 1;
        break;
    }
    if ( c < '0' || c > '9' ) {
        ThrowError(fFormatError, "bad signed integer in line "
            + NStr::SizetToString(m_Input.GetLine()));
    }
    while ( (c = m_Input.PeekChar(i)) >= '0' && c <= '9' ) {
        ++i;
    }
    m_Input.SkipChars(i);
}

void CObjectIStreamAsn::SkipUNumber(void)
{
    size_t i;
    char c = SkipWhiteSpace();
    switch ( c ) {
    case '+':
        c = m_Input.PeekChar(1);
        // next char
        i = 2;
        break;
    default:
        // next char
        i = 1;
        break;
    }
    if ( c < '0' || c > '9' ) {
        ThrowError(fFormatError, "bad unsigned integer in line "
            + NStr::SizetToString(m_Input.GetLine()));
    }
    while ( (c = m_Input.PeekCharNoEOF(i)) >= '0' && c <= '9' ) {
        ++i;
    }
    m_Input.SkipChars(i);
}

void CObjectIStreamAsn::SkipFNumber(void)
{
    Expect('{', true);
    SkipSNumber();
    Expect(',', true);
    unsigned base = ReadUint4();
    Expect(',', true);
    SkipSNumber();
    Expect('}', true);
    if ( base != 2 && base != 10 )
        ThrowError(fFormatError, "illegal REAL base (must be 2 or 10)");
}

void CObjectIStreamAsn::SkipString(EStringType type)
{
    Expect('\"', true);
    size_t startLine = m_Input.GetLine();
    size_t i = 0;
    try {
        for (;;) {
            char c = m_Input.PeekChar(i);
            switch ( c ) {
            case '\r':
            case '\n':
                // flush string
                m_Input.SkipChars(i + 1);
                i = 0;
                // skip end of line
                SkipEndOfLine(c);
                break;
            case '\"':
                if ( m_Input.PeekChar(i + 1) == '\"' ) {
                    // double quote -> one quote
                    m_Input.SkipChars(i + 2);
                    i = 0;
                }
                else {
                    // end of string
                    m_Input.SkipChars(i + 1);
                    return;
                }
                break;
            default:
                if (type == eStringTypeVisible) {
                    FixVisibleChar(c, m_FixMethod, startLine);
                }
                // ok: skip char
                if ( ++i == 128 ) {
                    // too long string -> flush it
                    m_Input.SkipChars(i);
                    i = 0;
                }
                break;
            }
        }
    }
    catch ( CEofException& ) {
        SetFailFlags(fEOF);
        UnendedString(startLine);
        throw;
    }
}

void CObjectIStreamAsn::SkipNull(void)
{
    if ( SkipWhiteSpace() == 'N' &&
         m_Input.PeekCharNoEOF(1) == 'U' &&
         m_Input.PeekCharNoEOF(2) == 'L' &&
         m_Input.PeekCharNoEOF(3) == 'L' &&
         !IdChar(m_Input.PeekCharNoEOF(4)) ) {
        m_Input.SkipChars(4);
        return;
    }
    ThrowError(fFormatError, "NULL expected");
}

void CObjectIStreamAsn::SkipByteBlock(void)
{
    Expect('\'', true);
    for ( ;; ) {
        char c = GetChar();
        if ( c >= '0' && c <= '9' ) {
            continue;
        }
        else if ( c >= 'A' && c <= 'F' ) {
            continue;
        }
        else if ( c >= 'a' && c <= 'f' ) {
            continue;
        }
        else if ( c == '\'' ) {
            break;
        }
        else if ( c == '\r' || c == '\n' ) {
            SkipEndOfLine(c);
        }
        else {
            m_Input.UngetChar(c);
            ThrowError(fFormatError, "bad char in octet string: #"
                + NStr::IntToString(c));
        }
    }
    Expect('H', 'B', true);
}

void CObjectIStreamAsn::StartBlock(void)
{
    Expect('{', true);
    m_BlockStart = true;
}

bool CObjectIStreamAsn::NextElement(void)
{
    char c = SkipWhiteSpace();
    if ( m_BlockStart ) {
        // first element
        m_BlockStart = false;
        return c != '}';
    }
    else {
        // next element
        if ( c == ',' ) {
            m_Input.SkipChar();
            return true;
        }
        else if ( c != '}' )
            ThrowError(fFormatError, "',' or '}' expected");
        return false;
    }
}

void CObjectIStreamAsn::EndBlock(void)
{
    Expect('}');
}

void CObjectIStreamAsn::BeginContainer(const CContainerTypeInfo* /*cType*/)
{
    StartBlock();
}

void CObjectIStreamAsn::EndContainer(void)
{
    EndBlock();
}

bool CObjectIStreamAsn::BeginContainerElement(TTypeInfo /*elementType*/)
{
    return NextElement();
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamAsn::ReadContainer(const CContainerTypeInfo* contType,
                                      TObjectPtr contPtr)
{
    StartBlock();
    
    BEGIN_OBJECT_FRAME(eFrameArrayElement);

    CContainerTypeInfo::CIterator iter;
    bool old_element = contType->InitIterator(iter, contPtr);
    TTypeInfo elementType = contType->GetElementType();
    while ( NextElement() ) {
        if ( old_element ) {
            elementType->ReadData(*this, contType->GetElementPtr(iter));
            old_element = contType->NextElement(iter);
        }
        else {
            contType->AddElement(contPtr, *this);
        }
    }
    if ( old_element ) {
        contType->EraseAllElements(iter);
    }

    END_OBJECT_FRAME();

    EndBlock();
}

void CObjectIStreamAsn::SkipContainer(const CContainerTypeInfo* contType)
{
    StartBlock();

    TTypeInfo elementType = contType->GetElementType();
    BEGIN_OBJECT_FRAME(eFrameArrayElement);

    while ( NextElement() ) {
        SkipObject(elementType);
    }

    END_OBJECT_FRAME();

    EndBlock();
}
#endif

void CObjectIStreamAsn::BeginClass(const CClassTypeInfo* /*classInfo*/)
{
    StartBlock();
}

void CObjectIStreamAsn::EndClass(void)
{
    EndBlock();
}

void CObjectIStreamAsn::UnexpectedMember(const CTempString& id,
                                         const CItemsInfo& items)
{
    string message =
        "\""+string(id)+"\": unexpected member, should be one of: ";
    for ( CItemsInfo::CIterator i(items); i.Valid(); ++i ) {
        message += '\"' + items.GetItemInfo(i)->GetId().ToString() + "\" ";
    }
    ThrowError(fFormatError, message);
}

TMemberIndex
CObjectIStreamAsn::BeginClassMember(const CClassTypeInfo* classType)
{
    if ( !NextElement() )
        return kInvalidMember;

    CTempString id = ReadMemberId(SkipWhiteSpace());
    TMemberIndex index = GetMemberIndex(classType, id);
    if ( index == kInvalidMember ) {
        if (CanSkipUnknownMembers()) {
            SetFailFlags(fUnknownValue);
            SkipAnyContent();
            return BeginClassMember(classType);
        } else {
            UnexpectedMember(id, classType->GetMembers());
        }
    }
    return index;
}

TMemberIndex
CObjectIStreamAsn::BeginClassMember(const CClassTypeInfo* classType,
                                    TMemberIndex pos)
{
    if ( !NextElement() )
        return kInvalidMember;

    CTempString id = ReadMemberId(SkipWhiteSpace());
    TMemberIndex index = GetMemberIndex(classType, id, pos);
    if ( index == kInvalidMember ) {
        if (CanSkipUnknownMembers()) {
            SetFailFlags(fUnknownValue);
            SkipAnyContent();
            return BeginClassMember(classType, pos);
        } else {
            UnexpectedMember(id, classType->GetMembers());
        }
    }
    return index;
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamAsn::ReadClassRandom(const CClassTypeInfo* classType,
                                        TObjectPtr classPtr)
{
    BEGIN_OBJECT_FRAME3(eFrameClass, classType, classPtr);
    StartBlock();
    
    ReadClassRandomContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType)) != kInvalidMember ) {
        ReadClassRandomContentsMember(classPtr);
    }

    ReadClassRandomContentsEnd();
    
    EndBlock();
    END_OBJECT_FRAME();
}

void CObjectIStreamAsn::ReadClassSequential(const CClassTypeInfo* classType,
                                            TObjectPtr classPtr)
{
    BEGIN_OBJECT_FRAME3(eFrameClass, classType, classPtr);
    StartBlock();
    
    ReadClassSequentialContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType,*pos)) != kInvalidMember ) {
        ReadClassSequentialContentsMember(classPtr);
    }

    ReadClassSequentialContentsEnd(classPtr);
    
    EndBlock();
    END_OBJECT_FRAME();
}

void CObjectIStreamAsn::SkipClassRandom(const CClassTypeInfo* classType)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    StartBlock();
    
    SkipClassRandomContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType)) != kInvalidMember ) {
        SkipClassRandomContentsMember();
    }

    SkipClassRandomContentsEnd();
    
    EndBlock();
    END_OBJECT_FRAME();
}

void CObjectIStreamAsn::SkipClassSequential(const CClassTypeInfo* classType)
{
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    StartBlock();
    
    SkipClassSequentialContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType,*pos)) != kInvalidMember ) {
        SkipClassSequentialContentsMember();
    }

    SkipClassSequentialContentsEnd();
    
    EndBlock();
    END_OBJECT_FRAME();
}
#endif

void CObjectIStreamAsn::BeginChoice(const CChoiceTypeInfo* choiceType)
{
    if (choiceType->GetVariantInfo(kFirstMemberIndex)->GetId().IsAttlist()) {
        TopFrame().SetNotag();
        StartBlock();
    }
    m_BlockStart = true;
}
void CObjectIStreamAsn::EndChoice(void)
{
    if (TopFrame().GetNotag()) {
        SkipWhiteSpace();
        EndBlock();
    }
    m_BlockStart = false;
}

TMemberIndex CObjectIStreamAsn::BeginChoiceVariant(const CChoiceTypeInfo* choiceType)
{
    bool skipname = !m_BlockStart;
    if ( !NextElement() )
        return kInvalidMember;
    CTempString id = ReadMemberId(SkipWhiteSpace());
    if (skipname) {
        id = ReadMemberId(SkipWhiteSpace());
    }
    if ( id.empty() )
        ThrowError(fFormatError, "choice variant id expected");

    TMemberIndex index = GetChoiceIndex(choiceType, id);
    if ( index == kInvalidMember ) {
        if (CanSkipUnknownVariants()) {
            SetFailFlags(fUnknownValue);
        } else {
            UnexpectedMember(id, choiceType->GetVariants());
        }
    }

    return index;
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamAsn::ReadChoice(const CChoiceTypeInfo* choiceType,
                                   TObjectPtr choicePtr)
{
    TMemberIndex index = BeginChoiceVariant(choiceType);

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());

    variantInfo->ReadVariant(*this, choicePtr);

    END_OBJECT_FRAME();
}

void CObjectIStreamAsn::SkipChoice(const CChoiceTypeInfo* choiceType)
{
    TMemberIndex index = BeginChoiceVariant(choiceType);

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());

    variantInfo->SkipVariant(*this);

    END_OBJECT_FRAME();
}
#endif

void CObjectIStreamAsn::BeginBytes(ByteBlock& )
{
    Expect('\'', true);
}

int CObjectIStreamAsn::GetHexChar(void)
{
    for ( ;; ) {
        char c = GetChar();
        if ( c >= '0' && c <= '9' ) {
            return c - '0';
        }
        else if ( c >= 'A' && c <= 'F' ) {
            return c - 'A' + 10;
        }
        else if ( c >= 'a' && c <= 'f' ) {
            return c - 'a' + 10;
        }
        switch ( c ) {
        case '\'':
            return -1;
        case '\r':
        case '\n':
            SkipEndOfLine(c);
            break;
        default:
            m_Input.UngetChar(c);
            ThrowError(fFormatError, "bad char in octet string: #"
                + NStr::IntToString(c));
        }
    }
}

size_t CObjectIStreamAsn::ReadBytes(ByteBlock& block,
                                    char* dst, size_t length)
{
    size_t count = 0;
    while ( length-- > 0 ) {
        int c1 = GetHexChar();
        if ( c1 < 0 ) {
            block.EndOfBlock();
            return count;
        }
        int c2 = GetHexChar();
        if ( c2 < 0 ) {
            *dst++ = c1 << 4;
            count++;
            block.EndOfBlock();
            return count;
        }
        else {
            *dst++ = (c1 << 4) | c2;
            count++;
        }
    }
    return count;
}

void CObjectIStreamAsn::EndBytes(const ByteBlock& )
{
    Expect('H');
}

void CObjectIStreamAsn::BeginChars(CharBlock& )
{
    Expect('\"', true);
}

size_t CObjectIStreamAsn::ReadChars(CharBlock& block,
                                    char* dst, size_t length)
{
    size_t count = 0;
    try {
        while (length-- > 0) {
            char c = m_Input.GetChar();
            switch ( c ) {
            case '\r':
            case '\n':
                break;
            case '\"':
                if ( m_Input.PeekCharNoEOF() == '\"' ) {
                    m_Input.SkipChar();
                    dst[count++] = c;
                }
                else {
                    // end of string
                    // Check the string for non-printable characters
                    EFixNonPrint fix_method = m_FixMethod;
                    if ( fix_method != eFNP_Allow ) {
                        size_t line = m_Input.GetLine();
                        for (size_t i = 0;  i < count;  i++) {
                            FixVisibleChar(dst[i], fix_method, line);
                        }
                    }
                    block.EndOfBlock();
                    return count;
                }
                break;
            default:
                // ok: append char
                {
                    dst[count++] = c;
                }
                break;
            }
        }
    }
    catch ( CEofException& ) {
        SetFailFlags(fEOF);
        UnendedString(m_Input.GetLine());
        throw;
    }
    return count;
}

CObjectIStream::EPointerType CObjectIStreamAsn::ReadPointerType(void)
{
    switch ( PeekChar(true) ) {
    case 'N':
        if ( m_Input.PeekCharNoEOF(1) == 'U' &&
             m_Input.PeekCharNoEOF(2) == 'L' &&
             m_Input.PeekCharNoEOF(3) == 'L' &&
             !IdChar(m_Input.PeekCharNoEOF(4)) ) {
            // "NULL"
            m_Input.SkipChars(4);
            return eNullPointer;
        }
        break;
    case '@':
        m_Input.SkipChar();
        return eObjectPointer;
    case ':':
        m_Input.SkipChar();
        return eOtherPointer;
    default:
        break;
    }
    return eThisPointer;
}

CObjectIStream::TObjectIndex CObjectIStreamAsn::ReadObjectPointer(void)
{
    if ( sizeof(TObjectIndex) == sizeof(Int4) )
        return ReadInt4();
    else if ( sizeof(TObjectIndex) == sizeof(Int8) )
        return TObjectIndex(ReadInt8());
    else
        ThrowError(fIllegalCall, "invalid size of TObjectIndex:"
            " must be either sizeof(Int4) or sizeof(Int8)");
    return 0;
}

string CObjectIStreamAsn::ReadOtherPointer(void)
{
    return ReadTypeId(SkipWhiteSpace());
}

END_NCBI_SCOPE
