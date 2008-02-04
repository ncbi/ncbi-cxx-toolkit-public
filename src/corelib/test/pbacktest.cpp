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
 *   Test CStreamUtils::Pushback() interface.
 *
 */

#include <ncbi_pch.hpp>
#include "pbacktest.hpp"
#include <corelib/ncbidbg.hpp>
#include <corelib/stream_utils.hpp>
#include <vector>
#include <stdlib.h>
#include <time.h>

#include <common/test_assert.h>  /* This header must go last */

#define _STR(a) #a
#define  STR(a) _STR(a)

#ifdef   max
#  undef max
#endif
#define  max(a, b) ((a) > (b) ? (a) : (b))


BEGIN_NCBI_SCOPE


static const size_t kBufferSize = 512*1024;


/* NOTE about MSVC compiler and its C++ std. library:
 *
 * The C++ standard is very confusing on the stream's ability to do
 * pushback() and unget(); it says nothing about the case whether
 * they are guaranteed, and if so, under what circumstances.
 * The only clear message is seen in "The C++ Programming Language",
 * 3rd ed. by B.Stroustrup, p.644, which reads "what is guaranteed
 * is that you can back up one character after a successful read."
 *
 * Most implementation obey this; but there are some that do not.
 *
 * A bug or not a bug, we had to put putback and unget tests to only
 * follow a first pushback operation, which (internally) changes the
 * stream buffer and always keeps "usual backup condition", described in
 * the standard, thus allowing at least one-char backup after a read.
 *
 * So, if you plan to do a portable backup, which can possibly follow
 * an unformatted read, do so by using CStreamUtils::Pushback() instead of
 * standard means, as the latter may be broken.
 */

