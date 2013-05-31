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
 * Author: Nathan Bouk
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/genomecoll/genome_collection__.hpp>
#include <objects/genomecoll/genomic_collections_cli.hpp>
#include <algo/id_mapper/id_mapper.hpp>


#include <boost/test/output_test_stream.hpp> 
using boost::test_tools::output_test_stream;

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BOOST_AUTO_TEST_SUITE(TestSuiteTrimAlignment)

BOOST_AUTO_TEST_CASE(TestCaseUcscToRefSeqMapping)
{
    // Fetch Gencoll
    CGenomicCollectionsService GCService;
    CRef<CGC_Assembly> GenColl = GCService.GetAssembly("GCF_000001405.13", 
                                    CGCClient_GetAssemblyRequest::eLevel_scaffold);
    
    // Make a Spec
    CGencollIdMapper::SIdSpec MapSpec;
    MapSpec.TypedChoice = CGC_TypedSeqId::e_Refseq;
    MapSpec.Alias = CGC_SeqIdAlias::e_Public;

    // Do a Map
    CGencollIdMapper Mapper(GenColl);
    CSeq_loc OrigLoc;
    OrigLoc.SetWhole().SetLocal().SetStr("chr1");

    CRef<CSeq_loc> Result = Mapper.Map(OrigLoc, MapSpec);
    
    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(Result->GetId()->GetSeqIdString(true), "NC_000001.10");
}

BOOST_AUTO_TEST_SUITE_END();

