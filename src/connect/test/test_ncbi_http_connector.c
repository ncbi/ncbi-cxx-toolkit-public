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
 *   Standard test for the HTTP-based CONNECTOR
 *
 */

#include <connect/ncbi_http_connector.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include "ncbi_conntest.h"
#include <stdlib.h>

#include "test_assert.h"  /* This header must go last */


/* Hard-coded pseudo-registry getter
 */

#define TEST_HOST            "www.ncbi.nlm.nih.gov"
#define TEST_PORT            "80"
#define TEST_PATH            "/Service/bounce.cgi"
#define TEST_ARGS            "arg1+arg2+arg3"
#define TEST_DEBUG_PRINTOUT  "yes"
#define TEST_REQ_METHOD      "any"


#if defined(__cplusplus)
extern "C" {
    static void s_REG_Get(void* user_data,
                          const char* section, const char* name,
                          char* value, size_t value_size);
}
#endif /* __cplusplus */

/*ARGSUSED*/
static void s_REG_Get
(void*       user_data,
 const char* section,
 const char* name,
 char*       value,
 size_t      value_size)
{
    if (strcmp(section, DEF_CONN_REG_SECTION) != 0) {
        assert(0);
        return;
    }

#define X_GET_VALUE(x_name, x_value)            \
  if (strcmp(name, x_name) == 0) {              \
      strncpy0(value, x_value, value_size - 1); \
      return;                                   \
  }

    X_GET_VALUE(REG_CONN_HOST,           TEST_HOST);
    X_GET_VALUE(REG_CONN_PORT,           TEST_PORT);
    X_GET_VALUE(REG_CONN_PATH,           TEST_PATH);
    X_GET_VALUE(REG_CONN_ARGS,           TEST_ARGS);
    X_GET_VALUE(REG_CONN_REQ_METHOD,     TEST_REQ_METHOD);
    X_GET_VALUE(REG_CONN_DEBUG_PRINTOUT, TEST_DEBUG_PRINTOUT);
}


/*****************************************************************************
 *  MAIN
 */

int main(int argc, char* argv[])
{
    char*       user_header = 0;
    CONNECTOR   connector;
    FILE*       data_file;
    STimeout    timeout;
    THTTP_Flags flags;

    /* Log and data-log streams */
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    data_file = fopen("test_ncbi_http_connector.log", "ab");
    assert(data_file);

    /* Tune to the test URL using hard-coded pseudo-registry */
    CORE_SetREG( REG_Create(0, s_REG_Get, 0, 0, 0) );

    /* Connection timeout */
    timeout.sec  = 5;
    timeout.usec = 123456;

    if (argc > 1) {
        /* Generate user header and check graceful failure with
         * bad request status if the header ends up too large. */
        static const char kHttpHeader[] = "My-Header: ";
        size_t n, header_size = (size_t) atoi(argv[1]);
        user_header = (char*) malloc(sizeof(kHttpHeader) + header_size);
        if (user_header) {
            header_size += sizeof(kHttpHeader)-1;
            memcpy(user_header, kHttpHeader, sizeof(kHttpHeader)-1);
            for (n = sizeof(kHttpHeader)-1;  n < header_size;  n++)
                user_header[n] = '.';
            user_header[n] = '\0';
        }
    }

    /* Run the tests */
    flags = fHTTP_KeepHeader | fHTTP_UrlCodec | fHCC_UrlEncodeArgs/*obsolete*/;
    connector = HTTP_CreateConnector(0, user_header, flags);
    CONN_TestConnector(connector, &timeout, data_file, fTC_SingleBouncePrint);

    flags = 0;
    connector = HTTP_CreateConnector(0, user_header, flags);
    CONN_TestConnector(connector, &timeout, data_file, fTC_SingleBounceCheck);

    flags = fHTTP_AutoReconnect;
    connector = HTTP_CreateConnector(0, user_header, flags);
    CONN_TestConnector(connector, &timeout, data_file, fTC_Everything);

    flags = fHTTP_AutoReconnect | fHTTP_UrlCodec;
    connector = HTTP_CreateConnector(0, user_header, flags);
    CONN_TestConnector(connector, &timeout, data_file, fTC_Everything);

    /* Cleanup and Exit */
    CORE_SetREG(0);
    fclose(data_file);

    if (user_header)
        free(user_header);

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;
}
