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
#include <corelib/tempstr.hpp>
#include <serial/objistrxml.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objhook.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/impl/choice.hpp>
#include <serial/impl/ptrinfo.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/impl/aliasinfo.hpp>
#include <serial/impl/memberlist.hpp>
#include <serial/impl/memberid.hpp>

BEGIN_NCBI_SCOPE

static
const char* s_SchemaInstanceNamespace = "http://www.w3.org/2001/XMLSchema-instance";

CObjectIStream* CObjectIStream::CreateObjectIStreamXml()
{
    return new CObjectIStreamXml();
}

CObjectIStreamXml::CObjectIStreamXml(void)
    : CObjectIStream(eSerial_Xml),
      m_TagState(eTagOutside), m_Attlist(false),
      m_StdXml(false), m_Doctype_found(false), m_IsNil(false),
      m_Encoding( eEncoding_Unknown ),
      m_StringEncoding( eEncoding_Unknown ),
      m_SkipNextTag(false)
{
    m_Utf8Pos = m_Utf8Buf.begin();
}

CObjectIStreamXml::~CObjectIStreamXml(void)
{
}

EEncoding CObjectIStreamXml::GetEncoding(void) const
{
    return m_Encoding;
}

void CObjectIStreamXml::SetDefaultStringEncoding(EEncoding enc)
{
    m_StringEncoding = enc;
}

EEncoding CObjectIStreamXml::GetDefaultStringEncoding(void) const
{
    return m_StringEncoding;
}

bool CObjectIStreamXml::EndOfData(void)
{
    if (CObjectIStream::EndOfData()) {
        return true;
    }
    try {
        SkipWSAndComments();
    } catch (...) {
        return true;
    }
    return false;
}

string CObjectIStreamXml::GetPosition(void) const
{
    return "line "+NStr::SizetToString(m_Input.GetLine());
}

void CObjectIStreamXml::SetEnforcedStdXml(bool set)
{
    if ( set ) {
        SetFlags(fFlagEnforcedStdXml);
    }
    else {
        ClearFlags(fFlagEnforcedStdXml);
    }
    if (set) {
        m_StdXml = false;
    }
}

template<typename Type> inline
Type CObjectIStreamXml::x_UseMemberDefault(void)
{
    return GetMemberDefault() ? CTypeConverter<Type>::Get(GetMemberDefault()) : Type();
}

static inline
bool IsBaseChar(char c)
{
    return
        (c >= 'A' && c <='Z') ||
        (c >= 'a' && c <= 'z') ||
        (c >= '\xC0' && c <= '\xD6') ||
        (c >= '\xD8' && c <= '\xF6') ||
        (c >= '\xF8' && c <= '\xFF');
}

static inline
bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

static inline
bool IsIdeographic(char /*c*/)
{
    return false;
}

static inline
bool IsLetter(char c)
{
    return IsBaseChar(c) || IsIdeographic(c);
}

static inline
bool IsFirstNameChar(char c)
{
    return IsLetter(c) || c == '_' || c == ':';
}

static inline
bool IsCombiningChar(char /*c*/)
{
    return false;
}

static inline
bool IsExtender(char c)
{
    return c == '\xB7';
}

static inline
bool IsNameChar(char c)
{
    return IsFirstNameChar(c) ||
        IsDigit(c) || c == '.' || c == '-' ||
        IsCombiningChar(c) || IsExtender(c);
}

static inline
bool IsWhiteSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static inline
bool IsEndOfTagChar(char c)
{
    return c == '>' || c == '/';
}

char CObjectIStreamXml::SkipWS(void)
{
//    _ASSERT(InsideTag());
    for ( ;; ) {
        char c = m_Input.SkipSpaces();
        switch ( c ) {
        case '\t':
            m_Input.SkipChar();
            continue;
        case '\r':
        case '\n':
            m_Input.SkipChar();
            m_Input.SkipEndOfLine(c);
            continue;
        default:
            return c;
        }
    }
}

char CObjectIStreamXml::SkipWSAndComments(void)
{
    _ASSERT(OutsideTag());
    for ( ;; ) {
        char c = m_Input.SkipSpaces();
        switch ( c ) {
        case '\t':
            m_Input.SkipChar();
            continue;
        case '\r':
        case '\n':
            m_Input.SkipChar();
            m_Input.SkipEndOfLine(c);
            continue;
        case '<':
// http://www.w3.org/TR/REC-xml/#dt-comment
            if ( m_Input.PeekChar(1) == '!' &&
                 m_Input.PeekChar(2) == '-' &&
                 m_Input.PeekChar(3) == '-' ) {
                // start of comment
                m_Input.SkipChars(4);
                if (m_Input.PeekChar(0) == '-' &&
                    m_Input.PeekChar(1) == '-') {
                        ThrowError(fFormatError,
                                   "double-hyphen '--' is not allowed in XML comments");
                }
                for ( ;; ) {
                    m_Input.FindChar('-');
                    if ( m_Input.PeekChar(1) == '-' ) {
                        // --
                        if ( m_Input.PeekChar(2) == '>' ) {
                            // -->
                            m_Input.SkipChars(3);
                            break;
                        }
                        else {
                            // --[^>]
                            ThrowError(fFormatError,
                                       "double-hyphen '--' is not allowed in XML comments");
                        }
                    }
                    else {
                        // -[^-]
                        m_Input.SkipChars(2);
                    }
                    
                }
                continue; // skip the next WS or comment
            }
            return '<';
        default:
            return c;
        }
    }
}

void CObjectIStreamXml::EndTag(void)
{
    char c = SkipWS();
    if (m_Attlist) {
        if (c == '=') {
            m_Input.SkipChar();
            c = SkipWS();
            if (c == '\"') {
                m_Input.SkipChar();
                return;
            }
        }
        if (c == '\"') {
            m_Input.SkipChar();
            m_TagState = eTagInsideOpening;
            return;
        }
        if (c == '/' && m_Input.PeekChar(1) == '>' ) {
            m_Input.SkipChars(2);
            m_TagState = eTagInsideOpening;
            Found_slash_gt();
            return;
        }
    }
    if ( c != '>' ) {
        c = ReadUndefinedAttributes();
        if ( c != '>' ) {
            ThrowError(fFormatError, "'>' expected");
        }
    }
    m_Input.SkipChar();
    Found_gt();
}

bool CObjectIStreamXml::EndOpeningTagSelfClosed(void)
{
    if (!StackIsEmpty() && TopFrame().GetNotag()) {
        return SelfClosedTag();
    }
    if( InsideOpeningTag() ) {
        char c = SkipWS();
        if (m_Attlist) {
            return false;
        }
        if ( c == '/' && m_Input.PeekChar(1) == '>' ) {
            // end of self closed tag
            m_Input.SkipChars(2);
            Found_slash_gt();
            return true;
        }

        if ( c != '>' ) {
            c = ReadUndefinedAttributes();
            if ( c == '/' && m_Input.PeekChar(1) == '>' ) {
                // end of self closed tag
                m_Input.SkipChars(2);
                Found_slash_gt();
                return true;
            }
            if ( c != '>' )
                ThrowError(fFormatError, "end of tag expected");
        }

        // end of open tag
        m_Input.SkipChar(); // '>'
        Found_gt();
    }
    return SelfClosedTag();
}

bool CObjectIStreamXml::UseSpecialCaseRead(void)
{
    if (ExpectSpecialCase()==0) {
        return false;
    }
    bool empty = !m_Attlist &&
        (   SelfClosedTag() ||
            EndOpeningTagSelfClosed() ||
            (m_Input.PeekChar(0) == '<' && m_Input.PeekChar(1) == '/')
        );
    if (empty) {
        if (m_IsNil && (ExpectSpecialCase() & CObjectIStream::eReadAsNil)!=0) {
            m_IsNil=false;
            SetSpecialCaseUsed(CObjectIStream::eReadAsNil);
//            NCBI_THROW(CSerialException,eNullValue, kEmptyStr);
            return true;
        }
        if ( GetMemberDefault()) {
            SetSpecialCaseUsed(CObjectIStream::eReadAsDefault);
            return true;
        }
    }
    return false;
}

char CObjectIStreamXml::BeginOpeningTag(void)
{
    BeginData();
    // find beginning '<'
    char c = SkipWSAndComments();
    if ( c != '<' )
        ThrowError(fFormatError, "'<' expected");
    c = m_Input.PeekChar(1);
    if ( c == '/' )
        ThrowError(fFormatError, "unexpected '</'");
    m_Input.SkipChar();
    Found_lt();
    return c;
}

char CObjectIStreamXml::BeginClosingTag(void)
{
    BeginData();
    // find beginning '<'
    char c = SkipWSAndComments();
    if ( c != '<' || m_Input.PeekChar(1) != '/' )
        ThrowError(fFormatError, "'</' expected");
    m_Input.SkipChars(2);
    Found_lt_slash();
    return m_Input.PeekChar();
}

