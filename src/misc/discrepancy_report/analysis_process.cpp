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
 *   class for storing tests for Discrepancy Report
 *
 * $Log:$ 
 */

#include <ncbi_pch.hpp>

#include <misc/discrepancy_report/analysis_process.hpp>
#include <misc/discrepancy_report/src_qual_problem.hpp>
#include <misc/discrepancy_report/missing_genes.hpp>

#include <objects/macro/String_constraint.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE;

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);


CRef<CAnalysisProcess> CAnalysisProcessFactory::Create(const string &test_name)
{
    CRef<CAnalysisProcess> test(new CCountNucSeqs());
    if (test->Handles(test_name)) {
        return test;
    }
    test.Reset(new CSrcQualProblem());
    if (test->Handles(test_name)) {
        return test;
    }
    test.Reset(new CMissingGenes());
    if (test->Handles(test_name)) {
        return test;
    }
    test.Reset(new COverlappingCDS());
    if (test->Handles(test_name)) {
        return test;
    }
    test.Reset(new CContainedCDS());
    if (test->Handles(test_name)) {
        return test;
    }

    return CRef<CAnalysisProcess>(new CUnimplemented(test_name));
}


vector<string> CAnalysisProcessFactory::Implemented()
{
    vector<string> list;

    list.push_back(kMISSING_GENES);
    list.push_back(kDISC_SUPERFLUOUS_GENE);
    list.push_back(kDISC_COUNT_NUCLEOTIDES);
    list.push_back(kDISC_OVERLAPPING_CDS);
    list.push_back(kDISC_CONTAINED_CDS);
    list.push_back(kDISC_SOURCE_QUALS_ASNDISC);
    list.push_back(kDISC_SRC_QUAL_PROBLEM);
    list.push_back(kDISC_MISSING_SRC_QUAL);
    list.push_back(kDISC_DUP_SRC_QUAL);

    return list;
}


bool CAnalysisProcess::Handles(const string& test_name) const
{
    vector<string> list = Handles();
    ITERATE(vector<string>, it, list) {
        if (NStr::EqualNocase(test_name, *it)) {
            return true;
        }
    }
    return false;
}


vector<string> CAnalysisProcess::x_HandlesOne(const string& test_name)
{
    vector<string> list;

    list.push_back(test_name);

    return list;
}

void CAnalysisProcess::x_DropReferences(TReportObjectList& obj_list, CScope& scope)
{
    NON_CONST_ITERATE(TReportObjectList, it, obj_list) {
        (*it)->DropReference(scope);
    }
}


bool CAnalysisProcess::HasLineage(const CBioSource& biosrc, const string& type)
{
   if (NStr::FindNoCase(m_DefaultLineage, type) != string::npos) {
        return true;
   }
   else if (m_DefaultLineage.empty() && biosrc.IsSetLineage()
                && NStr::FindNoCase(biosrc.GetLineage(), type) != string::npos){
      return true;
   }
   else return false;
}


bool CAnalysisProcess::IsEukaryotic(CBioseq_Handle bh)
{
   const CBioSource* 
       biosrc = sequence::GetBioSource(bh);
   if (biosrc) {
      CBioSource :: EGenome genome = (CBioSource::EGenome) biosrc->GetGenome();
      if (genome != CBioSource :: eGenome_mitochondrion
                  && genome != CBioSource :: eGenome_chloroplast
                  && genome != CBioSource :: eGenome_plastid
                  && genome != CBioSource :: eGenome_apicoplast
                  && HasLineage(*biosrc, "Eukaryota")) {
         return true;
     }
   }
   return false;
}


bool CAnalysisProcess::x_StrandsMatch(const CSeq_loc& loc1, const CSeq_loc& loc2)
{
    bool rval = false;

    ENa_strand strand1 = loc1.GetStrand();
    ENa_strand strand2 = loc2.GetStrand();
    if (((strand1 == strand2 && strand1 == eNa_strand_minus) ||
        (strand1 != eNa_strand_minus && strand2 != eNa_strand_minus))) {
        rval = true;
    }
    return rval;
}


