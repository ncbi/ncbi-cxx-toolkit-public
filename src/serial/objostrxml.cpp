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

void CObjectOStreamXml::WriteTypeName(const string& name)
{
    if ( m_Output.ZeroIndentLevel() ) {
        m_Output.PutString("<!DOCTYPE ");
        m_Output.PutString(name);
        m_Output.PutString(" SYSTEM \"ncbi.dtd\">");
    }
}

bool CObjectOStreamXml::WriteEnum(const CEnumeratedTypeValues& values,
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
        return true;
    }
    else {
        // local enum (member, variant or element)
        _ASSERT(m_LastTagAction == eTagOpen);
        if ( name.empty() ) {
            _ASSERT(values.IsInteger());
            return false;
        }
        m_Output.BackChar('>');
        m_Output.PutString(" value=\"");
        m_Output.PutString(name);
        if ( values.IsInteger() ) {
            m_Output.PutString("\">");
            return false;
        }
        else {
            m_LastTagAction = eTagSelfClosed;
            m_Output.PutString("\"/>");
            return true;
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

static
void Print(COStreamBuffer& out, const CObjectStackFrame& e)
{
    if ( e.GetNameType() == CObjectStackFrame::eNameTypeInfo ) {
        const string& name = e.GetNameTypeInfo()->GetName();
        if ( !name.empty() ) {
            out.PutString(name);
        }
        else {
            _ASSERT(e.GetPrevous());
            Print(out, *e.GetPrevous());
        }
    }
    else {
        _ASSERT(e.GetPrevous());
        Print(out, *e.GetPrevous());
        if ( e.HaveName() ) {
            out.PutChar('_');
            e.AppendTo(out);
        }
    }
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
void CObjectOStreamXml::OpenTag(const CObjectStackFrame& e)
{
    OpenTagStart();
    Print(m_Output, e);
    OpenTagEnd();
}

inline
void CObjectOStreamXml::CloseTag(const CObjectStackFrame& e,
                                 bool forceEolBefore)
{
    if ( CloseTagStart(forceEolBefore) ) {
        Print(m_Output, e);
        CloseTagEnd();
    }
}

void CObjectOStreamXml::WriteOther(TConstObjectPtr object,
                                   TTypeInfo typeInfo)
{
    const string& name = typeInfo->GetName();
    _ASSERT(!name.empty());
    OpenTag(name);
    WriteObject(object, typeInfo);
    CloseTag(name);
}

void CObjectOStreamXml::WriteContainer(const CConstObjectInfo& container,
                                       CWriteContainerElementsHook& hook)
{
    const string& arrayName = container.GetTypeInfo()->GetName();
    if ( arrayName.empty() ) {
        WriteContainerElements(container, hook);
    }
    else {
        CObjectStackArray array(*this, container.GetTypeInfo());
        OpenTag(arrayName);
        WriteContainerElements(container, hook);
        CloseTag(arrayName, true);
        array.End();
    }
}

void CObjectOStreamXml::WriteContainerElement(const CConstObjectInfo& element)
{
    const string& elementName = element.GetTypeInfo()->GetName();
    if ( elementName.empty() ) {
        CObjectStackArrayElement e(*this, false);
        OpenTag(e);
        
        WriteObject(element);
        
        CloseTag(e);
        e.End();
    }
    else {
        WriteObject(element);
    }
}

void CObjectOStreamXml::WriteNamedType(TTypeInfo namedTypeInfo,
                                       TTypeInfo typeInfo,
                                       TConstObjectPtr object)
{
    CObjectStackNamedFrame name(*this, namedTypeInfo);
    const string& typeName = namedTypeInfo->GetName();
    _ASSERT(!typeName.empty());
    OpenTag(typeName);
    WriteObject(object, typeInfo);
    CloseTag(typeName);
    name.End();
}

void CObjectOStreamXml::BeginClass(CObjectStackClass& /*cls*/,
                                   const CClassTypeInfo* classInfo)
{
    const string& name = classInfo->GetName();
    if ( !name.empty() )
        OpenTag(name);
}

void CObjectOStreamXml::EndClass(CObjectStackClass& cls)
{
    const string& name = cls.GetTypeInfo()->GetName();
    if ( !name.empty() )
        CloseTag(name, true);
    cls.End();
}

void CObjectOStreamXml::BeginClassMember(CObjectStackClassMember& m,
                                         const CMemberId& /*id*/)
{
    OpenTag(m);
}

void CObjectOStreamXml::EndClassMember(CObjectStackClassMember& m)
{
    CloseTag(m);
    m.End();
}

void CObjectOStreamXml::DoWriteClass(const CConstObjectInfo& object,
                                     CWriteClassMembersHook& hook)
{
    const string& className = object.GetTypeInfo()->GetName();
    if ( className.empty() ) {
        hook.WriteClassMembers(*this, object);
    }
    else {
        CObjectStackClass cls(*this, object.GetTypeInfo());
        OpenTag(className);
    
        hook.WriteClassMembers(*this, object);

        CloseTag(className, true);
        cls.End();
    }
}

void CObjectOStreamXml::DoWriteClass(TConstObjectPtr objectPtr,
                                     const CClassTypeInfo* objectType)
{
    const string& className = objectType->GetName();
    if ( className.empty() ) {
        WriteClassMembers(objectPtr, objectType);
    }
    else {
        CObjectStackClass cls(*this, objectType);
        OpenTag(className);
    
        WriteClassMembers(objectPtr, objectType);

        CloseTag(className, true);
        cls.End();
    }
}

void CObjectOStreamXml::DoWriteClassMember(const CMemberId& id,
                                           const CConstObjectInfo& object,
                                           TMemberIndex index,
                                           CWriteClassMemberHook& hook)
{
    CObjectStackClassMember m(*this, id);
    OpenTag(m);

    hook.WriteClassMember(*this, object, index);
    
    CloseTag(m);
    m.End();
}

void CObjectOStreamXml::DoWriteClassMember(const CMemberId& id,
                                           TConstObjectPtr memberPtr,
                                           TTypeInfo memberType)
{
    CObjectStackClassMember m(*this, id);
    OpenTag(m);

    WriteObject(memberPtr, memberType);
    
    CloseTag(m);
    m.End();
}

void CObjectOStreamXml::WriteChoice(const CConstObjectInfo& choice,
                                    CWriteChoiceVariantHook& hook)
{
    const string& choiceName = choice.GetTypeInfo()->GetName();
    if ( choiceName.empty() ) {
        WriteChoiceContents(choice, hook);
    }
    else {
        CObjectStackChoice c(*this, choice.GetTypeInfo());
        OpenTag(choiceName);
        WriteChoiceContents(choice, hook);
        CloseTag(choiceName);
        c.End();
    }
}

void CObjectOStreamXml::WriteChoice(const CConstObjectInfo& choice)
{
    const string& choiceName = choice.GetTypeInfo()->GetName();
    if ( choiceName.empty() ) {
        WriteChoiceContents(choice);
    }
    else {
        CObjectStackChoice c(*this, choice.GetTypeInfo());
        OpenTag(choiceName);
        WriteChoiceContents(choice);
        CloseTag(choiceName);
        c.End();
    }
}

void CObjectOStreamXml::WriteChoiceContents(const CConstObjectInfo& choice,
                                            CWriteChoiceVariantHook& hook)
{
    TMemberIndex index = choice.WhichChoice();
    const CMemberId& id = choice.GetMemberInfo(index)->GetId();
    CObjectStackChoiceVariant v(*this, id);
    OpenTag(v);
    
    hook.WriteChoiceVariant(*this, choice, index);
    
    CloseTag(v);
    v.End();
}

void CObjectOStreamXml::WriteChoiceContents(const CConstObjectInfo& choice)
{
    TMemberIndex index = choice.WhichChoice();
    const CMemberId& id = choice.GetMemberInfo(index)->GetId();
    CObjectStackChoiceVariant v(*this, id);
    OpenTag(v);
    
    WriteChoiceVariant(choice, index);
    
    CloseTag(v);
    v.End();
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