CTempString CObjectIStreamXml::ReadName(char c)
{
    _ASSERT(InsideTag());
    if ( !IsFirstNameChar(c) )
        ThrowError(fFormatError,
            "Name begins with an invalid character: #"
            +NStr::UIntToString((unsigned int)c));

    // find end of tag name
    size_t i = 1, iColon = 0;
    while ( IsNameChar(c = m_Input.PeekChar(i)) ) {
        if (!m_Doctype_found && c == ':') {
            iColon = i+1;
        }
        ++i;
    }

    // save beginning of tag name
    const char* ptr = m_Input.GetCurrentPos();

    // check end of tag name
    m_Input.SkipChars(i);
    if (c == '\n' || c == '\r') {
        m_Input.SkipChar();
        m_Input.SkipEndOfLine(c);
    }
    m_LastTag = CTempString(ptr+iColon, i-iColon);
    if (iColon > 1) {
        string ns_prefix( CTempString(ptr, iColon-1));
        if (ns_prefix == "xmlns") {
            string value;
            ReadAttributeValue(value, true);
            if (m_LastTag == m_CurrNsPrefix) {
                size_t depth = GetStackDepth();
                TTypeInfo type=0;
                    if (depth > 1 && FetchFrameFromTop(1).HasTypeInfo()) {
                        type = FetchFrameFromTop(1).GetTypeInfo();
                        if (type->GetName().empty() &&
                            depth > 3 && FetchFrameFromTop(3).HasTypeInfo()) {
                                type = FetchFrameFromTop(3).GetTypeInfo();
                        }
                    }
                if (type) {
                    type->SetNamespacePrefix(m_CurrNsPrefix);
                    type->SetNamespaceName(value);
                }
            }
            m_NsPrefixToName[m_LastTag] = value;
            m_NsNameToPrefix[value] = m_LastTag;
            char ch = SkipWS();
            return IsEndOfTagChar(ch) ? CTempString() : ReadName(ch);
        } else if (ns_prefix == "xml") {
            iColon = 0;
        } else {
            m_CurrNsPrefix = ns_prefix;
        }
    } else {
        if (!m_Attlist) {
            m_CurrNsPrefix.erase();
        }
        if (m_Attlist && m_LastTag == "xmlns") {
            string value;
            ReadAttributeValue(value, true);
            if (GetStackDepth() > 1 && FetchFrameFromTop(1).HasTypeInfo()) {
                TTypeInfo type = FetchFrameFromTop(1).GetTypeInfo();
                type->SetNamespacePrefix(m_CurrNsPrefix);
                type->SetNamespaceName(value);
            }
            m_NsPrefixToName[m_LastTag] = value;
            m_NsNameToPrefix[value] = m_LastTag;
            char ch = SkipWS();
            return IsEndOfTagChar(ch) ? CTempString() : ReadName(ch);
        }
    }
#if defined(NCBI_SERIAL_IO_TRACE)
    cout << ", Read= " << m_LastTag;
#endif
    if (m_Attlist && m_LastTag.size() == 3 &&
        m_LastTag == "nil" &&
        (m_NsPrefixToName.find(m_CurrNsPrefix) == m_NsPrefixToName.end() ||
        NStr::strcmp(m_NsPrefixToName[m_CurrNsPrefix].c_str(),s_SchemaInstanceNamespace)== 0)) {
        string value;
        ReadAttributeValue(value, true);
        m_IsNil = NStr::StringToBool(value);
        char ch = SkipWS();
        return IsEndOfTagChar(ch) ? CTempString() : ReadName(ch);
    }
    return CTempString(ptr+iColon, i-iColon);
}

CTempString CObjectIStreamXml::RejectedName(void)
{
    _ASSERT(!m_RejectedTag.empty());
    m_LastTag = m_RejectedTag;
    m_RejectedTag.erase();
    m_TagState = eTagInsideOpening;
#if defined(NCBI_SERIAL_IO_TRACE)
    cout << ", Redo= " << m_LastTag;
#endif
    return m_LastTag;
}

void CObjectIStreamXml::SkipAttributeValue(char c)
{
    _ASSERT(InsideOpeningTag());
    m_Input.SkipChar();
    m_Input.FindChar(c);
    m_Input.SkipChar();
}

void CObjectIStreamXml::SkipQDecl(void)
{
    _ASSERT(InsideOpeningTag());
    m_Input.SkipChar();

    CTempString tagName;
    tagName = ReadName( SkipWS());
//    _ASSERT(tagName == "xml");
    for (;;) {
        char ch = SkipWS();
        if (ch == '?') {
            break;
        }
        tagName = ReadName(ch);
        string value;
        ReadAttributeValue(value);
        if (tagName == "encoding") {
            if (NStr::CompareNocase(value.c_str(),"UTF-8") == 0) {
                m_Encoding = eEncoding_UTF8;
            } else if (NStr::CompareNocase(value.c_str(),"ISO-8859-1") == 0) {
                m_Encoding = eEncoding_ISO8859_1;
            } else if (NStr::CompareNocase(value.c_str(),"Windows-1252") == 0) {
                m_Encoding = eEncoding_Windows_1252;
            } else {
                ThrowError(fFormatError, "unsupported encoding: " + value);
            }
            break;
        }
    }
    for ( ;; ) {
        m_Input.FindChar('?');
        if ( m_Input.PeekChar(1) == '>' ) {
            // ?>
            m_Input.SkipChars(2);
            Found_gt();
            return;
        }
        else
            m_Input.SkipChar();
    }
}

string CObjectIStreamXml::ReadFileHeader(void)
{
// check for UTF8 Byte Order Mark (EF BB BF)
// http://unicode.org/faq/utf_bom.html#BOM
    {
        char c = m_Input.PeekChar();
        if ((unsigned char)c == 0xEF) {
            if ((unsigned char)m_Input.PeekChar(1) == 0xBB &&
                (unsigned char)m_Input.PeekChar(2) == 0xBF) {
                m_Input.SkipChars(3);
                m_Encoding = eEncoding_UTF8;
            }
        }
    }
    
    m_Doctype_found = false;
    for ( ;; ) {
        switch ( BeginOpeningTag() ) {
        case '?':
            SkipQDecl();
            break;
        case '!':
            {
                m_Input.SkipChar();
                CTempString tagName = ReadName(m_Input.PeekChar());
                if ( tagName == "DOCTYPE" ) {
                    m_Doctype_found = true;
                    CTempString docType = ReadName(SkipWS());
                    // skip the rest of !DOCTYPE
                    for ( ;; ) {
                        char c = SkipWS();
                        if ( c == '>' ) {
                            m_Input.SkipChar();
                            Found_gt();
                            break;
                        }
                        else if ( c == '"' || c == '\'' ) {
                            SkipAttributeValue(c);
                        }
                        else {
                            ReadName(c);
                        }
                    }
                }
                else {
                    // unknown tag
                    ThrowError(fFormatError,
                        "unknown tag in file header: "+string(tagName));
                }
            }
            break;
        default:
            {
                string typeName = ReadName(m_Input.PeekChar());
                if (!m_Doctype_found && !StackIsEmpty()) {
                    // verify typename
                    const CObjectStack::TFrame& top = TopFrame();
                    if (top.GetFrameType() == CObjectStackFrame::eFrameNamed &&
                        top.HasTypeInfo()) {
                        const string& tname = top.GetTypeInfo()->GetName();
                        if ( !typeName.empty() && !tname.empty() && typeName != tname ) {
                            string tmp = m_CurrNsPrefix + ":" + typeName;
                            if (tmp == tname) {
                                typeName = tmp;
                                m_LastTag = tmp;
                                m_CurrNsPrefix.erase();
                                m_Doctype_found = true;
                            }
                        }
                    }
                }
                UndoClassMember();
                return typeName;
            }
/*
            m_Input.UngetChar('<');
            Back_lt();
            ThrowError(fFormatError, "unknown DOCTYPE");
*/
        }
    }
    return NcbiEmptyString;
}

string CObjectIStreamXml::PeekNextTypeName(void)
{
    if (!m_RejectedTag.empty()) {
        return m_RejectedTag;
    }
    string typeName = ReadName(BeginOpeningTag());
    UndoClassMember();
    return typeName;
}

void CObjectIStreamXml::FindFileHeader(bool find_XMLDecl)
{
    char c;
    for (;;) {
        c = m_Input.PeekChar();
        if (c == '<') {
            if (!find_XMLDecl) {
                return;
            }
            if (m_Input.PeekChar(1) == '?' &&
                m_Input.PeekChar(2) == 'x' &&
                m_Input.PeekChar(3) == 'm' &&
                m_Input.PeekChar(4) == 'l') {
                return;
            }
        }
        m_Input.SkipChar();
    }
}

void CObjectIStreamXml::x_EndTypeNamespace(void)
{
    if (x_IsStdXml()) {
        if (TopFrame().HasTypeInfo()) {
            TTypeInfo type = TopFrame().GetTypeInfo();
            if (type->HasNamespaceName()) {
                string nsName = type->GetNamespaceName();
                string nsPrefix = m_NsNameToPrefix[nsName];
// not sure about it - should we erase them or not?
//                m_NsNameToPrefix.erase(nsName);
//                m_NsPrefixToName.erase(nsPrefix);
            }
        }
        if (GetStackDepth() <= 2) {
            m_NsNameToPrefix.clear();
            m_NsPrefixToName.clear();
        }
    }
}

