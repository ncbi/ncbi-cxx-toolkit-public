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
#include <serial/memberid.hpp>
#include <serial/memberlist.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objhook.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/continfo.hpp>

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

void CObjectOStreamXml::WriteTypeName(TTypeInfo type)
{
    if ( true || m_Output.ZeroIndentLevel() ) {
        m_Output.PutString("<!DOCTYPE ");
        m_Output.PutString(type->GetName());
        m_Output.PutString(" SYSTEM \"ncbi.dtd\">");
    }
}

void CObjectOStreamXml::WriteEnum(const CEnumeratedTypeValues& values,
                                  long value)
{
    const string& name = values.FindName(value, values.IsInteger());
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

void CObjectOStreamXml::WriteString(const string& str)
{
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        WriteEscapedChar(*i);
    }
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

inline
void CObjectOStreamXml::OpenTagEnd(void)
{
    m_Output.PutChar('>');
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

inline
void CObjectOStreamXml::CloseTagEnd(void)
{
    m_Output.PutChar('>');
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

void CObjectOStreamXml::OpenTag(const string& name)
{
    OpenTagStart();
    m_Output.PutString(name);
    OpenTagEnd();
}

void CObjectOStreamXml::CloseTag(const string& name, bool forceEolBefore)
{
    if ( CloseTagStart(forceEolBefore) ) {
        m_Output.PutString(name);
        CloseTagEnd();
    }
}

inline
void CObjectOStreamXml::OpenTag(size_t level)
{
    OpenTagStart();
    PrintTagName(level);
    OpenTagEnd();
}

inline
void CObjectOStreamXml::CloseTag(size_t level,
                                 bool forceEolBefore)
{
    if ( CloseTagStart(forceEolBefore) ) {
        PrintTagName(level);
        CloseTagEnd();
    }
}

void CObjectOStreamXml::WriteOtherBegin(TTypeInfo typeInfo)
{
    _ASSERT(!typeInfo->GetName().empty());
    OpenTag(typeInfo->GetName());
}

void CObjectOStreamXml::WriteOtherEnd(TTypeInfo typeInfo)
{
    CloseTag(typeInfo->GetName());
}

void CObjectOStreamXml::WriteOther(TConstObjectPtr object,
                                   TTypeInfo typeInfo)
{
    _ASSERT(!typeInfo->GetName().empty());
    OpenTag(typeInfo->GetName());
    WriteObject(object, typeInfo);
    CloseTag(typeInfo->GetName());
}

void CObjectOStreamXml::BeginContainer(const CContainerTypeInfo* containerType)
{
    if ( !containerType->GetName().empty() )
        OpenTag(containerType->GetName());
}

void CObjectOStreamXml::EndContainer(void)
{
    const string& containerName = TopFrame().GetTypeInfo()->GetName();
    if ( !containerName.empty() )
        CloseTag(containerName);
}

void CObjectOStreamXml::BeginContainerElement(TTypeInfo elementType)
{
    if ( elementType->GetName().empty() )
        OpenTag(0);
}

void CObjectOStreamXml::EndContainerElement(void)
{
    if ( TopFrame().GetTypeInfo()->GetName().empty() )
        CloseTag(0);
}

void CObjectOStreamXml::WriteContainer(TConstObjectPtr containerPtr,
                                       const CContainerTypeInfo* containerType)
{
    if ( !containerType->GetName().empty() ) {
        BEGIN_OBJECT_FRAME2(eFrameArray, containerType);
        OpenTag(containerType->GetName());

        WriteContainerContents(containerPtr, containerType);

        CloseTag(containerType->GetName(), true);
        END_OBJECT_FRAME();
    }
    else {
        WriteContainerContents(containerPtr, containerType);
    }
}

void CObjectOStreamXml::WriteContainerContents(TConstObjectPtr containerPtr,
                                               const CContainerTypeInfo* containerType)
{
    TTypeInfo elementType = containerType->GetElementType();
    auto_ptr<CContainerTypeInfo::CConstIterator> i(containerType->NewConstIterator());
    if ( !elementType->GetName().empty() ) {
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
                OpenTag(0);
                WriteObject(i->GetElementPtr(), elementType);
                CloseTag(0);
            } while ( i->Next() );
        }

        END_OBJECT_FRAME();
    }
}                                               

void CObjectOStreamXml::WriteContainer(const CConstObjectInfo& container,
                                       CWriteContainerElementsHook& hook)
{
    if ( !container.GetTypeInfo()->GetName().empty() ) {
        BEGIN_OBJECT_FRAME2(eFrameArray, container.GetTypeInfo());
        OpenTag(container.GetTypeInfo()->GetName());

        WriteContainerElements(container, hook);

        CloseTag(container.GetTypeInfo()->GetName(), true);
        END_OBJECT_FRAME();
    }
    else {
        WriteContainerElements(container, hook);
    }
}

void CObjectOStreamXml::WriteContainerElement(const CConstObjectInfo& element)
{
    if ( !element.GetTypeInfo()->GetName().empty() ) {
        WriteObject(element);
    }
    else {
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, element.GetTypeInfo());
        OpenTag(0);
        
        WriteObject(element);
        
        CloseTag(0);
        END_OBJECT_FRAME();
    }
}

void CObjectOStreamXml::BeginNamedType(TTypeInfo namedTypeInfo)
{
    _ASSERT(!namedTypeInfo->GetName().empty());
    OpenTag(namedTypeInfo->GetName());
}

void CObjectOStreamXml::EndNamedType(void)
{
    _ASSERT(!TopFrame().GetTypeInfo()->GetName().empty());
    CloseTag(TopFrame().GetTypeInfo()->GetName());
}

