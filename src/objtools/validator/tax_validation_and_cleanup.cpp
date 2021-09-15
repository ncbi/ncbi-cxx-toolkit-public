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
 *   Tools for batch processing taxonomy-related validation and cleanup
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <objmgr/object_manager.hpp>

#include <serial/iterator.hpp>

#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/feature.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/scope.hpp>

#include <objects/taxon3/taxon3.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/validerror_imp.hpp>
#include <objtools/validator/tax_validation_and_cleanup.hpp>
#include <objtools/validator/utilities.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Validator

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;

const string kInvalidReplyMsg = "Taxonomy service returned invalid reply";


CQualifierRequest::CQualifierRequest()
{
    x_Init();
}


void CQualifierRequest::x_Init()
{
    m_ValuesToTry.clear();
    m_RepliesProcessed = 0;
    m_Descs.clear();
    m_Feats.clear();
}


void CQualifierRequest::AddParent(CConstRef<CSeqdesc> desc, CConstRef<CSeq_entry> ctx)
{
    m_Descs.push_back(TDescPair(desc, ctx));
}


void CQualifierRequest::AddParent(CConstRef<CSeq_feat> feat)
{
    m_Feats.push_back(feat);
}


void CQualifierRequest::AddRequests(vector<CRef<COrg_ref> >& request_list) const
{
    for (const string& it : m_ValuesToTry) {
        CRef<COrg_ref> rq(new COrg_ref);
        rq->SetTaxname(it);
        request_list.push_back(rq);
    }
}


bool CQualifierRequest::MatchTryValue(const string& val) const
{
    for (const string& it : m_ValuesToTry) {
        if (NStr::EqualNocase(val, it)) {
            return true;
        }
    }
    return false;
}


void CQualifierRequest::PostErrors(CValidError_imp& imp)
{
    vector<TTaxError> errs;
    ListErrors(errs);
    for (const auto& e : errs) {
        for (const auto& it : m_Descs) {
            imp.PostObjErr(e.severity, e.err_type, e.err_msg, *(it.first), it.second);
        }
        for (const auto& it : m_Feats) {
            imp.PostObjErr(e.severity, e.err_type, e.err_msg, *it);
        }
    }
}


CSpecificHostRequest::CSpecificHostRequest(const string& host, const COrg_ref& org, bool for_fix) :
    CQualifierRequest(),
    m_Host(host),
    m_Response(eUnrecognized),
    m_HostLineage(),
    m_OrgLineage()
{
    string host_check = SpecificHostValueToCheck(host);
    if (NStr::IsBlank(host_check)) {
        m_Response = eNormal;
        return;
    }
    if (!for_fix && !NStr::Equal(host, host_check)) {
        m_ValuesToTry.push_back(host_check);
    }
    m_ValuesToTry.push_back(host);

    m_SuggestedFix.clear();
    if (org.IsSetLineage()) {
        m_OrgLineage = org.GetLineage();
    }
}


void CSpecificHostRequest::AddReply(const CT3Reply& reply)
{
    if (m_Response == eAmbiguous) {
        string new_error = InterpretSpecificHostResult(m_ValuesToTry[m_RepliesProcessed], reply, m_Host);
        if (NStr::IsBlank(new_error)) {
            m_Response = eNormal;
            m_SuggestedFix = m_Host;
            m_HostLineage = reply.GetData().GetOrg().IsSetLineage() ? reply.GetData().GetOrg().GetLineage() : kEmptyStr;
            m_Error = kEmptyStr;
        }
    } else if (m_Response == eUnrecognized) {
        m_Error = InterpretSpecificHostResult(m_ValuesToTry[m_RepliesProcessed], reply, m_Host);
        if (NStr::IsBlank(m_Error)) {
            m_Response = eNormal;
            m_SuggestedFix = m_Host;
            m_HostLineage = reply.GetData().GetOrg().IsSetLineage() ? reply.GetData().GetOrg().GetLineage() : kEmptyStr;
        } else if (NStr::Find(m_Error, "ambiguous") != NPOS) {
            m_Response = eAmbiguous;
        } else if (NStr::StartsWith(m_Error, "Invalid value for specific host") && !IsLikelyTaxname(m_Host)) {
            m_Response = eNormal;
            m_SuggestedFix = m_Host;
        } else if (NStr::StartsWith(m_Error, "Specific host value is alternate name")) {
            m_Response = eAlternateName;
            m_SuggestedFix = reply.GetData().GetOrg().GetTaxname();
            m_HostLineage = reply.GetData().GetOrg().IsSetLineage() ? reply.GetData().GetOrg().GetLineage() : kEmptyStr;
        } else {
            m_Response = eUnrecognized;
            if (NStr::IsBlank(m_SuggestedFix) && reply.IsData() && reply.GetData().IsSetOrg()) {
                if (HasMisSpellFlag(reply.GetData())) {
                    m_SuggestedFix = reply.GetData().GetOrg().GetTaxname();
                    m_HostLineage = reply.GetData().GetOrg().IsSetLineage() ? reply.GetData().GetOrg().GetLineage() : kEmptyStr;
                } else if (!FindMatchInOrgRef(m_Host, reply.GetData().GetOrg())
                        && !IsCommonName(reply.GetData())) {
                    m_SuggestedFix = reply.GetData().GetOrg().GetTaxname();
                    m_HostLineage = reply.GetData().GetOrg().IsSetLineage() ? reply.GetData().GetOrg().GetLineage() : kEmptyStr;
                }
            }
        }
    }
    m_RepliesProcessed++;
}


void CSpecificHostRequest::ListErrors(vector<TTaxError>& errs) const
{
    switch (m_Response) {
        case eNormal:
            break;
        case eAmbiguous:
            errs.push_back(TTaxError{ eDiag_Info, eErr_SEQ_DESCR_AmbiguousSpecificHost, m_Error });
            break;
        case eUnrecognized:
            errs.push_back(TTaxError{ eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, m_Error });
            break;
        case eAlternateName:
            errs.push_back(TTaxError{ eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost, m_Error });
            break;
    }

    if (!NStr::IsBlank(m_HostLineage) && !NStr::IsBlank(m_OrgLineage) &&
        (NStr::Find(m_OrgLineage, "Streptophyta;") != NPOS || NStr::Find(m_OrgLineage, "Metazoa;") != NPOS) &&
        (NStr::Find(m_HostLineage, "Fungi;") != NPOS || NStr::Find(m_HostLineage, "Bacteria;") != NPOS ||
        NStr::Find(m_HostLineage, "Archaea;") != NPOS || NStr::Find(m_HostLineage, "Viruses;") != NPOS)) {
        errs.push_back(TTaxError{ eDiag_Warning, eErr_SEQ_DESCR_BadSpecificHost,
            "Suspect Host Value - a prokaryote, fungus or virus is suspect as a host for a plant or animal" });
    }
}