int CObjectIStreamXml::ReadEscapedChar(char endingChar, bool* encoded)
{
    char c = m_Input.PeekChar();
    if (encoded) {
        *encoded = false;
    }
    if ( c == '&' ) {
        if (encoded) {
            *encoded = true;
        }
        m_Input.SkipChar();
        const size_t limit = 32;
        size_t offset = m_Input.PeekFindChar(';', limit);
        if ( offset >= limit )
            ThrowError(fFormatError, "entity reference is too long");
        const char* p = m_Input.GetCurrentPos(); // save entity string pointer
        m_Input.SkipChars(offset + 1); // skip it
        if ( offset == 0 )
            ThrowError(fFormatError, "invalid entity reference");
        if ( *p == '#' ) {
            const char* end = p + offset;
            ++p;
            // char ref
            if ( p == end )
                ThrowError(fFormatError, "invalid char reference");
            unsigned v = 0;
            if ( *p == 'x' ) {
                // hex
                if ( ++p == end )
                    ThrowError(fFormatError, "invalid char reference");
                do {
                    c = *p++;
                    if ( c >= '0' && c <= '9' )
                        v = v * 16 + (c - '0');
                    else if ( c >= 'A' && c <='F' )
                        v = v * 16 + (c - 'A' + 0xA);
                    else if ( c >= 'a' && c <='f' )
                        v = v * 16 + (c - 'a' + 0xA);
                    else
                        ThrowError(fFormatError,
                            "invalid symbol in char reference");
                } while ( p < end );
            }
            else {
                // dec
                if ( p == end )
                    ThrowError(fFormatError, "invalid char reference");
                do {
                    c = *p++;
                    if ( c >= '0' && c <= '9' )
                        v = v * 10 + (c - '0');
                    else
                        ThrowError(fFormatError,
                            "invalid symbol in char reference");
                } while ( p < end );
            }
            return v & 0xFF;
        }
        else {
            CTempString e(p, offset);
            if ( e == "lt" )
                return '<';
            if ( e == "gt" )
                return '>';
            if ( e == "amp" )
                return '&';
            if ( e == "apos" )
                return '\'';
            if ( e == "quot" )
                return '"';
            ThrowError(fFormatError, "unknown entity name: " + string(e));
        }
    }
    else if ( c == endingChar ) {
        return -1;
    }
    m_Input.SkipChar();
    return c & 0xFF;
}

/*
In XML 1.1, almost all chars are allowed:
http://www.w3.org/TR/xml11/#NT-Char
BUT, we declare this as xml 1.0:
    CObjectOStreamXml::WriteFileHeader
Once so, some chars are not allowed
http://www.w3.org/TR/xml/#charsets
    
*/
inline bool BAD_CHAR(int x) {
    return (x < 0x20 && x > 0x0 && x != 0x9 && x != 0xA && x != 0xD);
}
inline int CObjectIStreamXml::x_VerifyChar(int x) {
    return BAD_CHAR(x) ?
        ReplaceVisibleChar(x, x_FixCharsMethod(), this, kEmptyStr) : x;
}
inline
int CObjectIStreamXml::ReadEncodedChar(char endingChar, EStringType type, bool* encoded)
{
    return x_VerifyChar(x_ReadEncodedChar(endingChar,type,encoded));
}

int CObjectIStreamXml::x_ReadEncodedChar(char endingChar, EStringType type, bool* encoded)
{
    EEncoding enc_out( type == eStringTypeUTF8 ? eEncoding_UTF8 : m_StringEncoding);
    EEncoding enc_in(m_Encoding == eEncoding_Unknown ? eEncoding_UTF8 : m_Encoding);

    if (enc_out == eEncoding_UTF8 &&
        !m_Utf8Buf.empty() && m_Utf8Pos != m_Utf8Buf.end()) {
        if (++m_Utf8Pos != m_Utf8Buf.end()) {
            return *m_Utf8Pos & 0xFF;
        } else {
            m_Utf8Buf.erase();
        }
    }
    if (enc_in != enc_out && enc_out != eEncoding_Unknown) {
        int c = ReadEscapedChar(endingChar, encoded);
        if (c < 0) {
            return c;
        }
        if (enc_out != eEncoding_UTF8) {
            TUnicodeSymbol chU = enc_in == eEncoding_UTF8 ?
                ReadUtf8Char(c) : CUtf8::CharToSymbol( c, enc_in);
            Uint1 ch = CUtf8::SymbolToChar( chU, enc_out);
            return ch & 0xFF;
        }
        if ((c & 0x80) == 0) {
            return c;
        }
        char ch = c;
        m_Utf8Buf = CUtf8::AsUTF8( CTempString(&ch,1), enc_in);
        m_Utf8Pos = m_Utf8Buf.begin();
        return *m_Utf8Pos & 0xFF;
    }
    return ReadEscapedChar(endingChar, encoded);
}

TUnicodeSymbol CObjectIStreamXml::ReadUtf8Char(char c)
{
    size_t more = 0;
    TUnicodeSymbol chU = CUtf8::DecodeFirst(c, more);
    while (chU && more--) {
        chU = CUtf8::DecodeNext(chU, m_Input.GetChar());
    }
    if (chU == 0) {
        ThrowError(fInvalidData, "invalid UTF8 string");
    }
    return chU;
}

CTempString CObjectIStreamXml::ReadAttributeName(void)
{
    if ( OutsideTag() )
        ThrowError(fFormatError, "attribute expected");
    return ReadName(SkipWS());
}

void CObjectIStreamXml::ReadAttributeValue(string& value, bool skipClosing)
{
    if ( SkipWS() != '=' )
        ThrowError(fFormatError, "'=' expected");
    m_Input.SkipChar(); // '='
    char startChar = SkipWS();
    if ( startChar != '\'' && startChar != '\"' )
        ThrowError(fFormatError, "attribute value must start with ' or \"");
    m_Input.SkipChar();
    for ( ;; ) {
        int c = ReadEncodedChar(startChar,eStringTypeUTF8);
        if ( c < 0 )
            break;
        value += char(c);
    }
    if (!m_Attlist || skipClosing) {
        m_Input.SkipChar();
    }
}

char CObjectIStreamXml::ReadUndefinedAttributes(void)
{
    char c;
    m_Attlist = true;
    for (;;) {
        c = SkipWS();
        if (IsEndOfTagChar(c)) {
            m_Attlist = false;
            break;
        }
        CTempString tagName = ReadName(c);
        if (!tagName.empty()) {
            string value;
            ReadAttributeValue(value, true);
        }
    }
    return c;
}

bool CObjectIStreamXml::ReadBool(void)
{
    CTempString attr;
// accept both   <a>true</a>   and   <a value="true"/>
// for compatibility with ASN-generated classes
    string sValue;
    bool haveattr=false;
    if (!m_Attlist) {
        while (HasAttlist()) {
            attr = ReadAttributeName();
            if ( attr == "value" ) {    
                ReadAttributeValue(sValue);
                haveattr = true;
                continue;
            }
            if ( attr == "nil") {    
                m_IsNil = true;                
            }
            string value;
            ReadAttributeValue(value);
        }
        if (ExpectSpecialCase()!=0 && UseSpecialCaseRead()) {
            return x_UseMemberDefault<bool>();
        }
    }
    if (!haveattr) {
        ReadTagData(sValue);
    }
    NStr::TruncateSpacesInPlace(sValue);

// http://www.w3.org/TR/xmlschema11-2/#boolean
    bool value;
    if ( sValue == "true"  || sValue == "1")
        value = true;
    else {
        if ( sValue != "false"  && sValue != "0") {
            ThrowError(fFormatError,
                       "'true' or 'false' value expected: "+sValue);
        }
        value = false;
    }
    if ( !m_Attlist && !EndOpeningTagSelfClosed() && !NextTagIsClosing() )
        ThrowError(fFormatError, "boolean tag must have empty contents");
    return value;
}

char CObjectIStreamXml::ReadChar(void)
{
    if (ExpectSpecialCase()!=0 && UseSpecialCaseRead()) {
        return x_UseMemberDefault<char>();
    }
    BeginData();
    int c = ReadEscapedChar('<');
    if ( c < 0 || m_Input.PeekChar() != '<' )
        ThrowError(fFormatError, "one char tag content expected");
    return c;
}

Int4 CObjectIStreamXml::ReadInt4(void)
{
    if (ExpectSpecialCase()!=0 && UseSpecialCaseRead()) {
        return x_UseMemberDefault<Int4>();
    }
    BeginData();
    return m_Input.GetInt4();
}

Uint4 CObjectIStreamXml::ReadUint4(void)
{
    if (ExpectSpecialCase()!=0 && UseSpecialCaseRead()) {
        return x_UseMemberDefault<Uint4>();
    }
    BeginData();
    return m_Input.GetUint4();
}

Int8 CObjectIStreamXml::ReadInt8(void)
{
    if (ExpectSpecialCase()!=0 && UseSpecialCaseRead()) {
        return x_UseMemberDefault<Int8>();
    }
    BeginData();
    return m_Input.GetInt8();
}

Uint8 CObjectIStreamXml::ReadUint8(void)
{
    if (ExpectSpecialCase()!=0 && UseSpecialCaseRead()) {
        return x_UseMemberDefault<Uint8>();
    }
    BeginData();
    return m_Input.GetUint8();
}

double CObjectIStreamXml::ReadDouble(void)
{
    if (ExpectSpecialCase()!=0 && UseSpecialCaseRead()) {
        return x_UseMemberDefault<double>();
    }
    string s;
    ReadTagData(s);
    char* endptr;
    double data = NStr::StringToDoublePosix(s.c_str(), &endptr);
    while (IsWhiteSpace(*endptr)) {
        ++endptr;
    }
    if ( *endptr != 0 )
        ThrowError(fFormatError, "invalid float number");
    return data;
}

void CObjectIStreamXml::ReadNull(void)
{
    if ( !EndOpeningTagSelfClosed() && !NextTagIsClosing() )
        ThrowError(fFormatError, "empty tag expected");
}

