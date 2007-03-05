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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Test UTIL_PushbackStream() interface.
 *
 */

#include <ncbi_pch.hpp>
#include "../../corelib/test/pbacktest.hpp"
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_util.h>
/* This header must go last */
#include "test_assert.h"


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;

    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_All);

    string host = "www.ncbi.nlm.nih.gov";
    string path = "/Service/bounce.cgi";
    string args = kEmptyStr;
    string uhdr = kEmptyStr;

    LOG_POST("Creating HTTP connection to http://" + host + path + args);
    CConn_HttpStream ios(host, path, args, uhdr);

    int n = TEST_StreamPushback(ios,
                                argc > 1 ? (unsigned int) atoi(argv[1]) : 0,
                                false/*no rewind*/);

    CORE_SetREG(0);
    CORE_SetLOG(0);
    CORE_SetLOCK(0);

    return n;
}
