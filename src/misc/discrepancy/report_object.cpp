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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   class for storing results for Discrepancy Report
 *
 * $Log:$ 
 */

#include <ncbi_pch.hpp>

#include <misc/discrepancy/report_object.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/edit/apply_object.hpp>


BEGIN_NCBI_SCOPE;

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);


void CReportObject::SetText(CScope& scope)
{
    m_Text = GetTextObjectDescription(m_Object, scope);
    if (!NStr::IsBlank(m_Filename)) {
        m_Text = m_Filename + ":" + m_Text;
    }
}


void CReportObject::SetFeatureTable(CScope& scope)
{
    m_FeatureTable = GetFeatureTableObjectDescription(m_Object, scope);
}


void CReportObject::SetXML(CScope& scope)
{
    m_XML = GetXMLObjectDescription(m_Object, scope);
}


void CReportObject::Format(CScope& scope)
{
    SetText(scope);
    SetFeatureTable(scope);
    SetXML(scope);
}


string GetLocusTagForFeature(const CSeq_feat& seq_feat, CScope& scope)
{
    string tag(kEmptyStr);
    if (seq_feat.GetData().IsGene()) {
        const CGene_ref& gene = seq_feat.GetData().GetGene();
        tag =  (gene.CanGetLocus_tag()) ? gene.GetLocus_tag() : kEmptyStr;
    } else {
        const CGene_ref* gene = seq_feat.GetGeneXref();
        if (gene) {
            tag = (gene->CanGetLocus_tag()) ? gene->GetLocus_tag() : kEmptyStr;
        }
        else {
            CConstRef<CSeq_feat> 
            gene= sequence::GetBestOverlappingFeat(seq_feat.GetLocation(),
                                                    CSeqFeatData::e_Gene,
				                    sequence::eOverlap_Contained,
                                                    scope);
            if (gene.NotEmpty()) {
                tag = (gene->GetData().GetGene().CanGetLocus_tag()) ? gene->GetData().GetGene().GetLocus_tag() : kEmptyStr;
            }
        }
    }
  
    return tag;
}


string GetProduct(const CProt_ref& prot_ref)
{
    string rval = "";
    if (prot_ref.CanGetName() && !prot_ref.GetName().empty()) {
        rval = prot_ref.GetName().front();
    }
    return rval;
}


string GetProductForCDS(const CSeq_feat& cds, CScope& scope)
{
    // use protein xref if available
    const CProt_ref* prot = cds.GetProtXref();
    if (prot) {
        return GetProduct(*prot);
    }
    
    // should be the longest protein feature on the bioseq pointed by the cds.GetProduct()
    if (cds.IsSetProduct()) {
        CConstRef <CSeq_feat> prot_seq_feat = sequence::GetBestOverlappingFeat(cds.GetProduct(),
                                                CSeqFeatData::e_Prot, 
                                                sequence::eOverlap_Contains,
                                                scope);
        if (prot_seq_feat && prot_seq_feat->GetData().IsProt()) {
            return GetProduct(prot_seq_feat->GetData().GetProt());
        }
    }
    return (kEmptyStr);
}


void GetSeqFeatLabel(const CSeq_feat& seq_feat, string& label)
{
     label = kEmptyStr;
     
     feature::GetLabel(seq_feat, &label, feature::fFGL_Content);
     size_t pos;
     if (!label.empty() && (string::npos != (pos = label.find("-")) ) ) {
          label = CTempString(label).substr(pos+1);
     }
     string number = "/number=";
     if (!label.empty() 
            && (seq_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_exon 
                   || seq_feat.GetData().GetSubtype() 
                              == CSeqFeatData::eSubtype_intron)
            && (string::npos != (pos = label.find(number)))) {
          label = label.substr(pos + number.size());
          if (label.find("exon") == 0 || label.find("intron") == 0) { // pos
             label = label.substr(0, label.find(' '));
          }
     }
}


CConstRef<CSeq_id> GetBestId(const CBioseq& bioseq, CSeq_id::E_Choice choice)
{
    const CBioseq::TId& seq_id_ls = bioseq.GetId();
    CConstRef<CSeq_id> best_seq_id;
    int best_score = 99999;
    ITERATE (CBioseq::TId, it, seq_id_ls) {
        if (choice == (*it)->Which()) {
            return *it;
        } else if (best_score > (*it)->BaseBestRankScore()) {
            best_seq_id = *it;
            best_score = (*it)->BaseBestRankScore();
        }
    }
    return best_seq_id;  
}


