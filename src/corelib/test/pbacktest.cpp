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
 * Revision 1.5  2002/02/05 21:45:19  lavr
 * Included header files rearranged
 *
 * Revision 1.4  2002/02/05 16:06:41  lavr
 * List of included header files revised; Use macro IOS_BASE instead of raw ios
 *
 * Revision 1.3  2002/02/04 20:23:40  lavr
 * Long comment and workaround for MSVC putback()/unget() bug/feature
 * Additional test for stream positioning during read back
 *
 * Revision 1.2  2002/01/30 20:07:13  lavr
 * Verbose message about the difference, not just a position number
 *
 * Revision 1.1  2002/01/29 16:02:19  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include "../../connect/test/test_assert.h"

#include "pbacktest.hpp"
#include <corelib/ncbidbg.hpp>
#include <util/stream_pushback.hpp>
#include <stdlib.h>
#include <time.h>

#define _GLUE2(a, b) a ## b
#define _GLUE1(a, b) _GLUE2(#a, b)
#define   GLUE(a, b) _GLUE1( a, b)


BEGIN_NCBI_SCOPE


static const size_t kBufferSize = 1024*1024;


static size_t s_Read(iostream& ios, char* buffer, size_t size)
{
    if (ios.eof())
        ios.clear();
    ios.read(buffer, size);
    size_t count = ios.gcount();
    return count;
}


/* NOTE about MSVC compiler and its C++ std. library:
 *
 * The C++ standard is very confusing on the stream's ability to do
 * pushback() and unget(); it says nothing about the case whether
 * they are guaranteed, and if so, under what circumstances.
 * The only clear message is seen in "The C++ Programming Language",
 * 3rd ed. by B.Stroustrup, p.644, which reads "what is guaranteed
 * is that you can back up one character after a successful read."
 * Most of the implementations obey this; but MSVC's one does not.
 * If you do only unformatted reads from the file, your program gets data
 * via unbuffered stream (!) because basic_filebuf::uflow() gets control
 * and never allocates internal filebuf buffer; whereas in formatted
 * input filebuf::underflow() is used, and it does proper buffer allocation,
 * which in turn allows the backup after a read.
 * A bug or not a bug, we had to put putback and unget tests to only
 * follow a first pushback operation, which (internally) changes the
 * stream buffer and always keeps "usual backup condition", described in
 * the standard, thus allowing at least one-char backup after a read.
 *
 * So, if you plan to do a portable backup, which can possibly follow
 * an unformatted read, do so by using UTIL_StreamPushback() instead of
 * standard means, as the latter may be broken.
 */

extern int TEST_StreamPushback(iostream&    ios,
                               unsigned int seed_in,
                               bool         rewind)
{
    size_t i, j, k;

    LOG_POST("Generating array of random data");
    unsigned int seed = seed_in ? seed_in : (unsigned int) time(0);
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

    LOG_POST("Sending data down the stream");
    if (!(ios << buf1)) {
        ERR_POST("Could not send data");
        return 1;
    }
    if (rewind)
        ios.seekg(0);

    LOG_POST("Doing random reads and pushbacks of the reply");
    char c = 0;
    size_t buflen = 0;
#ifdef NCBI_COMPILER_MSVC
    bool first_pushback_done = false;
#endif
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
        _ASSERT(j > 0);
        if (c && buf2[buflen] != c) {
            LOG_POST(Error <<
                     "Mismatch, putback: " << c << ", read: " << buf2[buflen]);
            return 2;
        } else
            c = 0;
        buflen += j;

        if (rewind && rand() % 7 == 0 && buflen < kBufferSize) {
            LOG_POST(Info << "Testing seekg(" << buflen <<
                     ", " << GLUE(IOS_BASE, "::beg)"));
            ios.seekg(buflen, IOS_BASE::beg);
            if (!ios.good()) {
                ERR_POST("Error in stream re-positioning");
                return 2;
            }
        } else if (ios.good() && rand() % 5 == 0 && j > 1) {
#ifdef NCBI_COMPILER_MSVC
            if (!rewind || first_pushback_done) {
#endif
                c = buf2[--buflen];
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
#ifdef NCBI_COMPILER_MSVC
            }
#endif
        }

        i = rand() % j + 1;
        if (i != j || --i) {
            buflen -= i;
            LOG_POST(Info << "Pushing back " << i << " byte(s)");
            UTIL_StreamPushback(ios, &buf2[buflen], i);
            c = buf2[buflen];
#ifdef NCBI_COMPILER_MSVC
            first_pushback_done = true;
#endif
        }
        //LOG_POST(Info << "Obtained " << buflen << " of " << kBufferSize);
    }

    LOG_POST(Info << buflen << " bytes obtained in " << k << " iteration(s)" <<
             (ios.eof() ? " (EOF)" : ""));
    buf2[buflen] = '\0';

    for (i = 0; i < kBufferSize; i++) {
        if (!buf2[i]) {
            ERR_POST("Zero byte within encountered at position " << i + 1);
            return 1;
        }
        if (buf2[i] != buf1[i]) {
            ERR_POST("In: '" << buf1[i] << "', Out: '" << buf2[i] << '\''
                     << " at position " << i + 1);
            return 1;
        }
    }
    if (buflen > kBufferSize) {
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << buflen);
        return 1;
    } else
        LOG_POST(Info << "Test passed");

    delete[] buf1;
    delete[] buf2;

    return 0/*okay*/;
}

END_NCBI_SCOPE