bool CObjectIStreamXml::ReadAnyContent(const string& ns_prefix, string& value)
{
    if (ThisTagIsSelfClosed()) {
        EndSelfClosedTag();
        return false;
    }
    while (!NextTagIsClosing()) {
        while (NextIsTag()) {
            string tagAny;
            tagAny = ReadName(BeginOpeningTag());
            value += '<';
            value += tagAny;
            while (HasAttlist()) {
                string attribName = ReadName(SkipWS());
                if (attribName.empty()) {
                    break;
                }
                if (m_CurrNsPrefix.empty() || m_CurrNsPrefix == ns_prefix) {
                    value += " ";
                    value += attribName;
                    value += "=\"";
                    string attribValue;
                    ReadAttributeValue(attribValue, true);
                    value += attribValue;
                    value += "\"";
                } else {
                    // skip attrib from different namespaces
                    string attribValue;
                    ReadAttributeValue(attribValue, true);
                }
            }
            string value2;
            if (ReadAnyContent(ns_prefix, value2)) {
                CloseTag(tagAny);
            }
            if (value2.empty()) {
                value += "/>";
            } else {
                value += '>';
                value += value2;
                value += "</";
                value += tagAny;
                value += '>';
            }
        }
        string data;
        ReadTagData(data,eStringTypeUTF8);
        value += data;
    }
    return true;
}

void CObjectIStreamXml::ReadAnyContentObject(CAnyContentObject& obj)
{
    obj.Reset();
    string tagName;
    if (!m_RejectedTag.empty()) {
        tagName = RejectedName();
        obj.SetName( tagName);
    } else if (!StackIsEmpty() && TopFrame().HasMemberId()) {
        obj.SetName( TopFrame().GetMemberId().GetName());
    }
    string ns_prefix(m_CurrNsPrefix);

    BEGIN_OBJECT_FRAME(eFrameOther);
    while (HasAttlist()) {
        string attribName = ReadName(SkipWS());
        if (attribName.empty()) {
            break;
        }
        string value;
        ReadAttributeValue(value, true);
        if (attribName == "xmlns") {
            m_NsPrefixToName[ns_prefix] = value;
            m_NsNameToPrefix[value] = ns_prefix;
        } else {
            obj.AddAttribute( attribName, m_NsPrefixToName[m_CurrNsPrefix],CUtf8::AsUTF8(value,eEncoding_UTF8));
        }
    }
    obj.SetNamespacePrefix(ns_prefix);
    obj.SetNamespaceName(m_NsPrefixToName[ns_prefix]);
    string value;
    if (ReadAnyContent(ns_prefix,value) && !tagName.empty()) {
        CloseTag(tagName);
    }
    obj.SetValue(CUtf8::AsUTF8(value,eEncoding_UTF8));
    END_OBJECT_FRAME();
}

bool CObjectIStreamXml::SkipAnyContent(void)
{
    if (ThisTagIsSelfClosed()) {
        //EndSelfClosedTag();
        return true;
    }
    while (!NextTagIsClosing()) {
        while (NextIsTag()) {
            string tagName = ReadName(BeginOpeningTag());
            if (SkipAnyContent()) {
                CloseTag(tagName);
            }
        }
        string data;
        ReadTagData(data);
    }
    return true;
}

void CObjectIStreamXml::SkipAnyContentObject(void)
{
    string tagName;
    if (!m_RejectedTag.empty()) {
        tagName = RejectedName();
    }
    if (SkipAnyContent() && !tagName.empty()) {
        CloseTag(tagName);
    }
}

void CObjectIStreamXml::ReadBitString(CBitString& obj)
{
    obj.clear();
#if BITSTRING_AS_VECTOR
    if (EndOpeningTagSelfClosed()) {
        return;
    }
    BeginData();
    size_t reserve;
    const size_t step=128;
    obj.reserve( reserve=step );
    for (int c= GetHexChar(); c >= 0; c= GetHexChar()) {
        Uint1 byte = c;
        for (Uint1 mask= 0x8; mask != 0; mask >>= 1) {
            obj.push_back( (byte & mask) != 0 );
            if (--reserve == 0) {
                obj.reserve(obj.size() + (reserve=step));
            }
        }
    }
    obj.reserve(obj.size());
#else
    obj.resize(0);
    if (EndOpeningTagSelfClosed()) {
        return;
    }
    if (TopFrame().HasMemberId() && TopFrame().GetMemberId().IsCompressed()) {
        ReadCompressedBitString(obj);
        return;
    }
    BeginData();
    CBitString::size_type len = 0;
    for ( ;; ++len) {
        char c = m_Input.GetChar();
        if (c == '1') {
            obj.resize(len+1);
            obj.set_bit(len);
        } else if (c != '0') {
            if (IsWhiteSpace(c)) {
                --len;
                continue;
            }
            m_Input.UngetChar(c);
            if ( c == '<' )
                break;
            ThrowError(fFormatError, "invalid char in bit string");
        }
    }
    obj.resize(len);
#endif
}

void CObjectIStreamXml::SkipBitString(void)
{
    SkipByteBlock();
}

void CObjectIStreamXml::ReadString(string& str, EStringType type)
{
    str.erase();
    if (ExpectSpecialCase()!=0 && UseSpecialCaseRead()) {
        EEncoding enc_in(m_Encoding == eEncoding_Unknown ? eEncoding_UTF8 : m_Encoding);
        CStringUTF8 u( CUtf8::AsUTF8(x_UseMemberDefault<string>(),enc_in));
        if (type == eStringTypeUTF8 || m_StringEncoding == eEncoding_Unknown) {
            str = u;
        } else {
            str = CUtf8::AsSingleByteString(u,m_StringEncoding);
        }
        return;
    }
    if (EndOpeningTagSelfClosed()) {
        return;
    }
    ReadTagData(str, type);
}

char* CObjectIStreamXml::ReadCString(void)
{
    if ( EndOpeningTagSelfClosed() ) {
        // null pointer string
        return 0;
    }
    string str;
    ReadTagData(str);
    return strdup(str.c_str());
}

bool CObjectIStreamXml::ReadCDSection(string& str)
// http://www.w3.org/TR/2000/REC-xml-20001006#dt-cdsection
// must begin with <![CDATA[
// must end with   ]]>
{
    if (m_Input.PeekChar() != '<' || m_Input.PeekChar(1) != '!') {
        return false;
    }
    m_Input.SkipChars(2);
    const char* open = "[CDATA[";
    for ( ; *open; ++open) {
        if (m_Input.PeekChar() != *open) {
            ThrowError(fFormatError, "CDATA section expected");
        }
        m_Input.SkipChar();
    }
    while ( m_Input.PeekChar(0) != ']' ||
            m_Input.PeekChar(1) != ']' ||
            m_Input.PeekChar(2) != '>') {
        str += m_Input.PeekChar();
        m_Input.SkipChar();
    }
    m_Input.SkipChars(3);
    return true;
}

void CObjectIStreamXml::ReadTagData(string& str, EStringType type)
/*
    White Space Handling:
    http://www.w3.org/TR/2000/REC-xml-20001006#sec-white-space

    End-of-Line Handling
    http://www.w3.org/TR/2000/REC-xml-20001006#sec-line-ends
    
    Attribute-Value Normalization
    http://www.w3.org/TR/2000/REC-xml-20001006#AVNormalize
*/
{
    BeginData();
    bool encoded = false;
    bool CR = false;
    try {
        for ( ;; ) {
            int c = ReadEncodedChar(m_Attlist ? '\"' : '<', type, &encoded);
            if ( c < 0 ) {
                if (m_Attlist || !ReadCDSection(str)) {
                    break;
                }
                CR = false;
                continue;
            }
            if (CR) {
                if (c == '\n') {
                    CR = false;
                } else if (c == '\r') {
                    c = '\n';
                }
            } else if (c == '\r') {
                CR = true;
                continue;
            }
            if (m_Attlist && !encoded && IsWhiteSpace(c)) {
                c = ' ';
            }
            str += char(c);
            // pre-allocate memory for long strings
            if ( str.size() > 128  &&  double(str.capacity())/(str.size()+1.0) < 1.1 ) {
                str.reserve(str.size()*2);
            }
        }
    } catch (CEofException&) {
    }
    str.reserve(str.size());
}

TEnumValueType CObjectIStreamXml::ReadEnum(const CEnumeratedTypeValues& values)
{
    const string& enumName = values.GetName();
    if ( !m_SkipNextTag && !enumName.empty() ) {
        // global enum
        OpenTag(enumName);
        _ASSERT(InsideOpeningTag());
    }
    TEnumValueType value;
    if ( InsideOpeningTag() ) {
        // try to read attribute 'value'
        if ( IsEndOfTagChar( SkipWS()) ) {
            // no attribute
            if ( !values.IsInteger() )
                ThrowError(fFormatError, "attribute 'value' expected");
            m_Input.SkipChar();
            Found_gt();
            BeginData();
            value = m_Input.GetInt4();
        }
        else {
            if (m_Attlist) {
                string valueName;
                ReadAttributeValue(valueName);
                NStr::TruncateSpacesInPlace(valueName);
                value = values.FindValue(valueName);
            } else {
                CTempString attr;
                while (HasAttlist()) {
                    attr = ReadAttributeName();
                    if ( attr == "value" ) {    
                        break;
                    }
                    string value_tmp;
                    ReadAttributeValue(value_tmp);
                }
                if ( attr != "value" ) {
                    EndOpeningTagSelfClosed();
                    ThrowError(fMissingValue,"attribute 'value' is missing");
                }
                string valueName;
                ReadAttributeValue(valueName);
                NStr::TruncateSpacesInPlace(valueName);
                value = values.FindValue(valueName);
                if ( !EndOpeningTagSelfClosed() && values.IsInteger() ) {
                    // read integer value
                    SkipWSAndComments();
                    if ( value != m_Input.GetInt4() )
                        ThrowError(fInvalidData,
                                   "incompatible name and value of named integer");
                }
            }
        }
    }
    else {
        // outside of tag
        if ( !values.IsInteger() )
            ThrowError(fFormatError, "attribute 'value' expected");
        BeginData();
        value = m_Input.GetInt4();
    }
    if ( !m_SkipNextTag && !enumName.empty() ) {
        // global enum
        CloseTag(enumName);
    }
    return value;
}

