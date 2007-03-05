#ifndef APP_BLAST_CLIENT___QUEUE_POLL__HPP
#define APP_BLAST_CLIENT___QUEUE_POLL__HPP

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
 * Author:  Kevin Bealer
 */

/// @file queue_poll.hpp
/// Queueing and Polling code for blast_client.
///
/// This contains the interface of queueing and polling code for blast_client.
/// There is only one function interface.  The goal of this interface is to
/// seperate the computational / network code from the UI (CNcbiApplication).


#include "search_opts.hpp"
#include "align_parms.hpp"

using namespace ncbi;


/// Queue the request with the given parameters; poll and display the results.
///
/// This function does the bulk of the work for the blast_client program.  It
/// queues the search, waits for processing, retrieves the results, and
/// displays the output.
/// @param program       Program to run (e.g. blastp, blastn...).
/// @param service       Service to run (e.g. plain, rpsblast, megablast...).
/// @param database      Database to search (nr, nt, pdb, etc).
/// @param opts          Optional netblast parameters.
/// @param verbose       Debugging output - displays netblast socket traffic.
/// @param trust_defline Assume sequence and defline are consistent with db.
/// @param raw_asn       Display text ASN.1, not formatted output.
/// @param alparms       Parameters to control output formatting.

Int4 QueueAndPoll(string                program,
                  string                service,
                  string                database,
                  CNetblastSearchOpts & opts,
                  CNcbiIstream        & query_in,
                  bool                  verbose,
                  bool                  trust_defline,
                  bool                  raw_asn,
                  CAlignParms         & alparms);

#endif // APP_BLAST_CLIENT___QUEUE_POLL__HPP
