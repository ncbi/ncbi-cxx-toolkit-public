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
*   XML object output stream
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.22  2000/12/15 15:38:45  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.21  2000/12/04 19:02:41  beloslyu
* changes for FreeBSD
*
* Revision 1.20  2000/11/07 17:25:41  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.19  2000/10/20 15:51:43  vasilche
* Fixed data error processing.
* Added interface for costructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.18  2000/10/17 18:45:35  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.17  2000/10/13 20:22:56  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.16  2000/10/13 16:28:40  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.15  2000/10/05 15:52:51  vasilche
* Avoid useing snprintf bacause it's missing on osf1_gcc
*
* Revision 1.14  2000/10/05 13:17:17  vasilche
* Added missing #include <stdio.h>
*
* Revision 1.13  2000/10/04 19:19:00  vasilche
* Fixed processing floating point data.
*
* Revision 1.12  2000/10/03 17:22:45  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.11  2000/09/29 16:18:24  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.10  2000/09/26 17:38:22  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.9  2000/09/18 20:00:25  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.8  2000/09/01 13:16:20  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.7  2000/08/15 19:44:50  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.6  2000/07/03 18:42:46  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.5  2000/06/16 19:24:22  vasilche
* Updated MSVC project.
* Fixed error on MSVC with static const class member.
*
* Revision 1.4  2000/06/16 16:31:22  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.3  2000/06/07 19:46:00  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.2  2000/06/01 19:07:05  vasilche
* Added parsing of XML data.
*
* Revision 1.1  2000/05/24 20:08:49  vasilche
* Implemented XML dump.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostrxml.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/memberid.hpp>
#include <serial/memberlist.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objhook.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/continfo.hpp>
#include <serial/delaybuf.hpp>
#include <serial/ptrinfo.hpp>

#include <stdio.h>
#include <math.h>
#include <limits.h>
#if HAVE_WINDOWS_H || defined(__FreeBSD__)
// In MSVC limits.h doesn't define FLT_MIN & FLT_MAX
// FreeBSD needs this to find FLT_DIG and DBL_DIG
# include <float.h>
#endif

BEGIN_NCBI_SCOPE

CObjectOStream* OpenObjectOStreamXml(CNcbiOstream& out, bool deleteOut)
{
    return new CObjectOStreamXml(out, deleteOut);
}

CObjectOStreamXml::CObjectOStreamXml(CNcbiOstream& out, bool deleteOut)
    : CObjectOStream(out, deleteOut), m_LastTagAction(eTagClose)
{
    m_Output.SetBackLimit(1);
}

CObjectOStreamXml::~CObjectOStreamXml(void)
{
}

ESerialDataFormat CObjectOStreamXml::GetDataFormat(void) const
{
    return eSerial_Xml;
}

string CObjectOStreamXml::GetPosition(void) const
{
    return "line "+NStr::UIntToString(m_Output.GetLine());
}

static string GetPublicModuleName(TTypeInfo type)
{
    const string& s = type->GetModuleName();
    string name;
    for ( string::const_iterator i = s.begin(); i != s.end(); ++i ) {
        char c = *i;
        if ( !isalnum(c) )
            name += ' ';
        else
            name += c;
    }
    return name;
}

static string GetModuleName(TTypeInfo type)
{
    const string& s = type->GetModuleName();
    string name;
    for ( string::const_iterator i = s.begin(); i != s.end(); ++i ) {
        char c = *i;
        if ( c == '-' )
            name += '_';
        else
            name += c;
    }
    return name;
}

void CObjectOStreamXml::WriteFileHeader(TTypeInfo type)
{
    m_Output.PutString("<?xml version=\"1.0\"?>");
    m_Output.PutEol();
    m_Output.PutString("<!DOCTYPE ");
    m_Output.PutString(type->GetName());
    m_Output.PutString(" PUBLIC \"-//NCBI//");
    m_Output.PutString(GetPublicModuleName(type));
    m_Output.PutString("/EN\" \"");
    m_Output.PutString(GetModuleName(type));
    m_Output.PutString(".dtd\">");
    m_Output.PutEol();
    m_LastTagAction = eTagClose;
}