CObjectIStream::EPointerType CObjectIStreamXml::ReadPointerType(void)
{
    if ((ExpectSpecialCase() & CObjectIStream::eReadAsNil)!=0) {
        if (m_IsNil) {
            m_IsNil=false;
            SetSpecialCaseUsed(CObjectIStream::eReadAsNil);
            return eNullPointer;
        }
    }
    if ( !HasAttlist() && InsideOpeningTag() && EndOpeningTagSelfClosed() ) {
        // self closed tag
        return eNullPointer;
    }
    return eThisPointer;
}

CObjectIStreamXml::TObjectIndex CObjectIStreamXml::ReadObjectPointer(void)
{
    ThrowError(fNotImplemented, "Not Implemented");
    return 0;
/*
    CTempString attr = ReadAttributeName();
    if ( attr != "index" )
        ThrowError(fIllegalCall, "attribute 'index' expected");
    string index;
    ReadAttributeValue(index);
    EndOpeningTagSelfClosed();
    return NStr::StringToInt(index);
*/
}

string CObjectIStreamXml::ReadOtherPointer(void)
{
    ThrowError(fNotImplemented, "Not Implemented");
    return NcbiEmptyString;
}

void CObjectIStreamXml::StartDelayBuffer(void)
{
    BeginData();
    CObjectIStream::StartDelayBuffer();
    if (!m_RejectedTag.empty()) {
        m_Input.GetSubSourceCollector()->AddChunk("<", 1);
        m_Input.GetSubSourceCollector()->AddChunk(m_RejectedTag.c_str(), m_RejectedTag.size());
    }
}

CRef<CByteSource> CObjectIStreamXml::EndDelayBuffer(void)
{
    _ASSERT(OutsideTag());
    return CObjectIStream::EndDelayBuffer();
}

CTempString CObjectIStreamXml::SkipTagName(CTempString tag,
                                            const char* str, size_t length)
{
    if ( tag.size() < length ||
         memcmp(tag.data(), str, length) != 0 )
        ThrowError(fFormatError, "invalid tag name: "+string(tag));
    return CTempString(tag.data() + length, tag.size() - length);
}

CTempString CObjectIStreamXml::SkipStackTagName(CTempString tag,
                                                 size_t level)
{
    const TFrame& frame = FetchFrameFromTop(level);
    switch ( frame.GetFrameType() ) {
    case TFrame::eFrameNamed:
    case TFrame::eFrameArray:
    case TFrame::eFrameClass:
    case TFrame::eFrameChoice:
        {
            const string& name = frame.GetTypeInfo()->GetName();
            if ( !name.empty() )
                return SkipTagName(tag, name);
            else
                return SkipStackTagName(tag, level + 1);
        }
    case TFrame::eFrameClassMember:
    case TFrame::eFrameChoiceVariant:
        {
            tag = SkipStackTagName(tag, level + 1, '_');
            return SkipTagName(tag, frame.GetMemberId().GetName());
        }
    case TFrame::eFrameArrayElement:
        {
            if (GetStackDepth() > level+1) {
                tag = SkipStackTagName(tag, level + 1);
                return SkipTagName(tag, "_E");
            }
            return CTempString();
        }
    default:
        break;
    }
    ThrowError(fIllegalCall, "illegal frame type");
    return tag;
}

CTempString CObjectIStreamXml::SkipStackTagName(CTempString tag,
                                                 size_t level, char c)
{
    tag = SkipStackTagName(tag, level);
    if ( tag.empty() || tag[0] != c )
        ThrowError(fFormatError, "invalid tag name: "+string(tag));
    return CTempString(tag.data() + 1, tag.size() - 1);
}

void CObjectIStreamXml::OpenTag(const string& e)
{
    CTempString tagName;
    if (m_RejectedTag.empty()) {
        tagName = ReadName(BeginOpeningTag());
    } else {
        tagName = RejectedName();
    }
    if ( tagName != e )
        ThrowError(fFormatError, "tag '"+e+"' expected: "+string(tagName));
}

void CObjectIStreamXml::CloseTag(const string& e)
{
    if ( SelfClosedTag() ) {
        EndSelfClosedTag();
    }
    else {
        CTempString tagName = ReadName(BeginClosingTag());
        if ( tagName != e )
            ThrowError(fFormatError, "tag '"+e+"' expected: "+string(tagName));
        EndClosingTag();
    }
}

void CObjectIStreamXml::OpenStackTag(size_t level)
{
    CTempString tagName;
    if (m_RejectedTag.empty()) {
        tagName = ReadName(BeginOpeningTag());
        if (!x_IsStdXml()) {
            CTempString rest = SkipStackTagName(tagName, level);
            if ( !rest.empty() )
                ThrowError(fFormatError,
                    "unexpected tag: "+string(tagName)+string(rest));
        }
    } else {
        tagName = RejectedName();
    }
}

void CObjectIStreamXml::CloseStackTag(size_t level)
{
    if ( SelfClosedTag() ) {
        EndSelfClosedTag();
    }
    else {
        if (m_Attlist) {
            m_TagState = eTagInsideClosing;
        } else {
            CTempString tagName = ReadName(BeginClosingTag());
            if (!x_IsStdXml()) {
                CTempString rest = SkipStackTagName(tagName, level);
                if ( !rest.empty() )
                    ThrowError(fFormatError,
                        "unexpected tag: "+string(tagName)+string(rest));
            }
        }
        EndClosingTag();
    }
}

void CObjectIStreamXml::OpenTagIfNamed(TTypeInfo type)
{
    if ( !type->GetName().empty() ) {
        OpenTag(type->GetName());
    }
}

void CObjectIStreamXml::CloseTagIfNamed(TTypeInfo type)
{
    if ( !type->GetName().empty() )
        CloseTag(type->GetName());
}

bool CObjectIStreamXml::WillHaveName(TTypeInfo elementType)
{
    while ( elementType->GetName().empty() ) {
        if ( elementType->GetTypeFamily() != eTypeFamilyPointer )
            return false;
        elementType = CTypeConverter<CPointerTypeInfo>::SafeCast(
            elementType)->GetPointedType();
    }
    // found named type
    return true;
}

bool CObjectIStreamXml::HasAttlist(void)
{
    if (InsideTag()) {
        return !IsEndOfTagChar( SkipWS() );
    }
    return false;
}

bool CObjectIStreamXml::NextIsTag(void)
{
    BeginData();
    return SkipWSAndComments() == '<' &&
        m_Input.PeekChar(1) != '/' &&
        m_Input.PeekChar(1) != '!';
}

bool CObjectIStreamXml::NextTagIsClosing(void)
{
    BeginData();
    return SkipWSAndComments() == '<' && m_Input.PeekChar(1) == '/';
}

bool CObjectIStreamXml::ThisTagIsSelfClosed(void)
{
    if (InsideOpeningTag()) {
        return EndOpeningTagSelfClosed();
    }
    return false;
}


void
CObjectIStreamXml::BeginContainer(const CContainerTypeInfo*  containerType)
{
    if (!m_StdXml) {
        if (TopFrame().GetFrameType() == CObjectStackFrame::eFrameArray &&
            FetchFrameFromTop(1).GetFrameType() == CObjectStackFrame::eFrameNamed) {
            const CClassTypeInfo* clType =
                dynamic_cast<const CClassTypeInfo*>(FetchFrameFromTop(1).GetTypeInfo());
            if (clType && clType->Implicit()) {
                TopFrame().SetNotag();
                return;
            }
        }
        OpenTagIfNamed(containerType);
    }
}

void CObjectIStreamXml::EndContainer(void)
{
    if (!m_StdXml && !TopFrame().GetNotag()) {
        CloseTagIfNamed(TopFrame().GetTypeInfo());
    }
}

bool CObjectIStreamXml::BeginContainerElement(TTypeInfo elementType)
{
    if (!HasMoreElements(elementType)) {
        return false;
    }
    if ( !WillHaveName(elementType) ) {
        BeginArrayElement(elementType);
    }
    return true;
}

void CObjectIStreamXml::EndContainerElement(void)
{
    if ( !WillHaveName(TopFrame().GetTypeInfo()) ) {
        EndArrayElement();
    }
}

TMemberIndex CObjectIStreamXml::HasAnyContent(const CClassTypeInfoBase* classType, TMemberIndex pos)
{
    const CItemsInfo& items = classType->GetItems();
    TMemberIndex i = (pos != kInvalidMember ? pos : items.FirstIndex());
    for (; i <= items.LastIndex(); ++i) {
        const CItemInfo* itemInfo = items.GetItemInfo( i );
        if (itemInfo->GetId().HasAnyContent()) {
            return i;
        }
        if (itemInfo->GetId().HasNotag()) {
            if (itemInfo->GetTypeInfo()->GetTypeFamily() == eTypeFamilyContainer) {
                CObjectTypeInfo elem = CObjectTypeInfo(itemInfo->GetTypeInfo()).GetElementType();
                if (elem.GetTypeFamily() == eTypeFamilyPointer) {
                    elem = elem.GetPointedType();
                }
                if (elem.GetTypeFamily() == eTypeFamilyPrimitive &&
                    elem.GetPrimitiveValueType() == ePrimitiveValueAny) {
                    return i;
                }
            }
        }
    }
/*
    if (items.Size() == 1) {
        const CItemInfo* itemInfo = items.GetItemInfo( items.FirstIndex() );
        if (itemInfo->GetId().HasNotag()) {
            if (itemInfo->GetTypeInfo()->GetTypeFamily() == eTypeFamilyContainer) {
                CObjectTypeInfo elem = CObjectTypeInfo(itemInfo->GetTypeInfo()).GetElementType();
                if (elem.GetTypeFamily() == eTypeFamilyPointer) {
                    elem = elem.GetPointedType();
                }
                if (elem.GetTypeFamily() == eTypeFamilyPrimitive &&
                    elem.GetPrimitiveValueType() == ePrimitiveValueAny) {
                    return items.FirstIndex();
                }
            }
        }
    }
*/
    return kInvalidMember;
}

