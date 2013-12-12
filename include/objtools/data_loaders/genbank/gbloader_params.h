#ifndef GBLOADER_PARAMS__HPP_INCLUDED
#define GBLOADER_PARAMS__HPP_INCLUDED

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
*    GenBank data loader configuration parameters
*
* ===========================================================================
*/

/* Name of GenBank data loader driver */
#define NCBI_GBLOADER_DRIVER_NAME "genbank"

/* Two following parameters enumerate readers/writers in the order of
 * their priority separated by semicolon ';'.
 * Each item is the list of driver names separated by colon ':' to try,
 * the first one avaliable will be used by loader with corresponding priority.
 * To skip some priority you can put empty string between semicolons.
 * If list of driver names is prepended by dash '-' this priority slot is
 * optional and GBLoader initialization will not fail if none of reader/writer
 * drivers is available.
 */
/* List of readers */
#define NCBI_GBLOADER_PARAM_READER_NAME "ReaderName"
/* available driver names for reader are: id1 id2 pubseqos cache */
/* List of writers */
#define NCBI_GBLOADER_PARAM_WRITER_NAME "WriterName"
/* available driver names for writer are: cache_writer */
/* List of readers and writers */
#define NCBI_GBLOADER_PARAM_LOADER_METHOD "loader_method"

/* Size of id resolution GC queues */
#define NCBI_GBLOADER_PARAM_ID_GC_SIZE "ID_GC_SIZE"
/* Whether to open first connection immediately or not (default: true) */
#define NCBI_GBLOADER_PARAM_PREOPEN  "preopen"
/* Expiration timeout of id resolution information in seconds (must be > 0) */
#define NCBI_GBLOADER_PARAM_ID_EXPIRATION_TIMEOUT "ID_EXPIRATION_TIMEOUT"

#endif
