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
 *   Standard test for the MEMORY CONNECTOR
 *
 */

#include <connect/ncbi_memory_connector.h>
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include "ncbi_conntest.h"
/* This header must go last */
#include "test_assert.h"


int main(void)
{
    static STimeout timeout/* = 0 */;
    CONNECTOR       connector;
    FILE*           data_file;

    /* Log and data-log streams */
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);
    data_file = fopen("test_ncbi_memory_connector.log", "wb");
    assert(data_file);

     /* Run the tests */
    connector = MEMORY_CreateConnector();
    CONN_TestConnector(connector, &timeout, data_file, fTC_SingleBounceCheck);

    connector = MEMORY_CreateConnectorEx(0, 0);
    CONN_TestConnector(connector, &timeout, data_file, fTC_Everything);

    /* Cleanup and Exit */
    fclose(data_file);
    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;
}
