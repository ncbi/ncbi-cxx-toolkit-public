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

#include <corelib/ncbistd.hpp>
#include <serial/objistrxml.hpp>
#include <serial/enumvalues.hpp>
#include <serial/memberlist.hpp>
#include <serial/memberid.hpp>

BEGIN_NCBI_SCOPE

CObjectIStream* CreateObjectIStreamXml()
{
    return new CObjectIStreamXml();
}

CObjectIStreamXml::CObjectIStreamXml(void)
    : m_TagState(eTagOutside)
{
}

CObjectIStreamXml::~CObjectIStreamXml(void)
{
}

ESerialDataFormat CObjectIStreamXml::GetDataFormat(void) const
{
    return eSerial_Xml;
}

string CObjectIStreamXml::GetPosition(void) const
{
    return "line "+NStr::UIntToString(m_Input.GetLine());
}

inline
bool CObjectIStreamXml::InsideTag(void) const
{
    return m_TagState == eTagInsideOpening || m_TagState == eTagInsideClosing;
}

inline
bool CObjectIStreamXml::InsideOpeningTag(void) const
{
    return m_TagState == eTagInsideOpening;
}

inline
bool CObjectIStreamXml::InsideClosingTag(void) const
{
    return m_TagState == eTagInsideClosing;
}

inline
bool CObjectIStreamXml::OutsideTag(void) const
{
    return m_TagState == eTagOutside;
}

inline
bool CObjectIStreamXml::SelfClosedTag(void) const
{
    return m_TagState == eTagSelfClosed;
}

inline
void CObjectIStreamXml::Found_lt(void)
{
    _ASSERT(OutsideTag());
    m_TagState = eTagInsideOpening;
}

inline
void CObjectIStreamXml::Back_lt(void)
{
    _ASSERT(InsideOpeningTag());
    m_TagState = eTagOutside;
}

inline
void CObjectIStreamXml::Found_lt_slash(void)
{
    _ASSERT(OutsideTag());
    m_TagState = eTagInsideClosing;
}

inline
void CObjectIStreamXml::Found_gt(void)
{
    _ASSERT(InsideTag());
    m_TagState = eTagOutside;
}

inline
void CObjectIStreamXml::Found_slash_gt(void)
{
    _ASSERT(InsideOpeningTag());
    m_TagState = eTagSelfClosed;
}

inline
void CObjectIStreamXml::EndSelfClosedTag(void)
{
    _ASSERT(SelfClosedTag());
    m_TagState = eTagOutside;
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
                            ThrowError(eFormatError, "double dash '--' is not allowed in XML comments");
                        }
                    }
                    else {
                        // -[^-]
                        m_Input.SkipChars(2);
                    }
                    
                }
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
    if ( c != '>' ) {
        ThrowError(eFormatError, "'>' expected");
    }
    m_Input.SkipChar();
    Found_gt();
}

inline
void CObjectIStreamXml::EndOpeningTag(void)
{
    _ASSERT(InsideOpeningTag());
    EndTag();
}

inline
void CObjectIStreamXml::EndClosingTag(void)
{
    _ASSERT(InsideClosingTag());
    EndTag();
}

inline
bool CObjectIStreamXml::EndOpeningTagSelfClosed(void)
{
    _ASSERT(InsideOpeningTag());
    char c = SkipWS();
    if ( c == '/' && m_Input.PeekChar(1) == '>' ) {
        // end of self closed tag
        m_Input.SkipChars(2);
        Found_slash_gt();
        return true;
    }

    if ( c != '>' )
        ThrowError(eFormatError, "end of tag expected");

    // end of open tag
    m_Input.SkipChar(); // '>'
    Found_gt();
    return false;
}

inline
void CObjectIStreamXml::BeginData(void)
{
    if ( InsideOpeningTag() )
        EndOpeningTag();
    _ASSERT(OutsideTag());
}

char CObjectIStreamXml::BeginOpeningTag(void)
{
    BeginData();
    // find beginning '<'
    char c = SkipWSAndComments();
    if ( c != '<' )
        ThrowError(eFormatError, "'<' expected");
    c = m_Input.PeekChar(1);
    if ( c == '/' )
        ThrowError(eFormatError, "unexpected '</'");
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
        ThrowError(eFormatError, "'</' expected");
    m_Input.SkipChars(2);
    Found_lt_slash();
    return m_Input.PeekChar();
}

