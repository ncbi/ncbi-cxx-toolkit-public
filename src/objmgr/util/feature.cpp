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
* Author:  Clifford Clausen
*
* File Description:
*   Sequence utilities
*/

#include <ncbi_pch.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>


#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/impl/handle_range_map.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Rsite_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Feat_id.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBI8aa.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Heterogen.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>

#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_set.hpp>

#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(feature)
USING_SCOPE(sequence);


// Appends a label onto "label" based on the type of feature       
void s_GetTypeLabel(const CSeq_feat& feat, string* label, TFeatLabelFlags flags)
{    
    string tlabel;
    
    // Determine typelabel
    CSeqFeatData::ESubtype idx = feat.GetData().GetSubtype();
    if ( idx != CSeqFeatData::eSubtype_bad ) {
        tlabel = feat.GetData().GetKey();
        if (feat.GetData().IsImp()  &&  tlabel != "CDS") {
            tlabel = "[" + tlabel + "]";
        } else if ((flags & fFGL_NoComments) == 0  &&  feat.GetData().IsRegion()
                   &&  feat.GetData().GetRegion() == "Domain"
                   &&  feat.IsSetComment() ) {
            tlabel = "Domain";
        }
    } else if (feat.GetData().IsImp()) {
        tlabel = "[" + feat.GetData().GetImp().GetKey() + "]";
    } else {
        tlabel = "Unknown=0";
    }
    *label += tlabel;  
}


// Appends a label onto tlabel for a CSeqFeatData::e_Cdregion
inline
static void s_GetCdregionLabel
(const CSeq_feat& feat, 
 string*          tlabel,
 CScope*          scope)
{
    // Check that tlabel exists and that the feature data is Cdregion
    if (!tlabel  ||  !feat.GetData().IsCdregion()) {
        return;
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
        pref->GetLabel(tlabel);
        return;
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
                        it->GetProt().GetLabel(tlabel);
                        return;
                    }
                }
            }
        } catch (CObjmgrUtilException&) {}
    }
    
    // Try and create a label from a CGene_ref in CSeqFeatXref in feature
    if (gref) {
        gref->GetLabel(tlabel);
    }

    // check to see if the CDregion is just an open reading frame
    if (feat.GetData().GetCdregion().IsSetOrf()  &&
        feat.GetData().GetCdregion().GetOrf()) {
        string str("open reading frame: ");
        switch (feat.GetData().GetCdregion().GetFrame()) {
        case CCdregion::eFrame_not_set:
            str += "frame not set; ";
            break;
        case CCdregion::eFrame_one:
            str += "frame 1; ";
            break;
        case CCdregion::eFrame_two:
            str += "frame 2; ";
            break;
        case CCdregion::eFrame_three:
            str += "frame 3; ";
            break;
        }

        switch (sequence::GetStrand(feat.GetLocation(), scope)) {
        case eNa_strand_plus:
            str += "positive strand";
            break;
        case eNa_strand_minus:
            str += "negative strand";
            break;
        case eNa_strand_both:
            str += "both strands";
            break;
        case eNa_strand_both_rev:
            str += "both strands (reverse)";
            break;
        default:
            str += "strand unknown";
            break;
        }

        *tlabel += str;
    }


}


inline
static void s_GetRnaRefLabelFromComment
(const CSeq_feat& feat, 
 string*          label,
 TFeatLabelFlags  flags,
 const string*    type_label)
{
    if ((flags & fFGL_NoComments) == 0  &&  feat.IsSetComment()
        &&  !feat.GetComment().empty()) {
        if ((flags & fFGL_Type) == 0  &&  type_label != NULL
            &&  feat.GetComment().find(*type_label) == string::npos) {
            *label += *type_label + "-" + feat.GetComment();
        } else {
            *label += feat.GetComment();
        }
    } else if (type_label) {
        *label += *type_label;
    }
}


// Appends a label onto "label" for a CRNA_ref
inline
static void s_GetRnaRefLabel
(const CSeq_feat& feat, 
 string*          label,
 TFeatLabelFlags  flags,
 const string*    type_label)
{
    // Check that label exists and that feature data is type RNA-ref
    if (!label  ||  !feat.GetData().IsRna()) {
        return;
    }
    
    const CRNA_ref& rna = feat.GetData().GetRna();
    
    // Append the feature comment, the type label, or both  and return 
    // if Ext is not set
    if (!rna.IsSetExt()) {
        s_GetRnaRefLabelFromComment(feat, label, flags, type_label);
        return;
    }
    
    // Append a label based on the type of the type of the ext of the
    // CRna_ref
    string tmp_label;
    switch (rna.GetExt().Which()) {
    case CRNA_ref::C_Ext::e_not_set:
        s_GetRnaRefLabelFromComment(feat, label, flags, type_label);
        break;
    case CRNA_ref::C_Ext::e_Name:
        tmp_label = rna.GetExt().GetName();
        if (feat.CanGetQual()  &&
            (tmp_label == "ncRNA"  ||  tmp_label == "tmRNA"
             ||  tmp_label == "misc_RNA")) {
            ITERATE (CSeq_feat::TQual, q, feat.GetQual()) {
                if ((*q)->GetQual() == "product") {
                    tmp_label = (*q)->GetVal();
                    break;
                }
            }
        }
        if ((flags & fFGL_Type) == 0  &&  type_label != 0
            &&  tmp_label.find(*type_label) == string::npos) {
            *label += *type_label + "-" + tmp_label;
        } else if (!tmp_label.empty()) {
            *label += tmp_label;
        } else if (type_label) {
            *label += *type_label;
        }
        break;
    case CRNA_ref::C_Ext::e_TRNA:
    {
        if ( !rna.GetExt().GetTRNA().IsSetAa() ) {
            s_GetRnaRefLabelFromComment(feat, label, flags, type_label);
            break;                
        }
        try {
            CTrna_ext::C_Aa::E_Choice aa_code_type = 
                rna.GetExt().GetTRNA().GetAa().Which();
            int aa_code;
            CSeq_data in_seq, out_seq;
            string str_aa_code;
            switch (aa_code_type) {
            case CTrna_ext::C_Aa::e_Iupacaa:        
                // Convert an e_Iupacaa code to an Iupacaa3 code for the label
                aa_code = rna.GetExt().GetTRNA().GetAa().GetIupacaa();
                str_aa_code = CSeqportUtil::GetCode(CSeq_data::e_Iupacaa,
                                                    aa_code); 
                in_seq.SetIupacaa().Set() = str_aa_code;
                CSeqportUtil::Convert(in_seq, &out_seq,
                                      CSeq_data::e_Ncbistdaa);
                if (out_seq.GetNcbistdaa().Get().size()) {
                    aa_code = out_seq.GetNcbistdaa().Get()[0];
                    tmp_label = CSeqportUtil::GetIupacaa3(aa_code);
                } else {
                    s_GetRnaRefLabelFromComment(feat, label, flags, type_label);
                }
                break;
            case CTrna_ext::C_Aa::e_Ncbieaa:
                // Convert an e_Ncbieaa code to an Iupacaa3 code for the label
                aa_code = rna.GetExt().GetTRNA().GetAa().GetNcbieaa();
                str_aa_code = CSeqportUtil::GetCode(CSeq_data::e_Ncbieaa,
                                                    aa_code);
                in_seq.SetNcbieaa().Set() = str_aa_code;
                CSeqportUtil::Convert(in_seq, &out_seq,
                                      CSeq_data::e_Ncbistdaa);
                if (out_seq.GetNcbistdaa().Get().size()) {
                    aa_code = out_seq.GetNcbistdaa().Get()[0];
                    tmp_label = CSeqportUtil::GetIupacaa3(aa_code);
                } else {
                    s_GetRnaRefLabelFromComment(feat, label, flags, type_label);
                }
                break;
            case CTrna_ext::C_Aa::e_Ncbi8aa:
                // Convert an e_Ncbi8aa code to an Iupacaa3 code for the label
                aa_code = rna.GetExt().GetTRNA().GetAa().GetNcbi8aa();
                tmp_label = CSeqportUtil::GetIupacaa3(aa_code);
                break;
            case CTrna_ext::C_Aa::e_Ncbistdaa:
                // Convert an e_Ncbistdaa code to an Iupacaa3 code for the label
                aa_code = rna.GetExt().GetTRNA().GetAa().GetNcbistdaa();
                tmp_label = CSeqportUtil::GetIupacaa3(aa_code);
                break;
            default:
                break;
            }
        
            // Append to label, depending on flags
            if ((flags & fFGL_Type) == 0  &&  type_label != 0) {
                *label += *type_label + "-" + tmp_label;
            } else if (!tmp_label.empty()) {
                *label += tmp_label;
            } else if (type_label) {
                *label += *type_label;
            }
        } catch (CSeqportUtil::CBadIndex&) {
            // fall back to comment (if any)
            s_GetRnaRefLabelFromComment(feat, label, flags, type_label);
        }
        
        break;
    }
    case CRNA_ref::C_Ext::e_Gen:
        if (rna.GetExt().GetGen().CanGetProduct()) {
            *label = rna.GetExt().GetGen().GetProduct();
        } else if (rna.GetExt().GetGen().CanGetClass()) {
            *label = rna.GetExt().GetGen().GetClass();
        } else {
            s_GetRnaRefLabelFromComment(feat, label, flags, type_label);
        }
        break;
    }
}