string GetProductNameForProteinSequence(CBioseq_Handle bsh)
{
    if (!bsh) {
        return "";
    }
    CFeat_CI prot(bsh, CSeqFeatData::eSubtype_prot);
    if (!prot || !prot->IsSetData() || !prot->GetData().IsProt() ||
        !prot->GetData().GetProt().IsSetName() ||
        prot->GetData().GetProt().GetName().size() < 1) {
        return "";
    }
    return prot->GetData().GetProt().GetName().front();
}


string GetProductForCDS(const CMappedFeat& f, CScope& scope)
{
    if (!f.IsSetData() || !f.GetData().IsCdregion() ||
        !f.IsSetProduct()) {
        return "";
    }
    CBioseq_Handle bsh = scope.GetBioseqHandle(f.GetProduct());
    return GetProductNameForProteinSequence(bsh);
}


void CAnalysisProcess::x_DeleteProteinSequence(CBioseq_Handle prot)
{
    // Should be a protein!
    if ( prot && prot.IsProtein() && !prot.IsRemoved() ) {
        // Get the protein parent set before you remove the protein
        CBioseq_set_Handle bssh = prot.GetParentBioseq_set();

        // Delete the protein
        CBioseq_EditHandle prot_eh(prot);
        prot_eh.Remove();

        // If lone nuc remains, renormalize the nuc-prot set
        if (bssh && bssh.IsSetClass() 
            && bssh.GetClass() == CBioseq_set::eClass_nuc_prot
            && !bssh.IsEmptySeq_set() 
            && bssh.GetBioseq_setCore()->GetSeq_set().size() == 1) 
        {
            // Renormalize the lone nuc that's inside the nuc-prot set into  
            // a nuc bioseq.  This call will remove annots/descrs from the 
            // set and attach them to the seq.
            bssh.GetParentEntry().GetEditHandle().ConvertSetToSeq();
        }
    }
}


bool CAnalysisProcess::x_ConvertCDSToMiscFeat(const CSeq_feat& feat, CScope& scope)
{
    if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature) {
        // already a misc_feat, don't need to convert
        return false;
    }
    if (!feat.GetData().IsCdregion()) {
        // for now, only Cdregion conversions are implemented
        return false;
    }

    CRef<CSeq_feat> replacement(new CSeq_feat());
    replacement->Assign(feat);
    replacement->SetData().SetImp().SetKey("misc_feature");

    if (feat.IsSetProduct()) {
        CBioseq_Handle bsh = scope.GetBioseqHandle(feat.GetProduct());
        string product = GetProductNameForProteinSequence(bsh);
        if (!NStr::IsBlank(product)) {
            if (replacement->IsSetComment() && !NStr::IsBlank(replacement->GetComment())) {
                replacement->SetComment() += "; ";
            }
            replacement->SetComment() += product;
        }
        x_DeleteProteinSequence(bsh);
    }
    
    try {
        CSeq_feat_Handle fh = scope.GetSeq_featHandle(feat);
        // This is necessary, to make sure that we are in "editing mode"
        const CSeq_annot_Handle& annot_handle = fh.GetAnnot();
        CSeq_entry_EditHandle eh = annot_handle.GetParentEntry().GetEditHandle();

        // now actually edit feature
        CSeq_feat_EditHandle feh(fh);
        feh.Replace(*replacement);
    } catch (CException& ex) {
        // feature may have already been removed or converted
        return false;
    }
    return true;
}


TReportItemList CUnimplemented::GetReportItems(CReportConfig& cfg) const
{
    TReportItemList list;
    if (cfg.IsEnabled(m_TestName)) {
        CRef<CReportItem> item(new CReportItem());
        item->SetSettingName(m_TestName);
        item->SetText(m_TestName + " is not implemented");
        list.push_back(item);
    }

    return list;
}