//LCOV_EXCL_START
//used by cleanup
const string& CSpecificHostRequest::SuggestFix() const
{
    if (m_ValuesToTry.empty()) {
        return m_Host;
    } else {
        return m_SuggestedFix;
    }
}
//LCOV_EXCL_STOP


bool CStrainRequest::x_IgnoreStrain(const string& str)
{
    // per VR-762, ignore strain if combination of letters and numbers
    bool has_number = false;
    bool has_letter = false;
    for (char ch : str) {
        if (isdigit(ch)) {
            has_number = true;
        } else if (isalpha(ch)) {
            has_letter = true;
        } else {
            return false;
        }
    }
    if (!has_number || !has_letter) {
        return false;
    } else {
        return true;
    }
}


CStrainRequest::CStrainRequest(const string& strain, const COrg_ref& org) : CQualifierRequest(), m_Strain(strain)
{
    if (org.IsSetTaxname()) {
        m_Taxname = org.GetTaxname();
    } else {
        m_Taxname.clear();
    }

    m_IsInvalid = false;
    if (NStr::IsBlank(strain) || x_IgnoreStrain(strain)) {
        return;
    }

    m_ValuesToTry.push_back(strain);
    size_t pos = 0;
    for (char ch : strain) {
        if (isalpha(ch)) {
            ++pos;
        } else {
            if (pos >= 5) {
                m_ValuesToTry.push_back(strain.substr(0, pos));
            }
            break;
        }
    }

    if (RequireTaxname(m_Taxname)) {
        m_ValuesToTry.push_back(MakeKey(strain, m_Taxname));
    }
}


string CStrainRequest::MakeKey(const string& strain, const string& taxname)
{
    if (RequireTaxname(taxname)) {
        return taxname.substr(0, taxname.length() - 3) + strain;
    } else {
        return strain;
    }
}


bool CStrainRequest::RequireTaxname(const string& taxname)
{
    if (NStr::EndsWith(taxname, " sp.")) {
        return true;
    } else {
        return false;
    }
}


bool CStrainRequest::x_IsUnwanted(const string& str)
{
    if (NStr::FindNoCase(str, "virus") != NPOS ||
        NStr::FindNoCase(str, "viroid") != NPOS ||
        NStr::FindNoCase(str, "vector") != NPOS ||
        NStr::FindNoCase(str, "phage") != NPOS) {
        return true;
    } else {
        return false;
    }
}


bool CStrainRequest::Check(const COrg_ref& org)
{
    if (org.IsSetLineage() && x_IsUnwanted(org.GetLineage())) {
        return false;
    }
    if (org.IsSetTaxname() && x_IsUnwanted(org.GetTaxname())) {
        return false;
    }
    if (!org.IsSetOrgMod()) {
        return false;
    }
    for (const auto& it : org.GetOrgname().GetMod()) {
        if (it->IsSetSubtype() && it->IsSetSubname() &&
            it->GetSubtype() == COrgMod::eSubtype_strain) {
            return true;
        }
    }
    return false;
}


void CStrainRequest::ListErrors(vector<TTaxError>& errs) const
{
    if (m_IsInvalid) {
        errs.push_back(TTaxError{ eDiag_Warning, eErr_SEQ_DESCR_StrainContainsTaxInfo,
            "Strain '" + m_Strain + "' contains taxonomic name information" });
    }
}


void CStrainRequest::AddReply(const CT3Reply& reply)
{
    if (!m_IsInvalid) {
        if (reply.IsData() && reply.GetData().IsSetOrg()) {
            // TODO: if using just a one word input, make sure name is actually in taxname
            if (m_ValuesToTry[m_RepliesProcessed].length() < m_Strain.length()) {
                if (NStr::EqualNocase(m_ValuesToTry[m_RepliesProcessed], reply.GetData().GetOrg().GetTaxname())) {
                    m_IsInvalid = true;
                }
            } else {
                m_IsInvalid = true;
            }
        }
    }
    m_RepliesProcessed++;
}


void CQualLookupMap::Clear()
{
    m_Populated = false;
    m_Map.clear();
}


void CQualLookupMap::AddDesc(CConstRef<CSeqdesc> desc, CConstRef<CSeq_entry> ctx)
{
    m_Populated = true;
    if (!desc->IsSource() || !desc->GetSource().IsSetOrg()) {
        return;
    }
    const COrg_ref& org = desc->GetSource().GetOrg();
    if (!org.IsSetOrgMod()) {
        return;
    }
    if (!Check(org)) {
        return;
    }
    for (const auto& mod_it : org.GetOrgname().GetMod()) {
        if (mod_it->IsSetSubtype()
            && mod_it->GetSubtype() == m_Subtype
            && mod_it->IsSetSubname()) {
            string qual = mod_it->GetSubname();
            string key = GetKey(qual, org);
            TQualifierRequests::iterator find = m_Map.find(key);
            if (find == m_Map.end()) {
                m_Map[key] = x_MakeNewRequest(qual, org);
                m_Map[key]->AddParent(desc, ctx);
            } else {
                find->second->AddParent(desc, ctx);
            }
        }
    }
}


void CQualLookupMap::AddFeat(CConstRef<CSeq_feat> feat)
{
    m_Populated = true;
    if (!feat->IsSetData() || !feat->GetData().IsBiosrc() ||
        !feat->GetData().GetBiosrc().IsSetOrg()) {
        return;
    }
    const COrg_ref& org = feat->GetData().GetBiosrc().GetOrg();
    if (!org.IsSetOrgMod()) {
        return;
    }
    if (!Check(org)) {
        return;
    }
    for (const auto& mod_it : org.GetOrgname().GetMod()) {
        if (mod_it->IsSetSubtype()
            && mod_it->GetSubtype() == m_Subtype
            && mod_it->IsSetSubname()) {
            string qual = mod_it->GetSubname();
            string key = GetKey(qual, feat->GetData().GetBiosrc().GetOrg());
            TQualifierRequests::iterator find = m_Map.find(key);
            if (find == m_Map.end()) {
                m_Map[key] = x_MakeNewRequest(qual, feat->GetData().GetBiosrc().GetOrg());
                m_Map[key]->AddParent(feat);
            } else {
                find->second->AddParent(feat);
            }
        }
    }
}


