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
 * Authors: Sema
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/macro/String_constraint.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(discrepancy_case);


// COUNT_NUCLEOTIDES

DISCREPANCY_CASE(COUNT_NUCLEOTIDES, CSeq_inst, eAll, "Count nucleotide sequences")
{
    CSeq_inst::TMol mol = obj.GetMol();
    if (mol != CSeq_inst::eMol_dna && mol != CSeq_inst::eMol_rna && mol != CSeq_inst::eMol_na) {
        return;
    }
    m_Objs["[n] nucleotide Bioseq[s] [is] present"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
}


DISCREPANCY_SUMMARIZE(COUNT_NUCLEOTIDES)
{
    m_Objs["[n] nucleotide Bioseq[s] [is] present"]; // If no sequences found still report 0
    m_ReportItems = m_Objs.Export(GetName())->GetSubitems();
}


// COUNT_PROTEINS

DISCREPANCY_CASE(COUNT_PROTEINS, CSeq_inst, eAll, "Count Proteins")
{
    if (obj.GetMol() != CSeq_inst::eMol_aa) {
        return;
    }
    m_Objs["[n] protein sequence[s] [is] present"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
}


DISCREPANCY_SUMMARIZE(COUNT_PROTEINS)
{
    m_Objs["[n] protein sequence[s] [is] present"]; // If no sequences found still report 0
    m_ReportItems = m_Objs.Export(GetName())->GetSubitems();
}


// COUNT_TRNAS
// (also report extra and missing tRNAs)

struct DesiredAAData
{
    char short_symbol;
    string long_symbol;
    size_t num_expected;
};

static const DesiredAAData desired_aaList [] = {
    {'A', "Ala", 1 },
    {'B', "Asx", 0 },
    {'C', "Cys", 1 },
    {'D', "Asp", 1 },
    {'E', "Glu", 1 },
    {'F', "Phe", 1 },
    {'G', "Gly", 1 },
    {'H', "His", 1 },
    {'I', "Ile", 1 },
    {'J', "Xle", 0 },
    {'K', "Lys", 1 },
    {'L', "Leu", 2 },
    {'M', "Met", 1 },
    {'N', "Asn", 1 },
    {'P', "Pro", 1 },
    {'Q', "Gln", 1 },
    {'R', "Arg", 1 },
    {'S', "Ser", 2 },
    {'T', "Thr", 1 },
    {'V', "Val", 1 },
    {'W', "Trp", 1 },
    {'X', "Xxx", 0 },
    {'Y', "Tyr", 1 },
    {'Z', "Glx", 0 },
    {'U', "Sec", 0 },
    {'O', "Pyl", 0 },
    {'*', "Ter", 0 }
};


DISCREPANCY_CASE(COUNT_TRNAS, CSeqFeatData, eNormal, "Count tRNAs")
{
    if (obj.GetSubtype() != CSeqFeatData::eSubtype_tRNA) {
        return;
    }
    CBioSource::TGenome genome = context.GetCurrentGenome();
    if (genome != CBioSource::eGenome_mitochondrion && genome != CBioSource::eGenome_chloroplast && genome != CBioSource::eGenome_plastid) return;

    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        Summarize();
        m_Objs[kEmptyStr].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }

    string aa;
    feature::GetLabel(*context.GetCurrentSeq_feat(), &aa, feature::fFGL_Content);
    size_t n = aa.find_last_of('-');            // cut off the "tRNA-" prefix
    if (n != string::npos) {
        aa = aa.substr(n+1); // is there any better way to get the aminoacid name?
    }
    m_Objs[aa].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef()), false);
}


