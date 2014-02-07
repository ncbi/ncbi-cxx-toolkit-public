/* $Id$
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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Test CStreamUtils::Pushback() interface.
 *
 */

#include <ncbi_pch.hpp>
#include "pbacktest.hpp"
#include <corelib/ncbidiag.hpp>
#include <corelib/test_mt.hpp>
#include <stdio.h>                 // remove()
/* This header must go last */
#include <common/test_assert.h>


BEGIN_NCBI_SCOPE


class CTestApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);

private:
    static const char sm_Filename[];
};


const char CTestApp::sm_Filename[] = "test_fstream_pushback.data";


bool CTestApp::Thread_Run(int idx)
{
    string id = NStr::IntToString(idx);

    string filename = sm_Filename + string(1, '.') + id;

    PushDiagPostPrefix(("@" + id).c_str());

    CNcbiFstream fs(filename.c_str(),
                    IOS_BASE::in    | IOS_BASE::out   |
                    IOS_BASE::trunc | IOS_BASE::binary);

    int ret = TEST_StreamPushback(fs, true/*rewind*/);

    PopDiagPostPrefix();

    if (ret)
        return false;

    remove(filename.c_str());
    return true;
}


END_NCBI_SCOPE


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;

    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags((SetDiagPostAllFlags(eDPF_Default) & ~eDPF_All)
                        | eDPF_DateTime    | eDPF_Severity
                        | eDPF_OmitInfoSev | eDPF_ErrorID
                        | eDPF_Prefix);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

    s_NumThreads = 2; // default is small

    return CTestApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
