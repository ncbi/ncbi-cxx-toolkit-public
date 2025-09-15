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
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include "test_assert.h"  /* This header must go last */


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
    int i, j, k;
    EMIME_Type     type;
    EMIME_SubType  subtype;
    EMIME_Encoding encoding;
    char str[CONN_CONTENT_TYPE_LEN+1];

    CORE_LOG(eLOG_Note, "MIME test started");

    *str = '\0';
    for (i = 0, type = (EMIME_Type) i;
         i <= (int) eMIME_T_Unknown;
         ++i, type = (EMIME_Type) i) {
        for (j = 0, subtype = (EMIME_SubType) j;
             j <= (int) eMIME_Unknown;
             ++j, subtype = (EMIME_SubType) j) {
            for (k = 0, encoding = (EMIME_Encoding) k;
                 k < (int) eENCOD_Unknown;
                 ++k, encoding = (EMIME_Encoding) k) {
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


static void TEST_Misc(void)
{
    char buf[80];

    CORE_LOG(eLOG_Note, "MISC test started");

    assert(UTIL_NcbiLocalHostName(strcpy(buf, "www.ncbi.nlm.nih.gov.")) ==buf);
    assert(strcmp(buf, "www") == 0);
    assert(UTIL_NcbiLocalHostName(strcpy(buf, "web.ncbi.nlm.nih.gov"))  ==buf);
    assert(strcmp(buf, "web") == 0);
    assert(UTIL_NcbiLocalHostName(strcpy(buf, "one.host.ncbi.nih.gov."))==buf);
    assert(strcmp(buf, "one.host") == 0);
    assert(UTIL_NcbiLocalHostName(strcpy(buf, "some.host.ncbi.nih.gov"))==buf);
    assert(strcmp(buf, "some.host") == 0);
    assert(!UTIL_NcbiLocalHostName(strcpy(buf, "bad..ncbi.nih.gov")));
    assert(!UTIL_NcbiLocalHostName(strcpy(buf, "host.nih.gov")));

    CORE_LOG(eLOG_Note, "MISC test completed");
}


static void x_FillupNetInfo(SConnNetInfo* net_info)
{
    net_info->scheme = eURL_Unspec;
    memset(net_info->host, 'h', sizeof(net_info->host));
    net_info->host[CONN_HOST_LEN] = '\0';
    net_info->port = 9999;
    memset(net_info->user, 'u', sizeof(net_info->user));
    net_info->user[CONN_USER_LEN] = '\0';
    memset(net_info->pass, 'p', sizeof(net_info->pass));
    net_info->pass[CONN_PASS_LEN] = '\0';
    memset(net_info->path, 'x', sizeof(net_info->path));
    net_info->path[CONN_PATH_LEN] = '\0';
}


static void TEST_ConnNetInfo(void)
{
    size_t n;
    char* str;
    SConnNetInfo* net_info;

    CORE_LOG(eLOG_Note, "ConnNetInfo test started");

    net_info = ConnNetInfo_Create(0);
    assert(net_info);

    assert(ConnNetInfo_ParseURL(net_info,
                                "ftp://user:pass@host:8888/ro.t/p@th"
                                "?arg/arg:arg@arg:arg/arg"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(net_info->scheme               == eURL_Ftp);
    assert(strcmp(net_info->user, "user") == 0);
    assert(strcmp(net_info->pass, "pass") == 0);
    assert(strcmp(net_info->host, "host") == 0);
    assert(       net_info->port          == 8888);
    assert(strcmp(net_info->path,    "/ro.t/p@th?arg/arg:arg@arg:arg/arg")==0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info),"arg/arg:arg@arg:arg/arg")==0);

    assert(ConnNetInfo_ParseURL(net_info, "https://www/path"
                                "?arg:arg@arg#frag"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(       net_info->scheme       == eURL_Https);
    assert(      *net_info->user         == 0);
    assert(      *net_info->pass         == 0);
    assert(strcmp(net_info->host, "www") == 0);
    assert(       net_info->port         == 0);
    assert(strcmp(net_info->path,          "/path?arg:arg@arg#frag") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "arg:arg@arg#frag") == 0);

    assert(ConnNetInfo_ParseURL(net_info, "/path1?arg1#frag2"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(ConnNetInfo_GetArgs(net_info), "arg1#frag2") == 0);

    assert(ConnNetInfo_ParseURL(net_info, "path0/0"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,        "/path0/0#frag2") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "#frag2") == 0);

    assert(ConnNetInfo_ParseURL(net_info, "#frag3"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,        "/path0/0#frag3") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "#frag3") == 0);

    assert(ConnNetInfo_ParseURL(net_info, "path2"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,    "/path0/path2#frag3") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "#frag3") == 0);

    assert(ConnNetInfo_ParseURL(net_info, "/path3?arg3"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,         "/path3?arg3#frag3") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "arg3#frag3") == 0);

    strcpy(net_info->user, "user");
    strcpy(net_info->pass, "pass");

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "https://www/path3?arg3#frag3") == 0);
    free(str);

    assert(ConnNetInfo_ParseURL(net_info, "path4/path5?arg4#"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->user, "user") == 0);
    assert(strcmp(net_info->pass, "pass") == 0);
    assert(strcmp(net_info->path,   "/path4/path5?arg4") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "arg4") == 0);

    assert(ConnNetInfo_ParseURL(net_info, "../path6?args"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->user, "user") == 0);
    assert(strcmp(net_info->pass, "pass") == 0);
    assert(strcmp(net_info->path, "/path4/../path6?args") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info),  "args") == 0);

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "https://www/path4/../path6?args") == 0);
    free(str);

    assert(ConnNetInfo_ParseURL(net_info, "http:///path7?newargs#frag"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(net_info->scheme               == eURL_Http);
    assert(strcmp(net_info->user, "user") == 0);
    assert(strcmp(net_info->pass, "pass") == 0);
    assert(strcmp(net_info->path,         "/path7?newargs#frag") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "newargs#frag") == 0);

    assert(ConnNetInfo_SetPath(net_info,  "/path8?moreargs"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,         "/path8?moreargs#frag") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "moreargs#frag") == 0);

    assert(ConnNetInfo_SetPath(net_info,   "/path9#morefrag"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,          "/path9#morefrag") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "#morefrag") == 0);

    assert(ConnNetInfo_SetArgs(net_info,  "newarg=newval#newfrag"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,         "/path9?newarg=newval#newfrag")==0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "newarg=newval#newfrag")==0);

    assert(ConnNetInfo_SetArgs(net_info,  "arg=val"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,         "/path9?arg=val#newfrag")==0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "arg=val#newfrag")==0);

    assert(ConnNetInfo_SetArgs(net_info,  "#xfrag"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,          "/path9#xfrag") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "#xfrag") == 0);

    assert(ConnNetInfo_AddPath(net_info,   "/path10"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,          "/path9/path10#xfrag") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info),        "#xfrag") == 0);

    assert(ConnNetInfo_AddPath(net_info,   "path11/?args"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,          "/path9/path11/?args#xfrag") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info),          "args#xfrag") == 0);

    assert(ConnNetInfo_AddPath(net_info,   "/path12#xxx"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,          "/path9/path11/path12#xxx") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info),               "#xxx") == 0);

    assert(ConnNetInfo_SetPath(net_info, ""));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(strcmp(net_info->path,                "#xxx") == 0);
    assert(strcmp(ConnNetInfo_GetArgs(net_info), "#xxx") == 0);

    assert(ConnNetInfo_SetArgs(net_info, 0));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    assert(net_info->path[0] == '\0');
    assert(net_info->path    == ConnNetInfo_GetArgs(net_info));

    x_FillupNetInfo(net_info);
    assert(ConnNetInfo_ParseURL(net_info, "http://130.14.29.110/Service/index.html"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(net_info->scheme == eURL_Http);
    assert(strcmp(net_info->host, "130.14.29.110") == 0);
    assert(net_info->port == 0);
    assert(!*net_info->user);
    assert(!*net_info->pass);
    assert(strcmp(net_info->path, "/Service/index.html") == 0);

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "http://130.14.29.110/Service/index.html") == 0);
    free(str);

    x_FillupNetInfo(net_info);   
    assert(ConnNetInfo_ParseURL(net_info, "https://130.14.29.110:8080/Service/index.html"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(net_info->scheme == eURL_Https);
    assert(strcmp(net_info->host, "130.14.29.110") == 0);
    assert(net_info->port == 8080);
    assert(!*net_info->user);
    assert(!*net_info->pass);
    assert(strcmp(net_info->path, "/Service/index.html") == 0);

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "https://130.14.29.110:8080/Service/index.html") == 0);
    free(str);

    x_FillupNetInfo(net_info);
    assert(ConnNetInfo_ParseURL(net_info, "ftp://[130.14.29.110]:8888/Service/index.html"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(net_info->scheme == eURL_Ftp);
    assert(strcmp(net_info->host, "130.14.29.110") == 0);
    assert(net_info->port == 8888);
    assert(!*net_info->user);
    assert(!*net_info->pass);
    assert(strcmp(net_info->path, "/Service/index.html") == 0);

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "ftp://130.14.29.110:8888/Service/index.html") == 0);
    free(str);

    x_FillupNetInfo(net_info);
    assert(ConnNetInfo_ParseURL(net_info, "130.14.29.110:8080"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(net_info->scheme == eURL_Unspec);
    assert(strcmp(net_info->host, "130.14.29.110") == 0);
    assert(net_info->port == 8080);
    assert(!*net_info->user);
    assert(!*net_info->pass);
    assert(strcmp(net_info->path, "") == 0);

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "//130.14.29.110:8080") == 0);
    free(str);

    x_FillupNetInfo(net_info);
    assert(ConnNetInfo_ParseURL(net_info, "[130.14.29.110]:8888/"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(net_info->scheme == eURL_Unspec);
    assert(strcmp(net_info->host, "130.14.29.110") == 0);
    assert(net_info->port == 8888);
    assert(!*net_info->user);
    assert(!*net_info->pass);
    assert(strcmp(net_info->path, "/") == 0);

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "//130.14.29.110:8888/") == 0);
    free(str);

    x_FillupNetInfo(net_info);
    assert(!ConnNetInfo_ParseURL(net_info, "ftp://2607:f220:41e:4290::110/Service/index.html"));

    x_FillupNetInfo(net_info);
    assert(ConnNetInfo_ParseURL(net_info, "http://[2607:f220:41e:4290::110]/Service/index.html"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(net_info->scheme == eURL_Http);
    assert(strcmp(net_info->host, "2607:f220:41e:4290::110") == 0);
    assert(net_info->port == 0);
    assert(!*net_info->user);
    assert(!*net_info->pass);
    assert(strcmp(net_info->path, "/Service/index.html") == 0);

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "http://[2607:f220:41e:4290::110]/Service/index.html") == 0);
    free(str);

    x_FillupNetInfo(net_info);
    assert(ConnNetInfo_ParseURL(net_info, "https://[2607:f220:41e:4290::110]:4444/Service/index.html"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(net_info->scheme == eURL_Https);
    assert(strcmp(net_info->host, "2607:f220:41e:4290::110") == 0);
    assert(net_info->port == 4444);
    assert(!*net_info->user);
    assert(!*net_info->pass);
    assert(strcmp(net_info->path, "/Service/index.html") == 0);

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "https://[2607:f220:41e:4290::110]:4444/Service/index.html") == 0);
    free(str);

    x_FillupNetInfo(net_info);
    assert(ConnNetInfo_ParseURL(net_info, "[2607:f220:41e:4290::110]:4444"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(net_info->scheme == eURL_Unspec);
    assert(strcmp(net_info->host, "2607:f220:41e:4290::110") == 0);
    assert(net_info->port == 4444);
    assert(!*net_info->user);
    assert(!*net_info->pass);
    assert(strcmp(net_info->path, "") == 0);

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "//[2607:f220:41e:4290::110]:4444") == 0);
    free(str);
    
    x_FillupNetInfo(net_info);
    assert(!ConnNetInfo_ParseURL(net_info, "http://" "user" "@" "/somepath"));
    assert( ConnNetInfo_ParseURL(net_info, "http://" "@" "host" "/somepath"));
    assert(!ConnNetInfo_ParseURL(net_info, "http://" ":pass" "@" "/somepath"));
    assert(!ConnNetInfo_ParseURL(net_info, "http://" "user" ":" "pass" "@" "/somepath"));
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(net_info->scheme == eURL_Http);
    assert(strcmp(net_info->host, "host") == 0);
    assert(net_info->port == 0);
    assert(!*net_info->user);
    assert(!*net_info->pass);
    assert(strcmp(net_info->path, "/somepath") == 0);

    str = ConnNetInfo_URL(net_info);
    assert(str);
    assert(strcmp(str, "http://host/somepath") == 0);
    free(str);
    
    ConnNetInfo_SetUserHeader(net_info, "");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(!net_info->http_user_header);

    ConnNetInfo_AppendUserHeader(net_info,
                                 "T0: V0\n"
                                 "T1:V1\r\n"
                                 "T2: V2\r\n"
                                 "T3: V3\n"
                                 "T4: V4\n"
                                 "T1: V6\n"
                                 "T2:V2.2");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->http_user_header,
                  "T0: V0\n"
                  "T1:V1\r\n"
                  "T2: V2\r\n"
                  "T3: V3\n"
                  "T4: V4\n"
                  "T1: V6\n"
                  "T2:V2.2\r\n") == 0);

    ConnNetInfo_OverrideUserHeader(net_info,
                                   "T0\r\n"
                                   "T1:    \t  \r\n"
                                   "T2:V2.1\r\n"
                                   "T3:V3\r\n");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->http_user_header,
                  "T0: V0\n"
                  "T2:V2.1\r\n"
                  "T3:V3\n"
                  "T4: V4\r\n") == 0);

    ConnNetInfo_OverrideUserHeader(net_info,
                                   "T0: V00\n"
                                   "T5: V5\n"
                                   "T6: V6\r\n"
                                   "T4: W4 X4 Z4\r\n");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->http_user_header,
                  "T0: V00\n"
                  "T2:V2.1\r\n"
                  "T3:V3\n"
                  "T4: W4 X4 Z4\r\n"
                  "T5: V5\n"
                  "T6: V6\r\n") == 0);

    ConnNetInfo_ExtendUserHeader(net_info,
                                 "T0: V00\n"
                                 "T1:V1\r\n"
                                 "T2:V2\n"
                                 "T3: T3:V3\n"
                                 "T4: X4\n"
                                 "T5");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->http_user_header,
                  "T0: V00\n"
                  "T2:V2.1 V2\r\n"
                  "T3:V3 T3:V3\n"
                  "T4: W4 X4 Z4\r\n"
                  "T5: V5\n"
                  "T6: V6\r\n"
                  "T1:V1\r\n") == 0);

    ConnNetInfo_ExtendUserHeader(net_info,
                                 "T1  \n"
                                 "T2: V2\n"
                                 "T3: V3\r\n"
                                 "T4:X4\r\n"
                                 "T4: Z4");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->http_user_header,
                  "T0: V00\n"
                  "T2:V2.1 V2\r\n"
                  "T3:V3 T3:V3\n"
                  "T4: W4 X4 Z4\r\n"
                  "T5: V5\n"
                  "T6: V6\r\n"
                  "T1:V1\r\n") == 0);

    ConnNetInfo_DeleteUserHeader(net_info, "T5\t\nT2: \r\n T6:\nT0");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->http_user_header,
                  "T3:V3 T3:V3\n"
                  "T4: W4 X4 Z4\r\n"
                  "T5: V5\n"
                  "T6: V6\r\n"
                  "T1:V1\r\n") == 0);

    ConnNetInfo_DeleteUserHeader(net_info,
                                 net_info->http_user_header);
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(!net_info->http_user_header);

    ConnNetInfo_SetUserHeader(net_info, 0);
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(!net_info->http_user_header);

    for (n = strcspn(net_info->path, "?")+1;  n < sizeof(net_info->path);  ++n)
        net_info->path[n] = "0123456789"[rand() % 10];

    strcpy(net_info->path, "/path?#frag");
    ConnNetInfo_SetArgs(net_info, "a=b#");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->path, "/path?a=b") == 0);

    strcpy(net_info->path, "/path?");
    ConnNetInfo_PrependArg(net_info, "a", "b");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->path, "/path?a=b") == 0);

    strcpy(net_info->path, "/path?#frag");
    ConnNetInfo_PrependArg(net_info, "a", "b");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->path, "/path?a=b#frag") == 0);
    ConnNetInfo_PrependArg(net_info, "c", "d");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->path, "/path?c=d&a=b#frag") == 0);

    ConnNetInfo_SetPath(net_info, "#");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->path, "") == 0);

    strcpy(net_info->path, "/path?");
    ConnNetInfo_AppendArg(net_info, "a", "b");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->path, "/path?a=b") == 0);

    strcpy(net_info->path, "/path?#frag");
    ConnNetInfo_AppendArg(net_info, "a", "b");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->path, "/path?a=b#frag") == 0);
    ConnNetInfo_AppendArg(net_info, "c", "d");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    assert(strcmp(net_info->path, "/path?a=b&c=d#frag") == 0);

    for (n = strcspn(net_info->path, "?")+1;  n < sizeof(net_info->path);  ++n)
        net_info->path[n] = "0123456789"[rand() % 10];

    ConnNetInfo_SetArgs(net_info, "a=b&b=c&c=d#f");
    printf("HTTP Arg:                     \"%s\"\n",
           ConnNetInfo_GetArgs(net_info));

    ConnNetInfo_PrependArg(net_info, "d=e", 0);
    ConnNetInfo_PrependArg(net_info, "e", "f");
    printf("HTTP Arg after prepend:       \"%s\"\n",
           ConnNetInfo_GetArgs(net_info));
    assert(strcmp(ConnNetInfo_GetArgs(net_info),
                  "e=f&d=e&a=b&b=c&c=d#f") == 0);

    ConnNetInfo_AppendArg(net_info, "f=g", 0);
    ConnNetInfo_AppendArg(net_info, "g", "h");
    printf("HTTP Arg after append:        \"%s\"\n",
           ConnNetInfo_GetArgs(net_info));
    assert(strcmp(ConnNetInfo_GetArgs(net_info),
                  "e=f&d=e&a=b&b=c&c=d&f=g&g=h#f") == 0);

    ConnNetInfo_PreOverrideArg(net_info, "a=z&b#p", "y#t");
    ConnNetInfo_PreOverrideArg(net_info, "c", "x");
    printf("HTTP Arg after pre-override:  \"%s\"\n",
           ConnNetInfo_GetArgs(net_info));
    assert(strcmp(ConnNetInfo_GetArgs(net_info),
                  "c=x&a=z&b=y&e=f&d=e&f=g&g=h#f") == 0);

    ConnNetInfo_PostOverrideArg(net_info, "d=w&e#q", "v#s");
    ConnNetInfo_PostOverrideArg(net_info, "f", "u");
    printf("HTTP Arg after post-override: \"%s\"\n",
           ConnNetInfo_GetArgs(net_info));
    assert(strcmp(ConnNetInfo_GetArgs(net_info),
                  "c=x&a=z&b=y&g=h&d=w&e=v&f=u#f") == 0);

    ConnNetInfo_DeleteArg(net_info, "g");
    ConnNetInfo_DeleteArg(net_info, "h=n");
    printf("HTTP Arg after delete:        \"%s\"\n",
           ConnNetInfo_GetArgs(net_info));
    assert(strcmp(ConnNetInfo_GetArgs(net_info),
                  "c=x&a=z&b=y&d=w&e=v&f=u#f") == 0);

    ConnNetInfo_DeleteAllArgs(net_info, "a=b&p=q&f=d#t");
    printf("HTTP Arg after delete-all:    \"%s\"\n",
           ConnNetInfo_GetArgs(net_info));
    assert(strcmp(ConnNetInfo_GetArgs(net_info),
                  "c=x&b=y&d=w&e=v#f") == 0);

    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());
    ConnNetInfo_Destroy(net_info);

    CORE_LOG(eLOG_Note, "ConnNetInfo test completed");
}


