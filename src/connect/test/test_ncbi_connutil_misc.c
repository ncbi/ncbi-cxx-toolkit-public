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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Tests for "ncbi_connutil.c"
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2000/03/24 22:53:38  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <connect/ncbi_util.h>
#include <connect/ncbi_connutil.h>

#include <string.h>


/***********************************************************************
 *  TEST:  URL_Encode(), URL_Decode()
 */

static void TEST_URL_Encoding(void)
{
    typedef struct {
        const char* src_buf;
        size_t      src_size;
        size_t      src_read;
        const char* dst_buf;
        size_t      dst_size;
        size_t      dst_written;
        int/*bool*/ ok;
    } STestArg;

    static const STestArg s_TestEncode[] = {
        { "",           0, 0,  "",    0, 0, 1/*true*/ },
        { "abc",        3, 3,  "abc", 3, 3, 1/*true*/ },
        { "_ _%_;_\n_", 7, 0,  "",    0, 0, 1/*true*/ },
        { "_ _%_;_\n_", 0, 0,  "",    0, 0, 1/*true*/ },
        { "_ _%_;_\n_", 0, 0,  "",    7, 0, 1/*true*/ },
        { "_ _%_;_\n_:_\\_\"_", 15, 15,
          "_+_%25_%3B_%0A_%3A_%5C_%22_", 27, 27, 1/*true*/ },
        { "_ _%_;_\n_:_\\_\"_", 15, 13,
          "_+_%25_%3B_%0A_%3A_%5C_%22_", 25, 23, 1/*true*/ },
        { "_%_", 3, 1,  "_%25_", 2, 1, 1/*true*/ },
        { "_ _%_;_\n_", 7, 7,
          "_+_%25_%3B_%0A", 100, 11, 1/*true*/ }
    };

    static const STestArg s_TestDecode[] = {
        { "",    0, 0,   "", 0, 0,  1/*true*/ },
        { "%25", 1, 0,   "", 0, 0,  1/*true*/ },
        { "%25", 2, 0,   "", 0, 0,  1/*true*/ },
        { "%25", 3, 3,  "%", 1, 1,  1/*true*/ },
        { "%25", 3, 0,  "%", 0, 0,  1/*true*/ },
        { "%%%", 2, 0,   "", 1, 0,  0/*false*/ },
        { "%%%", 3, 0,   "", 1, 0,  0/*false*/ },
        { "%xy", 3, 0,   "", 1, 0,  0/*false*/ },
        { "\n",  1, 0,   "", 1, 0,  0/*false*/ },
        { "a\t", 2, 1,  "a", 1, 1,  1/*true*/ },
        { "#\n", 1, 0,   "", 0, 0,  1/*true*/ },
        { "%a-", 3, 0,   "", 1, 0,  0/*false*/ },
        { "%a-", 3, 0,   "", 0, 0,  1/*true*/ },
        { "_+_%25_%3B_%0A_%3A_%5C_%22_", 27, 27,
          "_ _%_;_\n_:_\\_\"_", 15, 15, 1/*true*/ },
        { "_+_%25_%3B_%0A_%3A_%5C_%22_", 25, 23,
          "_ _%_;_\n_:_\\_\"_", 13, 13, 1/*true*/ },
        { "_+_%25_%3B_%0A_%3A_%5C_%22_", 27, 23,
          "_ _%_;_\n_:_\\_\"_", 13, 13, 1/*true*/ }
    };

    static const STestArg s_TestDecodeEx[] = {
        { "",    0, 0,    "", 0, 0,  1/*true*/ },
        { "%25", 3, 0,   "%", 0, 0,  1/*true*/ },
        { "%%%", 2, 0,    "", 1, 0,  0/*false*/ },
        { "%xy", 3, 0,    "", 1, 0,  0/*false*/ },
        { "\n",  1, 0,    "", 1, 0,  0/*false*/ },
        { ">>a", 3, 3, ">>a", 3, 3,  1/*true*/ },
        { ">b[", 3, 3, ">b[", 4, 3,  1/*true*/ },
        { ">b]", 3, 2, ">b",  3, 2,  1/*true*/ },
        { "[b]", 3, 2, "[b",  3, 2,  1/*true*/ },
        { "<b>", 3, 0,   "",  3, 0,  0/*false*/ },
        { "<e>", 3, 0,   "",  5, 0,  0/*false*/ }
    };

    size_t i;
    size_t src_read, dst_written;
    char   dst[1024];

#define ARRAY_DIM(arr) (sizeof(arr)/sizeof((arr)[0]))

    for (i = 0;  i < ARRAY_DIM(s_TestEncode);  i++) {
        const STestArg* arg = &s_TestEncode[i];
        URL_Encode(arg->src_buf, arg->src_size, &src_read,
                   dst, arg->dst_size, &dst_written);
        assert(src_read == arg->src_read);
        assert(dst_written == arg->dst_written);
        assert(!dst_written  ||  !memcmp(dst, arg->dst_buf, dst_written));
    }

    for (i = 0;  i < ARRAY_DIM(s_TestDecode);  i++) {
        const STestArg* arg = &s_TestDecode[i];
        int/*bool*/ ok = URL_Decode(arg->src_buf, arg->src_size, &src_read,
                                    dst, arg->dst_size, &dst_written);
        assert(ok == arg->ok);
        assert(src_read == arg->src_read);
        assert(dst_written == arg->dst_written);
        assert(!dst_written  ||  !memcmp(dst, arg->dst_buf, dst_written));
    }

    for (i = 0;  i < ARRAY_DIM(s_TestDecodeEx);  i++) {
        const STestArg* arg = &s_TestDecodeEx[i];
        int/*bool*/ ok = URL_DecodeEx(arg->src_buf, arg->src_size, &src_read,
                                      dst, arg->dst_size, &dst_written, "[>");
        assert(ok == arg->ok);
        assert(src_read == arg->src_read);
        assert(dst_written == arg->dst_written);
        assert(!dst_written  ||  !memcmp(dst, arg->dst_buf, dst_written));
    }
}