DISCREPANCY_SUMMARIZE(COUNT_TRNAS)
{
    if (m_Objs.empty()) {
        return;
    }

    static map<string, int> DesiredCount;
    if (DesiredCount.empty()) {
        for (size_t i = 0; i < sizeof(desired_aaList)/sizeof(desired_aaList[0]); i++) {
            DesiredCount[desired_aaList[i].long_symbol] = desired_aaList[i].num_expected;
        }
    }

    CRef<CReportObj> bioseq = m_Objs[kEmptyStr].GetObjects()[0];
    string short_name = bioseq->GetShort();
    m_Objs[kEmptyStr].clear();

    size_t total = 0;
    // count tRNAs
    CReportNode::TNodeMap& map = m_Objs.GetMap();
    NON_CONST_ITERATE (CReportNode::TNodeMap, it, map) {
        if (!NStr::IsBlank(it->first)) {
            total += it->second->GetObjects().size();
        }
    }

    CNcbiOstrstream ss;
    ss << " sequence " << short_name << " has " << total << " tRNA feature" << (total==1 ? kEmptyStr : "s");
    m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(*bioseq);

    // extra tRNAs
    for (size_t i = 0; i < sizeof(desired_aaList)/sizeof(desired_aaList[0]); i++) {
        const size_t n = m_Objs[desired_aaList[i].long_symbol].GetObjects().size();
        if (n <= desired_aaList[i].num_expected) {
            continue;
        }
        CNcbiOstrstream ss;
        ss << "sequence " << short_name << " has " << n << " trna-" << desired_aaList[i].long_symbol << " feature" << (n==1 ? kEmptyStr : "s");
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(*bioseq);
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(m_Objs[desired_aaList[i].long_symbol].GetObjects(), false);
    }
    NON_CONST_ITERATE (CReportNode::TNodeMap, it, map) {
        if (NStr::IsBlank(it->first) || DesiredCount.find(it->first) != DesiredCount.end()) {
            continue;
        }
        CNcbiOstrstream ss;
        ss << "sequence " << short_name << " has " << it->second->GetObjects().size() << " trna-" << it->first << " feature" << (it->second->GetObjects().size() == 1 ? kEmptyStr : "s");
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(*bioseq);
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(m_Objs[it->first].GetObjects(), false);
    }

    // missing tRNAs
    for (size_t i = 0; i < sizeof(desired_aaList)/sizeof(desired_aaList[0]); i++) {
        if (!desired_aaList[i].num_expected) {
            continue;
        }
        const size_t n = m_Objs[desired_aaList[i].long_symbol].GetObjects().size();
        if (n >= desired_aaList[i].num_expected) {
            continue;
        }
        CNcbiOstrstream ss;
        ss << "sequence " << short_name << " is missing trna-" << desired_aaList[i].long_symbol;
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(*bioseq);
    }

    m_ReportItems = m_Objs[kEmptyStr].Export(GetName(), false)->GetSubitems();
    m_Objs.clear();
}


DISCREPANCY_ALIAS(COUNT_TRNAS, FIND_DUP_TRNAS);


// COUNT_RRNAS

DISCREPANCY_CASE(COUNT_RRNAS, CSeqFeatData, eNormal, "Count rRNAs")
{
    if (obj.GetSubtype() != CSeqFeatData::eSubtype_rRNA) {
        return;
    }
    CBioSource::TGenome genome = context.GetCurrentGenome();
    if (genome != CBioSource::eGenome_mitochondrion && genome != CBioSource::eGenome_chloroplast && genome != CBioSource::eGenome_plastid) return;

    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        Summarize();
        m_Objs[kEmptyStr].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }

    string aa;
    feature::GetLabel(*context.GetCurrentSeq_feat(), &aa, feature::fFGL_Content);
    size_t n = aa.find_last_of('-');            // cut off the "rRNA-" prefix
    if (n != string::npos) {
        aa = aa.substr(n+1); // is there any better way to get the aminoacid name?
    }
    m_Objs[aa].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef()), false);
}


