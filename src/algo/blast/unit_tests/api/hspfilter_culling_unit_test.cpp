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
 * Authors: Tom Madden
 *
 */

/** @file hspfilter_culling_unit_test.cpp
 * Unit tests for the culling.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/core/hspfilter_culling.h>

#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

using namespace std;
using namespace ncbi;

BlastHSPCullingParams* s_GetCullingParams()
{
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    const bool kIsGapped = true;
    const int kMax = 500;

    BlastHitSavingOptions* hit_options = NULL;
    BlastHitSavingOptionsNew(kProgram, &hit_options, kIsGapped);
    BlastHSPCullingOptions* culling_opts = BlastHSPCullingOptionsNew(kMax);
    BlastHSPCullingParams* culling_params = 
         BlastHSPCullingParamsNew(hit_options, culling_opts, false, kIsGapped);
    culling_opts = BlastHSPCullingOptionsFree(culling_opts);
    BOOST_REQUIRE(culling_opts == NULL);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    BOOST_REQUIRE(hit_options == NULL);
    return culling_params;

}

BOOST_AUTO_TEST_SUITE(hspfilter_culling)


BOOST_AUTO_TEST_CASE(HSPCullingParams)
{
    const int kMax = 500;

    BlastHSPCullingParams* culling_params = s_GetCullingParams();

    BOOST_REQUIRE(culling_params);

    BOOST_REQUIRE_EQUAL(culling_params->culling_max, kMax);
    BOOST_REQUIRE(culling_params->hsp_num_max > kMax);

    culling_params = BlastHSPCullingParamsFree(culling_params);

    BOOST_REQUIRE(culling_params == NULL);
}

BOOST_AUTO_TEST_CASE(HSPCullingWriter)
{
    BlastQueryInfo query_info;
    BlastHSPCullingParams* culling_params = s_GetCullingParams();

    BlastHSPWriterInfo* writer_info = BlastHSPCullingInfoNew(culling_params);
    BOOST_REQUIRE(writer_info);
    BOOST_REQUIRE(writer_info->NewFnPtr);
    BOOST_REQUIRE(writer_info->params);
    BlastHSPWriter* writer = BlastHSPWriterNew(&writer_info, &query_info);
    BOOST_REQUIRE(writer_info == NULL);
    BOOST_REQUIRE(writer);
    // Following call also frees culling_params
    writer = writer->FreeFnPtr(writer);
    BOOST_REQUIRE(writer == NULL);
}

BOOST_AUTO_TEST_CASE(HSPCullingPipe)
{
    BlastQueryInfo query_info;
    BlastHSPCullingParams* culling_params = s_GetCullingParams();

    BlastHSPPipeInfo* pipe_info = BlastHSPCullingPipeInfoNew(culling_params);
    BOOST_REQUIRE(pipe_info);
    BOOST_REQUIRE(pipe_info->NewFnPtr);
    BOOST_REQUIRE(pipe_info->params);
    BlastHSPPipe* pipe = BlastHSPPipeNew(&pipe_info, &query_info);
    BOOST_REQUIRE(pipe_info == NULL);
    BOOST_REQUIRE(pipe);
    // Following call also frees culling_params
    pipe = pipe->FreeFnPtr(pipe);
    BOOST_REQUIRE(pipe == NULL);
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