void CObjectOStreamXml::WriteEnum(const CEnumeratedTypeValues& values,
                                  TEnumValueType value,
                                  const string& valueName)
{
    if ( !values.GetName().empty() ) {
        // global enum
        OpenTagStart();
        m_Output.PutString(values.GetName());
        if ( !valueName.empty() ) {
            m_Output.PutString(" value=\"");
            m_Output.PutString(valueName);
            m_Output.PutChar('\"');
        }
        if ( values.IsInteger() ) {
            OpenTagEnd();
            m_Output.PutInt4(value);
            CloseTagStart();
            m_Output.PutString(values.GetName());
            CloseTagEnd();
        }
        else {
            _ASSERT(!valueName.empty());
            SelfCloseTagEnd();
            m_LastTagAction = eTagClose;
        }
    }
    else {
        // local enum (member, variant or element)
        if ( valueName.empty() ) {
            _ASSERT(values.IsInteger());
            m_Output.PutInt4(value);
        }
        else {
            OpenTagEndBack();
            m_Output.PutString(" value=\"");
            m_Output.PutString(valueName);
            m_Output.PutChar('"');
            if ( values.IsInteger() ) {
                OpenTagEnd();
                m_Output.PutInt4(value);
            }
            else {
                SelfCloseTagEnd();
            }
        }
    }
}

void CObjectOStreamXml::WriteEnum(const CEnumeratedTypeValues& values,
                                  TEnumValueType value)
{
    WriteEnum(values, value, values.FindName(value, values.IsInteger()));
}

void CObjectOStreamXml::CopyEnum(const CEnumeratedTypeValues& values,
                                 CObjectIStream& in)
{
    TEnumValueType value = in.ReadEnum(values);
    WriteEnum(values, value, values.FindName(value, values.IsInteger()));
}

void CObjectOStreamXml::WriteEscapedChar(char c)
{
    switch ( c ) {
    case '&':
        m_Output.PutString("&amp;");
        break;
    case '<':
        m_Output.PutString("&lt;");
        break;
    case '>':
        m_Output.PutString("&gt;");
        break;
    case '\'':
        m_Output.PutString("&apos;");
        break;
    case '"':
        m_Output.PutString("&quot;");
        break;
    default:
        m_Output.PutChar(c);
        break;
    }
}

void CObjectOStreamXml::WriteBool(bool data)
{
    OpenTagEndBack();
    if ( data )
        m_Output.PutString(" value=\"true\"");
    else
        m_Output.PutString(" value=\"false\"");
    SelfCloseTagEnd();
}

void CObjectOStreamXml::WriteChar(char data)
{
    WriteEscapedChar(data);
}

void CObjectOStreamXml::WriteInt4(Int4 data)
{
    m_Output.PutInt4(data);
}

void CObjectOStreamXml::WriteUint4(Uint4 data)
{
    m_Output.PutUint4(data);
}

void CObjectOStreamXml::WriteInt8(Int8 data)
{
    m_Output.PutInt8(data);
}

void CObjectOStreamXml::WriteUint8(Uint8 data)
{
    m_Output.PutUint8(data);
}

void CObjectOStreamXml::WriteDouble2(double data, size_t digits)
{
    int shift = int(ceil(log10(fabs(data))));
    int precision = int(digits - shift);
    if ( precision < 0 )
        precision = 0;
    if ( precision > 64 ) // limit precision of data
        precision = 64;

    char buffer[128];
    // ensure buffer is large enough to fit result
    // (additional bytes are for sign, dot and exponent)
    _ASSERT(sizeof(buffer) > size_t(precision + 16));
    int width = sprintf(buffer, "%.*f", precision, data);
    if ( width <= 0 || width >= int(sizeof(buffer) - 1) )
        ThrowError(eOverflow, "buffer overflow");
    _ASSERT(int(strlen(buffer)) == width);
    if ( precision != 0 ) { // skip trailing zeroes
        while ( buffer[width - 1] == '0' ) {
            --width;
        }
        if ( buffer[width - 1] == '.' )
            --width;
    }

    m_Output.PutString(buffer, width);
}