void CSimpleStorageProcess::DropReferences(CScope& scope)
{
    x_DropReferences(m_Objs, scope);
}


void CCountNucSeqs::CollectData(CSeq_entry_Handle seh, const CReportMetadata& metadata)
{
    string filename = metadata.GetFilename();
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        CConstRef<CObject> obj(bi->GetCompleteBioseq().GetPointer());
        CRef<CReportObject> r(new CReportObject(obj));
        r->SetFilename(filename);
        m_Objs.push_back(r);
        ++bi;
    }
}


TReportItemList CCountNucSeqs::GetReportItems(CReportConfig& cfg) const
{
    TReportItemList list;

    if (cfg.IsEnabled(kDISC_COUNT_NUCLEOTIDES)) {
        CRef<CReportItem> item(new CReportItem());
        item->SetSettingName(kDISC_COUNT_NUCLEOTIDES);
        item->SetObjects() = m_Objs;
        item->SetTextWithIs("nucleotide Bioseq", "present");
        list.push_back(item);
    }

    return list;
}


bool COverlappingCDS::x_LocationsOverlapOnSameStrand(const CSeq_loc& loc1, const CSeq_loc& loc2, CScope* scope)
{
    bool rval = false;
    
    if (x_StrandsMatch(loc1, loc2) &&
        sequence::eNoOverlap 
                != sequence::Compare(loc1, loc2, scope),
                sequence::fCompareOverlapping) {
        rval = true;
    }
    return rval;
}


void COverlappingCDS::x_AddFeature(const CMappedFeat& f, const string& filename)
{
    CRef<CReportObject> obj(new CReportObject(CConstRef<CObject>(f.GetSeq_feat().GetPointer())));
    obj->SetFilename(filename);
    if (HasOverlapNote(*(f.GetSeq_feat()))) {
        m_ObjsNote.push_back(obj);
    } else {
        m_ObjsNoNote.push_back(obj);
    }
}


void COverlappingCDS::CollectData(CSeq_entry_Handle seh, const CReportMetadata& metadata)
{
    string filename = metadata.GetFilename();
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        CFeat_CI f(*bi, SAnnotSelector(CSeqFeatData::e_Cdregion));
        
        while (f) {
            string product1 = GetProductForCDS(*f, seh.GetScope());
            bool added_this = false;
            CFeat_CI f2 = f;
            ++f2;
            while (f2) {
                if (x_LocationsOverlapOnSameStrand(f->GetLocation(), f2->GetLocation(), &(seh.GetScope()))) {
                    string product2 = GetProductForCDS(*f2, seh.GetScope());
                    if (x_ProductNamesAreSimilar(product1, product2)) {
                        if (!added_this) {
                            if (!x_ShouldIgnore(product1)) {
                                x_AddFeature(*f, filename);
                            }
                            added_this = true;
                        }
                        if (!x_ShouldIgnore(product2)) {
                            x_AddFeature(*f2, filename);
                        }
                    }
                }

                ++f2;
            }
            ++f;
        }
        ++bi;
    }
}


static const string kSimilarProductWords[] = 
{ "transposase",
  "integrase"
};

static const int kNumSimilarProductWords = sizeof (kSimilarProductWords) / sizeof (string);

static const string kIgnoreSimilarProductWords[] = 
{ "hypothetical protein",
  "phage",
  "predicted protein"
};

static const int kNumIgnoreSimilarProductWords = sizeof (kIgnoreSimilarProductWords) / sizeof (string);

