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
* Revision 1.3  2000/09/01 13:16:20  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.2  2000/08/15 19:44:51  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.1  2000/05/24 20:08:49  vasilche
* Implemented XML dump.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objstack.hpp>
#include <serial/strbuffer.hpp>

BEGIN_NCBI_SCOPE

static size_t KInitialStackSize = 16;

CObjectStack::CObjectStack(void)
{
    TFrame* stack = m_Stack = m_StackPtr = new TFrame[KInitialStackSize];
    m_StackEnd = stack + KInitialStackSize;
    for ( size_t i = 0; i < KInitialStackSize; ++i ) {
        m_Stack[i].Reset();
    }
}

CObjectStack::~CObjectStack(void)
{
    delete[] m_Stack;
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
    _ASSERT(FetchFrameFromBottom(0).m_FrameType == TFrame::eFrameNamed);
    _ASSERT(FetchFrameFromBottom(0).m_TypeInfo);
    string stack = FetchFrameFromBottom(0).m_TypeInfo->GetName();
    for ( size_t i = 1; i < GetStackDepth(); ++i ) {
        const TFrame& frame = FetchFrameFromBottom(i);
        switch ( frame.m_FrameType ) {
        case TFrame::eFrameClassMember:
        case TFrame::eFrameChoiceVariant:
            {
                _ASSERT(frame.m_MemberId);
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

    for ( size_t i = 0; i < oldSize; ++i )
        newStack[i] = m_Stack[i];
    for ( size_t i = oldSize; i < newSize; ++i )
        newStack[i].Reset();

    delete[] m_Stack;

    m_Stack = newStack;
    m_StackEnd = newStack + newSize;

    return *(m_StackPtr = (newStack + depth + 1));
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

END_NCBI_SCOPE