void CObjectOStreamXml::WriteDouble(double data)
{
    WriteDouble2(data, DBL_DIG);
}

void CObjectOStreamXml::WriteFloat(float data)
{
    WriteDouble2(data, FLT_DIG);
}

void CObjectOStreamXml::WriteNull(void)
{
    OpenTagEndBack();
    SelfCloseTagEnd();
}

void CObjectOStreamXml::WriteCString(const char* str)
{
    if ( str == 0 ) {
        OpenTagEndBack();
        SelfCloseTagEnd();
    }
    else {
		while ( *str ) {
			WriteEscapedChar(*str++);
		}
    }
}

void CObjectOStreamXml::WriteString(const string& str)
{
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        WriteEscapedChar(*i);
    }
}

void CObjectOStreamXml::WriteStringStore(const string& str)
{
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        WriteEscapedChar(*i);
    }
}

void CObjectOStreamXml::CopyString(CObjectIStream& in)
{
    string str;
    in.ReadStd(str);
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        WriteEscapedChar(*i);
    }
}

void CObjectOStreamXml::CopyStringStore(CObjectIStream& in)
{
    string str;
    in.ReadStringStore(str);
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        WriteEscapedChar(*i);
    }
}

void CObjectOStreamXml::WriteNullPointer(void)
{
    OpenTagEndBack();
    SelfCloseTagEnd();
}

void CObjectOStreamXml::WriteObjectReference(TObjectIndex index)
{
    m_Output.PutString("<object index=");
    if ( sizeof(TObjectIndex) == sizeof(Int4) )
        m_Output.PutInt4(Int4(index));
    else if ( sizeof(TObjectIndex) == sizeof(Int8) )
        m_Output.PutInt8(index);
    else
        ThrowError(eIllegalCall, "invalid size of TObjectIndex");
    m_Output.PutString("/>");
}

void CObjectOStreamXml::OpenTagStart(void)
{
    if ( m_LastTagAction == eTagOpen )
        m_Output.PutEol(false);
    m_Output.PutIndent();
    m_Output.PutChar('<');
    m_LastTagAction = eTagOpen;
}

void CObjectOStreamXml::OpenTagEnd(void)
{
    m_Output.PutChar('>');
    m_Output.IncIndentLevel();
}

void CObjectOStreamXml::OpenTagEndBack(void)
{
    _ASSERT(m_LastTagAction == eTagOpen);
    m_Output.BackChar('>');
    m_Output.DecIndentLevel();
}

void CObjectOStreamXml::SelfCloseTagEnd(void)
{
    _ASSERT(m_LastTagAction == eTagOpen);
    m_Output.PutString("/>");
    m_Output.PutEol(false);
    m_LastTagAction = eTagSelfClosed;
}

void CObjectOStreamXml::EolIfEmptyTag(void)
{
    _ASSERT(m_LastTagAction != eTagSelfClosed);
    if ( m_LastTagAction == eTagOpen ) {
        m_Output.PutEol(false);
        m_LastTagAction = eTagClose;
    }
}

void CObjectOStreamXml::CloseTagStart(void)
{
    _ASSERT(m_LastTagAction != eTagSelfClosed);
    m_Output.DecIndentLevel();
    if ( m_LastTagAction == eTagClose )
        m_Output.PutIndent();
    m_Output.PutString("</");
    m_LastTagAction = eTagClose;
}

void CObjectOStreamXml::CloseTagEnd(void)
{
    m_Output.PutChar('>');
    m_Output.PutEol(false);
}

