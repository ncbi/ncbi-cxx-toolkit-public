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
* Revision 1.5  2001/08/31 20:05:45  ucko
* Fix ICC build.
*
* Revision 1.4  2001/05/17 15:07:07  lavr
* Typos corrected
*
* Revision 1.3  2001/01/22 23:23:58  vakatov
* Added   CIStreamClassMemberIterator
* Renamed CIStreamContainer --> CIStreamContainerIterator
*
* Revision 1.2  2000/10/20 19:29:36  vasilche
* Adapted for MSVC which doesn't like explicit operator templates.
*
* Revision 1.1  2000/10/20 15:51:40  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
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

#ifdef NCBI_COMPILER_ICC
void* COStreamFrame::operator new(size_t size)
{
    return ::operator new(size);
}

void* COStreamFrame::operator new[](size_t size)
{
    return ::operator new[](size);
}

void* CIStreamFrame::operator new(size_t size)
{
    return ::operator new(size);
}

void* CIStreamFrame::operator new[](size_t size)
{
    return ::operator new[](size);
}
#endif


/////////////////////////////////////////////////////////////////////////////
// read/write classMember

inline
const CMemberInfo* CIStreamClassMemberIterator::GetMemberInfo(void) const
{
    return m_ClassType.GetClassTypeInfo()->GetMemberInfo(m_MemberIndex);
}

inline
void CIStreamClassMemberIterator::BeginClassMember(void)
{
    if ( m_ClassType.GetClassTypeInfo()->RandomOrder() ) {
        m_MemberIndex =
            GetStream().BeginClassMember(m_ClassType.GetClassTypeInfo());
    } else {
        m_MemberIndex =
            GetStream().BeginClassMember(m_ClassType.GetClassTypeInfo(),
                                         m_MemberIndex + 1);
    }

    if ( *this )
        GetStream().SetTopMemberId(GetMemberInfo()->GetId());
}

inline
void CIStreamClassMemberIterator::IllegalCall(const char* message) const
{
    GetStream().ThrowError(CObjectIStream::eIllegalCall, message);
}

inline
void CIStreamClassMemberIterator::BadState(void) const
{
    IllegalCall("bad CIStreamClassMemberIterator state");
}

CIStreamClassMemberIterator::CIStreamClassMemberIterator(CObjectIStream& in,
                                     const CObjectTypeInfo& classType)
    : CParent(in), m_ClassType(classType)
{
    const CClassTypeInfo* classTypeInfo = classType.GetClassTypeInfo();
    in.PushFrame(CObjectStackFrame::eFrameClass, classTypeInfo);
    in.BeginClass(classTypeInfo);
    
    in.PushFrame(CObjectStackFrame::eFrameClassMember);
    m_MemberIndex = kFirstMemberIndex - 1;
    BeginClassMember();
}

CIStreamClassMemberIterator::~CIStreamClassMemberIterator(void)
{
    if ( Good() ) {
        if ( *this )
            GetStream().EndClassMember();
        GetStream().PopFrame();
        GetStream().EndClass();
        GetStream().PopFrame();
    }
}

inline
void CIStreamClassMemberIterator::CheckState(void)
{
    if ( m_MemberIndex == kInvalidMember )
        BadState();
}

void CIStreamClassMemberIterator::NextClassMember(void)
{
    CheckState();
    GetStream().EndClassMember();
    BeginClassMember();
}

void CIStreamClassMemberIterator::ReadClassMember(const CObjectInfo& member)
{
    CheckState();
    GetStream().ReadSeparateObject(member);
}

void CIStreamClassMemberIterator::SkipClassMember(const CObjectTypeInfo& member)
{
    CheckState();
    GetStream().SkipObject(member.GetTypeInfo());
}

void CIStreamClassMemberIterator::SkipClassMember(void)
{
    CheckState();
    GetStream().SkipObject(GetMemberInfo()->GetTypeInfo());
}


/////////////////////////////////////////////////////////////////////////////
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
void CIStreamContainerIterator::BeginElement(void)
{
    _ASSERT(m_State == eError);
    if ( GetStream().BeginContainerElement(m_ElementTypeInfo) )
        m_State = eElementBegin;
    else
        m_State = eNoMoreElements;
}

inline
void CIStreamContainerIterator::IllegalCall(const char* message) const
{
    GetStream().ThrowError(CObjectIStream::eIllegalCall, message);
}

inline
void CIStreamContainerIterator::BadState(void) const
{
    IllegalCall("bad CIStreamContainerIterator state");
}

CIStreamContainerIterator::CIStreamContainerIterator(CObjectIStream& in,
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

CIStreamContainerIterator::~CIStreamContainerIterator(void)
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
void CIStreamContainerIterator::CheckState(EState state)
{
    bool ok = (m_State == state);
    m_State = eError;
    if ( !ok )
        BadState();
}

void CIStreamContainerIterator::NextElement(void)
{
    CheckState(eElementEnd);
    GetStream().EndContainerElement();
    BeginElement();
}

inline
void CIStreamContainerIterator::BeginElementData(void)
{
    CheckState(eElementBegin);
}

inline
void CIStreamContainerIterator::BeginElementData(const CObjectTypeInfo& )
{
    //if ( elementType.GetTypeInfo() != GetElementTypeInfo() )
    //    IllegalCall("wrong element type");
    BeginElementData();
}

void CIStreamContainerIterator::ReadElement(const CObjectInfo& element)
{
    BeginElementData(element);
    GetStream().ReadSeparateObject(element);
    m_State = eElementEnd;
}

void CIStreamContainerIterator::SkipElement(const CObjectTypeInfo& elementType)
{
    BeginElementData(elementType);
    GetStream().SkipObject(elementType.GetTypeInfo());
    m_State = eElementEnd;
}

void CIStreamContainerIterator::SkipElement(void)
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
