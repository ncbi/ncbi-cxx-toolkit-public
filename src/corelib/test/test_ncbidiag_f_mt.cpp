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
 * Author:  Vyacheslav Kononenko
 *
 * File Description:
 *   Test for "NCBIDIAG" filter mechanism in multithreaded environment
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbidiag.hpp>
#include <algorithm>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


#define NUM_TESTS 9



/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestDiagApp : public CThreadedApp
{
public:
    void SetCase(int case_num);

    virtual bool Thread_Init(int idx);
    virtual bool Thread_Run (int idx);

protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
    virtual bool TestApp_Args(CArgDescriptions& args);

private:
    typedef pair<int,int>           TCase;
    typedef map<TCase, string>      TMessages;
    typedef set< pair<TCase,int> >  TThreads;

    void   x_SetExpects(const string& expects, const string& ex_expects);
    string x_MakeMessage(const string& msg, 
                         int           idx, 
                         const TCase&  ncase);
    void   x_PrintMessages(int         case_number, 
                           int         idx, 
                           const char* module, 
                           const char* nclass, 
                           const char* function);
    
    string    m_Case;
    TMessages m_Messages;
    TThreads  m_CaseExpected;
};


static CNcbiOstrstream s_Sout;

void CTestDiagApp::SetCase(int case_num)
{
    string filter;

    switch(case_num){
    case 1 :
        filter = string();
        x_SetExpects("111111111", "111111111");
        break;
    case 2 :
        filter = "module";
        x_SetExpects("111100000", "000000000");
        break;
    case 3 :
        filter = "::class";
        x_SetExpects("110011000", "000000000");
        break;
    case 4:
        filter = "function()";
        x_SetExpects("101010100", "000000000");
        break;
    case 5:
        filter = "?";
        x_SetExpects("000011110", "111111111");
        break;
    case 6:
        filter = "!foo";
        x_SetExpects("111111110", "111111111");
        break;
    case 7:
        filter = "/corelib";
        x_SetExpects("111111111", "111111111");
        break;
    case 8:
        filter = "/corelib/";
        x_SetExpects("000000000", "000000000");
        break;
    case 9:
        filter = "module::class::function()";
        x_SetExpects("100000000", "000000000");
        break;
    case 10:
        filter = "module::class";
        x_SetExpects("110000000", "000000000");
        break;
    case 11:
        filter = "::class::function()";
        x_SetExpects("100010000", "000000000");
        break;
    case 12:
        filter = "module foo";
        x_SetExpects("111100001", "000000000");
        break;
    case 13:
        filter = "::class ::boo";
        x_SetExpects("110011001", "000000000");
        break;
    case 14:
        // TWO spaces ...
        filter = "::class  ::boo";
        x_SetExpects("110011001", "000000000");
        break;
    case 15:
        filter = "function() doo()";
        x_SetExpects("101010101", "000000000");
        break;
    case 16:
        filter = "module ::boo";
        x_SetExpects("111100001", "000000000");
        break;
    case 17:
        filter = "module doo()";
        x_SetExpects("111100001", "000000000");
        break;
    case 18:
        filter = "::class doo()";
        x_SetExpects("110011001", "000000000");
        break;
    case 19:
        filter = "!module";
        x_SetExpects("000011111", "111111111");
        break;
    case 20:
        filter = "!::class";
        x_SetExpects("001100111", "111111111");
        break;
    case 21:
        filter = "!function()";
        x_SetExpects("010101011", "111111111");
        break;
    case 22:
        filter = "!module foo";
        x_SetExpects("000000001", "000000000");
        break;
    case 23:
        filter = "!module ::class";
        x_SetExpects("000011000", "000000000");
        break;
    case 24:
        filter = "!module function()";
        x_SetExpects("000010100", "000000000");
        break;
    case 25:
        filter = "!::class module";
        x_SetExpects("001100000", "000000000");
        break;
    case 26:
        filter = "!function() module";
        x_SetExpects("010100000", "000000000");
        break;
    case 27:
        filter = "module !foo";
        x_SetExpects("111100000", "000000000");
        break;
    case 28:
        filter = "module !::class";
        x_SetExpects("001100000", "000000000");
        break;
    case 29:
        filter = "module !function()";
        x_SetExpects("010100000", "000000000");
        break;
    case 30:
        filter = "!module !foo";
        x_SetExpects("000011110", "111111111");
        break;
    case 31:
        filter = "!module !::class";
        x_SetExpects("000000111", "111111111");
        break;
    case 32:
        filter = "!module !function()";
        x_SetExpects("000001011", "111111111");
        break;
    case 33:
        filter = "!foo !module";
        x_SetExpects("000011110", "111111111");
        break;
    case 34:
        filter = "!::class !module";
        x_SetExpects("000000111", "111111111");
        break;
    case 35:
        filter = "!function() !module";
        x_SetExpects("000001011", "111111111");
        break;
    case 36:
        filter = "!/corelib";
        x_SetExpects("000000000", "000000000");
        break;
    case 37:
        filter = "!/corelib/";
        x_SetExpects("111111111", "111111111");
        break;
    case 38:
        filter = "!function() !module !foo";
        x_SetExpects("000001010", "111111111");
        break;
    case 39:
        filter = "!/corelib/test";
        x_SetExpects("000000000", "000000000");
        break;
    case 40:
        filter = "!/corelib/test/";
        x_SetExpects("000000000", "000000000");
        break;
    case 41:
        filter = "/corelib !/dbapi";
        x_SetExpects("111111111", "111111111");
        break;
    case 42:
        filter = "!/dbapi /corelib";
        x_SetExpects("111111111", "111111111");
        break;
    case 43:
        filter = "!/dbapi !/corelib";
        x_SetExpects("000000000", "000000000");
        break;
    case 44:
        filter = "/dbapi !/corelib";
        x_SetExpects("000000000", "000000000");
        break;
    case 45:
        filter = "!/corelib /dbapi";
        x_SetExpects("000000000", "000000000");
        break;
    case 46:
        filter = "!/dbapi !/corelib/";
        x_SetExpects("111111111", "111111111");
        break;
    case 47:
        filter = "!/corelib/ !/dbapi";
        x_SetExpects("111111111", "111111111");
        break;
    }

    if ( !filter.empty() ) {
        SetDiagFilter(eDiagFilter_All, filter.c_str());
    } else {
        filter = "no filter";
    }
    m_Case = "filter \"" + filter + "\"";

}