DISCREPANCY_SUMMARIZE(COUNT_RRNAS)
{
    if (m_Objs.empty()) {
        return;
    }

    CRef<CReportObj> bioseq = m_Objs[kEmptyStr].GetObjects()[0];
    string short_name = bioseq->GetShort();
    m_Objs[kEmptyStr].clear();

    size_t total = 0;
    // count rRNAs
    CReportNode::TNodeMap& map = m_Objs.GetMap();
    NON_CONST_ITERATE (CReportNode::TNodeMap, it, map) {
        if (!NStr::IsBlank(it->first)) {
            total += it->second->GetObjects().size();
        }
    }
    CNcbiOstrstream ss;
    ss << " sequence " << short_name << " has " << total << " rRNA feature" << (total==1 ? kEmptyStr : "s");
    m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(*bioseq);

    // duplicated rRNA names
    NON_CONST_ITERATE (CReportNode::TNodeMap, it, map) {
        if (NStr::IsBlank(it->first) || it->second->GetObjects().size() <= 1) {
            continue;
        }
        CNcbiOstrstream ss;
        ss << it->second->GetObjects().size() << " rRNA features on " << short_name << " have the same name (" << it->first << ")";
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(*bioseq);
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(m_Objs[it->first].GetObjects(), false);
    }

    m_ReportItems = m_Objs[kEmptyStr].Export(GetName(), false)->GetSubitems();
    m_Objs.clear();
}


DISCREPANCY_ALIAS(COUNT_RRNAS, FIND_DUP_RRNAS);


// OVERLAPPING_CDS

static bool StrandsMatch(const CSeq_loc& loc1, const CSeq_loc& loc2)    //
{
    ENa_strand strand1 = loc1.GetStrand();
    ENa_strand strand2 = loc2.GetStrand();
    return (strand1 == eNa_strand_minus && strand2 == eNa_strand_minus) || (strand1 != eNa_strand_minus && strand2 != eNa_strand_minus);
}


static string GetProductName(const CProt_ref& prot)
{
    string prot_nm(kEmptyStr);
    if (prot.IsSetName() && prot.GetName().size() > 0) {
        prot_nm = prot.GetName().front();
    }
    return prot_nm;
}


static string GetProductName(const CSeq_feat& cds, CScope& scope)
{
    string prot_nm(kEmptyStr);
    if (cds.IsSetProduct()) {
        CBioseq_Handle prot_bsq = sequence::GetBioseqFromSeqLoc(cds.GetProduct(), scope);

        if (prot_bsq) {
            CFeat_CI prot_ci(prot_bsq, CSeqFeatData::e_Prot);
            if (prot_ci) {
                prot_nm = GetProductName(prot_ci->GetOriginalFeature().GetData().GetProt());
            }
        }
    }
    else if (cds.IsSetXref()) {
        ITERATE(CSeq_feat::TXref, it, cds.GetXref()) {
            if ((*it)->IsSetData() && (*it)->GetData().IsProt()) {
                prot_nm = GetProductName((*it)->GetData().GetProt());
            }
        }
    }
    return prot_nm;

}


static const string kSimilarProductWords[] = { "transposase", "integrase" };

static const int kNumSimilarProductWords = sizeof (kSimilarProductWords) / sizeof (string);

static const string kIgnoreSimilarProductWords[] = { "hypothetical protein", "phage", "predicted protein" };

static const int kNumIgnoreSimilarProductWords = sizeof (kIgnoreSimilarProductWords) / sizeof (string);


static bool ProductNamesAreSimilar(const string& product1, const string& product2)
{
    bool str1_has_similarity_word = false, str2_has_similarity_word = false;

    size_t i;
    // if both product names contain one of the special case similarity words, the product names are similar.
  
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
  
    // otherwise, if one of the product names contains one of special ignore similarity
    // words, the product names are not similar.

    for (i = 0; i < kNumIgnoreSimilarProductWords; i++) {
        if (string::npos != NStr::FindNoCase(product1, kIgnoreSimilarProductWords[i]) || string::npos != NStr::FindNoCase(product2, kIgnoreSimilarProductWords[i])) {
            return false;
        }
    }

    return !NStr::CompareNocase(product1, product2);
}


