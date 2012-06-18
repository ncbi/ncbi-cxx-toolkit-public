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
};


static char s_Str[k_NumThreadsMax*200] = "";
static ostrstream* s_Sout;
static size_t s_CreateLine = 0;
static size_t s_LogLine = 0;
static size_t s_ErrLine = 0;


bool CTestDiagApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddOptionalKey("format", "Format", "Log format",
        CArgDescriptions::eString);
    args.SetConstraint("format", &(*new CArgAllow_Strings, "old", "new"));
    return true;
}


bool CTestDiagApp::Thread_Init(int idx)
{
    if (!s_CreateLine) s_CreateLine = __LINE__ + 1;
    LOG_POST("Thread " + NStr::IntToString(idx) + " created");
    return true;
}

bool CTestDiagApp::Thread_Run(int idx)
{
    if (!s_LogLine)
        s_LogLine = __LINE__ + 1;
    LOG_POST("LOG message from thread " + NStr::IntToString(idx));
    if (!s_ErrLine)
        s_ErrLine = __LINE__ + 1;
    ERR_POST("ERROR message from thread " + NStr::IntToString(idx));
    for ( int i = 0; i < 1000000; ++i ) {
        CThreadSystemID::GetCurrent();
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
    NcbiCout << NcbiEndl
             << "Testing NCBIDIAG with "
             << NStr::IntToString(s_NumThreads)
             << " threads ("
             << (GetDiagContext().IsSetOldPostFormat() ? "old" : "new")
             << " format)..."
             << NcbiEndl;
    // Output to the string stream -- to verify the result
    s_Sout = new ostrstream(s_Str, k_NumThreadsMax*200, ios::out);
    SetDiagStream(s_Sout);
    return true;
}

bool CTestDiagApp::TestApp_Exit(void)
{
    // Verify the result
    string test_res(s_Str);
    TStringList messages;

    // Get the list of messages and check the size
    NStr::Split(test_res, "\r\n", messages);

    assert(messages.size() == s_NumThreads*3+m_LogMsgCount);

    if (GetDiagContext().IsSetOldPostFormat()) {
        x_TestOldFormat(messages);
    }
    else {
        x_TestNewFormat(messages);
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
            "Thread " + NStr::IntToString(i) + " created");
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

    // Verify "Log" messages
    for (unsigned int i=0; i<s_NumThreads; i++) {
        TStringList::iterator it = find(
            messages.begin(),
            messages.end(),
            "LOG message from thread " + NStr::IntToString(i));
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
        else if (msg_text.find("LOG ") == 0) {
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
    assert(err_msg_count == s_NumThreads);
#ifdef _DEBUG
    assert(create_msg_count == s_NumThreads);
    assert(log_msg_count == s_NumThreads);
    assert(other_msg_count == m_LogMsgCount);
#endif
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestDiagApp().AppMain(argc, argv);
}
