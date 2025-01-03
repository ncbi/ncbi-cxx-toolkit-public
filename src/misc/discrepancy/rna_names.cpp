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
#include "discrepancy_core.hpp"
#include "utils.hpp"
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);


// RRNA_NAME_CONFLICTS

static const char* rrna_standard_name[] = {
    "4.5S ribosomal RNA",
    "5S ribosomal RNA",
    "5.8S ribosomal RNA",
    "12S ribosomal RNA",
    "15S ribosomal RNA",
    "16S ribosomal RNA",
    "18S ribosomal RNA",
    "21S ribosomal RNA",
    "23S ribosomal RNA",
    "25S ribosomal RNA",
    "26S ribosomal RNA",
    "28S ribosomal RNA",
    "large subunit ribosomal RNA",
    "small subunit ribosomal RNA"
};

static const size_t rrna_standard_name_len = ArraySize(rrna_standard_name);

static const pair<const char*, const char*> rrna_name_replace[] = {
    { "16S rRNA. Bacterial SSU", "16S ribosomal RNA" },
    { "23S rRNA. Bacterial LSU", "23S ribosomal RNA" },
    { "5S rRNA. Bacterial TSU", "5S ribosomal RNA" },
    { "Large Subunit Ribosomal RNA; lsuRNA; 23S ribosomal RNA", "23S ribosomal RNA" },
    { "Small Subunit Ribosomal RNA; ssuRNA; 16S ribosomal RNA", "16S ribosomal RNA" },
    { "Small Subunit Ribosomal RNA; ssuRNA; SSU ribosomal RNA", "small subunit ribosomal RNA" },
    { "Large Subunit Ribosomal RNA; lsuRNA; LSU ribosomal RNA", "large subunit ribosomal RNA" }
};

static const size_t rrna_name_replace_len = ArraySize(rrna_name_replace);


DISCREPANCY_CASE(RRNA_NAME_CONFLICTS, FEAT, eDisc | eSubmitter | eSmart | eFatal, "rRNA name conflicts")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsRna() && feat.GetData().GetRna().CanGetExt() && feat.GetData().GetRna().GetType() == CRNA_ref::eType_rRNA) {
            const string& name = feat.GetData().GetRna().GetExt().GetName();
            bool report = true;
            bool autofix = false;
            for (auto& s : rrna_standard_name) {
                if (NStr::EqualNocase(name, s)) {
                    if (name == s) {
                        report = false;
                    }
                    else {
                        autofix = true;
                    }
                    break;
                }
            }
            if (report) {
                for (auto& p : rrna_name_replace) {
                    if (NStr::EqualNocase(name, p.first)) {
                        autofix = true;
                        break;
                    }
                }
            }
            if (report) {
                m_Objs["[n] rRNA product name[s] [is] not standard. Correct the names to the standard format, eg \"16S ribosomal RNA\""].Add(*context.SeqFeatObjRef(feat, autofix ? &feat : nullptr)).Fatal();
            }
        }
    }
}


DISCREPANCY_AUTOFIX(RRNA_NAME_CONFLICTS)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    string name = sf->GetData().GetRna().GetExt().GetName();
    for (size_t i = 0; i < rrna_standard_name_len; i++) {
        if (NStr::EqualNocase(name, rrna_standard_name[i])) {
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*sf);
            new_feat->SetData().SetRna().SetExt().SetName(rrna_standard_name[i]);
            context.ReplaceSeq_feat(*obj, *sf, *new_feat);
            obj->SetFixed();
            return CRef<CAutofixReport>(new CAutofixReport("RRNA_NAME_CONFLICTS: [n] rRNA name[s] fixed", 1));
        }
    }
    for (size_t i = 0; i < rrna_name_replace_len; ++i) {
        if (NStr::EqualNocase(name, rrna_name_replace[i].first)) {
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*sf);
            new_feat->SetData().SetRna().SetExt().SetName(rrna_name_replace[i].second);
            context.ReplaceSeq_feat(*obj, *sf, *new_feat);
            obj->SetFixed();
            return CRef<CAutofixReport>(new CAutofixReport("RRNA_NAME_CONFLICTS: [n] rRNA name[s] fixed", 1));
        }
    }
    return CRef<CAutofixReport>();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