static void TEST_DoubleConv(void)
{
    static const char* good_tests[] = { "100.25", "-100.25",
                                        "100.987654", "-100.987654",
                                        "0.01", "-0.01",
                                        "0.001", "-0.001",
                                        "+1.0001", "-1.0001",
                                        "0.2345", "-0.5432",
                                        "1", "-1",
                                        "+100", "-100"};
    static const char* bad_tests[] = { "-", "+", "-.", "+.", "+-", "-+",
                                       "-+3", "+-2", "+-1.2", "-+2.1",
                                       "-a", "+a", ".a",
                                       "-.a", "+.a" };
    static const char* weird_tests[] = { "-0a", "+0a", "0a",
                                         "-1a", "+2a", "3a",
                                         "-0.a", "+0.a", "0.a",
                                         "-1.a", "+2.a", "3.a",
                                         "-0.1a", "+0.2a" , "0.3a",
                                         "-.1a", "+.2a", ".3a" };
    size_t i;

    CORE_LOG(eLOG_Note, "Simple double conversion test started");

    CORE_LOG(eLOG_Note, "Conversion checks");

    for (i = 0;  i < sizeof(good_tests) / sizeof(good_tests[0]);  i++) {
        int p, q, okay;
        char buf[80], *end = 0;
        const char* str = good_tests[i];
        double val = NCBI_simple_atof(str, &end);

        assert(end  &&  !*end  &&  end == str + strlen(str));
        if (!(end = (char*) strchr(str, '.')))
            q = 0;
        else
            q = (int) strlen(str) - (int)(end - str) - 1;
        printf("str = \"%s\", val =  %.*f\n", str, q, val);

        okay = 0/*false*/;
        for (p = 0;  p < 10;  p++) {
            end = NCBI_simple_ftoa(buf, val, p);
            printf("str = \"%s\", buf = \"%s\"%s\n", str, buf,
                   p == q ? (okay = 1/*true*/, " *") : "");
            assert(end  &&  (p != q  ||
                             strcmp(buf, *str == '+' ? str + 1 : str) == 0));
        }
        assert(okay);
    }

    CORE_LOG(eLOG_Note, "Illegal input checks");

    for (i = 0;  i < sizeof(bad_tests) / sizeof (bad_tests[0]);  i++) {
        char* end = 0;
        const char* str = bad_tests[i];
        double val = NCBI_simple_atof(str, &end);

        printf("str = \"%s\", end = \"%s\", val = %f (%d%s%s)\n",
               str, end, val, errno,
               errno ? ", "            : "",
               errno ? strerror(errno) : "");
        assert(!val  &&  end  &&  end == str);
    }
 
    CORE_LOG(eLOG_Note, "Corner case checks");

    for (i = 0;  i < sizeof(weird_tests) / sizeof (weird_tests[0]);  i++) {
        char* end = 0;
        const char* str = weird_tests[i];
        double val = NCBI_simple_atof(str, &end);

        printf("str = \"%s\", end = \"%s\", val = %f\n", str, end, val);
        assert(end  &&  end != str);
    }

    CORE_LOG(eLOG_Note, "Simple double conversion test completed");
}



/***********************************************************************
 *  MAIN
 */


int main(void)
{
    g_NCBI_ConnectRandomSeed
        = (unsigned int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    srand(g_NCBI_ConnectRandomSeed);

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Short   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Fatal, 0/*no auto-close*/);

    CORE_LOG(eLOG_Note, "Miscellaneous tests started");

    TEST_URL_Encoding();
    TEST_MIME();
    TEST_Misc();
    TEST_ConnNetInfo();
    TEST_DoubleConv();

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;
}