/***********************************************************************
 *  TEST:  Miscellaneous
 */


static int/*bool*/ TEST_Misc_MIME
(const char*    str,
 EMIME_SubType  subtype,
 EMIME_Encoding encoding)
{
    EMIME_SubType  x_subtype;
    EMIME_Encoding x_encoding;

    if (!MIME_ParseContentType(str, &x_subtype, 0)   || x_subtype  != subtype)
        return 0/*false*/;
    if (!MIME_ParseContentType(str, 0, &x_encoding)  || x_encoding != encoding)
        return 0/*false*/;    
    if (!MIME_ParseContentType(str, &x_subtype, &x_encoding)  ||
        x_subtype != subtype  ||  x_encoding != encoding)
        return 0/*false*/;    

    str = strchr(str, ':');
    if ( str ) {
        str++;
        return TEST_Misc_MIME(str, subtype, encoding);
    }

    return 1/*true*/;
}


static void TEST_Misc(void)
{
    int i,j;

    /* MIME API */
    EMIME_SubType  subtype;
    EMIME_Encoding encoding;
    char           str[MAX_CONTENT_TYPE_LEN];
    *str = '\0';
    for (i = 0, subtype = (EMIME_SubType) i;
         i <= (int)eMIME_Unknown;  i++, subtype = (EMIME_SubType) i) {
        for (j = 0, encoding = (EMIME_Encoding) j; 
             j <= (int)eENCOD_None;  j++, encoding = (EMIME_Encoding) j) {
            assert(!TEST_Misc_MIME(str, subtype, encoding));
            MIME_ComposeContentType(subtype, encoding, str, sizeof(str));
            assert(TEST_Misc_MIME(str, subtype, encoding));
        }
    }

    assert(TEST_Misc_MIME("content-type:  x-ncbi-data/x-asn-binary ",
                          eMIME_AsnBinary, eENCOD_None));
    assert(TEST_Misc_MIME("content-TYPE:"
                          " \t x-ncbi-data/x-asn-text-url-encoded\r",
                          eMIME_AsnText, eENCOD_Url));
    assert(TEST_Misc_MIME("x-ncbi-data/x-eeee",
                          eMIME_Unknown, eENCOD_None));

    assert(!TEST_Misc_MIME("content-TYPE : x-ncbi-data/x-unknown\r",
                           eMIME_Unknown, eENCOD_None));
    assert(!TEST_Misc_MIME("", eMIME_Unknown, eENCOD_None));
    assert(!TEST_Misc_MIME(0, eMIME_Unknown, eENCOD_None));
}



/***********************************************************************
 *  MAIN
 */


int main(void)
{
    CORE_SetLOGFILE(stderr, 0/*false*/);

    TEST_URL_Encoding();
    TEST_Misc();
    return 0;
}
