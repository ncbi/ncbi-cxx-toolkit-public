#if defined(OBJSTACK__HPP)  &&  !defined(OBJSTACK__INL)
#define OBJSTACK__INL

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
* Revision 1.5  2000/09/01 13:16:02  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.4  2000/08/15 19:44:42  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.3  2000/06/07 19:45:44  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.2  2000/06/01 19:06:58  vasilche
* Added parsing of XML data.
*
* Revision 1.1  2000/05/24 20:08:15  vasilche
* Implemented XML dump.
*
* ===========================================================================
*/

inline
void CObjectStackFrame::Reset(void)
{
    m_FrameType = eFrameOther;
    m_TypeInfo = 0;
    m_MemberId = 0;
}

inline
CObjectStackFrame::EFrameType CObjectStackFrame::GetFrameType(void) const
{
    return m_FrameType;
}

inline
TTypeInfo CObjectStackFrame::GetTypeInfo(void) const
{
    _ASSERT(m_FrameType != eFrameOther &&
            m_FrameType != eFrameClassMember &&
            m_FrameType != eFrameChoiceVariant);
    _ASSERT(m_TypeInfo != 0);
    return m_TypeInfo;
}

inline
const CMemberId& CObjectStackFrame::GetMemberId(void) const
{
    _ASSERT(m_FrameType == eFrameClassMember ||
            m_FrameType == eFrameChoiceVariant);
    _ASSERT(m_MemberId != 0);
    return *m_MemberId;
}

inline
void CObjectStackFrame::SetMemberId(const CMemberId& memberid)
{
    _ASSERT(m_FrameType == eFrameClassMember ||
            m_FrameType == eFrameChoiceVariant);
    m_MemberId = &memberid;
}

inline
size_t CObjectStack::GetStackDepth(void) const
{
    return m_StackPtr - m_Stack;
}

inline
bool CObjectStack::StackIsEmpty(void) const
{
    return m_Stack == m_StackPtr;
}

inline
CObjectStack::TFrame& CObjectStack::PushFrame(void)
{
    TFrame* newPtr = m_StackPtr + 1;
    if ( newPtr >= m_StackEnd )
        return PushFrameLong();
    m_StackPtr = newPtr;
    return *newPtr;
}

inline
CObjectStack::TFrame& CObjectStack::PushFrame(EFrameType type)
{
    TFrame& frame = PushFrame();
    frame.m_FrameType = type;
    return frame;
}

inline
CObjectStack::TFrame& CObjectStack::PushFrame(EFrameType type,
                                              TTypeInfo typeInfo)
{
    _ASSERT(type != TFrame::eFrameOther &&
            type != TFrame::eFrameClassMember &&
            type != TFrame::eFrameChoiceVariant);
    _ASSERT(typeInfo != 0);
    TFrame& frame = PushFrame(type);
    frame.m_TypeInfo = typeInfo;
    return frame;
}

inline
CObjectStack::TFrame& CObjectStack::PushFrame(EFrameType type,
                                              const CMemberId& memberId)
{
    _ASSERT(type == TFrame::eFrameClassMember ||
            type == TFrame::eFrameChoiceVariant);
    TFrame& frame = PushFrame(type);
    frame.m_MemberId = &memberId;
    return frame;
}

inline
void CObjectStack::PopFrame(void)
{
    _ASSERT(!StackIsEmpty());
    --m_StackPtr;
}

inline
CObjectStack::TFrame& CObjectStack::FetchFrameFromTop(size_t index)
{
    TFrame* ptr = m_StackPtr - index;
    _ASSERT(ptr > m_Stack);
    return *ptr;
}

inline
const CObjectStack::TFrame& CObjectStack::FetchFrameFromTop(size_t index) const
{
    TFrame* ptr = m_StackPtr - index;
    _ASSERT(ptr > m_Stack);
    return *ptr;
}

inline
const CObjectStack::TFrame& CObjectStack::TopFrame(void) const
{
    _ASSERT(!StackIsEmpty());
    return *m_StackPtr;
}

inline
CObjectStack::TFrame& CObjectStack::TopFrame(void)
{
    _ASSERT(!StackIsEmpty());
    return *m_StackPtr;
}

inline
const CObjectStack::TFrame& CObjectStack::FetchFrameFromBottom(size_t index) const
{
    TFrame* ptr = m_Stack + 1 + index;
    _ASSERT(ptr <= m_StackPtr);
    return *ptr;
}

#endif /* def OBJSTACK__HPP  &&  ndef OBJSTACK__INL */
