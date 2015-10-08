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
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/seqqa/prot_prod_tests.hpp>
#include <corelib/ncbitime.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqtest/Seq_test_result.hpp>
#include <objects/entrez2/entrez2_client.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <connect/ncbi_conn_stream.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

bool CTestProtProd::CanTest(const CSerialObject& obj,
                            const CSeqTestContext* ctx) const
{
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if (id  &&  ctx) {
        CBioseq_Handle handle = ctx->GetScope().GetBioseqHandle(*id);

        CSeqdesc_CI iter(handle, CSeqdesc::e_Molinfo);
        for ( ;  iter;  ++iter) {
            const CMolInfo& info = iter->GetMolinfo();
            if (info.GetBiomol() == CMolInfo::eBiomol_peptide) {
                return true;
            }
        }
    }
    return false;
}


CRef<CSeq_test_result_set>
CTestProtProd_ProteinLength::RunTest(const CSerialObject& obj,
                                     const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> ref;
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if ( !id  ||  !ctx ) {
        return ref;
    }

    ref.Reset(new CSeq_test_result_set());

    CRef<CSeq_test_result> result = x_SkeletalTestResult("protein_length");
    ref->Set().push_back(result);

    int len = ctx->GetScope()
        .GetBioseqHandle(dynamic_cast<const CSeq_id&>(obj)).GetInst_Length();
    result->SetOutput_data()
        .AddField("length", len);
    return ref;
}


CRef<CSeq_test_result_set>
CTestProtProd_Cdd::RunTest(const CSerialObject& obj,
                           const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> ref;
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if ( !id  ||  !ctx ) {
        return ref;
    }

    ref.Reset(new CSeq_test_result_set());

    CRef<CSeq_test_result> result = x_SkeletalTestResult("cdd");
    ref->Set().push_back(result);

    CBioseq_Handle hand = ctx->GetScope().GetBioseqHandle(*id);
    CAnnot_CI it(hand);
    int hits = 0;
    bool has_partial = false;
    while (it) {
        if (it->IsNamed() && it->GetName() == "CDDSearch") {
            const list<CRef<CSeq_feat> >& ftable
                = it->GetCompleteSeq_annot()->GetData().GetFtable();
            hits = ftable.size();
            ITERATE (list<CRef<CSeq_feat> >, it, ftable) {
                if ((*it)->IsSetPartial() && (*it)->GetPartial()) {
                    has_partial = true;
                    break;
                }
            }
            break;
        }
        ++it;
    }

    CRef<CSeq_annot> annot(new CSeq_annot);
    result->SetOutput_data()
        .AddField("annotated_cdd_hits", hits);
    result->SetOutput_data()
        .AddField("has_partial_cdd_hit", has_partial);
    return ref;
}


// Find the taxid from a bioseq handle by iterating over
// Seqdesc's of type "source".  Return 0 if not found.
int s_GetTaxId(const CBioseq_Handle& hand)
{
    CSeqdesc_CI it(hand, CSeqdesc::e_Source);
    int taxid;
    while (it) {
        taxid = it->GetSource().GetOrg().GetTaxId();
        if (taxid != 0) {
            return taxid;
        }
    }
    return 0;
}


bool CTestProtProd_EntrezNeighbors::CanTest(const CSerialObject& obj,
                                            const CSeqTestContext* ctx) const
{
    // Does it pass the base class criteria?
    if (!CTestProtProd::CanTest(obj, ctx)) {
        return false;
    }
    // If so, it must be a CSeq_id.  Is it resolvable to a gi?
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    CBioseq_Handle hand = ctx->GetScope().GetBioseqHandle(*id);
    try {
        sequence::GetId(hand, sequence::eGetId_ForceGi).GetGi();
    }
    catch (std::exception&) {
        return false;
    }
    return true;
}


CRef<CSeq_test_result_set>
CTestProtProd_EntrezNeighbors::RunTest(const CSerialObject& obj,
                                       const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> ref;
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if ( !id  ||  !ctx ) {
        return ref;
    }

    ref.Reset(new CSeq_test_result_set());

    CRef<CSeq_test_result> result = x_SkeletalTestResult("entrez_neighbors");
    ref->Set().push_back(result);

    CBioseq_Handle hand = ctx->GetScope().GetBioseqHandle(*id);
    TGi gi = sequence::GetId(hand, sequence::eGetId_ForceGi).GetGi();
    int taxid = s_GetTaxId(hand);
    if (taxid == 0) {
        throw runtime_error("CTestProtProd_EntrezNeighbors::RunTest: "
                            "taxid not found for " + id->GetSeqIdString(true));
    }
    const unsigned int kChunkSize = 50;
    CEntrez2Client e2c;
    vector<TGi> neigh;
    e2c.GetNeighbors(gi, "protein", "protein", neigh);
    vector<TGi> sp_neigh;
    vector<TGi> neigh_subset;
    neigh_subset.reserve(kChunkSize);
    for (unsigned int start = 0; start < neigh.size(); start += kChunkSize) {
        neigh_subset.clear();
        for (unsigned int i = 0; i < kChunkSize; ++i) {
            if (start + i == neigh.size()) {
                break;
            }
            neigh_subset.push_back(neigh[start + i]);
        }
        e2c.FilterIds(neigh_subset, "protein", "srcdb_swiss-prot[PROP] NOT txid"
                      + NStr::NumericToString(taxid) + "[ORGN]", sp_neigh);
        if (!sp_neigh.empty()) {
            break;
        }
    }
    result->SetOutput_data().AddField("has_swissprot_neighbor_different_taxid",
                                      !sp_neigh.empty());
    if (!sp_neigh.empty()) {
        // Order not necessarily preserved by FilterIds, so figure out
        // which element of sp_neigh comes first in neigh
        map<TGi, unsigned int> index;
        for (unsigned int i = 0;  i < neigh_subset.size();  ++i) {
            index[neigh_subset[i]] = i;
        }
        unsigned int lowest_index = neigh_subset.size();
        TGi first_gi = ZERO_GI;  // initialize to avoid compiler warning
        for (unsigned int i = 0;  i < sp_neigh.size();  ++i) {
            if (index[sp_neigh[i]] < lowest_index) {
                lowest_index = index[sp_neigh[i]];
                first_gi = GI_FROM(int, sp_neigh[i]);
            }
        }
        CSeq_id neigh_id;
        neigh_id.SetGi(first_gi);
        result->SetOutput_data()
            .AddField("top_match_seq_id", neigh_id.GetSeqIdString(true));
        int length_top_match =
            ctx->GetScope().GetBioseqHandle(neigh_id).GetBioseqLength();
        result->SetOutput_data()
            .AddField("length_top_match", length_top_match);
    }
    return ref;
}


END_NCBI_SCOPE
