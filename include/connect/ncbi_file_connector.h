#ifndef CONNECT___NCBI_FILE_CONNECTOR__H
#define CONNECT___NCBI_FILE_CONNECTOR__H

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
 * Author:  Vladimir Alekseyev, Denis Vakatov
 *
 * File Description:
 *   Implement CONNECTOR for a FILE stream
 *
 *   See in "connectr.h" for the detailed specification of the underlying
 *   connector("CONNECTOR", "SConnectorTag") methods and structures.
 *
 */

#include <connect/ncbi_connector.h>


/** @addtogroup Connectors
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/* Create new CONNECTOR structure to handle a data transfer between two files.
 * Return NULL on error.
 */
extern NCBI_XCONNECT_EXPORT CONNECTOR FILE_CreateConnector
(const char* in_file_name,   /* to read data from */
 const char* out_file_name   /* to write the read data to */
 );


/* Open mode for the output data file
 */
typedef enum {
    eFCM_Truncate, /* create new or replace existing file */
    eFCM_Seek,     /* seek before the first i/o */
    eFCM_Append    /* add after the end of file */
} EFileConnMode;


/* Extended file connector attributes
 */
typedef struct {
    size_t        r_pos;  /* file position to start reading at */
    EFileConnMode w_mode; /* how to open output file */
    size_t        w_pos;  /* eFCM_Seek mode only: begin to write at "w_pos" */
} SFileConnAttr;


/* An extended version of FILE_CreateConnector().
 */
extern NCBI_XCONNECT_EXPORT CONNECTOR FILE_CreateConnectorEx
(const char*          in_file_name,
 const char*          out_file_name,
 const SFileConnAttr* attr
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_FILE_CONNECTOR__H */