void UpgradeSeqLocId(CSeq_point& pnt, CScope& scope) 
{
    if (pnt.IsSetId()) {
        CBioseq_Handle bsh = scope.GetBioseqHandle(pnt.GetId());
        if (bsh) {
            CConstRef<CSeq_id> best_id = GetBestId(*(bsh.GetCompleteBioseq()), CSeq_id::e_Genbank);
            if (best_id) {
                pnt.SetId().Assign(*best_id);
            }
        }
    }
}


void UpgradeSeqLocId(CSeq_interval& interval, CScope& scope)
{
    if (interval.IsSetId()) {
        CBioseq_Handle bsh = scope.GetBioseqHandle(interval.GetId());
        if (bsh) {
            CConstRef<CSeq_id> best_id = GetBestId(*(bsh.GetCompleteBioseq()), CSeq_id::e_Genbank);
            if (best_id) {
                interval.SetId().Assign(*best_id);
            }
        }
    }
}


void UpgradeSeqLocId(CSeq_loc& loc, CScope& scope)
{
    switch (loc.Which()) {
        case CSeq_loc::e_Bond:
            if (loc.GetBond().IsSetA()) {
                UpgradeSeqLocId(loc.SetBond().SetA(), scope);
            }
            if (loc.GetBond().IsSetB()) {
                UpgradeSeqLocId(loc.SetBond().SetB(), scope);
            }
            break;
        case CSeq_loc::e_Equiv:
            NON_CONST_ITERATE(CSeq_loc::TEquiv::Tdata, it, loc.SetEquiv().Set()) {
                UpgradeSeqLocId(**it, scope);
            }
            break;
        case CSeq_loc::e_Int:
            UpgradeSeqLocId(loc.SetInt(), scope);
            break;

        case CSeq_loc::e_Mix:
            NON_CONST_ITERATE(CSeq_loc::TMix::Tdata, it, loc.SetMix().Set()) {
                UpgradeSeqLocId(**it, scope);
            }
            break;
        case CSeq_loc::e_Packed_int:
            NON_CONST_ITERATE(CSeq_loc::TPacked_int::Tdata, it, loc.SetPacked_int().Set()) {
                UpgradeSeqLocId(**it, scope);
            }
            break;
        case CSeq_loc::e_Packed_pnt:
            if (loc.GetPacked_pnt().IsSetId()) {
                CBioseq_Handle bsh = scope.GetBioseqHandle(loc.GetPacked_pnt().GetId());
                if (bsh) {
                    CConstRef<CSeq_id> best_id = GetBestId(*(bsh.GetCompleteBioseq()), CSeq_id::e_Genbank);
                    if (best_id) {
                        loc.SetPacked_pnt().SetId().Assign(*best_id);
                    }
                }
            }
            break;
        case CSeq_loc::e_Pnt:
            UpgradeSeqLocId(loc.SetPnt(), scope);
            break;
        case CSeq_loc::e_Whole:
            {{
                CBioseq_Handle bsh = scope.GetBioseqHandle(loc.GetPacked_pnt().GetId());
                if (bsh) {
                    CConstRef<CSeq_id> best_id = GetBestId(*(bsh.GetCompleteBioseq()), CSeq_id::e_Genbank);
                    if (best_id) {
                        loc.SetWhole().Assign(*best_id);
                    }
                }
            }}
            break;
        case CSeq_loc::e_Null:
        case CSeq_loc::e_Empty:
        case CSeq_loc::e_Feat:
        case CSeq_loc::e_not_set:
            break;
    }
}


string GetSeqLocDescription(const CSeq_loc& loc, CScope& scope)
{
    string location = "";    

    CRef<CSeq_loc> cpy(new CSeq_loc());
    cpy->Assign(loc);
    UpgradeSeqLocId(*cpy, scope);
    cpy->GetLabel(&location);
    return location;
}


string CReportObject::GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, const string& product)
{
    string location = GetSeqLocDescription(seq_feat.GetLocation(), scope);
    string label = seq_feat.GetData().GetKey();
    string locus_tag = GetLocusTagForFeature (seq_feat, scope);
    string rval = label + "\t" + product + "\t" + location + "\t" + locus_tag + "\n";
    return rval;
}


