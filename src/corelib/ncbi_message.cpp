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
#include <stdlib.h>
#include <list>


BEGIN_NCBI_SCOPE


struct SListenerNode {
public:
    SListenerNode(IMessageListener& listener,
                  IMessageListener::EListenFlag flag)
                  : m_Listener(&listener), m_Flag(flag) {}

    CRef<IMessageListener> m_Listener;
    IMessageListener::EListenFlag m_Flag;
};


typedef list<SListenerNode> TListenerStack;

void s_ListenerStackCleanup(TListenerStack* value,
                          void* /*cleanup_data*/)
{
    if ( !value ) return;
    delete value;
}


static CStaticTls<TListenerStack> s_Listeners;


TListenerStack& s_GetListenerStack(void)
{
    TListenerStack* ls = s_Listeners.GetValue();
    if ( !ls ) {
        ls = new TListenerStack;
        s_Listeners.SetValue(ls, s_ListenerStackCleanup);
    }
    _ASSERT(ls);
    return *ls;
}

size_t IMessageListener::PushListener(IMessageListener& listener,
                                      EListenFlag flag)
{
    TListenerStack& ls = s_GetListenerStack();
    ls.push_front(SListenerNode(listener, flag));
    return ls.size();
}


void IMessageListener::PopListener(size_t depth)
{
    TListenerStack& ls = s_GetListenerStack();
    if ( ls.empty() ) return;
    if (depth == 0) {
        depth = ls.size();
    }
    while (ls.size() >= depth) {
        ls.pop_front();
    }
}


IMessageListener::EPostResult
IMessageListener::Post(const IMessage& message)
{
    EPostResult ret = eUnhandled;
    TListenerStack ls = s_GetListenerStack();
    NON_CONST_ITERATE(TListenerStack, it, ls) {
        if (ret == eHandled  &&  it->m_Flag == eListen_Unhandled) continue;
        if (it->m_Listener->PostMessage(message) == eHandled) {
            ret = eHandled;
        }
    }
    return ret;
}


IMessageListener::EPostResult
IMessageListener::Post(const string& message,
                       Uint8         current,
                       Uint8         total)
{
    EPostResult ret = eUnhandled;
    TListenerStack ls = s_GetListenerStack();
    NON_CONST_ITERATE(TListenerStack, it, ls) {
        if (ret == eHandled  &&  it->m_Flag == eListen_Unhandled) continue;
        if (it->m_Listener->PostProgress(message, current, total) == eHandled) {
            ret = eHandled;
        }
    }
    return ret;
}


CMessage_Base::CMessage_Base(const string& txt,
                             EDiagSev      sev,
                             int           err_code,
                             int           sub_code)
    : m_Text(txt),
      m_Severity(sev),
      m_ErrCode(err_code),
      m_SubCode(sub_code)
{
}


string CMessage_Base::GetText(void) const
{
    return m_Text;
}


EDiagSev CMessage_Base::GetSeverity(void) const
{
    return m_Severity;
}


int CMessage_Base::GetCode(void) const
{
    return m_ErrCode;
}


int CMessage_Base::GetSubCode(void) const
{
    return m_SubCode;
}


IMessage* CMessage_Base::Clone(void) const
{
    return new CMessage_Base(*this);
}


void CMessage_Base::Write(CNcbiOstream& out) const
{
    out << CNcbiDiag::SeverityName(GetSeverity()) << ": " << GetText() << endl;
}


string CMessage_Base::Compose(void) const
{
    CNcbiOstrstream out;
    Write(out);
    return CNcbiOstrstreamToString(out);
}


IMessageListener::EPostResult
CMessageListener_Base::PostMessage(const IMessage& message)
{
    m_Messages.push_back(AutoPtr<IMessage>(message.Clone()));
    return eHandled;
}


IMessageListener::EPostResult
CMessageListener_Base::PostProgress(const string& message,
                                    Uint8         current,
                                    Uint8         total)
{
    ERR_POST(Note << message << " [" << current << "/" << total << "]");
    return eHandled;
}


const IMessage& CMessageListener_Base::GetMessage(size_t index) const
{
    return *m_Messages[index].get();
}


size_t CMessageListener_Base::Count(void) const
{
    return m_Messages.size();
}


void CMessageListener_Base::Clear(void)
{
    m_Messages.clear();
}


END_NCBI_SCOPE