// Appends a label to tlabel for a CImp_feat. A return value of true indicates 
// that the label was created for a CImp_feat key = "Site-ref" 
inline
static bool s_GetImpLabel
(const CSeq_feat&      feat, 
 string*               tlabel,
 TFeatLabelFlags       flags,
 const string*         type_label)
{
    // Return if tlablel does not exist or feature data is not Imp-feat
    if (!tlabel  ||  !feat.GetData().IsImp()) {
        return false;
    }
    
    const string& key = feat.GetData().GetImp().GetKey();
    bool empty = true;
    
    // If the key is Site-ref
    if (NStr::EqualNocase(key, "Site-ref")) {
        if (feat.IsSetCit()) {
            // Create label based on Pub-set
            feat.GetCit().GetLabel(tlabel);
            return true;
        }
    // else if the key is not Site-ref
    } else if ((flags & fFGL_Type) == 0) {
        // If the key is CDS
        if (NStr::EqualNocase(key, "CDS")) {
            *tlabel += "[CDS]";
        // else if the key is repeat_unit or repeat_region
        } else if (NStr::EqualNocase(key, "repeat_unit")  ||
                   NStr::EqualNocase(key, "repeat_region")) {
            if (feat.IsSetQual()) {
                // Loop thru the feature qualifiers
                ITERATE( CSeq_feat::TQual, it, feat.GetQual()) {
                    // If qualifier qual is rpt_family append qualifier val
                    if (NStr::EqualNocase((*it)->GetQual(),"rpt_family")) { 
                        *tlabel += (*it)->GetVal();
                        empty = false;
                        break;
                    }
                }
            }
            
            // If nothing has been appended yet
            if (empty) {
                *tlabel += type_label ? *type_label : string("");
            }
        // else if the key is STS
        } else if (NStr::EqualNocase(key, "STS")) {
            if (feat.IsSetQual()) {
                ITERATE( CSeq_feat::TQual, it, feat.GetQual()) {
                    if (NStr::EqualNocase((*it)->GetQual(),"standard_name"))
                    { 
                           *tlabel = (*it)->GetVal();
                           empty = false;
                           break;
                    }
                }
            }
            
            // If nothing has been appended yet
            if (empty) {
                if ((flags & fFGL_NoComments) == 0  &&  feat.IsSetComment()) {
                    size_t pos = feat.GetComment().find(";");
                    if (pos == string::npos) {
                        *tlabel += feat.GetComment();
                    } else {
                        *tlabel += feat.GetComment().substr(0, pos);
                    } 
                } else {
                    *tlabel += type_label ? *type_label : string("");
                }
            }
        // else if the key is misc_feature
        } else if (NStr::EqualNocase(key, "misc_feature")) {
            if (feat.IsSetQual()) {
                // Look for a single qualifier qual in order of preference 
                // "standard_name", "function", "number", any and
                // append to tlabel and return if found
                ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
                    if (NStr::EqualNocase((*it)->GetQual(),"standard_name")) {
                        *tlabel += (*it)->GetVal();
                        return false;
                    }
                }
                ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
                    if (NStr::EqualNocase((*it)->GetQual(), "function")) {
                        *tlabel += (*it)->GetVal();
                        return false;
                    }
                }
                ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
                    if (NStr::EqualNocase((*it)->GetQual(), "number")) {
                        *tlabel += (*it)->GetVal();
                        return false;
                    }
                }
                ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
                    *tlabel += (*it)->GetVal();
                    return false;
                }
                // Append type_label if there is one
                if (empty) {
                    *tlabel += type_label ? *type_label : string("");
                    return false;
                }
            }
        }
    } 
    return false;                
}

 
// Return a label based on the content of the feature
void s_GetContentLabel
(const CSeq_feat&      feat,
 string*               label,
 const string*         type_label,
 TFeatLabelFlags       flags,
 CScope*               scope)
{
    string tlabel;
    
    // Get a content label dependent on the type of the feature data
    switch (feat.GetData().Which()) {
    case CSeqFeatData::e_Gene:
        feat.GetData().GetGene().GetLabel(&tlabel);
        break;
    case CSeqFeatData::e_Org:
        feat.GetData().GetOrg().GetLabel(&tlabel);
        break;
    case CSeqFeatData::e_Cdregion:
        s_GetCdregionLabel(feat, &tlabel, scope);
        break;
    case CSeqFeatData::e_Prot:
        feat.GetData().GetProt().GetLabel(&tlabel);
        break;
    case CSeqFeatData::e_Rna:
        s_GetRnaRefLabel(feat, &tlabel, flags, type_label);
        break;  
    case CSeqFeatData::e_Pub:
        feat.GetData().GetPub().GetPub().GetLabel(&tlabel); 
        break;
    case CSeqFeatData::e_Seq:
        break;
    case CSeqFeatData::e_Imp:
        if (s_GetImpLabel(feat, &tlabel, flags, type_label)) {
            *label += tlabel;
            return;
        }
        break;
    case CSeqFeatData::e_Region:
        if (feat.GetData().GetRegion().find("Domain") != string::npos  && 
            (flags & fFGL_NoComments) == 0  &&  feat.IsSetComment()) {
            tlabel += feat.GetComment();
        } else {
            tlabel += feat.GetData().GetRegion();
        }
        break;
    case CSeqFeatData::e_Comment:
        if ((flags & fFGL_NoComments) == 0  &&  feat.IsSetComment()) {
            tlabel += feat.GetComment();
        }
        break;
    case CSeqFeatData::e_Bond:
        // Get the ASN string name for the enumerated EBond type
        tlabel += CSeqFeatData::GetTypeInfo_enum_EBond()
            ->FindName(feat.GetData().GetBond(), true);
        break;
    case CSeqFeatData::e_Site:
        // Get the ASN string name for the enumerated ESite type
        tlabel += CSeqFeatData::GetTypeInfo_enum_ESite()
            ->FindName(feat.GetData().GetSite(), true);
        break;
    case CSeqFeatData::e_Rsite:
        switch (feat.GetData().GetRsite().Which()) {
        case CRsite_ref::e_Str:
            tlabel += feat.GetData().GetRsite().GetStr();
            break;
        case CRsite_ref::e_Db:
            tlabel += feat.GetData().GetRsite().GetDb().GetTag().IsStr() ?
                feat.GetData().GetRsite().GetDb().GetTag().GetStr() : 
                string("?");
            break;
        default:
            break;
        }
        break;
    case CSeqFeatData::e_User:
        if (feat.GetData().GetUser().IsSetClass()) {
            tlabel += feat.GetData().GetUser().GetClass();
        } else if (feat.GetData().GetUser().GetType().IsStr()) {
            tlabel += feat.GetData().GetUser().GetType().GetStr();
        }
    case CSeqFeatData::e_Txinit:
        break;
    case CSeqFeatData::e_Num:
        break;
    case CSeqFeatData::e_Psec_str:
        tlabel += CSeqFeatData::GetTypeInfo_enum_EPsec_str()
            ->FindName(feat.GetData().GetPsec_str(), true);
        break;    
    case CSeqFeatData::e_Non_std_residue:
        tlabel += feat.GetData().GetNon_std_residue();
        break;
    case CSeqFeatData::e_Het:
        tlabel += feat.GetData().GetHet().Get();
        break;        
    case CSeqFeatData::e_Biosrc:
        {{
            const CBioSource& biosrc = feat.GetData().GetBiosrc();
            string str;
            if (biosrc.IsSetSubtype()) {
                ITERATE (CBioSource::TSubtype, iter, biosrc.GetSubtype()) {
                    if ( !str.empty() ) {
                        str += "; ";
                    }
                    (*iter)->GetLabel(&str);
                }
            }
            if (str.empty()) {
                feat.GetData().GetBiosrc().GetOrg().GetLabel(&str);
            } else {
                str += " (";
                feat.GetData().GetBiosrc().GetOrg().GetLabel(&str);
                str += ")";
            }
            tlabel += str;
        }}
        break;        
    default:
        break;
    }
    
    // Return if a label has been calculated above
    if (!tlabel.empty()) {
        *label += tlabel;
        return;
    }
    
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
    if ((flags & fFGL_NoComments) == 0  &&  feat.IsSetComment()) {
        if (tlabel.empty()) {
            tlabel = feat.GetComment();
        } else {
            tlabel += "; " + feat.GetComment();
        }
    }
    
    *label += tlabel;
}


