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
*/

#include <ncbi_pch.hpp>
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
    if ( GetStream().GetStackDepth() != m_Depth ) {
        try {
            GetStream().PopErrorFrame();
        }
        catch (...) {
            GetStream().SetFailFlags(CObjectIStream::fIllegalCall,
                "object stack frame error");
        }
    }
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
    if ( GetStream().GetStackDepth() != m_Depth ) {
        try {
            GetStream().PopErrorFrame();
        }
        catch (...) {
            GetStream().SetFailFlags(CObjectOStream::fIllegalCall,
                "object stack frame error");
        }
    }
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
    GetStream().ThrowError(CObjectIStream::fIllegalCall, message);
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
        try {
            if ( *this )
                GetStream().EndClassMember();
            GetStream().PopFrame();
            GetStream().EndClass();
            GetStream().PopFrame();
        }
        catch (...) {
            GetStream().SetFailFlags(CObjectIStream::fIllegalCall,
                "class member iterator error");
        }
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
        try {
            GetStream().EndClassMember();
            GetStream().PopFrame();
        }
        catch (...) {
            GetStream().SetFailFlags(CObjectIStream::fIllegalCall,
                "class member write error");
        }
    }
}

// read/write container
inline
void CIStreamContainerIterator::BeginElement(void)
{
    _ASSERT(m_State == eElementEnd);
    if ( GetStream().BeginContainerElement(m_ElementTypeInfo) )
        m_State = eElementBegin;
    else
        m_State = eNoMoreElements;
}

inline
void CIStreamContainerIterator::IllegalCall(const char* message) const
{
    GetStream().ThrowError(CObjectIStream::fIllegalCall, message);
    // GetStream().SetFailFlags(CObjectIStream::fIllegalCall, message);
}

inline
void CIStreamContainerIterator::BadState(void) const
{
    IllegalCall("bad CIStreamContainerIterator state");
}

CIStreamContainerIterator::CIStreamContainerIterator(CObjectIStream& in,
                                     const CObjectTypeInfo& containerType)
    : CParent(in), m_ContainerType(containerType), m_State(eElementEnd)
{
    const CContainerTypeInfo* containerTypeInfo;
    if (m_ContainerType.GetTypeFamily() == eTypeFamilyClass) {
        const CClassTypeInfo* classType =
            CTypeConverter<CClassTypeInfo>::SafeCast(m_ContainerType.GetTypeInfo());
        const CItemInfo* itemInfo =
            classType->GetItems().GetItemInfo(classType->GetItems().FirstIndex());
        _ASSERT(itemInfo->GetTypeInfo()->GetTypeFamily() == eTypeFamilyContainer);
        containerTypeInfo =
            CTypeConverter<CContainerTypeInfo>::SafeCast(itemInfo->GetTypeInfo());
        in.PushFrame(CObjectStackFrame::eFrameNamed, m_ContainerType.GetTypeInfo());
        in.BeginNamedType(m_ContainerType.GetTypeInfo());
    } else {
        containerTypeInfo = GetContainerType().GetContainerTypeInfo();
    }

    in.PushFrame(CObjectStackFrame::eFrameArray, containerTypeInfo);
    in.BeginContainer(containerTypeInfo);
    
    TTypeInfo elementTypeInfo = m_ElementTypeInfo =
        containerTypeInfo->GetElementType();
    in.PushFrame(CObjectStackFrame::eFrameArrayElement, elementTypeInfo);
    BeginElement();
    if ( m_State == eNoMoreElements ) {
        in.PopFrame();
        in.EndContainer();
        in.PopFrame();
        if (m_ContainerType.GetTypeFamily() == eTypeFamilyClass) {
            in.EndNamedType();
            in.PopFrame();
        }
    }
}

CIStreamContainerIterator::~CIStreamContainerIterator(void)
{
    if ( Good() ) {
        switch ( m_State ) {
        case eNoMoreElements:
            // normal state
            return;
        case eElementBegin:
        case eElementEnd:
            // not read element(s)
            m_State = eError;
            GetStream().SetFailFlags(CObjectIStream::fIllegalCall,
                "not all elements read");
            break;
        default:
            // error -> do nothing
            return;
        }
    }
}

inline
void CIStreamContainerIterator::CheckState(EState state)
{
    bool ok = (m_State == state);
    if ( !ok ) {
        m_State = eError;
        BadState();
    }
}


