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
#include <serial/objistrxml.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objhook.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/continfo.hpp>
#include <serial/memberlist.hpp>
#include <serial/memberid.hpp>

BEGIN_NCBI_SCOPE

CObjectIStream* CObjectIStream::CreateObjectIStreamXml()
{
    return new CObjectIStreamXml();
}

CObjectIStreamXml::CObjectIStreamXml(void)
    : CObjectIStream(eSerial_Xml),
      m_TagState(eTagOutside), m_Attlist(false),
      m_StdXml(false), m_EnforcedStdXml(false),
      m_Encoding( eEncoding_Unknown )
{
}

CObjectIStreamXml::~CObjectIStreamXml(void)
{
}

CObjectIStreamXml::EEncoding CObjectIStreamXml::GetEncoding(void) const
{
    return m_Encoding;
}

string CObjectIStreamXml::GetPosition(void) const
{
    return "line "+NStr::UIntToString(m_Input.GetLine());
}

void CObjectIStreamXml::SetEnforcedStdXml(bool set)
{
    m_EnforcedStdXml = set;
    if (m_EnforcedStdXml) {
        m_StdXml = false;
    }
}

static inline
bool IsBaseChar(char c)
{
    return
        c >= 'A' && c <='Z' ||
        c >= 'a' && c <= 'z' ||
        c >= '\xC0' && c <= '\xD6' ||
        c >= '\xD8' && c <= '\xF6' ||
        c >= '\xF8' && c <= '\xFF';
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
    _ASSERT(InsideTag());
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
            if ( m_Input.PeekChar(1) == '!' &&
                 m_Input.PeekChar(2) == '-' &&
                 m_Input.PeekChar(3) == '-' &&
                 m_Input.PeekChar(4) != '-' ) {
                // start of comment
                m_Input.SkipChars(4);
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
                                       "double dash '--' is not allowed in XML comments");
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
    if (TopFrame().GetNotag()) {
        return SelfClosedTag();
    }
    _ASSERT(InsideOpeningTag());
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

CLightString CObjectIStreamXml::ReadName(char c)
{
    _ASSERT(InsideTag());
    if ( !IsFirstNameChar(c) )
        ThrowError(fFormatError,
            "Name begins with an invalid character: #"
            +NStr::UIntToString(c));

    // find end of tag name
    size_t i = 1, iColon = 0;
    while ( IsNameChar(c = m_Input.PeekChar(i)) ) {
        if (c == ':') {
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
    m_LastTag = CLightString(ptr+iColon, i-iColon);
    if (iColon > 1) {
        string ns_prefix( CLightString(ptr, iColon-1));
        if (ns_prefix == "xmlns") {
            string value;
            ReadAttributeValue(value, true);
            if (m_LastTag == m_CurrNsPrefix) {
                if (FetchFrameFromTop(1).HasTypeInfo()) {
                    TTypeInfo type = FetchFrameFromTop(1).GetTypeInfo();
                    type->SetNamespacePrefix(m_CurrNsPrefix);
                    type->SetNamespaceName(value);
                }
            }
            m_NsPrefixToName[m_LastTag] = value;
            m_NsNameToPrefix[value] = m_LastTag;
            char ch = SkipWS();
            return IsEndOfTagChar(ch) ? CLightString() : ReadName(ch);
        } else if (ns_prefix == "xml") {
            iColon = 0;
        } else {
            if (m_Attlist) {
                if (m_CurrNsPrefix != ns_prefix) {
                    string value;
                    ReadAttributeValue(value, true);
                    char ch = SkipWS();
                    return IsEndOfTagChar(ch) ? CLightString() : ReadName(ch);
                }
            } else {
                m_CurrNsPrefix = ns_prefix;
            }
        }
    } else {
        m_CurrNsPrefix.erase();
        if (m_Attlist && m_LastTag == "xmlns") {
            string value;
            ReadAttributeValue(value, true);
            if (FetchFrameFromTop(1).HasTypeInfo()) {
                TTypeInfo type = FetchFrameFromTop(1).GetTypeInfo();
                type->SetNamespacePrefix(m_CurrNsPrefix);
                type->SetNamespaceName(value);
            }
            m_NsPrefixToName[m_LastTag] = value;
            m_NsNameToPrefix[value] = m_LastTag;
            char ch = SkipWS();
            return IsEndOfTagChar(ch) ? CLightString() : ReadName(ch);
        }
    }
#if defined(NCBI_SERIAL_IO_TRACE)
    cout << ", Read= " << m_LastTag;
#endif
    return CLightString(ptr+iColon, i-iColon);
}

CLightString CObjectIStreamXml::RejectedName(void)
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

    CLightString tagName;
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
                ThrowError(fFormatError, "unknown encoding: " + value);
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
    for ( ;; ) {
        switch ( BeginOpeningTag() ) {
        case '?':
            SkipQDecl();
            break;
        case '!':
            {
                m_Input.SkipChar();
                CLightString tagName = ReadName(m_Input.PeekChar());
                if ( tagName == "DOCTYPE" ) {
                    CLightString docType = ReadName(SkipWS());
                    string typeName = docType;
                    // skip the rest of !DOCTYPE
                    for ( ;; ) {
                        char c = SkipWS();
                        if ( c == '>' ) {
                            m_Input.SkipChar();
                            Found_gt();
                            return typeName;
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
        default:
            {
                string typeName = ReadName(m_Input.PeekChar());
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
            CLightString e(p, offset);
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
    if ((c & 0x80) != 0) {
        if (m_Encoding == eEncoding_UTF8 || m_Encoding == eEncoding_Unknown) {
            Uint1 ch = c;
            Uint1 chRes = (ch & 0x1F);
            m_Input.SkipChar();
            ch = m_Input.PeekChar();
            chRes = (chRes << 6) | (ch & 0x3F);
            c = chRes;
        }
    }
    m_Input.SkipChar();
    return c & 0xFF;
}

CLightString CObjectIStreamXml::ReadAttributeName(void)
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
        int c = ReadEscapedChar(startChar);
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
        CLightString tagName = ReadName(c);
        if (tagName.GetLength()) {
            string value;
            ReadAttributeValue(value, true);
        }
    }
    return c;
}

bool CObjectIStreamXml::ReadBool(void)
{
    CLightString attr;
    while (HasAttlist()) {
        attr = ReadAttributeName();
        if ( attr == "value" ) {    
            break;
        }
        string value;
        ReadAttributeValue(value);
    }
    if ( attr != "value" ) {
        EndOpeningTagSelfClosed();
        ThrowError(fMissingValue,"attribute 'value' is missing");
    }
    string sValue;
    ReadAttributeValue(sValue);
    bool value;
    if ( sValue == "true" )
        value = true;
    else {
        if ( sValue != "false" ) {
            ThrowError(fFormatError,
                       "'true' or 'false' attrubute value expected: "+sValue);
        }
        value = false;
    }
    if ( !EndOpeningTagSelfClosed() && !NextTagIsClosing() )
        ThrowError(fFormatError, "boolean tag must have empty contents");
    return value;
}

char CObjectIStreamXml::ReadChar(void)
{
    BeginData();
    int c = ReadEscapedChar('<');
    if ( c < 0 || m_Input.PeekChar() != '<' )
        ThrowError(fFormatError, "one char tag content expected");
    return c;
}

Int4 CObjectIStreamXml::ReadInt4(void)
{
    BeginData();
    return m_Input.GetInt4();
}

Uint4 CObjectIStreamXml::ReadUint4(void)
{
    BeginData();
    return m_Input.GetUint4();
}

Int8 CObjectIStreamXml::ReadInt8(void)
{
    BeginData();
    return m_Input.GetInt8();
}

Uint8 CObjectIStreamXml::ReadUint8(void)
{
    BeginData();
    return m_Input.GetUint8();
}

double CObjectIStreamXml::ReadDouble(void)
{
    string s;
    ReadTagData(s);
    char* endptr;
    double data = strtod(s.c_str(), &endptr);
    if ( *endptr != 0 )
        ThrowError(fFormatError, "invalid float number");
    return data;
}

void CObjectIStreamXml::ReadNull(void)
{
    if ( !EndOpeningTagSelfClosed() && !NextTagIsClosing() )
        ThrowError(fFormatError, "empty tag expected");
}

void CObjectIStreamXml::ReadAnyContentTo(
    const string& ns_prefix, string& value, const CLightString& tagName)
{
    if (ThisTagIsSelfClosed()) {
        EndSelfClosedTag();
        return;
    }
    while (!NextTagIsClosing()) {
        while (NextIsTag()) {
            CLightString tagAny;
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
            ReadAnyContentTo(ns_prefix, value2,tagAny);
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
        ReadTagData(data);
        value += data;
    }
    CloseTag(tagName);
}

void CObjectIStreamXml::ReadAnyContentObject(CAnyContentObject& obj)
{
    BEGIN_OBJECT_FRAME(eFrameOther);
    CLightString tagName;
    if (m_RejectedTag.empty()) {
        tagName = ReadName(BeginOpeningTag());
    } else {
        tagName = RejectedName();
    }
    obj.SetName( tagName);
    string ns_prefix(m_CurrNsPrefix);
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
            obj.AddAttribute( attribName, m_NsPrefixToName[m_CurrNsPrefix],value);
        }
    }
    obj.SetNamespacePrefix(ns_prefix);
    obj.SetNamespaceName(m_NsPrefixToName[ns_prefix]);
    string value;
    ReadAnyContentTo(ns_prefix,value,obj.GetName());
    obj.SetValue(value);
    END_OBJECT_FRAME();
}

void CObjectIStreamXml::SkipAnyContentObject(void)
{
    CAnyContentObject obj;
    ReadAnyContentObject(obj);
}

void CObjectIStreamXml::ReadString(string& str, EStringType type)
{
    str.erase();
    if ( !EndOpeningTagSelfClosed() ) {
        EEncoding enc = m_Encoding;
        if (type == eStringTypeUTF8) {
            if (m_Encoding == eEncoding_UTF8 || m_Encoding == eEncoding_Unknown) {
                m_Encoding = eEncoding_ISO8859_1;
            } else {
                string tmp;
                ReadTagData(tmp);
                (static_cast<CStringUTF8&>(str)) = tmp;
                return;
            }
        }
        ReadTagData(str);
        m_Encoding = enc;
    }
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

void CObjectIStreamXml::ReadTagData(string& str)
{
    BeginData();
    bool skip_spaces = false;
    bool encoded = false;
    for ( ;; ) {
        int c = ReadEscapedChar(m_Attlist ? '\"' : '<', &encoded);
        if ( c < 0 ) {
            break;
        }
        if (!encoded) {
            if (c == '\n' || c == '\r') {
                skip_spaces = true;
                continue;
            }
            if (skip_spaces) {
                if (IsWhiteSpace(c)) {
                    continue;
                } else {
                    skip_spaces = false;
                    str += ' ';
                }
            }
        }
        str += char(c);
        // pre-allocate memory for long strings
        if ( str.size() > 128  &&  double(str.capacity())/(str.size()+1.0) < 1.1 ) {
            str.reserve(str.size()*2);
        }
    }
    str.reserve(str.size());
}

TEnumValueType CObjectIStreamXml::ReadEnum(const CEnumeratedTypeValues& values)
{
    const string& enumName = values.GetName();
    if ( !enumName.empty() ) {
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
                value = values.FindValue(valueName);
            } else {
                CLightString attr;
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
                value = values.FindValue(valueName);
                if ( !EndOpeningTagSelfClosed() && values.IsInteger() ) {
                    // read integer value
                    SkipWSAndComments();
                    if ( value != m_Input.GetInt4() )
                        ThrowError(fFormatError,
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
    if ( !enumName.empty() ) {
        // global enum
        CloseTag(enumName);
    }
    return value;
}

CObjectIStream::EPointerType CObjectIStreamXml::ReadPointerType(void)
{
    if ( !HasAttlist() && InsideOpeningTag() && EndOpeningTagSelfClosed() ) {
        // self closed tag
        return eNullPointer;
    }
    return eThisPointer;
}

CObjectIStreamXml::TObjectIndex CObjectIStreamXml::ReadObjectPointer(void)
{
    ThrowError(fIllegalCall, "unimplemented");
    return 0;
/*
    CLightString attr = ReadAttributeName();
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
    ThrowError(fIllegalCall, "unimplemented");
    return NcbiEmptyString;
}

void CObjectIStreamXml::StartDelayBuffer(void)
{
    BeginData();
    CObjectIStream::StartDelayBuffer();
}

CRef<CByteSource> CObjectIStreamXml::EndDelayBuffer(void)
{
    _ASSERT(OutsideTag());
    return CObjectIStream::EndDelayBuffer();
}

CLightString CObjectIStreamXml::SkipTagName(CLightString tag,
                                            const char* str, size_t length)
{
    if ( tag.GetLength() < length ||
         memcmp(tag.GetString(), str, length) != 0 )
        ThrowError(fFormatError, "invalid tag name: "+string(tag));
    return CLightString(tag.GetString() + length, tag.GetLength() - length);
}

CLightString CObjectIStreamXml::SkipStackTagName(CLightString tag,
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
            tag = SkipStackTagName(tag, level + 1);
            return SkipTagName(tag, "_E");
        }
    default:
        break;
    }
    ThrowError(fIllegalCall, "illegal frame type");
    return tag;
}

CLightString CObjectIStreamXml::SkipStackTagName(CLightString tag,
                                                 size_t level, char c)
{
    tag = SkipStackTagName(tag, level);
    if ( tag.Empty() || *tag.GetString() != c )
        ThrowError(fFormatError, "invalid tag name: "+string(tag));
    return CLightString(tag.GetString() + 1, tag.GetLength() - 1);
}

void CObjectIStreamXml::OpenTag(const string& e)
{
    CLightString tagName;
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
        CLightString tagName = ReadName(BeginClosingTag());
        if ( tagName != e )
            ThrowError(fFormatError, "tag '"+e+"' expected: "+string(tagName));
        EndClosingTag();
    }
}

void CObjectIStreamXml::OpenStackTag(size_t level)
{
    CLightString tagName;
    if (m_RejectedTag.empty()) {
        tagName = ReadName(BeginOpeningTag());
        if (!x_IsStdXml()) {
            CLightString rest = SkipStackTagName(tagName, level);
            if ( !rest.Empty() )
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
            CLightString tagName = ReadName(BeginClosingTag());
            if (!x_IsStdXml()) {
                CLightString rest = SkipStackTagName(tagName, level);
                if ( !rest.Empty() )
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
    return SkipWSAndComments() == '<' && m_Input.PeekChar(1) != '/';
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
CObjectIStreamXml::
BeginContainer(const CContainerTypeInfo* /* containerType */)
{
/*
    if (!x_IsStdXml()) {
        OpenTagIfNamed(containerType);
    }
*/
}

void CObjectIStreamXml::EndContainer(void)
{
/*
    if (!x_IsStdXml()) {
        CloseTagIfNamed(TopFrame().GetTypeInfo());
    }
*/
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

bool CObjectIStreamXml::HasAnyContent(const CClassTypeInfoBase* classType)
{
    const CItemsInfo& items = classType->GetItems();
    if (items.Size() == 1) {
        const CItemInfo* itemInfo = items.GetItemInfo( items.FirstIndex());
        if (itemInfo->GetId().HasAnyContent()) {
            return true;
        }
    }
    return false;
}

bool CObjectIStreamXml::HasMoreElements(TTypeInfo elementType)
{
    if (ThisTagIsSelfClosed() || NextTagIsClosing()) {
        m_LastPrimitive.erase();
        return false;
    }
    if (x_IsStdXml()) {
        CLightString tagName;
        TTypeInfo type = elementType;
        // this is to handle STL containers of primitive types
        if (GetRealTypeFamily(type) == eTypeFamilyPrimitive) {
            if (!m_RejectedTag.empty()) {
                m_LastPrimitive = m_RejectedTag;
                return true;
            } else {
                tagName = ReadName(BeginOpeningTag());
                UndoClassMember();
                bool res = (tagName == m_LastPrimitive);
                if (!res) {
                    m_LastPrimitive.erase();
                }
                return res;
            }
        }
        if (type->GetTypeFamily() == eTypeFamilyPointer) {
            const CPointerTypeInfo* ptr =
                dynamic_cast<const CPointerTypeInfo*>(type);
            if (ptr) {
                type = ptr->GetPointedType();
            }
        }
        const CClassTypeInfoBase* classType =
            dynamic_cast<const CClassTypeInfoBase*>(type);
        if (classType) {
            if (m_RejectedTag.empty()) {
                tagName = ReadName(BeginOpeningTag());
            } else {
                tagName = RejectedName();
            }
            UndoClassMember();

            if (classType->GetName().empty()) {
                return classType->GetItems().FindDeep(tagName) != kInvalidMember ||
                    HasAnyContent(classType);
            }
            return tagName == classType->GetName();
        }
    }
    return true;
}


TMemberIndex CObjectIStreamXml::FindDeep(TTypeInfo type,
                                         const CLightString& name) const
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
    if ( containerType->GetName().empty() ) {
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
    if ( containerType->GetName().empty() ) {
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
    if (x_IsStdXml() && GetRealTypeFamily(elementType) != eTypeFamilyPrimitive) {
        TopFrame().SetNotag();
    } else {
        OpenStackTag(0);
    }
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
    const CClassTypeInfo* classType =
        dynamic_cast<const CClassTypeInfo*>(namedTypeInfo);
    if (classType) {
        CheckStdXml(classType);
    }
    OpenTag(namedTypeInfo);
}

void CObjectIStreamXml::EndNamedType(void)
{
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
    if (m_EnforcedStdXml) {
        m_StdXml = false;
        return;
    }
    TMemberIndex first = classType->GetItems().FirstIndex();
    m_StdXml = classType->GetItems().GetItemInfo(first)->GetId().HaveNoPrefix();
}

ETypeFamily CObjectIStreamXml::GetRealTypeFamily(TTypeInfo typeInfo)
{
    ETypeFamily type = typeInfo->GetTypeFamily();
    if (type == eTypeFamilyPointer) {
        const CPointerTypeInfo* ptr =
            dynamic_cast<const CPointerTypeInfo*>(typeInfo);
        if (ptr) {
            type = ptr->GetPointedType()->GetTypeFamily();
        }
    }
    return type;
}

ETypeFamily CObjectIStreamXml::GetContainerElementTypeFamily(TTypeInfo typeInfo)
{
    if (typeInfo->GetTypeFamily() == eTypeFamilyPointer) {
        const CPointerTypeInfo* ptr =
            dynamic_cast<const CPointerTypeInfo*>(typeInfo);
        if (ptr) {
            typeInfo = ptr->GetPointedType();
        }
    }
    _ASSERT(typeInfo->GetTypeFamily() == eTypeFamilyContainer);
    const CContainerTypeInfo* ptr =
        dynamic_cast<const CContainerTypeInfo*>(typeInfo);
    return GetRealTypeFamily(ptr->GetElementType());
}


void CObjectIStreamXml::BeginClass(const CClassTypeInfo* classInfo)
{
    CheckStdXml(classInfo);
    if (x_IsStdXml()) {
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

void CObjectIStreamXml::UnexpectedMember(const CLightString& id,
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
    CLightString tagName;
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
                return ind;
            }
        }
// if it is an attribute list, but the tag is unrecognized - just skip it
        if (m_Attlist) {
            string value;
            ReadAttributeValue(value);
            m_Input.SkipChar();
            more = true;
        }
    } while (more);

    CLightString id = SkipStackTagName(tagName, 1, '_');
    TMemberIndex index = classType->GetMembers().Find(id);
    if ( index == kInvalidMember ) {
        if (GetSkipUnknownMembers() == eSerialSkipUnknown_Yes) {
            string value;
            ReadAnyContentTo(m_CurrNsPrefix,value, tagName);
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
    CLightString tagName;
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
                    if (tagName.GetLength()) {
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
                if ( NextTagIsClosing() )
                    return kInvalidMember;
                if (!NextIsTag()) {
                    TMemberIndex ind = first+1;
                    if (classType->GetMemberInfo(ind)->GetId().HasNotag()) {
                        TopFrame().SetNotag();
                        return ind;
                    }
                }
            }
            if ( SelfClosedTag()) {
                TMemberIndex last = classType->GetMembers().LastIndex();
                if (pos == last) {
                    if (classType->GetMemberInfo(pos)->GetId().HasNotag()) {
                        TopFrame().SetNotag();
                        return pos;
                    }
                }
                m_Attlist = false;
                return kInvalidMember;
            }
            if ( ThisTagIsSelfClosed() || NextTagIsClosing() )
                return kInvalidMember;
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
                    needUndo = (GetContainerElementTypeFamily(
                        mem_info->GetTypeInfo()) == eTypeFamilyPrimitive);
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
        if (pos==first && HasAnyContent(classType)) {
            TopFrame().SetNotag();
            return first;
        }
        if (GetSkipUnknownMembers() == eSerialSkipUnknown_Yes &&
            pos <= classType->GetMembers().LastIndex()) {
            string value;
            ReadAnyContentTo(m_CurrNsPrefix,value, RejectedName());
            return BeginClassMember(classType, pos);
        }
        return kInvalidMember;
    }
    CLightString id = SkipStackTagName(tagName, 1, '_');
    TMemberIndex index = classType->GetMembers().Find(id, pos);
    if ( index == kInvalidMember ) {
        if (GetSkipUnknownMembers() == eSerialSkipUnknown_Yes) {
            string value;
            ReadAnyContentTo(m_CurrNsPrefix,value, tagName);
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
    CheckStdXml(choiceType);
    OpenTagIfNamed(choiceType);
}
void CObjectIStreamXml::EndChoice(void)
{
    CloseTagIfNamed(TopFrame().GetTypeInfo());
    x_EndTypeNamespace();
}

TMemberIndex CObjectIStreamXml::BeginChoiceVariant(const CChoiceTypeInfo* choiceType)
{
    CLightString tagName;
    TMemberIndex first = choiceType->GetVariants().FirstIndex();
    if (m_RejectedTag.empty()) {
        if (!m_Attlist) {
            if (choiceType->GetVariantInfo(first)->GetId().IsAttlist()) {
                m_Attlist = true;
                if (m_TagState == eTagOutside) {
                    m_Input.UngetChar('>');
                    m_TagState = eTagInsideOpening;
                }
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
                    needUndo = (GetContainerElementTypeFamily(
                        var_info->GetTypeInfo()) == eTypeFamilyPrimitive);
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
    CLightString id = SkipStackTagName(tagName, 1, '_');
    ind = choiceType->GetVariants().Find(id);
    if ( ind == kInvalidMember ) {
        UnexpectedMember(tagName, choiceType->GetVariants());
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

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamXml::ReadChoice(const CChoiceTypeInfo* choiceType,
                                   TObjectPtr choicePtr)
{
    if ( choiceType->GetName().empty() ) {
        ReadChoiceContents(choiceType, choicePtr);
    }
    else {
        BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);

        OpenTag(choiceType);
        ReadChoiceContents(choiceType, choicePtr);
        CloseTag(choiceType);

        END_OBJECT_FRAME();
    }
}

void CObjectIStreamXml::ReadChoiceContents(const CChoiceTypeInfo* choiceType,
                                           TObjectPtr choicePtr)
{
    CLightString tagName = ReadName(BeginOpeningTag());
    CLightString id = SkipStackTagName(tagName, 0, '_');
    TMemberIndex index = choiceType->GetVariants().Find(id);
    if ( index == kInvalidMember )
        UnexpectedMember(id, choiceType->GetVariants());

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());

    variantInfo->ReadVariant(*this, choicePtr);
    
    CloseStackTag(0);
    END_OBJECT_FRAME();
}

void CObjectIStreamXml::SkipChoice(const CChoiceTypeInfo* choiceType)
{
    if ( choiceType->GetName().empty() ) {
        SkipChoiceContents(choiceType);
    }
    else {
        BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);

        OpenTag(choiceType);
        SkipChoiceContents(choiceType);
        CloseTag(choiceType);

        END_OBJECT_FRAME();
    }
}

void CObjectIStreamXml::SkipChoiceContents(const CChoiceTypeInfo* choiceType)
{
    CLightString tagName = ReadName(BeginOpeningTag());
    CLightString id = SkipStackTagName(tagName, 0, '_');
    TMemberIndex index = choiceType->GetVariants().Find(id);
    if ( index == kInvalidMember )
        UnexpectedMember(id, choiceType->GetVariants());

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());

    variantInfo->SkipVariant(*this);
    
    CloseStackTag(0);
    END_OBJECT_FRAME();
}
#endif

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

size_t CObjectIStreamXml::ReadBytes(ByteBlock& block,
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
    if ( m_Input.PeekChar() != '\'' )
        ThrowError(fFormatError, "' expected");
    m_Input.SkipChar();
    for ( ;; ) {
        char c = m_Input.GetChar();
        if ( c >= '0' && c <= '9' ) {
            continue;
        }
        else if ( c >= 'A' && c <= 'Z' ) {
            continue;
        }
        else if ( c >= 'a' && c <= 'z' ) {
            continue;
        }
        else if ( c == '\'' ) {
            break;
        }
        else {
            m_Input.UngetChar(c);
            ThrowError(fFormatError, "invalid char in octet string");
        }
    }
    if ( m_Input.PeekChar() != 'H' )
        ThrowError(fFormatError, "'H' expected");
    m_Input.SkipChar();
}

END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.71  2005/02/01 21:47:14  grichenk
* Fixed warnings
*
* Revision 1.70  2005/01/26 18:54:28  gouriano
* Changed BeginContainer / EndContainer
*
* Revision 1.69  2004/12/10 15:14:32  gouriano
* Corrected BeginClassMember for empty tags
*
* Revision 1.68  2004/09/09 21:02:00  ucko
* Remove redundant "return" from StartDelayBuffer.
*
* Revision 1.67  2004/09/09 19:16:58  vasilche
* StartDelayBuffer/EndDelayBuffer made virtual to update input if needed.
*
* Revision 1.66  2004/07/22 17:48:32  gouriano
* Corrected diagnostic message in ReadEnum
*
* Revision 1.65  2004/06/30 13:51:17  gouriano
* Corrected reading of object namespace name
*
* Revision 1.64  2004/06/22 14:58:58  gouriano
* Corrected reading namespace name of AnyContentObject
*
* Revision 1.63  2004/06/18 18:17:12  gouriano
* Use case-insensitive comparison to identify character encoding
*
* Revision 1.62  2004/06/08 20:22:32  gouriano
* Moved several functions out of VIRTUAL_MID_LEVEL_IO condition:
* there is no need for them to be there
*
* Revision 1.61  2004/05/19 17:26:19  gouriano
* Corrected serialization of containers when the code was generated by DTD
*
* Revision 1.60  2004/05/17 21:03:03  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.59  2004/04/28 19:24:29  gouriano
* Corrected reading of containers
*
* Revision 1.58  2004/03/23 15:39:23  gouriano
* Added setup options for skipping unknown data members
*
* Revision 1.57  2004/03/05 20:29:38  gouriano
* make it possible to skip unknown data fields
*
* Revision 1.56  2004/01/30 20:29:56  gouriano
* Corrected reading white spaces
*
* Revision 1.55  2004/01/29 20:48:07  gouriano
* Corrected handling of white spaces in XML tags: preserve insignificant
* white spaces inside the tag.
* Corrected reading of AnyContent object attributes and tags with empty content
*
* Revision 1.54  2004/01/22 20:50:17  gouriano
* corrected reading of undefined attributes, of boolean values, of tags with empty content
*
* Revision 1.53  2004/01/12 16:52:58  gouriano
* Corrected reading EMPTY tag value. Added skipping undescribed class member attributes.
*
* Revision 1.52  2004/01/08 18:16:53  gouriano
* correction when skipping end of line
*
* Revision 1.51  2004/01/08 17:40:53  gouriano
* Added encoding Windows-1252.
* Corrected reading of containers.
* Made it possible to read a choice variant, which is am empty container.
*
* Revision 1.50  2003/11/26 19:59:40  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.49  2003/09/25 19:44:08  gouriano
* Corrected serialization of AnyContent object's attributes
*
* Revision 1.48  2003/09/16 14:48:36  gouriano
* Enhanced AnyContent objects to support XML namespaces and attribute info items.
*
* Revision 1.47  2003/08/25 15:59:09  gouriano
* added possibility to use namespaces in XML i/o streams
*
* Revision 1.46  2003/08/14 20:03:58  vasilche
* Avoid memory reallocation when reading over preallocated object.
* Simplified CContainerTypeInfo iterators interface.
*
* Revision 1.45  2003/08/13 15:47:45  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.44  2003/07/02 13:01:29  gouriano
* added ability to read/write XML files with reference to schema
*
* Revision 1.43  2003/06/30 15:39:48  gouriano
* added encoding (utf-8 or iso-8859-1)
*
* Revision 1.42  2003/06/24 20:57:36  gouriano
* corrected code generation and serialization of non-empty unnamed containers (XML)
*
* Revision 1.41  2003/05/22 20:10:02  gouriano
* added UTF8 strings
*
* Revision 1.40  2003/05/16 18:02:18  gouriano
* revised exception error messages
*
* Revision 1.39  2003/05/05 20:09:10  gouriano
* fixed "skipping" an object
*
* Revision 1.38  2003/03/26 16:14:23  vasilche
* Removed TAB symbols. Some formatting.
*
* Revision 1.37  2003/02/07 16:09:22  gouriano
* correction of GetContainerElementTypeFamily for the case of copying objects
*
* Revision 1.36  2003/02/05 17:08:01  gouriano
* added possibility to read/write objects generated from an ASN.1 spec as "standard" XML - without scope prefixes
*
* Revision 1.35  2003/01/21 19:32:27  gouriano
* corrected reading containers of primitive types
*
* Revision 1.34  2002/12/26 19:32:33  gouriano
* changed XML I/O streams to properly handle object copying
*
* Revision 1.33  2002/12/17 19:04:32  gouriano
* corrected reading/writing of character references
*
* Revision 1.32  2002/12/13 21:50:42  gouriano
* corrected reading of choices
*
* Revision 1.31  2002/12/12 21:08:07  gouriano
* implemented handling of complex XML containers
*
* Revision 1.30  2002/11/26 22:09:02  gouriano
* added HasMoreElements method,
* added unnamed lists of sequences (choices) as container elements
*
* Revision 1.29  2002/11/20 21:22:51  gouriano
* corrected processing of unnamed sequences as choice variants
*
* Revision 1.28  2002/11/19 19:48:52  gouriano
* added support of XML attributes of choice variants
*
* Revision 1.27  2002/11/15 20:33:12  gouriano
* support of XML attributes of empty elements
*
* Revision 1.26  2002/11/14 20:58:54  gouriano
* added support of XML attribute lists
*
* Revision 1.25  2002/10/25 14:49:27  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.24  2002/10/15 13:47:59  gouriano
* modified to handle "standard" (generated from DTD) XML i/o
*
* Revision 1.23  2002/09/26 18:11:01  gouriano
* added more checks for MemberId
*
* Revision 1.22  2002/09/25 19:37:36  gouriano
* added the possibility of having no tag prefix in XML I/O streams
*
* Revision 1.21  2001/11/13 20:53:36  grichenk
* Fixed comments reading
*
* Revision 1.20  2001/10/17 20:41:24  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.19  2001/08/08 18:35:09  grichenk
* ReadTagData() -- added memory pre-allocation for long strings
*
* Revision 1.18  2001/05/17 15:07:08  lavr
* Typos corrected
*
* Revision 1.17  2000/12/26 22:24:12  vasilche
* Fixed errors of compilation on Mac.
*
* Revision 1.16  2000/12/15 15:38:44  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.15  2000/11/07 17:25:40  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.14  2000/10/20 15:51:41  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.13  2000/10/13 20:59:21  vasilche
* Avoid using of ssize_t absent on some compilers.
*
* Revision 1.12  2000/10/13 20:22:55  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.11  2000/10/03 17:22:44  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.10  2000/09/29 16:18:23  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.9  2000/09/18 20:00:24  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.8  2000/09/01 13:16:18  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.7  2000/08/15 19:44:49  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.6  2000/07/03 20:47:23  vasilche
* Removed unused variables/functions.
*
* Revision 1.5  2000/07/03 18:42:45  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.4  2000/06/16 19:24:22  vasilche
* Updated MSVC project.
* Fixed error on MSVC with static const class member.
*
* Revision 1.3  2000/06/16 16:31:20  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.2  2000/06/07 19:45:59  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.1  2000/06/01 19:07:04  vasilche
* Added parsing of XML data.
*
* ===========================================================================
*/
