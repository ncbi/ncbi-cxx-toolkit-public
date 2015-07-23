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
 * Authors:  Aleksey Grichenko
 *
 * File Description:   IMessage/IMessageListener implementation
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_message.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/error_codes.hpp>
#include <stdlib.h>
#include <list>

#define NCBI_USE_ERRCODE_X   Corelib_Message

BEGIN_NCBI_SCOPE


CMessageListener_Stack::CMessageListener_Stack(void)
{
}


size_t CMessageListener_Stack::PushListener(IMessageListener& listener,
    IMessageListener::EListenFlag flag)
{
    m_Stack.push_front(SListenerNode(listener, flag));
    return m_Stack.size();
}


void CMessageListener_Stack::PopListener(size_t depth)
{
    size_t sz = m_Stack.size();
    if (depth == 0) depth = sz;
    if (m_Stack.empty()  ||  sz < depth) {
        // Nothing to pop.
        ERR_POST_X_ONCE(1, Warning <<
            "Unbalanced PushListener/PopListener calls: "
            "listener index " << depth << " has been already removed");
        return;
    }
    if (sz > depth) {
        // Report lost listeners.
        ERR_POST_X_ONCE(2, Warning <<
            "Unbalanced PushListener/PopListener calls: "
            "removing " << sz - depth << " lost listeners");
    }
    while (m_Stack.size() >= depth) {
        m_Stack.pop_front();
    }
}


bool CMessageListener_Stack::HaveListeners(void)
{
    return !m_Stack.empty();
}


IMessageListener::EPostResult
CMessageListener_Stack::Post(const IMessage& message)
{
    IMessageListener::EPostResult ret = IMessageListener::eUnhandled;
    NON_CONST_ITERATE(TListenerStack, it, m_Stack) {
        if (ret == IMessageListener::eHandled  &&
            it->m_Flag == IMessageListener::eListen_Unhandled) continue;
        if (it->m_Listener->PostMessage(message) == IMessageListener::eHandled) {
            ret = IMessageListener::eHandled;
        }
    }
    return ret;
}


IMessageListener::EPostResult
CMessageListener_Stack::Post(const IProgressMessage& progress)
{
    IMessageListener::EPostResult ret = IMessageListener::eUnhandled;
    NON_CONST_ITERATE(TListenerStack, it, m_Stack) {
        if (ret == IMessageListener::eHandled  &&
            it->m_Flag == IMessageListener::eListen_Unhandled) continue;
        if (it->m_Listener->PostProgress(progress) == IMessageListener::eHandled) {
            ret = IMessageListener::eHandled;
        }
    }
    return ret;
}


void s_ListenerStackCleanup(CMessageListener_Stack* value,
                          void* /*cleanup_data*/)
{
    if ( !value ) return;
    delete value;
}


static CStaticTls<CMessageListener_Stack> s_Listeners;


CMessageListener_Stack& s_GetListenerStack(void)
{
    CMessageListener_Stack* ls = s_Listeners.GetValue();
    if ( !ls ) {
        ls = new CMessageListener_Stack;
        s_Listeners.SetValue(ls, s_ListenerStackCleanup);
    }
    _ASSERT(ls);
    return *ls;
}

size_t IMessageListener::PushListener(IMessageListener& listener,
                                      EListenFlag flag)
{
    CMessageListener_Stack& ls = s_GetListenerStack();
    return ls.PushListener(listener, flag);
}


void IMessageListener::PopListener(size_t depth)
{
    CMessageListener_Stack& ls = s_GetListenerStack();
    return ls.PopListener(depth);
}


bool IMessageListener::HaveListeners(void)
{
    return s_GetListenerStack().HaveListeners();
}


IMessageListener::EPostResult
IMessageListener::Post(const IMessage& message)
{
    return s_GetListenerStack().Post(message);
}


IMessageListener::EPostResult
IMessageListener::Post(const IProgressMessage& progress)
{
    return s_GetListenerStack().Post(progress);
}


CMessage_Basic::CMessage_Basic(const string& txt,
                               EDiagSev      sev,
                               int           err_code,
                               int           sub_code)
    : m_Text(txt),
      m_Severity(sev),
      m_ErrCode(err_code),
      m_SubCode(sub_code)
{
}


string CMessage_Basic::GetText(void) const
{
    return m_Text;
}


EDiagSev CMessage_Basic::GetSeverity(void) const
{
    return m_Severity;
}


int CMessage_Basic::GetCode(void) const
{
    return m_ErrCode;
}


int CMessage_Basic::GetSubCode(void) const
{
    return m_SubCode;
}


IMessage* CMessage_Basic::Clone(void) const
{
    return new CMessage_Basic(*this);
}


void CMessage_Basic::Write(CNcbiOstream& out) const
{
    out << CNcbiDiag::SeverityName(GetSeverity()) << ": " << GetText() << endl;
}


string CMessage_Basic::Compose(void) const
{
    CNcbiOstrstream out;
    Write(out);
    return CNcbiOstrstreamToString(out);
}


CProgressMessage_Basic::CProgressMessage_Basic(const string& txt,
                                               Uint8         current,
                                               Uint8         total)
    : m_Text(txt),
      m_Current(current),
      m_Total(total)
{
}


string CProgressMessage_Basic::GetText(void) const
{
    return m_Text;
}


Uint8 CProgressMessage_Basic::GetCurrent(void) const
{
    return m_Current;
}


Uint8 CProgressMessage_Basic::GetTotal(void) const
{
    return m_Total;
}


CProgressMessage_Basic* CProgressMessage_Basic::Clone(void) const
{
    return new CProgressMessage_Basic(*this);
}


void CProgressMessage_Basic::Write(CNcbiOstream& out) const
{
    out << GetText() << " [" << m_Current << "/" << m_Total << "]" << endl;
}


string CProgressMessage_Basic::Compose(void) const
{
    CNcbiOstrstream out;
    Write(out);
    return CNcbiOstrstreamToString(out);
}


IMessageListener::EPostResult
CMessageListener_Basic::PostMessage(const IMessage& message)
{
    m_Messages.push_back(AutoPtr<IMessage>(message.Clone()));
    return eHandled;
}


IMessageListener::EPostResult
CMessageListener_Basic::PostProgress(const IProgressMessage& progress)
{
    ERR_POST(Note << progress);
    return eHandled;
}


const IMessage& CMessageListener_Basic::GetMessage(size_t index) const
{
    return *m_Messages[index].get();
}


size_t CMessageListener_Basic::Count(void) const
{
    return m_Messages.size();
}


void CMessageListener_Basic::Clear(void)
{
    m_Messages.clear();
}


END_NCBI_SCOPE
