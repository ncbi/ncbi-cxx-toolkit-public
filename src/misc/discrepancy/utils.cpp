/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Sema Kachalo
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/seq_macros.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include "utils.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);


void AddComment(CSeq_feat& feat, const string& comment)
{
    if (!comment.empty()) {
        if (feat.IsSetComment() && !feat.GetComment().empty()) {
            if (feat.GetComment().find(comment) != -1) {
                return;
            }
            if (!NStr::EndsWith(feat.GetComment(), ";")) {
                feat.SetComment() += "; ";
            }
        }
        feat.SetComment() += comment;
    }
}


static string GetProductName(const CProt_ref& prot)
{
    return prot.IsSetName() && prot.GetName().size() > 0 ? prot.GetName().front() : "";
}


string GetProductName(const CSeq_feat& feat, objects::CScope& scope)
{
    if (feat.IsSetProduct()) {
        CBioseq_Handle prot_bsq = sequence::GetBioseqFromSeqLoc(feat.GetProduct(), scope);
        if (prot_bsq) {
            CFeat_CI prot_ci(prot_bsq, CSeqFeatData::e_Prot);
            if (prot_ci) {
                return GetProductName(prot_ci->GetOriginalFeature().GetData().GetProt());
            }
        }
    }
    else if (feat.IsSetXref()) {
        for (const auto& it : feat.GetXref()) {
            if (it->IsSetData() && it->GetData().IsProt()) {
                return GetProductName(it->GetData().GetProt());
            }
        }
    }
    return kEmptyStr;
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
