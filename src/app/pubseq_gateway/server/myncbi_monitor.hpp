#ifndef PUBSEQ_GATEWAY_MYNCBI_MONITOR__HPP
#define PUBSEQ_GATEWAY_MYNCBI_MONITOR__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   PSG server Cassandra monitoring thread
 *
 */

// The function is executed in a thread.
// It monitors changes in the DB as well as the initial connection to the DB.
void MyNCBIMonitorThreadedFunction(size_t  my_ncbi_dns_resolve_ok_period_sec,
                                   size_t  my_ncbi_dns_resolve_fail_period_sec,
                                   size_t  my_ncbi_test_ok_period_sec,
                                   size_t  my_ncbi_test_fail_period_sec,
                                   bool *  last_my_ncbi_resolve_oK,
                                   bool *  last_my_ncbi_test_ok);

#endif /* PUBSEQ_GATEWAY_MYNCBI_MONITOR__HPP */