string CReportObject::GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope)
{
    if ( seq_feat.GetData().IsProt()) {
        CConstRef <CBioseq> bioseq = sequence::GetBioseqFromSeqLoc(seq_feat.GetLocation(), scope).GetCompleteBioseq();
        if (bioseq) {
            const CSeq_feat* cds = sequence::GetCDSForProduct(*bioseq, &scope);
            if (cds) {
                string product = GetProduct(seq_feat.GetData().GetProt());
                return GetTextObjectDescription(*cds, scope, product);
            }
        }
    }

    string location= GetSeqLocDescription(seq_feat.GetLocation(), scope);
   
    string label = seq_feat.GetData().GetKey();
    string locus_tag = GetLocusTagForFeature (seq_feat, scope);
    string context_label(kEmptyStr);
    if (seq_feat.GetData().IsCdregion()) { 
        context_label = GetProductForCDS(seq_feat, scope);
        if (NStr::IsBlank(context_label)) {
            GetSeqFeatLabel(seq_feat, context_label);
        }
    } else if (seq_feat.GetData().IsPub()) {
        seq_feat.GetData().GetPub().GetPub().GetLabel(&context_label);
    } else if (seq_feat.GetData().IsGene()) {
        if (seq_feat.GetData().GetGene().CanGetLocus() &&
            !NStr::IsBlank(seq_feat.GetData().GetGene().GetLocus())) {
            context_label = seq_feat.GetData().GetGene().GetLocus(); 
        } else if (seq_feat.GetData().GetGene().CanGetDesc()) {
            context_label = seq_feat.GetData().GetGene().GetDesc(); 
        }
    } 
    else GetSeqFeatLabel(seq_feat, context_label);


    string rval = label + "\t" + context_label + "\t" + location + "\t" + locus_tag;
    return rval;
}


string CReportObject::GetTextObjectDescription(const CSeqdesc& sd, CScope& scope)
{
    string desc = GetTextObjectDescription(sd);
    CRef<CScope> s(&scope);
    CSeq_entry_Handle seh = edit::GetSeqEntryForSeqdesc (s, sd);

    if (seh) {
        string label;
        if (seh.IsSeq()) {
            seh.GetSeq().GetCompleteBioseq()->GetLabel(&label, CBioseq::eContent);
        } else if (seh.IsSet()) {
            seh.GetSet().GetCompleteBioseq_set()->GetLabel(&label, CBioseq_set::eContent);
        }
        if (!NStr::IsBlank(label)) {
            desc = label + ":" + desc;
        }
    }
    return desc;
}


string CReportObject::GetTextObjectDescription(const CSeqdesc& sd)
{
    string label(kEmptyStr);
    switch (sd.Which()) 
    {
        case CSeqdesc::e_Comment: return (sd.GetComment());
        case CSeqdesc::e_Region: return (sd.GetRegion());
        case CSeqdesc::e_Het: return (sd.GetHet());
        case CSeqdesc::e_Title: return (sd.GetTitle());
        case CSeqdesc::e_Name: return (sd.GetName());
        case CSeqdesc::e_Create_date:
            sd.GetCreate_date().GetDate(&label);
            break;
        case CSeqdesc::e_Update_date:
            sd.GetUpdate_date().GetDate(&label);
            break;
        case CSeqdesc::e_Org:
          {
            const COrg_ref& org = sd.GetOrg();
            label = (org.CanGetTaxname() ? org.GetTaxname() 
                       : (org.CanGetCommon()? org.GetCommon(): kEmptyStr));
          }
          break;
        case CSeqdesc::e_Pub:
            sd.GetPub().GetPub().GetLabel(&label);
            break;
        case CSeqdesc::e_User:
          {
            const CUser_object& uop = sd.GetUser();
            label = (uop.CanGetClass()? uop.GetClass() 
                 : (uop.GetType().IsStr()? uop.GetType().GetStr(): kEmptyStr));
          }
          break;
        case CSeqdesc::e_Method:
            label = (ENUM_METHOD_NAME(EGIBB_method)()->FindName(sd.GetMethod(), true));
            break;
        case CSeqdesc::e_Mol_type:
            label = (ENUM_METHOD_NAME(EGIBB_mol)()->FindName(sd.GetMol_type(), true));
            break;
        case CSeqdesc::e_Modif:
            ITERATE (list <EGIBB_mod>, it, sd.GetModif()) {
                label 
                  += ENUM_METHOD_NAME(EGIBB_mod)()->FindName(*it, true) + ", ";
            }
            label = label.substr(0, label.size()-2);
            break;
        case CSeqdesc::e_Source:
          {
            const COrg_ref& org = sd.GetSource().GetOrg();
            label = (org.CanGetTaxname() ? org.GetTaxname() 
                       : (org.CanGetCommon()? org.GetCommon(): kEmptyStr));
          }  
          break;
        case CSeqdesc::e_Maploc:
            sd.GetMaploc().GetLabel(&label);
            break; 
        case CSeqdesc::e_Molinfo:
            sd.GetMolinfo().GetLabel(&label);
            break;
        case CSeqdesc::e_Dbxref:
            sd.GetDbxref().GetLabel(&label);
            break;
        default:
            break; 
    }
    return label;
}


