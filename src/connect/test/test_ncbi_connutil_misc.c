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
 */

#include <connect/ncbi_connutil.h>
#include <connect/ncbi_util.h>
#include <string.h>
/* This header must go last */
#include "test_assert.h"


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
 *  TEST:  BASE64_Encode(), BAS64_Decode()
 */

static void TEST_BASE64_Encoding(void)
{
    const char test_string[] = "Quick brown fox jumps over the lazy dog";
    char buf1[1024], buf2[1024], buf3[1024];
    size_t read, written, len = 16, i, j;

    BASE64_Encode(test_string, strlen(test_string) + 1, &read,
                  buf1, sizeof(buf1), &written, &len);
    assert(read == strlen(test_string) + 1);
    assert(written < sizeof(buf1));
    assert(buf1[written] == '\0');

    assert(BASE64_Decode(buf1, written, &read,
                         buf2, sizeof(buf2), &written));
    assert(strlen(buf1) == read);
    assert(written == strlen(test_string) + 1);
    assert(buf2[written - 1] == '\0');
    assert(strcmp(buf2, test_string) == 0);

    for (i = 0; i < 100; i++) {
        len = rand() % 250;
        memset(buf1, '\0', sizeof(buf1));
        memset(buf2, '=',  sizeof(buf2));
        memset(buf3, '\0', sizeof(buf3));
        for (j = 0; j < len; j++) {
            buf1[j] = rand() & 0xFF;
        }

        j = rand() % 100;
        BASE64_Encode(buf1, len, &read, buf2, sizeof(buf2), &written, &j);
        assert(len == read);
        assert (written < sizeof(buf2));
        assert(buf2[written] == '\0');

        buf2[written] = '=';
        BASE64_Decode(buf2, written, &read, buf3, sizeof(buf3), &written);
        assert(len == written);
        assert(memcmp(buf1, buf3, len) == 0);
    }
}


/***********************************************************************
 *  TEST:  Miscellaneous
 */


static int/*bool*/ s_Try_MIME
(const char*    str,
 EMIME_Type     type,
 EMIME_SubType  subtype,
 EMIME_Encoding encoding)
{
    EMIME_Type     x_type;
    EMIME_SubType  x_subtype;
    EMIME_Encoding x_encoding;

    if (type == eMIME_T_NcbiData) {
        if (!MIME_ParseContentType(str, &x_subtype, 0)  ||
            x_subtype  != subtype) {
            return 0/*false*/;
        }
        if (!MIME_ParseContentType(str, 0, &x_encoding)  ||
            x_encoding != encoding) {
            return 0/*false*/;
        }
        if (!MIME_ParseContentType(str, &x_subtype, &x_encoding)  ||
            x_subtype != subtype  ||  x_encoding != encoding) {
            return 0/*false*/;
        }
    }

    if (!MIME_ParseContentTypeEx(str, &x_type, &x_subtype, 0)  ||
        x_type != type  ||  x_subtype != subtype) {
        return 0/*false*/;
    }
    if (!MIME_ParseContentTypeEx(str, &x_type, 0, &x_encoding)  ||
        x_type != type  ||  x_encoding != encoding) {
        return 0/*false*/;
    }
    if (!MIME_ParseContentTypeEx(str, &x_type, &x_subtype, &x_encoding)  ||
        x_type != type  ||  x_subtype != subtype  ||  x_encoding != encoding) {
        return 0/*false*/;
    }

    str = strchr(str, ':');
    if ( str ) {
        str++;
        return s_Try_MIME(str, type, subtype, encoding);
    }

    return 1/*true*/;
}


static void TEST_MIME(void)
{
    /* MIME API */
    EMIME_Type     type;
    EMIME_SubType  subtype;
    EMIME_Encoding encoding;
    char str[MAX_CONTENT_TYPE_LEN];
    int i,j,k;

    *str = '\0';
    for (k = 0, type = (EMIME_Type) k;
         k <= (int) eMIME_T_Unknown;  k++, type = (EMIME_Type) k) {
        for (i = 0, subtype = (EMIME_SubType) i;
             i <= (int) eMIME_Unknown;  i++, subtype = (EMIME_SubType) i) {
            for (j = 0, encoding = (EMIME_Encoding) j; 
                 j < (int) eENCOD_Unknown;
                 j++, encoding = (EMIME_Encoding) j) {
                assert(!s_Try_MIME(str, type, subtype, encoding));
                MIME_ComposeContentTypeEx(type, subtype, encoding,
                                          str, sizeof(str));
                assert(s_Try_MIME(str, type, subtype, encoding));
            }
        }
    }

    assert(s_Try_MIME("content-type:  x-ncbi-data/x-asn-binary ",
                      eMIME_T_NcbiData, eMIME_AsnBinary, eENCOD_None));
    assert(s_Try_MIME("content-type:  application/x-www-form-urlencoded ",
                      eMIME_T_Application, eMIME_WwwForm, eENCOD_Url));
    assert(s_Try_MIME("content-TYPE: \t x-ncbi-data/x-asn-text-urlencoded\r",
                      eMIME_T_NcbiData, eMIME_AsnText, eENCOD_Url));
    assert(s_Try_MIME("x-ncbi-data/x-eeee",
                      eMIME_T_NcbiData, eMIME_Unknown, eENCOD_None));
    assert(s_Try_MIME("x-ncbi-data/plain-",
                      eMIME_T_NcbiData, eMIME_Unknown, eENCOD_None));

    assert(!s_Try_MIME("content-TYPE : x-ncbi-data/x-unknown\r",
                       eMIME_T_NcbiData, eMIME_Unknown, eENCOD_None));
    assert(s_Try_MIME("text/html",
                      eMIME_T_Text, eMIME_Html, eENCOD_None));
    assert(s_Try_MIME("application/xml+soap",
                      eMIME_T_Application, eMIME_XmlSoap, eENCOD_None));
    assert(!s_Try_MIME("", eMIME_T_NcbiData, eMIME_Unknown, eENCOD_Unknown));
    assert(!s_Try_MIME(0, eMIME_T_NcbiData, eMIME_Unknown, eENCOD_Unknown));
}


