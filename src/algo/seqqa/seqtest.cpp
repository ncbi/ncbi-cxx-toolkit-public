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
 * Authors:  Mike DiCuccio, Josh Cherry
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/seqqa/seqtest.hpp>
#include <objects/seqtest/Seq_test_result.hpp>
#include <serial/iterator.hpp>

#include <algo/seqqa/xcript_tests.hpp>
#include <algo/seqqa/prot_prod_tests.hpp>
#include <algo/seqqa/blastp_tests.hpp>
#include <algo/seqqa/single_aln_tests.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <corelib/ncbitime.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/Seq_annot.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CRef<CSeq_test_result> CSeqTest::x_SkeletalTestResult(const string& test_name)
{
    CRef<CSeq_test_result> result(new CSeq_test_result());

    result->SetTest(test_name);
    result->SetDate().SetToTime(CTime(CTime::eCurrent));

    result->SetOutput_data().SetType().SetStr("Seq-test-result");
    result->SetOutput_data().SetClass("NCBI");

    return result;
}


CRef<CSeq_test_result_set>
CSeqTest::x_TestAllCdregions(const CSerialObject& obj,
                             const CSeqTestContext* ctx,
                             const string& test_name,
                             TCdregionTester cdregion_tester)
{
    CRef<CSeq_test_result_set> ref;
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if ( !id  ||  !ctx ) {
        return ref;
    }

    SAnnotSelector sel(CSeqFeatData::eSubtype_cdregion);
    sel.SetResolveDepth(0);
    CSeq_loc loc;
    loc.SetWhole().Assign(*id);
    CFeat_CI feat_iter(ctx->GetScope(), loc, sel);
    for ( ;  feat_iter;  ++feat_iter) {

        if ( !ref ) {
            ref.Reset(new CSeq_test_result_set());
        }

        CRef<CSeq_test_result> result = x_SkeletalTestResult(test_name);
        ref->Set().push_back(result);

        (*cdregion_tester)(*id, ctx, feat_iter, *result);
    }

    return ref;
}


void CSeqTestManager::RegisterStandardTests()
{
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_CountCdregions);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_CdsFlags);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_Code_break);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_CdsStartCodon);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_CdsStopCodon);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_PrematureStopCodon);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_CompareProtProdToTrans);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_InframeUpstreamStart);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_InframeUpstreamStop);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_TranscriptCdsLength);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_TranscriptLength);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_Utrs);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_PolyA);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_Orfs);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestTranscript_CodingPropensity);

    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestProtProd_ProteinLength);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestProtProd_Cdd);
    RegisterTest(CSeq_id::GetTypeInfo(),
                 new CTestProtProd_EntrezNeighbors);

    RegisterTest(CSeq_annot::GetTypeInfo(),
                 new CTestBlastp_All);

    RegisterTest(CSeq_align::GetTypeInfo(),
                 new CTestSingleAln_All);

}


void CSeqTestManager::RegisterTest(const CTypeInfo* info,
                                   CSeqTest* test)
{
    m_Tests.insert(TTests::value_type(info, CRef<CSeqTest>(test)));
}


CRef<CSeq_test_result_set>
CSeqTestManager::RunTests(const CSerialObject& obj,
                          const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> results(new CSeq_test_result_set());

    const CTypeInfo* info = obj.GetThisTypeInfo();
    pair<TTests::iterator, TTests::iterator> iter_pair =
        m_Tests.equal_range(info);
    for ( ;  iter_pair.first != iter_pair.second;  ++iter_pair.first) {
        TTests::iterator iter = iter_pair.first;
        if (!(*iter->second).CanTest(obj, ctx)) {
            continue;
        }
        CRef<CSeq_test_result_set> ref = (*iter->second).RunTest(obj, ctx);
        if (ref) {
            results->Set().insert(results->Set().end(),
                                  ref->Get().begin(), ref->Get().end());
        }
    }

    return results;
}


CSeqTestManager::TResults
CSeqTestManager::RunTests(const CSerialObject& obj,
                          const CTypeInfo& info,
                          const CSeqTestContext* ctx)
{
    TResults results;

    CTreeIterator iter(CBeginInfo((void*)&obj, &info));
    for ( ;  iter;  ++iter) {

        CRef<CSeq_test_result_set> ref = 
            RunTests(*static_cast<CSerialObject *>(iter.Get().GetObjectPtr()),
                     ctx);
        if (ref  &&  ref->Get().size()) {
            CRef<CSeqTestResults> r(new CSeqTestResults());
            r->SetResults(*ref);
            results.push_back(r);
        }
    }

    return results;
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/03/14 18:19:02  grichenk
 * Added SAnnotSelector(TFeatSubtype), fixed initialization of CFeat_CI and
 * SAnnotSelector.
 *
 * Revision 1.7  2005/01/12 17:17:08  vasilche
 * Avoid performance warning on MSVC.
 *
 * Revision 1.6  2004/10/21 21:02:03  jcherry
 * Added test for code-breaks in CDS feature and recording of
 * lengths of protein product and translation.
 *
 * Revision 1.5  2004/10/18 21:07:46  jcherry
 * Added premature stop codon test
 *
 * Revision 1.4  2004/10/13 15:49:06  jcherry
 * Use resolve depth of zero rather than adaptive depth
 *
 * Revision 1.3  2004/10/06 21:49:05  jcherry
 * Use "adaptive depth" (apparently will be necessary for use
 * of "fake" transcripts representing genomic annotation)
 *
 * Revision 1.2  2004/10/06 21:09:50  jcherry
 * Fixed msvc compile errors
 *
 * Revision 1.1  2004/10/06 19:57:15  jcherry
 * Initial version
 *
 * ===========================================================================
 */
