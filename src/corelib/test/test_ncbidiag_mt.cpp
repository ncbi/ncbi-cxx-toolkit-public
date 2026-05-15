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
 *   Test for "NCBIDIAG" in multithreaded environment
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>
#include <algorithm>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestDiagApp : public CThreadedApp
{
public:
    virtual bool Thread_Init(int idx);
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Args(CArgDescriptions& args);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
private:
    typedef list<string> TStringList;

    void x_TestOldFormat(TStringList& messages);
    void x_TestNewFormat(TStringList& messages);

    CAsyncDiagHandler m_Handler;
    bool m_UseAsync = false;
    bool m_AsyncDiscard = false;
};


static CNcbiOstrstream s_Sout;
static atomic<size_t> s_CreateLine(0);
static atomic<size_t> s_LogLine(0);
static atomic<size_t> s_ErrLine(0);


bool CTestDiagApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddOptionalKey("format", "Format", "Log format",
        CArgDescriptions::eString);
    args.SetConstraint("format", &(*new CArgAllow_Strings, "old", "new"));
    args.AddFlag("async", "Use async handler");
    args.AddFlag("async_discard", "Discard queue overflow in async mode");
    return true;
}


bool CTestDiagApp::Thread_Init(int idx)
{
    if (!s_CreateLine) s_CreateLine = __LINE__ + 1;
    ERR_POST(Note << "Thread " + NStr::IntToString(idx) + " created");
    return true;
}


const int kAsyncRequests = 10;
const int kAsyncRepeats = 10;

bool CTestDiagApp::Thread_Run(int idx)
{
    if (m_UseAsync) {
        for (int r = 0; r < 10; ++r) {
            auto& ctx = CDiagContext::GetRequestContext();
            auto rid = ctx.SetRequestID();
            GetDiagContext().PrintRequestStart();
            for (int i = 0; i < 10; ++i) {
                if (!s_LogLine)
                    s_LogLine = __LINE__ + 1;
                ERR_POST(Note << "NOTE message from thread " + NStr::IntToString(idx) << "; request " << rid);
                if (!s_ErrLine)
                    s_ErrLine = __LINE__ + 1;
                ERR_POST("ERROR message from thread " + NStr::IntToString(idx) << "; request " << rid);
            }
            GetDiagContext().PrintRequestStop();
        }
    }
    else {
        if (!s_LogLine)
            s_LogLine = __LINE__ + 1;
        ERR_POST(Note << "NOTE message from thread " + NStr::IntToString(idx));
        if (!s_ErrLine)
            s_ErrLine = __LINE__ + 1;
        ERR_POST("ERROR message from thread " + NStr::IntToString(idx));
    }
    for ( int i = 0; i < 1000000; ++i ) {
        GetCurrentThreadSystemID();
        //CThread::GetSelf();
    }
    return true;
}

bool CTestDiagApp::TestApp_Init(void)
{
    const CArgs& args = GetArgs();
    if ( args["format"] ) {
        GetDiagContext().SetOldPostFormat(args["format"].AsString() == "old");
    }
    m_UseAsync = args["async"];
    m_AsyncDiscard = args["async_discard"];

    NcbiCout << NcbiEndl
             << "Testing NCBIDIAG with "
             << NStr::IntToString(s_NumThreads)
             << " threads ("
             << (GetDiagContext().IsSetOldPostFormat() ? "old" : "new")
             << " format)..."
             << NcbiEndl;
    SetDiagStream(&s_Sout);
    if (m_UseAsync) {
        //if (m_AsyncDiscard) m_Handler.SetDiscardOnOverflow();
        m_Handler.InstallToDiag();
    }
    return true;
}


struct SRequestRec
{
    bool m_Start = false;
    bool m_Stop = false;
    int  m_MsgCount = 0;
};