void CObjectOStreamXml::PrintTagName(size_t level)
{
    const TFrame& frame = FetchFrameFromTop(level);
    switch ( frame.GetFrameType() ) {
    case TFrame::eFrameNamed:
    case TFrame::eFrameArray:
    case TFrame::eFrameClass:
    case TFrame::eFrameChoice:
        {
            _ASSERT(frame.GetTypeInfo());
            const string& name = frame.GetTypeInfo()->GetName();
            if ( !name.empty() )
                m_Output.PutString(name);
            else
                PrintTagName(level + 1);
            return;
        }
    case TFrame::eFrameClassMember:
    case TFrame::eFrameChoiceVariant:
        {
            PrintTagName(level + 1);
            m_Output.PutChar('_');
            m_Output.PutString(frame.GetMemberId().GetName());
            return;
        }
    case TFrame::eFrameArrayElement:
        {
            PrintTagName(level + 1);
            m_Output.PutString("_E");
            return;
        }
    default:
        break;
    }
    ThrowError(eIllegalCall, "illegal frame type");
}

void CObjectOStreamXml::WriteOtherBegin(TTypeInfo typeInfo)
{
    OpenTag(typeInfo);
}

void CObjectOStreamXml::WriteOtherEnd(TTypeInfo typeInfo)
{
    CloseTag(typeInfo);
}

void CObjectOStreamXml::WriteOther(TConstObjectPtr object,
                                   TTypeInfo typeInfo)
{
    OpenTag(typeInfo);
    WriteObject(object, typeInfo);
    CloseTag(typeInfo);
}

void CObjectOStreamXml::BeginContainer(const CContainerTypeInfo* containerType)
{
    OpenTagIfNamed(containerType);
}

void CObjectOStreamXml::EndContainer(void)
{
    CloseTagIfNamed(TopFrame().GetTypeInfo());
}

bool CObjectOStreamXml::WillHaveName(TTypeInfo elementType)
{
    while ( elementType->GetName().empty() ) {
        if ( elementType->GetTypeFamily() != eTypeFamilyPointer )
            return false;
        elementType = CTypeConverter<CPointerTypeInfo>::SafeCast(elementType)->GetPointedType();
    }
    // found named type
    return true;
}

void CObjectOStreamXml::BeginContainerElement(TTypeInfo elementType)
{
    if ( !WillHaveName(elementType) )
        OpenStackTag(0);
}

void CObjectOStreamXml::EndContainerElement(void)
{
    if ( !WillHaveName(TopFrame().GetTypeInfo()) )
        CloseStackTag(0);
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamXml::WriteContainer(const CContainerTypeInfo* cType,
                                       TConstObjectPtr containerPtr)
{
    if ( !cType->GetName().empty() ) {
        BEGIN_OBJECT_FRAME2(eFrameArray, cType);
        OpenTag(cType);

        WriteContainerContents(cType, containerPtr);

        EolIfEmptyTag();
        CloseTag(cType);
        END_OBJECT_FRAME();
    }
    else {
        WriteContainerContents(cType, containerPtr);
    }
}

void CObjectOStreamXml::WriteContainerContents(const CContainerTypeInfo* cType,
                                               TConstObjectPtr containerPtr)
{
    TTypeInfo elementType = cType->GetElementType();
    CContainerTypeInfo::CConstIterator i;
    if ( WillHaveName(elementType) ) {
        if ( cType->InitIterator(i, containerPtr) ) {
            do {

                WriteObject(cType->GetElementPtr(i), elementType);

            } while ( cType->NextElement(i) );
        }
    }
    else {
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        if ( cType->InitIterator(i, containerPtr) ) {
            do {
                OpenStackTag(0);

                WriteObject(cType->GetElementPtr(i), elementType);

                CloseStackTag(0);
            } while ( cType->NextElement(i) );
        }

        END_OBJECT_FRAME();
    }
}
#endif

void CObjectOStreamXml::BeginNamedType(TTypeInfo namedTypeInfo)
{
    OpenTag(namedTypeInfo);
}

void CObjectOStreamXml::EndNamedType(void)
{
    CloseTag(TopFrame().GetTypeInfo());
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamXml::WriteNamedType(TTypeInfo namedTypeInfo,
                                       TTypeInfo typeInfo,
                                       TConstObjectPtr object)
{
    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);
    OpenTag(namedTypeInfo);

    WriteObject(object, typeInfo);

    CloseTag(namedTypeInfo);
    END_OBJECT_FRAME();
}

void CObjectOStreamXml::CopyNamedType(TTypeInfo namedTypeInfo,
                                      TTypeInfo objectType,
                                      CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameNamed, namedTypeInfo);
    copier.In().BeginNamedType(namedTypeInfo);

    OpenTag(namedTypeInfo);

    CopyObject(objectType, copier);

    CloseTag(namedTypeInfo);

    copier.In().EndNamedType();
    END_OBJECT_2FRAMES_OF(copier);
}
#endif

