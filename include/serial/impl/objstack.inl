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
*/

inline
void CObjectStackFrame::Reset(void)
{
    m_FrameType = eFrameOther;
    m_Notag = false;
    m_NsqMode = eNSQNotSet;
    m_TypeInfo = 0;
    m_MemberId = 0;
    m_ObjectPtr = 0;
}

inline
CObjectStackFrame::EFrameType CObjectStackFrame::GetFrameType(void) const
{
    return m_FrameType;
}

inline
bool CObjectStackFrame::HasTypeInfo(void) const
{
    return (m_FrameType != eFrameOther &&
            m_FrameType != eFrameChoiceVariant &&
            m_TypeInfo  != 0);
}

inline
bool CObjectStackFrame::HasTypeInfo(TTypeInfo type) const
{
    return m_TypeInfo == type;
}

inline
TTypeInfo CObjectStackFrame::GetTypeInfo(void) const
{
    _ASSERT(m_FrameType != eFrameOther &&
            m_FrameType != eFrameChoiceVariant);
    _ASSERT(m_TypeInfo != 0);
    return m_TypeInfo;
}

inline
TConstObjectPtr CObjectStackFrame::GetObjectPtr(void) const
{
    return m_ObjectPtr;
}

inline
bool CObjectStackFrame::HasMemberId(void) const
{
    return (m_FrameType == eFrameClassMember ||
            m_FrameType == eFrameChoiceVariant) && (m_MemberId != 0);
}

inline
const CMemberId& CObjectStackFrame::GetMemberId(void) const
{
    _ASSERT(m_FrameType == eFrameClassMember ||
            m_FrameType == eFrameChoiceVariant ||
            m_FrameType == eFrameArray);
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
void CObjectStackFrame::SetNotag(bool set)
{
    m_Notag = set;
#if defined(NCBI_SERIAL_IO_TRACE)
    cout << ", "  << (m_Notag ? "N" : "!N");
#endif
}
inline
bool CObjectStackFrame::GetNotag(void) const
{
    return m_Notag;
}

inline
ENsQualifiedMode CObjectStackFrame::IsNsQualified(void) const
{
    return m_NsqMode;
}
inline
void CObjectStackFrame::SetNsQualified(ENsQualifiedMode mode)
{
    m_NsqMode = mode;
}


inline
size_t CObjectStack::GetStackDepth(void) const
{
    return static_cast<size_t>(m_StackPtr - m_Stack);
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
#if defined(NCBI_SERIAL_IO_TRACE)
    TracePushFrame(true);
#endif
    return frame;
}

inline
CObjectStack::TFrame& CObjectStack::PushFrame(EFrameType type,
                                              TTypeInfo typeInfo,
                                              TConstObjectPtr objectPtr)
{
    _ASSERT(type != TFrame::eFrameOther &&
            type != TFrame::eFrameClassMember &&
            type != TFrame::eFrameChoiceVariant);
    _ASSERT(typeInfo != 0);
    TFrame& frame = PushFrame(type);
    frame.m_TypeInfo = typeInfo;
    frame.m_ObjectPtr = objectPtr;
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
    x_PushStackPath();
    return frame;
}

inline
void CObjectStack::PopFrame(void)
{
    _ASSERT(!StackIsEmpty());
#if defined(NCBI_SERIAL_IO_TRACE)
    TracePushFrame(false);
#endif
    x_PopStackPath();
    m_StackPtr->Reset();
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
void CObjectStack::SetTopMemberId(const CMemberId& memberid)
{
    x_PopStackPath();
    TopFrame().SetMemberId(memberid);
    x_PushStackPath();
}

inline
const CObjectStack::TFrame& CObjectStack::FetchFrameFromBottom(size_t index) const
{
    TFrame* ptr = m_Stack + 1 + index;
    _ASSERT(ptr <= m_StackPtr);
    return *ptr;
}

inline
void CObjectStack::WatchPathHooks(bool set)
{
    m_WatchPathHooks = set;
    m_PathValid = false;
    GetStackPath();
}


#endif /* def OBJSTACK__HPP  &&  ndef OBJSTACK__INL */
