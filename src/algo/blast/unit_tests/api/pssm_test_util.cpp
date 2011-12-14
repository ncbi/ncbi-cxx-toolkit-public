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
 * Author: Christiam Camacho
 *
 */

/** @file psiblast_test_util.cpp
 * Utilities to develop and debug unit tests for BLAST
 */


#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include "blast_test_util.hpp"
#include "pssm_test_util.hpp"


using namespace std;
using namespace ncbi;
using namespace ncbi::blast;

/// @param query protein sequence in ncbistdaa with sentinel bytes
/// @param query_size length of the query sequence (w/o including sentinel
//  bytes)
BlastScoreBlk* InitializeBlastScoreBlk(const unsigned char* query,
                                       Uint4 query_size)
{
    const EBlastProgramType kProgramType = eBlastTypeBlastp;
    const double kScaleFactor = 1.0;
    Blast_Message* errors = NULL;
    short status = 0;

    // Setup the scoring options
    CBlastScoringOptions opts;
    status = BlastScoringOptionsNew(kProgramType, &opts);
    BOOST_REQUIRE(status == 0);

    // Setup the sequence block structure
    CBLAST_SequenceBlk query_blk;
    status = BlastSeqBlkNew(&query_blk);
    BOOST_REQUIRE(status == 0);
    status = BlastSeqBlkSetSequence(query_blk, query, query_size);
    BOOST_REQUIRE(status == 0);
    // don't delete the sequences upon exit!
    query_blk->sequence_allocated = FALSE;
    query_blk->sequence_start_allocated = FALSE;

    const Uint1 kNullByte = GetSentinelByte(eBlastEncodingProtein);
    BOOST_REQUIRE(query_blk.Get() != NULL);
    BOOST_REQUIRE(query_blk->sequence[0] != kNullByte);
    BOOST_REQUIRE(query_blk->sequence[query_blk->length - 1] != kNullByte);
    BOOST_REQUIRE(query_blk->sequence_start[0] == kNullByte);
    BOOST_REQUIRE(query_blk->sequence_start[query_blk->length + 1] ==
                  kNullByte);

    // Setup the query info structure
    CBlastQueryInfo query_info(TestUtil::CreateProtQueryInfo(query_size));

    BlastScoreBlk* retval = NULL;
    status = BlastSetup_ScoreBlkInit(query_blk,
                                     query_info,
                                     opts,
                                     kProgramType,
                                     &retval,
                                     kScaleFactor,
                                     &errors,
                                     &BlastFindMatrixPath);
    if (status) {
        throw runtime_error(errors->message);
    }
    BOOST_REQUIRE(retval->kbp_ideal);

    /*********************************************************************/

    return retval;
}