// label for Bioseq includes "best" ID, plus length, plus number of
// non-ATGC characters (if any), plus number of gaps (if any)
string CReportObject::GetTextObjectDescription(CBioseq_Handle bsh)
{
    string rval;

    CConstRef<CSeq_id> id = GetBestId(*(bsh.GetCompleteBioseq()), CSeq_id::e_Genbank);

    id->GetLabel(&rval, CSeq_id::eContent);

    rval += " (length " + NStr::NumericToString(bsh.GetInst_Length());

    size_t num_other = 0;
    size_t num_gap = 0;
    CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    CSeqVector::const_iterator vi = vec.begin();
    while (vi != vec.end()) {
        if (*vi == 'A' || *vi == 'T' || *vi == 'G' || *vi == 'C') {
            // normal
        } else if (vi.IsInGap()) {
            num_gap++;
        } else {
            num_other++;
        }
        ++vi;
    }

    if (num_other > 0 && bsh.IsNa()) {
        rval += ", " + NStr::NumericToString(num_other) + " other";
    }

    if (num_gap > 0) {
        rval += ", " + NStr::NumericToString(num_gap) + " gap";
    }
    
    rval += ")";
    return rval;
}


string CReportObject::GetTextObjectDescription(CConstRef<CObject> obj, CScope& scope)
{
    string rval = "";
    if (obj) {
        const CBioseq* seq = dynamic_cast<const CBioseq*>(obj.GetPointer());
        const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(obj.GetPointer());
        const CSeqdesc* sd = dynamic_cast<const CSeqdesc*>(obj.GetPointer());
        if (seq) {
            CBioseq_Handle bsh = scope.GetBioseqHandle(*seq);
            rval = GetTextObjectDescription(bsh);
        } else if (feat) {
            rval = GetTextObjectDescription(*feat, scope);
        } else if (sd) {
            rval = GetTextObjectDescription(*sd, scope);
        } else {
            rval = "stub";
        }
    } else {
        rval = "Unable to generate description";
    }

    return rval;
}


string CReportObject::GetFeatureTableObjectDescription(CConstRef<CObject> obj, CScope& scope)
{
    string rval = "feature table format display of object";
    return rval;
}


string CReportObject::GetXMLObjectDescription(CConstRef<CObject> obj, CScope& scope)
{
    string rval = "xml description of object";
    return rval;
}


void CReportObject::DropReference()
{
    m_Object.Reset(NULL);
}


void CReportObject::DropReference(CScope& scope) 
{
    if (!m_Object) {
        return;
    }
    if (NStr::IsBlank(m_Text)) {
        SetText(scope);
    }
    if (NStr::IsBlank(m_FeatureTable)) {
        SetFeatureTable(scope);
    }
    if (NStr::IsBlank(m_XML)) {
        SetXML(scope);
    }
}


bool CReportObject::Equal(const CReportObject& other) const
{
    string filename1 = GetFilename();
    string filename2 = other.GetFilename();

    if (!NStr::Equal(filename1, filename2)) {
        return false;
    }
    
    CConstRef<CObject> o1 = GetObject();
    CConstRef<CObject> o2 = other.GetObject();
    if (o1 || o2) {
        if (o1 && o2 && o1.GetPointer() == o2.GetPointer()) {
            return true;
        } else {
            return false;
        }
    }            

    string cmp1 = GetText();
    string cmp2 = other.GetText();
    if (!NStr::IsBlank(cmp1) || !NStr::IsBlank(cmp2)) {
        return NStr::Equal(cmp1, cmp2);
    }

    cmp1 = GetXML();
    cmp2 = other.GetXML();
    if (!NStr::IsBlank(cmp1) || !NStr::IsBlank(cmp2)) {
        return NStr::Equal(cmp1, cmp2);
    }

    cmp1 = GetFeatureTable();
    cmp2 = other.GetFeatureTable();
    if (!NStr::IsBlank(cmp1) || !NStr::IsBlank(cmp2)) {
        return NStr::Equal(cmp1, cmp2);
    }

    return true;
}


bool CReportObject::AlreadyInList(const TReportObjectList& list, const CReportObject& new_obj)
{
    ITERATE(TReportObjectList, it, list) {
        if ((*it)->Equal(new_obj)) {
            return true;
        }
    }
    return false;
}

END_NCBI_SCOPE