CLightString CObjectIStreamXml::ReadName(char c)
{
    _ASSERT(InsideTag());
    if ( !IsFirstNameChar(c) )
        ThrowError(eFormatError, "bad first name symbol");

    // find end of tag name
    size_t i = 1;
    while ( IsNameChar(c = m_Input.PeekChar(i)) ) {
        ++i;
    }

    // save beginning of tag name
    const char* ptr = m_Input.GetCurrentPos();

    // check end of tag name
    if ( IsWhiteSpace(c) ) {
        // whitespace -> attributes may follow
        m_Input.SkipChars(i + 1);
    }
    else {
        m_Input.SkipChars(i);
    }
    return CLightString(ptr, i);
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

string CObjectIStreamXml::ReadTypeName(void)
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
                    ThrowError(eFormatError, "bad tag");
                }
            }
        default:
            m_Input.UngetChar('<');
            Back_lt();
            return NcbiEmptyString;
        }
    }
}

int CObjectIStreamXml::ReadEscapedChar(char endingChar)
{
    char c = m_Input.PeekChar();
    if ( c == '&' ) {
        m_Input.SkipChar();
        int offset = m_Input.PeekFindChar(';', 32);
        if ( offset < 0 )
            ThrowError(eFormatError, "too long entity reference");
        const char* p = m_Input.GetCurrentPos(); // save entity string pointer
        m_Input.SkipChars(offset + 1); // skip it
        if ( offset == 0 )
            ThrowError(eFormatError, "invalid entity reference");
        if ( *p == '#' ) {
            const char* end = p + offset;
            ++p;
            // char ref
            if ( p == end )
                ThrowError(eFormatError, "invalid char reference");
            unsigned v = 0;
            if ( *p == 'x' ) {
                // hex
                if ( ++p == end )
                    ThrowError(eFormatError, "invalid char reference");
                do {
                    c = *p++;
                    if ( c >= '0' || c <= '9' )
                        v = v * 16 + (c - '0');
                    else if ( c >= 'A' && c <='F' )
                        v = v * 16 + (c - 'A');
                    else if ( c >= 'a' && c <='f' )
                        v = v * 16 + (c - 'a');
                    else
                        ThrowError(eFormatError, "bad char reference");
                } while ( p < end );
            }
            else {
                // dec
                if ( p == end )
                    ThrowError(eFormatError, "invalid char reference");
                do {
                    c = *p++;
                    if ( c >= '0' || c <= '9' )
                        v = v * 10 + (c - '0');
                    else
                        ThrowError(eFormatError, "bad char reference");
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
            ThrowError(eFormatError, "unknown entity name");
        }
    }
    else if ( c == endingChar ) {
        return -1;
    }
    m_Input.SkipChar();
    return c & 0xFF;
}

CLightString CObjectIStreamXml::ReadAttributeName(void)
{
    if ( OutsideTag() )
        ThrowError(eFormatError, "attribute expected");
    return ReadName(SkipWS());
}

void CObjectIStreamXml::ReadAttributeValue(string& value)
{
    if ( SkipWS() != '=' )
        ThrowError(eFormatError, "'=' expected");
    m_Input.SkipChar(); // '='
    char startChar = SkipWS();
    if ( startChar != '\'' && startChar != '\"' )
        ThrowError(eFormatError, "attribute value must start with ' or \"");
    m_Input.SkipChar();
    for ( ;; ) {
        int c = ReadEscapedChar(startChar);
        if ( c < 0 )
            break;
        value += char(c);
    }
    m_Input.SkipChar();
}

bool CObjectIStreamXml::ReadBool(void)
{
    CLightString attr = ReadAttributeName();
    if ( attr != "value" )
        ThrowError(eFormatError, "attribute 'value' expected");
    string sValue;
    ReadAttributeValue(sValue);
    bool value;
    if ( sValue == "true" )
        value = true;
    else {
        if ( sValue != "false" ) {
            ThrowError(eFormatError,
                       "'true' or 'false' attrubute value expected");
        }
        value = false;
    }
    EndOpeningTagSelfClosed();
    return value;
}

char CObjectIStreamXml::ReadChar(void)
{
    BeginData();
    int c = ReadEscapedChar('<');
    if ( c < 0 || m_Input.PeekChar() != '<' )
        ThrowError(eFormatError, "one char tag content expected");
    return c;
}

int CObjectIStreamXml::ReadInt(void)
{
    BeginData();
    return m_Input.GetInt();
}

unsigned CObjectIStreamXml::ReadUInt(void)
{
    BeginData();
    return m_Input.GetUInt();
}

long CObjectIStreamXml::ReadLong(void)
{
    BeginData();
    return m_Input.GetLong();
}

unsigned long CObjectIStreamXml::ReadULong(void)
{
    BeginData();
    return m_Input.GetULong();
}

double CObjectIStreamXml::ReadDouble(void)
{
    string s;
    ReadTagData(s);
    char* endptr;
    double data = strtod(s.c_str(), &endptr);
    if ( *endptr != 0 )
        ThrowError(eFormatError, "bad float number");
    return data;
}

void CObjectIStreamXml::ReadNull(void)
{
    string s;
    ReadTagData(s);
    if ( s != "null" )
        ThrowError(eFormatError, "'null' contents expected");
/*
    CLightString attr = ReadAttributeName();
    if ( attr != "null" )
        ThrowError(eFormatError, "attribute 'null' expected");
    EndOpeningTagSelfClosed();
*/
}

void CObjectIStreamXml::ReadString(string& str)
{
    str.erase();
    ReadTagData(str);
}

void CObjectIStreamXml::ReadTagData(string& str)
{
    BeginData();
    for ( ;; ) {
        int c = ReadEscapedChar('<');
        if ( c < 0 )
            break;
        str += char(c);
    }
}

pair<long, bool>
CObjectIStreamXml::ReadEnum(const CEnumeratedTypeValues& values)
{
    const string& enumName = values.GetName();
    if ( !enumName.empty() ) {
        // global enum
        OpenTag(enumName);
        _ASSERT(InsideOpeningTag());
    }
    pair<long, bool> res;
    if ( InsideOpeningTag() ) {
        // try to read attribute 'value'
        if ( SkipWS() == '>' ) {
            // no attribute
            if ( !values.IsInteger() )
                ThrowError(eFormatError, "attribute 'value' expected");
            m_Input.SkipChar();
            Found_gt();
            res.second = false;
        }
        else {
            CLightString attr = ReadAttributeName();
            if ( attr != "value" )
                ThrowError(eFormatError, "attribute 'value' expected");
            string valueName;
            ReadAttributeValue(valueName);
            res.first = values.FindValue(valueName);
            res.second = true;
            if ( !EndOpeningTagSelfClosed() && values.IsInteger() ) {
                // read integer value
                SkipWSAndComments();
                if ( res.first != m_Input.GetLong() )
                    ThrowError(eFormatError,
                               "incompatible name and value of enum");
            }
        }
    }
    else {
        // outside of tag
        if ( !values.IsInteger() )
            ThrowError(eFormatError, "attribute 'value' expected");
        res.second = false;
    }
    if ( !enumName.empty() ) {
        // global enum
        CloseTag(enumName);
    }
    return res;
}

CObjectIStream::EPointerType CObjectIStreamXml::ReadPointerType(void)
{
    return eThisPointer;
}

CObjectIStreamXml::TObjectIndex CObjectIStreamXml::ReadObjectPointer(void)
{
    ThrowError(eIllegalCall, "unimplemented");
    return 0;
/*
    CLightString attr = ReadAttributeName();
    if ( attr != "index" )
        ThrowError(eIllegalCall, "attribute 'index' expected");
    string index;
    ReadAttributeValue(index);
    EndOpeningTagSelfClosed();
    return NStr::StringToInt(index);
*/
}

string CObjectIStreamXml::ReadOtherPointer(void)
{
    ThrowError(eIllegalCall, "unimplemented");
    return NcbiEmptyString;
}

inline
CLightString CObjectIStreamXml::SkipTagName(CLightString tag, char c)
{
    if ( tag.Empty() || *tag.GetString() != c )
        ThrowError(eFormatError, "invalid tag name");
    return CLightString(tag.GetString() + 1, tag.GetLength() - 1);
}

CLightString CObjectIStreamXml::SkipTagName(CLightString tag,
                                            const char* str, size_t length)
{
    if ( tag.GetLength() < length ||
         memcmp(tag.GetString(), str, length) != 0 )
        ThrowError(eFormatError, "invalid tag name");
    return CLightString(tag.GetString() + length, tag.GetLength() - length);
}

inline
CLightString CObjectIStreamXml::SkipTagName(CLightString tag, const char* s)
{
    return SkipTagName(tag, s, strlen(s));
}

inline
CLightString CObjectIStreamXml::SkipTagName(CLightString tag, const string& s)
{
    return SkipTagName(tag, s.data(), s.size());
}

CLightString CObjectIStreamXml::SkipTagName(CLightString tag,
                                            const CObjectStackFrame& e)
{
    if ( e.GetNameType() == CObjectStackFrame::eNameTypeInfo ) {
        const string& name = e.GetNameTypeInfo()->GetName();
        if ( !name.empty() ) {
            return SkipTagName(tag, name);
        }
        else {
            _ASSERT(e.GetPrevous());
            return SkipTagName(tag, *e.GetPrevous());
        }
    }
    else {
        _ASSERT(e.GetPrevous());
        tag = SkipTagName(tag, *e.GetPrevous());
        if ( e.HaveName() ) {
            tag = SkipTagName(tag, '_');
            switch ( e.GetNameType() ) {
            case CObjectStackFrame::eNameCharPtr:
                return SkipTagName(tag, e.GetNameCharPtr());
                break;
            case CObjectStackFrame::eNameString:
                return SkipTagName(tag, e.GetNameString());
                break;
            case CObjectStackFrame::eNameId:
                return SkipTagName(tag, e.GetNameId().GetName());
                break;
            case CObjectStackFrame::eNameChar:
                return SkipTagName(tag, e.GetNameChar());
                break;
            case CObjectStackFrame::eNameTypeInfo:
                return SkipTagName(tag, e.GetNameTypeInfo()->GetName());
                break;
            default:
                break;
            }
        }
        return tag;
    }
}

void CObjectIStreamXml::OpenTag(const string& e)
{
    CLightString tagName = ReadName(BeginOpeningTag());
    if ( tagName != e )
        ThrowError(eFormatError, "tag '"+e+"' expected");
}

void CObjectIStreamXml::CloseTag(const string& e)
{
    if ( SelfClosedTag() ) {
        EndSelfClosedTag();
    }
    else {
        CLightString tagName = ReadName(BeginClosingTag());
        if ( tagName != e )
            ThrowError(eFormatError, "tag '"+e+"' expected");
        EndClosingTag();
    }
}

void CObjectIStreamXml::OpenTag(const CObjectStackFrame& e)
{
    CLightString tagName = ReadName(BeginOpeningTag());
    CLightString rest = SkipTagName(tagName, e);
    if ( !rest.Empty() )
        ThrowError(eFormatError, "unexpected tag");
}

void CObjectIStreamXml::CloseTag(const CObjectStackFrame& e)
{
    if ( SelfClosedTag() ) {
        EndSelfClosedTag();
    }
    else {
        CLightString tagName = ReadName(BeginClosingTag());
        CLightString rest = SkipTagName(tagName, e);
        if ( !rest.Empty() )
            ThrowError(eFormatError, "unexpected tag");
        EndClosingTag();
    }
}

inline
bool CObjectIStreamXml::NextTagIsClosing(void)
{
    BeginData();
    return SkipWSAndComments() == '<' && m_Input.PeekChar(1) == '/';
}

void CObjectIStreamXml::ReadArray(CObjectArrayReader& reader,
                                  TTypeInfo arrayType, bool randomOrder,
                                  TTypeInfo elementType)
{
    const string& arrayName = arrayType->GetName();
    if ( arrayName.empty() ) {
        ReadArrayContents(reader, elementType);
    }
    else {
        CObjectStackArray array(*this, arrayType, randomOrder);
        OpenTag(arrayName);
        ReadArrayContents(reader, elementType);
        CloseTag(arrayName);
        array.End();
    }
}

void CObjectIStreamXml::ReadArrayContents(CObjectArrayReader& reader,
                                          TTypeInfo elementType)
{
    if ( NextTagIsClosing() )
        return;

    if ( elementType->GetName().empty() ) {
        CObjectStackArrayElement element(*this, elementType != 0);
        element.Begin();
            
        do {
            OpenTag(element);
                
            reader.ReadElement(*this);
                
            CloseTag(element);
        } while ( !NextTagIsClosing() );
            
        element.End();
    }
    else {
        do {
            reader.ReadElement(*this);
        } while ( !NextTagIsClosing() );
    }
}

void CObjectIStreamXml::ReadNamedType(TTypeInfo namedTypeInfo,
                                      TTypeInfo typeInfo,
                                      TObjectPtr object)
{
    CObjectStackNamedFrame name(*this, namedTypeInfo);
    const string& typeName = namedTypeInfo->GetName();
    _ASSERT(!typeName.empty());
    OpenTag(typeName);
    typeInfo->ReadData(*this, object);
    CloseTag(typeName);
    name.End();
}

void CObjectIStreamXml::BeginClass(CObjectStackClass& cls)
{
    const string& name = cls.GetTypeInfo()->GetName();
    if ( !name.empty() )
        OpenTag(name);
}

void CObjectIStreamXml::EndClass(CObjectStackClass& cls)
{
    const string& name = cls.GetTypeInfo()->GetName();
    if ( !name.empty() )
        CloseTag(name);
    cls.End();
}

void CObjectIStreamXml::UnexpectedMember(const CLightString& id,
                                         const CMembers& members)
{
    string message =
        "\""+string(id)+"\": unexpected member, should be one of: ";
    iterate ( CMembers::TMembers, i, members.GetMembers() ) {
        message += '\"' + i->ToString() + "\" ";
    }
    ThrowError(eFormatError, message);
}

CObjectIStreamXml::TMemberIndex
CObjectIStreamXml::BeginClassMember(CObjectStackClassMember& m,
                                    const CMembers& members)
{
    if ( NextTagIsClosing() )
        return -1;

    CLightString tagName = ReadName(BeginOpeningTag());
    CLightString id =
        SkipTagName(SkipTagName(tagName, m.GetClassFrame()), '_');
    TMemberIndex index = members.FindMember(id);
    if ( index < 0 )
        UnexpectedMember(id, members);
    m.SetName(members.GetMemberId(index));
    m.Begin();
    return index;
}

CObjectIStreamXml::TMemberIndex
CObjectIStreamXml::BeginClassMember(CObjectStackClassMember& m,
                                    const CMembers& members,
                                    CClassMemberPosition& pos)
{
    if ( NextTagIsClosing() )
        return -1;

    CLightString tagName = ReadName(BeginOpeningTag());
    CLightString id =
        SkipTagName(SkipTagName(tagName, m.GetClassFrame()), '_');
    TMemberIndex index = members.FindMember(id, pos.GetLastIndex());
    if ( index < 0 )
        UnexpectedMember(id, members);
    m.SetName(members.GetMemberId(index));
    m.Begin();
    pos.SetLastIndex(index);
    return index;
}

void CObjectIStreamXml::EndClassMember(CObjectStackClassMember& m)
{
    CloseTag(m);
    m.End();
}

void CObjectIStreamXml::ReadChoice(CObjectChoiceReader& reader,
                                   TTypeInfo choiceInfo,
                                   const CMembersInfo& members)
{
    const string& choiceName = choiceInfo->GetName();
    if ( choiceName.empty() ) {
        ReadChoiceVariant(reader, members);
    }
    else {
        CObjectStackChoice choice(*this, choiceInfo);
        OpenTag(choiceName);
        ReadChoiceVariant(reader, members);
        CloseTag(choiceName);
        choice.End();
    }
}

void CObjectIStreamXml::ReadChoiceVariant(CObjectChoiceReader& reader,
                                          const CMembersInfo& members)
{
    CLightString tagName = ReadName(BeginOpeningTag());
    _ASSERT(GetTop());
    CLightString id = SkipTagName(SkipTagName(tagName, *GetTop()), '_');
    TMemberIndex index = members.FindMember(id);
    if ( index < 0 )
        UnexpectedMember(id, members);
    CObjectStackChoiceVariant variant(*this, members.GetMemberId(index));
    reader.ReadChoiceVariant(*this, members, index);
    CloseTag(variant);
    variant.End();
}

static const char* const HEX = "0123456789ABCDEF";

void CObjectIStreamXml::BeginBytes(ByteBlock& )
{
    BeginData();
    if ( m_Input.PeekChar() != '\'' )
        ThrowError(eFormatError, "' expected");
    m_Input.SkipChar();
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
    else if ( c != '\'' ) {
        m_Input.UngetChar(c);
        ThrowError(eFormatError, "bad char in octet string");
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

void CObjectIStreamXml::EndBytes(const ByteBlock& )
{
	if ( m_Input.PeekChar() != 'H' )
        ThrowError(eFormatError, "'H' expected");
    m_Input.SkipChar();
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
    ReadLong();
}

void CObjectIStreamXml::SkipUNumber(void)
{
    ReadULong();
}

void CObjectIStreamXml::SkipFNumber(void)
{
    ReadDouble();
}

void CObjectIStreamXml::SkipString(void)
{
    BeginData();
    while ( ReadEscapedChar('<') >= 0 )
        continue;
}

void CObjectIStreamXml::SkipNull(void)
{
    ReadNull();
}

void CObjectIStreamXml::SkipByteBlock(void)
{
    BeginData();
    if ( m_Input.PeekChar() != '\'' )
        ThrowError(eFormatError, "' expected");
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
            ThrowError(eFormatError, "bad char in octet string");
        }
    }
	if ( m_Input.PeekChar() != 'H' )
        ThrowError(eFormatError, "'H' expected");
    m_Input.SkipChar();
}

END_NCBI_SCOPE
