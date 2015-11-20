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
#include "utils.hpp"
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/macro/String_constraint.hpp>
#include <objects/misc/sequence_util_macros.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
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
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
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
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// SHORT_SEQUENCES
DISCREPANCY_CASE(SHORT_SEQUENCES, CSeq_inst, eAll, "Find Short Sequences")
{
    if (obj.IsAa()) {
        return;
    }
    if (obj.IsSetLength() && obj.GetLength() < 50 && !context.IsCurrentRnaInGenProdSet()) {
        m_Objs["[n] sequence[s] [is] shorter than 50 nt"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }
}


DISCREPANCY_SUMMARIZE(SHORT_SEQUENCES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// PERCENT_N

static size_t ambigBasesInSeq(const CSeq_data& seq_data_in) {
    CSeq_data as_iupacna;
    size_t num_Ns = 0;

    TSeqPos nconv = CSeqportUtil::Convert(seq_data_in, &as_iupacna,
            CSeq_data::e_Iupacna);
    if (nconv == 0) {
        // TODO: is this an actual error, should we do something more drastic?
        return 0;
    }

    const string& iupacna_str = as_iupacna.GetIupacna().Get();

    ITERATE( string, base_iter, iupacna_str ) {
        if (toupper(*base_iter) == 'N') {
            ++num_Ns;
        }
    }
    return num_Ns;
}


DISCREPANCY_CASE(PERCENT_N, CSeq_inst, eAll, "Greater than 5 percent Ns")
{
    if (obj.IsAa() || context.SequenceHasFarPointers()) {
        return;
    }

    // Make a Seq Map so that we can explicitly look at the gaps vs. the unknowns.
    const CRef<CSeqMap> seq_map = CSeqMap::CreateSeqMapForBioseq(*context.GetCurrentBioseq());
    SSeqMapSelector sel;
    sel.SetFlags(CSeqMap::fFindData | CSeqMap::fFindGap);

    CSeqMap_CI seq_iter(seq_map, &context.GetScope(), sel);

    size_t count = 0;
    size_t tot_length = 0;

    for ( ; seq_iter; ++seq_iter) {
        switch(seq_iter.GetType()) {
            case CSeqMap::eSeqData:
                count += ambigBasesInSeq(seq_iter.GetData());
                tot_length += seq_iter.GetLength();
                break;
            case CSeqMap::eSeqGap:
                tot_length += seq_iter.GetLength();
                break;
            default:
                break;
        }
    }

    size_t percent_Ns = (count * 100) / tot_length;

    if (percent_Ns > 5) {
        m_Objs["[n] sequence[s] had greater than 5% Ns"].Add(
                *new CDiscrepancyObject(context.GetCurrentBioseq(),
                    context.GetScope(),
                    context.GetFile(),
                    context.GetKeepRef()));
    }
}


DISCREPANCY_SUMMARIZE(PERCENT_N)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INTERNAL_TRANSCRIBED_SPACER_RRNA

static const char* kRRNASpacer[] = { "internal", "transcribed", "spacer", "\0" };


DISCREPANCY_CASE(INTERNAL_TRANSCRIBED_SPACER_RRNA, CRNA_ref, eOncaller, "Look for rRNAs that contain either 'internal', 'transcribed' or 'spacer'")
{
    CConstRef<CSeq_feat> feat = context.GetCurrentSeq_feat();
    if (feat->GetData().GetSubtype() != CSeqFeatData::eSubtype_rRNA) {
        return;
    }

    const string& rna_name = obj.GetRnaProductName();
    for (size_t i = 0; kRRNASpacer[i][0] != '\0'; ++i) {
        if (NStr::FindNoCase(rna_name, kRRNASpacer[i]) != NPOS) {
            m_Objs["[n] rRNA feature products contain 'internal', 'transcribed' or 'spacer'"].Add(
                *new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
        }
    }
}


DISCREPANCY_SUMMARIZE(INTERNAL_TRANSCRIBED_SPACER_RRNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


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
    if (feat.IsSetComment() && NStr::Find(feat.GetComment(), kOverlappingCDSNoteText) != string::npos) {
        return false;
    }
    AddComment(feat, (string)kOverlappingCDSNoteText);
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
    m_ReportItems = m_Objs[kEmptyStr].Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(OVERLAPPING_CDS)
{
    TReportObjectList list = item->GetDetails();
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf) {
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*sf);
            if (SetOverlapNote(*new_feat)) {
                CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(*sf));
                feh.Replace(*new_feat);
            }
        }
    }
}