static bool ShouldIgnore(const string& product)
{
    if (NStr::Find(product, "transposon") != string::npos || NStr::Find(product, "transposase") != string::npos) {
        return true;
    }
    CString_constraint constraint;
    constraint.SetMatch_text("ABC");
    constraint.SetCase_sensitive(true);
    constraint.SetWhole_word(true);
    return constraint.Match(product);
}


static bool LocationsOverlapOnSameStrand(const CSeq_loc& loc1, const CSeq_loc& loc2, CScope* scope)
{
    return StrandsMatch(loc1, loc2) && sequence::Compare(loc1, loc2, scope) != sequence::eNoOverlap;
}


static const string kOverlappingCDSNoteText = "overlaps another CDS with the same product name";


static bool HasOverlapNote(const CSeq_feat& feat)
{
    return feat.IsSetComment() && NStr::Find(feat.GetComment(), kOverlappingCDSNoteText) != string::npos;
}


static bool SetOverlapNote(CSeq_feat& feat)
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


DISCREPANCY_CASE(OVERLAPPING_CDS, CSeqFeatData, eNormal, "Overlapping CDs")
{
    static const char* kOverlap0 = "[n] coding region[s] overlap[S] another coding region with a similar or identical name.";
    static const char* kOverlap1 = "[n] coding region[s] overlap[S] another coding region with a similar or identical name, but [has] the appropriate note text.";
    static const char* kOverlap2 = "[n] coding region[s] overlap[S] another coding region with a similar or identical name and [does] not have the appropriate note text.";

    if (obj.Which() != CSeqFeatData::e_Cdregion) {
        return;
    }
    if (!context.GetCurrentBioseq()->CanGetInst() || !context.GetCurrentBioseq()->GetInst().IsNa()) {
        return;
    }
    string product = GetProductName(*context.GetCurrentSeq_feat(), context.GetScope());
    if (product.empty() || ShouldIgnore(product)) {
        return;
    }
    const CSeq_loc& location = context.GetCurrentSeq_feat()->GetLocation();

    if (m_Count != context.GetCountBioseq()) {  // cleanup temporary data
        m_Count = context.GetCountBioseq();
        CReportNode tmp = m_Objs[kEmptyStr];    // keep the test results
        m_Objs.clear();
        m_Objs[kEmptyStr] = tmp;
    }

    CReportNode::TNodeMap& map = m_Objs.GetMap();
    NON_CONST_ITERATE(CReportNode::TNodeMap, it, map) {
        if (it->first.empty() || !ProductNamesAreSimilar(it->first, product)) {
            continue;
        }
        TReportObjectList& list = it->second->GetObjects();
        NON_CONST_ITERATE(TReportObjectList, robj, list) {
            CConstRef<CSeq_feat> sf((CSeq_feat*)&*(*robj)->GetObject());
            const CSeq_loc& loc = sf->GetLocation();
            if (!LocationsOverlapOnSameStrand(location, loc, &context.GetScope())) {
                continue;
            }
            bool has_note = HasOverlapNote(*sf);
            m_Objs[kEmptyStr][kOverlap0][has_note ? kOverlap1 : kOverlap2].Add(*new CDiscrepancyObject(sf, context.GetScope(), context.GetFile(), context.GetKeepRef(), !has_note));
            has_note = HasOverlapNote(*context.GetCurrentSeq_feat());
            m_Objs[kEmptyStr][kOverlap0][has_note ? kOverlap1 : kOverlap2].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), !has_note));
        }
    }
    m_Objs[product].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), true));
}


DISCREPANCY_SUMMARIZE(OVERLAPPING_CDS)
{
    m_ReportItems = m_Objs[kEmptyStr].Export(GetName())->GetSubitems();
}


DISCREPANCY_AUTOFIX(OVERLAPPING_CDS)
{

}

