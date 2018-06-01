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

#include <objects/taxon1/taxon1.hpp>

#include "wgs_tax.hpp"
#include "wgs_utils.hpp"
#include "wgs_seqentryinfo.hpp"


namespace wgsparse
{

static bool AddOrgRef(const COrg_ref& org_ref, list<COrgRefInfo>& org_refs)
{
    for (auto& cur_org : org_refs) {
        if (cur_org.m_org_ref->Equals(org_ref)) {
            return false;
        }
    }

    COrgRefInfo org_ref_info;
    org_ref_info.m_org_ref.Reset(new COrg_ref);
    org_ref_info.m_org_ref->Assign(org_ref);
    org_refs.push_back(org_ref_info);

    return true;
}

void CollectOrgRefs(const CSeq_entry& entry, list<COrgRefInfo>& org_refs)
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

static CTaxon1& GetTaxon1()
{
    static unique_ptr<CTaxon1> taxon1;
    if (taxon1 == nullptr) {
        taxon1.reset(new CTaxon1);
        taxon1->Init();
    }

    return *taxon1;
}

static CRef<COrg_ref> PerformLookup(const COrg_ref& org_ref, const string& taxname)
{
    CRef<COrg_ref> ret;

    CTaxon1& taxon1 = GetTaxon1();
    if (taxon1.IsAlive() || taxon1.Init()) {

        ret.Reset(new COrg_ref);
        ret->Assign(org_ref);
        CConstRef<CTaxon2_data> data = taxon1.LookupMerge(*ret);
        if (data.NotEmpty()) {

            if (data->IsSetIs_species_level() && !data->GetIs_species_level()) {
                ERR_POST_EX(0, 0, Warning << "Taxarch hit is not on species level (" << taxname << ").");
            }

            if (!data->IsSetOrg()) {
                ret.Reset();
            }
        }
        else {
            ret.Reset();
            if (taxon1.GetTaxIdByOrgRef(org_ref) < 0) {
                ERR_POST_EX(0, 0, Warning << "Not an unique Taxonomic Id for " << taxname << ".");
            }
            else {
                ERR_POST_EX(0, 0, Warning << "Taxon Id not found for [" << taxname << "].");
            }
        }
    }
    else {
        ERR_POST_EX(0, 0, Critical << "Taxonomy lookup failed for " << taxname << ". Error: [" << taxon1.GetLastError() << "]. Cannot generate ASN.1 for this entry.");
    }

    return ret;
}

static void NormalizeLineage(COrg_ref& org_ref)
{
    if (org_ref.IsSetLineage()) {
        string& lineage = org_ref.SetOrgname().SetLineage();
        lineage = NStr::Sanitize(lineage);
    }
}

void LookupCommonOrgRefs(list<COrgRefInfo>& org_refs)
{
    if (org_refs.empty()) {
        return;
    }

    CTaxon1& taxon1 = GetTaxon1();
    if (taxon1.IsAlive() || taxon1.Init()) {

        for (auto& org_ref : org_refs) {

            string taxname = org_ref.m_org_ref->IsSetTaxname() ? org_ref.m_org_ref->GetTaxname() : kEmptyStr;
            CRef<COrg_ref> cur_org_ref = PerformLookup(*org_ref.m_org_ref, taxname);

            if (cur_org_ref.NotEmpty()) {
                NormalizeLineage(*cur_org_ref);
                org_ref.m_org_ref_after_lookup = cur_org_ref;

                ERR_POST_EX(0, 0, Info << "Taxon Id _was_ found for [" << taxname << "].");
            }
        }
    }
    else {
        ERR_POST_EX(0, 0, Critical << "Taxonomy lookup failed to initialize. Error: [" << taxon1.GetLastError() << "]. Cannot proceed.");
    }
}

static const string& GetTaxname(const COrg_ref& org_ref)
{
    if (org_ref.IsSetTaxname()) {
        return org_ref.GetTaxname();
    }

    return kEmptyStr;
}

static void CheckMetagenomes(CBioSource& biosource, bool warn)
{
    COrg_ref& org_ref = biosource.SetOrg();

    if (!org_ref.IsSetLineage() || !HasLineage(org_ref.GetOrgname().GetLineage(), "metagenomes")) {
        return;
    }

    bool got_meta = false,
         got_sample = false;

    if (biosource.IsSetSubtype()) {
        for (auto& subtype : biosource.GetSubtype()) {
            if (subtype->IsSetSubtype()) {
                if (subtype->GetSubtype() == CSubSource::eSubtype_environmental_sample) {
                    got_sample = true;
                }
                else if (subtype->GetSubtype() == CSubSource::eSubtype_metagenomic) {
                    got_meta = true;
                }
            }
        }
    }

    if (!got_sample) {
        CRef<CSubSource> sample(new CSubSource);
        sample->SetSubtype(CSubSource::eSubtype_environmental_sample);
        biosource.SetSubtype().push_back(sample);

        if (warn) {
            ERR_POST_EX(0, 0, Warning << "Added \"environmental-sample\" SubSource for \"" << GetTaxname(org_ref) << "\" organism based on the contents of the lineage.");
        }
    }

    if (!got_meta) {
        CRef<CSubSource> meta(new CSubSource);
        meta->SetSubtype(CSubSource::eSubtype_metagenomic);
        biosource.SetSubtype().push_back(meta);

        if (warn) {
            ERR_POST_EX(0, 0, Warning << "Added \"metagenomic\" SubSource for \"" << GetTaxname(org_ref) << "\" organism based on the contents of the lineage.");
        }
    }
}

bool PerformTaxLookup(CBioSource& biosource, const list<COrgRefInfo>& org_refs, bool is_tax_lookup)
{
    if (!biosource.IsSetOrg()) {
        return !is_tax_lookup;
    }

    COrg_ref& org_ref = biosource.SetOrg();

    if (!org_refs.empty()) {

        bool found = false;
        for (auto& cur_org_ref : org_refs) {
            if (cur_org_ref.m_org_ref->Equals(org_ref)) {
                if (cur_org_ref.m_org_ref_after_lookup.NotEmpty()) {
                    org_ref.Assign(*cur_org_ref.m_org_ref_after_lookup);
                    found = true;
                }
                break;
            }
        }

        if (found) {
            CheckMetagenomes(biosource, false);
            return true;
        }
    }

    // TODO figure out how it is possible to reach this point
    if (is_tax_lookup) {
    }
    return false;
}

}