bool COverlappingCDS::x_ProductNamesAreSimilar(const string& product1, const string& product2)
{
    bool str1_has_similarity_word = false, str2_has_similarity_word = false;

    size_t i;
    /* if both product names contain one of the special case similarity words,
    * the product names are similar. */
  
    for (i = 0; i < kNumSimilarProductWords; i++) {
        if (string::npos != NStr::FindNoCase(product1, kSimilarProductWords[i])) {
            str1_has_similarity_word = true;
        }

        if (string::npos != NStr::FindNoCase(product2, kSimilarProductWords[i])) {
            str2_has_similarity_word = true;
        }
    }
    if (str1_has_similarity_word && str2_has_similarity_word) {
        return true;
    }
  
    /* otherwise, if one of the product names contains one of special ignore similarity
    * words, the product names are not similar.
    */
    for (i = 0; i < kNumIgnoreSimilarProductWords; i++) {
        if (string::npos != NStr::FindNoCase(product1, kSimilarProductWords[i])
            || string::npos 
                != NStr::FindNoCase(product2, kSimilarProductWords[i])) {
            return false;
        }
    }

    if (!(NStr::CompareNocase(product1, product2))) {
        return true;
    } else {
        return false;
    }

}


bool COverlappingCDS::x_ShouldIgnore(const string& product)
{
    if (NStr::Find(product, "transposon") != string::npos ||
        NStr::Find(product, "transposase") != string::npos) {
        return true;
    }
    CString_constraint constraint;
    constraint.SetMatch_text("ABC");
    constraint.SetCase_sensitive(true);
    constraint.SetWhole_word(true);
    if (constraint.Match(product)) {
        return true;
    } else {
        return false;
    }
}


const string kOverlapSimilar = "another coding region with a similar or identical name";

TReportItemList COverlappingCDS::GetReportItems(CReportConfig& cfg) const
{
    TReportItemList list;

    if (cfg.IsEnabled(kDISC_OVERLAPPING_CDS) && 
        (m_ObjsNote.size() > 0 || m_ObjsNoNote.size() > 0)) {
        CRef<CReportItem> item(new CReportItem());
        item->SetSettingName(kDISC_OVERLAPPING_CDS);
        item->SetObjects().insert(item->SetObjects().end(), m_ObjsNote.begin(), m_ObjsNote.end());
        item->SetObjects().insert(item->SetObjects().end(), m_ObjsNoNote.begin(), m_ObjsNoNote.end());
        item->SetTextWithSimpleVerb("coding region", "overlap", kOverlapSimilar + ".");

        if (m_ObjsNote.size() > 0) {
            CRef<CReportItem> sub(new CReportItem());
            sub->SetSettingName(kDISC_OVERLAPPING_CDS);
            sub->SetObjects() = m_ObjsNote;
            sub->SetTextWithSimpleVerb("coding region", "overlap",
                                       kOverlapSimilar + " but have the appropriate note text.");
            item->SetSubitems().push_back(sub);
        }
        if (m_ObjsNoNote.size() > 0) {
            CRef<CReportItem> sub(new CReportItem());
            sub->SetSettingName(kDISC_OVERLAPPING_CDS);
            sub->SetObjects() = m_ObjsNoNote;
            sub->SetTextWithSimpleVerb("coding region", "overlap",
                                       kOverlapSimilar + " that do not have the appropriate note text.");
            item->SetSubitems().push_back(sub);
        }
        list.push_back(item);
    }

    return list;
}


bool COverlappingCDS::Autofix(CScope& scope)
{
    bool rval = false;
    if (m_ObjsNoNote.size() > 0) {
        NON_CONST_ITERATE(TReportObjectList, it, m_ObjsNoNote) {
            const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>((*it)->GetObject().GetPointer());
            if (sf) {
                CRef<CSeq_feat> new_feat(new CSeq_feat());
                new_feat->Assign(*sf);
                if (SetOverlapNote(*new_feat)) {
                    CSeq_feat_Handle fh = scope.GetSeq_featHandle(*sf);
                    // This is necessary, to make sure that we are in "editing mode"
                    const CSeq_annot_Handle& annot_handle = fh.GetAnnot();
                    CSeq_entry_EditHandle eh = annot_handle.GetParentEntry().GetEditHandle();

                    // now actually edit feature
                    CSeq_feat_EditHandle feh(fh);
                    feh.Replace(*new_feat);
                    rval = true;
                }
            }
        }        
    }
    return rval;
}