void CQualLookupMap::AddOrg(const COrg_ref& org)
{
    m_Populated = true;
    if (!org.IsSetOrgMod()) {
        return;
    }
    if (!Check(org)) {
        return;
    }
    for (const auto& mod_it : org.GetOrgname().GetMod()) {
        if (mod_it->IsSetSubtype()
            && mod_it->GetSubtype() == m_Subtype
            && mod_it->IsSetSubname()) {
            string qual = mod_it->GetSubname();
            string key = GetKey(qual, org);
            TQualifierRequests::iterator find = m_Map.find(key);
            if (find == m_Map.end()) {
                m_Map[key] = x_MakeNewRequest(qual, org);
            }
        }
    }
}


//LCOV_EXCL_START
//only used by biosample
void CQualLookupMap::AddString(const string& val)
{
    m_Populated = true;
    TQualifierRequests::iterator find = m_Map.find(val);
    if (find == m_Map.end()) {
        CRef<COrg_ref> org(new COrg_ref());
        m_Map[val] = x_MakeNewRequest(val, *org);
    }
}
//LCOV_EXCL_STOP


vector<CRef<COrg_ref> > CQualLookupMap::GetRequestList()
{
    vector<CRef<COrg_ref> > org_rq_list;
    org_rq_list.reserve(m_Map.size());
    for (auto& it : m_Map) {
        it.second->AddRequests(org_rq_list);
    }
    return org_rq_list;
}


CQualLookupMap::TQualifierRequests::iterator CQualLookupMap::x_FindRequest(const string& val)
{
    TQualifierRequests::iterator map_it = m_Map.find(val);
    if (map_it != m_Map.end() && map_it->second->NumRemainingReplies() > 0) {
        return map_it;
    }
    map_it = m_Map.begin();
    while (map_it != m_Map.end()) {
        if (map_it->second->MatchTryValue(val) && map_it->second->NumRemainingReplies() > 0) {
            return map_it;
        }
        ++map_it;
    }
    return m_Map.end();
}


string CQualLookupMap::IncrementalUpdate(const vector<CRef<COrg_ref> >& input, const CTaxon3_reply& reply)
{
    string error_message;
    CTaxon3_reply::TReply::const_iterator reply_it = reply.GetReply().begin();
    vector<CRef<COrg_ref> >::const_iterator rq_it = input.begin();

    while (reply_it != reply.GetReply().end() && rq_it != input.end()) {
        TQualifierRequests::iterator map_it = x_FindRequest((*rq_it)->GetTaxname());
        if (map_it == m_Map.end()) {
            error_message = "Unexpected taxonomy response for " + (*rq_it)->GetTaxname();
            return error_message;
        }
        map_it->second->AddReply(**reply_it);
        ++rq_it;
        ++reply_it;
    }

    if (reply_it != reply.GetReply().end()) {
        error_message = "Unexpected taxonomy responses for " + COrgMod::GetSubtypeName(m_Subtype);
    }
    return kEmptyStr;
}


//LCOV_EXCL_START
//only used for cleanup
bool CQualLookupMap::IsUpdateComplete() const
{
    for (const auto& rq_it : m_Map) {
        if (rq_it.second->NumRemainingReplies() > 0) {
            return false;
        }
    }
    return true;
}
//LCOV_EXCL_STOP


void CQualLookupMap::PostErrors(CValidError_imp& imp)
{
    for (auto& rq_it : m_Map) {
        rq_it.second->PostErrors(imp);
    }
}


//LCOV_EXCL_START
//only used by biosample
void CQualLookupMap::ListErrors(vector<TTaxError>& errs) const
{
    for (const auto& rq_it : m_Map) {
        rq_it.second->ListErrors(errs);
    }
}


//LCOV_EXCL_STOP


CRef<CQualifierRequest> CSpecificHostMap::x_MakeNewRequest(const string& orig_val, const COrg_ref& org)
{
    CRef<CQualifierRequest> rq(new CSpecificHostRequest(orig_val, org));
    return rq;
}


//LCOV_EXCL_START
//used for cleanup
CRef<CQualifierRequest> CSpecificHostMapForFix::x_MakeNewRequest(const string& orig_val, const COrg_ref& org)
{
    CRef<CQualifierRequest> rq(new CSpecificHostRequest(orig_val, org, true));
    return rq;
}


string CSpecificHostMapForFix::x_DefaultSpecificHostAdjustments(const string& host_val)
{
    string adjusted = host_val;
    NStr::TruncateSpacesInPlace(adjusted);
    adjusted = COrgMod::FixHost(adjusted);
    return adjusted;
}


bool CSpecificHostMapForFix::ApplyToOrg(COrg_ref& org_ref) const
{
    if (!org_ref.IsSetOrgname() ||
        !org_ref.GetOrgname().IsSetMod()) {
        return false;
    }

    bool changed = false;

    for (auto& m : org_ref.SetOrgname().SetMod()) {
        if (m->IsSetSubtype() &&
            m->GetSubtype() == COrgMod::eSubtype_nat_host &&
            m->IsSetSubname()) {
            string host_val = x_DefaultSpecificHostAdjustments(m->GetSubname());
            TQualifierRequests::const_iterator it = m_Map.find(host_val);
            if (it != m_Map.end()) {
                const CSpecificHostRequest* rq = dynamic_cast<const CSpecificHostRequest*>(it->second.GetPointer());
                string new_val = x_DefaultSpecificHostAdjustments(rq->SuggestFix());
                if (!NStr::IsBlank(new_val) && !NStr::Equal(new_val, m->GetSubname())) {
                    m->SetSubname(new_val);
                    changed = true;
                }
            }
        }
    }

    return changed;
}
//LCOV_EXCL_STOP


CRef<CQualifierRequest> CStrainMap::x_MakeNewRequest(const string& orig_val, const COrg_ref& org)
{
    CRef<CQualifierRequest> rq(new CStrainRequest(orig_val, org));
    return rq;
}


