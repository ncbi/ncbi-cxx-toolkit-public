/* $Id$
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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko.......
 *
 * File Description:
 *   Validates CSeq_entries and CSeq_submits
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/general/User_object.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/validator/validerror_format.hpp>
#include <objtools/validator/validatorp.hpp>
#include <util/static_map.hpp>



BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
USING_SCOPE(sequence);


// *********************** CValidErrorFormat implementation **********************


CValidErrorFormat::CValidErrorFormat(CObjectManager& objmgr) :
    m_ObjMgr(&objmgr)
{
}


CValidErrorFormat::~CValidErrorFormat(void)
{
}


ESubmitterFormatErrorGroup CValidErrorFormat::GetSubmitterFormatErrorGroup(CValidErrItem::TErrIndex err_code) const
{
    ESubmitterFormatErrorGroup rval = eSubmitterFormatErrorGroup_Default;

    switch(err_code) {
        case eErr_SEQ_FEAT_NotSpliceConsensus:
        case eErr_SEQ_FEAT_NotSpliceConsensusDonor:
        case eErr_SEQ_FEAT_NotSpliceConsensusAcceptor:
        case eErr_SEQ_FEAT_RareSpliceConsensusDonor:
            rval = eSubmitterFormatErrorGroup_ConsensusSplice;
            break;
        case eErr_SEQ_FEAT_BadEcNumberFormat:
            rval = eSubmitterFormatErrorGroup_BadEcNumberFormat;
            break;
        case eErr_SEQ_FEAT_BadEcNumberValue:
        case eErr_SEQ_FEAT_DeletedEcNumber:
        case eErr_SEQ_FEAT_ReplacedEcNumber:
        case eErr_SEQ_FEAT_SplitEcNumber:
            rval = eSubmitterFormatErrorGroup_BadEcNumberValue;
            break;
        case eErr_SEQ_FEAT_EcNumberProblem:
            rval = eSubmitterFormatErrorGroup_BadEcNumberProblem;
            break;
        case eErr_SEQ_DESCR_BadSpecificHost:
            rval = eSubmitterFormatErrorGroup_BadSpecificHost;
            break;
        case eErr_SEQ_DESCR_BadInstitutionCode:
            rval = eSubmitterFormatErrorGroup_BadInstitutionCode;
            break;
        case eErr_SEQ_DESCR_LatLonCountry:
        case eErr_SEQ_DESCR_LatLonWater:
            rval = eSubmitterFormatErrorGroup_LatLonCountry;
            break;
        default:
            break;
    }
    return rval;
}


string CValidErrorFormat::GetSubmitterFormatErrorGroupTitle(CValidErrItem::TErrIndex err_code) const
{
    string rval = "";
    switch(err_code) {
        case eErr_SEQ_FEAT_NotSpliceConsensus:
        case eErr_SEQ_FEAT_NotSpliceConsensusDonor:
        case eErr_SEQ_FEAT_NotSpliceConsensusAcceptor:
        case eErr_SEQ_FEAT_RareSpliceConsensusDonor:
            rval = "Not Splice Consensus";
            break;
        case eErr_SEQ_FEAT_BadEcNumberFormat:
            rval = "EC Number Format";
            break;
        case eErr_SEQ_FEAT_BadEcNumberValue:
        case eErr_SEQ_FEAT_DeletedEcNumber:
        case eErr_SEQ_FEAT_ReplacedEcNumber:
        case eErr_SEQ_FEAT_SplitEcNumber:
            rval = "EC Number Value";
            break;
        case eErr_SEQ_FEAT_EcNumberProblem:
            rval = "EC Number Problem";
            break;
        case eErr_SEQ_DESCR_BadSpecificHost:
            rval = "Bad Specific-host Values";
            break;
        case eErr_SEQ_DESCR_BadInstitutionCode:
            rval = "Bad Institution Codes";
            break;
        case eErr_SEQ_DESCR_LatLonCountry:
        case eErr_SEQ_DESCR_LatLonWater:
            rval = "LatLonCountry Errors";
            break;
        default:
            rval = CValidErrItem::ConvertErrCode(err_code);
            break;
    }
    
    return rval;
}


string CValidErrorFormat::FormatForSubmitterReport(const CValidErrItem& error, CScope& scope) const
{
    string rval = "";

    switch (error.GetErrIndex()) {
        case eErr_SEQ_FEAT_NotSpliceConsensus:
        case eErr_SEQ_FEAT_NotSpliceConsensusDonor:
        case eErr_SEQ_FEAT_NotSpliceConsensusAcceptor:
        case eErr_SEQ_FEAT_RareSpliceConsensusDonor:
            rval = x_FormatConsensusSpliceForSubmitterReport(error);
            break;
        case eErr_SEQ_FEAT_PartialProblem:
            rval = x_FormatPartialProblemForSubmitterReport(error);
            break;
        case eErr_SEQ_FEAT_BadEcNumberFormat:
        case eErr_SEQ_FEAT_BadEcNumberValue:
        case eErr_SEQ_FEAT_EcNumberProblem:
        case eErr_SEQ_FEAT_DeletedEcNumber:
        case eErr_SEQ_FEAT_ReplacedEcNumber:
        case eErr_SEQ_FEAT_SplitEcNumber:
            rval = x_FormatECNumberForSubmitterReport(error, scope);
            break;
        case eErr_SEQ_DESCR_BadSpecificHost:
            rval = x_FormatBadSpecificHostForSubmitterReport(error);
            break;
        case eErr_SEQ_DESCR_BadInstitutionCode:
            rval = x_FormatBadInstCodeForSubmitterReport(error);
            break;
        case eErr_SEQ_DESCR_LatLonCountry:
        case eErr_SEQ_DESCR_LatLonWater:
            rval = x_FormatLatLonCountryForSubmitterReport(error);
            break;
        default:
            rval = error.GetAccession() + ":" + error.GetObjDesc();
            break;
    }

    return rval;
}


string CValidErrorFormat::x_FormatConsensusSpliceForSubmitterReport(const CValidErrItem& error) const
{
    string rval = "";
    if (!error.IsSetMsg() || NStr::IsBlank(error.GetMsg())) {
        return rval;
    }
    string msg = error.GetMsg();
    if (NStr::Find(msg, "(AG) not found") != string::npos) {
        rval = "AG";
    }
    else if (NStr::Find(msg, "(GT) not found") != string::npos) {
        rval = "GT";
    }
    if (NStr::IsBlank(rval)) {
        return rval;
    }

    size_t position_pos = NStr::Find(msg, "position ");
    if (position_pos != string::npos) {
        string pos_str = msg.substr(position_pos);
        long int pos;
        if (sscanf(pos_str.c_str(), "position %ld of ", &pos) == 1) {
            rval += " at " + NStr::NumericToString(pos);
            size_t seq_pos = NStr::Find(pos_str, " of ");
            if (seq_pos != string::npos) {
                rval = pos_str.substr(seq_pos + 4) + "\t" + rval;
            }
        }
    }

    string obj_desc = error.GetObjDesc();
    size_t type_pos = NStr::Find(obj_desc, "FEATURE: ");
    if (type_pos != string::npos) {
        obj_desc = obj_desc.substr(type_pos + 9);
        size_t space_pos = NStr::Find(obj_desc, ":");
        if (space_pos != string::npos) {
            obj_desc = obj_desc.substr(0, space_pos);
        }
    }

    rval = obj_desc + "\t" + rval;

    return rval;
}


string CValidErrorFormat::x_FormatPartialProblemForSubmitterReport(const CValidErrItem& error) const
{
    string obj_desc = error.GetObjDesc();
    size_t type_pos = NStr::Find(obj_desc, "FEATURE: ");
    if (type_pos != string::npos) {
        obj_desc = obj_desc.substr(type_pos + 9);
    }
    NStr::ReplaceInPlace(obj_desc, ":", "\t", 0, 1);
    size_t close_pos = NStr::Find(obj_desc, "]");
    if (close_pos != string::npos) {
        obj_desc = obj_desc.substr(0, close_pos);
        NStr::ReplaceInPlace(obj_desc, "[", "\t");
    }

    string rval = error.GetAccession() + ":" + obj_desc;

    return rval;
}


string CValidErrorFormat::x_FormatECNumberForSubmitterReport(const CValidErrItem& error, CScope& scope) const
{
    string rval = "";
    string ec_numbers = "";
    string prot_name = "";
    string locus_tag = "";

    // want: accession number for sequence, ec numbers, locus tag, protein name

    const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(&(error.GetObject()));
    if (!feat) {
        return rval;
    }

    // look for EC number in quals
    if (feat->IsSetQual()) {
        ITERATE(CSeq_feat::TQual, it, feat->GetQual()) {
            if ((*it)->IsSetQual() && 
                NStr::EqualNocase((*it)->GetQual(), "EC_number") &&
                (*it)->IsSetVal() &&
                !NStr::IsBlank((*it)->GetVal())) {
                if (!NStr::IsBlank(ec_numbers)) {
                    ec_numbers += ";";
                }
                ec_numbers += (*it)->GetVal();
            }
        }
    }
    // look for EC number in prot-ref
    if (feat->IsSetData() && feat->GetData().IsProt() &&
        feat->GetData().GetProt().IsSetEc()) {
        ITERATE(CProt_ref::TEc, it, feat->GetData().GetProt().GetEc()) {
            if (!NStr::IsBlank(ec_numbers)) {
                    ec_numbers += ";";
            }
            ec_numbers += *it;
        }
    }

    if (NStr::IsBlank(ec_numbers)) {
        ec_numbers = "Blank EC number";
    }

    // look for protein name
    if (feat->IsSetData() && feat->GetData().IsProt() &&
        feat->GetData().GetProt().IsSetName() &&
        !feat->GetData().GetProt().GetName().empty()) {
        prot_name = feat->GetData().GetProt().GetName().front();
    }

    // get locus tag
    CConstRef <CSeq_feat> gene = CValidError_bioseq::GetGeneForFeature (*feat, &scope);
    if (gene && gene->GetData().GetGene().IsSetLocus_tag()) {
        locus_tag = gene->GetData().GetGene().GetLocus_tag();
    }

    rval = error.GetAccnver() + "\t" + ec_numbers + "\t" + locus_tag + "\t" + prot_name;
    return rval;
}


string s_GetSpecificHostFromBioSource(const CBioSource& biosrc)
{
    string rval = "";

    if (biosrc.IsSetOrg() && 
        biosrc.GetOrg().IsSetOrgname() &&
        biosrc.GetOrg().GetOrgname().IsSetMod()) {
        ITERATE(COrgName::TMod, it, biosrc.GetOrg().GetOrgname().GetMod()) {
            if ((*it)->IsSetSubtype() &&
                (*it)->GetSubtype() == COrgMod::eSubtype_nat_host &&
                (*it)->IsSetSubname() &&
                !NStr::IsBlank((*it)->GetSubname())) {
                if (!NStr::IsBlank(rval)) {
                    rval += ";";
                }
                rval += (*it)->GetSubname();
            }
        }
    }
    return rval;
}


string CValidErrorFormat::x_FormatBadSpecificHostForSubmitterReport(const CValidErrItem& error) const
{
    string rval = "";
    string spec_host = "";
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc *>(&(error.GetObject()));
    if (desc && desc->IsSource()) {
        spec_host = s_GetSpecificHostFromBioSource(desc->GetSource());
    }
    const CSeq_feat* feat = dynamic_cast<const CSeq_feat *>(&(error.GetObject()));
    if (feat && feat->IsSetData() && feat->GetData().IsBiosrc()) {
        spec_host = s_GetSpecificHostFromBioSource(feat->GetData().GetBiosrc());
    }

    if (!NStr::IsBlank(spec_host)) {
        rval = error.GetAccession() + "\t" + spec_host;
    }

    return rval;
}


string s_GetInstCodeFromBioSource(const CBioSource& biosrc)
{
    string rval = "";

    if (biosrc.IsSetOrg() && 
        biosrc.GetOrg().IsSetOrgname() &&
        biosrc.GetOrg().GetOrgname().IsSetMod()) {
        ITERATE(COrgName::TMod, it, biosrc.GetOrg().GetOrgname().GetMod()) {
            if ((*it)->IsSetSubtype() &&
                ((*it)->GetSubtype() == COrgMod::eSubtype_bio_material ||
                 (*it)->GetSubtype() == COrgMod::eSubtype_culture_collection ||
                 (*it)->GetSubtype() == COrgMod::eSubtype_specimen_voucher) &&
                (*it)->IsSetSubname() &&
                !NStr::IsBlank((*it)->GetSubname())) {
                size_t pos = NStr::Find((*it)->GetSubname(), ":");
                if (pos != string::npos) {
                    string code = (*it)->GetSubname().substr(0, pos);
                    if (!NStr::IsBlank(code)) {
                        if (!NStr::IsBlank(rval)) {
                            rval += ";";
                        }
                        rval += code;
                    }
                }
            }
        }
    }
    return rval;
}


string CValidErrorFormat::x_FormatBadInstCodeForSubmitterReport(const CValidErrItem& error) const
{
    string rval = "";

    string codes = "";
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc *>(&(error.GetObject()));
    if (desc && desc->IsSource()) {
        codes = s_GetInstCodeFromBioSource(desc->GetSource());
    }
    const CSeq_feat* feat = dynamic_cast<const CSeq_feat *>(&(error.GetObject()));
    if (feat && feat->IsSetData() && feat->GetData().IsBiosrc()) {
        codes = s_GetInstCodeFromBioSource(feat->GetData().GetBiosrc());
    }

    if (!NStr::IsBlank(codes)) {
        rval = error.GetAccession() + "\t" + codes;
    }

    return rval;
}


string CValidErrorFormat::FormatForSubmitterReport(const CValidError& errors, CScope& scope, CValidErrItem::TErrIndex err_code) const
{
    string rval = "";
    for ( CValidError_CI vit(errors); vit; ++vit) {
        if (err_code == vit->GetErrIndex()) {
            string this_val = FormatForSubmitterReport(*vit, scope);
            if (!NStr::IsBlank(this_val)) {
                if (NStr::IsBlank(rval)) {
                    rval += GetSubmitterFormatErrorGroupTitle(err_code) + "\n";
                }
                rval += this_val + "\n";
            }
        }
    }
    return rval;
}


string CValidErrorFormat::FormatCategoryForSubmitterReport
    (const CValidError& errors, CScope& scope, ESubmitterFormatErrorGroup grp) const
{
    string rval = "";
    for ( CValidError_CI vit(errors); vit; ++vit) {
        CValidErrItem::TErrIndex err_code = vit->GetErrIndex();
        if (GetSubmitterFormatErrorGroup(err_code) == grp) {
            string this_val = FormatForSubmitterReport(*vit, scope);
            if (!NStr::IsBlank(this_val)) {
                if (NStr::IsBlank(rval)) {
                    rval += GetSubmitterFormatErrorGroupTitle(err_code) + "\n";
                }
                rval += this_val + "\n";
            }
        }
    }
    return rval;
}


string CValidErrorFormat::x_FormatLatLonCountryForSubmitterReport(const CValidErrItem& error) const
{
    string rval = error.GetAccession() + ":" + error.GetMsg();
    return rval;
}


vector<unsigned int> CValidErrorFormat::GetListOfErrorCodes(const CValidError& errors) const
{
    vector<unsigned int> list;

    for ( CValidError_CI vit(errors); vit; ++vit) {
        list.push_back(vit->GetErrIndex());
    }
    sort(list.begin(), list.end());
    list.erase(unique(list.begin(), list.end()), list.end());
    return list;
}


vector<string> CValidErrorFormat::FormatCompleteSubmitterReport(const CValidError& errors, CScope& scope) const
{
    vector<string> list;

    // first, do special categories
    for (unsigned int t = eSubmitterFormatErrorGroup_ConsensusSplice; t < eSubmitterFormatErrorGroup_Default; ++t) {
        string this_val = FormatCategoryForSubmitterReport(errors, scope, (ESubmitterFormatErrorGroup)t);
        if (!NStr::IsBlank(this_val)) {
            list.push_back(this_val);
        }
    }

    // now do errors not in special categories
    vector<unsigned int> codes = GetListOfErrorCodes(errors);
    ITERATE(vector<unsigned int>, it, codes) {
        if (GetSubmitterFormatErrorGroup(*it) == eSubmitterFormatErrorGroup_Default) {
            string this_val = FormatForSubmitterReport(errors, scope, *it);
            if (!NStr::IsBlank(this_val)) {
                list.push_back(this_val);
            }
        }
    }
    return list;
}


static string s_GetFeatureIdLabel (const CObject_id& object_id)
{
    string feature_id = "";

    if (object_id.IsId()) {
        feature_id = NStr::IntToString(object_id.GetId());
    } else if (object_id.IsStr()) {
        feature_id = object_id.GetStr();
    }
    return feature_id;
}


string CValidErrorFormat::GetFeatureIdLabel (const CFeat_id& feat_id)
{
    string feature_id = "";
    if (feat_id.IsLocal()) {
        feature_id = s_GetFeatureIdLabel(feat_id.GetLocal());
    } else if (feat_id.IsGeneral()) {
        if (feat_id.GetGeneral().IsSetDb()) {
            feature_id += feat_id.GetGeneral().GetDb();
        }
        feature_id += ":";
        if (feat_id.GetGeneral().IsSetTag()) {
            feature_id += s_GetFeatureIdLabel (feat_id.GetGeneral().GetTag());
        }
    }
    return feature_id;
}


static void s_FixBioseqLabelProblems (string& str)
{
    size_t pos = NStr::Find(str, ",");
    if (pos != string::npos && str.c_str()[pos + 1] != 0 && str.c_str()[pos + 1] != ' ') {
        str = str.substr(0, pos + 1) + " " + str.substr(pos + 1);
    }
    pos = NStr::Find(str, "=");
    if (pos != string::npos && str.c_str()[pos + 1] != 0 && str.c_str()[pos + 1] != ' ') {
        str = str.substr(0, pos + 1) + " " + str.substr(pos + 1);
    }
}



static string s_GetOrgRefContentLabel (const COrg_ref& org)
{
    string content = "";
    if (org.IsSetTaxname()) {
        content = org.GetTaxname();
    } else if (org.IsSetCommon()) {
        content = org.GetCommon();
    } else if (org.IsSetDb() && !org.GetDb().empty()) {
        org.GetDb().front()->GetLabel(&content);
    }
    return content;
}


static string s_GetBioSourceContentLabel (const CBioSource& bsrc)
{
    string content = "";

    if (bsrc.IsSetOrg()) {
        content = s_GetOrgRefContentLabel(bsrc.GetOrg());
    }
    return content;
}


static string s_GetFeatureContentLabelExtras (const CSeq_feat& feat)
{
    string tlabel = "";

    // Put Seq-feat qual into label
    if (feat.IsSetQual()) {
        string prefix("/");
        ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
            tlabel += prefix + (**it).GetQual();
            prefix = " ";
            if (!(**it).GetVal().empty()) {
                tlabel += "=" + (**it).GetVal();
            }
        }
    }
    
    // Put Seq-feat comment into label
    if (feat.IsSetComment()) {
        if (tlabel.empty()) {
            tlabel = feat.GetComment();
        } else {
            tlabel += "; " + feat.GetComment();
        }
    }
    return tlabel;
}


static string s_GetCdregionContentLabel (const CSeq_feat& feat, CRef<CScope> scope)
{
    string content = "";

    // Check that feature data is Cdregion
    if (!feat.GetData().IsCdregion()) {
        return content;
    }
    
    const CGene_ref* gref = 0;
    const CProt_ref* pref = 0;
    
    // Look for CProt_ref object to create a label from
    if (feat.IsSetXref()) {
        ITERATE ( CSeq_feat::TXref, it, feat.GetXref()) {
            const CSeqFeatXref& xref = **it;
            if ( !xref.IsSetData() ) {
                continue;
            }

            switch (xref.GetData().Which()) {
            case CSeqFeatData::e_Prot:
                pref = &xref.GetData().GetProt();
                break;
            case CSeqFeatData::e_Gene:
                gref = &xref.GetData().GetGene();
                break;
            default:
                break;
            }
        }
    }
    
    // Try and create a label from a CProt_ref in CSeqFeatXref in feature
    if (pref) {
        pref->GetLabel(&content);
        return content;
    }
    
    // Try and create a label from a CProt_ref in the feat product and
    // return if found 
    if (feat.IsSetProduct()  &&  scope) {
        try {
            const CSeq_id& id = GetId(feat.GetProduct(), scope);            
            CBioseq_Handle hnd = scope->GetBioseqHandle(id);
            if (hnd) {
                const CBioseq& seq = *hnd.GetCompleteBioseq();
            
                // Now look for a CProt_ref feature in seq and
                // if found call GetLabel() on the CProt_ref
                CTypeConstIterator<CSeqFeatData> it = ConstBegin(seq);
                for (;it; ++it) {
                    if (it->IsProt()) {
                        it->GetProt().GetLabel(&content);
                        return content;
                    }
                }
            }
        } catch (CObjmgrUtilException&) {}
    }
    
    // Try and create a label from a CGene_ref in CSeqFeatXref in feature
    if (gref) {
        gref->GetLabel(&content);
    }

    if (NStr::IsBlank(content)) {
        content = s_GetFeatureContentLabelExtras(feat);
    }

    return content;
}


string CValidErrorFormat::GetFeatureContentLabel (const CSeq_feat& feat, CRef<CScope> scope)
{
    string content_label = "";

    switch (feat.GetData().Which()) {
        case CSeqFeatData::e_Pub:
            content_label = "Cit: ";
            feat.GetData().GetPub().GetPub().GetLabel(&content_label);
            break;
        case CSeqFeatData::e_Biosrc:
            content_label = "Src: " + s_GetBioSourceContentLabel (feat.GetData().GetBiosrc());
            break;
        case CSeqFeatData::e_Imp:
            {
                feature::GetLabel(feat, &content_label, feature::fFGL_Both, scope);
                if (feat.GetData().GetImp().IsSetKey()) {
                    string key = feat.GetData().GetImp().GetKey();
                    string tmp = "[" + key + "]";
                    if (NStr::StartsWith(content_label, tmp)) {
                        content_label = key + content_label.substr(tmp.length());
                    }
                }
            }
            break;
        case CSeqFeatData::e_Rna:
            feature::GetLabel(feat, &content_label, feature::fFGL_Both, scope);
            if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA
                && NStr::Equal(content_label, "tRNA: tRNA")) {
                content_label = "tRNA: ";
            }
            break;
        case CSeqFeatData::e_Cdregion:
            content_label = "CDS: " + s_GetCdregionContentLabel(feat, scope);
            break;
        case CSeqFeatData::e_Prot:
            feature::GetLabel(feat, &content_label, feature::fFGL_Both, scope);
            if (feat.GetData().GetProt().IsSetProcessed()) {
                switch (feat.GetData().GetProt().GetProcessed()) {
                    case CProt_ref::eProcessed_mature:
                        content_label = "mat_peptide: " + content_label.substr(6);
                        break;
                    case CProt_ref::eProcessed_signal_peptide:
                        content_label = "sig_peptide: " + content_label.substr(6);
                        break;
                    case CProt_ref::eProcessed_transit_peptide:
                        content_label = "trans_peptide: " + content_label.substr(6);
                        break;
                    default:
                        break;
                }
            }
            break;
        default:
            feature::GetLabel(feat, &content_label, feature::fFGL_Both, scope);
            break;
    }
    return content_label;
}


string CValidErrorFormat::GetFeatureLabel(const CSeq_feat& ft, CRef<CScope> scope, bool suppress_context)
{
    // Add feature part of label
    string desc = "FEATURE: ";
    string content_label = CValidErrorFormat::GetFeatureContentLabel(ft, scope);
    desc += content_label;

    // Add feature ID part of label (if present)
    string feature_id = "";
    if (ft.IsSetId()) {
        feature_id = CValidErrorFormat::GetFeatureIdLabel(ft.GetId());
    } else if (ft.IsSetIds()) {
        ITERATE (CSeq_feat::TIds, id_it, ft.GetIds()) {
            feature_id = CValidErrorFormat::GetFeatureIdLabel ((**id_it));
            if (!NStr::IsBlank(feature_id)) {
                break;
            }
        }
    }
    if (!NStr::IsBlank(feature_id)) {
        desc += " <" + feature_id + "> ";
    }

    string loc_label;
    // Add feature location part of label
    if (ft.IsSetLocation() && scope) {
        if (suppress_context) {
            CSeq_loc loc;
            loc.Assign(ft.GetLocation());
            ChangeSeqLocId(&loc, false, scope);
            loc_label = "[" + GetValidatorLocationLabel(loc, *scope) + "]";
        } else {
            loc_label = "[" + GetValidatorLocationLabel(ft.GetLocation(), *scope) + "]";
        }
        if (loc_label.size() > 800) {
            loc_label.replace(796, NPOS, "...]");
        }
        desc += " " + loc_label;
    }

    // Append label for bioseq of feature location
    if (!suppress_context && scope) {
        bool find_failed = false;
        try {
            CBioseq_Handle hnd = scope->GetBioseqHandle(ft.GetLocation());
            if (hnd) {
                desc += CValidErrorFormat::GetBioseqLabel(hnd);
            }
        } catch (CObjMgrException& ex) {
            if (ex.GetErrCode() == CObjMgrException::eFindFailed) {
                find_failed = true;
            }
        } catch (CException ) {
        } catch (std::exception ) {
        };
        if (find_failed) {
            try {
                CSeq_loc_CI li(ft.GetLocation());
                CBioseq_Handle hnd = scope->GetBioseqHandle(li.GetSeq_id());
                if (hnd) {
                    desc += CValidErrorFormat::GetBioseqLabel(hnd);
                }

            } catch (CException) {
            } catch (std::exception) {
            };
        }
    }

    // Append label for product of feature
    if (ft.IsSetProduct() && scope) {
        loc_label.erase();
        if (suppress_context) {
            CSeq_loc loc;
            loc.Assign(ft.GetProduct());
            ChangeSeqLocId(&loc, false, scope);
            loc_label = GetValidatorLocationLabel(loc, *scope);
        } else {
            loc_label = GetValidatorLocationLabel (ft.GetProduct(), *scope);
        }
        if (loc_label.size() > 800) {
            loc_label.replace(797, NPOS, "...");
        }
        if (!loc_label.empty()) {
            desc += " -> [";
            desc += loc_label;
            desc += "]";
        }
    }
    return desc;
}


string CValidErrorFormat::GetDescriptorContent (const CSeqdesc& ds)
{
    string content = "";

    switch (ds.Which()) {
        case CSeqdesc::e_Pub:
            content = "Pub: ";
            ds.GetPub().GetPub().GetLabel(&content);
            break;
        case CSeqdesc::e_Source:
            content = "BioSource: " + s_GetBioSourceContentLabel(ds.GetSource());
            break;
        case CSeqdesc::e_Modif:
            ds.GetLabel(&content, CSeqdesc::eBoth);
            if (NStr::StartsWith(content, "modif: ,")) {
                content = "Modifier: " + content.substr(8);
            }
            break;
        case CSeqdesc::e_Molinfo:
            ds.GetLabel(&content, CSeqdesc::eBoth);
            if (NStr::StartsWith(content, "molinfo: ,")) {
                content = "molInfo: " + content.substr(10);
            }
            break;
        case CSeqdesc::e_Comment:
            ds.GetLabel(&content, CSeqdesc::eBoth);
            if (NStr::StartsWith(content, "comment: ") && NStr::IsBlank(content.substr(9))) {
                content = "comment: ";
            }
            break;
        case CSeqdesc::e_User:
            content = "UserObj: ";
            if (ds.GetUser().IsSetClass()) {
                content += ds.GetUser().GetClass();
            } else if (ds.GetUser().IsSetType() && ds.GetUser().GetType().IsStr()) {
                content += ds.GetUser().GetType().GetStr();
            }
            break;
        default:
            ds.GetLabel(&content, CSeqdesc::eBoth);
            break;
    }
    // fix descriptor type names
    string first = content.substr(0, 1);
    NStr::ToUpper(first);
    content = first + content.substr(1);
    size_t colon_pos = NStr::Find(content, ":");
    if (colon_pos != string::npos) {
        size_t dash_pos = NStr::Find(content, "-", 0, colon_pos);
        if (dash_pos != string::npos) {
            string after_dash = content.substr(dash_pos + 1, 1);
            NStr::ToUpper (after_dash);
            content = content.substr(0, dash_pos) + after_dash + content.substr(dash_pos + 2);
        }
    }
    if (NStr::StartsWith(content, "BioSource:")) {
        content = "BioSrc:" + content.substr(10);
    } else if (NStr::StartsWith(content, "Modif:")) {
        content = "Modifier:" + content.substr(6);
    } else if (NStr::StartsWith(content, "Embl:")) {
        content = "EMBL:" + content.substr(5);
    } else if (NStr::StartsWith(content, "Pir:")) {
        content = "PIR:" + content.substr(4);
    }
    return content;
}


string CValidErrorFormat::GetDescriptorLabel(const CSeqdesc& ds, const CSeq_entry& ctx, CRef<CScope> scope, bool suppress_context)
{
    string desc("DESCRIPTOR: ");

    string content = CValidErrorFormat::GetDescriptorContent (ds);

    desc += content;

    desc += " ";
    if (ctx.IsSeq()) {
        AppendBioseqLabel(desc, ctx.GetSeq(), suppress_context);
    } else {
        desc += CValidErrorFormat::GetBioseqSetLabel(ctx.GetSet(), scope, suppress_context);
    }
   return desc;
}


string CValidErrorFormat::GetBioseqLabel (CBioseq_Handle bh)
{
    string desc = "";

    CBioseq_Handle::TBioseqCore bc = bh.GetBioseqCore();
    desc += " [";
    string bc_label = "";
    bc->GetLabel(&bc_label, CBioseq::eBoth);
    s_FixBioseqLabelProblems(bc_label);
    desc += bc_label;
    desc += "]";
    return desc;
}


string CValidErrorFormat::GetBioseqSetLabel(const CBioseq_set& st, CRef<CScope> scope, bool suppress_context)
{
    string str = "";
    // GetLabel for CBioseq_set does not follow C Toolkit conventions
    // AND is a horrible performance hit for sets with lots of sequences

    const CBioseq* best = 0;
    CTypeConstIterator<CBioseq> si(ConstBegin(st));
    if (si) {
        best = &(*si);
    }
    // Add content to label.
    if (!best) {
        str += "BIOSEQ-SET: ";
        if (!suppress_context && st.IsSetClass()) {
            const CEnumeratedTypeValues* tv =
                CBioseq_set::GetTypeInfo_enum_EClass();
            const string& cn = tv->FindName(st.GetClass(), true);
            str += cn;
            str += ": ";
        }

        str += "(No Bioseqs)";
    } else if (st.IsSetClass()) {
        str += "BIOSEQ-SET: ";
        if (!suppress_context) {
            const CEnumeratedTypeValues* tv =
                CBioseq_set::GetTypeInfo_enum_EClass();
            const string& cn = tv->FindName(st.GetClass(), true);
            str += cn;
            str += ": ";
        }
        if (scope) {
            string content = "";
            int version = 0;
            const string& accn = GetAccessionFromObjects(&st, NULL, *scope, &version);
            content += accn;
            // best->GetLabel(&content, CBioseq::eContent, supress_context);
            // fix problems with label
            s_FixBioseqLabelProblems(content);
            str += content;
        }
    } else {
        AppendBioseqLabel(str, *best, suppress_context);
    }
    return str;
}


string CValidErrorFormat::GetObjectLabel(const CObject& obj, const CSeq_entry& ctx, CRef<CScope> scope, bool suppress_context)
{
    string label = "Unknown object";

    const CSeq_feat* ft = dynamic_cast<const CSeq_feat*>(&obj);
    const CSeqdesc* ds = dynamic_cast<const CSeqdesc*>(&obj);
    const CBioseq* b = dynamic_cast<const CBioseq*>(&obj);
    const CBioseq_set* set = dynamic_cast<const CBioseq_set*>(&obj);

    if (ft) {
        label = GetFeatureLabel(*ft, scope, suppress_context);
    } else if (ds) {
        label = GetDescriptorLabel(*ds, ctx, scope, suppress_context);
    } else if (b) {
        label = GetBioseqLabel(scope->GetBioseqHandle(*b));
    } else if (set) {
        label = GetBioseqSetLabel(*set, scope, suppress_context);
    }
    return label;
}


const string kSuppressFieldLabel = "Suppress";

bool s_IsSuppressField (const CUser_field& field)
{
    if (field.IsSetLabel() && 
        field.GetLabel().IsStr() && 
        NStr::EqualNocase(field.GetLabel().GetStr(), kSuppressFieldLabel)) {
        return true;
    } else {
        return false;
    }
}


void CValidErrorFormat::AddSuppression(CUser_object& user, unsigned int error_code)
{
    bool found = false;
    if (user.IsSetData()) {
        NON_CONST_ITERATE(CUser_object::TData, it, user.SetData()) {
            if (s_IsSuppressField(**it)) {
                if ((*it)->IsSetData()) {
                    if ((*it)->GetData().IsInt()) {
                        unsigned int old_val = (*it)->GetData().GetInt();
                        if (old_val == error_code) {
                            // do nothing, already there
                        } else {
                            (*it)->SetData().SetInts().push_back(old_val);
                            (*it)->SetData().SetInts().push_back(error_code);                            
                        }
                        found = true;
                        break;
                    } else if ((*it)->GetData().IsInts()) {
                        ITERATE(CUser_field::TData::TInts, ii, (*it)->GetData().GetInts()) {
                            if (*ii == error_code) {
                                found = true;
                                break;
                            }
                        } 
                        if (!found) {
                            (*it)->SetData().SetInts().push_back(error_code);
                            found = true;
                        }
                        break;
                    }
                }
            }
        }
    } 
    if (!found) {
        CRef<CUser_field> field(new CUser_field());
        field->SetLabel().SetStr(kSuppressFieldLabel);
        field->SetData().SetInts().push_back(error_code);
        user.SetData().push_back(field);
    }
}


void CValidErrorFormat::SetSuppressionRules(const CUser_object& user, CValidError& errors)
{
    if (!user.IsSetData()) {
        return;
    }
    ITERATE(CUser_object::TData, it, user.GetData()) {
        if ((*it)->IsSetData() && s_IsSuppressField(**it)) {
            if ((*it)->GetData().IsInt()) {                
                errors.SuppressError((*it)->GetData().GetInt());
            } else if ((*it)->GetData().IsInts()) {
                ITERATE(CUser_field::TData::TInts, ii, (*it)->GetData().GetInts()) {
                    errors.SuppressError(*ii);
                }
            } else if ((*it)->GetData().IsStr()) {
                unsigned int ec = CValidErrItem::ConvertToErrCode((*it)->GetData().GetStr());
                if (ec != eErr_MAX) {
                    errors.SuppressError(ec);
                }
            } else if ((*it)->GetData().IsStrs()) {
                ITERATE(CUser_field::TData::TStrs, si, (*it)->GetData().GetStrs()) {
                    unsigned int ec = CValidErrItem::ConvertToErrCode(*si);
                    if (ec != eErr_MAX) {
                        errors.SuppressError(ec);
                    }
                }
            }
        }
    }
}


void CValidErrorFormat::SetSuppressionRules(const CSeq_entry& se, CValidError& errors)
{
    if (se.IsSeq()) {
        SetSuppressionRules(se.GetSeq(), errors);
    } else if (se.IsSet()) {
        const CBioseq_set& set = se.GetSet();
        if (set.IsSetDescr()) {
            ITERATE(CBioseq_set::TDescr::Tdata, it, set.GetDescr().Get()) {
                if ((*it)->IsUser() && 
                    (*it)->GetUser().GetObjectType() == CUser_object::eObjectType_ValidationSuppression) {
                    SetSuppressionRules((*it)->GetUser(), errors);
                }
            }
        }
        if (set.IsSetSeq_set()) {
            ITERATE(CBioseq_set::TSeq_set, it, set.GetSeq_set()) {
                SetSuppressionRules(**it, errors);
            }
        }
    }
}


void CValidErrorFormat::SetSuppressionRules(const CSeq_entry_Handle& se, CValidError& errors)
{
    SetSuppressionRules(*(se.GetCompleteSeq_entry()), errors);
}


void CValidErrorFormat::SetSuppressionRules(const CSeq_submit& ss, CValidError& errors)
{
    if (ss.IsEntrys()) {
        ITERATE(CSeq_submit::TData::TEntrys, it, ss.GetData().GetEntrys()) {
            SetSuppressionRules(**it, errors);
        }
    }
}


void CValidErrorFormat::SetSuppressionRules(const CBioseq& seq, CValidError& errors)
{
    if (seq.IsSetDescr()) {
        ITERATE(CBioseq::TDescr::Tdata, it, seq.GetDescr().Get()) {
            if ((*it)->IsUser() && 
                (*it)->GetUser().GetObjectType() == CUser_object::eObjectType_ValidationSuppression) {
                SetSuppressionRules((*it)->GetUser(), errors);
            }
        }
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