void GetLabel
(const CSeq_feat&    feat,
 string*             label,
 TFeatLabelFlags     flags,
 CScope*             scope)
{
 
    // Ensure that label exists
    if (!label) {
        return;
    }
    
    // Get the type label
    string type_label;
    s_GetTypeLabel(feat, &type_label, flags);
    
    // Append the type label and return if content label not required
    if ((flags & fFGL_Type) != 0) {
        *label += type_label;
        if ((flags & fFGL_Content) != 0) {
            *label += ": ";
        } else {
            return;
        }
    }
    
    // Append the content label
    size_t label_len = label->size();
    s_GetContentLabel(feat, label, &type_label, flags, scope);
    
    // If there is no content label, append the type label
    if (label->size() == label_len  &&  (flags & fFGL_Type) == 0) {
        *label += type_label;
    }
}


void GetLabel (const CSeq_feat&    feat, 
               string*             label, 
               ELabelType          label_type,
               CScope*             scope)
{
    TFeatLabelFlags flags = 0;
    switch (label_type) {
    case eType:    flags = fFGL_Type;     break;
    case eContent: flags = fFGL_Content;  break;
    case eBoth:    flags = fFGL_Both;     break;
    }
    GetLabel(feat, label, flags, scope);
}


void CFeatIdRemapper::Reset(void)
{
    m_IdMap.clear();
}


size_t CFeatIdRemapper::GetFeatIdsCount(void) const
{
    return m_IdMap.size();
}


int CFeatIdRemapper::RemapId(int old_id, const CTSE_Handle& tse)
{
    TFullId key(old_id, tse);
    int& new_id = m_IdMap[key];
    if ( !new_id ) {
        new_id = m_IdMap.size();
    }
    return new_id;
}


bool CFeatIdRemapper::RemapId(CFeat_id& id, const CTSE_Handle& tse)
{
    bool mapped = false;
    if ( id.IsLocal() ) {
        CObject_id& local = id.SetLocal();
        if ( local.IsId() ) {
            int old_id = local.GetId();
            int new_id = RemapId(old_id, tse);
            if ( new_id != old_id ) {
                mapped = true;
                local.SetId(new_id);
            }
        }
    }
    return mapped;
}


bool CFeatIdRemapper::RemapId(CFeat_id& id, const CFeat_CI& feat_it)
{
    bool mapped = false;
    if ( id.IsLocal() ) {
        CObject_id& local = id.SetLocal();
        if ( local.IsId() ) {
            int old_id = local.GetId();
            int new_id = RemapId(old_id, feat_it.GetAnnot().GetTSE_Handle());
            if ( new_id != old_id ) {
                mapped = true;
                local.SetId(new_id);
            }
        }
    }
    return mapped;
}


bool CFeatIdRemapper::RemapIds(CSeq_feat& feat, const CTSE_Handle& tse)
{
    bool mapped = false;
    if ( feat.IsSetId() ) {
        if ( RemapId(feat.SetId(), tse) ) {
            mapped = true;
        }
    }
    if ( feat.IsSetXref() ) {
        NON_CONST_ITERATE ( CSeq_feat::TXref, it, feat.SetXref() ) {
            CSeqFeatXref& xref = **it;
            if ( xref.IsSetId() && RemapId(xref.SetId(), tse) ) {
                mapped = true;
            }
        }
    }
    return mapped;
}


CRef<CSeq_feat> CFeatIdRemapper::RemapIds(const CFeat_CI& feat_it)
{
    CRef<CSeq_feat> feat(SerialClone(feat_it->GetMappedFeature()));
    if ( feat->IsSetId() ) {
        RemapId(feat->SetId(), feat_it);
    }
    if ( feat->IsSetXref() ) {
        NON_CONST_ITERATE ( CSeq_feat::TXref, it, feat->SetXref() ) {
            CSeqFeatXref& xref = **it;
            if ( xref.IsSetId() ) {
                RemapId(xref.SetId(), feat_it);
            }
        }
    }
    return feat;
}


bool CFeatComparatorByLabel::Less(const CSeq_feat& f1,
                                  const CSeq_feat& f2,
                                  CScope* scope)
{
    string l1, l2;
    GetLabel(f1, &l1, fFGL_Both, scope);
    GetLabel(f2, &l2, fFGL_Both, scope);
    return l1 < l2;
}