CTaxValidationAndCleanup::CTaxValidationAndCleanup()
{
    m_taxon3.reset(new CTaxon3(CTaxon3::initialize::yes));
    m_tax_func = [this](const vector<CRef<COrg_ref>>& list) -> CRef<CTaxon3_reply> {
        return m_taxon3->SendOrgRefList(list);
    };
}

CTaxValidationAndCleanup::CTaxValidationAndCleanup(taxupdate_func_t tax_func)
  : m_tax_func(tax_func)
{
}

void CTaxValidationAndCleanup::Init(const CSeq_entry& se)
{
    m_SrcDescs.clear();
    m_DescCtxs.clear();
    m_SrcFeats.clear();
    m_SpecificHostRequests.clear();
    m_SpecificHostRequestsBuilt = false;
    m_SpecificHostRequestsUpdated = false;
    m_StrainRequestsBuilt = false;
    x_GatherSources(se);
}


CConstRef<CSeq_entry> CTaxValidationAndCleanup::GetTopReportObject() const
{
    if (!m_DescCtxs.empty()) {
        return m_DescCtxs.front();
    } else {
        return CConstRef<CSeq_entry>();
    }
}


void CTaxValidationAndCleanup::x_GatherSources(const CSeq_entry& se)
{
    // get source descriptors
    FOR_EACH_DESCRIPTOR_ON_SEQENTRY(it, se)
    {
        if ((*it)->IsSource() && (*it)->GetSource().IsSetOrg()) {
            CConstRef<CSeqdesc> desc;
            desc.Reset(*it);
            m_SrcDescs.push_back(desc);
            CConstRef<CSeq_entry> r_se;
            r_se.Reset(&se);
            m_DescCtxs.push_back(r_se);
        }
    }
    // also get features
    FOR_EACH_ANNOT_ON_SEQENTRY(annot_it, se)
    {
        FOR_EACH_SEQFEAT_ON_SEQANNOT(feat_it, **annot_it)
        {
            if ((*feat_it)->IsSetData() && (*feat_it)->GetData().IsBiosrc()
                && (*feat_it)->GetData().GetBiosrc().IsSetOrg()) {
                CConstRef<CSeq_feat> feat;
                feat.Reset(*feat_it);
                m_SrcFeats.push_back(feat);
            }
        }
    }

    // if set, recurse
    if (se.IsSet()) {
        FOR_EACH_SEQENTRY_ON_SEQSET(it, se.GetSet())
        {
            x_GatherSources(**it);
        }
    }
}


vector< CRef<COrg_ref> > CTaxValidationAndCleanup::GetTaxonomyLookupRequest() const
{
    // request list for taxon3
    vector< CRef<COrg_ref> > org_rq_list;

    // first do descriptors
    vector<CConstRef<CSeqdesc> >::const_iterator desc_it = m_SrcDescs.cbegin();
    vector<CConstRef<CSeq_entry> >::const_iterator ctx_it = m_DescCtxs.cbegin();
    while (desc_it != m_SrcDescs.cend() && ctx_it != m_DescCtxs.cend()) {
        CRef<COrg_ref> rq(new COrg_ref);
        const COrg_ref& org = (*desc_it)->GetSource().GetOrg();
        rq->Assign(org);
        org_rq_list.push_back(rq);

        ++desc_it;
        ++ctx_it;
    }

    // now do features
    vector<CConstRef<CSeq_feat> >::const_iterator feat_it = m_SrcFeats.cbegin();
    while (feat_it != m_SrcFeats.cend()) {
        CRef<COrg_ref> rq(new COrg_ref);
        const COrg_ref& org = (*feat_it)->GetData().GetBiosrc().GetOrg();
        rq->Assign(org);
        org_rq_list.push_back(rq);

        ++feat_it;
    }
    return org_rq_list;
}


void CTaxValidationAndCleanup::x_InterpretTaxonomyError(const CT3Error& error, const COrg_ref& org, const EErrType type, vector<TTaxError>& errs) const
{
    const string err_str = error.IsSetMessage() ? error.GetMessage() : "?";

    if (NStr::Equal(err_str, "Organism not found")) {
        string msg = "Organism not found in taxonomy database";
        if (error.IsSetOrg() && error.GetOrg().IsSetTaxname() &&
            !NStr::Equal(error.GetOrg().GetTaxname(), "Not valid") &&
            (!org.IsSetTaxname() ||
                !NStr::Equal(org.GetTaxname(), error.GetOrg().GetTaxname()))) {
            msg += " (suggested:" + error.GetOrg().GetTaxname() + ")";
        }
        errs.push_back(TTaxError{ eDiag_Warning, eErr_SEQ_DESCR_OrganismNotFound, msg });
    } else if (NStr::StartsWith(err_str, "Organism not found. Possible matches")) {
        errs.push_back(TTaxError{ eDiag_Warning, eErr_SEQ_DESCR_OrganismNotFound, err_str });
    } else if (NStr::Equal(err_str, kInvalidReplyMsg)) {
        errs.push_back(TTaxError{ eDiag_Error, eErr_SEQ_DESCR_TaxonomyLookupProblem, err_str });
    } else if (NStr::Find(err_str, "ambiguous name") != NPOS) {
        errs.push_back(TTaxError{ eDiag_Warning, eErr_SEQ_DESCR_TaxonomyAmbiguousName,
            "Taxonomy lookup failed with message '" + err_str + "'"});
    } else {
        errs.push_back(TTaxError{ eDiag_Warning, type,
            "Taxonomy lookup failed with message '" + err_str + "'" });
    }
}