const string kOverlappingCDSNoteText = "overlaps another CDS with the same product name";


bool COverlappingCDS::HasOverlapNote(const CSeq_feat& feat)
{
    if (feat.IsSetComment() && NStr::Find(feat.GetComment(), kOverlappingCDSNoteText) != string::npos) {
        return true;
    } else {
        return false;
    }
}


bool COverlappingCDS::SetOverlapNote(CSeq_feat& feat)
{
    if (feat.IsSetComment()) {
        if (NStr::Find(feat.GetComment(), kOverlappingCDSNoteText) != string::npos) {
            return false;
        }
        if (!NStr::EndsWith(feat.GetComment(), ";")) {
            feat.SetComment() += "; ";
        }
    }
    feat.SetComment() += kOverlappingCDSNoteText;
    return true;
}


void CContainedCDS::CollectData(CSeq_entry_Handle seh, const CReportMetadata& metadata)
{
    string filename = metadata.GetFilename();
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        if (IsEukaryotic(*bi)) {
            // ignore this BioSeq
        } else {
            CFeat_CI f(*bi, SAnnotSelector(CSeqFeatData::e_Cdregion));
        
            while (f) {
                CFeat_CI f2 = f;
                ++f2;
                while (f2) {
                    sequence::ECompare compare = 
                        sequence::Compare(f->GetLocation(),
                                          f2->GetLocation(),
                                          &(seh.GetScope()), sequence::fCompareOverlapping);
                    if (compare == sequence::eContains || 
                        compare == sequence::eSame || 
                        compare == sequence::eContained) {
                        bool same_strand = x_StrandsMatch(f->GetLocation(), f2->GetLocation());
                        x_AddFeature(*f, metadata.GetFilename(), same_strand);
                        x_AddFeature(*f2, metadata.GetFilename(), same_strand);
                    }
                    ++f2;
                }
                ++f;
            }
        }
        ++bi;
    }
}


TReportItemList CContainedCDS::GetReportItems(CReportConfig& cfg) const
{
    TReportItemList list;

    if (!cfg.IsEnabled(kDISC_CONTAINED_CDS) ) {
        return list;
    }

    size_t num_lists = 0;
    if (m_ObjsNote.size() > 0) {
        num_lists++;
    }
    if (m_ObjsSameStrand.size() > 0) {
        num_lists++;
    }
    if (m_ObjsDiffStrand.size() > 0) {
        num_lists++;
    }
    if (num_lists > 0) {
        // if only one kind of containment is found, add subcategory directly to list

        CRef<CReportItem> item;
        if (num_lists > 1) {
            item.Reset(new CReportItem());
            item->SetSettingName(kDISC_CONTAINED_CDS);
            item->SetObjects().insert(item->SetObjects().end(), m_ObjsNote.begin(), m_ObjsNote.end());
            item->SetObjects().insert(item->SetObjects().end(), m_ObjsSameStrand.begin(), m_ObjsSameStrand.end());
            item->SetObjects().insert(item->SetObjects().end(), m_ObjsDiffStrand.begin(), m_ObjsDiffStrand.end());
            item->SetTextWithIs("coding region", "completely contained in another coding region.");
            list.push_back(item);
        }

        if (m_ObjsNote.size() > 0) {
            CRef<CReportItem> sub(new CReportItem());
            sub->SetSettingName(kDISC_CONTAINED_CDS);
            sub->SetObjects() = m_ObjsNote;
            sub->SetTextWithIs("coding region", "completely contained in another coding region but have note.");
            if (item) {
                item->SetSubitems().push_back(sub);
            } else {
                list.push_back(sub);
            }
        }
        if (m_ObjsSameStrand.size() > 0) {
            CRef<CReportItem> sub(new CReportItem());
            sub->SetSettingName(kDISC_CONTAINED_CDS);
            sub->SetObjects() = m_ObjsSameStrand;
            sub->SetTextWithIs("coding region", "completely contained in another coding region on the same strand.");
            if (item) {
                item->SetSubitems().push_back(sub);
            } else {
                list.push_back(sub);
            }
        }
        if (m_ObjsDiffStrand.size() > 0) {
            CRef<CReportItem> sub(new CReportItem());
            sub->SetSettingName(kDISC_CONTAINED_CDS);
            sub->SetObjects() = m_ObjsDiffStrand;
            sub->SetTextWithIs("coding region", "completely contained in another coding region, but on the opposite strand");
            if (item) {
                item->SetSubitems().push_back(sub);
            } else {
                list.push_back(sub);
            }
        }
    }

    return list;
}


