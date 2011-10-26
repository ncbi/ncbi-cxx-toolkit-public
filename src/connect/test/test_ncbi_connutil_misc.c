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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Tests for "ncbi_connutil.c"
 *
 */

#include <connect/ncbi_connutil.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <stdlib.h>
#include <time.h>
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

    CORE_LOG(eLOG_Note, "URL encoding test started");

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

    CORE_LOG(eLOG_Note, "URL encoding test completed");
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

    CORE_LOG(eLOG_Note, "MIME test started");

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

    CORE_LOG(eLOG_Note, "MIME test completed");
}


static void TEST_ConnNetInfo(void)
{
    size_t n;
    char* str;
    char buf[256];
    SConnNetInfo* net_info;

    CORE_LOG(eLOG_Note, "ConnNetInfo test started");

    net_info = ConnNetInfo_Create(0);
    assert(net_info);

    assert(ConnNetInfo_ParseURL(net_info,
                                "ftp://user:pass@host:8888/ro.t/p@th"
                                "?arg/arg:arg@arg:arg/arg"));
    assert(net_info->scheme                           == eURL_Ftp);
    assert(strcmp(net_info->user, "user")                    == 0);
    assert(strcmp(net_info->pass, "pass")                    == 0);
    assert(strcmp(net_info->host, "host")                    == 0);
    assert(       net_info->port                             == 8888);
    assert(strcmp(net_info->path, "/ro.t/p@th")              == 0);
    assert(strcmp(net_info->args, "arg/arg:arg@arg:arg/arg") == 0);

    assert(ConnNetInfo_ParseURL(net_info, "https://www/path"
                                "?arg:arg@arg#frag"));
    assert(       net_info->scheme            == eURL_Https);
    assert(      *net_info->user                       == 0);
    assert(      *net_info->pass                       == 0);
    assert(strcmp(net_info->host, "www")               == 0);
    assert(       net_info->port                       == 0);
    assert(strcmp(net_info->path, "/path")             == 0);
    assert(strcmp(net_info->args, "arg:arg@arg#frag")  == 0);

    assert(ConnNetInfo_ParseURL(net_info, "/path1?arg1#frag2"));
    assert(strcmp(net_info->args, "arg1#frag2")        == 0);

    assert(ConnNetInfo_ParseURL(net_info, "path0/0"));
    assert(strcmp(net_info->path, "/path0/0")          == 0);
    assert(strcmp(net_info->args, "#frag2")            == 0);

    assert(ConnNetInfo_ParseURL(net_info, "#frag3"));
    assert(strcmp(net_info->path, "/path0/0")          == 0);
    assert(strcmp(net_info->args, "#frag3")            == 0);

    assert(ConnNetInfo_ParseURL(net_info, "path2"));
    assert(strcmp(net_info->path, "/path0/path2")      == 0);
    assert(strcmp(net_info->args, "#frag3")            == 0);

    assert(ConnNetInfo_ParseURL(net_info, "/path3?arg3"));
    assert(strcmp(net_info->path, "/path3")            == 0);
    assert(strcmp(net_info->args, "arg3#frag3")        == 0);

    strcpy(net_info->user, "user");
    strcpy(net_info->pass, "pass");

    ConnNetInfo_LogEx(net_info, eLOG_Note, CORE_GetLOG());

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "https://www/path3?arg3#frag3") == 0);
    free(str);

    assert(ConnNetInfo_ParseURL(net_info, "path4/path5?arg4#"));
    assert(strcmp(net_info->user, "user")              == 0);
    assert(strcmp(net_info->pass, "pass")              == 0);
    assert(strcmp(net_info->path, "/path4/path5")      == 0);
    assert(strcmp(net_info->args, "arg4")              == 0);

    assert(ConnNetInfo_ParseURL(net_info, "../path6?args"));
    assert(strcmp(net_info->path, "/path4/../path6")   == 0);
    assert(strcmp(net_info->args, "args")              == 0);

    ConnNetInfo_LogEx(net_info, eLOG_Note, CORE_GetLOG());

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "https://www/path4/../path6?args") == 0);
    free(str);

    ConnNetInfo_SetUserHeader(net_info, "");
    str = UTIL_PrintableString(net_info->http_user_header, 0, buf, 0);
    printf("HTTP User Header after set:\n%s%s%s\n",
           "\"" + !str, str ? buf : "NULL", "\"" + !str);
    assert(!net_info->http_user_header  &&  !str);

    ConnNetInfo_AppendUserHeader(net_info,
                                 "T0: V0\n"
                                 "T1:V1\r\n"
                                 "T2: V2\r\n"
                                 "T3: V3\n"
                                 "T4: V4\n"
                                 "T1: V6");
    str = UTIL_PrintableString(net_info->http_user_header, 0, buf, 0);
    if (str)
        *str = '\0';
    printf("HTTP User Header after append:\n%s%s%s\n",
           "\"" + !str, str ? buf : "NULL", "\"" + !str);
    assert(strcmp(net_info->http_user_header,
                  "T0: V0\n"
                  "T1:V1\r\n"
                  "T2: V2\r\n"
                  "T3: V3\n"
                  "T4: V4\n"
                  "T1: V6\r\n") == 0);

    ConnNetInfo_OverrideUserHeader(net_info,
                                   "T0\r\n"
                                   "T5: V5\n"
                                   "T1:    \t  \r\n"
                                   "T2:V2.1\r\n"
                                   "T3:V3\r\n"
                                   "T6: V6\r\n"
                                   "T4: W4 X4 Z4");
    str = UTIL_PrintableString(net_info->http_user_header, 0, buf, 0);
    if (str)
        *str = '\0';
    printf("HTTP User Header after override:\n%s%s%s\n",
           "\"" + !str, str ? buf : "NULL", "\"" + !str);
    assert(strcmp(net_info->http_user_header,
                  "T0: V0\n"
                  "T2:V2.1\r\n"
                  "T3:V3\n"
                  "T4: W4 X4 Z4\r\n"
                  "T5: V5\n"
                  "T6: V6\r\n") == 0);

    ConnNetInfo_ExtendUserHeader(net_info,
                                 "T0: V0\n"
                                 "T1:V1\r\n"
                                 "T2:V2\n"
                                 "T3: T3:V3\n"
                                 "T4: X4\n"
                                 "T5");
    str = UTIL_PrintableString(net_info->http_user_header, 0, buf, 0);
    if (str)
        *str = '\0';
    printf("HTTP User Header after extend:\n%s%s%s\n",
           "\"" + !str, str ? buf : "NULL", "\"" + !str);
    assert(strcmp(net_info->http_user_header,
                  "T0: V0\n"
                  "T2:V2.1 V2\r\n"
                  "T3:V3 T3:V3\n"
                  "T4: W4 X4 Z4\r\n"
                  "T5: V5\n"
                  "T6: V6\r\n"
                  "T1:V1\r\n") == 0);

    ConnNetInfo_ExtendUserHeader(net_info,
                                 "T2: V2\n"
                                 "T3: V3\r\n"
                                 "T4:X4\r\n"
                                 "T4: Z4");
    str = UTIL_PrintableString(net_info->http_user_header, 0, buf, 0);
    if (str)
        *str = '\0';
    printf("HTTP User Header after no-op extend:\n%s%s%s\n",
           "\"" + !str, str ? buf : "NULL", "\"" + !str);
    assert(strcmp(net_info->http_user_header,
                  "T0: V0\n"
                  "T2:V2.1 V2\r\n"
                  "T3:V3 T3:V3\n"
                  "T4: W4 X4 Z4\r\n"
                  "T5: V5\n"
                  "T6: V6\r\n"
                  "T1:V1\r\n") == 0);

    ConnNetInfo_DeleteUserHeader(net_info,
                                 net_info->http_user_header);
    str = UTIL_PrintableString(net_info->http_user_header, 0, buf, 0);
    if (str)
        *str = '\0';
    printf("HTTP User Header after self-delete:\n%s%s%s\n",
           "\"" + !str, str ? buf : "NULL", "\"" + !str);
    assert(strcmp(net_info->http_user_header, "") == 0);

    ConnNetInfo_SetUserHeader(net_info, 0);
    str = UTIL_PrintableString(net_info->http_user_header, 0, buf, 0);
    if (str)
        *str = '\0';
    printf("HTTP User Header after reset:\n%s%s%s\n",
           "\"" + !str, str ? buf : "NULL", "\"" + !str);
    assert(!net_info->http_user_header);

    for (n = 0; n < sizeof(net_info->args); n++)
        net_info->args[n] = "0123456789"[rand() % 10];

    strncpy0(net_info->args, "a=b&b=c&c=d", sizeof(net_info->args) - 1);
    printf("HTTP Arg: \"%s\"\n", net_info->args);

    ConnNetInfo_PrependArg(net_info, "d=e", 0);
    ConnNetInfo_PrependArg(net_info, "e", "f");
    printf("HTTP Arg after prepend: \"%s\"\n", net_info->args);

    ConnNetInfo_AppendArg(net_info, "f=g", 0);
    ConnNetInfo_AppendArg(net_info, "g", "h");
    printf("HTTP Arg after append: \"%s\"\n", net_info->args);

    ConnNetInfo_PreOverrideArg(net_info, "a=z&b", "y");
    ConnNetInfo_PreOverrideArg(net_info, "c", "x");
    printf("HTTP Arg after pre-override: \"%s\"\n", net_info->args);

    ConnNetInfo_PostOverrideArg(net_info, "d=w&e", "v");
    ConnNetInfo_PostOverrideArg(net_info, "f", "u");
    printf("HTTP Arg after post-override: \"%s\"\n", net_info->args);

    ConnNetInfo_DeleteArg(net_info, "g");
    ConnNetInfo_DeleteArg(net_info, "h=n");
    printf("HTTP Arg after delete: \"%s\"\n", net_info->args);

    ConnNetInfo_DeleteAllArgs(net_info, "a=b&p=q&f=d");
    printf("HTTP Arg after delete-all: \"%s\"\n", net_info->args);

    ConnNetInfo_LogEx(net_info, eLOG_Note, CORE_GetLOG());

    ConnNetInfo_Destroy(net_info);

    CORE_LOG(eLOG_Note, "ConnNetInfo test completed");
}


/***********************************************************************
 *  MAIN
 */


int main(void)
{
    g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    srand(g_NCBI_ConnectRandomSeed);

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    CORE_LOG(eLOG_Note, "Miscellaneous tests started");

    TEST_URL_Encoding();
    TEST_MIME();
    TEST_ConnNetInfo();

    CORE_LOG(eLOG_Note, "All tests completed successfully");
    CORE_SetLOG(0);
    return 0;
}