extern int TEST_StreamPushback(iostream&    ios,
                               unsigned int seed_in,
                               bool         rewind)
{
    size_t i, j, k;
    vector<CT_CHAR_TYPE*> v;

    unsigned int seed = seed_in ? seed_in : (unsigned int) time(0);
    LOG_POST("Seed = " << seed);
    srand(seed);

    LOG_POST("Generating array of random data");
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

    LOG_POST("Doing random reads and {push|step}backs of the reply");
    int d = 0;
    char c = 0;
    size_t busy = 0;
    size_t buflen = 0;
#ifdef NCBI_COMPILER_MSVC
    bool first_pushback_done = false;
#endif
    for (k = 0;  buflen < kBufferSize;  k++) {
        size_t m = kBufferSize + 1 - buflen;
        if (m >= 10)
            m /= 10;
        i = rand() % m + 1;

        if (buflen + i > kBufferSize + 1)
            i = kBufferSize + 1 - buflen;
        LOG_POST(Info << "Reading " << i << " byte" << (i == 1 ? "" : "s"));
        // Force at least minimal blocking, since Readsome might not
        // block at all and accepting 0-byte reads could lead to spinning.
        ios.peek();
        j = CStreamUtils::Readsome(ios, buf2 + buflen, i);
        if (!ios.good()) {
            ERR_POST("Error receiving data");
            return 2;
        }
        if (j != i)
            LOG_POST("Received " << j << " byte" << (j == 1 ? "" : "s"));
        assert(j > 0);
        if (c  &&  c != buf2[buflen]) {
            LOG_POST(Error <<
                     "Mismatch, putback: " << c << ", read: " << buf2[buflen]);
            return 2;
        } else
            c = 0;
        buflen += j;
        LOG_POST("Obtained so far " << buflen << " out of " << kBufferSize);

        bool pback = false;
        if (rewind  &&  rand() % 7 == 0  &&  buflen < kBufferSize) {
            if (rand() & 1) {
                LOG_POST(Info << "Testing pre-seekg(" << buflen << ", "
                         << STR(IOS_BASE) "::beg)");
                ios.seekg(buflen, IOS_BASE::beg);
            } else {
                LOG_POST(Info << "Testing pre-seekg(" << buflen << ')');
                ios.seekg(buflen);
            }
            if (!ios.good()) {
                ERR_POST("Error in stream re-positioning");
                return 2;
            }
        } else if (ios.good()  &&  rand() % 5 == 0  &&  j > 1) {
#ifdef NCBI_COMPILER_MSVC
            if (!rewind  ||  first_pushback_done) {
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
                pback = true;
#ifdef NCBI_COMPILER_MSVC
            }
#endif
        }

        if (!(rand() % 101)  &&  d++ < 5)
            i = rand() % buflen + 1;
        else
            i = rand() % j      + 1;

        if ((i != buflen  &&  i != j)  ||  --i) {
            buflen -= i;
            int how = rand() & 0x0F;
            int pushback = how & 0x01;
            int longform = how & 0x02;
            int original = how & 0x04;
            int passthru = how & 0x08;  // meaningful only if !original
            CT_CHAR_TYPE* p = buf2 + buflen;
            size_t slack = 0;
            if (!original) {
                slack = rand() & 0x7FF;  // slack space filled with garbage
                p = new CT_CHAR_TYPE[i + (slack << 1)];
                for (size_t ii = 0;  ii < slack;  ii++) {
                    p[            ii] = rand() % 26 + 'A';
                    p[slack + i + ii] = rand() % 26 + 'a';
                }
                memcpy(p + slack, buf2 + buflen, i);
            } else
                passthru = 0;
            LOG_POST(Info << (pushback ? "Pushing" : "Stepping") << " back "
                     << i << " byte" << &"s"[i == 1]
                     << (!longform
                         ? (original
                            ? ""
                            : " (copy)")
                         : (original
                            ? " (original)"
                            : (passthru
                               ? " (passthru copy)"
                               : " (retained copy)"))));
            switch (how &= 3) {
            case 0:
                CStreamUtils::Stepback(ios, p + slack, i);
                break;
            case 1:
                CStreamUtils::Pushback(ios, p + slack, i);
                break;
            case 2:
                CStreamUtils::Stepback(ios, p + slack, i, passthru ? p : 0);
                break;
            case 3:
                CStreamUtils::Pushback(ios, p + slack, i, passthru ? p : 0);
                if (!original  &&  !passthru)
                    v.push_back(p);
                break;
            }
            c = buf2[buflen];
            if (how ^ 3) {
                if (!original  &&  (!longform  ||  !passthru)) {
                    for (size_t ii = slack;  ii < slack + i;  ii++)
                        p[ii] = "%*-+="[rand() % 5];
                    delete[] p;
                }
                for (size_t ii = max(busy, buflen);  ii < buflen + i;  ii++)
                    buf2[ii] = "!@#$&"[rand() % 5];
            } else if (original  &&  busy < buflen + i)
                busy = buflen + i;
#ifdef NCBI_COMPILER_MSVC
            first_pushback_done = true;
#endif
            pback = true;
        }

        if (rewind  &&  rand() % 9 == 0  &&  buflen < kBufferSize) {
            if (pback) {
                buf2[buflen++] = c;
                c = 0;
            }
            if (rand() & 1) {
                LOG_POST(Info << "Tesing post-seekg(" << buflen << ')');
                ios.seekg(buflen);
            } else {
                LOG_POST(Info << "Tesing post-seekg(" << buflen << ", "
                         << STR(IOS_BASE) << "::beg)");
                ios.seekg(buflen, IOS_BASE::beg);
            }
            if (!ios.good()) {
                ERR_POST("Error in stream re-positioning");
                return 2;
            }
        }

        //LOG_POST(Info << "Obtained " << buflen << " of " << kBufferSize);
    }

    LOG_POST(Info << buflen << " bytes obtained in " << k << " iteration(s)" <<
             (ios.eof() ? " (EOF)" : ""));
    buf2[buflen] = '\0';

    for (i = 0;  i < kBufferSize;  i++) {
        if (!buf2[i]) {
            ERR_POST("Zero byte encountered at position " << i + 1);
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

    for (i = 0;  i < v.size();  i++)
        delete[] v[i];

    return 0/*okay*/;
}

END_NCBI_SCOPE