/*
DISCREPANCY_CASE(OVERLAPPING_CDS, TReportObjectList m_ObjsNoNote)
{
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        CFeat_CI f(*bi, SAnnotSelector(CSeqFeatData::e_Cdregion));
        
        while (f) {
            string product1 = GetProductForCDS(*f, seh.GetScope());
            bool added_this = false;
            CFeat_CI f2 = f;
            ++f2;
            while (f2) {
                if (LocationsOverlapOnSameStrand(f->GetLocation(), f2->GetLocation(), &(seh.GetScope()))) {
                    string product2 = GetProductForCDS(*f2, seh.GetScope());
                    if (ProductNamesAreSimilar(product1, product2)) {
                        if (!added_this) {
                            if (!ShouldIgnore(product1)) {
                                CConstRef<CObject> obj(f->GetSeq_feat().GetPointer());
                                CRef<CDiscrepancyObject> r(new CDiscrepancyObject(obj, bi->GetScope(), context.m_File, context.m_KeepRef));
                                Add(HasOverlapNote(*(f->GetSeq_feat())) ? m_Objs : m_ObjsNoNote, r);
                            }
                            added_this = true;
                        }
                        if (!ShouldIgnore(product2)) {
                            CConstRef<CObject> obj(f2->GetSeq_feat().GetPointer());
                            CRef<CDiscrepancyObject> r(new CDiscrepancyObject(obj, bi->GetScope(), context.m_File, context.m_KeepRef));
                            Add(HasOverlapNote(*(f2->GetSeq_feat())) ? m_Objs : m_ObjsNoNote, r);
                        }
                    }
                }
                ++f2;
            }
            ++f;
        }
        ++bi;
    }

    size_t n = m_Objs.size() + m_ObjsNoNote.size();
    if (!n) return false;

    m_ReportItems.clear();
    CNcbiOstrstream ss;
    ss << n << " coding region" << (n==1 ? " overlaps" : "s overlap") << " another coding region with a similar or identical name";
    CRef<CDiscrepancyItem> item(new CDiscrepancyItem(GetName(), CNcbiOstrstreamToString(ss)));
    TReportObjectList Objs(m_Objs);
    copy(m_ObjsNoNote.begin(), m_ObjsNoNote.end(), back_inserter(Objs));
    item->SetDetails(Objs);
    m_ReportItems.push_back(CRef<CReportItem>(item.Release()));

    return true;
}
*/
/*
DISCREPANCY_AUTOFIX(OVERLAPPING_CDS)
{
    bool ret = false;
    NON_CONST_ITERATE(TReportObjectList, it, m_ObjsNoNote) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
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
                ret = true;
            }
        }
    }
    return ret;
}
*/

// CONTAINED_CDS

static bool HasLineage(const CBioSource& biosrc, const string& def_lineage, const string& type)
{
    return NStr::FindNoCase(def_lineage, type) != string::npos
        || def_lineage.empty() && biosrc.IsSetLineage() && NStr::FindNoCase(biosrc.GetLineage(), type) != string::npos;
}


static bool IsEukaryotic(const CBioSource* biosrc, const string& def_lineage)
{
    if (biosrc) {
        CBioSource :: EGenome genome = (CBioSource::EGenome) biosrc->GetGenome();
        if (genome != CBioSource :: eGenome_mitochondrion
            && genome != CBioSource :: eGenome_chloroplast
            && genome != CBioSource :: eGenome_plastid
            && genome != CBioSource :: eGenome_apicoplast
            && HasLineage(*biosrc, def_lineage, "Eukaryota")) {
            return true;
        }
    }
    return false;
}


static bool HasContainedNote(const CSeq_feat& feat)
{
    return feat.IsSetComment() && NStr::EqualNocase(feat.GetComment(), "completely contained in another CDS");
}


static void DeleteProteinSequence(CBioseq_Handle prot)
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

/*
static bool ConvertCDSToMiscFeat(const CSeq_feat& feat, CScope& scope)
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
        DeleteProteinSequence(bsh);
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
*/