void CObjectOStreamXml::BeginClass(const CClassTypeInfo* classInfo)
{
    OpenTagIfNamed(classInfo);
}

void CObjectOStreamXml::EndClass(void)
{
    EolIfEmptyTag();
    CloseTagIfNamed(TopFrame().GetTypeInfo());
}

void CObjectOStreamXml::BeginClassMember(const CMemberId& /*id*/)
{
    OpenStackTag(0);
}

void CObjectOStreamXml::EndClassMember(void)
{
    CloseStackTag(0);
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamXml::WriteClass(const CClassTypeInfo* classType,
                                   TConstObjectPtr classPtr)
{
    if ( !classType->GetName().empty() ) {
        BEGIN_OBJECT_FRAME2(eFrameClass, classType);
        OpenTag(classType);
        
        for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) {
            classType->GetMemberInfo(i)->WriteMember(*this, classPtr);
        }
        
        EolIfEmptyTag();
        CloseTag(classType);
        END_OBJECT_FRAME();
    }
    else {
        for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) {
            classType->GetMemberInfo(i)->WriteMember(*this, classPtr);
        }
    }
}

void CObjectOStreamXml::WriteClassMember(const CMemberId& memberId,
                                         TTypeInfo memberType,
                                         TConstObjectPtr memberPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
    OpenStackTag(0);
    
    WriteObject(memberPtr, memberType);
    
    CloseStackTag(0);
    END_OBJECT_FRAME();
}

bool CObjectOStreamXml::WriteClassMember(const CMemberId& memberId,
                                         const CDelayBuffer& buffer)
{
    if ( !buffer.HaveFormat(eSerial_Xml) )
        return false;

    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
    OpenStackTag(0);
    
    Write(buffer.GetSource());
    
    CloseStackTag(0);
    END_OBJECT_FRAME();

    return true;
}
#endif

void CObjectOStreamXml::BeginChoiceVariant(const CChoiceTypeInfo* choiceType,
                                           const CMemberId& /*id*/)
{
    OpenTagIfNamed(choiceType);
    OpenStackTag(0);
}

void CObjectOStreamXml::EndChoiceVariant(void)
{
    CloseStackTag(0);
    CloseTagIfNamed(FetchFrameFromTop(1).GetTypeInfo());
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamXml::WriteChoice(const CChoiceTypeInfo* choiceType,
                                    TConstObjectPtr choicePtr)
{
    if ( !choiceType->GetName().empty() ) {
        BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);
        OpenTag(choiceType);

        WriteChoiceContents(choiceType, choicePtr);

        CloseTag(choiceType);
        END_OBJECT_FRAME();
    }
    else {
        WriteChoiceContents(choiceType, choicePtr);
    }
}

void CObjectOStreamXml::WriteChoiceContents(const CChoiceTypeInfo* choiceType,
                                            TConstObjectPtr choicePtr)
{
    TMemberIndex index = choiceType->GetIndex(choicePtr);
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());
    OpenStackTag(0);
    
    variantInfo->WriteVariant(*this, choicePtr);
    
    CloseStackTag(0);
    END_OBJECT_FRAME();
}
#endif

static const char* const HEX = "0123456789ABCDEF";

void CObjectOStreamXml::WriteBytes(const ByteBlock& ,
                                   const char* bytes, size_t length)
{
	while ( length-- > 0 ) {
		char c = *bytes++;
		m_Output.PutChar(HEX[(c >> 4) & 0xf]);
        m_Output.PutChar(HEX[c & 0xf]);
	}
}

END_NCBI_SCOPE
