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
* Revision 1.1  2000/10/20 15:51:40  vasilche
* Fixed data error processing.
* Added interface for costructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objectio.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/objhook.hpp>

BEGIN_NCBI_SCOPE

void operator<<(CObjectOStream& out, const CConstObjectInfoMI& member)
{
    const CMemberInfo* memberInfo = member.GetMemberInfo();
    TConstObjectPtr classPtr = member.GetClassObject().GetObjectPtr();
    out.WriteClassMember(memberInfo->GetId(),
                         memberInfo->GetTypeInfo(),
                         memberInfo->GetMemberPtr(classPtr));
}

void operator>>(CObjectIStream& in, const CObjectInfoMI& member)
{
    const CMemberInfo* memberInfo = member.GetMemberInfo();
    TObjectPtr classPtr = member.GetClassObject().GetObjectPtr();
    in.ReadObject(memberInfo->GetMemberPtr(classPtr),
                  memberInfo->GetTypeInfo());
}

// readers
void CObjectInfo::ReadContainer(CObjectIStream& in,
                                CReadContainerElementHook& hook)
{
    const CContainerTypeInfo* containerType = GetContainerTypeInfo();
    BEGIN_OBJECT_FRAME_OF2(in, eFrameArray, containerType);
    in.BeginContainer(containerType);

    TTypeInfo elementType = containerType->GetElementType();
    BEGIN_OBJECT_FRAME_OF2(in, eFrameArrayElement, elementType);

    while ( in.BeginContainerElement(elementType) ) {
        hook.ReadContainerElement(in, *this);
        in.EndContainerElement();
    }

    END_OBJECT_FRAME_OF(in);

    in.EndContainer();
    END_OBJECT_FRAME_OF(in);
}

inline
CIStreamFrame::CIStreamFrame(CObjectIStream& stream)
    : m_Stream(stream), m_Depth(stream.GetStackDepth())
{
}

CIStreamFrame::~CIStreamFrame(void)
{
    if ( GetStream().GetStackDepth() != m_Depth )
        GetStream().PopErrorFrame();
}

inline
bool CIStreamFrame::Good(void) const
{
    return GetStream().InGoodState();
}

inline
COStreamFrame::COStreamFrame(CObjectOStream& stream)
    : m_Stream(stream), m_Depth(stream.GetStackDepth())
{
}

inline
bool COStreamFrame::Good(void) const
{
    return GetStream().InGoodState();
}

COStreamFrame::~COStreamFrame(void)
{
    if ( GetStream().GetStackDepth() != m_Depth )
        GetStream().PopErrorFrame();
}

// read/write class member

COStreamClassMember::COStreamClassMember(CObjectOStream& out,
                                         const CObjectTypeInfoMI& member)
    : CParent(out)
{
    const CMemberInfo* memberInfo = member.GetMemberInfo();
    out.PushFrame(CObjectStackFrame::eFrameClassMember, memberInfo->GetId());
    out.BeginClassMember(memberInfo->GetId());
}

COStreamClassMember::~COStreamClassMember(void)
{
    if ( Good() ) {
        GetStream().EndClassMember();
        GetStream().PopFrame();
    }
}

// read/write container
inline
void CIStreamContainer::BeginElement(void)
{
    _ASSERT(m_State == eError);
    if ( GetStream().BeginContainerElement(m_ElementTypeInfo) )
        m_State = eElementBegin;
    else
        m_State = eNoMoreElements;
}

inline
void CIStreamContainer::IllegalCall(const char* message) const
{
    GetStream().ThrowError(CObjectIStream::eIllegalCall, message);
}

inline
void CIStreamContainer::BadState(void) const
{
    IllegalCall("bad CIStreamContainer state");
}

CIStreamContainer::CIStreamContainer(CObjectIStream& in,
                                     const CObjectTypeInfo& containerType)
    : CParent(in), m_ContainerType(containerType), m_State(eError)
{
    const CContainerTypeInfo* containerTypeInfo =
        GetContainerType().GetContainerTypeInfo();
    in.PushFrame(CObjectStackFrame::eFrameArray, containerTypeInfo);
    in.BeginContainer(containerTypeInfo);
    
    TTypeInfo elementTypeInfo = m_ElementTypeInfo =
        containerTypeInfo->GetElementType();
    in.PushFrame(CObjectStackFrame::eFrameArrayElement, elementTypeInfo);
    BeginElement();
}

CIStreamContainer::~CIStreamContainer(void)
{
    if ( Good() ) {
        switch ( m_State ) {
        case eElementBegin:
            // not read element
            m_State = eError;
            BadState();
            return;
        case eElementEnd:
            NextElement();
            if ( m_State != eNoMoreElements )
                IllegalCall("not all elements read");
            break;
        case eNoMoreElements:
            break;
        default:
            // error -> do nothing
            return;
        }
        GetStream().PopFrame();
        GetStream().EndContainer();
        GetStream().PopFrame();
    }
}

inline
void CIStreamContainer::CheckState(EState state)
{
    bool ok = (m_State == state);
    m_State = eError;
    if ( !ok )
        BadState();
}

void CIStreamContainer::NextElement(void)
{
    CheckState(eElementEnd);
    GetStream().EndContainerElement();
    BeginElement();
}

inline
void CIStreamContainer::BeginElementData(void)
{
    CheckState(eElementBegin);
}

inline
void CIStreamContainer::BeginElementData(const CObjectTypeInfo& )
{
    //if ( elementType.GetTypeInfo() != GetElementTypeInfo() )
    //    IllegalCall("wrong element type");
    BeginElementData();
}

void CIStreamContainer::ReadElement(const CObjectInfo& element)
{
    BeginElementData(element);
    GetStream().ReadSeparateObject(element);
    m_State = eElementEnd;
}

void CIStreamContainer::SkipElement(const CObjectTypeInfo& elementType)
{
    BeginElementData(elementType);
    GetStream().SkipObject(elementType.GetTypeInfo());
    m_State = eElementEnd;
}

void CIStreamContainer::SkipElement(void)
{
    BeginElementData();
    GetStream().SkipObject(m_ElementTypeInfo);
    m_State = eElementEnd;
}

COStreamContainer::COStreamContainer(CObjectOStream& out,
                                     const CObjectTypeInfo& containerType)
    : CParent(out), m_ContainerType(containerType)
{
    const CContainerTypeInfo* containerTypeInfo = 
        GetContainerType().GetContainerTypeInfo();
    out.PushFrame(CObjectStackFrame::eFrameArray, containerTypeInfo);
    out.BeginContainer(containerTypeInfo);

    TTypeInfo elementTypeInfo = m_ElementTypeInfo =
        containerTypeInfo->GetElementType();
    out.PushFrame(CObjectStackFrame::eFrameArrayElement, elementTypeInfo);
}

COStreamContainer::~COStreamContainer(void)
{
    if ( Good() ) {
        GetStream().PopFrame();
        GetStream().EndContainer();
        GetStream().PopFrame();
    }
}

void COStreamContainer::WriteElement(const CConstObjectInfo& element)
{
    GetStream().BeginContainerElement(m_ElementTypeInfo);

    GetStream().WriteSeparateObject(element);

    GetStream().EndContainerElement();
}

END_NCBI_SCOPE
