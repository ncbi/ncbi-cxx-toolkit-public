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
 * Authors:  Josh Cherry
 *
 * File Description:  Tests operating on a pre-existing blastp result
 *
 */

#include <ncbi_pch.hpp>
#include <algo/seqqa/blastp_tests.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


// Expect a CSeq_annot whose data field is a set of alignments
bool CTestBlastp::CanTest(const CSerialObject& obj,
                             const CSeqTestContext* ctx) const
{
    const CSeq_annot* annot = dynamic_cast<const CSeq_annot*>(&obj);
    if (annot) {
        if (annot->GetData().IsAlign()) {
            return true;
        }
    }
    return false;
}


static CObject_id::TId s_GetTaxid(const CSeq_id& id, CScope& scope) {
    CBioseq_Handle hand = scope.GetBioseqHandle(id);
    const CSeq_descr& descr = hand.GetDescr();
    ITERATE (CSeq_descr::Tdata, desc, descr.Get()) {
        if ((*desc)->IsSource()) {
            const vector<CRef<CDbtag> >& db
                = (*desc)->GetSource().GetOrg().GetDb();
            ITERATE (vector<CRef<CDbtag> >, dbtag, db) {
                if ((*dbtag)->GetDb() == "taxon") {
                    return (*dbtag)->GetTag().GetId();
                }
            }
        }
    }
    throw runtime_error("didn't find taxon");
}


CRef<CSeq_test_result_set>
CTestBlastp_All::RunTest(const CSerialObject& obj,
                         const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> ref;
    const CSeq_annot* annot = dynamic_cast<const CSeq_annot*>(&obj);
    if ( !annot  ||  !ctx ) {
        return ref;
    }

    ref.Reset(new CSeq_test_result_set());

    CRef<CSeq_test_result> result = x_SkeletalTestResult("blastp_all");
    ref->Set().push_back(result);

    const list<CRef<CSeq_align> >& alns = annot->GetData().GetAlign();
    if (alns.size() == 0) {
        // No alignments (blastp found nothing)
        return ref;
    }

    CScope& scope = ctx->GetScope();

    // the first Seq-id in the first--or any--alignment should be
    // the id of the transcript
    const CSeq_id& prod_id
        = *alns.front()->GetSegs().GetDenseg().GetIds()[0];
    CObject_id::TId prod_taxid = s_GetTaxid(prod_id, scope);
    
    int score;
    int top_score = 0;
    int length_top_match;
    ITERATE (list<CRef<CSeq_align> >, aln, annot->GetData().GetAlign()) {
        const CSeq_id& match_id = *(*aln)->GetSegs().GetDenseg().GetIds()[1];
        if (s_GetTaxid(match_id, scope)
            == prod_taxid) {
            // ignore matches from same organism
            continue;
        }
        if (!(*aln)->GetNamedScore("score", score)) {
            // yikes, no blast score
            throw runtime_error("No BLAST score found");
        }
        if (score > top_score) {
            top_score = score;
            length_top_match =
                scope.GetBioseqHandle(match_id).GetBioseqLength();
        }
    }

    result->SetOutput_data().AddField("best_score", top_score);
    if (top_score > 0) {
        result->SetOutput_data()
            .AddField("length_top_match", length_top_match);
    }
    return ref;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/10/06 19:57:15  jcherry
 * Initial version
 *
 * ===========================================================================
 */
