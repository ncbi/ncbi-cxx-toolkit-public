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
        case eErr_SEQ_FEAT_BadEcNumberFormat:
        case eErr_SEQ_FEAT_BadEcNumberValue:
        case eErr_SEQ_FEAT_EcNumberProblem:
            rval = x_FormatECNumberForSubmitterReport(error, scope);
            break;
        case eErr_SEQ_DESCR_BadSpecificHost:
            rval = x_FormatBadSpecificHostForSubmitterReport(error);
            break;
        case eErr_SEQ_DESCR_BadInstitutionCode:
            rval = x_FormatBadInstCodeForSubmitterReport(error);
            break;
        case eErr_SEQ_DESCR_LatLonCountry:
            rval = x_FormatLatLonCountryForSubmitterReport(error);
            break;
        default:
            rval = rval = error.GetAccession() + ":" + error.GetObjDesc();
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
    } else if (NStr::Find(msg, "(GT) not found") != string::npos) {
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
    rval = "\t" + error.GetObjDesc() + "\t" + rval;

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


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
