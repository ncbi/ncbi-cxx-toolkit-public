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
 * Authors: Ning Ma
 *
 */

/** @file hspfilter_best_hit_unit_test.cpp
 * Unit tests for the best hit.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/core/hspfilter_besthit.h>

#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

using namespace std;
using namespace ncbi;

BlastHSPBestHitParams* s_GetBestHitParams()
{
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    const bool kIsGapped = true;
    const double kOverhang = 0.1;

    BlastHitSavingOptions* hit_options = NULL;
    BlastHitSavingOptionsNew(kProgram, &hit_options, kIsGapped);
    BlastHSPBestHitOptions* best_hit_opts =
        BlastHSPBestHitOptionsNew(kOverhang, kBestHit_ScoreEdgeDflt);
    BlastHSPBestHitParams* best_hit_params = 
         BlastHSPBestHitParamsNew(hit_options, best_hit_opts, false, kIsGapped);
    best_hit_opts = BlastHSPBestHitOptionsFree(best_hit_opts);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    return best_hit_params;
}

BOOST_AUTO_TEST_SUITE(hspfilter_best_hit)


BOOST_AUTO_TEST_CASE(HSPBestHitParams)
{
    const double kOverhang = 0.1;

    BlastHSPBestHitParams* best_hit_params = s_GetBestHitParams();

    BOOST_REQUIRE(best_hit_params);

    BOOST_REQUIRE_EQUAL(best_hit_params->overhang, kOverhang);

    best_hit_params = BlastHSPBestHitParamsFree(best_hit_params);

    BOOST_REQUIRE(best_hit_params == NULL);
}

BOOST_AUTO_TEST_CASE(HSPBestHitWriter)
{
    BlastQueryInfo query_info; 
    BlastHSPBestHitParams* best_hit_params = s_GetBestHitParams();

    BlastHSPWriterInfo* writer_info = BlastHSPBestHitInfoNew(best_hit_params);
    BOOST_REQUIRE(writer_info);
    BOOST_REQUIRE(writer_info->NewFnPtr);
    BOOST_REQUIRE(writer_info->params);
    BlastHSPWriter* writer = BlastHSPWriterNew(&writer_info, &query_info);
    BOOST_REQUIRE(writer_info == NULL);
    BOOST_REQUIRE(writer);
    // Following call also frees best_hit_params
    writer = writer->FreeFnPtr(writer);
    BOOST_REQUIRE(writer == NULL);
}

BOOST_AUTO_TEST_CASE(HSPBestHitPipe)
{
    BlastQueryInfo query_info; 
    BlastHSPBestHitParams* best_hit_params = s_GetBestHitParams();

    BlastHSPPipeInfo* pipe_info = BlastHSPBestHitPipeInfoNew(best_hit_params);
    BOOST_REQUIRE(pipe_info);
    BOOST_REQUIRE(pipe_info->NewFnPtr);
    BOOST_REQUIRE(pipe_info->params);
    BlastHSPPipe* pipe = BlastHSPPipeNew(&pipe_info, &query_info);
    BOOST_REQUIRE(pipe_info == NULL);
    BOOST_REQUIRE(pipe);
    // Following call also frees best_hit_params
    pipe = pipe->FreeFnPtr(pipe);
    BOOST_REQUIRE(pipe == NULL);
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
