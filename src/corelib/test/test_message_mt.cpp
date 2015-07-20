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
 *   Test for IMessage/IMessageListener in multithreaded environment
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbi_message.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestMessageApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);
    void DoPostMessages(int idx);

protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
};


class CTestListener : public CMessageListener_Base
{
public:
    CTestListener(const string& handle_prefix) : m_HandlePrefix(handle_prefix) {}

    virtual EPostResult PostMessage(const IMessage& message)
    {
        if ( NStr::StartsWith(message.GetText(), m_HandlePrefix) ) {
            return CMessageListener_Base::PostMessage(message);
        }
        return eUnhandled;
    }

private:
    string m_HandlePrefix;
};


static const int kMsgCount = 1000;

void CTestMessageApp::DoPostMessages(int idx)
{
    string idx_str = NStr::NumericToString(idx);

    _ASSERT(IMessageListener::HaveListeners());

    for (int i = 0; i < kMsgCount; i++) {
        string s = idx_str + "-" + NStr::NumericToString(i);
        IMessageListener::Post(CMessage_Base("msg2-" + s, eDiag_Error));
        IMessageListener::Post(CMessage_Base("msg3-" + s, eDiag_Error));
        IMessageListener::Post(CMessage_Base("msg4-" + s, eDiag_Error));
    }
}


bool CTestMessageApp::Thread_Run(int idx)
{
    // This one should receive all messages, both handled and unhandled.
    CRef<CMessageListener_Base> ml1(new CMessageListener_Base());
    size_t ml1_depth = IMessageListener::PushListener(*ml1, IMessageListener::eListen_All);

    // Receive unhandled messages only.
    CRef<CMessageListener_Base> ml2(new CMessageListener_Base());
    size_t ml2_depth = IMessageListener::PushListener(*ml2);

    // Receive all messages, handle only those starting with 'msg3'
    CRef<CTestListener> ml3(new CTestListener("msg3"));
    size_t ml3_depth = IMessageListener::PushListener(*ml3, IMessageListener::eListen_All);

    // Handle messages starting with 'msg4'
    CRef<CTestListener> ml4(new CTestListener("msg4"));
    size_t ml4_depth = IMessageListener::PushListener(*ml4);

    // Handle nothing. Used to test popping lost listeners.
    CRef<CTestListener> ml5(new CTestListener("---"));
    size_t ml5_depth = IMessageListener::PushListener(*ml5);

    DoPostMessages(idx);

    // Pop both ml4 and ml5. Warns about lost listener once per process.
    IMessageListener::PopListener(ml4_depth);
    // Once-per-process warning about already removed listener.
    IMessageListener::PopListener(ml5_depth);
    // Remove just the topmost listener.
    IMessageListener::PopListener();
    // Only one listener should be removed.
    IMessageListener::PopListener(ml2_depth);
    IMessageListener::PopListener();
    _ASSERT(!IMessageListener::HaveListeners());

    _ASSERT(ml1->Count() == kMsgCount*3); // All messages
    _ASSERT(ml2->Count() == kMsgCount); // 'msg2' only
    _ASSERT(ml3->Count() == kMsgCount); // 'msg3' only
    _ASSERT(ml4->Count() == kMsgCount); // 'msg4' only
    _ASSERT(ml5->Count() == 0);

    return true;
}


bool CTestMessageApp::TestApp_Init(void)
{
    NcbiCout << NcbiEndl
             << "Testing message listeners with "
             << NStr::IntToString(s_NumThreads)
             << " threads..."
             << NcbiEndl;

    return true;
}

bool CTestMessageApp::TestApp_Exit(void)
{
    NcbiCout << "Test completed successfully!"
             << NcbiEndl << NcbiEndl;
    return true;
}



///////////////////////////////////
// MAIN
//

int main(int argc, const char* argv[]) 
{
    return CTestMessageApp().AppMain(argc, argv);
}