void CTaxValidationAndCleanup::ListTaxLookupErrors
(const CT3Reply& reply, const COrg_ref& org, CBioSource::TGenome genome, bool is_insd_patent, bool is_wp, vector<TTaxError>& errs) const
{
    if (reply.IsError()) {
        x_InterpretTaxonomyError(reply.GetError(), org, eErr_SEQ_DESCR_TaxonomyLookupProblem, errs);
    } else if (reply.IsData()) {
        bool is_species_level = true;
        bool is_unidentified = false;
        bool force_consult = false;
        bool has_nucleomorphs = false;
        if (reply.GetData().IsSetOrg()) {
            const COrg_ref& orp_rep = reply.GetData().GetOrg();
            if (org.IsSetTaxname() && orp_rep.IsSetTaxname()) {
                const string& taxname_req = org.GetTaxname();
                const string& taxname_rep = orp_rep.GetTaxname();
                if (NStr::Equal(taxname_rep, "unidentified")) {
                    is_unidentified = true;
                }
                TTaxId taxid_request = org.GetTaxId();
                TTaxId taxid_reply = orp_rep.GetTaxId();

                if (taxid_request != ZERO_TAX_ID && taxid_reply != ZERO_TAX_ID && taxid_request != taxid_reply) {
                    errs.push_back(TTaxError{ eDiag_Error, eErr_SEQ_DESCR_TaxonomyLookupProblem,
                        "Organism name is '" + taxname_req
                        + "', taxonomy ID should be '" + NStr::NumericToString(taxid_reply)
                        + "' but is '" + NStr::NumericToString(taxid_request) + "'" });
                }
            }
        }
        reply.GetData().GetTaxFlags(is_species_level, force_consult, has_nucleomorphs);
        if (!is_species_level && !is_wp) {
            errs.push_back(TTaxError{ eDiag_Warning, eErr_SEQ_DESCR_TaxonomyIsSpeciesProblem,
                "Taxonomy lookup reports is_species_level FALSE"});
        }
        if (force_consult) {
            if (is_insd_patent && is_unidentified) {
                force_consult = false;
            }
        }
        if (force_consult) {
            errs.push_back(TTaxError{eDiag_Warning, eErr_SEQ_DESCR_TaxonomyConsultRequired,
                "Taxonomy lookup reports taxonomy consultation needed"});
        }
        if (genome == CBioSource::eGenome_nucleomorph
            && !has_nucleomorphs) {
            errs.push_back(TTaxError{eDiag_Warning, eErr_SEQ_DESCR_TaxonomyNucleomorphProblem,
                    "Taxonomy lookup does not have expected nucleomorph flag"});
        } else if (genome == CBioSource::eGenome_plastid
            && (!reply.GetData().HasPlastids())) {
            errs.push_back(TTaxError{eDiag_Warning, eErr_SEQ_DESCR_TaxonomyPlastidsProblem,
                    "Taxonomy lookup does not have expected plastid flag"});
        }
    }

}

void CTaxValidationAndCleanup::ReportTaxLookupErrors
(const CTaxon3_reply& reply,
 CValidError_imp& imp,
 bool is_insd_patent) const
{
    CTaxon3_reply::TReply::const_iterator reply_it = reply.GetReply().begin();

    // process descriptor responses
    vector<CConstRef<CSeqdesc> >::const_iterator desc_it = m_SrcDescs.cbegin();
    vector<CConstRef<CSeq_entry> >::const_iterator ctx_it = m_DescCtxs.cbegin();

    while (reply_it != reply.GetReply().end()
        && desc_it != m_SrcDescs.cend()
        && ctx_it != m_DescCtxs.cend()) {
        vector<TTaxError> errs;
        const COrg_ref& orp_req = (*desc_it)->GetSource().GetOrg();
        ListTaxLookupErrors(**reply_it, orp_req,
            (*desc_it)->GetSource().IsSetGenome() ? (*desc_it)->GetSource().GetGenome() : CBioSource::eGenome_unknown,
            is_insd_patent, imp.IsWP(), errs);
        for (const TTaxError& e : errs) {
            imp.PostObjErr(e.severity, e.err_type, e.err_msg, **desc_it, *ctx_it);
        }
        ++reply_it;
        ++desc_it;
        ++ctx_it;
    }
    // process feat responses
    vector<CConstRef<CSeq_feat> >::const_iterator feat_it = m_SrcFeats.cbegin();
    while (reply_it != reply.GetReply().cend()
        && feat_it != m_SrcFeats.end()) {
        vector<TTaxError> errs;
        const COrg_ref& orp_req = (*feat_it)->GetData().GetBiosrc().GetOrg();
        ListTaxLookupErrors(**reply_it, orp_req,
            (*feat_it)->GetData().GetBiosrc().IsSetGenome() ? (*feat_it)->GetData().GetBiosrc().GetGenome() : CBioSource::eGenome_unknown,
            is_insd_patent, imp.IsWP(), errs);
        for (const TTaxError& e : errs) {
            imp.PostErr(e.severity, e.err_type, e.err_msg,* *feat_it);
        }
        ++reply_it;
        ++feat_it;
    }

}


void CTaxValidationAndCleanup::ReportIncrementalTaxLookupErrors
(const CTaxon3_reply& reply,
    CValidError_imp& imp,
    bool is_insd_patent,
    size_t offset) const
{
    // cout << MSerial_AsnText << reply << endl;

    CTaxon3_reply::TReply::const_iterator reply_it = reply.GetReply().begin();

    // process descriptor responses
    vector<CConstRef<CSeqdesc> >::const_iterator desc_it = m_SrcDescs.cbegin();
    vector<CConstRef<CSeq_entry> >::const_iterator ctx_it = m_DescCtxs.cbegin();

    size_t skipped = 0;
    while (skipped < offset
        && desc_it != m_SrcDescs.cend()
        && ctx_it != m_DescCtxs.cend()) {
        ++desc_it;
        ++ctx_it;
        skipped++;
    }

    while (reply_it != reply.GetReply().end()
        && desc_it != m_SrcDescs.cend()
        && ctx_it != m_DescCtxs.cend()) {
        vector<TTaxError> errs;
        const COrg_ref& orp_req = (*desc_it)->GetSource().GetOrg();
        ListTaxLookupErrors(**reply_it, orp_req,
            (*desc_it)->GetSource().IsSetGenome() ? (*desc_it)->GetSource().GetGenome() : CBioSource::eGenome_unknown,
            is_insd_patent, imp.IsWP(), errs);
        for (const TTaxError& e : errs) {
            imp.PostObjErr(e.severity, e.err_type, e.err_msg, **desc_it, *ctx_it);
        }
        ++reply_it;
        ++desc_it;
        ++ctx_it;
    }

    if (reply_it == reply.GetReply().end()) {
        return;
    }
    // process feat responses
    vector<CConstRef<CSeq_feat> >::const_iterator feat_it = m_SrcFeats.cbegin();
    while (skipped < offset && feat_it != m_SrcFeats.end()) {
        ++feat_it;
        skipped++;
    }
    while (reply_it != reply.GetReply().cend() &&
        feat_it != m_SrcFeats.end()) {
        vector<TTaxError> errs;
        const COrg_ref& orp_req = (*feat_it)->GetData().GetBiosrc().GetOrg();
        ListTaxLookupErrors(**reply_it, orp_req,
            (*feat_it)->GetData().GetBiosrc().IsSetGenome() ? (*feat_it)->GetData().GetBiosrc().GetGenome() : CBioSource::eGenome_unknown,
            is_insd_patent, imp.IsWP(), errs);
        for (const TTaxError& e : errs) {
            imp.PostErr(e.severity, e.err_type, e.err_msg, **feat_it);
        }
        ++reply_it;
        ++feat_it;
    }


}



