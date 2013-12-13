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
#include <serial/impl/objstack.hpp>

BEGIN_NCBI_SCOPE

static const size_t KInitialStackSize = 16;

CObjectStack::CObjectStack(void)
{
    TFrame* stack = m_Stack = m_StackPtr = new TFrame[KInitialStackSize];
    m_StackEnd = stack + KInitialStackSize;
    for ( size_t i = 0; i < KInitialStackSize; ++i ) {
        m_Stack[i].Reset();
    }
    m_WatchPathHooks = m_PathValid = false;
}

CObjectStack::~CObjectStack(void)
{
    delete[] m_Stack;
}

void CObjectStack::ResetState(void)
{
    m_PathValid = false;
}

void CObjectStack::UnendedFrame(void)
{
}

void CObjectStack::ClearStack(void)
{
    m_StackPtr = m_Stack;
}

string CObjectStack::GetStackTraceASN(void) const
{
    if (!GetStackDepth()) {
        return "stack is empty";
    }
//    _ASSERT(FetchFrameFromBottom(0).m_FrameType == TFrame::eFrameNamed);
//    _ASSERT(FetchFrameFromBottom(0).m_TypeInfo);
    string stack = FetchFrameFromBottom(0).HasTypeInfo() ?
        FetchFrameFromBottom(0).m_TypeInfo->GetName() : "?";
    for ( size_t i = 1; i < GetStackDepth(); ++i ) {
        const TFrame& frame = FetchFrameFromBottom(i);
        switch ( frame.m_FrameType ) {
        case TFrame::eFrameClassMember:
        case TFrame::eFrameChoiceVariant:
            {
                if ( !frame.m_MemberId ) {
                    _ASSERT(i == GetStackDepth() - 1);
                }
                else {
                    const CMemberId& id = *frame.m_MemberId;
                    stack += '.';
                    if ( !id.GetName().empty() ) {
                        stack += id.GetName();
                    }
                    else {
                        stack += '[';
                        stack += NStr::IntToString(id.GetTag());
                        stack += ']';
                    }
                }
            }
            break;
        case TFrame::eFrameArrayElement:
            stack += ".E";
            break;
        default:
            break;
        }
    }
    return stack;
}

CObjectStack::TFrame& CObjectStack::PushFrameLong(void)
{
    size_t depth = m_StackPtr - m_Stack;
    size_t oldSize = m_StackEnd - m_Stack;
    size_t newSize = oldSize * 2;
    TFrame* newStack = new TFrame[newSize];

    {
        // copy old stack
        for ( size_t i = 0; i < oldSize; ++i )
            newStack[i] = m_Stack[i];
    }
    {
        // clear new area of new stack
        for ( size_t i = oldSize; i < newSize; ++i )
            newStack[i].Reset();
    }

    delete[] m_Stack;

    m_Stack = newStack;
    m_StackEnd = newStack + newSize;

    return *(m_StackPtr = (newStack + depth + 1));
}

bool CObjectStack::IsNsQualified(void)
{
    if (StackIsEmpty()) {
        return true;
    }
    ENsQualifiedMode mode;
    if (TopFrame().HasTypeInfo()) {
        if (!TopFrame().GetTypeInfo()->GetModuleName().empty()) {
            return true;
        }
    }
    size_t i, count = GetStackDepth();
    for (i=0; i<count; ++i) {

        CObjectStack::TFrame& frame = FetchFrameFromTop(i);
        mode = frame.IsNsQualified();
        if (mode != eNSQNotSet) {
            return mode == eNSQualified;
        }

        if (frame.HasTypeInfo()) {
            mode = frame.GetTypeInfo()->IsNsQualified();
            if (mode != eNSQNotSet) {
                frame.SetNsQualified(mode);
                return mode == eNSQualified;
            }
        }

        if (frame.HasMemberId()) {
            const CMemberId& mem = frame.GetMemberId();
            mode = mem.IsNsQualified();
            if (mode != eNSQNotSet) {
                frame.SetNsQualified(mode);
                return mode == eNSQualified;
            }
            if (mem.IsAttlist()) {
                frame.SetNsQualified(eNSUnqualified);
                return false;
            }
        }
    }
    TopFrame().SetNsQualified(eNSQualified);
    return true;
}

void CObjectStack::x_PushStackPath(void)
{
    if (!m_WatchPathHooks) {
        m_PathValid = false;
        return;
    }
    if (!m_PathValid) {
        for ( size_t i = 1; i < GetStackDepth(); ++i ) {
            const TFrame& frame = FetchFrameFromTop(i);
            if (frame.HasTypeInfo()) {
                // there is no "root" symbol
                m_MemberPath = frame.GetTypeInfo()->GetName();
                break;
            }
        }
    }
    const CMemberId& mem_id = TopFrame().GetMemberId();
    if (mem_id.HasNotag() || mem_id.IsAttlist()) {
        return;
    }
    // member separator symbol is '.'
    m_MemberPath += '.';
    const string& member = mem_id.GetName();
    if (!member.empty()) {
        m_MemberPath += member;
    } else {
        m_MemberPath += NStr::IntToString(mem_id.GetTag());
    }
    m_PathValid = true;
    x_SetPathHooks(true);
}