static void TEST_ConnNetInfo(void)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(0);

    assert(net_info);
    printf("HTTP User Header:\n\"%s\"\n",
           net_info->http_user_header ? net_info->http_user_header : "<NONE>");
    ConnNetInfo_AppendUserHeader(net_info,
                                 "My-Tag1: Value1\r\n"
                                 "My-Tag2: Value2\r\n"
                                 "My-Tag3: Value3\r\n");
    printf("HTTP User Header after append:\n\"%s\"\n",
           net_info->http_user_header ? net_info->http_user_header : "<NONE>");
    ConnNetInfo_OverrideUserHeader(net_info,
                                   "My-TAG1:    \t  \r\n"
                                   "My-TaG2: Value 2.1\r\n"
                                   "My-Tag4: Value 4\r\n");
    printf("HTTP User Header after override:\n\"%s\"\n",
           net_info->http_user_header ? net_info->http_user_header : "<NONE>");
    ConnNetInfo_ExtendUserHeader(net_info,
                                 "My-Tag3: \t \r\n"
                                 "My-Tag4: Value 4.1\r\n"
                                 "My-Tag5: \t \r\n"
                                 "My-Tag6: Value 6\r\n");
    printf("HTTP User Header after extend:\n\"%s\"\n",
           net_info->http_user_header ? net_info->http_user_header : "<NONE>");
    ConnNetInfo_SetUserHeader(net_info, 0);
    printf("HTTP User Header after set:\n\"%s\"\n",
           net_info->http_user_header ? net_info->http_user_header : "<NONE>");
    ConnNetInfo_ExtendUserHeader(net_info,
                                 "My-Tag7: Value7\r\n"
                                 "My-Tag8: \t \r\n");
    printf("HTTP User Header after second extend:\n\"%s\"\n",
           net_info->http_user_header ? net_info->http_user_header : "<NONE>");
    ConnNetInfo_Destroy(net_info);
}



/***********************************************************************
 *  MAIN
 */


int main(void)
{
    CORE_SetLOGFILE(stderr, 0/*false*/);

    TEST_URL_Encoding();
    TEST_BASE64_Encoding();
    TEST_MIME();
    TEST_ConnNetInfo();

    CORE_SetLOG(0);
    return 0;
}


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.17  2005/03/21 17:04:51  lavr
 * BASE64_{En|De}code tests extended
 *
 * Revision 6.16  2005/03/19 02:17:08  lavr
 * Fix change log entry
 *
 * Revision 6.15  2005/03/19 02:14:10  lavr
 * +Test for BASE64_{En|De}code
 *
 * Revision 6.14  2004/04/01 14:14:02  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 6.13  2004/01/14 18:53:09  lavr
 * Use "application/xml+soap" in the test case
 *
 * Revision 6.12  2004/01/07 19:24:03  lavr
 * Added test for MIME content-type "application/xml"
 *
 * Revision 6.11  2002/12/13 21:20:55  lavr
 * Move log to end
 *
 * Revision 6.10  2002/11/22 15:09:40  lavr
 * Replace all occurrences of "ray" with "yar"
 *
 * Revision 6.9  2002/10/11 19:57:17  lavr
 * Add tests for ConnNetInfo_*UserHeader() routines
 *
 * Revision 6.8  2002/03/22 19:46:51  lavr
 * Test_assert.h made last among the include files
 *
 * Revision 6.7  2002/02/20 19:12:39  lavr
 * Swapped eENCOD_Url and eENCOD_None; eENCOD_Unknown introduced; test cleaned
 *
 * Revision 6.6  2002/02/05 21:45:55  lavr
 * Included header files rearranged
 *
 * Revision 6.5  2002/01/16 21:23:15  vakatov
 * Utilize header "test_assert.h" to switch on ASSERTs in the Release mode too
 *
 * Revision 6.4  2000/11/07 23:24:43  vakatov
 * [MIME]  In-sync with the C Toolkit "connutil.c:R6.15"
 *
 * Revision 6.3  2000/04/12 15:22:07  vakatov
 * Always #undef NDEBUG
 *
 * Revision 6.2  2000/03/29 17:21:48  vakatov
 * + CORE_SetLOG(0) at the program end.
 *
 * Revision 6.1  2000/03/24 22:53:38  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