CMappedFeat MapSeq_feat(const CSeq_feat_Handle& feat,
                        const CBioseq_Handle& master_seq,
                        const CRange<TSeqPos>& range)
{
    SAnnotSelector sel(feat.GetFeatSubtype());
    sel.SetExactDepth();
    sel.SetResolveAll();
    CSeq_annot_Handle annot = feat.GetAnnot();
    sel.SetLimitSeqAnnot(annot);
    sel.SetSourceLoc(feat.GetOriginalSeq_feat()->GetLocation());
    for ( size_t depth = 0; depth < 10; ++depth ) {
        sel.SetResolveDepth(depth);
        for ( CFeat_CI it(master_seq, range, sel); it; ++it ) {
            if ( it->GetSeq_feat_Handle() == feat ) {
                return *it;
            }
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "MapSeq_feat: feature not found");
}


NCBI_XOBJUTIL_EXPORT
CMappedFeat MapSeq_feat(const CSeq_feat_Handle& feat,
                        const CSeq_id_Handle& master_id,
                        const CRange<TSeqPos>& range)
{
    CBioseq_Handle master_seq = feat.GetScope().GetBioseqHandle(master_id);
    if ( !master_seq ) {
        NCBI_THROW(CObjmgrUtilException, eBadLocation,
                   "MapSeq_feat: master sequence not found");
    }
    return MapSeq_feat(feat, master_seq, range);
}


NCBI_XOBJUTIL_EXPORT
CMappedFeat MapSeq_feat(const CSeq_feat_Handle& feat,
                        const CBioseq_Handle& master_seq)
{
    return MapSeq_feat(feat, master_seq, CRange<TSeqPos>::GetWhole());
}


NCBI_XOBJUTIL_EXPORT
CMappedFeat MapSeq_feat(const CSeq_feat_Handle& feat,
                        const CSeq_id_Handle& master_id)
{
    CBioseq_Handle master_seq = feat.GetScope().GetBioseqHandle(master_id);
    if ( !master_seq ) {
        NCBI_THROW(CObjmgrUtilException, eBadLocation,
                   "MapSeq_feat: master sequence not found");
    }
    return MapSeq_feat(feat, master_seq);
}


struct STypeLink
{
    STypeLink(CSeqFeatData::ESubtype subtype);
        
    bool IsValid(void) const {
        return m_ParentType != CSeqFeatData::eSubtype_bad;
    }

    void Next(void);
        
    CSeqFeatData::ESubtype m_CurrentType;
    CSeqFeatData::ESubtype m_ParentType;
    bool                   m_ByProduct;
};


STypeLink::STypeLink(CSeqFeatData::ESubtype subtype)
    : m_CurrentType(subtype),
      m_ParentType(CSeqFeatData::eSubtype_bad),
      m_ByProduct(false)
{
    switch ( subtype ) {
    case CSeqFeatData::eSubtype_max:
    case CSeqFeatData::eSubtype_bad:
        // artificial subtypes
        m_ParentType = CSeqFeatData::eSubtype_bad;
        break;
    case CSeqFeatData::eSubtype_operon:
    case CSeqFeatData::eSubtype_gap:
        // operon and gap features do not inherit anything
        m_ParentType = CSeqFeatData::eSubtype_bad;
        break;
    case CSeqFeatData::eSubtype_gene:
        // Gene features can inherit operon by overlap (CONTAINED_WITHIN)
        m_ParentType = CSeqFeatData::eSubtype_operon;
        break;
    case CSeqFeatData::eSubtype_mat_peptide:
    case CSeqFeatData::eSubtype_sig_peptide:
        m_ParentType = CSeqFeatData::eSubtype_prot;
        break;
    case CSeqFeatData::eSubtype_cdregion:
        m_ParentType = CSeqFeatData::eSubtype_mRNA;
        break;
    case CSeqFeatData::eSubtype_prot:
        m_ByProduct = true;
        m_ParentType = CSeqFeatData::eSubtype_cdregion;
        break;
    default:
        m_ParentType = CSeqFeatData::eSubtype_gene;
        break;
    }
}


void STypeLink::Next(void)
{
    if ( m_CurrentType == CSeqFeatData::eSubtype_prot ) {
        // no way to link proteins without cdregion
        m_ParentType = CSeqFeatData::eSubtype_bad;
        return;
    }
    switch ( m_ParentType ) {
    case CSeqFeatData::eSubtype_gene:
        // no inherit of operons if no gene
        m_ParentType = CSeqFeatData::eSubtype_bad;
        break;
    case CSeqFeatData::eSubtype_mRNA:
        if ( m_ByProduct ) {
            m_ByProduct = false;
            m_ParentType = CSeqFeatData::eSubtype_gene;
        }
        else {
            m_ByProduct = true;
        }
        break;
    default:
        *this = STypeLink(m_ParentType);
        break;
    }
}


namespace {
    // Checks if the location has mixed strands or wrong order of intervals
    static
    bool sx_IsIrregularLocation(const CSeq_loc& loc)
    {
        try {
            // simple locations are regular
            if ( !loc.IsMix() ) {
                return false;
            }
            
            if ( !loc.GetId() ) {
                // multiple ids locations are irregular
                return true;
            }
            
            ENa_strand strand = loc.GetStrand();
            if ( strand == eNa_strand_other ) {
                // mixed strands
                return true;
            }

            bool plus_strand = !IsReverse(strand);
            TSeqPos pos = plus_strand? 0: kInvalidSeqPos;
            bool stop = false;
            
            const CSeq_loc_mix& mix = loc.GetMix();
            ITERATE ( CSeq_loc_mix::Tdata, it, mix.Get() ) {
                const CSeq_loc& loc1 = **it;
                if ( sx_IsIrregularLocation(loc1) ) {
                    return true;
                }
                CRange<TSeqPos> range = loc1.GetTotalRange();
                if ( range.Empty() ) {
                    continue;
                }
                if ( stop ) {
                    return true;
                }
                if ( plus_strand ) {
                    if ( range.GetFrom() < pos ) {
                        return true;
                    }
                    pos = range.GetTo()+1;
                    stop = pos == 0;
                }
                else {
                    if ( range.GetTo() > pos ) {
                        return true;
                    }
                    pos = range.GetFrom();
                    stop = pos == 0;
                    --pos;
                }
            }
            
            return false;
        }
        catch ( CException& ) {
            // something's wrong -> irregular
            return true;
        }
    }


    static inline
    bool sx_CanHaveTranscriptId(CSeqFeatData::ESubtype type)
    {
        return
            type == CSeqFeatData::eSubtype_mRNA ||
            type == CSeqFeatData::eSubtype_cdregion;
    }


    static inline
    EOverlapType sx_GetOverlapType(const STypeLink& link,
                                   const CSeq_loc& loc)
    {
        EOverlapType overlap_type = eOverlap_Contained;
        if ( link.m_CurrentType == CSeqFeatData::eSubtype_cdregion ) {
            overlap_type = eOverlap_CheckIntervals;
        }
        if ( link.m_ParentType == CSeqFeatData::eSubtype_gene &&
             sx_IsIrregularLocation(loc) ) {
            // LOCATION_SUBSET if bad order or mixed strand
            // otherwise CONTAINED_WITHIN
            overlap_type = eOverlap_Subset;
        }
        return overlap_type;
    }


    static
    int sx_GetRootDistance(CSeqFeatData::ESubtype type)
    {
        int distance = 0;
        while ( type != CSeqFeatData::eSubtype_bad ) {
            ++distance;
            type = STypeLink(type).m_ParentType;
        }
        return distance;
    }


    static const int kSameTypeParentQuality = 1000;
    static const int kWorseTypeParentQuality = 500;

    static
    int sx_GetParentTypeQuality(CSeqFeatData::ESubtype parent,
                                CSeqFeatData::ESubtype child)
    {
        int d_child = sx_GetRootDistance(child);
        int d_parent = sx_GetRootDistance(parent);
        if ( d_parent <= d_child ) {
            return kSameTypeParentQuality - (d_child - d_parent);
        }
        else {
            return kWorseTypeParentQuality + (d_child - d_parent);
        }
    }


    static
    CMappedFeat sx_GetParentByRef(const CMappedFeat& feat,
                                  const STypeLink& link)
    {
        if ( !feat.IsSetXref() ) {
            return CMappedFeat();
        }

        CTSE_Handle tse = feat.GetAnnot().GetTSE_Handle();
        const CSeq_feat::TXref& xrefs = feat.GetXref();
        ITERATE ( CSeq_feat::TXref, it, xrefs ) {
            const CSeqFeatXref& xref = **it;
            if ( xref.IsSetId() ) {
                const CFeat_id& id = xref.GetId();
                if ( id.IsLocal() ) {
                    CSeq_feat_Handle feat1 =
                        tse.GetFeatureWithId(link.m_ParentType, id.GetLocal());
                    if ( feat1 ) {
                        return feat1;
                    }
                }
            }
            if ( link.m_ParentType == CSeqFeatData::eSubtype_gene &&
                 xref.IsSetData() ) {
                const CSeqFeatData& data = xref.GetData();
                if ( data.IsGene() ) {
                    CSeq_feat_Handle feat1 = tse.GetGeneByRef(data.GetGene());
                    if ( feat1 ) {
                        return feat1;
                    }
                }
            }
        }
        return CMappedFeat();
    }


    static
    CMappedFeat sx_GetParentByOverlap(const CMappedFeat& feat,
                                      const STypeLink& link)
    {
        CMappedFeat best_parent;

        const CSeq_loc& c_loc = feat.GetLocation();

        // find best suitable parent by overlap score
        EOverlapType overlap_type = sx_GetOverlapType(link, c_loc);
    
        Int8 best_overlap = kMax_I8;
        SAnnotSelector sel(link.m_ParentType);
        sel.SetByProduct(link.m_ByProduct);
        for (CFeat_CI it(feat.GetScope(), c_loc, sel); it; ++it) {
            Int8 overlap = TestForOverlap64(it->GetLocation(),
                                            c_loc,
                                            overlap_type,
                                            kInvalidSeqPos,
                                            &feat.GetScope());
            if ( overlap >= 0 && overlap < best_overlap ) {
                best_parent = *it;
                best_overlap = overlap;
            }
        }
        return best_parent;
    }
}


/// @name GetParentFeature
/// The algorithm is the following:
/// 1. Feature types are organized in a tree of possible 
///   parent-child relationship:
///   1.1. operon, gap cannot have a parent,
///   1.2. gene can have operon as a parent,
///   1.3. mRNA can have gene as a parent,
///   1.4. cdregion can have mRNA as a parent,
///   1.5. prot can have cdregion as a parent (by its product location),
///   1.6. mat_peptide, sig_peptide can have prot as a parent,
///   1.x. all other feature types can have gene as a parent.
/// 2. If parent of a nearest feature type is not found then the next type
///   in the tree is checked, except prot which will have no parent
///   if no cdregion is found.
/// 3. For each parent type candidate the search is done in several ways:
///   3.1. first we look for a parent by Seq-feat.xref field,
///   3.2. then by Gene-ref if current parent type is gene,
///   3.3. then parent candidates are searched by the best intersection
///        of their locations (product in case of prot -> cdregion link),
///   3.4. if no candidates are found next parent type is checked.
NCBI_XOBJUTIL_EXPORT
CMappedFeat GetParentFeature(const CMappedFeat& feat)
{
    CMappedFeat best_parent;

    STypeLink link(feat.GetFeatSubtype());
    while ( link.IsValid() ) {
        best_parent = sx_GetParentByRef(feat, link);
        if ( best_parent ) {
            // found by Xref
            break;
        }

        best_parent = sx_GetParentByOverlap(feat, link);
        if ( best_parent ) {
            // parent is found by overlap
            break;
        }

        link.Next();
    }
    return best_parent;
}


CFeatTree::CFeatTree(void)
    : m_AssignedParents(0),
      m_FeatIdMode(eFeatId_by_type)
{
}


CFeatTree::CFeatTree(CFeat_CI it)
    : m_AssignedParents(0),
      m_FeatIdMode(eFeatId_by_type)
{
    AddFeatures(it);
}


CFeatTree::~CFeatTree(void)
{
}


void CFeatTree::SetFeatIdMode(EFeatIdMode mode)
{
    m_FeatIdMode = mode;
}


void CFeatTree::AddFeatures(CFeat_CI it)
{
    for ( ; it; ++it ) {
        AddFeature(*it);
    }
}


void CFeatTree::AddFeature(const CMappedFeat& feat)
{
    CFeatInfo& info = m_InfoMap[feat.GetSeq_feat_Handle()];
    info.m_Feat = feat;
    if ( feat.IsSetQual() && sx_CanHaveTranscriptId(feat.GetFeatSubtype()) ) {
        ITERATE ( CSeq_feat::TQual, it, feat.GetQual() ) {
            if ( (*it)->GetQual() == "transcript_id" && (*it)->IsSetVal() ) {
                info.m_TranscriptId = &(*it)->GetVal();
                break;
            }
        }
    }
}


CFeatTree::CFeatInfo& CFeatTree::x_GetInfo(const CMappedFeat& feat)
{
    return x_GetInfo(feat.GetSeq_feat_Handle());
}


CFeatTree::CFeatInfo& CFeatTree::x_GetInfo(const CSeq_feat_Handle& feat)
{
    TInfoMap::iterator it = m_InfoMap.find(feat);
    if ( it == m_InfoMap.end() ) {
        NCBI_THROW(CObjMgrException, eFindFailed,
                   "CFeatTree: feature not found");
    }
    return it->second;
}


CFeatTree::CFeatInfo* CFeatTree::x_FindInfo(const CSeq_feat_Handle& feat)
{
    TInfoMap::iterator it = m_InfoMap.find(feat);
    if ( it == m_InfoMap.end() ) {
        return 0;
    }
    return &it->second;
}


namespace {
    struct SFeatRangeInfo {
        CSeq_id_Handle m_Id;
        CRange<TSeqPos> m_Range;
        CFeatTree::CFeatInfo* m_Info;
        
        // min start coordinate for all entries after this
        TSeqPos m_MinFrom;

        SFeatRangeInfo(CFeatTree::CFeatInfo& info, bool by_product = false)
            : m_Info(&info)
            {
                if ( by_product ) {
                    m_Id = info.m_Feat.GetProductId();
                    if ( m_Id ) {
                        m_Range = info.m_Feat.GetProductTotalRange();
                    }
                }
                else {
                    m_Id = info.m_Feat.GetLocationId();
                    if ( m_Id ) {
                        m_Range = info.m_Feat.GetLocationTotalRange();
                    }
                }
            }
        SFeatRangeInfo(CFeatTree::CFeatInfo& info,
                       CHandleRangeMap::const_iterator it)
            : m_Id(it->first),
              m_Range(it->second.GetOverlappingRange()),
              m_Info(&info)
            {
            }
    };
    struct PLessByStart {
        // sort first by start coordinate, then by end coordinate
        bool operator()(const SFeatRangeInfo& a, const SFeatRangeInfo& b) const
            {
                return a.m_Id < b.m_Id || a.m_Id == b.m_Id &&
                    a.m_Range < b.m_Range;
            }
    };
    struct PLessByEnd {
        // sort first by end coordinate, then by start coordinate
        bool operator()(const SFeatRangeInfo& a, const SFeatRangeInfo& b) const
            {
                return a.m_Id < b.m_Id || a.m_Id == b.m_Id &&
                    (a.m_Range.GetToOpen() < b.m_Range.GetToOpen() ||
                     a.m_Range.GetToOpen() == b.m_Range.GetToOpen() &&
                     a.m_Range.GetFrom() < b.m_Range.GetFrom());
            }
    };

    void s_AddRanges(vector<SFeatRangeInfo>& rr,
                     CFeatTree::CFeatInfo& info,
                     const CSeq_loc& loc)
    {
        CHandleRangeMap hrmap;
        hrmap.AddLocation(loc);
        ITERATE ( CHandleRangeMap, it, hrmap ) {
            SFeatRangeInfo range_info(info, it);
            rr.push_back(range_info);
        }
    }
}


void CFeatTree::x_CollectNeeded(TParentInfoMap& pinfo_map)
{
    // collect all necessary parent candidates
    NON_CONST_ITERATE ( TInfoMap, it, m_InfoMap ) {
        CFeatInfo& info = it->second;
        CSeqFeatData::ESubtype feat_type = info.m_Feat.GetFeatSubtype();
        SFeatSet& feat_set = pinfo_map[feat_type];
        if ( feat_set.m_NeedAll && !feat_set.m_CollectedAll ) {
            feat_set.m_All.push_back(&info);
        }
    }
    // mark collected 
    NON_CONST_ITERATE ( TParentInfoMap, it, pinfo_map ) {
        SFeatSet& feat_set = *it;
        if ( feat_set.m_NeedAll && !feat_set.m_CollectedAll ) {
            feat_set.m_CollectedAll = true;
        }
    }
}


pair<int, CFeatTree::CFeatInfo*>
CFeatTree::x_LookupParentByRef(CFeatInfo& info,
                               CSeqFeatData::ESubtype parent_type)
{
    pair<int, CFeatInfo*> ret(0, 0);
    if ( !info.m_Feat.IsSetXref() ) {
        return ret;
    }
    CTSE_Handle tse = info.GetTSE();
    const CSeq_feat::TXref& xrefs = info.m_Feat.GetXref();
    ITERATE ( CSeq_feat::TXref, xit, xrefs ) {
        const CSeqFeatXref& xref = **xit;
        if ( !xref.IsSetId() ) {
            continue;
        }
        const CFeat_id& id = xref.GetId();
        if ( !id.IsLocal() ) {
            continue;
        }
        vector<CSeq_feat_Handle> ff =
            tse.GetFeaturesWithId(parent_type, id.GetLocal());
        ITERATE ( vector<CSeq_feat_Handle>, fit, ff ) {
            CFeatInfo* parent = x_FindInfo(*fit);
            if ( !parent ) {
                continue;
            }
            int quality =
                sx_GetParentTypeQuality(parent->m_Feat.GetFeatSubtype(),
                                        info.m_Feat.GetFeatSubtype());
            if ( quality > ret.first ) {
                ret.first = quality;
                ret.second = parent;
            }
        }
    }
    if ( ret.second ) {
        return ret;
    }
    if ( parent_type == CSeqFeatData::eSubtype_gene ||
         parent_type == CSeqFeatData::eSubtype_any ) {
        ITERATE ( CSeq_feat::TXref, xit, xrefs ) {
            const CSeqFeatXref& xref = **xit;
            if ( xref.IsSetData() ) {
                const CSeqFeatData& data = xref.GetData();
                if ( data.IsGene() ) {
                    vector<CSeq_feat_Handle> ff =
                        tse.GetGenesByRef(data.GetGene());
                    ITERATE ( vector<CSeq_feat_Handle>, fit, ff ) {
                        CFeatInfo* gene = x_FindInfo(*fit);
                        if ( gene ) {
                            ret.first = 1;
                            ret.second = gene;
                            return ret;
                        }
                    }
                }
            }
        }
    }
    return ret;
}


bool CFeatTree::x_AssignParentByRef(CFeatInfo& info)
{
    pair<int, CFeatInfo*> parent =
        x_LookupParentByRef(info, CSeqFeatData::eSubtype_any);
    if ( !parent.second ) {
        return false;
    }
    if ( parent.second->IsSetParent() && parent.second->m_Parent == &info ) {
        // circular reference, keep existing parent
        return false;
    }
    pair<int, CFeatInfo*> grand_parent =
        x_LookupParentByRef(*parent.second, CSeqFeatData::eSubtype_any);
    if ( grand_parent.second == &info ) {
        // new circular reference, choose by quality
        if ( parent.first < grand_parent.first ) {
            return false;
        }
    }
    x_SetParent(info, *parent.second);
    return true;
}


void CFeatTree::x_AssignParentsByRef(TFeatArray& features,
                                     const STypeLink& link)
{
    if ( features.empty() || !link.IsValid() ) {
        return;
    }
    TFeatArray::iterator dst = features.begin();
    ITERATE ( TFeatArray, it, features ) {
        CFeatInfo& info = **it;
        if ( info.IsSetParent() ) {
            continue;
        }
        CFeatInfo* parent =
            x_LookupParentByRef(info, link.m_ParentType).second;
        if ( parent ) {
            x_SetParent(info, *parent);
        }
        else {
            *dst++ = *it;
        }
    }
    features.erase(dst, features.end());
}


void CFeatTree::x_AssignParentsByOverlap(TFeatArray& features,
                                         const STypeLink& link,
                                         TFeatArray& parents)
{
    if ( features.empty() || parents.empty() ) {
        return;
    }
    
    typedef vector<SFeatRangeInfo> TRangeArray;
    TRangeArray pp, cc;

    // collect parents parameters
    ITERATE ( TFeatArray, it, parents ) {
        CFeatInfo& feat_info = **it;
        if ( link.m_ByProduct && !feat_info.m_Feat.IsSetProduct() ) {
            continue;
        }
        SFeatRangeInfo range_info(feat_info, link.m_ByProduct);
        if ( range_info.m_Id ) {
            pp.push_back(range_info);
        }
        else {
            s_AddRanges(pp, feat_info,
                        link.m_ByProduct?
                        feat_info.m_Feat.GetProduct():
                        feat_info.m_Feat.GetLocation());
        }
    }
    sort(pp.begin(), pp.end(), PLessByEnd());

    // collect children parameters
    ITERATE ( TFeatArray, it, features ) {
        CFeatInfo& feat_info = **it;
        SFeatRangeInfo range_info(feat_info);
        if ( range_info.m_Id ) {
            cc.push_back(range_info);
        }
        else {
            s_AddRanges(cc, feat_info, feat_info.m_Feat.GetLocation());
        }
    }
    sort(cc.begin(), cc.end(), PLessByStart());

    // assign parents in single scan over both lists
    {{
        TRangeArray::iterator pi = pp.begin();
        TRangeArray::iterator ci = cc.begin();
        for ( ; ci != cc.end(); ) {
            // skip all parents with Seq-ids smaller than first child
            while ( pi != pp.end() && pi->m_Id < ci->m_Id ) {
                ++pi;
            }
            if ( pi == pp.end() ) { // no more parents
                break;
            }
            const CSeq_id_Handle& cur_id = pi->m_Id;
            if ( ci->m_Id < cur_id || !ci->m_Id ) {
                // skip all children with Seq-ids smaller than first parent
                do {
                    ++ci;
                } while ( ci != cc.end() && (ci->m_Id < cur_id || !ci->m_Id) );
                continue;
            }

            // find end of Seq-id parents
            TRangeArray::iterator pe = pi;
            while ( pe != pp.end() && pe->m_Id == cur_id ) {
                ++pe;
            }

            {{
                // update parents' m_MinFrom on the Seq-id
                TRangeArray::iterator i = pe;
                TSeqPos min_from = (--i)->m_Range.GetFrom();
                i->m_MinFrom = min_from;
                while ( i != pi ) {
                    min_from = min(min_from, (--i)->m_Range.GetFrom());
                    i->m_MinFrom = min_from;
                }
            }}

            // scan all Seq-id children
            for ( ; ci != cc.end() && pi != pe && ci->m_Id == cur_id; ++ci ) {
                // child parameters
                CFeatInfo& info = *ci->m_Info;
                const CSeq_loc& c_loc = info.m_Feat.GetLocation();
                EOverlapType overlap_type = sx_GetOverlapType(link, c_loc);

                // skip non-overlapping parents
                while ( pi != pe &&
                        pi->m_Range.GetToOpen() < ci->m_Range.GetFrom() ) {
                    ++pi;
                }
            
                // scan parent candidates
                for ( TRangeArray::iterator pc = pi;
                      pc != pe && pc->m_MinFrom < ci->m_Range.GetToOpen();
                      ++pc ) {
                    if ( !pc->m_Range.IntersectingWith(ci->m_Range) ) {
                        continue;
                    }
                    const CMappedFeat& p_feat = pc->m_Info->m_Feat;
                    const CSeq_loc& p_loc =
                        link.m_ByProduct?
                        p_feat.GetProduct():
                        p_feat.GetLocation();
                    Uint1 quality = 0;
                    if ( info.m_TranscriptId && pc->m_Info->m_TranscriptId &&
                         *info.m_TranscriptId == *pc->m_Info->m_TranscriptId ){
                        quality = 1;
                    }
                    Int8 overlap = TestForOverlap64(p_loc,
                                                    c_loc,
                                                    overlap_type,
                                                    kInvalidSeqPos,
                                                    &p_feat.GetScope());
                    if ( overlap < 0 ) {
                        continue;
                    }
                    if ( quality > info.m_ParentQuality ||
                         (quality == info.m_ParentQuality &&
                          overlap < info.m_ParentOverlap) ) {
                        info.m_ParentQuality = quality;
                        info.m_ParentOverlap = overlap;
                        info.m_Parent = pc->m_Info;
                    }
                }
            }
            // skip remaining Seq-id children
            for ( ; ci != cc.end() && ci->m_Id == cur_id; ++ci ) {
            }
        }
    }}
    // assign found parents
    TFeatArray::iterator dst = features.begin();
    ITERATE ( TFeatArray, it, features ) {
        CFeatInfo& info = **it;
        if ( !info.IsSetParent() ) {
            if ( info.m_Parent && info.m_ParentOverlap < kMax_I8 ) {
                // assign best parent
                CFeatInfo* p_info = info.m_Parent;
                info.m_Parent = 0;
                x_SetParent(info, *p_info);
            }
            else {
                // store for future processing
                *dst++ = &info;
            }
        }
    }
    // remove all features with assigned parents
    features.erase(dst, features.end());
}


void CFeatTree::x_AssignParents(void)
{
    if ( m_AssignedParents >= m_InfoMap.size() ) {
        return;
    }
    
    // collect all features without assigned parent
    TParentInfoMap pinfo_map(CSeqFeatData::eSubtype_max+1);
    size_t new_count = 0;
    NON_CONST_ITERATE ( TInfoMap, it, m_InfoMap ) {
        CFeatInfo& info = it->second;
        if ( info.IsSetParent() ) {
            continue;
        }
        if ( m_FeatIdMode == eFeatId_always && x_AssignParentByRef(info) ) {
            continue;
        }
        CSeqFeatData::ESubtype feat_type = info.m_Feat.GetFeatSubtype();
        STypeLink link(feat_type);
        if ( !link.IsValid() ) {
            // no parent
            x_SetNoParent(info);
        }
        else {
            SFeatSet& feat_set = pinfo_map[feat_type];
            if ( feat_set.m_New.empty() ) {
                feat_set.m_FeatType = feat_type;
                pinfo_map[link.m_ParentType].m_NeedAll = true;
            }
            feat_set.m_New.push_back(&info);
            ++new_count;
        }
    }
    if ( new_count == 0 ) { // no work to do
        return;
    }
    // collect all necessary parent candidates
    x_CollectNeeded(pinfo_map);
    // assign parents for each parent type
    NON_CONST_ITERATE ( TParentInfoMap, it1, pinfo_map ) {
        SFeatSet& feat_set = *it1;
        if ( feat_set.m_New.empty() ) {
            // no work to do
            continue;
        }
        STypeLink link(feat_set.m_FeatType);
        while ( link.IsValid() ) {
            if ( m_FeatIdMode == eFeatId_by_type ) {
                x_AssignParentsByRef(feat_set.m_New, link);
                if ( feat_set.m_New.empty() ) {
                    break;
                }
            }
            if ( !pinfo_map[link.m_ParentType].m_CollectedAll ) {
                pinfo_map[link.m_ParentType].m_NeedAll = true;
                x_CollectNeeded(pinfo_map);
            }
            _ASSERT(pinfo_map[link.m_ParentType].m_CollectedAll);
            x_AssignParentsByOverlap(feat_set.m_New, link,
                                     pinfo_map[link.m_ParentType].m_All);
            if ( feat_set.m_New.empty() ) {
                break;
            }

            link.Next();
        }
        // all remaining features are without parent
        ITERATE ( TFeatArray, it, feat_set.m_New ) {
            x_SetNoParent(**it);
        }
    }

    if ( m_FeatIdMode == eFeatId_always ) {
        NON_CONST_ITERATE ( TInfoMap, it, m_InfoMap ) {
            x_VerifyLinkedToRoot(it->second);
        }
    }
    m_AssignedParents = m_InfoMap.size();
}


void CFeatTree::x_VerifyLinkedToRoot(CFeatInfo& info)
{
    _ASSERT(info.IsSetParent());
    if ( info.m_IsLinkedToRoot == info.eIsLinkedToRoot_linking ) {
        NcbiCout << MSerial_AsnText
                 << info.m_Feat.GetOriginalFeature()
                 << info.m_Parent->m_Feat.GetOriginalFeature()
                 << NcbiEndl;
        NCBI_THROW(CObjMgrException, eFindConflict,
                   "CFeatTree: cycle in xrefs to parent feature");
    }
    if ( info.m_Parent ) {
        info.m_IsLinkedToRoot = info.eIsLinkedToRoot_linking;
        x_VerifyLinkedToRoot(*info.m_Parent);
        info.m_IsLinkedToRoot = info.eIsLinkedToRoot_linked;
    }
    _ASSERT(info.m_IsLinkedToRoot == info.eIsLinkedToRoot_linked);
}


void CFeatTree::x_SetParent(CFeatInfo& info, CFeatInfo& parent)
{
    _ASSERT(!info.IsSetParent());
    _ASSERT(!info.m_Parent);
    _ASSERT(!parent.m_IsSetChildren);
    _ASSERT(parent.m_IsLinkedToRoot != info.eIsLinkedToRoot_linking);
    parent.m_Children.push_back(&info);
    info.m_Parent = &parent;
    info.m_IsSetParent = true;
    info.m_IsLinkedToRoot = parent.m_IsLinkedToRoot;
}


void CFeatTree::x_SetNoParent(CFeatInfo& info)
{
    _ASSERT(!info.IsSetParent());
    _ASSERT(!info.m_Parent);
    m_RootInfo.m_Children.push_back(&info);
    info.m_IsSetParent = true;
    info.m_IsLinkedToRoot = info.eIsLinkedToRoot_linked;
}


CFeatTree::CFeatInfo* CFeatTree::x_GetParent(CFeatInfo& info)
{
    if ( !info.IsSetParent() ) {
        x_AssignParents();
    }
    return info.m_Parent;
}


const CFeatTree::TChildren& CFeatTree::x_GetChildren(CFeatInfo& info)
{
    x_AssignParents();
    return info.m_Children;
}


CMappedFeat CFeatTree::GetParent(const CMappedFeat& feat)
{
    CMappedFeat ret;
    CFeatInfo* info = x_GetParent(x_GetInfo(feat));
    if ( info ) {
        ret = info->m_Feat;
    }
    return ret;
}


CMappedFeat CFeatTree::GetParent(const CMappedFeat& feat,
                                 CSeqFeatData::E_Choice type)
{
    CMappedFeat parent = GetParent(feat);
    while ( parent && parent.GetFeatType() != type ) {
        parent = GetParent(parent);
    }
    return parent;
}


CMappedFeat CFeatTree::GetParent(const CMappedFeat& feat,
                                 CSeqFeatData::ESubtype subtype)
{
    CMappedFeat parent = GetParent(feat);
    while ( parent && parent.GetFeatSubtype() != subtype ) {
        parent = GetParent(parent);
    }
    return parent;
}


vector<CMappedFeat> CFeatTree::GetChildren(const CMappedFeat& feat)
{
    vector<CMappedFeat> children;
    GetChildrenTo(feat, children);
    return children;
}


void CFeatTree::GetChildrenTo(const CMappedFeat& feat,
                              vector<CMappedFeat>& children)
{
    children.clear();
    const TChildren* infos;
    if ( feat ) {
        infos = &x_GetChildren(x_GetInfo(feat));
    }
    else {
        x_AssignParents();
        infos = &m_RootInfo.m_Children;
    }
    children.reserve(infos->size());
    ITERATE ( TChildren, it, *infos ) {
        children.push_back((*it)->m_Feat);
    }
}


CFeatTree::CFeatInfo::CFeatInfo(void)
    : m_TranscriptId(0),
      m_IsSetParent(false),
      m_IsSetChildren(false),
      m_IsLinkedToRoot(eIsLinkedToRoot_unknown),
      m_ParentQuality(0),
      m_Parent(0),
      m_ParentOverlap(kMax_I8)
{
}


CFeatTree::CFeatInfo::~CFeatInfo(void)
{
}


const CTSE_Handle& CFeatTree::CFeatInfo::GetTSE(void) const
{
    return m_Feat.GetAnnot().GetTSE_Handle();
}


void CFeatTree::AddFeaturesFor(const CMappedFeat& feat,
                               CSeqFeatData::ESubtype bottom_type,
                               CSeqFeatData::ESubtype top_type)
{
    AddFeature(feat);
    SAnnotSelector sel(bottom_type);
    if ( top_type != bottom_type ) {
        for ( STypeLink link(bottom_type); link.IsValid(); link.Next() ) {
            CSeqFeatData::ESubtype parent_type = link.m_ParentType;
            sel.IncludeFeatSubtype(parent_type);
            if ( parent_type == top_type ) {
                break;
            }
        }
    }
    sel.SetResolveAll().SetAdaptiveDepth();
    CFeat_CI feat_it(feat.GetScope(), feat.GetLocation(), sel);
    AddFeatures(feat_it);
}


void CFeatTree::AddFeaturesFor(CScope& scope, const CSeq_loc& loc,
                               CSeqFeatData::ESubtype bottom_type,
                               CSeqFeatData::ESubtype top_type,
                               const SAnnotSelector* base_sel)
{
    SAnnotSelector sel;
    if ( base_sel ) {
        sel = *base_sel;
    }
    else {
        sel.SetResolveAll().SetAdaptiveDepth();
    }
    sel.SetFeatSubtype(bottom_type);
    if ( top_type != bottom_type ) {
        for ( STypeLink link(bottom_type); link.IsValid(); link.Next() ) {
            CSeqFeatData::ESubtype parent_type = link.m_ParentType;
            sel.IncludeFeatSubtype(parent_type);
            if ( parent_type == top_type ) {
                break;
            }
        }
    }
    CFeat_CI feat_it(scope, loc, sel);
    AddFeatures(feat_it);
}


void CFeatTree::AddGenesForMrna(const CMappedFeat& mrna_feat)
{
    AddFeaturesFor(mrna_feat,
                   CSeqFeatData::eSubtype_gene,
                   CSeqFeatData::eSubtype_gene);
}


void CFeatTree::AddCdsForMrna(const CMappedFeat& mrna_feat)
{
    AddFeaturesFor(mrna_feat,
                   CSeqFeatData::eSubtype_cdregion,
                   CSeqFeatData::eSubtype_mRNA);
}


void CFeatTree::AddGenesForCds(const CMappedFeat& cds_feat)
{
    AddFeaturesFor(cds_feat,
                   CSeqFeatData::eSubtype_mRNA,
                   CSeqFeatData::eSubtype_gene);
}


void CFeatTree::AddMrnasForCds(const CMappedFeat& cds_feat)
{
    AddFeaturesFor(cds_feat,
                   CSeqFeatData::eSubtype_mRNA,
                   CSeqFeatData::eSubtype_mRNA);
}


void CFeatTree::AddMrnasForGene(const CMappedFeat& gene_feat)
{
    AddFeaturesFor(gene_feat,
                   CSeqFeatData::eSubtype_mRNA,
                   CSeqFeatData::eSubtype_gene);
}


void CFeatTree::AddCdsForGene(const CMappedFeat& gene_feat)
{
    AddFeaturesFor(gene_feat,
                   CSeqFeatData::eSubtype_cdregion,
                   CSeqFeatData::eSubtype_gene);
}


/////////////////////////////////////////////////////////////////////////////
// New API for GetBestXxxForXxx()

CMappedFeat
GetBestGeneForMrna(const CMappedFeat& mrna_feat,
                   CFeatTree* feat_tree)
{
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddGenesForMrna(mrna_feat);
        return GetBestGeneForMrna(mrna_feat, &tree);
    }
    return feat_tree->GetParent(mrna_feat, CSeqFeatData::eSubtype_gene);
}