bool CObjectIStreamXml::HasMoreElements(TTypeInfo elementType)
{
    bool no_more=false;
    try {
        no_more = ThisTagIsSelfClosed() || NextTagIsClosing();
    } catch (CEofException&) {
        no_more = true;
    }
    if (no_more) {
        m_LastPrimitive.erase();
        return false;
    }
    if (x_IsStdXml()) {
        CTempString tagName;
        TTypeInfo type = GetRealTypeInfo(elementType);
        // this is to handle STL containers of primitive types
        if (GetRealTypeFamily(type) == eTypeFamilyPrimitive) {
            if (!m_RejectedTag.empty()) {
                m_LastPrimitive = m_RejectedTag;
                return true;
            } else {
                tagName = ReadName(BeginOpeningTag());
                UndoClassMember();
                bool res = (tagName == m_LastPrimitive ||
                    tagName == type->GetName() ||
                    CObjectTypeInfo(type).GetPrimitiveValueType() == ePrimitiveValueAny);
                if (!res) {
                    m_LastPrimitive.erase();
                }
                return res;
            }
        }
        const CClassTypeInfoBase* classType =
            dynamic_cast<const CClassTypeInfoBase*>(type);
        const CAliasTypeInfo* aliasType = classType ? NULL :
            dynamic_cast<const CAliasTypeInfo*>(type);
        if (classType || aliasType) {
            if (m_RejectedTag.empty()) {
                if (!NextIsTag()) {
                    return true;
                }
                tagName = ReadName(BeginOpeningTag());
            } else {
                tagName = RejectedName();
            }
            UndoClassMember();

            if (classType && classType->GetName().empty()) {
                return classType->GetItems().FindDeep(tagName) != kInvalidMember ||
                    HasAnyContent(classType) != kInvalidMember;
            }
            TTypeInfo nextType = classType ? (TTypeInfo)classType : (TTypeInfo)aliasType;
            return tagName == nextType->GetName();
        }
    }
    return true;
}


TMemberIndex CObjectIStreamXml::FindDeep(TTypeInfo type,
                                         const CTempString& name) const
{
    for (;;) {
        if (type->GetTypeFamily() == eTypeFamilyContainer) {
            const CContainerTypeInfo* cont =
                dynamic_cast<const CContainerTypeInfo*>(type);
            if (cont) {
                type = cont->GetElementType();
            }
        } else if (type->GetTypeFamily() == eTypeFamilyPointer) {
            const CPointerTypeInfo* ptr =
                dynamic_cast<const CPointerTypeInfo*>(type);
            if (ptr) {
                type = ptr->GetPointedType();
            }
        } else {
            break;
        }
    }
    const CClassTypeInfoBase* classType =
        dynamic_cast<const CClassTypeInfoBase*>(type);
    if (classType) {
        TMemberIndex i = classType->GetItems().FindDeep(name);
        if (i != kInvalidMember) {
            return i;
        }
    }
    return kInvalidMember;
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamXml::ReadContainer(const CContainerTypeInfo* containerType,
                                      TObjectPtr containerPtr)
{
    if ( m_StdXml || containerType->GetName().empty() ) {
        ReadContainerContents(containerType, containerPtr);
    }
    else {
        BEGIN_OBJECT_FRAME2(eFrameArray, containerType);
        OpenTag(containerType);

        ReadContainerContents(containerType, containerPtr);

        CloseTag(containerType);
        END_OBJECT_FRAME();
    }
}

void CObjectIStreamXml::SkipContainer(const CContainerTypeInfo* containerType)
{
    if ( m_StdXml || containerType->GetName().empty() ) {
        SkipContainerContents(containerType);
    }
    else {
        BEGIN_OBJECT_FRAME2(eFrameArray, containerType);
        OpenTag(containerType);

        SkipContainerContents(containerType);

        CloseTag(containerType);
        END_OBJECT_FRAME();
    }
}
#endif


void CObjectIStreamXml::BeginArrayElement(TTypeInfo elementType)
{
    if (x_IsStdXml()) {
        CObjectTypeInfo type(GetRealTypeInfo(elementType));
        if (type.GetTypeFamily() != eTypeFamilyPrimitive ||
            type.GetPrimitiveValueType() == ePrimitiveValueAny) {
            TopFrame().SetNotag();
            return;
        }
    }
    OpenStackTag(0);
}

void CObjectIStreamXml::EndArrayElement(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
    } else {
        CloseStackTag(0);
    }
}

void CObjectIStreamXml::ReadContainerContents(const CContainerTypeInfo* cType,
                                              TObjectPtr containerPtr)
{
    int count = 0;
    TTypeInfo elementType = cType->GetElementType();
    if ( !WillHaveName(elementType) ) {
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        CContainerTypeInfo::CIterator iter;
        bool old_element = cType->InitIterator(iter, containerPtr);
        while ( HasMoreElements(elementType) ) {
            BeginArrayElement(elementType);
            do {
                if ( old_element ) {
                    elementType->ReadData(*this, cType->GetElementPtr(iter));
                    old_element = cType->NextElement(iter);
                }
                else {
                    cType->AddElement(containerPtr, *this);
                }
            } while (!m_RejectedTag.empty() &&
                     FindDeep(elementType,m_RejectedTag) != kInvalidMember);
            EndArrayElement();
            ++count;
        }
        if ( old_element ) {
            cType->EraseAllElements(iter);
        }

        END_OBJECT_FRAME();
    }
    else {
        CContainerTypeInfo::CIterator iter;
        bool old_element = cType->InitIterator(iter, containerPtr);
        while ( HasMoreElements(elementType) ) {
            if ( old_element ) {
                elementType->ReadData(*this, cType->GetElementPtr(iter));
                old_element = cType->NextElement(iter);
            }
            else {
                cType->AddElement(containerPtr, *this);
            }
            ++count;
        }
        if ( old_element ) {
            cType->EraseAllElements(iter);
        }
    }
    if (count == 0) {
        const TFrame& frame = FetchFrameFromTop(0);
        if (frame.GetFrameType() == CObjectStackFrame::eFrameNamed) {
            const CClassTypeInfo* clType =
                dynamic_cast<const CClassTypeInfo*>(frame.GetTypeInfo());
            if (clType && clType->Implicit() && clType->IsImplicitNonEmpty()) {
                ThrowError(fFormatError, "container is empty");
            }
        }
    }
}

void CObjectIStreamXml::SkipContainerContents(const CContainerTypeInfo* cType)
{
    TTypeInfo elementType = cType->GetElementType();
    if ( !WillHaveName(elementType) ) {
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        while ( HasMoreElements(elementType) ) {
            BeginArrayElement(elementType);
            SkipObject(elementType);
            EndArrayElement();
        }
        
        END_OBJECT_FRAME();
    }
    else {
        while ( HasMoreElements(elementType) ) {
            SkipObject(elementType);
        }
    }
}

void CObjectIStreamXml::BeginNamedType(TTypeInfo namedTypeInfo)
{
    bool isclass = false;
    if (m_SkipNextTag) {
        TopFrame().SetNotag();
        m_SkipNextTag = false;
    } else {
        const CClassTypeInfo* classType =
            dynamic_cast<const CClassTypeInfo*>(namedTypeInfo);
        if (classType) {
            CheckStdXml(classType);
            isclass = true;
        }
        OpenTag(namedTypeInfo);
    }
    if (!isclass) {
        const CAliasTypeInfo* aliasType = 
            dynamic_cast<const CAliasTypeInfo*>(namedTypeInfo);
        if (aliasType) {
            m_SkipNextTag = aliasType->IsFullAlias();
        }
    }
}

void CObjectIStreamXml::EndNamedType(void)
{
    m_SkipNextTag = false;
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
        return;
    }
    CloseTag(TopFrame().GetTypeInfo()->GetName());
}

#ifdef VIRTUAL_MID_LEVEL_IO

void CObjectIStreamXml::ReadNamedType(TTypeInfo namedTypeInfo,
                                      TTypeInfo typeInfo,
                                      TObjectPtr object)
{
    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);

    BeginNamedType(namedTypeInfo);
    ReadObject(object, typeInfo);
    EndNamedType();

    END_OBJECT_FRAME();
}
#endif

void CObjectIStreamXml::CheckStdXml(const CClassTypeInfoBase* classType)
{
    TMemberIndex first = classType->GetItems().FirstIndex();
    m_StdXml = classType->GetItems().GetItemInfo(first)->GetId().HaveNoPrefix();
}

TTypeInfo CObjectIStreamXml::GetRealTypeInfo(TTypeInfo typeInfo)
{
    if (typeInfo->GetTypeFamily() == eTypeFamilyPointer) {
        const CPointerTypeInfo* ptr =
            dynamic_cast<const CPointerTypeInfo*>(typeInfo);
        if (ptr) {
            typeInfo = ptr->GetPointedType();
        }
    }
    return typeInfo;
}

ETypeFamily CObjectIStreamXml::GetRealTypeFamily(TTypeInfo typeInfo)
{
    return GetRealTypeInfo( typeInfo )->GetTypeFamily();
}

