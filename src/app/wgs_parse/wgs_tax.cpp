/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include "wgs_tax.hpp"
#include "wgs_utils.hpp"

namespace wgsparse
{

static bool AddOrgRef(const COrg_ref& org_ref, list<CRef<COrg_ref>>& org_refs)
{
    for (auto& cur_org : org_refs) {
        if (cur_org->Equals(org_ref)) {
            return false;
        }
    }

    CRef<COrg_ref> new_org_ref(new COrg_ref);
    new_org_ref->Assign(org_ref);
    org_refs.push_back(new_org_ref);

    return true;
}

void CollectOrgRefs(const CSeq_entry& entry, list<CRef<COrg_ref>>& org_refs)
{
    static const size_t MAX_ORG_REF_NUM = 10;
    if (org_refs.size() > MAX_ORG_REF_NUM) {
        return;
    }

    const CSeq_descr* descrs = nullptr;
    if (GetDescr(entry, descrs)) {

        if (descrs && descrs->IsSet()) {
            for (auto& descr : descrs->Get()) {
                if (descr->IsSource() && descr->GetSource().IsSetOrg()) {
                    if (AddOrgRef(descr->GetSource().GetOrg(), org_refs) && org_refs.size() > MAX_ORG_REF_NUM) {
                        return;
                    }
                }
            }
        }
    }

    const CBioseq::TAnnot* annots = nullptr;
    if (GetAnnot(entry, annots)) {

        if (annots) {
            for (auto& annot : *annots) {
                if (annot->IsFtable()) {

                    for (auto& feat : annot->GetData().GetFtable()) {
                        if (feat->IsSetData() && feat->GetData().IsBiosrc() && feat->GetData().GetBiosrc().IsSetOrg()) {
                            if (AddOrgRef(feat->GetData().GetBiosrc().GetOrg(), org_refs) && org_refs.size() > MAX_ORG_REF_NUM) {
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}

}