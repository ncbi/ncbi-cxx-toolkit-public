#ifndef PSG_LOADER_PARAMS__HPP_INCLUDED
#define PSG_LOADER_PARAMS__HPP_INCLUDED

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
*    PSG data loader configuration parameters
*
* ===========================================================================
*/

/* Name of PSG data loader driver */
#define NCBI_PSG_LOADER_DRIVER_NAME "psg_loader"

/* Name of NCBI PSG service to use, default: "" (allow PSG client library to decide */
#define NCBI_PSG_LOADER_PARAM_SERVICE_NAME "service_name"
/* Whether to ask non-split (original) entries from PSG server: default: false */
#define NCBI_PSG_LOADER_PARAM_NO_SPLIT "no_split"
/* Whether to ask whole entries in non-bulk requests from PSG server, default: false */
#define NCBI_PSG_LOADER_PARAM_WHOLE_TSE "whole_tse"
/* Whether to ask whole entries in bulk requests from PSG server, default: false */
#define NCBI_PSG_LOADER_PARAM_WHOLE_TSE_BULK "whole_tse_bulk"
/* Maximal number of processing threads, default: 10 */
#define NCBI_PSG_LOADER_PARAM_MAX_POOL_THREADS "max_pool_threads"
/* Number of PSG IO even loops to use, default: 3 */
#define NCBI_PSG_LOADER_PARAM_IO_EVENT_LOOPS "io_event_loops"
/* Whether to run background CDD prefetch job, default: false */
#define NCBI_PSG_LOADER_PARAM_PREFETCH_CDD "prefetch_cdd"
/* Number of tries to process non-bulk PSG requests before bailing out, default: 4 */
#define NCBI_PSG_LOADER_PARAM_RETRY_COUNT "retry_count"
/* Number of tries to process bulk PSG requests before bailing out, default: 8 */
#define NCBI_PSG_LOADER_PARAM_BULK_RETRY_COUNT "bulk_retry_count"
/* Whether to use IPG taxonomy id, default: false */
#define NCBI_PSG_LOADER_PARAM_IPG_TAX_ID "ipg_tax_id"

/* controls wait time before re-try after a failure: */
#define NCBI_PSG_LOADER_PARAM_WAIT_TIME "wait_time"
#define NCBI_PSG_LOADER_PARAM_WAIT_TIME_MAX "wait_time_max"
#define NCBI_PSG_LOADER_PARAM_WAIT_TIME_MULTIPLIER "wait_time_multiplier"
#define NCBI_PSG_LOADER_PARAM_WAIT_TIME_INCREMENT "wait_time_increment"

/* The following parameters are the same as in GENBANK section: */
/* Size of id resolution GC queues */
#define NCBI_PSG_LOADER_PARAM_ID_GC_SIZE "ID_GC_SIZE"
/* Expiration timeout of id resolution information in seconds (must be > 0) */
#define NCBI_PSG_LOADER_PARAM_ID_EXPIRATION_TIMEOUT "ID_EXPIRATION_TIMEOUT"
/* Expiration timeout of id resolution information in seconds if there's no data (must be > 0) */
#define NCBI_PSG_LOADER_PARAM_NO_ID_EXPIRATION_TIMEOUT "NO_ID_EXPIRATION_TIMEOUT"
/* Add WGS master descriptors to all WGS sequences */
#define NCBI_PSG_LOADER_PARAM_ADD_WGS_MASTER "ADD_WGS_MASTER"

#endif
