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

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestDiagApp : public CThreadedApp
{
public:
    virtual bool Thread_Init(int idx);
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
};


static char s_Str[k_NumThreadsMax*200] = "";
static ostrstream s_Sout(s_Str, k_NumThreadsMax*200, ios::out);


bool CTestDiagApp::Thread_Init(int idx)
{
    LOG_POST("Thread " + NStr::IntToString(idx) + " created");
    return true;
}

bool CTestDiagApp::Thread_Run(int idx)
{
    LOG_POST("LOG message from thread " + NStr::IntToString(idx));
    ERR_POST("ERROR message from thread " + NStr::IntToString(idx));
    for ( int i = 0; i < 1000000; ++i ) {
        CThreadSystemID::GetCurrent();
        //CThread::GetSelf();
    }
    return true;
}

bool CTestDiagApp::TestApp_Init(void)
{
    NcbiCout << NcbiEndl
             << "Testing NCBIDIAG with "
             << NStr::IntToString(s_NumThreads)
             << " threads..."
             << NcbiEndl;
    // Output to the string stream -- to verify the result
    SetDiagStream(&s_Sout);

    return true;
}

bool CTestDiagApp::TestApp_Exit(void)
{
    // Verify the result
    string test_res(s_Str);
    typedef list<string> string_list;
    string_list messages;

    // Get the list of messages and check the size
    NStr::Split(test_res, "\r\n", messages);

    assert(messages.size() == s_NumThreads*3+1);

    // Verify "created" messages
    for (unsigned int i=0; i<s_NumThreads; i++) {
        string_list::iterator it = find(
            messages.begin(),
            messages.end(),
            "Thread " + NStr::IntToString(i) + " created");
        assert(it != messages.end());
        messages.erase(it);
    }
    assert(messages.size() == s_NumThreads*2+1);

    // Verify "Error" messages
    for (unsigned int i=0; i<s_NumThreads; i++) {
        string_list::iterator it = find(
            messages.begin(),
            messages.end(),
            "Error: ERROR message from thread " + NStr::IntToString(i));
        assert(it != messages.end());
        messages.erase(it);
    }
    assert(messages.size() == s_NumThreads+1);

    // Verify "Log" messages
    for (unsigned int i=0; i<s_NumThreads; i++) {
        string_list::iterator it = find(
            messages.begin(),
            messages.end(),
            "LOG message from thread " + NStr::IntToString(i));
        assert(it != messages.end());
        messages.erase(it);
    }
    assert(messages.size() == 1);

    // Cleaunp
    SetDiagStream(0);

    NcbiCout << "Test completed successfully!"
             << NcbiEndl << NcbiEndl;
    return true;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CTestDiagApp app;
    return app.AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 6.8  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.7  2003/05/17 15:00:39  grichenk
 * Fixed number of message lines
 *
 * Revision 6.6  2003/05/08 20:50:12  grichenk
 * Allow MT tests to run in ST mode using CThread::fRunAllowST flag.
 *
 * Revision 6.5  2002/09/19 20:05:43  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 6.4  2002/04/23 13:11:50  gouriano
 * test_mt.cpp/hpp moved into another location
 *
 * Revision 6.3  2002/04/16 18:49:08  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 6.2  2001/04/06 16:44:14  grichenk
 * Redesigned to use MT test suite API from "test_mt.[ch]pp"
 *
 * Revision 6.1  2001/03/30 22:46:59  grichenk
 * Initial revision
 *
 * ===========================================================================
 */
