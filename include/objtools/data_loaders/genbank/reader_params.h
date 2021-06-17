#ifndef GBLOADER__READER_PARAMS__H_INCLUDED
#define GBLOADER__READER_PARAMS__H_INCLUDED

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
*  ===========================================================================
*
*  Author: Eugene Vasilchenko
*
*  File Description:
*    GenBank reader common configuration parameters
*
* ===========================================================================
*/

/* Maximum number of simultaneous connection */
#define NCBI_GBLOADER_READER_PARAM_NUM_CONN "max_number_of_connections"
#define NCBI_GBLOADER_READER_PARAM2_NUM_CONN "no_conn"
/* Whether to open first connection immediately or not (default: true) */
#define NCBI_GBLOADER_READER_PARAM_PREOPEN "preopen"
/* Number of retries on errors */
#define NCBI_GBLOADER_READER_PARAM_RETRY_COUNT "retry"

/* Number of sequential connect errors to wait between connection attempts */
#define NCBI_GBLOADER_READER_PARAM_WAIT_TIME_ERRORS "wait_time_errors"
/* Initial wait between connection attempts */
#define NCBI_GBLOADER_READER_PARAM_WAIT_TIME "wait_time"
/* Open timeout multiplier in case of errors */
#define NCBI_GBLOADER_READER_PARAM_WAIT_TIME_MULTIPLIER "wait_time_multiplier"
/* Open timeout increment in case of errors */
#define NCBI_GBLOADER_READER_PARAM_WAIT_TIME_INCREMENT "wait_time_increment"
/* Maximal open timeout of network connections in seconds */
#define NCBI_GBLOADER_READER_PARAM_WAIT_TIME_MAX "wait_time_max"

#endif