bool CTestDiagApp::TestApp_Exit(void)
{
    if (m_UseAsync) {
        // This must be done before processing the output to make sure all messages were flushed.
        m_Handler.RemoveFromDiag();
    }
    bool old_format = GetDiagContext().IsSetOldPostFormat();
    // Verify the result
    string test_res = CNcbiOstrstreamToString(s_Sout);
    TStringList messages;

    // Get the list of messages and check the size
    NStr::Split(test_res, "\r\n", messages,
        NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    if (m_UseAsync) {
        int msg_per_request = kAsyncRepeats * 2;
        if (!old_format) msg_per_request += 2; // start/stop
        int msg_per_thread = kAsyncRequests * msg_per_request + 1;
        int expected = s_NumThreads * msg_per_thread;
        if (m_AsyncDiscard) {
            //assert(messages.size() <= expected + m_LogMsgCount);
        }
        else {
            //assert(messages.size() == expected + m_LogMsgCount);
        }

        using TReqId = CRequestContext::TCount;
        map<TReqId, SRequestRec> req_by_id;
        for (const auto& s : messages) {
            if (old_format) {
                size_t p = s.find("request");
                if (p == NPOS) continue;
                TReqId rid = NStr::StringToNumeric<TReqId>(s.substr(p + 8));
                req_by_id[rid].m_MsgCount++;
            }
            else {
                SDiagMessage m(s);
                auto& rec = req_by_id[m.m_RequestId];
                rec.m_MsgCount++;
                if (m.m_Flags & eDPF_AppLog) {
                    if (m.m_Event == SDiagMessage::eEvent_RequestStart) rec.m_Start = true;
                    if (m.m_Event == SDiagMessage::eEvent_RequestStop) rec.m_Stop = true;
                }
            }
        }
        for (const auto& rec : req_by_id) {
            if (rec.second.m_Start) { // main thread has no request-start and different number of messages
                //assert(rec.second.m_MsgCount == msg_per_request ||
                //    (m_AsyncDiscard && rec.second.m_MsgCount <= msg_per_request));
            }
            if (!old_format) {
                //assert(rec.second.m_Start == rec.second.m_Stop);
            }
        }
    }
    else {
        assert(messages.size() == s_NumThreads * 3 + m_LogMsgCount);
        if (old_format) {
            x_TestOldFormat(messages);
        }
        else {
            x_TestNewFormat(messages);
        }
    }

    // Cleaunp
    SetDiagStream(0);

    NcbiCout << "Test completed successfully!"
             << NcbiEndl << NcbiEndl;
    return true;
}


void CTestDiagApp::x_TestOldFormat(TStringList& messages)
{
    // Verify "created" messages
    for (unsigned int i=0; i<s_NumThreads; i++) {
        TStringList::iterator it = find(
            messages.begin(),
            messages.end(),
            "Note[E]: Thread " + NStr::IntToString(i) + " created");
        assert(it != messages.end());
        messages.erase(it);
    }
    assert(messages.size() == s_NumThreads*2+m_LogMsgCount);

    // Verify "Error" messages
    for (unsigned int i=0; i<s_NumThreads; i++) {
        TStringList::iterator it = find(
            messages.begin(),
            messages.end(),
            "Error: ERROR message from thread " + NStr::IntToString(i));
        assert(it != messages.end());
        messages.erase(it);
    }
    assert(messages.size() == s_NumThreads+m_LogMsgCount);

    // Verify "NOTE" messages
    for (unsigned int i=0; i<s_NumThreads; i++) {
        TStringList::iterator it = find(
            messages.begin(),
            messages.end(),
            "Note[E]: NOTE message from thread " + NStr::IntToString(i));
        assert(it != messages.end());
        messages.erase(it);
    }
    assert(messages.size() == m_LogMsgCount);
}


void CTestDiagApp::x_TestNewFormat(TStringList& messages)
{
    CDiagContext::TUID uid = GetDiagContext().GetUID();
    CDiagContext::TPID pid = GetDiagContext().GetPID();
    // string app_name = GetProgramDisplayName();
    size_t create_msg_count = 0;
    size_t err_msg_count = 0;
    size_t log_msg_count = 0;
    size_t other_msg_count = 0;

    ITERATE(TStringList, it, messages) {
        if ( it->empty() ) {
            continue;
        }
        SDiagMessage msg(*it);
        assert(msg.GetUID() == uid);
        assert(msg.m_PID == pid);

        string msg_text(msg.m_Buffer, msg.m_BufferLen);
        size_t pos = msg_text.find(" created");
        if (pos != NPOS  &&  msg_text.find("Thread ") == 0) {
            if ( msg.m_Function ) {
                assert(strcmp(msg.m_Function, "Thread_Init") == 0);
            }
            assert(msg.m_Line == s_CreateLine);
            create_msg_count++;
        }
        else if (msg_text.find("NOTE ") == 0) {
            pos = msg_text.find_last_of(" ");
            assert(pos != NPOS);
#if !defined(NCBI_NO_THREADS)
            assert(msg.m_TID != 0);
#endif
            if ( msg.m_Function ) {
                assert(strcmp(msg.m_Function, "Thread_Run") == 0);
            }
            assert(msg.m_Line == s_LogLine);
            log_msg_count++;
        }
        else if (msg_text.find("ERROR ") == 0) {
            pos = msg_text.find_last_of(" ");
            assert(pos != NPOS);
#if !defined(NCBI_NO_THREADS)
            assert(msg.m_TID != 0);
#endif
            if ( msg.m_Function ) {
                assert(strcmp(msg.m_Function, "Thread_Run") == 0);
            }
            assert(msg.m_Line == s_ErrLine);
            err_msg_count++;
        }
        else {
            other_msg_count++;
            continue;
        }
        assert(msg.m_Severity == eDiag_Error);
        if ( msg.m_File ) {
            assert(strcmp(msg.m_File, "test_ncbidiag_mt.cpp") == 0);
        }
        if ( msg.m_Module ) {
            assert(strcmp(msg.m_Module, "TEST") == 0);
        }
        if ( msg.m_Class ) {
            assert(strcmp(msg.m_Class, "CTestDiagApp") == 0);
        }
    }
    _VERIFY(err_msg_count == s_NumThreads);
    _VERIFY(create_msg_count == s_NumThreads);
    _VERIFY(log_msg_count == s_NumThreads);
    _VERIFY(other_msg_count == m_LogMsgCount);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestDiagApp().AppMain(argc, argv);
}
