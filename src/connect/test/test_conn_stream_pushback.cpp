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

#include "test_assert.h"

#include <connect/ncbi_util.h>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <util/stream_pushback.hpp>
#include <stdlib.h>
#include <time.h>


BEGIN_NCBI_SCOPE


static const size_t kBufferSize = 1024*1024;


static size_t s_Read(iostream& ios, char* buffer, size_t size)
{
    ios.read(buffer, size);
    size_t count = ios.gcount();
    if (count  &&  ios.eof())
        ios.clear();
    return count;
}


END_NCBI_SCOPE


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;
    size_t i, j, k;

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

    LOG_POST("Generating array of random data");
    unsigned int seed = argc > 1
        ? (unsigned int) atoi(argv[1]) : (unsigned int) time(0);
    LOG_POST("Seed = " << seed);
    srand(seed);
    char *buf1 = new char[kBufferSize + 1];
    char *buf2 = new char[kBufferSize + 2];
    for (j = 0; j < kBufferSize/1024; j++) {
        for (i = 0; i < 1024 - 1; i++)
            buf1[j*1024 + i] = "0123456789"[rand() % 10];
        buf1[j*1024 + 1024 - 1] = '\n';
    }
    buf1[kBufferSize] = '\0';

    LOG_POST("Sending data down to server");
    if (!(ios << buf1)) {
        ERR_POST("Error sending data");
        return 1;
    }

    LOG_POST("Doing random reads and pushbacks of the reply");
    char c = 0;
    size_t buflen = 0;
    for (k = 0; buflen < kBufferSize; k++) {
        size_t m = kBufferSize + 1 - buflen;
        if (m > 10)
            m /= 10;
        i = rand() % m + 1;

        if (buflen + i > kBufferSize + 1)
            i = kBufferSize + 1 - buflen;
        LOG_POST(Info << "Reading " << i << " byte(s)");
        j = s_Read(ios, &buf2[buflen], i);
        if (!ios.good() && !ios.eof()) {
            ERR_POST("Error receiving data");
            return 2;
        }
        if (j != i)
            LOG_POST("Bytes requested: " << i << ", received: " << j);
        if (c && buf2[buflen] != c) {
            LOG_POST(Error <<
                     "Mismatch, putback: " << c << ", read: " << buf2[buflen]);
            return 2;
        } else
            c = 0;

        if (ios.good() && rand() % 5 == 0 && j > 1) {
            c = buf2[buflen + j - 1];
            if (rand() & 1) {
                LOG_POST(Info << "Putback ('" << c << "')");
                ios.putback(c);
            } else {
                LOG_POST(Info << "Unget ('" << c << "')");
                ios.unget();
            }
            if (!ios.good()) {
                ERR_POST("Error putting a byte back");
                return 2;
            }
            j--;
        }
        _ASSERT(j > 0);

        buflen += j;
        i = rand() % j + 1;
        if (i != j || --i) {
            buflen -= i;
            LOG_POST(Info << "Pushing back " << i << " byte(s)");
            UTIL_StreamPushback(ios, &buf2[buflen], i);
            c = buf2[buflen];
        }
        //LOG_POST(Info << "Obtained " << buflen << " of " << kBufferSize);
    }

    LOG_POST(Info << buflen << " bytes obtained in " << k << " iteration(s)" <<
             (ios.eof() ? " (EOF)" : ""));
    buf2[buflen] = '\0';

    for (i = 0; i < kBufferSize; i++) {
        if (!buf2[i])
            break;
        if (buf2[i] != buf1[i])
            break;
    }

    if (i < kBufferSize) {
        ERR_POST("Not entirely bounced, mismatch position: " << i + 1);
        return 1;
    } else if (buflen > kBufferSize) {
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << buflen);
        return 1;
    } else
        LOG_POST(Info << "Test passed");
    
    CORE_SetREG(0);
    CORE_SetLOG(0);

    delete[] buf1;
    delete[] buf2;

    return 0/*okay*/;
}
