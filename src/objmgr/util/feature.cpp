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

#include <algorithm>

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

                for (CFeat_CI feat_it(hnd,
                                      SAnnotSelector()
                                      .IncludeFeatType(CSeqFeatData::e_Prot));
                     feat_it;  ++feat_it) {
                    feat_it->GetData().GetProt().GetLabel(tlabel);
                    return;
                }
            }
            else {
                ERR_POST(Error << "cannot find sequence: " + id.AsFastaString());
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
        if ((flags & fFGL_Type) != 0  &&  type_label != NULL
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
            const CSeq_feat_Base::TQual & qual = feat.GetQual(); // must store reference since ITERATE macro evaluates 3rd arg multiple times
            ITERATE( CSeq_feat::TQual, q, qual ) {
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
                const CSeq_feat_Base::TQual & qual = feat.GetQual(); // must store reference since ITERATE macro evaluates 3rd arg multiple times
                ITERATE( CSeq_feat::TQual, it, qual ) {
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
                const CSeq_feat_Base::TQual & qual = feat.GetQual(); // must store reference since ITERATE macro evaluates 3rd arg multiple times
                ITERATE( CSeq_feat::TQual, it, qual ) {
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
                const CSeq_feat_Base::TQual & qual = feat.GetQual(); // must store reference since ITERATE macro evaluates 3rd arg multiple times
                ITERATE( CSeq_feat::TQual, it, qual ) {
                    if (NStr::EqualNocase((*it)->GetQual(),"standard_name")) {
                        *tlabel += (*it)->GetVal();
                        return false;
                    }
                }
                const CSeq_feat_Base::TQual & qual2 = feat.GetQual(); // must store reference since ITERATE macro evaluates 3rd arg multiple times
                ITERATE( CSeq_feat::TQual, it, qual2 ) {
                    if (NStr::EqualNocase((*it)->GetQual(), "function")) {
                        *tlabel += (*it)->GetVal();
                        return false;
                    }
                }
                const CSeq_feat_Base::TQual & qual3 = feat.GetQual(); // must store reference since ITERATE macro evaluates 3rd arg multiple times
                ITERATE( CSeq_feat::TQual, it, qual3 ) {
                    if (NStr::EqualNocase((*it)->GetQual(), "number")) {
                        *tlabel += (*it)->GetVal();
                        return false;
                    }
                }
                const CSeq_feat_Base::TQual & qual4 = feat.GetQual(); // must store reference since ITERATE macro evaluates 3rd arg multiple times
                ITERATE( CSeq_feat::TQual, it, qual4 ) {
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
        const CSeq_feat_Base::TQual & qual = feat.GetQual(); // must store reference since ITERATE macro evaluates 3rd arg multiple times
        ITERATE( CSeq_feat::TQual, it, qual ) {
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

    int d = NStr::Compare(l1, l2);
    if ( d != 0 ) {
        return d < 0;
    }

    // TODO: To make C and C++ match better, we stop comparing CDS's at this point.
    // This can be removed once we have gone completely to C++.
    if( f1.IsSetData() && f1.GetData().IsCdregion() &&
        f2.IsSetData() && f2.GetData().IsCdregion() ) 
    {
        return false;
    }

    if ( f1.IsSetComment() != f2.IsSetComment() ) {
        return !f1.IsSetComment();
    }
    if ( f1.IsSetComment() ) {
        d = NStr::Compare(f1.GetComment(), f2.GetComment());
        if ( d != 0 ) {
            return d < 0;
        }
    }

    if ( f1.IsSetId() != f2.IsSetId() ) {
        return f1.IsSetId();
    }
    if ( f1.IsSetId() ) {
        const CFeat_id& id1 = f1.GetId();
        const CFeat_id& id2 = f2.GetId();
        if ( id1.Which() != id2.Which() ) {
            return id1.Which() < id2.Which();
        }
        if ( id1.IsLocal() ) {
            const CObject_id& oid1 = id1.GetLocal();
            const CObject_id& oid2 = id2.GetLocal();
            if ( oid1.Which() != oid2.Which() ) {
                return oid1.Which() < oid2.Which();
            }
            if ( oid1.IsId() ) {
                int oid1int = oid1.GetId();
                int oid2int = oid2.GetId();
                if ( oid1int != oid2int ) {
                    return oid1int < oid2int;
                }
            }
            else if ( oid1.IsStr() ) {
                const string& oid1str = oid1.GetStr();
                const string& oid2str = oid2.GetStr();
                int diff = NStr::CompareNocase(oid1str, oid2str);
                if ( diff != 0 ) {
                    return diff < 0;
                }
            }
        }
    }

    if ( f1.GetData().IsGene() && f2.GetData().IsGene() ) {
        const CGene_ref& g1 = f1.GetData().GetGene();
        const CGene_ref& g2 = f2.GetData().GetGene();
        if ( g1.IsSetLocus_tag() != g2.IsSetLocus_tag() ) {
            return !g1.IsSetLocus_tag();
        }
        if ( g1.IsSetLocus_tag() ) {
            d = NStr::Compare(g1.GetLocus_tag(), g2.GetLocus_tag());
            if ( d != 0 ) {
                return d < 0;
            }
        }
    }

    return false;
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
    STypeLink(CSeqFeatData::ESubtype subtype = CSeqFeatData::eSubtype_imp,
              CSeqFeatData::ESubtype start = CSeqFeatData::eSubtype_bad);
        
    bool IsValid(void) const {
        return m_ParentType != CSeqFeatData::eSubtype_bad;
    }
    operator bool(void) const {
        return IsValid();
    }
    bool operator!(void) const {
        return !IsValid();
    }

    void Next(void);
    STypeLink& operator++(void) {
        Next();
        return *this;
    }

    bool CanHaveGeneParent(void) const;
    bool CanHaveCommonGene(void) const;

    CSeqFeatData::ESubtype m_StartType;   // initial feature type
    CSeqFeatData::ESubtype m_CurrentType; // current link child type
    CSeqFeatData::ESubtype m_ParentType;  // current link parent type
    bool                   m_ByProduct;
};


STypeLink::STypeLink(CSeqFeatData::ESubtype subtype,
                     CSeqFeatData::ESubtype start)
    : m_StartType(start == CSeqFeatData::eSubtype_bad? subtype: start),
      m_CurrentType(subtype),
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


inline bool STypeLink::CanHaveGeneParent(void) const
{
    return *this && m_CurrentType != CSeqFeatData::eSubtype_gene;
}


inline bool STypeLink::CanHaveCommonGene(void) const
{
    return CanHaveGeneParent();
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
        *this = STypeLink(m_ParentType, m_StartType);
        break;
    }
}


namespace {
    // Checks if the location has mixed strands or wrong order of intervals
    static
    bool sx_IsIrregularLocation(const CSeq_loc& loc,
                                TSeqPos circular_length)
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
                if ( sx_IsIrregularLocation(loc1, circular_length) ) {
                    return true;
                }
                if ( circular_length != kInvalidSeqPos ) {
                    // cannot check interval order on circular sequences
                    continue;
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


    static
    TSeqPos sx_GetCircularLength(CScope& scope,
                                 const CSeq_loc& loc)
    {
        try {
            const CSeq_id* single_id = 0;
            loc.CheckId(single_id);
            if ( !single_id ) {
                return kInvalidSeqPos;
            }
            
            CBioseq_Handle bh = scope.GetBioseqHandle(*single_id);
            if ( bh && bh.IsSetInst_Topology() &&
                 bh.GetInst_Topology() == CSeq_inst::eTopology_circular ) {
                return bh.GetBioseqLength();
            }
        }
        catch ( CException& /*ignored*/ ) {
            return kInvalidSeqPos;
        }
        return kInvalidSeqPos;
    }


    static
    TSeqPos sx_GetCircularLength(CScope& scope,
                                 const CSeq_id_Handle& id)
    {
        try {
            CBioseq_Handle bh = scope.GetBioseqHandle(id);
            if ( bh && bh.IsSetInst_Topology() &&
                 bh.GetInst_Topology() == CSeq_inst::eTopology_circular ) {
                return bh.GetBioseqLength();
            }
        }
        catch ( CException& /*ignored*/ ) {
            return kInvalidSeqPos;
        }
        return kInvalidSeqPos;
    }


    static inline
    bool sx_CanHaveTranscriptId(CSeqFeatData::ESubtype type)
    {
        return
            type == CSeqFeatData::eSubtype_mRNA ||
            type == CSeqFeatData::eSubtype_cdregion;
    }


    static
    const CGb_qual* sx_GetTranscriptId(const CMappedFeat& feat)
    {
        if ( feat.IsSetQual() &&
             sx_CanHaveTranscriptId(feat.GetFeatSubtype()) ) {
            CConstRef<CSeq_feat> f = feat.GetSeq_feat();
            const CSeq_feat::TQual& qual = f->GetQual();
            ITERATE ( CSeq_feat::TQual, it, qual ) {
                if ( (*it)->GetQual() == "transcript_id" &&
                     (*it)->IsSetVal() ) {
                    return *it;
                }
            }
        }
        return 0;
    }


    static inline
    bool sx_GetTranscriptIdMatch(const CGb_qual* id1,
                                 const CGb_qual* id2)
    {
        return id1 && id2 && id1->GetVal() == id2->GetVal();
    }


    static inline
    EOverlapType sx_GetOverlapType(const STypeLink& link,
                                   const CSeq_loc& loc,
                                   TSeqPos circular_length)
    {
        EOverlapType overlap_type = eOverlap_Contained;
        if ( link.m_StartType == CSeqFeatData::eSubtype_cdregion &&
             link.m_CurrentType == CSeqFeatData::eSubtype_cdregion &&
             link.m_ParentType == CSeqFeatData::eSubtype_mRNA ) {
            overlap_type = eOverlap_CheckIntervals;
        }
        if ( link.m_ParentType == CSeqFeatData::eSubtype_gene &&
             (true || sx_IsIrregularLocation(loc, circular_length)) ) {
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


    static
    bool sx_IsParentType(CSeqFeatData::ESubtype parent_type,
                         CSeqFeatData::ESubtype feat_type)
    {
        if ( feat_type != parent_type ) {
            for ( STypeLink link(feat_type); link; ++link ) {
                if ( link.m_ParentType == parent_type ) {
                    return true;
                }
            }
        }
        return false;
    }

    
    static const int kBetterTypeParentQuality= 1000;
    static const int kByLocusParentQuality   =  750;
    static const int kSameTypeParentQuality  =  500;
    static const int kWorseTypeParentQuality =  kSameTypeParentQuality;

    static
    int sx_GetParentTypeQuality(CSeqFeatData::ESubtype parent,
                                CSeqFeatData::ESubtype child)
    {
        int d_child = sx_GetRootDistance(child);
        int d_parent = sx_GetRootDistance(parent);
        if ( d_parent < d_child ) {
            // parent candidate is higher than child
            // return value <= kBetterTypeParentQuality
            return kBetterTypeParentQuality - (d_child - d_parent);
        }
        else {
            // parent candidate is not higher than child
            // return value <= kWorseTypeParentQuality
            return kWorseTypeParentQuality - (d_parent - d_child);
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
                                      const STypeLink& link,
                                      TSeqPos circular_length)
    {
        CMappedFeat best_parent;

        const CSeq_loc& c_loc = feat.GetLocation();

        // find best suitable parent by overlap score
        EOverlapType overlap_type =
            sx_GetOverlapType(link, c_loc, circular_length);
    
        Int8 best_overlap = kMax_I8;
        SAnnotSelector sel(link.m_ParentType);
        sel.SetByProduct(link.m_ByProduct);
        for (CFeat_CI it(feat.GetScope(), c_loc, sel); it; ++it) {
            Int8 overlap = TestForOverlap64(it->GetLocation(),
                                            c_loc,
                                            overlap_type,
                                            circular_length,
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
    TSeqPos circular_length =
        sx_GetCircularLength(feat.GetScope(), feat.GetLocation());
    for( STypeLink link(feat.GetFeatSubtype()); link; ++link ) {
        best_parent = sx_GetParentByRef(feat, link);
        if ( best_parent ) {
            // found by Xref
            break;
        }

        best_parent = sx_GetParentByOverlap(feat, link, circular_length);
        if ( best_parent ) {
            // parent is found by overlap
            break;
        }
    }
    return best_parent;
}


/////////////////////////////////////////////////////////////////////////////
// CFeatTreeIndex
/////////////////////////////////////////////////////////////////////////////


namespace {
    struct SBestInfo {
        typedef CFeatTree::CFeatInfo CFeatInfo;
        SBestInfo(void)
            : m_Quality(kMin_I1),
              m_Overlap(kMax_I8),
              m_Info(0)
            {
            }

        void CheckBest(Int1 quality, Int8 overlap, CFeatInfo* info)
            {
                _ASSERT(overlap >= 0);
                if ( (quality > m_Quality ||
                      (quality == m_Quality && overlap < m_Overlap)) ) {
                    m_Quality = quality;
                    m_Overlap = overlap;
                    m_Info = info;
                }
            }

        Int1 m_Quality;
        Int8 m_Overlap;
        CFeatInfo* m_Info;
    };
    struct SFeatRangeInfo {
        typedef CFeatTree::CFeatInfo CFeatInfo;

        CSeq_id_Handle m_Id;
        CRange<TSeqPos> m_Range;
        CFeatInfo* m_Info;

        // min start coordinate for all entries after this
        TSeqPos m_MinFrom;

        // results
        SBestInfo* m_Best;
        
        SFeatRangeInfo(CFeatInfo& info, SBestInfo* best,
                       bool by_product = false)
            : m_Info(&info),
              m_Best(best)
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
        SFeatRangeInfo(CFeatInfo& info, SBestInfo* best,
                       CHandleRangeMap::const_iterator it)
            : m_Id(it->first),
              m_Range(it->second.GetOverlappingRange()),
              m_Info(&info),
              m_Best(best)
            {
            }
    };
    struct PLessByStart {
        // sort first by start coordinate, then by end coordinate
        bool operator()(const SFeatRangeInfo& a, const SFeatRangeInfo& b) const
            {
                return a.m_Id < b.m_Id ||
                    (a.m_Id == b.m_Id && a.m_Range < b.m_Range);
            }
    };
    struct PLessByEnd {
        // sort first by end coordinate, then by start coordinate
        bool operator()(const SFeatRangeInfo& a, const SFeatRangeInfo& b) const
            {
                return a.m_Id < b.m_Id ||
                    (a.m_Id == b.m_Id &&
                     (a.m_Range.GetToOpen() < b.m_Range.GetToOpen() ||
                      (a.m_Range.GetToOpen() == b.m_Range.GetToOpen() &&
                       a.m_Range.GetFrom() < b.m_Range.GetFrom())));
            }
    };

    void s_AddRanges(vector<SFeatRangeInfo>& rr,
                     CFeatTree::CFeatInfo& info,
                     SBestInfo* best,
                     const CSeq_loc& loc)
    {
        info.m_MultiId = true;
        CHandleRangeMap hrmap;
        hrmap.AddLocation(loc);
        ITERATE ( CHandleRangeMap, it, hrmap ) {
            SFeatRangeInfo range_info(info, best, it);
            rr.push_back(range_info);
        }
    }

    typedef vector<SBestInfo> TBestArray;
    typedef vector<SFeatRangeInfo> TRangeArray;
    typedef vector<CFeatTree::CFeatInfo*> TInfoArray;

    inline
    Int1 s_GetParentQuality(const CFeatTree::CFeatInfo& feat,
                            const CFeatTree::CFeatInfo& parent)
    {
        return sx_GetTranscriptIdMatch(feat.m_TranscriptId,
                                       parent.m_TranscriptId);
    }

    class CFeatTreeParentTypeIndex : public CObject
    {
    public:
        CFeatTreeParentTypeIndex(CSeqFeatData::ESubtype type,
                                 bool by_product)
            : m_Type(type),
              m_ByProduct(by_product),
              m_IndexedParents(0)
            {
            }

        TRangeArray& GetIndex(const TInfoArray& feats) {
            if ( m_IndexedParents == feats.size() ) {
                return m_Index;
            }
            for ( size_t ind = m_IndexedParents; ind < feats.size(); ++ind ) {
                CFeatTree::CFeatInfo& feat_info = *feats[ind];
                if ( feat_info.m_AddIndex < m_IndexedParents ||
                     feat_info.GetSubtype() != m_Type ||
                     (m_ByProduct && !feat_info.m_Feat.IsSetProduct()) ) {
                    continue;
                }
                SFeatRangeInfo range_info(feat_info, 0, m_ByProduct);
                if ( range_info.m_Id ) {
                    m_Index.push_back(range_info);
                }
                else {
                    s_AddRanges(m_Index, feat_info, 0,
                                m_ByProduct?
                                feat_info.m_Feat.GetProduct():
                                feat_info.m_Feat.GetLocation());
                }
            }
            sort(m_Index.begin(), m_Index.end(), PLessByEnd());
            m_IndexedParents = feats.size();
            return m_Index;
        }
        
    private:
        CSeqFeatData::ESubtype m_Type;
        bool m_ByProduct;
        size_t m_IndexedParents;
        TRangeArray m_Index;
    };
}


class CFeatTreeIndex : public CObject
{
public:
    typedef pair<CSeqFeatData::ESubtype, bool> TParentKey;
    typedef map<TParentKey, CRef<CFeatTreeParentTypeIndex> > TIndex;

    TRangeArray& GetIndex(CSeqFeatData::ESubtype type,
                          bool by_product,
                          const TInfoArray& feats) {
        CRef<CFeatTreeParentTypeIndex>& index =
            m_Index[TParentKey(type, by_product)];
        if ( !index ) {
            index = new CFeatTreeParentTypeIndex(type, by_product);
        }
        return index->GetIndex(feats);
    }

    TRangeArray& GetIndex(const STypeLink& link, const TInfoArray& feats) {
        return GetIndex(link.m_ParentType, link.m_ByProduct, feats);
    }

private:
    TIndex m_Index;
};


/////////////////////////////////////////////////////////////////////////////
// CFeatTree
/////////////////////////////////////////////////////////////////////////////

CFeatTree::CFeatTree(void)
{
    x_Init();
}


CFeatTree::CFeatTree(CFeat_CI it)
{
    x_Init();
    AddFeatures(it);
}


CFeatTree::CFeatTree(const CSeq_annot_Handle& sah)
{
    x_Init();
    CFeat_CI it(sah);
    AddFeatures(it);
}


CFeatTree::CFeatTree(const CSeq_entry_Handle& seh)
{
    x_Init();
    CFeat_CI it(seh);
    AddFeatures(it);
}


CFeatTree::~CFeatTree(void)
{
}


CFeatTree::CFeatTree(const CFeatTree& ft)
{
    *this = ft;
}


CFeatTree& CFeatTree::operator=(const CFeatTree& ft)
{
    if ( this != &ft ) {
        m_AssignedParents = 0;
        m_AssignedGenes = 0;
        m_InfoMap.clear();
        m_InfoArray.clear();
        m_RootInfo = CFeatInfo();
        m_FeatIdMode = ft.m_FeatIdMode;
        m_BestGeneFeatIdMode = ft.m_BestGeneFeatIdMode;
        m_GeneCheckMode = ft.m_GeneCheckMode;
        m_SNPStrandMode = ft.m_SNPStrandMode;
        m_Index = null;
        m_InfoArray.reserve(ft.m_InfoArray.size());
        ITERATE ( TInfoArray, it, ft.m_InfoArray ) {
            AddFeature((*it)->m_Feat);
        }
    }
    return *this;
}


void CFeatTree::x_Init(void)
{
    m_AssignedParents = 0;
    m_AssignedGenes = 0;
    m_FeatIdMode = eFeatId_by_type;
    m_BestGeneFeatIdMode = eBestGeneFeatId_always;
    m_GeneCheckMode = eGeneCheck_match;
    m_SNPStrandMode = eSNPStrand_both;
}


void CFeatTree::SetFeatIdMode(EFeatIdMode mode)
{
    m_FeatIdMode = mode;
}


void CFeatTree::SetGeneCheckMode(EGeneCheckMode mode)
{
    m_GeneCheckMode = mode;
}


void CFeatTree::SetSNPStrandMode(ESNPStrandMode mode)
{
    m_SNPStrandMode = mode;
}


void CFeatTree::AddFeatures(CFeat_CI it)
{
    for ( ; it; ++it ) {
        AddFeature(*it);
    }
}


void CFeatTree::AddFeature(const CMappedFeat& feat)
{
    if ( !feat ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CFeatTree: feature is null");
    }
    _ASSERT(m_InfoMap.size() == m_InfoArray.size());
    size_t index = m_InfoMap.size();
    CFeatInfo& info = m_InfoMap[feat.GetSeq_feat_Handle()];
    if ( !info.m_Feat ) {
        _ASSERT(m_InfoMap.size() == m_InfoArray.size()+1);
        m_InfoArray.push_back(&info);
        info.m_AddIndex = index;
        info.m_Feat = feat;
        info.m_TranscriptId = sx_GetTranscriptId(feat);
    }
    else {
        _ASSERT(m_InfoMap.size() == m_InfoArray.size());
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


pair<int, CFeatTree::CFeatInfo*>
CFeatTree::x_LookupParentByRef(CFeatInfo& info,
                               CSeqFeatData::ESubtype parent_type)
{
    pair<int, CFeatInfo*> ret(0, nullptr);
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
                sx_GetParentTypeQuality(parent->GetSubtype(),
                                        info.GetSubtype());
            if ( quality > ret.first ) {
                ret.first = quality;
                ret.second = parent;
            }
        }
    }
    if ( ret.first > kByLocusParentQuality ) {
        return ret;
    }
    if ( (parent_type == CSeqFeatData::eSubtype_gene ||
          parent_type == CSeqFeatData::eSubtype_any) &&
         sx_IsParentType(CSeqFeatData::eSubtype_gene,
                         info.GetSubtype()) ) {
        // assign non-genes to genes by Gene-ref
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
                            ret.first = kByLocusParentQuality;
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
    _ASSERT(m_FeatIdMode != eFeatId_ignore);
    pair<int, CFeatInfo*> parent =
        x_LookupParentByRef(info, CSeqFeatData::eSubtype_any);
    if ( !parent.second ) {
        return false;
    }
    if ( parent.first <= kWorseTypeParentQuality ||
         parent.first == kSameTypeParentQuality ) {
        // found reference is of the same or worse type
        if ( m_FeatIdMode == eFeatId_by_type ) {
            // eFeatId_by_type limits parents to regular tree order
            return false;
        }
        _ASSERT(m_FeatIdMode == eFeatId_always);
        // otherwise check for circular references
        if ( parent.second->IsSetParent() &&
             parent.second->m_Parent == &info ) {
            // two features cycle, keep existing parent
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
    }
    // check if gene is found over possible intemediate parents
    if ( parent.second->IsGene() ) {
        // the gene link may be turned off
        if ( m_BestGeneFeatIdMode == eBestGeneFeatId_ignore ) {
            return false;
        }
        // if intermediate parents are possible
        if ( STypeLink(info.GetSubtype()).m_ParentType!=CSeqFeatData::eSubtype_gene ) {
            // then assign gene only
            info.m_Gene = parent.second;
            return false;
        }
    }
    x_SetParent(info, *parent.second);
    return true;
}

enum EStrandMatchRule {
    eStrandMatch_all,
    eStrandMatch_at_least_one,
    eStrandMatch_any
};
// Check what strand match is required
static EStrandMatchRule s_GetStrandMatchRule(const STypeLink& link,
                                             const CFeatTree::CFeatInfo& info,
                                             const CFeatTree* tree)
{
    if ( link.m_ParentType == CSeqFeatData::eSubtype_gene ) {
        if ( link.m_StartType == CSeqFeatData::eSubtype_variation &&
             tree->GetSNPStrandMode() == tree->eSNPStrand_both ) {
            // try snp rev
            return eStrandMatch_any;
        }
        if ( info.m_Feat.IsSetExcept_text() &&
             info.m_Feat.GetExcept_text().find("trans-splicing") != NPOS ) {
            return eStrandMatch_at_least_one;
        }
    }
    return eStrandMatch_all;
}


static void s_CollectBestOverlaps(CFeatTree::TFeatArray& features,
                                  TBestArray& bests,
                                  const STypeLink& link,
                                  TRangeArray& pp,
                                  CFeatTree* tree)
{
    _ASSERT(!features.empty());
    _ASSERT(!pp.empty());
    
    bool check_genes = false;
    if ( tree->GetGeneCheckMode() == tree->eGeneCheck_match &&
         link.m_ParentType != CSeqFeatData::eSubtype_gene &&
         link.CanHaveCommonGene() ) {
        // tree uses common gene information
        tree->GetBestGene(features[0]->m_Feat, tree->eBestGene_OverlappedOnly);
        check_genes = true;
    }

    TRangeArray cc;
    // collect children parameters
    size_t cnt = features.size();
    bests.resize(cnt);
    for ( size_t i = 0; i < cnt; ++i ) {
        CFeatTree::CFeatInfo& feat_info = *features[i];
        SBestInfo* best = &bests[i];
        SFeatRangeInfo range_info(feat_info, best);
        if ( range_info.m_Id ) {
            cc.push_back(range_info);
        }
        else {
            s_AddRanges(cc, feat_info, best, feat_info.m_Feat.GetLocation());
        }
    }
    sort(cc.begin(), cc.end(), PLessByStart());

    typedef pair<CFeatTree::CFeatInfo*, CFeatTree::CFeatInfo*> TFeatPair;
    set<TFeatPair> multi_id_tested;

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

            TSeqPos circular_length =
                sx_GetCircularLength(pi->m_Info->m_Feat.GetScope(), cur_id);
            
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
                CFeatTree::CFeatInfo& info = *ci->m_Info;
                const CSeq_loc& c_loc = info.m_Feat.GetLocation();
                CRef<CSeq_loc> c_loc2;
                ENa_strand c_loc2_strand = eNa_strand_unknown;
                EOverlapType overlap_type =
                    sx_GetOverlapType(link, c_loc, circular_length);
                EStrandMatchRule strand_match_rule =
                    s_GetStrandMatchRule(link, info, tree);

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
                    if ( check_genes ) {
                        const CFeatTree::CFeatInfo* p_gene =
                            link.m_ParentType == CSeqFeatData::eSubtype_gene?
                            pc->m_Info: pc->m_Info->m_Gene;
                        if ( info.m_Gene != p_gene ) {
                            continue;
                        }
                    }
                    if ( info.m_MultiId && pc->m_Info->m_MultiId &&
                         !multi_id_tested.insert(TFeatPair(&info, pc->m_Info)).second ) {
                        // already tested this pair of child and parent
                        continue;
                    }
                    const CMappedFeat& p_feat = pc->m_Info->m_Feat;
                    const CSeq_loc& p_loc =
                        link.m_ByProduct?
                        p_feat.GetProduct():
                        p_feat.GetLocation();
                    CScope* scope = &p_feat.GetScope();
                    Int1 quality = s_GetParentQuality(info, *pc->m_Info);
                    Int8 overlap;
                    try {
                        overlap = TestForOverlap64(p_loc,
                                                   c_loc,
                                                   overlap_type,
                                                   circular_length,
                                                   scope);
                    }
                    catch ( CException& /*ignored*/ ) {
                        overlap = -1;
                    }
                    if ( overlap >= 0 ) {
                        ci->m_Best->CheckBest(quality, overlap, pc->m_Info);
                        continue;
                    }
                    if ( strand_match_rule == eStrandMatch_all ) {
                        // strands mismatch -> no overlap
                        continue;
                    }
                    if ( info.m_MultiId || pc->m_Info->m_MultiId ) {
                        // cannot compare strands on multi-id locations
                        continue;
                    }
                    ENa_strand pstrand = GetStrand(p_loc, scope);
                    if ( pstrand == eNa_strand_other ) {
                        // parent has mixed strands -> no overlap
                        continue;
                    }
                    if ( pstrand == eNa_strand_unknown ) {
                        pstrand = eNa_strand_plus;
                    }
                    if ( strand_match_rule == eStrandMatch_at_least_one &&
                         GetStrand(c_loc) != eNa_strand_other ) {
                        // child's strand is single and doesn't match
                        continue;
                    }
                    if ( !c_loc2 || c_loc2_strand != pstrand ) {
                        // adjust strand to parent
                        if ( !c_loc2 ) {
                            c_loc2 = SerialClone(c_loc);
                        }
                        // force
                        c_loc2->SetStrand(pstrand);
                        c_loc2_strand = pstrand;
                    }
                    try {
                        overlap = TestForOverlap64(p_loc,
                                                   *c_loc2,
                                                   overlap_type,
                                                   circular_length,
                                                   scope);
                    }
                    catch ( CException& /*ignored*/ ) {
                        overlap = -1;
                    }
                    if ( overlap >= 0 ) {
                        ci->m_Best->CheckBest(quality-1, overlap, pc->m_Info);
                    }
                }
            }
            // skip remaining Seq-id children
            for ( ; ci != cc.end() && ci->m_Id == cur_id; ++ci ) {
            }
        }
    }}
}


void CFeatTree::x_AssignParentsByOverlap(TFeatArray& features,
                                         const STypeLink& link)
{
    if ( features.empty() ) {
        return;
    }
    if ( GetGeneCheckMode() == eGeneCheck_match &&
         link.m_ParentType == CSeqFeatData::eSubtype_gene ) {
        bool unassigned = false;
        // assign gene as parent
        ITERATE ( TFeatArray, it, features ) {
            CFeatInfo& info = **it;
            if ( !info.IsSetParent() ) {
                if ( info.m_Gene ) {
                    x_SetParent(info, *info.m_Gene);
                }
                else {
                    unassigned = true;
                }
            }
        }
        if ( !unassigned ) {
            features.clear();
            return;
        }
    }
    if ( !m_Index ) {
        m_Index = new CFeatTreeIndex;
    }
    TRangeArray& parents = m_Index->GetIndex(link, m_InfoArray);
    if ( parents.empty() ) {
        return;
    }
    TBestArray bests;
    s_CollectBestOverlaps(features, bests, link, parents, this);
    size_t cnt = features.size();
    _ASSERT(bests.size() == cnt);

    // assign found parents
    TFeatArray::iterator dst = features.begin();
    for ( size_t i = 0; i < cnt; ++i ) {
        CFeatInfo& info = *features[i];
        if ( !info.IsSetParent() ) {
            CFeatInfo* best = bests[i].m_Info;
            if ( best ) {
                // assign best parent
                x_SetParent(info, *best);
            }
            else {
                // store for future processing
                *dst++ = &info;
            }
        }
    }
    features.erase(dst, features.end());
}


void CFeatTree::x_AssignGenesByOverlap(TFeatArray& features)
{
    if ( features.empty() ) {
        return;
    }
    if ( !m_Index ) {
        m_Index = new CFeatTreeIndex;
    }
    TRangeArray& genes =
        m_Index->GetIndex(CSeqFeatData::eSubtype_gene, false, m_InfoArray);
    if ( genes.empty() ) {
        return;
    }
    TBestArray bests;
    s_CollectBestOverlaps(features, bests, STypeLink(), genes, this);
    size_t cnt = features.size();
    _ASSERT(bests.size() == cnt);

    // assign found genes
    for ( size_t i = 0; i < cnt; ++i ) {
        CFeatInfo& info = *features[i];
        if ( !info.m_Gene ) {
            CFeatInfo* best = bests[i].m_Info;
            if ( best ) {
                // assign best gene
                info.m_Gene = best;
            }
        }
    }
}


void CFeatTree::x_SetGeneRecursive(CFeatInfo& info, CFeatInfo& gene)
{
    info.m_Gene = &gene;
    ITERATE ( CFeatInfo::TChildren, it, info.m_Children ) {
        CFeatInfo& child = **it;
        if ( child.m_Gene != &gene ) {
            x_SetGeneRecursive(child, gene);
        }
    }
}


void CFeatTree::x_AssignGenes(void)
{
    if ( m_AssignedGenes >= m_InfoArray.size() ) {
        return;
    }

    for ( size_t ind = m_AssignedGenes; ind < m_InfoArray.size(); ++ind ) {
        CFeatInfo& info = *m_InfoArray[ind];
        if ( info.m_Gene ) {
            continue;
        }
        if ( CFeatInfo* parent = info.m_Parent ) {
            CFeatInfo* gene = parent->m_Gene;
            if ( !gene && parent->IsGene() ) {
                gene = parent;
            }
            if ( gene ) {
                x_SetGeneRecursive(info, *gene);
            }
        }
    }

    bool has_genes = false;
    TFeatArray old_feats, new_feats;
    // collect genes and other features
    for ( size_t ind = m_AssignedGenes; ind < m_InfoArray.size(); ++ind ) {
        CFeatInfo& info = *m_InfoArray[ind];
        TFeatArray* arr = 0;
        CSeqFeatData::ESubtype feat_type = info.GetSubtype();
        if ( feat_type == CSeqFeatData::eSubtype_gene ) {
            has_genes = true;
            continue;
        }
        else if ( !info.m_Gene && STypeLink(feat_type).CanHaveGeneParent() ) {
            if ( m_BestGeneFeatIdMode == eBestGeneFeatId_always ) {
                CFeatInfo* gene =
                    x_LookupParentByRef(info,
                                        CSeqFeatData::eSubtype_gene).second;
                if ( gene ) {
                    info.m_Gene = gene;
                    continue;
                }
            }
            arr = info.m_AddIndex >= m_AssignedGenes? &new_feats: &old_feats;
        }
        else {
            continue;
        }
        arr->push_back(&info);
    }
    if ( !old_feats.empty() ) {
        old_feats.insert(old_feats.end(),
                         new_feats.begin(), new_feats.end());
        swap(old_feats, new_feats);
        old_feats.clear();
    }
    if ( has_genes && !new_feats.empty() ) {
        x_AssignGenesByOverlap(new_feats);
    }
    m_AssignedGenes = m_InfoArray.size();
}


void CFeatTree::x_AssignParents(void)
{
    if ( m_AssignedParents >= m_InfoArray.size() ) {
        return;
    }

    // collect all features without assigned parent
    vector<TFeatArray> feats_by_type;
    feats_by_type.reserve(CSeqFeatData::eSubtype_max+1);
    size_t new_count = 0;
    for ( size_t ind = m_AssignedParents; ind < m_InfoArray.size(); ++ind ) {
        CFeatInfo& info = *m_InfoArray[ind];
        if ( info.IsSetParent() ) {
            continue;
        }
        if ( m_FeatIdMode != eFeatId_ignore && x_AssignParentByRef(info) ) {
            continue;
        }
        CSeqFeatData::ESubtype feat_type = info.GetSubtype();
        STypeLink link(feat_type);
        if ( !link ) {
            // no parent
            x_SetNoParent(info);
        }
        else {
            size_t index = feat_type;
            if ( index >= feats_by_type.size() ) {
                feats_by_type.resize(index+1);
            }
            feats_by_type[feat_type].push_back(&info);
            ++new_count;
        }
    }
    if ( new_count == 0 ) { // no work to do
        return;
    }
    // assign parents for each parent type
    for ( size_t type = 0; type < feats_by_type.size(); ++type ) {
        TFeatArray& feats = feats_by_type[type];
        if ( feats.empty() ) {
            // no work to do
            continue;
        }
        for ( STypeLink link((CSeqFeatData::ESubtype)type); link; ++link ) {
            x_AssignParentsByOverlap(feats, link);
            if ( feats.empty() ) {
                break;
            }
        }
        // all remaining features are without parent
        ITERATE ( TFeatArray, it, feats ) {
            x_SetNoParent(**it);
        }
    }

    if ( m_FeatIdMode == eFeatId_always ) {
        for ( size_t ind=m_AssignedParents; ind<m_InfoArray.size(); ++ind ) {
            CFeatInfo& info = *m_InfoArray[ind];
            x_VerifyLinkedToRoot(info);
        }
    }

    m_AssignedParents = m_InfoArray.size();
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


CMappedFeat CFeatTree::GetBestGene(const CMappedFeat& feat,
                                   EBestGeneType lookup_type)
{
    CMappedFeat ret;
    if ( lookup_type == eBestGene_TreeOnly ||
         lookup_type == eBestGene_AllowOverlapped ) {
        ret = GetParent(feat, CSeqFeatData::eSubtype_gene);
    }
    if ( !ret && lookup_type != eBestGene_TreeOnly ) {
        x_AssignGenes();
        CFeatInfo* gene = x_GetInfo(feat).m_Gene;
        if ( gene ) {
            ret = gene->m_Feat;
        }
    }
    return ret;
}


CFeatTree::CFeatInfo::CFeatInfo(void)
    : m_AddIndex(0),
      m_IsSetParent(false),
      m_IsSetChildren(false),
      m_MultiId(false),
      m_IsLinkedToRoot(eIsLinkedToRoot_unknown),
      m_Parent(0),
      m_Gene(0)
{
}


CFeatTree::CFeatInfo::~CFeatInfo(void)
{
}


const CTSE_Handle& CFeatTree::CFeatInfo::GetTSE(void) const
{
    return m_Feat.GetAnnot().GetTSE_Handle();
}


void CFeatTree::AddFeaturesFor(CScope& scope, const CSeq_loc& loc,
                               CSeqFeatData::ESubtype bottom_type,
                               CSeqFeatData::ESubtype top_type,
                               const SAnnotSelector* base_sel,
                               bool skip_bottom)
{
    SAnnotSelector sel;
    if ( base_sel ) {
        sel = *base_sel;
    }
    else {
        sel.SetResolveAll().SetAdaptiveDepth().SetOverlapTotalRange();
    }
    if ( skip_bottom ) {
        sel.SetAnnotType(CSeq_annot::C_Data::e_not_set);
    }
    else {
        sel.SetFeatSubtype(bottom_type);
    }
    if ( top_type != bottom_type ) {
        for ( STypeLink link(bottom_type); link; ++link ) {
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


void CFeatTree::AddFeaturesFor(const CMappedFeat& feat,
                               CSeqFeatData::ESubtype bottom_type,
                               CSeqFeatData::ESubtype top_type,
                               const SAnnotSelector* base_sel)
{
    AddFeature(feat);
    AddFeaturesFor(feat.GetScope(), feat.GetLocation(),
                   bottom_type, top_type, base_sel);
}


void CFeatTree::AddFeaturesFor(const CMappedFeat& feat,
                               CSeqFeatData::ESubtype top_type,
                               const SAnnotSelector* base_sel)
{
    AddFeature(feat);
    AddFeaturesFor(feat.GetScope(), feat.GetLocation(),
                   feat.GetFeatSubtype(), top_type, base_sel, true);
}


void CFeatTree::AddGenesForMrna(const CMappedFeat& mrna_feat,
                                const SAnnotSelector* base_sel)
{
    AddFeaturesFor(mrna_feat,
                   CSeqFeatData::eSubtype_gene,
                   base_sel);
}


void CFeatTree::AddCdsForMrna(const CMappedFeat& mrna_feat,
                              const SAnnotSelector* base_sel)
{
    AddFeaturesFor(mrna_feat,
                   CSeqFeatData::eSubtype_cdregion,
                   CSeqFeatData::eSubtype_mRNA,
                   base_sel);
}


void CFeatTree::AddGenesForCds(const CMappedFeat& cds_feat,
                               const SAnnotSelector* base_sel)
{
    AddFeaturesFor(cds_feat,
                   CSeqFeatData::eSubtype_gene,
                   base_sel);
}


void CFeatTree::AddMrnasForCds(const CMappedFeat& cds_feat,
                               const SAnnotSelector* base_sel)
{
    AddFeaturesFor(cds_feat,
                   CSeqFeatData::eSubtype_mRNA,
                   base_sel);
}


void CFeatTree::AddMrnasForGene(const CMappedFeat& gene_feat,
                                const SAnnotSelector* base_sel)
{
    AddFeaturesFor(gene_feat,
                   CSeqFeatData::eSubtype_mRNA,
                   CSeqFeatData::eSubtype_gene,
                   base_sel);
}


void CFeatTree::AddCdsForGene(const CMappedFeat& gene_feat,
                              const SAnnotSelector* base_sel)
{
    AddFeaturesFor(gene_feat,
                   CSeqFeatData::eSubtype_cdregion,
                   CSeqFeatData::eSubtype_gene,
                   base_sel);
}


void CFeatTree::AddGenesForFeat(const CMappedFeat& feat,
                                const SAnnotSelector* base_sel)
{
    AddFeaturesFor(feat,
                   CSeqFeatData::eSubtype_gene,
                   base_sel);
}


/////////////////////////////////////////////////////////////////////////////
// New API for GetBestXxxForXxx()

CMappedFeat
GetBestGeneForMrna(const CMappedFeat& mrna_feat,
                   CFeatTree* feat_tree,
                   const SAnnotSelector* base_sel,
                   CFeatTree::EBestGeneType lookup_type)
{
    if ( !mrna_feat ||
         mrna_feat.GetFeatSubtype() != CSeqFeatData::eSubtype_mRNA ) {
        NCBI_THROW(CObjmgrUtilException, eBadFeature,
                   "GetBestGeneForMrna: mrna_feat is not a mRNA");
    }
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddGenesForMrna(mrna_feat, base_sel);
        return tree.GetBestGene(mrna_feat, lookup_type);
    }
    return feat_tree->GetBestGene(mrna_feat, lookup_type);
}


CMappedFeat
GetBestGeneForCds(const CMappedFeat& cds_feat,
                  CFeatTree* feat_tree,
                  const SAnnotSelector* base_sel,
                  CFeatTree::EBestGeneType lookup_type)
{
    if ( !cds_feat ||
         cds_feat.GetFeatSubtype() != CSeqFeatData::eSubtype_cdregion ) {
        NCBI_THROW(CObjmgrUtilException, eBadFeature,
                   "GetBestGeneForCds: cds_feat is not a cdregion");
    }
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddGenesForCds(cds_feat, base_sel);
        return tree.GetBestGene(cds_feat, lookup_type);
    }
    return feat_tree->GetBestGene(cds_feat, lookup_type);
}


CMappedFeat
GetBestMrnaForCds(const CMappedFeat& cds_feat,
                  CFeatTree* feat_tree,
                  const SAnnotSelector* base_sel)
{
    if ( !cds_feat ||
         cds_feat.GetFeatSubtype() != CSeqFeatData::eSubtype_cdregion ) {
        NCBI_THROW(CObjmgrUtilException, eBadFeature,
                   "GetBestMrnaForCds: cds_feat is not a cdregion");
    }
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddMrnasForCds(cds_feat, base_sel);
        return tree.GetParent(cds_feat, CSeqFeatData::eSubtype_mRNA);
    }
    return feat_tree->GetParent(cds_feat, CSeqFeatData::eSubtype_mRNA);
}


CMappedFeat
GetBestCdsForMrna(const CMappedFeat& mrna_feat,
                  CFeatTree* feat_tree,
                  const SAnnotSelector* base_sel)
{
    if ( !mrna_feat ||
         mrna_feat.GetFeatSubtype() != CSeqFeatData::eSubtype_mRNA ) {
        NCBI_THROW(CObjmgrUtilException, eBadFeature,
                   "GetBestCdsForMrna: mrna_feat is not a mRNA");
    }
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddCdsForMrna(mrna_feat, base_sel);
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
                     CFeatTree* feat_tree,
                     const SAnnotSelector* base_sel)
{
    if ( !gene_feat ||
         gene_feat.GetFeatSubtype() != CSeqFeatData::eSubtype_gene ) {
        NCBI_THROW(CObjmgrUtilException, eBadFeature,
                   "GetMrnasForGene: gene_feat is not a gene");
    }
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddMrnasForGene(gene_feat, base_sel);
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
                    CFeatTree* feat_tree,
                    const SAnnotSelector* base_sel)
{
    if ( !gene_feat ||
         gene_feat.GetFeatSubtype() != CSeqFeatData::eSubtype_gene ) {
        NCBI_THROW(CObjmgrUtilException, eBadFeature,
                   "GetCdssForGene: gene_feat is not a gene");
    }
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddCdsForGene(gene_feat, base_sel);
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


CMappedFeat
GetBestGeneForFeat(const CMappedFeat& feat,
                   CFeatTree* feat_tree,
                   const SAnnotSelector* base_sel,
                   CFeatTree::EBestGeneType lookup_type)
{
    if ( !feat ) {
        NCBI_THROW(CObjmgrUtilException, eBadFeature,
                   "GetBestGeneForFeat: feat is null");
    }
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddGenesForFeat(feat, base_sel);
        return tree.GetBestGene(feat, lookup_type);
    }
    return feat_tree->GetBestGene(feat, lookup_type);
}


CMappedFeat
GetBestParentForFeat(const CMappedFeat& feat,
                     CSeqFeatData::ESubtype parent_type,
                     CFeatTree* feat_tree,
                     const SAnnotSelector* base_sel)
{
    if ( !feat ) {
        NCBI_THROW(CObjmgrUtilException, eBadFeature,
                   "GetBestParentForFeat: feat is null");
    }
    if ( !feat_tree ) {
        CFeatTree tree;
        tree.AddFeaturesFor(feat, parent_type, base_sel);
        return tree.GetParent(feat, parent_type);
    }
    return feat_tree->GetParent(feat, parent_type);
}


typedef pair<Int8, CMappedFeat> TMappedFeatScore;
typedef vector<TMappedFeatScore> TMappedFeatScores;

static
void GetOverlappingFeatures(CScope& scope, const CSeq_loc& loc,
                            CSeqFeatData::E_Choice feat_type,
                            CSeqFeatData::ESubtype feat_subtype,
                            sequence::EOverlapType overlap_type,
                            TMappedFeatScores& feats,
                            const SAnnotSelector* base_sel)
{
    bool revert_locations = false;
    SAnnotSelector::EOverlapType annot_overlap_type;
    switch (overlap_type) {
    case eOverlap_Simple:
    case eOverlap_Contained:
    case eOverlap_Contains:
        // Require total range overlap
        annot_overlap_type = SAnnotSelector::eOverlap_TotalRange;
        break;
    case eOverlap_Subset:
    case eOverlap_SubsetRev:
    case eOverlap_CheckIntervals:
    case eOverlap_Interval:
    case eOverlap_CheckIntRev:
        revert_locations = true;
        // there's no break here - proceed to "default"
    default:
        // Require intervals overlap
        annot_overlap_type = SAnnotSelector::eOverlap_Intervals;
        break;
    }

    CConstRef<CSeq_feat> feat_ref;

    CBioseq_Handle h;
    CRange<TSeqPos> range;
    ENa_strand strand = eNa_strand_unknown;
    if ( loc.IsWhole() ) {
        h = scope.GetBioseqHandle(loc.GetWhole());
        range = range.GetWhole();
    }
    else if ( loc.IsInt() ) {
        const CSeq_interval& interval = loc.GetInt();
        h = scope.GetBioseqHandle(interval.GetId());
        range.SetFrom(interval.GetFrom());
        range.SetTo(interval.GetTo());
        if ( interval.IsSetStrand() ) {
            strand = interval.GetStrand();
        }
    }
    else {
        range = range.GetEmpty();
    }

    // Check if the sequence is circular
    TSeqPos circular_length = kInvalidSeqPos;
    if ( h ) {
        if ( h.IsSetInst_Topology() &&
             h.GetInst_Topology() == CSeq_inst::eTopology_circular ) {
            circular_length = h.GetBioseqLength();
        }
    }
    else {
        try {
            const CSeq_id* single_id = 0;
            try {
                loc.CheckId(single_id);
            }
            catch (CException&) {
                single_id = 0;
            }
            if ( single_id ) {
                CBioseq_Handle h = scope.GetBioseqHandle(*single_id);
                if ( h && h.IsSetInst_Topology() &&
                     h.GetInst_Topology() == CSeq_inst::eTopology_circular ) {
                    circular_length = h.GetBioseqLength();
                }
            }
        }
        catch (CException& _DEBUG_ARG(e)) {
            _TRACE("test for circularity failed: " << e.GetMsg());
        }
    }

    try {
        SAnnotSelector sel;
        if ( base_sel ) {
            sel = *base_sel;
        }
        else {
            sel.SetResolveAll().SetAdaptiveDepth();
        }
        sel.SetFeatSubtype(feat_subtype).SetOverlapType(annot_overlap_type);
        if ( h ) {
            CFeat_CI feat_it(h, range, strand, sel);
            for ( ;  feat_it;  ++feat_it) {
                // treat subset as a special case
                Int8 cur_diff = ( !revert_locations ) ?
                    TestForOverlap64(feat_it->GetLocation(),
                                     loc,
                                     overlap_type,
                                     circular_length,
                                     &scope) :
                    TestForOverlap64(loc,
                                     feat_it->GetLocation(),
                                     overlap_type,
                                     circular_length,
                                     &scope);
                if (cur_diff < 0) {
                    continue;
                }

                TMappedFeatScore sc(cur_diff, *feat_it);
                feats.push_back(sc);
            }
        }
        else {
            CFeat_CI feat_it(scope, loc, sel);
            for ( ;  feat_it;  ++feat_it) {
                // treat subset as a special case
                Int8 cur_diff = ( !revert_locations ) ?
                    TestForOverlap64(feat_it->GetLocation(),
                                     loc,
                                     overlap_type,
                                     circular_length,
                                     &scope) :
                    TestForOverlap64(loc,
                                     feat_it->GetLocation(),
                                     overlap_type,
                                     circular_length,
                                     &scope);
                if (cur_diff < 0) {
                    continue;
                }

                TMappedFeatScore sc(cur_diff, *feat_it);
                feats.push_back(sc);
            }
        }
    }
    catch (CException&) {
        _TRACE("GetOverlappingFeatures(): error: feature iterator failed");
    }
}


static
CMappedFeat GetBestOverlappingFeat(CScope& scope,
                                   const CSeq_loc& loc,
                                   CSeqFeatData::ESubtype feat_subtype,
                                   sequence::EOverlapType overlap_type,
                                   TBestFeatOpts opts,
                                   const SAnnotSelector* base_sel)
{
    TMappedFeatScores scores;
    GetOverlappingFeatures(scope, loc,
        CSeqFeatData::GetTypeFromSubtype(feat_subtype), feat_subtype,
        overlap_type, scores, base_sel);

    if ( !scores.empty() ) {
        if (opts & fBestFeat_FavorLonger) {
            return max_element(scores.begin(), scores.end())->second;
        }
        else {
            return min_element(scores.begin(), scores.end())->second;
        }
    }
    return CMappedFeat();
}


CMappedFeat
GetBestOverlappingFeat(const CMappedFeat& feat,
                       CSeqFeatData::ESubtype need_subtype,
                       sequence::EOverlapType overlap_type,
                       CFeatTree* feat_tree,
                       const SAnnotSelector* base_sel)
{
    // special cases
    switch ( need_subtype ) {
    case CSeqFeatData::eSubtype_gene:
        switch ( feat.GetFeatSubtype() ) {
        case CSeqFeatData::eSubtype_operon:
        case CSeqFeatData::eSubtype_gene:
            break;
        case CSeqFeatData::eSubtype_mRNA:
            return GetBestGeneForMrna(feat, feat_tree, base_sel);
        case CSeqFeatData::eSubtype_cdregion:
            return GetBestGeneForCds(feat, feat_tree, base_sel);
        default:
            return GetBestGeneForFeat(feat, feat_tree, base_sel);
        }
        break;
    case CSeqFeatData::eSubtype_mRNA:
        if ( feat.GetFeatSubtype() == CSeqFeatData::eSubtype_cdregion ) {
            return GetBestMrnaForCds(feat, feat_tree, base_sel);
        }
        break;
    case CSeqFeatData::eSubtype_cdregion:
        if ( feat.GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA ) {
            return GetBestCdsForMrna(feat, feat_tree, base_sel);
        }
        break;
    default:
        break;
    }
    // in-tree child -> parent lookup
    if ( sx_IsParentType(need_subtype, feat.GetFeatSubtype()) ) {
        return GetBestParentForFeat(feat, need_subtype, feat_tree, base_sel);
    }
    // non-tree overlap
    return GetBestOverlappingFeat(feat.GetScope(), feat.GetLocation(),
                                  need_subtype, overlap_type, 0, base_sel);
}


CRef<CSeq_loc_Mapper>
CreateSeqLocMapperFromFeat(const CSeq_feat& feat,
                           CSeq_loc_Mapper::EFeatMapDirection dir,
                           CScope* scope)
{
    CRef<CSeq_loc_Mapper> mapper;
    if ( !feat.IsSetProduct() ) return mapper; // NULL

    bool benign_feat_exception = feat.IsSetExcept_text()  &&
        (feat.GetExcept_text() == "mismatches in translation"  ||
        feat.GetExcept_text() == "mismatches in transcription");
    bool severe_feat_exception = 
        ((feat.IsSetExcept() && feat.GetExcept())  ||
        feat.IsSetExcept_text())  && !benign_feat_exception;

    if (severe_feat_exception  ||
        feat.GetLocation().IsTruncatedStart(eExtreme_Biological)  ||
        feat.GetLocation().IsPartialStart(eExtreme_Biological)) {
        return mapper; // NULL
    }

    mapper.Reset(new CSeq_loc_Mapper(feat, dir, scope));
    return mapper;
}


/////////////////////////////////////////////////////////////////////////////
// Assigning feature ids
/////////////////////////////////////////////////////////////////////////////

void ClearFeatureIds(const CSeq_annot_EditHandle& annot)
{
    for ( CFeat_CI feat_it(annot); feat_it; ++feat_it ) {
        CSeq_feat_EditHandle feat(*feat_it);
        feat.ClearFeatIds();
        feat.ClearFeatXrefs();
    }
}


void ClearFeatureIds(const CSeq_entry_EditHandle& entry)
{
    for ( CFeat_CI feat_it(entry); feat_it; ++feat_it ) {
        CSeq_feat_EditHandle feat(*feat_it);
        feat.ClearFeatIds();
        feat.ClearFeatXrefs();
    }
}


static void s_SetFeatureId(CFeatTree& ft,
                           const CMappedFeat& feat,
                           int& last_id,
                           const CMappedFeat& parent);
static void s_SetChildrenFeatureIds(CFeatTree& ft,
                                    const CMappedFeat& feat,
                                    int& feat_id);

static void s_SetFeatureId(CFeatTree& ft,
                           const CMappedFeat& feat,
                           int& last_id,
                           const CMappedFeat& parent)
{
    CSeq_feat_EditHandle efeat(feat);
    efeat.SetFeatId(++last_id);

    if ( parent &&
         parent.GetFeatType() == CSeqFeatData::e_Rna  &&
         feat.GetFeatType() == CSeqFeatData::e_Cdregion ) {
        // conservative choice: link only between RNA and Cdregion features
        efeat.AddFeatXref(parent.GetId().GetLocal());
        CSeq_feat_EditHandle parent_efeat(parent);
        parent_efeat.AddFeatXref(last_id);
    }

    s_SetChildrenFeatureIds(ft, feat, last_id);
}


static void s_SetChildrenFeatureIds(CFeatTree& ft,
                                    const CMappedFeat& parent,
                                    int& last_id)
{
    vector<CMappedFeat> children = ft.GetChildren(parent);
    ITERATE (vector<CMappedFeat>, it, children ) {
        s_SetFeatureId(ft, *it, last_id, parent);
    }
}


void ReassignFeatureIds(const CSeq_entry_EditHandle& entry)
{
    ClearFeatureIds(entry);
    int feat_id = 0;
    CFeat_CI feat_it(entry);
    CFeatTree ft(feat_it);
    s_SetChildrenFeatureIds(ft, CMappedFeat(), feat_id);
}


void ReassignFeatureIds(const CSeq_annot_EditHandle& annot)
{
    ClearFeatureIds(annot);
    int feat_id = 0;
    CFeat_CI feat_it(annot);
    CFeatTree ft(feat_it);
    s_SetChildrenFeatureIds(ft, CMappedFeat(), feat_id);
}



END_SCOPE(feature)
END_SCOPE(objects)
END_NCBI_SCOPE
