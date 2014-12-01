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
* Author: Colleen Bollin, NCBI
*
* File Description:
*   functions for editing and working with biosources
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/edit/source_edit.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/taxon3/Taxon3_request.hpp>
#include <objects/taxon3/T3Request.hpp>
#include <objects/taxon3/SequenceOfInt.hpp>
#include <objects/taxon3/Taxon3_reply.hpp>
#include <objects/taxon3/T3Reply.hpp>
#include <objects/taxon3/taxon3.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


bool CleanupForTaxnameChange( objects::CBioSource& src )
{
    bool rval = RemoveOldName(src);
    rval |= RemoveTaxId(src);
    if (src.IsSetOrg() && src.GetOrg().IsSetCommon()) {
        src.SetOrg().ResetCommon();
        rval = true;
    }
    if (src.IsSetOrg() && src.GetOrg().IsSetSyn()) {
        src.SetOrg().ResetSyn();
        rval = true;
    }
    return rval;
}

bool RemoveOldName( objects::CBioSource& src )
{
    bool erased = false;
    if (src.IsSetOrg() && src.GetOrg().IsSetOrgname()
        && src.GetOrg().GetOrgname().IsSetMod()) {
        COrgName::TMod::iterator it = src.SetOrg().SetOrgname().SetMod().begin();
        while (it != src.SetOrg().SetOrgname().SetMod().end()) {
            if ((*it)->GetSubtype() && (*it)->GetSubtype() == COrgMod::eSubtype_old_name) {
                it = src.SetOrg().SetOrgname().SetMod().erase(it);
                erased = true;
            } else {
                it++;
            }
        }
        if (src.GetOrg().GetOrgname().GetMod().empty()) {
            src.SetOrg().SetOrgname().ResetMod();
        }
    }
    return erased;
}

bool RemoveTaxId( objects::CBioSource& src )
{
    bool erased = false;
    if (src.IsSetOrg() && src.GetOrg().IsSetDb()) {
        COrg_ref::TDb::iterator it = src.SetOrg().SetDb().begin();
        while (it != src.SetOrg().SetDb().end()) {
            if ((*it)->IsSetDb() && NStr::EqualNocase((*it)->GetDb(), "taxon")) {
                it = src.SetOrg().SetDb().erase(it);
                erased = true;
            } else {
                it++;
            }
        }
        if (src.GetOrg().GetDb().empty()) {
            src.SetOrg().ResetDb();
        }
    }
    return erased;
}


CRef<CBioSource> MakeCommonBioSource(const objects::CBioSource& src1, const objects::CBioSource& src2)
{ 
    CRef<CBioSource> common(NULL);

    if (!src1.IsSetOrg() || !src2.IsSetOrg()) {
        return common;
    }
    int taxid1 = src1.GetOrg().GetTaxId();
    int taxid2 = src2.GetOrg().GetTaxId();
    if (taxid1 == 0 || taxid2 == 0) {
        return common;
    } else if (taxid1 == taxid2) {
        common = src1.MakeCommon(src2);
    } else {
        CRef<CT3Request> rq(new CT3Request());
        rq->SetJoin().Set().push_back(taxid1);
        rq->SetJoin().Set().push_back(taxid2);
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
                    bool is_species_level = false, force_consult = false, has_nucleomorphs = false;
                    (*reply_it)->GetData().GetTaxFlags (is_species_level, force_consult, has_nucleomorphs);
                    if (is_species_level) {
                        common.Reset(new CBioSource());
                        common->SetOrg().Assign((*reply_it)->GetData().GetOrg());
                    }
                    break;
                }
                ++reply_it;
            }
        }
    }
    return common;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

