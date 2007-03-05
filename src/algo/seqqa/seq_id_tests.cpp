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
#include <algo/seqqa/seq_id_tests.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqtest/Seq_test_result.hpp>
#include <objmgr/seqdesc_ci.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


bool CTestSeqId::CanTest(const CSerialObject& obj,
                              const CSeqTestContext* ctx) const
{
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    return bool(id);
}


CRef<CSeq_test_result_set>
CTestSeqId_Biomol::RunTest(const CSerialObject& obj,
                           const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> ref;
    const CSeq_id* id = dynamic_cast<const CSeq_id*>(&obj);
    if ( !id  ||  !ctx ) {
        return ref;
    }

    ref.Reset(new CSeq_test_result_set());

    CBioseq_Handle handle = ctx->GetScope().GetBioseqHandle(*id);

    bool is_mrna = false;
    bool is_pre_rna = false;
    bool is_peptide = false;
    CSeqdesc_CI iter(handle, CSeqdesc::e_Molinfo);
    for ( ;  iter;  ++iter) {
        const CMolInfo& info = iter->GetMolinfo();
        if (info.GetBiomol() == CMolInfo::eBiomol_mRNA) {
            is_mrna = true;
        }
        if (info.GetBiomol() == CMolInfo::eBiomol_pre_RNA) {
            is_pre_rna = true;
        }
        if (info.GetBiomol() == CMolInfo::eBiomol_peptide) {
            is_peptide = true;
        }
    }

    CRef<CSeq_test_result> result = x_SkeletalTestResult("biomol");
    ref->Set().push_back(result);

    result->SetOutput_data().AddField("is_mrna", is_mrna);
    result->SetOutput_data().AddField("is_pre_rna", is_pre_rna);
    result->SetOutput_data().AddField("is_peptide", is_peptide);

    return ref;
}


END_NCBI_SCOPE
