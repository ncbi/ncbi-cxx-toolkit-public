#ifndef BDB___QUERY_PARSER__HPP
#define BDB___QUERY_PARSER__HPP
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:
 *	 Query parser.
 *
 */

/// @file bdb_query_parser.hpp
/// Query parser for BDB library.


#include <corelib/ncbistd.hpp>
#include <bdb/bdb_query.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup BDB_Query
 *
 * @{
 */

/// Parse query string, build the correct the query statement.
///
/// @param 
///    query_str - source query string
/// @param 
///    query - query object owns the statement (query clause tree) after
///    successful parsing.
void NCBI_BDB_EXPORT BDB_ParseQuery(const char* query_str, CBDB_Query* query);

/* @} */

END_NCBI_SCOPE

#endif