void CTestDiagApp::x_SetExpects(const string& expects, const string& ex_expects)
{
    for (int i = 0;  i < NUM_TESTS;  ++i) {
        if (expects[i] == '1') {
            for (unsigned int thr = 0;  thr < s_NumThreads;  ++thr)
                m_CaseExpected.insert(
                    TThreads::value_type(TCase(i, 0), thr) );
        }
        if (ex_expects[i] == '1') {
            for (int num = 1;  num < 3;  ++num) 
                for (unsigned int thr = 0;  thr < s_NumThreads;  ++thr)
                    m_CaseExpected.insert(
                        TThreads::value_type(TCase(i, num), thr) );
        }
    }
} 


bool CTestDiagApp::Thread_Init(int /* idx */)
{
    return true;
}


string CTestDiagApp::x_MakeMessage(const string& msg, 
                                   int           idx, 
                                   const TCase&  ncase)
{
    return string("!=") + NStr::IntToString(ncase.first) + " " 
        + NStr::IntToString(ncase.second) + " " 
        + NStr::IntToString(idx) + " "
        + msg + "=!";
}


void CTestDiagApp::x_PrintMessages(int         test_number,
                                   int         idx, 
                                   const char* module, 
                                   const char* nclass, 
                                   const char* function)
{
    string location  = string(module) + "::" + nclass + "::" +
        function + "()";
    string postmsg   = location + " in ERR_POST";
    string exceptmsg = location + " in the one level exception"; 
    string secondmsg = location + " in the two levels exception";

    if (idx == 0) {
        m_Messages[ TCase( test_number, 0 ) ] = postmsg;
        m_Messages[ TCase( test_number, 1 ) ] = exceptmsg;
        m_Messages[ TCase( test_number, 2 ) ] = secondmsg;
    }

    // ERR_POST
    ERR_POST(x_MakeMessage(postmsg, idx, TCase(test_number,0))
             << MDiagModule(module) 
             << MDiagClass(nclass) 
             << MDiagFunction(function));
    
    // one level exception
    try {
        NCBI_EXCEPTION_VAR(expt, CException, eUnknown, "exception");
        expt.SetModule(module);
        expt.SetClass(nclass);
        expt.SetFunction(function);
        NCBI_EXCEPTION_THROW(expt);
    }
    catch (const  CException& ex) {
        NCBI_REPORT_EXCEPTION(x_MakeMessage(exceptmsg, 
                                            idx, 
                                            TCase(test_number, 1)), ex);
    }


#if defined(NCBI_COMPILER_WORKSHOP)
# if NCBI_COMPILER_VERSION == 530 || NCBI_COMPILER_VERSION == 550
    // Workshop 5.3 and 5.5 have MT-unsafe throw. To avoid test failures
    // use mutex.
    DEFINE_STATIC_FAST_MUTEX(s_ThrowMutex);
    CFastMutexGuard guard(s_ThrowMutex);
# endif
#endif


    // two level exceptions
    try {
        try {
            NCBI_EXCEPTION_VAR(expt, CException, eUnknown, "exception1");
            expt.SetModule(module);
            expt.SetClass(nclass);
            expt.SetFunction(function);
            NCBI_EXCEPTION_THROW(expt);
        }
        catch (const  CException& ex) {
            NCBI_EXCEPTION_VAR_EX(e2, &ex, CException, eUnknown, "exception2");
            e2.SetModule(module);
            e2.SetClass(nclass);
            e2.SetFunction(function);
            NCBI_EXCEPTION_THROW(e2);
        }
    }
    catch (const  CException& ex) {
        NCBI_REPORT_EXCEPTION(x_MakeMessage(secondmsg, 
                                            idx, 
                                            TCase(test_number, 2)), ex);
    }
}