CMappedFeat
GetBestGeneForCds(const CMappedFeat& cds_feat,
                  CFeatTree* feat_tree)
{
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddGenesForCds(cds_feat);
        return GetBestGeneForCds(cds_feat, &tree);
    }
    return feat_tree->GetParent(cds_feat, CSeqFeatData::eSubtype_gene);
}

CMappedFeat
GetBestMrnaForCds(const CMappedFeat& cds_feat,
                  CFeatTree* feat_tree)
{
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddMrnasForCds(cds_feat);
        return GetBestMrnaForCds(cds_feat, &tree);
    }
    return feat_tree->GetParent(cds_feat, CSeqFeatData::eSubtype_mRNA);
}

CMappedFeat
GetBestCdsForMrna(const CMappedFeat& mrna_feat,
                  CFeatTree* feat_tree)
{
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddCdsForMrna(mrna_feat);
        return GetBestCdsForMrna(mrna_feat, &tree);
    }
    const vector<CMappedFeat>& children = feat_tree->GetChildren(mrna_feat);
    ITERATE ( vector<CMappedFeat>, it, children ) {
        if ( it->GetFeatSubtype() == CSeqFeatData::eSubtype_cdregion ) {
            return *it;
        }
    }
    return CMappedFeat();
}

void GetMrnasForGene(const CMappedFeat& gene_feat,
                     list< CMappedFeat >& mrna_feats,
                     CFeatTree* feat_tree)
{
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddMrnasForGene(gene_feat);
        GetMrnasForGene(gene_feat, mrna_feats, &tree);
        return;
    }
    const vector<CMappedFeat>& children = feat_tree->GetChildren(gene_feat);
    ITERATE ( vector<CMappedFeat>, it, children ) {
        if ( it->GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA ) {
            mrna_feats.push_back(*it);
        }
    }
}