DISCREPANCY_CASE(CONTAINED_CDS, CSeqFeatData, eNormal, "Contained CDs")
{
    static const char* kContained = "[n] coding region[s] completely contained in another coding region.";
    static const char* kContainedNote = "[n] coding region[s] completely contained in another coding region, but have note.";
    static const char* kContainedSame = "[n] coding region[s] completely contained in another coding region on the same strand.";
    static const char* kContainedOpps = "[n] coding region[s] completely contained in another coding region, but on the opposite strand.";

    if (obj.Which() != CSeqFeatData::e_Cdregion) {
        return;
    }
    if (!context.GetCurrentBioseq()->CanGetInst() || !context.GetCurrentBioseq()->GetInst().IsNa() || !IsEukaryotic(context.GetCurrentBiosource(), context.GetLineage())) {
        return;
    }

    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        m_Objs["tmp"].clear();
    }

    const CSeq_loc& location = context.GetCurrentSeq_feat()->GetLocation();
    TReportObjectList& list = m_Objs["tmp"].GetObjects();
    NON_CONST_ITERATE(TReportObjectList, robj, list) {
        CConstRef<CSeq_feat> sf((CSeq_feat*)&*(*robj)->GetObject());
        const CSeq_loc& loc = sf->GetLocation();
        sequence::ECompare compare = sequence::Compare(loc, location, &context.GetScope(), sequence::fCompareOverlapping);
        if (compare == sequence::eContains || compare == sequence::eSame || compare == sequence::eContained) {
            const char* strand = StrandsMatch(loc, location) ? kContainedSame : kContainedOpps;
            m_Objs[kEmptyStr][kContained][HasContainedNote(*sf) ? kContainedNote : strand].Add(*new CDiscrepancyObject(sf, context.GetScope(), context.GetFile(), context.GetKeepRef()));
            m_Objs[kEmptyStr][kContained][HasContainedNote(*context.GetCurrentSeq_feat()) ? kContainedNote : strand].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
        }
    }
    m_Objs["tmp"].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), true));
}


DISCREPANCY_SUMMARIZE(CONTAINED_CDS)
{
    m_ReportItems = m_Objs[kEmptyStr].Export(GetName())->GetSubitems();
}