bool CContainedCDS::HasContainedNote(const CSeq_feat& feat)
{
    if (feat.IsSetComment() &&
        NStr::EqualNocase(feat.GetComment(), "completely contained in another CDS")) {
        return true;
    } else {
        return false;
    }
}


void CContainedCDS::x_AddFeature(const CMappedFeat& f, const string& filename, bool same_strand)
{
    CRef<CReportObject> obj(new CReportObject(CConstRef<CObject>(f.GetSeq_feat().GetPointer())));
    obj->SetFilename(filename);
    if (HasContainedNote(*(f.GetSeq_feat()))) {
        if (!CReportObject::AlreadyInList(m_ObjsNote, *obj)) {
            m_ObjsNote.push_back(obj);
        }
    } else if (same_strand) {
        if (!CReportObject::AlreadyInList(m_ObjsSameStrand, *obj)) {
            m_ObjsSameStrand.push_back(obj);
        }
    } else {
        if (!CReportObject::AlreadyInList(m_ObjsDiffStrand, *obj)) {
            m_ObjsDiffStrand.push_back(obj);
        }
    }
}


bool CContainedCDS::Autofix(CScope& scope)
{
    bool rval = false;

    TReportObjectList list_all;
    list_all.insert(list_all.end(), m_ObjsNote.begin(), m_ObjsNote.end());
    list_all.insert(list_all.end(), m_ObjsSameStrand.begin(), m_ObjsSameStrand.end());
    list_all.insert(list_all.end(), m_ObjsDiffStrand.begin(), m_ObjsDiffStrand.end());

    TReportObjectList::iterator it1 = list_all.begin();
    while (it1 != list_all.end()) {
        const CSeq_feat* f1 = dynamic_cast<const CSeq_feat*>((*it1)->GetObject().GetPointer());
        TReportObjectList::const_iterator it2 = it1;
        ++it2;
        bool remove_f1 = false;
        while (it2 != list_all.end()) {
            bool remove_f2 = false;
            const CSeq_feat* f2 = dynamic_cast<const CSeq_feat*>((*it2)->GetObject().GetPointer());
            sequence::ECompare compare = 
                    sequence::Compare(f1->GetLocation(),
                                        f2->GetLocation(),
                                        &scope, sequence::fCompareOverlapping);
            if (compare == sequence::eContains) {
                // convert f2 to misc_feat
                if (x_ConvertCDSToMiscFeat(*f2, scope)) {
                    rval = true;
                    remove_f2 = true;
                }
            } else if (compare == sequence::eContained) {
                // convert f1 to misc_feat
                if (x_ConvertCDSToMiscFeat(*f1, scope)) {
                    rval = true;
                    remove_f1 = true;
                }
            }
            if (remove_f1) {
                break;
            }
            if (remove_f2) {
                it2 = list_all.erase(it2);
            } else {
                ++it2;
            }
        }
        if (remove_f1) {
            it1 = list_all.erase(it1);
        } else {
            ++it1;
        }
    }

    return rval;
}

END_NCBI_SCOPE
