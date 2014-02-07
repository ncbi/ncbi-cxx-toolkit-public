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
 *   Test MD5 HMAC generation
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>
#include <connect/ncbi_util.h>
#include <util/md5.hpp>
#include <string.h>

#include "test_assert.h"  // This header must go last


BEGIN_NCBI_SCOPE


extern "C" {

static int/*bool*/ x_Init(void** ctx)
{
    if (!ctx)
        return 0;
    *ctx = new CMD5;
    return 1/*true*/;
}


static void x_Update(void* ctx, const void* data, size_t data_len)
{
    CMD5* md5 = (CMD5*) ctx;
    if (md5  &&  data)
        md5->Update((const char*) data, data_len);
}


static void x_Fini(void* ctx, void* digest)
{
    CMD5* md5 = (CMD5*) ctx;
    if (md5  &&  digest)
        md5->Finalize((unsigned char*) digest);
    delete md5;
}

}

static const SHASH_Descriptor md5_hash = {
    64 /*block_len*/,
    16 /*digest_len*/,
    x_Init,
    x_Update,
    x_Fini
};


END_NCBI_SCOPE


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;

    // Setup error posting
    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Trace);
    SetDiagPostAllFlags(SetDiagPostAllFlags(eDPF_Default)
                        | eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default));

    // These are taken directly from RFC2104
    static const struct {
        const char* text;
        size_t      text_len;
        const char* key;
        size_t      key_len;
        const char* digest;
    } tests[] = {
        { "Hi There",
          8,
          "\x0b\x0b\x0b\x0b"
          "\x0b\x0b\x0b\x0b"
          "\x0b\x0b\x0b\x0b"
          "\x0b\x0b\x0b\x0b",
          16,
          "\x92\x94\x72\x7a"
          "\x36\x38\xbb\x1c"
          "\x13\xf4\x8e\xf8"
          "\x15\x8b\xfc\x9d"
        },
        { "what do ya want for nothing?",
          28,
          "Jefe",
          4,
          "\x75\x0c\x78\x3e"
          "\x6a\xb0\xb5\x03"
          "\xea\xa8\x6e\x31"
          "\x0a\x5d\xb7\x38"
        },
        { "\xDD\xDD\xDD\xDD\xDD"
          "\xDD\xDD\xDD\xDD\xDD"
          "\xDD\xDD\xDD\xDD\xDD"
          "\xDD\xDD\xDD\xDD\xDD"
          "\xDD\xDD\xDD\xDD\xDD"
          "\xDD\xDD\xDD\xDD\xDD"
          "\xDD\xDD\xDD\xDD\xDD"
          "\xDD\xDD\xDD\xDD\xDD"
          "\xDD\xDD\xDD\xDD\xDD"
          "\xDD\xDD\xDD\xDD\xDD",
          50,
          "\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA"
          "\xAA\xAA\xAA\xAA",
          16,
          "\x56\xbe\x34\x52"
          "\x1d\x14\x4c\x88"
          "\xdb\xb8\xc7\x33"
          "\xf0\xe8\xb3\xf6"
        }
    };

    LOG_POST(Info << "Testing MD5 HMAC generation");

    for (size_t i = 0;  i < sizeof(tests) / sizeof(tests[0]);  ++i) {
        unsigned char digest[16];
        LOG_POST(Info << "Test " << i + 1);
        _ASSERT(UTIL_GenerateHMAC(&md5_hash,
                                  tests[i].text,
                                  tests[i].text_len,
                                  tests[i].key,
                                  tests[i].key_len,
                                  digest));
        _ASSERT(memcmp(digest, tests[i].digest, sizeof(digest)) == 0);
    }

    LOG_POST(Info << "All tests completed successfully");
    return 0;
}
