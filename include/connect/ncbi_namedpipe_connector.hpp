#ifndef CONNECT___NCBI_NAMEDPIPE_CONNECTOR__HPP
#define CONNECT___NCBI_NAMEDPIPE_CONNECTOR__HPP

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
 * Author:  Vladimir Ivanov
 *
 *
 */

/// @file ncbi_namedpipe_connector.hpp
/// Implement CONNECTOR for a named pipe interprocess communication
/// (based on the NCBI CNamedPipe).
///
/// See in "connectr.h" for the detailed specification of the underlying
/// connector("CONNECTOR", "SConnectorTag") methods and structures.
 

#include <connect/ncbi_namedpipe.hpp>
#include <connect/ncbi_connector.h>


/** @addtogroup Connectors
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/* Create new CONNECTOR structure to handle a data transfer between two
 * processes over named pipe. Return NULL on error.
 *
 */

/// Create CNamedPipe-based CONNECTOR.
///
/// Create new CONNECTOR structure to handle a data transfer between two
/// process over nemed pipe.  Return NULL on error.
extern NCBI_XCONNECT_EXPORT CONNECTOR NAMEDPIPE_CreateConnector
(const string& pipename,
 size_t        pipebufsize = 0 /** use default */
 );


END_NCBI_SCOPE


/* @} */

#endif /* CONNECT___NCBI_NAMEDPIPE_CONNECTOR__HPP */
