#ifndef NCBI_MEMORY_CONNECTOR__H
#define NCBI_MEMORY_CONNECTOR__H

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
 * ===========================================================================
 *
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   In-memory CONNECTOR
 *
 *   See <connect/ncbi_connector.h> for the detailed specification of
 *   the connector's methods and structures.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2002/02/20 19:29:35  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_connector.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Create new CONNECTOR structure to handle a data transfer in-memory.
 * Use lock (may be NULL) to protect write/read operations.
 * Return NULL on error.
 */
extern CONNECTOR MEMORY_CreateConnector(MT_LOCK lock);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_MEMORY_CONNECTOR__H */
