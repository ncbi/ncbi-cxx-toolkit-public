#ifndef CONNECT___NCBI_MEMORY_CONNECTOR__H
#define CONNECT___NCBI_MEMORY_CONNECTOR__H

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
 */

#include <connect/ncbi_buffer.h>
#include <connect/ncbi_connector.h>


/** @addtogroup Connectors
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/* Create new CONNECTOR structure to handle a data transfer in-memory.
 * Onwership of "buf" (if passed non-NULL) is not assumed by the connector.
 * Return NULL on error.
 */
extern NCBI_XCONNECT_EXPORT CONNECTOR MEMORY_CreateConnector(void);


extern NCBI_XCONNECT_EXPORT CONNECTOR MEMORY_CreateConnectorEx(BUF buf);


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.6  2006/03/30 17:39:24  lavr
 * MEMORY_Connector: Remove unnecessary lock
 *
 * Revision 6.5  2004/10/27 18:44:14  lavr
 * +MEMORY_CreateConnectorEx()
 *
 * Revision 6.4  2003/04/09 19:05:45  siyan
 * Added doxygen support
 *
 * Revision 6.3  2003/01/08 01:59:33  lavr
 * DLL-ize CONNECT library for MSVC (add NCBI_XCONNECT_EXPORT)
 *
 * Revision 6.2  2002/09/19 18:01:14  lavr
 * Header file guard macro changed; log moved to the end
 *
 * Revision 6.1  2002/02/20 19:29:35  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_MEMORY_CONNECTOR__H */