//LCOV_EXCL_START
//used by Genome Workbench
bool CTaxValidationAndCleanup::AdjustOrgRefsWithTaxLookupReply
( const CTaxon3_reply& reply,
 vector<CRef<COrg_ref> > org_refs,
 string& error_message,
 bool use_error_orgrefs) const
{
    bool changed = false;
    CTaxon3_reply::TReply::const_iterator reply_it = reply.GetReply().begin();
    vector<CRef<COrg_ref> >::iterator org_it = org_refs.begin();
    while (reply_it != reply.GetReply().end() && org_it != org_refs.end()) {
        CRef<COrg_ref> cpy;
        if ((*reply_it)->IsData() &&
            (*reply_it)->GetData().IsSetOrg()) {
            cpy.Reset(new COrg_ref());
            cpy->Assign((*reply_it)->GetData().GetOrg());
        } else if (use_error_orgrefs &&
            (*reply_it)->IsError() &&
            (*reply_it)->GetError().IsSetOrg() &&
            (*reply_it)->GetError().GetOrg().IsSetTaxname() &&
            !NStr::Equal((*reply_it)->GetError().GetOrg().GetTaxname(), "Not valid")) {
            cpy.Reset(new COrg_ref());
            cpy->Assign((*reply_it)->GetError().GetOrg());
        }
        if (cpy) {
            cpy->CleanForGenBank();
            if (!cpy->Equals(**org_it)) {
                (*org_it)->Assign(*cpy);
                changed = true;
            }
        }
        ++reply_it;
        ++org_it;
    }
    if (reply_it != reply.GetReply().end()) {
        error_message = "More taxonomy replies than requests!";
    } else if (org_it != org_refs.end()) {
        error_message = "Not enough taxonomy replies!";
    }
    return changed;
}
//LCOV_EXCL_STOP


vector<CRef<COrg_ref> > CTaxValidationAndCleanup::GetSpecificHostLookupRequest(bool for_fix)
{
    if (for_fix) {
        if (!m_HostMapForFix.IsPopulated()) {
            x_CreateQualifierMap(m_HostMapForFix);
        }
        return m_HostMapForFix.GetRequestList();
    } else {
        if (!m_HostMap.IsPopulated()) {
            x_CreateQualifierMap(m_HostMap);
        }
        return m_HostMap.GetRequestList();
    }
}

vector<CRef<COrg_ref> > CTaxValidationAndCleanup::GetStrainLookupRequest()
{
    if (!m_StrainRequestsBuilt) {
        x_CreateStrainMap();
    }

    vector<CRef<COrg_ref> > org_rq_list = m_StrainMap.GetRequestList();
    return org_rq_list;
}


void CTaxValidationAndCleanup::x_CreateQualifierMap(CQualLookupMap& lookup)
{
    //first do descriptors
    vector<CConstRef<CSeqdesc> >::const_iterator desc_it = m_SrcDescs.begin();
    vector<CConstRef<CSeq_entry> >::const_iterator ctx_it = m_DescCtxs.begin();
    while (desc_it != m_SrcDescs.end() && ctx_it != m_DescCtxs.end()) {
        lookup.AddDesc(*desc_it, *ctx_it);
        ++desc_it;
        ++ctx_it;
    }
    // collect features with specific hosts
    vector<CConstRef<CSeq_feat> >::const_iterator feat_it = m_SrcFeats.begin();
    while (feat_it != m_SrcFeats.end()) {
        lookup.AddFeat(*feat_it);
        ++feat_it;
    }

}


void CTaxValidationAndCleanup::x_CreateStrainMap()
{
    x_CreateQualifierMap(m_StrainMap);
    m_StrainRequestsBuilt = true;
}


void CTaxValidationAndCleanup::ReportSpecificHostErrors(CValidError_imp& imp)
{
    m_HostMap.PostErrors(imp);
}

//LCOV_EXCL_START
//appears to not be used
void CTaxValidationAndCleanup::ReportSpecificHostErrors(const CTaxon3_reply& reply, CValidError_imp& imp)
{
    string error_message;
    if (!m_HostMap.IsUpdateComplete()) {
        vector<CRef<COrg_ref> > input = m_HostMap.GetRequestList();
        error_message = m_HostMap.IncrementalUpdate(input, reply);
    }
    if (!NStr::IsBlank(error_message)) {
        imp.PostErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyLookupProblem, error_message, *(GetTopReportObject()));
        return;
    }

    m_HostMap.PostErrors(imp);
}
//LCOV_EXCL_STOP


//LCOV_EXCL_START
//only used by cleanup
bool CTaxValidationAndCleanup::AdjustOrgRefsWithSpecificHostReply
(vector<CRef<COrg_ref> > requests,
 const CTaxon3_reply& reply,
 vector<CRef<COrg_ref> > org_refs,
 string& error_message)
{
    if (!m_HostMapForFix.IsUpdateComplete()) {
        // need to calculate requests for this list
        m_HostMapForFix.IncrementalUpdate(requests, reply);
    }
    return AdjustOrgRefsForSpecificHosts(org_refs);
}


bool CTaxValidationAndCleanup::AdjustOrgRefsForSpecificHosts(vector<CRef<COrg_ref> > org_refs)
{
    bool changed = false;
    for (auto org = org_refs.begin(); org != org_refs.end(); org++) {
        changed |= m_HostMapForFix.ApplyToOrg(**org);
    }
    return changed;
}


TSpecificHostRequests::iterator CTaxValidationAndCleanup::x_FindHostFixRequest(const string& val)
{
    TSpecificHostRequests::iterator map_it = m_SpecificHostRequests.find(val);
    if (map_it != m_SpecificHostRequests.end() && map_it->second.NumRemainingReplies() > 0) {
        return map_it;
    }
    map_it = m_SpecificHostRequests.begin();
    while (map_it != m_SpecificHostRequests.end()) {
        if (map_it->second.MatchTryValue(val) && map_it->second.NumRemainingReplies() > 0) {
            return map_it;
        }
        ++map_it;
    }
    return m_SpecificHostRequests.end();
}
//LCOV_EXCL_STOP