void CIStreamContainerIterator::NextElement(void)
{
    CheckState(eElementBegin);
    GetStream().EndContainerElement();
    m_State = eElementEnd;
    BeginElement();
    if ( m_State == eNoMoreElements ) {
        GetStream().PopFrame();
        GetStream().EndContainer();
        GetStream().PopFrame();
        if (m_ContainerType.GetTypeFamily() == eTypeFamilyClass) {
            GetStream().EndNamedType();
            GetStream().PopFrame();
        }
    }
    if (m_State != eNoMoreElements) {
        m_State = eElementEnd;
    }
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
    NextElement();
}

void CIStreamContainerIterator::SkipElement(const CObjectTypeInfo& elementType)
{
    BeginElementData(elementType);
    GetStream().SkipObject(elementType.GetTypeInfo());
    NextElement();
}

void CIStreamContainerIterator::SkipElement(void)
{
    BeginElementData();
    GetStream().SkipObject(m_ElementTypeInfo);
    NextElement();
}

void CIStreamContainerIterator::CopyElement(CObjectStreamCopier& copier,
                                            COStreamContainer& out)
{
    BeginElementData();
    out.GetStream().BeginContainerElement(m_ElementTypeInfo);
    copier.CopyObject(m_ElementTypeInfo);
    out.GetStream().EndContainerElement();
    NextElement();
}

CIStreamContainerIterator& CIStreamContainerIterator::operator++(void)
{
    if (m_State == eElementBegin) {
        SkipElement();
    }
    if (m_State != eNoMoreElements) {
        CheckState(eElementEnd);
        m_State = eElementBegin;
    }
    else {
        m_State = eFinished;
    }
    return *this;
}


COStreamContainer::COStreamContainer(CObjectOStream& out,
                                     const CObjectTypeInfo& containerType)
    : CParent(out), m_ContainerType(containerType)
{
    const CContainerTypeInfo* containerTypeInfo;
    if (m_ContainerType.GetTypeFamily() == eTypeFamilyClass) {
        const CClassTypeInfo* classType =
            CTypeConverter<CClassTypeInfo>::SafeCast(m_ContainerType.GetTypeInfo());
        const CItemInfo* itemInfo =
            classType->GetItems().GetItemInfo(classType->GetItems().FirstIndex());
        _ASSERT(itemInfo->GetTypeInfo()->GetTypeFamily() == eTypeFamilyContainer);
        containerTypeInfo =
            CTypeConverter<CContainerTypeInfo>::SafeCast(itemInfo->GetTypeInfo());
        out.PushFrame(CObjectStackFrame::eFrameNamed, m_ContainerType.GetTypeInfo());
        out.BeginNamedType(m_ContainerType.GetTypeInfo());
    } else {
        containerTypeInfo = GetContainerType().GetContainerTypeInfo();
    }
    out.PushFrame(CObjectStackFrame::eFrameArray, containerTypeInfo);
    out.BeginContainer(containerTypeInfo);

    TTypeInfo elementTypeInfo = m_ElementTypeInfo =
        containerTypeInfo->GetElementType();
    out.PushFrame(CObjectStackFrame::eFrameArrayElement, elementTypeInfo);
}

COStreamContainer::~COStreamContainer(void)
{
    if ( Good() ) {
        try {
            GetStream().PopFrame();
            GetStream().EndContainer();
            GetStream().PopFrame();
            if (m_ContainerType.GetTypeFamily() == eTypeFamilyClass) {
                GetStream().EndNamedType();
                GetStream().PopFrame();
            }
        }
        catch (...) {
            GetStream().SetFailFlags(CObjectOStream::fIllegalCall,
                "container write error");
        }
    }
}

void COStreamContainer::WriteElement(const CConstObjectInfo& element)
{
    GetStream().BeginContainerElement(m_ElementTypeInfo);
    GetStream().WriteSeparateObject(element);
    GetStream().EndContainerElement();
}

void COStreamContainer::WriteElement(CObjectStreamCopier& copier,
                                     CObjectIStream& /*in*/)
{
    GetStream().BeginContainerElement(m_ElementTypeInfo);
    copier.CopyObject(m_ElementTypeInfo);
    GetStream().EndContainerElement();
}


END_NCBI_SCOPE
