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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *   Test for base64url_encode() / base64url_decode().
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbi_base64.h>
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;

#define MAX_SRC_SEQ_LEN 3
#define STEP 100

typedef unsigned TBitArray;

BOOST_AUTO_TEST_CASE(TestBase64URL)
{
    _ASSERT(sizeof(TBitArray) >= MAX_SRC_SEQ_LEN);

    char original_buffer[sizeof(TBitArray)];
    char encoded_buffer[(sizeof(TBitArray) * 3) >> 1];
    char decoded_buffer[sizeof(TBitArray)];

    size_t encoded_len, decoded_len;

    for (int len = 1; len <= 2; ++len) {
        TBitArray bits = ((1 << ((len - 1) << 3)) - 1) << 8 | 0xFF;

        size_t expected_encoded_len = ((len << 2) + 2) / 3;

        for (;;) {
            for (int byte = 0; byte < len; ++byte)
                original_buffer[byte] = char((bits >> (byte >> 3)) & 0xFF);

            BOOST_CHECK(base64url_encode(NULL, len,
                    NULL, 0, &encoded_len) == eBase64_BufferTooSmall);

            BOOST_CHECK(encoded_len == expected_encoded_len);

            BOOST_CHECK(base64url_encode(original_buffer, len,
                    encoded_buffer, encoded_len, NULL) == eBase64_OK);

            BOOST_CHECK(base64url_decode(NULL, encoded_len,
                    NULL, 0, &decoded_len) == eBase64_BufferTooSmall);

            BOOST_CHECK((int) decoded_len == len);

            BOOST_CHECK(base64url_decode(encoded_buffer, encoded_len,
                    decoded_buffer, decoded_len, NULL) == eBase64_OK);

            BOOST_CHECK(memcmp(original_buffer, decoded_buffer, len) == 0);

            if (bits < STEP)
                break;

            bits -= STEP;
        }
    }
}

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
            "base64url Unit Test");
}
