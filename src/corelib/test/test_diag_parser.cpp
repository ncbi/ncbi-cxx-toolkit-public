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
 *   Test for NCBI diag parser
 *
 */

#define NCBI_MODULE TEST_DIAG_PARSER

#include <ncbi_pch.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiapp.hpp>

#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


////////////////////////////////
// Test application
//

class CDiagParserApp : public CNcbiApplication
{
public:
    CDiagParserApp(void);
    ~CDiagParserApp(void);

    void Init(void);
    int  Run(void);
private:
    void x_CheckMessage(TDiagPostFlags flags);
    void x_CheckMessage(const string& message);

    int        m_Count;
};


CDiagParserApp::CDiagParserApp(void)
    : m_Count(0)
{
}


CDiagParserApp::~CDiagParserApp(void)
{
}


void CDiagParserApp::Init(void)
{
    GetDiagContext().SetOldPostFormat(false);
}


const int kNumFlags = 12;
static TDiagPostFlags s_TestFlags[kNumFlags] = {
    eDPF_File + eDPF_LongFilename,
    eDPF_Line,
    eDPF_Severity,
    eDPF_ErrorID,
    eDPF_DateTime,
    eDPF_Location,
    eDPF_PID,
    eDPF_TID,
    eDPF_SerialNo,
    eDPF_SerialNo_Thread,
    eDPF_Iteration,
    eDPF_UID
};

int CDiagParserApp::Run(void)
{
    for (int mask = 0; mask < (1 << kNumFlags); mask++) {
        TDiagPostFlags flags = 0;
        for (int bit = 0; bit < kNumFlags; bit++) {
            if ((mask & (1 << bit)) != 0) {
                flags += s_TestFlags[bit];
            }
        }
        x_CheckMessage(flags);
    }
    x_CheckMessage(eDPF_All);

    x_CheckMessage("");
    x_CheckMessage("@0123456789AB");
    x_CheckMessage("01/02/03 4:56:00");
    x_CheckMessage("Error:");
    x_CheckMessage("MODULE(error code (text only))");
    x_CheckMessage("@not_an_UID");
    x_CheckMessage("@not_an_UID__");
    x_CheckMessage("21/22/03");
    x_CheckMessage("simple message");
    return 0;
}


// WorkShop fails to compare string to char* or to
// initialize string with NULL pointer.
bool CmpStr(const char* val1, const string& val2)
{
    return val1 ? string(val1) == val2 : val2.empty();
}


void CDiagParserApp::x_CheckMessage(TDiagPostFlags flags)
{
    SetDiagPostAllFlags(flags);
    SetFastCGIIteration(m_Count + 1);

    CDiagCompileInfo info;
    string result;
    auto_ptr<SDiagMessage> msg;
    CTime post_time(CTime::eCurrent);
    try {
        char buffer[4096] = "";
        ostrstream str(buffer, 4096, ios::out);
        SetDiagStream(&str);

        info = DIAG_COMPILE_INFO;
        ERR_POST_EX(123, 45, "Error post message " + NStr::IntToString(++m_Count));

        result = string(str.str(), str.pcount());
        msg.reset(new SDiagMessage(result));
    }
    catch (...) {
        SetDiagStream(&NcbiCout);
        throw;
    }
    SetDiagStream(&NcbiCout);

    if ((flags & eDPF_File) != 0) {
        _ASSERT(CmpStr(msg->m_File, info.GetFile()));
    }
    else {
        _ASSERT(!msg->m_File);
    }

    if ((flags & eDPF_Line) != 0) {
        _ASSERT(msg->m_Line == size_t(info.GetLine() + 1));
    }
    else {
        _ASSERT(!msg->m_Line);
    }

    if ((flags & eDPF_Severity) != 0) {
        _ASSERT(msg->m_Severity == eDiag_Error);
    }
    else {
        _ASSERT(msg->m_Severity == eDiagSevMin);
    }

    if ((flags & eDPF_ErrorID) != 0) {
        _ASSERT(CmpStr(msg->m_Module, info.GetModule()));
        _ASSERT(msg->m_ErrCode == 123);
        _ASSERT(msg->m_ErrSubCode == 45);
    }
    else {
        _ASSERT(!msg->m_Module);
        _ASSERT(msg->m_ErrCode == 0);
        _ASSERT(msg->m_ErrSubCode == 0);
    }

    if ((flags & eDPF_DateTime) != 0) {
        _ASSERT(msg->GetTime() >= post_time);
    }

    if ((flags & eDPF_ErrCodeMessage) != 0) {
        //
    }

    if ((flags & eDPF_Location) != 0) {
        _ASSERT(CmpStr(msg->m_Class, info.GetClass()));
        _ASSERT(CmpStr(msg->m_Function, info.GetFunction()));
    }
    else {
        _ASSERT(!msg->m_Class);
        _ASSERT(!msg->m_Function);
    }

    if ((flags & eDPF_PID) != 0) {
        if ((flags & eDPF_TID) != 0) {
            _ASSERT(CThread::TID(msg->m_TID) == CThread::GetSelf());
        }
        else {
            _ASSERT(msg->m_TID == 0);
        }
    }
    else {
        _ASSERT(msg->m_PID == 0);
    }

    if ((flags & eDPF_SerialNo) != 0) {
        _ASSERT(msg->m_ProcPost == m_Count);
        if ((flags & eDPF_SerialNo_Thread) != 0) {
            _ASSERT(msg->m_ThrPost == m_Count);
        }
        else {
            _ASSERT(msg->m_ThrPost == 0);
        }
    }
    else {
    }

    if ((flags & eDPF_Iteration) != 0) {
        _ASSERT(msg->m_Iteration == m_Count);
    }
    else {
        _ASSERT(msg->m_Iteration == 0);
    }

    if ((flags & eDPF_UID) != 0) {
        _ASSERT(msg->GetUID() == GetDiagContext().GetUID());
    }
    else {
        _ASSERT(msg->GetUID() == 0);
    }
}


void CDiagParserApp::x_CheckMessage(const string& message)
{
    SDiagMessage dmsg(message);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    // Execute main application function
    return CDiagParserApp().AppMain(argc, argv, 0, eDS_Default, 0);
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2006/07/11 16:39:50  grichenk
 * Reset DiagStream on exceptions
 *
 * Revision 1.2  2006/05/01 18:45:28  grichenk
 * Fixed warnings
 *
 * Revision 1.1  2006/04/17 15:38:26  grichenk
 * Initial revision
 *
 * Revision 1.5  2005/07/05 18:53:09  ivanov
 * Enable some tests, seems that it has been accidentally commented out.
 *
 * Revision 1.4  2005/04/14 20:27:03  ssikorsk
 * Retrieve a class name and a method/function name if NCBI_SHOW_FUNCTION_NAME is defined
 *
 * Revision 1.3  2005/03/15 15:05:34  dicuccio
 * Fixed typo: pathes -> paths
 *
 * Revision 1.2  2004/12/13 14:40:28  kuznets
 * Test for severity filtering
 *
 * Revision 1.1  2004/09/21 18:15:46  kononenk
 * Added tests for "Diagnostic Message Filtering"
 *
 * ===========================================================================
 */
