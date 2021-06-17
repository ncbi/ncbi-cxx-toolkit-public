#ifndef NCBI_CONNTEST__H
#define NCBI_CONNTEST__H

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
 *   Test suite for NCBI connector (CONNECTOR)
 *   (see also "ncbi_connection.[ch]", "ncbi_connector.h")
 *
 */

#include <connect/ncbi_connector.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Create a connection (CONN) based on the passed "connector", and then:
 *  1) write some data to the connection and expect them the same data come
 *     back from the connection;
 *  2) after reading back all the "bounced" data, read any extra data
 *     coming from the connection and write the extra data to the
 *     "extra_data_file" output file (if non-NULL).
 *  3) close the connection
 */

typedef enum {
    fTC_SingleBouncePrint = 0x1,
    fTC_MultiBouncePrint  = 0x2,
    fTC_SingleBounceCheck = 0x4,

    fTC_Everything        = 0x7
} ETestConnFlags;
typedef unsigned int TTestConnFlags;


void CONN_TestConnector
(CONNECTOR       connector,  /* [in]  connector handle */
 const STimeout* timeout,    /* [in]  timeout for all i/o */
 FILE*           data_file,  /* [in]  output data file */
 TTestConnFlags  flags       /* [in]  tests to run (binary OR of "eTC_***") */
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_CONNTEST__H */