void CObjectOStreamXml::WriteNamedType(TTypeInfo namedTypeInfo,
                                       TTypeInfo typeInfo,
                                       TConstObjectPtr object)
{
    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);
    _ASSERT(!namedTypeInfo->GetName().empty());
    OpenTag(namedTypeInfo->GetName());

    WriteObject(object, typeInfo);

    CloseTag(namedTypeInfo->GetName());
    END_OBJECT_FRAME();
}

void CObjectOStreamXml::BeginClass(const CClassTypeInfo* classInfo)
{
    if ( !classInfo->GetName().empty() )
        OpenTag(classInfo->GetName());
}

void CObjectOStreamXml::EndClass(void)
{
    TTypeInfo classType = TopFrame().GetTypeInfo();
    if ( !classType->GetName().empty() )
        CloseTag(classType->GetName(), true);
}

void CObjectOStreamXml::BeginClassMember(const CMemberId& /*id*/)
{
    OpenTag(0);
}

void CObjectOStreamXml::EndClassMember(void)
{
    CloseTag(0);
}

void CObjectOStreamXml::DoWriteClass(const CConstObjectInfo& object,
                                     CWriteClassMembersHook& hook)
{
    TTypeInfo objectType = object.GetTypeInfo();
    if ( !objectType->GetName().empty() ) {
        BEGIN_OBJECT_FRAME2(eFrameClass, objectType);
        OpenTag(objectType->GetName());
        
        hook.WriteClassMembers(*this, object);
        
        CloseTag(objectType->GetName(), true);
        END_OBJECT_FRAME();
    }
    else {
        hook.WriteClassMembers(*this, object);
    }
}

void CObjectOStreamXml::DoWriteClass(TConstObjectPtr objectPtr,
                                     const CClassTypeInfo* objectType)
{
    if ( !objectType->GetName().empty() ) {
        BEGIN_OBJECT_FRAME2(eFrameClass, objectType);
        OpenTag(objectType->GetName());
        
        WriteClassMembers(objectPtr, objectType);
        
        CloseTag(objectType->GetName(), true);
        END_OBJECT_FRAME();
    }
    else {
        WriteClassMembers(objectPtr, objectType);
    }
}

void CObjectOStreamXml::DoWriteClassMember(const CMemberId& id,
                                           const CConstObjectInfo& object,
                                           TMemberIndex index,
                                           CWriteClassMemberHook& hook)
{
    BEGIN_OBJECT_FRAME2(eFrameClassMember, id);
    OpenTag(0);
    
    hook.WriteClassMember(*this, object, index);
    
    CloseTag(0);
    END_OBJECT_FRAME();
}

void CObjectOStreamXml::DoWriteClassMember(const CMemberId& id,
                                           TConstObjectPtr memberPtr,
                                           TTypeInfo memberType)
{
    BEGIN_OBJECT_FRAME2(eFrameClassMember, id);
    OpenTag(0);
    
    WriteObject(memberPtr, memberType);
    
    CloseTag(0);
    END_OBJECT_FRAME();
}

void CObjectOStreamXml::BeginChoiceVariant(const CChoiceTypeInfo* choiceType,
                                           const CMemberId& id)
{
    const string& choiceName = choiceType->GetName();
    if ( !choiceName.empty() )
        OpenTag(choiceName);
    OpenTag(0);
}

void CObjectOStreamXml::EndChoiceVariant(void)
{
    CloseTag(0);
    const string& choiceName = FetchFrameFromTop(1).GetTypeInfo()->GetName();
    if ( !choiceName.empty() )
        CloseTag(choiceName);
}

void CObjectOStreamXml::WriteChoice(const CConstObjectInfo& choice,
                                    CWriteChoiceVariantHook& hook)
{
    TTypeInfo choiceType = choice.GetTypeInfo();
    if ( !choiceType->GetName().empty() ) {
        BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);
        OpenTag(choiceType->GetName());

        WriteChoiceContents(choice, hook);

        CloseTag(choiceType->GetName());
        END_OBJECT_FRAME();
    }
    else {
        WriteChoiceContents(choice, hook);
    }
}

void CObjectOStreamXml::WriteChoice(const CConstObjectInfo& choice)
{
    TTypeInfo choiceType = choice.GetTypeInfo();
    if ( !choiceType->GetName().empty() ) {
        BEGIN_OBJECT_FRAME2(eFrameChoice, choiceType);
        OpenTag(choiceType->GetName());

        WriteChoiceContents(choice);

        CloseTag(choiceType->GetName());
        END_OBJECT_FRAME();
    }
    else {
        WriteChoiceContents(choice);
    }
}

void CObjectOStreamXml::WriteChoiceContents(const CConstObjectInfo& choice,
                                            CWriteChoiceVariantHook& hook)
{
    CConstObjectInfo::CChoiceVariant v = choice.GetCurrentChoiceVariant();
    const CMemberId& id = v.GetVariantInfo()->GetId();
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, id);
    OpenTag(0);
    
    hook.WriteChoiceVariant(*this, choice, v.GetVariantIndex());
    
    CloseTag(0);
    END_OBJECT_FRAME();
}

void CObjectOStreamXml::WriteChoiceContents(const CConstObjectInfo& choice)
{
    CConstObjectInfo::CChoiceVariant v = choice.GetCurrentChoiceVariant();
    const CMemberId& id = v.GetVariantInfo()->GetId();
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, id);
    OpenTag(0);
    
    WriteChoiceVariant(choice, v.GetVariantIndex());
    
    CloseTag(0);
    END_OBJECT_FRAME();
}

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