bool CTestDiagApp::Thread_Run(int idx)
{
    int testnum = 0;

    x_PrintMessages(testnum++, idx, "module", "class", "function");
    x_PrintMessages(testnum++, idx, "module", "class", ""        );
    x_PrintMessages(testnum++, idx, "module", "",      "function");
    x_PrintMessages(testnum++, idx, "module", "",      ""        );
    x_PrintMessages(testnum++, idx, "",       "class", "function");
    x_PrintMessages(testnum++, idx, "",       "class", ""        );
    x_PrintMessages(testnum++, idx, "",       "",      "function");
    x_PrintMessages(testnum++, idx, "",       "",      ""        );
    x_PrintMessages(testnum++, idx, "foo",    "boo",   "doo"     );

    return true;
}


bool CTestDiagApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddPositional("case_number",
                       "case number",
                       CArgDescriptions::eInteger);

    args.SetConstraint("case_number",
                       new CArgAllow_Integers(1, 47));

    return true;
}


bool CTestDiagApp::TestApp_Init(void)
{
    // Get arguments
    const CArgs& args = GetArgs();
    SetCase( args["case_number"].AsInteger() );

    NcbiCout << NcbiEndl
             << "Testing NCBIDIAG FILTERING case ("
             << m_Case << ") with "
             << NStr::IntToString(s_NumThreads)
             << " threads..."
             << NcbiEndl;

    SetDiagPostFlag(eDPF_Location);
    SetDiagPostFlag(eDPF_MergeLines);
    // Output to the string stream -- to verify the result
    SetDiagStream(&s_Sout);

    return true;
}


bool CTestDiagApp::TestApp_Exit(void)
{
    // Cleanup
    SetDiagStream(0);

    // Verify the result
    typedef list<string> TStrings;
    TStrings messages;

    // Get the list of messages
    CTempString ts(s_Sout.str(), (size_t)s_Sout.pcount());
    NStr::Split(ts, "\r\n", messages);
    s_Sout.freeze(false);

    bool result = true;
    ITERATE(TStrings, i, messages) {
        size_t beg = i->find("!=");
        size_t end = i->find("=!");
        if (beg == string::npos  ||  end == string::npos)
            continue;

        string          full_msg = i->substr(beg + 2, end - beg - 2);
        CNcbiIstrstream in( full_msg.c_str() );
        TCase           ncase;
        int             thread;
        string          msg;
        
        in >> ncase.first >> ncase.second >> thread;
        getline(in, msg);

        if ( !m_CaseExpected.erase(make_pair(ncase, thread)) ) {
            NcbiCout << "Error: unexpected message \"" 
                     << msg << "\" for thread " << thread << NcbiEndl;
            result = false;
        }
        
    }

    ITERATE(TThreads, i, m_CaseExpected) {
        NcbiCout << "Error: message \"" << m_Messages[ i->first ]
                 << "\" for thread " << i->second << " not found"
                 << NcbiEndl;
        result = false;
    }

    if (result) {
        NcbiCout << "Test completed successfully!"
                 << NcbiEndl << NcbiEndl;
    }

    return result;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestDiagApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