string CTaxValidationAndCleanup::IncrementalSpecificHostMapUpdate(const vector<CRef<COrg_ref> >& input, const CTaxon3_reply& reply)
{
    string error_message;
    if (m_HostMap.IsPopulated()) {
        error_message = m_HostMap.IncrementalUpdate(input, reply);
    }
    if (NStr::IsBlank(error_message)) {
        if (m_HostMapForFix.IsPopulated()) {
            error_message = m_HostMapForFix.IncrementalUpdate(input, reply);
        }
    }
    return error_message;
}


//LCOV_EXCL_START
//used only by cleanup
bool CTaxValidationAndCleanup::IsSpecificHostMapUpdateComplete() const
{
    if (m_HostMap.IsPopulated()) {
        return m_HostMap.IsUpdateComplete();
    } else if (m_HostMapForFix.IsPopulated()) {
        return m_HostMapForFix.IsUpdateComplete();
    } else {
        return false;
    }
}


void CTaxValidationAndCleanup::x_UpdateSpecificHostMapWithReply(const CTaxon3_reply& reply, string& error_message)
{
    CTaxon3_reply::TReply::const_iterator reply_it = reply.GetReply().begin();
    TSpecificHostRequests::iterator rq_it = m_SpecificHostRequests.begin();
    while (rq_it != m_SpecificHostRequests.end()) {
        while (rq_it->second.NumRemainingReplies() > 0 && reply_it != reply.GetReply().end()) {
            rq_it->second.AddReply(**reply_it);
            ++reply_it;
        }
        if (rq_it->second.NumRemainingReplies() > 0) {
            error_message = "Failed to respond to all taxonomy requests for specific host";
            break;
        }
        ++rq_it;
    }

    if (reply_it != reply.GetReply().end()) {
        error_message = "Unexpected taxonomy responses for specific host";
    }
}


bool CTaxValidationAndCleanup::x_ApplySpecificHostMap(COrg_ref& org_ref) const
{
    if (!org_ref.IsSetOrgname() ||
        !org_ref.GetOrgname().IsSetMod()) {
        return false;
    }

    bool changed = false;

    for (auto& m : org_ref.SetOrgname().SetMod()) {
        if (m->IsSetSubtype() &&
            m->GetSubtype() == COrgMod::eSubtype_nat_host &&
            m->IsSetSubname()) {
            string host_val = x_DefaultSpecificHostAdjustments(m->GetSubname());
            TSpecificHostRequests::const_iterator it = m_SpecificHostRequests.find(host_val);
            if (it != m_SpecificHostRequests.end()) {
                const string& new_val = it->second.SuggestFix();
                if (!NStr::IsBlank(new_val) && !NStr::Equal(new_val, m->GetSubname())) {
                    m->SetSubname(new_val);
                    changed = true;
                }
            }
        }
    }

    return changed;
}


string CTaxValidationAndCleanup::x_DefaultSpecificHostAdjustments(const string& host_val)
{
    string adjusted = host_val;
    NStr::TruncateSpacesInPlace(adjusted);
    adjusted = COrgMod::FixHost(adjusted);
    return adjusted;
}


string CTaxValidationAndCleanup::IncrementalStrainMapUpdate(const vector<CRef<COrg_ref> >& input, const CTaxon3_reply& reply)
{
    return m_StrainMap.IncrementalUpdate(input, reply);
}


bool CTaxValidationAndCleanup::IsStrainMapUpdateComplete() const
{
    return m_StrainMap.IsUpdateComplete();
}
//LCOV_EXCL_STOP


void CTaxValidationAndCleanup::ReportStrainErrors(CValidError_imp& imp)
{
    m_StrainMap.PostErrors(imp);
}

CConstRef<CSeq_entry> CTaxValidationAndCleanup::GetSeqContext(size_t num) const
{
    return (num < m_DescCtxs.size()) ? m_DescCtxs[num] : CConstRef<CSeq_entry>();
}


//LCOV_EXCL_START
//used by Genome Workbench, asn_cleanup, and table2asn but not asnvalidate
bool CTaxValidationAndCleanup::DoTaxonomyUpdate(CSeq_entry_Handle seh, bool with_host)
{
    Init(*(seh.GetCompleteSeq_entry()));

    vector<CRef<COrg_ref> > original_orgs = GetTaxonomyLookupRequest();
    if (original_orgs.empty())
    {
        return false;
    }
    const size_t chunk_size = 1000;
    vector< CRef<COrg_ref> > edited_orgs;

    size_t i = 0;
    while (i < original_orgs.size())
    {
        size_t len = min(chunk_size, original_orgs.size() - i);
        vector< CRef<COrg_ref> >  tmp_original_orgs(original_orgs.begin() + i, original_orgs.begin() + i + len);
        vector< CRef<COrg_ref> >  tmp_edited_orgs;
        for (CRef<COrg_ref>& it : tmp_original_orgs)
        {
            CRef<COrg_ref> cpy(new COrg_ref());
            cpy->Assign(*it);
            tmp_edited_orgs.push_back(cpy);
        }
        CRef<CTaxon3_reply> tmp_lookup_reply = m_tax_func(tmp_original_orgs);
        string error_message;
        AdjustOrgRefsWithTaxLookupReply(*tmp_lookup_reply, tmp_edited_orgs, error_message);
        if (!NStr::IsBlank(error_message))
        {
            // post error message
            ERR_POST(Error << error_message);
            return false;
        }
        edited_orgs.insert(edited_orgs.end(), tmp_edited_orgs.begin(), tmp_edited_orgs.end());
        i += len;
    }

    if (with_host) {
        vector< CRef<COrg_ref> > spec_host_rq = GetSpecificHostLookupRequest(true);
        i = 0;
        while (i < spec_host_rq.size())
        {
            size_t len = min(chunk_size, spec_host_rq.size() - i);
            vector< CRef<COrg_ref> > tmp_spec_host_rq(spec_host_rq.begin() + i, spec_host_rq.begin() + i + len);
            CRef<CTaxon3_reply> tmp_spec_host_reply = m_tax_func(tmp_spec_host_rq);
            string error_message = IncrementalSpecificHostMapUpdate(tmp_spec_host_rq, *tmp_spec_host_reply);
            if (!NStr::IsBlank(error_message))
            {
                // post error message
                ERR_POST(Error << error_message);
                return false;
            }
            i += len;
        }

        AdjustOrgRefsForSpecificHosts(edited_orgs);
    }

    // update descriptors
    size_t num_descs = NumDescs();
    size_t num_updated_descs = 0;
    for (size_t n = 0; n < num_descs; n++) {
        if (!original_orgs[n]->Equals(*(edited_orgs[n]))) {
            CSeqdesc* orig = const_cast<CSeqdesc *>(GetDesc(n).GetPointer());
            orig->SetSource().SetOrg().Assign(*(edited_orgs[n]));
            num_updated_descs++;
        }
    }

    // now update features
    size_t num_updated_feats = 0;
    for (size_t n = 0; n < NumFeats(); n++) {
        if (!original_orgs[n + num_descs]->Equals(*edited_orgs[n + num_descs])) {
            CConstRef<CSeq_feat> feat = GetFeat(n);
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*feat);
            new_feat->SetData().SetBiosrc().SetOrg().Assign(*(edited_orgs[n + num_descs]));

            CSeq_feat_Handle fh = seh.GetScope().GetSeq_featHandle(*feat);
            CSeq_feat_EditHandle efh(fh);
            efh.Replace(*new_feat);
            num_updated_feats++;
        }
    }
    return (num_updated_descs > 0 || num_updated_feats > 0);
}
//LCOV_EXCL_STOP