/*
DISCREPANCY_CASE(CONTAINED_CDS, TReportObjectList m_ObjsSameStrand; TReportObjectList m_ObjsDiffStrand)
{
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        if (IsEukaryotic(*bi, context.m_Lineage)) {
            // ignore this BioSeq
        } else {
            CFeat_CI f(*bi, SAnnotSelector(CSeqFeatData::e_Cdregion));
        
            while (f) {
                CFeat_CI f2 = f;
                ++f2;
                while (f2) {
                    sequence::ECompare compare = sequence::Compare(f->GetLocation(), f2->GetLocation(), &(seh.GetScope()), sequence::fCompareOverlapping);
                    if (compare == sequence::eContains || compare == sequence::eSame || compare == sequence::eContained) {
                        bool same_strand = StrandsMatch(f->GetLocation(), f2->GetLocation());

                        CConstRef<CObject> obj(f->GetSeq_feat().GetPointer());
                        CRef<CDiscrepancyObject> r(new CDiscrepancyObject(obj, bi->GetScope(), context.m_File, context.m_KeepRef));
                        if (HasContainedNote(*(f->GetSeq_feat()))) {
                            Add(m_Objs, r);
                        } else if (same_strand) {
                            Add(m_ObjsSameStrand, r);
                        } else {
                            Add(m_ObjsDiffStrand, r);
                        }
                        obj.Reset(f2->GetSeq_feat().GetPointer());
                        r.Reset(new CDiscrepancyObject(obj, bi->GetScope(), context.m_File, context.m_KeepRef));
                        if (HasContainedNote(*(f->GetSeq_feat()))) {
                            Add(m_Objs, r);
                        } else if (same_strand) {
                            Add(m_ObjsSameStrand, r);
                        } else {
                            Add(m_ObjsDiffStrand, r);
                        }
                    }
                    ++f2;
                }
                ++f;
            }
        }
        ++bi;
    }

    size_t n = m_Objs.size() + m_ObjsSameStrand.size() + m_ObjsDiffStrand.size();
    if (!n) return false;

    m_ReportItems.clear();
    CNcbiOstrstream ss;
    ss << n << " coding region" << (n==1 ? " overlaps" : "s overlap") << " another coding region with a similar or identical name";
    CRef<CDiscrepancyItem> item(new CDiscrepancyItem(GetName(), CNcbiOstrstreamToString(ss)));
    TReportObjectList Objs(m_Objs);
    copy(m_ObjsSameStrand.begin(), m_ObjsSameStrand.end(), back_inserter(Objs));
    copy(m_ObjsDiffStrand.begin(), m_ObjsDiffStrand.end(), back_inserter(Objs));
    item->SetDetails(Objs);
    m_ReportItems.push_back(CRef<CReportItem>(item.Release()));

    return true;
}


DISCREPANCY_AUTOFIX(CONTAINED_CDS)
{
    bool rval = false;

    TReportObjectList list_all;
    list_all.insert(list_all.end(), m_Objs.begin(), m_Objs.end());
    list_all.insert(list_all.end(), m_ObjsSameStrand.begin(), m_ObjsSameStrand.end());
    list_all.insert(list_all.end(), m_ObjsDiffStrand.begin(), m_ObjsDiffStrand.end());

    TReportObjectList::iterator it1 = list_all.begin();
    while (it1 != list_all.end()) {
        //const CSeq_feat* f1 = dynamic_cast<const CSeq_feat*>((*it1)->GetObject().GetPointer());
        const CSeq_feat* f1 = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it1).GetNCPointer())->GetObject().GetPointer());
        TReportObjectList::iterator it2 = it1;
        ++it2;
        bool remove_f1 = false;
        while (it2 != list_all.end()) {
            bool remove_f2 = false;
            //const CSeq_feat* f2 = dynamic_cast<const CSeq_feat*>((*it2)->GetObject().GetPointer());
            const CSeq_feat* f2 = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it2).GetNCPointer())->GetObject().GetPointer());
            sequence::ECompare compare = 
                    sequence::Compare(f1->GetLocation(),
                                        f2->GetLocation(),
                                        &scope, sequence::fCompareOverlapping);
            if (compare == sequence::eContains) {
                // convert f2 to misc_feat
                if (ConvertCDSToMiscFeat(*f2, scope)) {
                    rval = true;
                    remove_f2 = true;
                }
            } else if (compare == sequence::eContained) {
                // convert f1 to misc_feat
                if (ConvertCDSToMiscFeat(*f1, scope)) {
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
*/


DISCREPANCY_CASE(DUMMY_NORMAL, CSeq_inst, eNormal, "Dummy entry for the debug purpose")
{
}


DISCREPANCY_SUMMARIZE(DUMMY_NORMAL)
{
    m_Objs[GetName()][GetName()][GetName()];
    m_ReportItems = m_Objs.Export(GetName())->GetSubitems();
}


DISCREPANCY_CASE(DUMMY_ONCALLER, CSeq_inst, eOncaller, "Dummy entry for the debug purpose")
{
}


DISCREPANCY_SUMMARIZE(DUMMY_ONCALLER)
{
    m_Objs[GetName()][GetName()][GetName()];
    m_ReportItems = m_Objs.Export(GetName())->GetSubitems();
}


DISCREPANCY_CASE(DUMMY_MEGA, CSeq_inst, eMega, "Dummy entry for the debug purpose")
{
}


DISCREPANCY_SUMMARIZE(DUMMY_MEGA)
{
    m_Objs[GetName()][GetName()][GetName()];
    m_ReportItems = m_Objs.Export(GetName())->GetSubitems();
}


DISCREPANCY_CASE(DUMMY_ALL, CSeq_inst, eAll, "Dummy entry for the debug purpose")
{
}


DISCREPANCY_SUMMARIZE(DUMMY_ALL)
{
    m_Objs[GetName()][GetName()][GetName()];
    m_ReportItems = m_Objs.Export(GetName())->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