void CObjectStack::x_PopStackPath(void)
{
    if (!m_WatchPathHooks) {
        m_PathValid = false;
        return;
    }
    if (GetStackDepth() == 1) {
        x_SetPathHooks(false);
        m_PathValid = false;
    } else {
        const TFrame& top = TopFrame();
        if (top.HasMemberId()) {
            const CMemberId& mem_id = top.GetMemberId();
            if (mem_id.HasNotag() || mem_id.IsAttlist()) {
                return;
            }
            x_SetPathHooks(false);
            // member separator symbol is '.'
            m_MemberPath.erase(m_MemberPath.find_last_of('.'));
        }
    }
}

const string& CObjectStack::GetStackPath(void)
{
    if (!m_PathValid && GetStackDepth()) {
//        _ASSERT(FetchFrameFromBottom(0).m_FrameType == TFrame::eFrameNamed);
//        _ASSERT(FetchFrameFromBottom(0).m_TypeInfo);
//        m_MemberPath = FetchFrameFromBottom(0).GetTypeInfo()->GetName();
        // there is no "root" symbol
        m_MemberPath = FetchFrameFromBottom(0).HasTypeInfo() ?
            FetchFrameFromBottom(0).m_TypeInfo->GetName() : "?";
        for ( size_t i = 1; i < GetStackDepth(); ++i ) {
            const TFrame& frame = FetchFrameFromBottom(i);
            if (frame.HasMemberId()) {
                const CMemberId& mem_id = frame.GetMemberId();
                if (mem_id.HasNotag() || mem_id.IsAttlist()) {
                    continue;
                }
                // member separator symbol is '.'
                m_MemberPath += '.';
                const string& member = mem_id.GetName();
                if (!member.empty()) {
                    m_MemberPath += member;
                } else {
                    m_MemberPath += NStr::IntToString(mem_id.GetTag());
                }
            }
        }
        m_PathValid = true;
    }
    return m_MemberPath;
}

void CObjectStack::PopErrorFrame(void)
{
    try {
        UnendedFrame();
    }
    catch (...) {
        PopFrame();
        throw;
    }
    PopFrame();
}

const char* CObjectStackFrame::GetFrameTypeName(void) const
{
    const char* s;
    switch (GetFrameType())
    {
    default:                  s = "UNKNOWN";             break;
    case eFrameOther:         s = "eFrameOther";         break;
    case eFrameNamed:         s = "eFrameNamed";         break;
    case eFrameArray:         s = "eFrameArray";         break;
    case eFrameArrayElement:  s = "eFrameArrayElement";  break;
    case eFrameClass:         s = "eFrameClass";         break;
    case eFrameClassMember:   s = "eFrameClassMember";   break;
    case eFrameChoice:        s = "eFrameChoice";        break;
    case eFrameChoiceVariant: s = "eFrameChoiceVariant"; break;
    }
    return s;
}

#if defined(NCBI_SERIAL_IO_TRACE)

void CObjectStack::TracePushFrame(bool push) const
{
    cout << endl ;
    int depth = (int)GetStackDepth();
    cout << depth;
    for (; depth>0; --depth) {
        cout.put(' ');
    }
    cout << (push ? "Enter " : "Leave ") << m_StackPtr->GetFrameTypeName();
}

#endif

string CObjectStackFrame::GetFrameInfo(void) const
{
    string info(" Frame type= ");
    info += GetFrameTypeName();
    if (m_TypeInfo) {
        info += ", Object type= " + m_TypeInfo->GetName();
    }
    if (m_MemberId) {
        info += ", Member name= " + m_MemberId->GetName();
    }
    return info;
}


string CObjectStackFrame::GetFrameName(void) const
{
    string info;
    switch ( GetFrameType() ) {
    case eFrameClassMember:
    case eFrameChoiceVariant:
        {
            if ( m_MemberId ) {
                const CMemberId& id = *m_MemberId;
                info = '.';
                if ( !id.GetName().empty() ) {
                    info += id.GetName();
                }
                else {
                    info += '[';
                    info += NStr::IntToString(id.GetTag());
                    info += ']';
                }
            }
        }
        break;
    case eFrameArrayElement:
        info = "[]";
        break;
    case eFrameArray:
        info = "[]";
        break;
    case eFrameNamed:
        info = GetTypeInfo()->GetName();
        break;
    default:
        {
            break;
        }
    }
    return info;
}


END_NCBI_SCOPE