//LCOV_EXCL_START
//only used by biosample
void CTaxValidationAndCleanup::FixOneSpecificHost(string& val)
{
    val = x_DefaultSpecificHostAdjustments(val);
    string err_msg;
    if(IsOneSpecificHostValid(val, err_msg)) {
        return;
    }
    m_HostMapForFix.Clear();
    m_HostMapForFix.AddString(val);

    vector< CRef<COrg_ref> > spec_host_rq = m_HostMapForFix.GetRequestList();
    if (spec_host_rq.empty()) {
        m_HostMapForFix.Clear();
        return;
    }
    vector< CRef<COrg_ref> > edited;
    edited.push_back(CRef<COrg_ref>(new COrg_ref()));
    edited.front()->SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_nat_host, val)));

    CRef<CTaxon3_reply> tmp_spec_host_reply = m_tax_func(spec_host_rq);

    if (!tmp_spec_host_reply->IsSetReply() || !tmp_spec_host_reply->GetReply().front()->IsData()) {
        val = kEmptyStr;
        m_HostMapForFix.Clear();
        return;
    }

    string error_message = IncrementalSpecificHostMapUpdate(spec_host_rq, *tmp_spec_host_reply);
    if (!NStr::IsBlank(error_message))
    {
        // post error message
        ERR_POST(Error << error_message);
    }


    AdjustOrgRefsForSpecificHosts(edited);

    val = edited.front()->GetOrgname().GetMod().front()->GetSubname();
    m_HostMapForFix.Clear();
}
//LCOV_EXCL_STOP


//LCOV_EXCL_START
//only used by biosample
bool CTaxValidationAndCleanup::IsOneSpecificHostValid(const string& val, string& error_msg)
{
    error_msg = kEmptyStr;
    m_HostMap.Clear();

    m_HostMap.AddString(val);

    vector< CRef<COrg_ref> > spec_host_rq = m_HostMap.GetRequestList();
    if (spec_host_rq.empty()) {
        m_HostMap.Clear();
        return true;
    }

    CRef<CTaxon3_reply> tmp_spec_host_reply = m_tax_func(spec_host_rq);

    string err_msg;
    if (tmp_spec_host_reply) {
        err_msg = IncrementalSpecificHostMapUpdate(spec_host_rq, *tmp_spec_host_reply);
    } else {
        err_msg = "Connection to taxonomy failed";
    }
    bool rval = true;
    error_msg = err_msg;

    if (!NStr::IsBlank(err_msg)) {
        ERR_POST(Error << err_msg);
        m_HostMap.Clear();
        rval = false;
    } else {
        vector<TTaxError> errs;
        m_HostMap.ListErrors(errs);
        if (errs.size() > 0) {
            error_msg = errs.front().err_msg;
            rval = false;
        }
    }
    m_HostMap.Clear();
    return rval;
}
//LCOV_EXCL_STOP


void CTaxValidationAndCleanup::CheckOneOrg(const COrg_ref& org, int genome, CValidError_imp& imp)
{
    x_ClearMaps();

    vector<TTaxError> errs;

    // lookup of whole org
    vector< CRef<COrg_ref> > org_rq_list;
    CRef<COrg_ref> rq(new COrg_ref);
    rq->Assign(org);
    org_rq_list.push_back(rq);

    CRef<CTaxon3_reply> reply = m_tax_func(org_rq_list);

    if (!reply || !reply->IsSetReply()) {
        imp.PostErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyServiceProblem,
                "Taxonomy service connection failure", org);
    } else {
        ListTaxLookupErrors(*(reply->GetReply().front()), org, genome,
            false, false, errs);
    }

    // Now look at specific-host values
    m_HostMap.AddOrg(org);
    org_rq_list = GetSpecificHostLookupRequest(false);

    if (!org_rq_list.empty()) {
        reply = m_tax_func(org_rq_list);
        string err_msg;
        if (reply) {
            err_msg = IncrementalSpecificHostMapUpdate(org_rq_list, *reply);
        } else {
            err_msg = "Connection to taxonomy failed";
        }
        if (!NStr::IsBlank(err_msg)) {
            imp.PostErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyLookupProblem, err_msg, org);
        } else {
            m_HostMap.ListErrors(errs);
        }
    }


    // validate strain
    m_StrainMap.AddOrg(org);
    org_rq_list = GetStrainLookupRequest();
    if (!org_rq_list.empty()) {
        reply = m_tax_func(org_rq_list);
        string err_msg = IncrementalStrainMapUpdate(org_rq_list, *reply);
        if (!NStr::IsBlank(err_msg)) {
            imp.PostErr(eDiag_Error, eErr_SEQ_DESCR_TaxonomyLookupProblem, err_msg, org);
        } else {
            m_StrainMap.ListErrors(errs);
        }
    }

    for (const TTaxError& e : errs) {
        imp.PostObjErr(e.severity, e.err_type, e.err_msg, org);
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