void GetCdssForGene(const CMappedFeat& gene_feat,
                    list< CMappedFeat >& cds_feats,
                    CFeatTree* feat_tree)
{
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddCdsForGene(gene_feat);
        GetCdssForGene(gene_feat, cds_feats, &tree);
        return;
    }
    const vector<CMappedFeat>& children = feat_tree->GetChildren(gene_feat);
    ITERATE ( vector<CMappedFeat>, it, children ) {
        if ( it->GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA ) {
            const vector<CMappedFeat>& children2 = feat_tree->GetChildren(*it);
            ITERATE ( vector<CMappedFeat>, it2, children2 ) {
                if ( it2->GetFeatSubtype()==CSeqFeatData::eSubtype_cdregion ) {
                    cds_feats.push_back(*it2);
                }
            }
        }
        else if ( it->GetFeatSubtype() == CSeqFeatData::eSubtype_cdregion ) {
            cds_feats.push_back(*it);
        }
    }
}

/*
static
CMappedFeat
sx_GetBestOverlappingFeat(const CMappedFeat& feat,
                          CSeqFeatData::E_Choice feat_type,
                          sequence::EOverlapType overlap_type)
{
    return CMappedFeat();
}

CMappedFeat
GetBestOverlappingFeat(const CMappedFeat& feat,
                       CSeqFeatData::E_Choice feat_type,
                       sequence::EOverlapType overlap_type)
{
    CMappedFeat ret;
    switch (feat_type) {
    case CSeqFeatData::e_Gene:
        ret = GetBestOverlappingFeat(feat,
                                     CSeqFeatData::eSubtype_gene,
                                     overlap_type);
        break;

    case CSeqFeatData::e_Rna:
        ret = GetBestOverlappingFeat(feat,
                                     CSeqFeatData::eSubtype_mRNA,
                                     overlap_type);
        break;

    case CSeqFeatData::e_Cdregion:
        ret = GetBestOverlappingFeat(feat,
                                     CSeqFeatData::eSubtype_cdregion,
                                     overlap_type);
        break;

    default:
        break;
    }

    if ( !ret ) {
        ret = sx_GetBestOverlappingFeat(feat, feat_type, overlap_type);
    }

    return ret;
}


CMappedFeat
GetBestOverlappingFeat(const CMappedFeat& feat,
                       CSeqFeatData::ESubtype subtype,
                       sequence::EOverlapType overlap_type)
{
    switch (feat.GetSubtype()) {
    case CSeqFeatData::eSubtype_mRNA:
        switch (subtype) {
        case CSeqFeatData::eSubtype_gene:
            return GetBestGeneForMrna(feat);

        case CSeqFeatData::eSubtype_cdregion:
            return GetBestCdsForMrna(feat);

        default:
            break;
        }
        break;

    case CSeqFeatData::eSubtype_cdregion:
        switch (subtype) {
        case CSeqFeatData::eSubtype_mRNA:
            return GetBestMrnaForCds(feat);

        case CSeqFeatData::eSubtype_gene:
            return GetBestGeneForCds(feat);

        default:
            break;
        }
        break;

    case CSeqFeatData::eSubtype_variation:
        return GetBestOverlapForSNP(feat, subtype);

    default:
        break;
    }

    return sx_GetBestOverlappingFeat(feat, subtype, overlap_type);
}
*/

END_SCOPE(feature)
END_SCOPE(objects)
END_NCBI_SCOPE