TTypeInfo CObjectIStreamXml::GetContainerElementTypeInfo(TTypeInfo typeInfo)
{
    typeInfo = GetRealTypeInfo( typeInfo );
    _ASSERT(typeInfo->GetTypeFamily() == eTypeFamilyContainer);
    const CContainerTypeInfo* ptr =
        dynamic_cast<const CContainerTypeInfo*>(typeInfo);
    return GetRealTypeInfo(ptr->GetElementType());
}

ETypeFamily CObjectIStreamXml::GetContainerElementTypeFamily(TTypeInfo typeInfo)
{
    typeInfo = GetRealTypeInfo( typeInfo );
    _ASSERT(typeInfo->GetTypeFamily() == eTypeFamilyContainer);
    const CContainerTypeInfo* ptr =
        dynamic_cast<const CContainerTypeInfo*>(typeInfo);
    return GetRealTypeFamily(ptr->GetElementType());
}


void CObjectIStreamXml::BeginClass(const CClassTypeInfo* classInfo)
{
    if (m_SkipNextTag) {
        TopFrame().SetNotag();
        m_SkipNextTag = false;
        return;
    }
    CheckStdXml(classInfo);
    if (x_IsStdXml()) {
        if (!m_Attlist) {
// if class spec defines no attributes, but there are some - skip them
            if (HasAttlist() && !classInfo->GetMemberInfo(
                classInfo->GetMembers().FirstIndex())->GetId().IsAttlist()) {
                ReadUndefinedAttributes();
            }
        }
        if (m_Attlist || HasAttlist()) {
            TopFrame().SetNotag();
        } else {
            OpenTagIfNamed(classInfo);
        }
    } else {
        OpenTagIfNamed(classInfo);
    }
}

void CObjectIStreamXml::EndClass(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
    } else {
        CloseTagIfNamed(TopFrame().GetTypeInfo());
    }
    x_EndTypeNamespace();
}

void CObjectIStreamXml::UnexpectedMember(const CTempString& id,
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
CObjectIStreamXml::BeginClassMember(const CClassTypeInfo* classType)
{
    CTempString tagName;
    bool more;
    do {
        more = false;
        if (m_RejectedTag.empty()) {
            if (m_Attlist && InsideTag()) {
                if (HasAttlist()) {
                    tagName = ReadName(SkipWS());
                } else {
                    return kInvalidMember;
                }
            } else {
                if (!m_Attlist && InsideOpeningTag()) {
                    TMemberIndex first = classType->GetMembers().FirstIndex();
                    if (classType->GetMemberInfo(first)->GetId().IsAttlist()) {
                        m_Attlist = true;
                        return first;
                    }
                }
                m_Attlist = false;
                if ( NextTagIsClosing() )
                    return kInvalidMember;
                tagName = ReadName(BeginOpeningTag());
            }
        } else {
            tagName = RejectedName();
        }
        TMemberIndex ind = classType->GetMembers().Find(tagName);
        if ( ind != kInvalidMember ) {
            if (x_IsStdXml()) {
                const CMemberInfo *mem_info = classType->GetMemberInfo(ind);
                ETypeFamily type = GetRealTypeFamily(mem_info->GetTypeInfo());
                bool needUndo = false;
                if (!GetEnforcedStdXml()) {
                    needUndo = (type != eTypeFamilyPrimitive);
                }
                if (needUndo) {
                    TopFrame().SetNotag();
                    UndoClassMember();
                }
                return ind;
            }
        }
// if it is an attribute list, but the tag is unrecognized - just skip it
        if (m_Attlist) {
            if (ind == kInvalidMember && tagName.empty()) {
                return ind;
            }
            string value;
            ReadAttributeValue(value);
            m_Input.SkipChar();
            more = true;
        }
    } while (more);

    CTempString id = SkipStackTagName(tagName, 1, '_');
    TMemberIndex index = classType->GetMembers().Find(id);
    if ( index == kInvalidMember ) {
        if (CanSkipUnknownMembers()) {
            SetFailFlags(fUnknownValue);
            string tag(tagName);
            if (SkipAnyContent()) {
                CloseTag(tag);
            }
            return BeginClassMember(classType);
        } else {
            UnexpectedMember(id, classType->GetMembers());
        }
    }
    return index;
}

TMemberIndex
CObjectIStreamXml::BeginClassMember(const CClassTypeInfo* classType,
                                    TMemberIndex pos)
{
    CTempString tagName;
    TMemberIndex first = classType->GetMembers().FirstIndex();
    if (m_RejectedTag.empty()) {
        if (m_Attlist && InsideTag()) {
            if (HasAttlist()) {
                for (;;) {
                    char ch = SkipWS();
                    if (IsEndOfTagChar(ch)) {
                        return kInvalidMember;
                    }
                    tagName = ReadName(ch);
                    if (!tagName.empty()) {
                        if (classType->GetMembers().Find(tagName) != kInvalidMember) {
                            break;
                        }
                        string value;
                        ReadAttributeValue(value, true);
                    }
                }
            } else {
                return kInvalidMember;
            }
        } else {
            if (!m_Attlist) {
                if (pos == first) {
                    if (classType->GetMemberInfo(first)->GetId().IsAttlist()) {
                        m_Attlist = true;
                        if (m_TagState == eTagOutside) {
                            m_Input.UngetChar('>');
                            m_TagState = eTagInsideOpening;
                        }
                        return first;
                    }
// if class spec defines no attributes, but there are some - skip them
                    if (HasAttlist()) {
                        ReadUndefinedAttributes();
                    }
                }
            }
            if (m_Attlist && !SelfClosedTag()) {
                m_Attlist = false;
                TMemberIndex ind = first+1;
                if (classType->GetMemberInfo(ind)->GetId().HasNotag()) {
                    TopFrame().SetNotag();
                    return ind;
                }
                if ( NextTagIsClosing() )
                    return kInvalidMember;
/*
                if (!NextIsTag()) {
                    TMemberIndex ind = first+1;
                    if (classType->GetMemberInfo(ind)->GetId().HasNotag()) {
                        TopFrame().SetNotag();
                        return ind;
                    }
                }
*/
            }
            if ( SelfClosedTag() || ThisTagIsSelfClosed()) {
                m_Attlist = false;
                TMemberIndex last = classType->GetMembers().LastIndex();
                if (pos == last) {
                    if (classType->GetMemberInfo(pos)->GetId().HasNotag() &&
                        !classType->GetMemberInfo(pos)->GetId().HasAnyContent()) {
                        TopFrame().SetNotag();
                        return pos;
                    }
                }
                return kInvalidMember;
            }
            if ( NextTagIsClosing() ) 
                return kInvalidMember; 
            if (pos <= classType->GetItems().LastIndex()) {
                const CMemberInfo* mem_info = classType->GetMemberInfo(pos);
                if (mem_info->GetId().HasNotag() &&
                    !mem_info->GetId().HasAnyContent()) {
                    if (GetRealTypeFamily(mem_info->GetTypeInfo()) == eTypeFamilyPrimitive) {
                        TopFrame().SetNotag();
                        return pos;
                    }
                }
            } else {
                if (CanSkipUnknownMembers() && NextIsTag()) {
                    SetFailFlags(fUnknownValue);
                    SkipAnyContent();
                }
                return kInvalidMember;
            }
            if (!NextIsTag()) {
                return kInvalidMember;
            }
            tagName = ReadName(BeginOpeningTag());
        }
    } else {
        tagName = RejectedName();
    }

    TMemberIndex ind = classType->GetMembers().Find(tagName);
    if (ind == kInvalidMember) {
        ind = classType->GetMembers().FindDeep(tagName);
        if (ind != kInvalidMember) {
            TopFrame().SetNotag();
            UndoClassMember();
            return ind;
        }
    } else {
        const CMemberInfo *mem_info = classType->GetMemberInfo(ind);
        if (x_IsStdXml()) {
            ETypeFamily type = GetRealTypeFamily(mem_info->GetTypeInfo());
            bool needUndo = false;
            if (GetEnforcedStdXml()) {
                if (type == eTypeFamilyContainer) {
                    TTypeInfo mem_type  = GetRealTypeInfo(mem_info->GetTypeInfo());
                    TTypeInfo elem_type = GetContainerElementTypeInfo(mem_type);
                    needUndo = (elem_type->GetTypeFamily() == eTypeFamilyPrimitive &&
                        elem_type->GetName() == mem_type->GetName());
                }
            } else {
                needUndo = (type != eTypeFamilyPrimitive) ||
                    mem_info->GetId().HasAnyContent();
            }
            if (needUndo) {
                TopFrame().SetNotag();
                UndoClassMember();
            }
            return ind;
        }
    }
    if (x_IsStdXml()) {
        UndoClassMember();
        ind = HasAnyContent(classType,pos);
        if (ind != kInvalidMember) {
            TopFrame().SetNotag();
            return ind;
        }
        if (CanSkipUnknownMembers() &&
            pos <= classType->GetMembers().LastIndex()) {
            SetFailFlags(fUnknownValue);
            string tag(RejectedName());
            if (SkipAnyContent()) {
                CloseTag(tag);
            }
            return BeginClassMember(classType, pos);
        }
        return kInvalidMember;
    }
    CTempString id = SkipStackTagName(tagName, 1, '_');
    TMemberIndex index = classType->GetMembers().Find(id, pos);
    if ( index == kInvalidMember ) {
        if (CanSkipUnknownMembers()) {
            SetFailFlags(fUnknownValue);
            string tag(tagName);
            if (SkipAnyContent()) {
                CloseTag(tag);
            }
            return BeginClassMember(classType, pos);
        } else {
           UnexpectedMember(id, classType->GetMembers());
        }
    }
    return index;
}

void CObjectIStreamXml::EndClassMember(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
    } else {
        CloseStackTag(0);
    }
}

