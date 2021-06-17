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
* Author:  Colleen Bollin
*
* File Description:
*   Extends CAutoDef to create docsum titles (which require a call to taxonomy)
*/

#include <ncbi_pch.hpp>
#include <objtools/edit/autodef_with_tax.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objects/taxon3/Taxon3_request.hpp>
#include <objects/taxon3/T3Request.hpp>
#include <objects/taxon3/SequenceOfInt.hpp>
#include <objects/taxon3/Taxon3_reply.hpp>
#include <objects/taxon3/T3Reply.hpp>
#include <objects/taxon3/taxon3.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CConstRef<CUser_object> GetOptionsForSet(CBioseq_set_Handle set)
{
    CConstRef<CUser_object> options(NULL);
    CBioseq_CI b(set, CSeq_inst::eMol_na);
    while (b && !options) {
        CSeqdesc_CI desc(*b, CSeqdesc::e_User);
        while (desc && desc->GetUser().GetObjectType() != CUser_object::eObjectType_AutodefOptions) {
            ++desc;
        }
        if (desc) {
            options.Reset(&(desc->GetUser()));
        }
    }
    return options;
}

bool CAutoDefWithTaxonomy::RegeneratePopsetTitles(CSeq_entry_Handle se)
{
    bool any = false;
    // update the title of the set
    for (CSeq_entry_CI si(se, CSeq_entry_CI::fRecursive | CSeq_entry_CI::fIncludeGivenEntry, CSeq_entry::e_Set); si; ++si) {
        if (si->IsSet() && si->GetSet().GetCompleteBioseq_set()->NeedsDocsumTitle()) {
            CAutoDefWithTaxonomy autodef;
            CConstRef<CUser_object> options = GetOptionsForSet(si->GetSet());
            if (options) {
                autodef.SetOptionsObject(*options);
            }
            autodef.AddSources(se);
            string defline = autodef.GetDocsumDefLine(*si);

            bool found_existing = false;
            CBioseq_set_EditHandle bsseh(si->GetSet());
            NON_CONST_ITERATE(CBioseq_set_EditHandle::TDescr::Tdata, it, bsseh.SetDescr().Set()) {
                if ((*it)->IsTitle()) {
                    if (!NStr::Equal((*it)->GetTitle(), defline)) {
                        (*it)->SetTitle(defline);
                        any = true;
                    }
                    found_existing = true;
                    break;
                }
            }
            if (!found_existing) {
                CRef<CSeqdesc> new_desc(new CSeqdesc());
                new_desc->SetTitle(defline);
                bsseh.SetDescr().Set().push_back(new_desc);
                any = true;
            }
        }
    }
    return any;
}


bool CAutoDefWithTaxonomy::RegenerateDefLines(CSeq_entry_Handle se)
{
    bool any = RegenerateSequenceDefLines(se);

    any |= RegeneratePopsetTitles(se);
    return any;
}


string CAutoDefWithTaxonomy::GetDocsumOrgDescription(CSeq_entry_Handle se)
{
    string joined_org = "Mixed organisms";

    CRef<CT3Request> rq(new CT3Request());
    CBioseq_CI bi(se, CSeq_inst::eMol_na);
    while (bi) {
        CSeqdesc_CI desc_ci(*bi, CSeqdesc::e_Source);
        if (desc_ci && desc_ci->GetSource().IsSetOrg()) {
            TTaxId taxid = desc_ci->GetSource().GetOrg().GetTaxId();
            if (taxid > ZERO_TAX_ID) {
                rq->SetJoin().Set().push_back(TAX_ID_TO(int, taxid));
            }
        }
        ++bi;
    }
    if (rq->IsJoin() && rq->GetJoin().Get().size() > 0) {
        CTaxon3_request request;
        request.SetRequest().push_back(rq);
        CTaxon3 taxon3;
        taxon3.Init();
        CRef<CTaxon3_reply> reply = taxon3.SendRequest(request);
        if (reply) {
            CTaxon3_reply::TReply::const_iterator reply_it = reply->GetReply().begin();
            while (reply_it != reply->GetReply().end()) {
                if ((*reply_it)->IsData()
                    && (*reply_it)->GetData().GetOrg().IsSetTaxname()) {
                    joined_org = (*reply_it)->GetData().GetOrg().GetTaxname();
                    break;
                }
                ++reply_it;
            }
        }
    }

    return joined_org;
}


string CAutoDefWithTaxonomy::GetDocsumDefLine(CSeq_entry_Handle se)
{
    string org_desc = GetDocsumOrgDescription(se);

    string feature_clauses;
    CBioseq_CI bi(se, CSeq_inst::eMol_na);
    if (bi) {
        unsigned int genome_val = CBioSource::eGenome_unknown;
        CSeqdesc_CI di(*bi, CSeqdesc::e_Source);
        if (di && di->GetSource().IsSetGenome()) {
            genome_val = di->GetSource().GetGenome();
        }
        feature_clauses = GetOneFeatureClauseList(*bi, genome_val);
    }

    return org_desc + feature_clauses;
}


END_SCOPE(objects)
END_NCBI_SCOPE
