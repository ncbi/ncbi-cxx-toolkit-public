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
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 1.6  2002/04/15 19:21:44  lavr
 * +#include "../test/test_assert.h"
 *
 * Revision 1.5  2002/01/29 16:03:17  lavr
 * Redesigned to use xpbacktest
 *
 * Revision 1.4  2002/01/28 20:28:08  lavr
 * Redesigned; pushbacks and standard putbacks and ungets are used together
 *
 * Revision 1.3  2002/01/16 21:23:14  vakatov
 * Utilize header "test_assert.h" to switch on ASSERTs in the Release mode too
 *
 * Revision 1.2  2001/12/17 22:18:56  ucko
 * Make wrapper for read() more robust, and enable it on more platforms.
 *
 * Revision 1.1  2001/12/07 22:59:38  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include "../../util/test/pbacktest.hpp"
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>
/* This header must go last */
#include "test_assert.h"


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;

    CORE_SetLOG(LOG_cxx2c());
    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_All);

    string host = "ray";
    string path = "/Service/bounce.cgi";
    string args = kEmptyStr;
    string uhdr = kEmptyStr;

    LOG_POST("Creating HTTP connection to http://" + host + path + args);
    CConn_HttpStream ios(host, path, args, uhdr);

    int n = TEST_StreamPushback(ios,
                                argc > 1 ? (unsigned int) atoi(argv[1]) : 0,
                                false/*no rewind*/);

    CORE_SetLOG(0);
    return n;
}