// CONTAINED_CDS

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


static bool ConvertCDSToMiscFeat(const CSeq_feat& feat, CScope& scope)
{
    if (!feat.GetData().IsCdregion() || feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature) {
        return false;
    }
    CRef<CSeq_feat> replacement(new CSeq_feat());
    replacement->Assign(feat);
    replacement->SetData().SetImp().SetKey("misc_feature");
    if (feat.IsSetProduct()) {
        string product = GetProductName(feat, scope);
        if (!NStr::IsBlank(product)) {
            AddComment(*replacement, product);
        }
    }
    try {
        CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(feat));
        feh.Replace(*replacement);
    } catch (...) {
        // feature may have already been removed or converted
        return false;
    }
    return true;
}


DISCREPANCY_CASE(CONTAINED_CDS, CSeqFeatData, eNormal, "Contained CDs")
{
    static const char* kContained = "[n] coding region[s] completely contained in another coding region.";
    static const char* kContainedNote = "[n] coding region[s] completely contained in another coding region, but have note.";
    static const char* kContainedSame = "[n] coding region[s] completely contained in another coding region on the same strand.";
    static const char* kContainedOpps = "[n] coding region[s] completely contained in another coding region, but on the opposite strand.";

    if (obj.Which() != CSeqFeatData::e_Cdregion) {
        return;
    }
    if (!context.GetCurrentBioseq()->CanGetInst() || !context.GetCurrentBioseq()->GetInst().IsNa() || !context.IsEukaryotic()) {
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
            m_Objs[kEmptyStr][kContained][HasContainedNote(*sf) ? kContainedNote : strand].Add(*new CDiscrepancyObject(sf, context.GetScope(), context.GetFile(), context.GetKeepRef(), compare == sequence::eContains));
            m_Objs[kEmptyStr][kContained][HasContainedNote(*context.GetCurrentSeq_feat()) ? kContainedNote : strand].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), compare == sequence::eContained));
        }
    }
    m_Objs["tmp"].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), true));
}


DISCREPANCY_SUMMARIZE(CONTAINED_CDS)
{
    m_ReportItems = m_Objs[kEmptyStr].Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(CONTAINED_CDS)
{
    TReportObjectList list = item->GetDetails();
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        if ((*it)->CanAutofix()) {
            const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
            if (sf) {
                ConvertCDSToMiscFeat(*sf, scope);
            }
        }
    }
}


DISCREPANCY_CASE(ZERO_BASECOUNT, CSeq_inst, eAll, "Zero Base Counts")
{
    if (obj.IsAa() || context.SequenceHasFarPointers()) {
        return;
    }
    map<char, size_t>& Map = context.GetNucleotideCount();
    if (!Map['A'] || !Map['C'] || !Map['G'] || !Map['T']) {
        m_Objs["[n] sequence[s] [has] a zero basecount for a nucleotide"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }
}


DISCREPANCY_SUMMARIZE(ZERO_BASECOUNT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(NO_ANNOTATION, CSeq_inst, eAll, "No annotation")
{
    if (obj.IsAa() || context.HasFeatures()) {
        return;
    }
    m_Objs["[n] bioseq[s] [has] no features"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
}


DISCREPANCY_SUMMARIZE(NO_ANNOTATION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(LONG_NO_ANNOTATION, CSeq_inst, eAll, "No annotation for LONG sequence")
{
    const int kSeqLength = 5000;
    if (obj.IsAa() || context.HasFeatures() || !(obj.CanGetLength() && obj.GetLength() > kSeqLength)) {
        return;
    }
    m_Objs["[n] bioseq[s] [is] longer than 5000nt and [has] no features"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
}


DISCREPANCY_SUMMARIZE(LONG_NO_ANNOTATION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(BAD_LOCUS_TAG_FORMAT, CSeqFeatData, eAll, "Bad locus tag format")
{
}


DISCREPANCY_SUMMARIZE(BAD_LOCUS_TAG_FORMAT)
{
}


DISCREPANCY_CASE(MISSING_LOCUS_TAGS, CSeqFeatData, eAll, "Missing locus tags")
{
}


DISCREPANCY_SUMMARIZE(MISSING_LOCUS_TAGS)
{
}


DISCREPANCY_CASE(INCONSISTENT_LOCUS_TAG_PREFIX, CSeqFeatData, eAll, "Inconsistent locus tag prefix")
{
}


DISCREPANCY_SUMMARIZE(INCONSISTENT_LOCUS_TAG_PREFIX)
{
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
