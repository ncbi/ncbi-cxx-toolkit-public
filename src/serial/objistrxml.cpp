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
* Added interface for costructing container objects directly into output stream.
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
                    ThrowError(eFormatError, "bad tag");
                }
            }
        default:
            m_Input.UngetChar('<');
            Back_lt();
            ThrowError(eFormatError, "unknown DOCTYPE");
        }
    }
    return NcbiEmptyString;
}

int CObjectIStreamXml::ReadEscapedChar(char endingChar)
{
    char c = m_Input.PeekChar();
    if ( c == '&' ) {
        m_Input.SkipChar();
        const size_t limit = 32;
        size_t offset = m_Input.PeekFindChar(';', limit);
        if ( offset >= limit )
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
    if ( !EndOpeningTagSelfClosed() )
        ThrowError(eFormatError, "boolean tag must have empty contents");
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
        ThrowError(eFormatError, "bad float number");
    return data;
}

void CObjectIStreamXml::ReadNull(void)
{
    if ( !EndOpeningTagSelfClosed() )
        ThrowError(eFormatError, "empty tag expected");
}

void CObjectIStreamXml::ReadString(string& str)
{
    str.erase();
    if ( !EndOpeningTagSelfClosed() )
        ReadTagData(str);
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
    for ( ;; ) {
        int c = ReadEscapedChar('<');
        if ( c < 0 )
            break;
        str += char(c);
    }
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
        if ( SkipWS() == '>' ) {
            // no attribute
            if ( !values.IsInteger() )
                ThrowError(eFormatError, "attribute 'value' expected");
            m_Input.SkipChar();
            Found_gt();
            BeginData();
            value = m_Input.GetInt4();
        }
        else {
            CLightString attr = ReadAttributeName();
            if ( attr != "value" )
                ThrowError(eFormatError, "attribute 'value' expected");
            string valueName;
            ReadAttributeValue(valueName);
            value = values.FindValue(valueName);
            if ( !EndOpeningTagSelfClosed() && values.IsInteger() ) {
                // read integer value
                SkipWSAndComments();
                if ( value != m_Input.GetInt4() )
                    ThrowError(eFormatError,
                               "incompatible name and value of enum");
            }
        }
    }
    else {
        // outside of tag
        if ( !values.IsInteger() )
            ThrowError(eFormatError, "attribute 'value' expected");
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
    if ( InsideOpeningTag() && EndOpeningTagSelfClosed() ) {
        // self closed tag
        return eNullPointer;
    }
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

CLightString CObjectIStreamXml::SkipTagName(CLightString tag,
                                            const char* str, size_t length)
{
    if ( tag.GetLength() < length ||
         memcmp(tag.GetString(), str, length) != 0 )
        ThrowError(eFormatError, "invalid tag name");
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
    ThrowError(eIllegalCall, "illegal frame type");
    return tag;
}

CLightString CObjectIStreamXml::SkipStackTagName(CLightString tag,
                                                 size_t level, char c)
{
    tag = SkipStackTagName(tag, level);
    if ( tag.Empty() || *tag.GetString() != c )
        ThrowError(eFormatError, "invalid tag name");
    return CLightString(tag.GetString() + 1, tag.GetLength() - 1);
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

void CObjectIStreamXml::OpenStackTag(size_t level)
{
    CLightString tagName = ReadName(BeginOpeningTag());
    CLightString rest = SkipStackTagName(tagName, level);
    if ( !rest.Empty() )
        ThrowError(eFormatError, "unexpected tag");
}

void CObjectIStreamXml::CloseStackTag(size_t level)
{
    if ( SelfClosedTag() ) {
        EndSelfClosedTag();
    }
    else {
        CLightString tagName = ReadName(BeginClosingTag());
        CLightString rest = SkipStackTagName(tagName, level);
        if ( !rest.Empty() )
            ThrowError(eFormatError, "unexpected tag");
        EndClosingTag();
    }
}

void CObjectIStreamXml::OpenTagIfNamed(TTypeInfo type)
{
    if ( !type->GetName().empty() )
        OpenTag(type->GetName());
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
        elementType = CTypeConverter<CPointerTypeInfo>::SafeCast(elementType)->GetPointedType();
    }
    // found named type
    return true;
}

inline
bool CObjectIStreamXml::NextTagIsClosing(void)
{
    BeginData();
    return SkipWSAndComments() == '<' && m_Input.PeekChar(1) == '/';
}

void CObjectIStreamXml::BeginContainer(const CContainerTypeInfo* containerType)
{
    OpenTagIfNamed(containerType);
}

void CObjectIStreamXml::EndContainer(void)
{
    CloseTagIfNamed(TopFrame().GetTypeInfo());
}

bool CObjectIStreamXml::BeginContainerElement(TTypeInfo elementType)
{
    if ( NextTagIsClosing() )
        return false;
    if ( !WillHaveName(elementType) )
        OpenStackTag(0);
    return true;
}

void CObjectIStreamXml::EndContainerElement(void)
{
    if ( !WillHaveName(TopFrame().GetTypeInfo()) )
        CloseStackTag(0);
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

void CObjectIStreamXml::ReadContainerContents(const CContainerTypeInfo* cType,
                                              TObjectPtr containerPtr)
{
    TTypeInfo elementType = cType->GetElementType();
    if ( !WillHaveName(elementType) ) {
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        while ( !NextTagIsClosing() ) {
            OpenStackTag(0);
            cType->AddElement(containerPtr, *this);
            CloseStackTag(0);
        }

        END_OBJECT_FRAME();
    }
    else {
        while ( !NextTagIsClosing() ) {
            cType->AddElement(containerPtr, *this);
        }
    }
}

void CObjectIStreamXml::SkipContainerContents(const CContainerTypeInfo* cType)
{
    TTypeInfo elementType = cType->GetElementType();
    if ( !WillHaveName(elementType) ) {
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        while ( !NextTagIsClosing() ) {
            OpenStackTag(0);
            SkipObject(elementType);
            CloseStackTag(0);
        }
        
        END_OBJECT_FRAME();
    }
    else {
        while ( !NextTagIsClosing() ) {
            SkipObject(elementType);
        }
    }
}
#endif

void CObjectIStreamXml::BeginNamedType(TTypeInfo namedTypeInfo)
{
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

    OpenTag(namedTypeInfo);
    ReadObject(object, typeInfo);
    CloseTag(namedTypeInfo);

    END_OBJECT_FRAME();
}
#endif

void CObjectIStreamXml::BeginClass(const CClassTypeInfo* classInfo)
{
    OpenTagIfNamed(classInfo);
}

void CObjectIStreamXml::EndClass(void)
{
    CloseTagIfNamed(TopFrame().GetTypeInfo());
}

void CObjectIStreamXml::UnexpectedMember(const CLightString& id,
                                         const CItemsInfo& items)
{
    string message =
        "\""+string(id)+"\": unexpected member, should be one of: ";
    for ( CItemsInfo::CIterator i(items); i.Valid(); ++i ) {
        message += '\"' + items.GetItemInfo(i)->GetId().ToString() + "\" ";
    }
    ThrowError(eFormatError, message);
}

TMemberIndex
CObjectIStreamXml::BeginClassMember(const CClassTypeInfo* classType)
{
    if ( NextTagIsClosing() )
        return kInvalidMember;

    CLightString tagName = ReadName(BeginOpeningTag());
    CLightString id = SkipStackTagName(tagName, 1, '_');
    TMemberIndex index = classType->GetMembers().Find(id);
    if ( index == kInvalidMember )
        UnexpectedMember(id, classType->GetMembers());
    return index;
}

TMemberIndex
CObjectIStreamXml::BeginClassMember(const CClassTypeInfo* classType,
                                    TMemberIndex pos)
{
    if ( NextTagIsClosing() )
        return kInvalidMember;

    CLightString tagName = ReadName(BeginOpeningTag());
    CLightString id = SkipStackTagName(tagName, 1, '_');
    TMemberIndex index = classType->GetMembers().Find(id, pos);
    if ( index == kInvalidMember )
        UnexpectedMember(id, classType->GetMembers());
    return index;
}

void CObjectIStreamXml::EndClassMember(void)
{
    CloseStackTag(0);
}

TMemberIndex CObjectIStreamXml::BeginChoiceVariant(const CChoiceTypeInfo* choiceType)
{
    OpenTagIfNamed(choiceType);
    CLightString tagName = ReadName(BeginOpeningTag());
    CLightString id = SkipStackTagName(tagName, 0, '_');
    return choiceType->GetVariants().Find(id);
}

void CObjectIStreamXml::EndChoiceVariant(void)
{
    CloseStackTag(0);
    CloseTagIfNamed(FetchFrameFromTop(1).GetTypeInfo());
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
        ThrowError(eFormatError, "bad number");
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
        ThrowError(eFormatError, "bad number");
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

void CObjectIStreamXml::SkipString(void)
{
    BeginData();
    while ( ReadEscapedChar('<') >= 0 )
        continue;
}

void CObjectIStreamXml::SkipNull(void)
{
    if ( !EndOpeningTagSelfClosed() )
        ThrowError(eFormatError, "empty tag expected");
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
