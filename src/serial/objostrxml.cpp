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

BEGIN_NCBI_SCOPE

CObjectOStream* OpenObjectOStreamXml(CNcbiOstream& out, bool deleteOut)
{
    return new CObjectOStreamXml(out, deleteOut);
}

CObjectOStreamXml::CObjectOStreamXml(CNcbiOstream& out, bool deleteOut)
    : CObjectOStream(out, deleteOut), m_LastTagAction(eTagClose)
{
    m_Output.SetBackLimit(1);
    m_Output.PutString("<?xml version=\"1.0\"?>\n");
}

CObjectOStreamXml::~CObjectOStreamXml(void)
{
}

ESerialDataFormat CObjectOStreamXml::GetDataFormat(void) const
{
    return eSerial_Xml;
}

void CObjectOStreamXml::WriteFileHeader(TTypeInfo type)
{
    if ( true || m_Output.ZeroIndentLevel() ) {
        m_Output.PutString("<!DOCTYPE ");
        m_Output.PutString(type->GetName());
        m_Output.PutString(" SYSTEM \"ncbi.dtd\">");
    }
}

void CObjectOStreamXml::WriteEnum(const CEnumeratedTypeValues& values,
                                  long value, const string& name)
{
    if ( !values.GetName().empty() ) {
        // global enum
        m_Output.PutEol();
        m_Output.PutChar('<');
        m_Output.PutString(values.GetName());
        if ( !name.empty() ) {
            m_Output.PutString(" value=\"");
            m_Output.PutString(name);
            m_Output.PutChar('\"');
        }
        if ( values.IsInteger() ) {
            m_Output.PutChar('>');
            m_Output.PutLong(value);
            m_Output.PutString("</");
            m_Output.PutString(values.GetName());
            m_Output.PutChar('>');
            m_LastTagAction = eTagClose;
        }
        else {
            _ASSERT(!name.empty());
            m_Output.PutString("/>");
            m_LastTagAction = eTagClose;
        }
    }
    else {
        // local enum (member, variant or element)
        _ASSERT(m_LastTagAction == eTagOpen);
        if ( name.empty() ) {
            _ASSERT(values.IsInteger());
            m_Output.PutLong(value);
        }
        else {
            m_Output.BackChar('>');
            m_Output.PutString(" value=\"");
            m_Output.PutString(name);
            if ( values.IsInteger() ) {
                m_Output.PutString("\">");
                m_Output.PutLong(value);
            }
            else {
                m_LastTagAction = eTagSelfClosed;
                m_Output.PutString("\"/>");
            }
        }
    }
}

void CObjectOStreamXml::WriteEnum(const CEnumeratedTypeValues& values,
                                  long value)
{
    WriteEnum(values, value, values.FindName(value, values.IsInteger()));
}

void CObjectOStreamXml::CopyEnum(const CEnumeratedTypeValues& values,
                                 CObjectIStream& in)
{
    long value = in.ReadEnum(values);
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
    m_Output.BackChar('>');
    if ( data )
        m_Output.PutString(" value=\"true\"/>");
    else
        m_Output.PutString(" value=\"false\"/>");
    m_LastTagAction = eTagSelfClosed;
}

void CObjectOStreamXml::WriteChar(char data)
{
    WriteEscapedChar(data);
}

void CObjectOStreamXml::WriteInt(int data)
{
    m_Output.PutInt(data);
}

void CObjectOStreamXml::WriteUInt(unsigned data)
{
    m_Output.PutUInt(data);
}

void CObjectOStreamXml::WriteLong(long data)
{
    m_Output.PutLong(data);
}

void CObjectOStreamXml::WriteULong(unsigned long data)
{
    m_Output.PutULong(data);
}

void CObjectOStreamXml::WriteDouble(double data)
{
    CNcbiOstrstream buff;
    buff << IO_PREFIX::setprecision(15) << data;
    size_t length = buff.pcount();
    const char* str = buff.str();    buff.freeze(false);
    m_Output.PutString(str, length);
}

void CObjectOStreamXml::WriteNull(void)
{
    m_Output.PutString("null");
}

void CObjectOStreamXml::WriteCString(const char* str)
{
    if ( str == 0 ) {
        WriteNull();
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
    WriteNull();
}

void CObjectOStreamXml::WriteObjectReference(TObjectIndex index)
{
    m_Output.PutString("<object index=");
    m_Output.PutInt(index);
    m_Output.PutString("/>");
}

void CObjectOStreamXml::OpenTagStart(void)
{
    m_Output.PutEol();
    m_Output.PutChar('<');
    m_Output.IncIndentLevel();
    m_LastTagAction = eTagOpen;
}

bool CObjectOStreamXml::CloseTagStart(bool forceEolBefore)
{
    m_Output.DecIndentLevel();
    switch ( m_LastTagAction ) {
    case eTagSelfClosed:
        m_LastTagAction = eTagClose;
        return false;
    case eTagOpen:
        if ( forceEolBefore )
            m_Output.PutEol();
        break;
    default:
        m_Output.PutEol();
        break;
    }        
    m_Output.PutString("</");
    m_LastTagAction = eTagClose;
    return true;
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
    THROW1_TRACE(runtime_error, "illegal frame type");
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

        CloseTag(cType, true);
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
    auto_ptr<CContainerTypeInfo::CConstIterator> i(cType->NewConstIterator());
    if ( WillHaveName(elementType) ) {
        if ( i->Init(containerPtr) ) {
            do {
                WriteObject(i->GetElementPtr(), elementType);
            } while ( i->Next() );
        }
    }
    else {
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        if ( i->Init(containerPtr) ) {
            do {
                OpenStackTag(0);
                WriteObject(i->GetElementPtr(), elementType);
                CloseStackTag(0);
            } while ( i->Next() );
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
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameNamed, namedTypeInfo);
    copier.In().BeginNamedType(namedTypeInfo);

    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);
    OpenTag(namedTypeInfo);

    CopyObject(objectType, copier);

    CloseTag(namedTypeInfo);
    END_OBJECT_FRAME();

    copier.In().EndNamedType();
    END_OBJECT_FRAME_OF(copier.In());
}
#endif

void CObjectOStreamXml::BeginClass(const CClassTypeInfo* classInfo)
{
    OpenTagIfNamed(classInfo);
}

void CObjectOStreamXml::EndClass(void)
{
    CloseTagIfNamed(TopFrame().GetTypeInfo(), true);
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
        
        CloseTag(classType, true);
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

void CObjectOStreamXml::BeginBytes(const ByteBlock& )
{
    m_Output.PutChar('\'');
}

void CObjectOStreamXml::WriteBytes(const ByteBlock& ,
                                   const char* bytes, size_t length)
{
	while ( length-- > 0 ) {
		char c = *bytes++;
		m_Output.PutChar(HEX[(c >> 4) & 0xf]);
        m_Output.PutChar(HEX[c & 0xf]);
	}
}

void CObjectOStreamXml::EndBytes(const ByteBlock& )
{
    m_Output.PutString("\'H");
}

END_NCBI_SCOPE
