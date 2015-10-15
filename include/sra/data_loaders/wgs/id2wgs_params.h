#ifndef SRA__LOADER__WGS__ID2WGS_PARAMS__H
#define SRA__LOADER__WGS__ID2WGS_PARAMS__H

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
*    CID2Processor WGS VDB plugin configuration parameters
*
* ===========================================================================
*/

/* Name of WGS VDB plugin driver */
#define NCBI_ID2PROC_WGS_DRIVER_NAME "wgs"

/*
 * configuration of CID2Processor WGS plugin is read from app config section
 * [id2proc/wgs]
 *
 */

/* Size of cache for open VDB accessions */
#define NCBI_ID2PROC_WGS_PARAM_VDB_CACHE_SIZE "vdb_cache_size"

/* Time in seconds between background updates of GI/accession indexes */
#define NCBI_ID2PROC_WGS_PARAM_INDEX_UPDATE_TIME "index_update_time"

/* Compress Seq-entries in replies (0-never,1-some,2-always) */
#define NCBI_ID2PROC_WGS_PARAM_COMPRESS_DATA "compress_data"

#endif//SRA__LOADER__WGS__ID2WGS_PARAMS__H
