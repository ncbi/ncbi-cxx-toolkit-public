#ifndef CORELIB___LISTENER_STACK__HPP
#define CORELIB___LISTENER_STACK__HPP

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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   IMessageListener stack implementation.
 *
 */


BEGIN_NCBI_SCOPE


class NCBI_XNCBI_EXPORT CMessageListener_Stack
{
public:
    CMessageListener_Stack(void);

    size_t PushListener(IMessageListener& listener,
                        IMessageListener::EListenFlag flag);
    void PopListener(size_t depth = 0);
    bool HaveListeners(void);
    IMessageListener::EPostResult Post(const IMessage& message);
    IMessageListener::EPostResult Post(const IProgressMessage& progress);
private:
    struct SListenerNode {
    public:
        SListenerNode(IMessageListener& listener,
                      IMessageListener::EListenFlag flag)
                      : m_Listener(&listener), m_Flag(flag) {}

        CRef<IMessageListener> m_Listener;
        IMessageListener::EListenFlag m_Flag;
    };

    typedef list<SListenerNode> TListenerStack;

    TListenerStack m_Stack;
};


END_NCBI_SCOPE

#endif  /* CORELIB___LISTENER_STACK__HPP */