void CObjectIStreamXml::UndoClassMember(void)
{
    if (InsideOpeningTag()) {
        m_RejectedTag = m_LastTag;
        m_TagState = eTagOutside;
#if defined(NCBI_SERIAL_IO_TRACE)
    cout << ", Undo= " << m_LastTag;
#endif
    }
}

void CObjectIStreamXml::BeginChoice(const CChoiceTypeInfo* choiceType)
{
    if (m_SkipNextTag) {
        TopFrame().SetNotag();
        m_SkipNextTag = false;
        return;
    }
    CheckStdXml(choiceType);
    OpenTagIfNamed(choiceType);
}
void CObjectIStreamXml::EndChoice(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
        return;
    }
    CloseTagIfNamed(TopFrame().GetTypeInfo());
    x_EndTypeNamespace();
}

TMemberIndex CObjectIStreamXml::BeginChoiceVariant(const CChoiceTypeInfo* choiceType)
{
    CTempString tagName;
    TMemberIndex first = choiceType->GetVariants().FirstIndex();
    if (m_RejectedTag.empty()) {
        if (!m_Attlist) {
            if (choiceType->GetVariantInfo(first)->GetId().IsAttlist()) {
                m_Attlist = true;
                if (m_TagState == eTagOutside) {
                    m_Input.UngetChar('>');
                    m_TagState = eTagInsideOpening;
                }
                TopFrame().SetNotag();
                return first;
            }
// if spec defines no attributes, but there are some - skip them
            if (HasAttlist()) {
                ReadUndefinedAttributes();
            }
        }
        m_Attlist = false;
        if ( NextTagIsClosing() ) {
            TMemberIndex ind = choiceType->GetVariants().FindEmpty();
            if (ind != kInvalidMember) {
                TopFrame().SetNotag();
            }
            return ind;
        }
        if (!NextIsTag()) {
            const CItemsInfo& items = choiceType->GetItems();
            for (TMemberIndex i = items.FirstIndex(); i <= items.LastIndex(); ++i) {
                if (items.GetItemInfo(i)->GetId().HasNotag()) {
                    if (GetRealTypeFamily(items.GetItemInfo(i)->GetTypeInfo()) == eTypeFamilyPrimitive) {
                        TopFrame().SetNotag();
                        return i;
                    }
                }
            }
            
        }
        tagName = ReadName(BeginOpeningTag());
    } else {
        tagName = RejectedName();
    }
    TMemberIndex ind = choiceType->GetVariants().Find(tagName);
    if (ind == kInvalidMember) {
        ind = choiceType->GetVariants().FindDeep(tagName);
        if (ind != kInvalidMember) {
            TopFrame().SetNotag();
            UndoClassMember();
            return ind;
        }
    } else {
        const CVariantInfo *var_info = choiceType->GetVariantInfo(ind);
        if (x_IsStdXml()) {
            ETypeFamily type = GetRealTypeFamily(var_info->GetTypeInfo());
            bool needUndo = false;
            if (GetEnforcedStdXml()) {
                if (type == eTypeFamilyContainer) {
                    TTypeInfo var_type  = GetRealTypeInfo(var_info->GetTypeInfo());
                    TTypeInfo elem_type = GetContainerElementTypeInfo(var_type);
                    needUndo = (elem_type->GetTypeFamily() == eTypeFamilyPrimitive &&
                        elem_type->GetName() == var_type->GetName());
                }
            } else {
                needUndo = (type != eTypeFamilyPrimitive);
            }
            if (needUndo) {
                TopFrame().SetNotag();
                UndoClassMember();
            }
            return ind;
        }
    }
    if (x_IsStdXml()) {
        UndoClassMember();
        UnexpectedMember(tagName, choiceType->GetVariants());
    }
    CTempString id = SkipStackTagName(tagName, 1, '_');
    ind = choiceType->GetVariants().Find(id);
    if ( ind == kInvalidMember ) {
        if (CanSkipUnknownVariants()) {
            SetFailFlags(fUnknownValue);
            UndoClassMember();
        } else {
            UnexpectedMember(tagName, choiceType->GetVariants());
        }
    }
    return ind;
}

void CObjectIStreamXml::EndChoiceVariant(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
    } else {
        CloseStackTag(0);
    }
}

void CObjectIStreamXml::BeginBytes(ByteBlock& )
{
    BeginData();
}

int CObjectIStreamXml::GetHexChar(void)
{
    char c = m_Input.GetChar();
    if ( c >= '0' && c <= '9' ) {
        return c - '0';
    }
    else if ( c >= 'A' && c <= 'Z' ) {
        return c - 'A' + 10;
    }
    else if ( c >= 'a' && c <= 'z' ) {
        return c - 'a' + 10;
    }
    else {
        m_Input.UngetChar(c);
        if ( c != '<' )
            ThrowError(fFormatError, "invalid char in octet string");
    }
    return -1;
}

int CObjectIStreamXml::GetBase64Char(void)
{
    char c = SkipWS();
    if ( IsDigit(c) ||
            ( c >= 'A' && c <= 'Z' ) ||
            ( c >= 'a' && c <= 'z' ) ||
            ( c == '+' || c == '/' || c == '=')) {
        return c;
    }
    else {
        if ( c != '<' )
            ThrowError(fFormatError, "invalid char in base64Binary data");
    }
    return -1;
}

size_t CObjectIStreamXml::ReadBytes(ByteBlock& block,
                                    char* dst, size_t length)
{
    size_t count = 0;
    if (TopFrame().HasMemberId() && TopFrame().GetMemberId().IsCompressed()) {
        bool end_of_data = false;
        const size_t chunk_in = 80;
        char src_buf[chunk_in];
        size_t bytes_left = length;
        size_t src_size, src_read, dst_written;
        while (!end_of_data && bytes_left > chunk_in && bytes_left <= length) {
            for ( src_size = 0; src_size < chunk_in; ) {
                int c = GetBase64Char();
                if (c < 0) {
                    end_of_data = true;
                    break;
                }
                /*if (c != '=')*/ {
                    src_buf[ src_size++ ] = c;
                }
                m_Input.SkipChar();
            }
            BASE64_Decode( src_buf, src_size, &src_read,
                        dst, bytes_left, &dst_written);
            if (src_size != src_read) {
                ThrowError(fFail, "error decoding base64Binary data");
            }
            count += dst_written;
            bytes_left -= dst_written;
            dst += dst_written;
        }
        if (end_of_data) {
            block.EndOfBlock();
        }
        return count;;
    }
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

void CObjectIStreamXml::BeginChars(CharBlock& )
{
    BeginData();
}

size_t CObjectIStreamXml::ReadChars(CharBlock& block,
                                    char* dst, size_t length)
{
    size_t count = 0;
    while ( length-- > 0 ) {
        char c = m_Input.GetChar();
        if (c == '<') {
            block.EndOfBlock();
            break;
        }
        *dst++ = c;
        count++;
    }
    return count;
}

void CObjectIStreamXml::SkipBool(void)
{
    ReadBool();
}

void CObjectIStreamXml::SkipChar(void)
{
    ReadChar();
}

void CObjectIStreamXml::SkipSNumber(void)
{
    if (ExpectSpecialCase()!=0 && UseSpecialCaseRead()) {
        return;
    }
    BeginData();
    size_t i;
    char c = SkipWSAndComments();
    switch ( c ) {
    case '+':
    case '-':
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
        ThrowError(fFormatError, "invalid symbol in number");
    }
    while ( (c = m_Input.PeekCharNoEOF(i)) >= '0' && c <= '9' ) {
        ++i;
    }
    m_Input.SkipChars(i);
}

void CObjectIStreamXml::SkipUNumber(void)
{
    if (ExpectSpecialCase()!=0 && UseSpecialCaseRead()) {
        return;
    }
    BeginData();
    size_t i;
    char c = SkipWSAndComments();
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
        ThrowError(fFormatError, "invalid symbol in number");
    }
    while ( (c = m_Input.PeekCharNoEOF(i)) >= '0' && c <= '9' ) {
        ++i;
    }
    m_Input.SkipChars(i);
}

void CObjectIStreamXml::SkipFNumber(void)
{
    ReadDouble();
}

void CObjectIStreamXml::SkipString(EStringType type)
{
    if (ExpectSpecialCase()!=0 && UseSpecialCaseRead()) {
        return;
    }
    BeginData();
    EEncoding enc = m_Encoding;
    if (type == eStringTypeUTF8) {
        m_Encoding = eEncoding_ISO8859_1;
    }
    while ( ReadEscapedChar(m_Attlist ? '\"' : '<') >= 0 )
        continue;
    m_Encoding = enc;
}

void CObjectIStreamXml::SkipNull(void)
{
    if ( !EndOpeningTagSelfClosed() )
        ThrowError(fFormatError, "empty tag expected");
}

void CObjectIStreamXml::SkipByteBlock(void)
{
    BeginData();
    for ( ;; ) {
        char c = m_Input.GetChar();
        if ( IsDigit(c) ) {
            continue;
        }
        else if ( c >= 'A' && c <= 'Z' ) {
            continue;
        }
        else if ( c >= 'a' && c <= 'z' ) {
            continue;
        }
        else if ( c == '\r' || c == '\n' ) {
            m_Input.SkipEndOfLine(c);
            continue;
        }
        else if ( c == '+' || c == '/' || c == '=' ) {
            // to allow base64 byte blocks
            continue;
        }
        else if ( c == '<' ) {
            m_Input.UngetChar(c);
            break;
        }
        else {
            m_Input.UngetChar(c);
            ThrowError(fFormatError, "invalid char in octet string");
        }
    }
}

END_NCBI_SCOPE
