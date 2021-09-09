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
    rval |= RemoveMod(src, COrgMod::eSubtype_type_material);
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
    return RemoveMod(src, COrgMod::eSubtype_old_name);
}

bool RemoveMod( objects::CBioSource& src, objects::COrgMod::ESubtype subtype )
{
    bool erased = false;
    if (src.IsSetOrg() && src.GetOrg().IsSetOrgname()
        && src.GetOrg().GetOrgname().IsSetMod()) {
        COrgName::TMod::iterator it = src.SetOrg().SetOrgname().SetMod().begin();
        while (it != src.SetOrg().SetOrgname().SetMod().end()) {
            if ((*it)->GetSubtype() && (*it)->GetSubtype() == subtype) {
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


bool s_ProcessReply(const CT3Reply& reply, CRef<COrg_ref> org)
{
    if (reply.IsData()) {
        org->Assign(reply.GetData().GetOrg());
        return true;
    } else if (reply.IsError() && reply.GetError().IsSetMessage()) {
        ERR_POST(reply.GetError().GetMessage());
        return false;
    } else {
        ERR_POST("Taxonomy service failure");
        return false;
    }
}


void AddMissingCommonOrgMods(const COrg_ref& o1, const COrg_ref& o2, COrg_ref& common)
{
    if (!o1.IsSetOrgMod() || !o2.IsSetOrgMod()) {
        return;
    }
    ITERATE(COrgName::TMod, it1, o1.GetOrgname().GetMod()) {
        bool found_in_both = false;
        ITERATE(COrgName::TMod, it2, o2.GetOrgname().GetMod()) {
            if ((*it1)->Equals(**it2)) {
                found_in_both = true;
                break;
            }
        }
        if (found_in_both) {
            bool already_in_common = false;
            if (common.IsSetOrgMod()) {
                ITERATE(COrgName::TMod, it3, common.GetOrgname().GetMod()) {
                    if ((*it3)->Equals(**it1)) {
                        already_in_common = true;
                        break;
                    }
                }
            }
            if (!already_in_common) {
                CRef<COrgMod> add(new COrgMod());
                add->Assign(**it1);
                common.SetOrgname().SetMod().push_back(add);
            }
        }
    }
}


CRef<CBioSource> MakeCommonBioSource(const objects::CBioSource& src1, const objects::CBioSource& src2)
{
    CRef<CBioSource> common(NULL);

    if (!src1.IsSetOrg() || !src2.IsSetOrg()) {
        return common;
    }

    CTaxon3 taxon3(CTaxon3::initialize::yes);

    // do lookup before attempting to merge
    vector<CRef<COrg_ref> > rq_list;
    CRef<COrg_ref> o1(new COrg_ref());
    o1->Assign(src1.GetOrg());
    rq_list.push_back(o1);
    CRef<COrg_ref> o2(new COrg_ref());
    o2->Assign(src2.GetOrg());
    rq_list.push_back(o2);
    CRef<CTaxon3_reply> reply = taxon3.SendOrgRefList(rq_list);
    if (!reply || reply->GetReply().size() != 2) {
        ERR_POST("Taxonomy service failure");
        return CRef<CBioSource>(NULL);
    }
    if (!s_ProcessReply(*(reply->GetReply().front()), o1) ||
        !s_ProcessReply(*(reply->GetReply().back()), o2)) {
        return common;
    }

    TTaxId taxid1 = o1->GetTaxId();
    TTaxId taxid2 = o2->GetTaxId();
    if (taxid1 == ZERO_TAX_ID) {
        ERR_POST("No taxonomy ID for " + o1->GetTaxname());
        return common;
    } else if (taxid2 == ZERO_TAX_ID) {
        ERR_POST("No taxonomy ID for " + o2->GetTaxname());
        return common;
    } else if (taxid1 == taxid2) {
        CRef<CBioSource> tmp1(new CBioSource());
        tmp1->Assign(src1);
        tmp1->SetOrg().Assign(*o1);
        CRef<CBioSource> tmp2(new CBioSource());
        tmp2->Assign(src2);
        tmp2->SetOrg().Assign(*o2);
        common = tmp1->MakeCommon(*tmp2);
    } else {
        CRef<CT3Request> rq(new CT3Request());
        rq->SetJoin().Set().push_back(TAX_ID_TO(int, taxid1));
        rq->SetJoin().Set().push_back(TAX_ID_TO(int, taxid2));
        string err_nums = "(" + NStr::NumericToString(taxid1) + "," + NStr::NumericToString(taxid2) + ")";
        CTaxon3_request request;
        request.SetRequest().push_back(rq);
        CRef<CTaxon3_reply> reply = taxon3.SendRequest(request);
        if (!reply || reply->GetReply().size() != 1) {
            ERR_POST("Taxonomy service failure" + err_nums);
            return CRef<CBioSource>(NULL);
        }
        const CT3Reply& join_reply = *(reply->GetReply().front());
        if (join_reply.IsData()) {
            if (join_reply.GetData().IsSetOrg()) {
                if (join_reply.GetData().GetOrg().IsSetTaxname()) {
                    bool is_species_level = false, force_consult = false, has_nucleomorphs = false;
                    join_reply.GetData().GetTaxFlags(is_species_level, force_consult, has_nucleomorphs);
                    if (is_species_level) {
                        common = src1.MakeCommonExceptOrg(src2);
                        common->SetOrg().Assign(join_reply.GetData().GetOrg());
                    } else {
                        ERR_POST("Taxonomy join reply is not species level" + err_nums);
                    }
                } else {
                    ERR_POST("Taxonomy join reply Org-ref does not contain taxname" + err_nums);
                }
            } else {
                ERR_POST("Taxonomy join reply does not contain Org-ref" + err_nums);
            }
        }
    }
    return common;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

