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
 * Authors: Sema Kachalo
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include "utils.hpp"
#include <sstream>
#include <objects/biblio/Author.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/macro/Source_location.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/seq_macros.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/submit/Contact_info.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <util/xregexp/regexp.hpp>
#include <objtools/cleanup/cleanup.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);


const CSeqSummary& CDiscrepancyContext::CurrentBioseqSummary() const
{
    return *m_CurrentNode->m_BioseqSummary;
}


void CDiscrepancyContext::SetSuspectRules(const string& name, bool read)
{
    if (!m_ProductRules || m_SuspectRules != name) {
        m_SuspectRules = name;
        if (read) {
            m_ProductRules = NDiscrepancy::GetProductRules(m_SuspectRules);
        }
    }
}


CConstRef<CSuspect_rule_set> CDiscrepancyContext::GetProductRules()
{
    if (!m_ProductRules) {
        m_ProductRules = NDiscrepancy::GetProductRules(m_SuspectRules);
    }
    return m_ProductRules;
}


CConstRef<CSuspect_rule_set> CDiscrepancyContext::GetOrganelleProductRules()
{
    if (!m_OrganelleProductRules) {
        m_OrganelleProductRules = NDiscrepancy::GetOrganelleProductRules();
    }
    return m_OrganelleProductRules;
}


bool CDiscrepancyContext::IsUnculturedNonOrganelleName(const string& taxname)
{
    if (NStr::Equal(taxname, "uncultured organism") ||
        NStr::Equal(taxname, "uncultured microorganism") ||
        NStr::Equal(taxname, "uncultured bacterium") ||
        NStr::Equal(taxname, "uncultured archaeon")) {
        return true;
    } else {
        return false;
    }

}


bool CDiscrepancyContext::HasLineage(const CBioSource& biosrc, const string& def_lineage, const string& type)
{
    return NStr::FindNoCase(def_lineage, type) != NPOS || (def_lineage.empty() && biosrc.IsSetLineage() && NStr::FindNoCase(biosrc.GetLineage(), type) != NPOS);
}


bool CDiscrepancyContext::HasLineage(const CBioSource* biosrc, const string& lineage) const
{
    return biosrc ? HasLineage(*biosrc, GetLineage(), lineage) : false;
}


bool CDiscrepancyContext::IsOrganelle(const CBioSource* biosrc) {
    if (biosrc && biosrc->IsSetGenome()) {
        int genome = biosrc->GetGenome();
        return genome == CBioSource::eGenome_chloroplast
            || genome == CBioSource::eGenome_chromoplast
            || genome == CBioSource::eGenome_kinetoplast
            || genome == CBioSource::eGenome_mitochondrion
            || genome == CBioSource::eGenome_cyanelle
            || genome == CBioSource::eGenome_nucleomorph
            || genome == CBioSource::eGenome_apicoplast
            || genome == CBioSource::eGenome_leucoplast
            || genome == CBioSource::eGenome_proplastid
            || genome == CBioSource::eGenome_hydrogenosome
            || genome == CBioSource::eGenome_plastid
            || genome == CBioSource::eGenome_chromatophore;
    }
    return false;
}


bool CDiscrepancyContext::IsEukaryotic(const CBioSource* biosrc) const
{
    if (biosrc) {
        int genome = biosrc->GetGenome();
        return genome != CBioSource::eGenome_mitochondrion
            && genome != CBioSource::eGenome_chloroplast
            && genome != CBioSource::eGenome_plastid
            && genome != CBioSource::eGenome_apicoplast
            && HasLineage(*biosrc, GetLineage(), "Eukaryota");
    }
    return false;
}


bool CDiscrepancyContext::IsBacterial(const CBioSource* biosrc) const
{
    return biosrc ? HasLineage(biosrc, "Bacteria") : false;
}


bool CDiscrepancyContext::IsViral(const CBioSource* biosrc) const
{
    return biosrc ? HasLineage(biosrc, "Viruses") : false;
}


string CSeqSummary::GetStats() const
{
    if (Stats.empty()) {
        Stats = "(length " + NStr::NumericToString(Len);
        if (N + Other) {
            Stats += ", " + NStr::NumericToString(N + Other) + " other";
        }
        if (Gaps > 0) {
            Stats += ", " + NStr::NumericToString(Gaps) + " gap";
        }
        Stats += ")";
    }
    return Stats;
}


inline static void _notN(CSeqSummary& sum)
{
    if (sum._Ns >= 10) {
        sum.NRuns.push_back({ sum._Pos + 1 - sum._Ns, sum._Pos });
    }
    sum._Ns = 0;
}

inline static void _QualityScore(CSeqSummary& sum)
{
    sum._QS++;
    size_t q = sum._QS;
    if (sum._Pos > CSeqSummary::WINDOW_SIZE) {
        for (size_t r = sum._CBread; r != sum._CBwrite && sum._CBposition[r] <= sum._Pos; r = r >= CSeqSummary::WINDOW_SIZE - 1 ? 0 : r + 1) {
            sum._CBread = r;
            q = sum._QS - sum._CBscore[r];
        }
    }
    if (q > sum.MinQ) { // yes, ">"!
        sum.MinQ = q;
    }
    sum._CBscore[sum._CBwrite] = sum._QS;
    sum._CBposition[sum._CBwrite] = sum._Pos + CSeqSummary::WINDOW_SIZE;
    sum._CBwrite++;
    if (sum._CBwrite >= CSeqSummary::WINDOW_SIZE) {
        sum._CBwrite = 0;
    }
}

inline static void sA(CSeqSummary& sum) { sum.A++; _notN(sum); }
inline static void sG(CSeqSummary& sum) { sum.G++; _notN(sum); }
inline static void sC(CSeqSummary& sum) { sum.C++; _notN(sum); }
inline static void sT(CSeqSummary& sum) { sum.T++; _notN(sum); }

inline static void sZ(CSeqSummary& sum)
{
    sum.Other++;
    _notN(sum);
    _QualityScore(sum);
}

inline static void sN(CSeqSummary& sum)
{
    sum.N++;
    sum._Ns++;
    if (sum._Ns > sum.MaxN) {
        sum.MaxN = sum._Ns;
    }
    if (sum.First) {
        sum.StartsWithGap = true;
    }
    sum.EndsWithGap = true;
    _QualityScore(sum);
}


static void CountNucleotides(const CSeq_data& seq_data, TSeqPos pos, TSeqPos len, CSeqSummary& sum)
{
/*
    enum E_Choice {
        e_not_set = 0,  ///< No variant selected
        e_Iupacna,      ///< IUPAC 1 letter nuc acid code
        e_Iupacaa,      ///< IUPAC 1 letter amino acid code
        e_Ncbi2na,      ///< 2 bit nucleic acid code
        e_Ncbi4na,      ///< 4 bit nucleic acid code
        e_Ncbi8na,      ///< 8 bit extended nucleic acid code
        e_Ncbipna,      ///< nucleic acid probabilities
        e_Ncbi8aa,      ///< 8 bit extended amino acid codes
        e_Ncbieaa,      ///< extended ASCII 1 letter aa codes
        e_Ncbipaa,      ///< amino acid probabilities
        e_Ncbistdaa,    ///< consecutive codes for std aas
        e_Gap           ///< gap types
    };
*/
    sum._Pos = pos;
    switch (seq_data.Which()) {
        case CSeq_data::e_Ncbi2na:
            //cout << "> e_Ncbi2na\n";
            {
                vector<char>::const_iterator it = seq_data.GetNcbi2na().Get().begin();
                unsigned char mask = 0xc0;
                unsigned char shift = 6;
                for (size_t n = 0; n < len; n++, sum._Pos++) {
                    unsigned char c = ((*it) & mask) >> shift;
                    mask >>= 2;
                    shift -= 2;
                    if (!mask) {
                        mask = 0xc0;
                        shift = 6;
                        ++it;
                    }
                    switch (c) {
                        case 0:
                            sA(sum);
                            break;
                        case 1:
                            sC(sum);
                            break;
                        case 2:
                            sG(sum);
                            break;
                        case 3:
                            sT(sum);
                            break;
                    }
                }
                if (len) {
                    sum.First = false;
                    sum.EndsWithGap = false;
                }
            }
            return;
        case CSeq_data::e_Ncbi4na:
            //cout << "> e_Ncbi4na\n";
            {
                vector<char>::const_iterator it = seq_data.GetNcbi4na().Get().begin();
                unsigned char mask = 0xf0;
                unsigned char shift = 4;
                for (size_t n = 0; n < len; n++, sum._Pos++) {
                    unsigned char c = ((*it) & mask) >> shift;
                    mask >>= 4;
                    shift -= 4;
                    if (!mask) {
                        mask = 0xf0;
                        shift = 4;
                        ++it;
                    }
                    sum.EndsWithGap = false;
                    switch (c) {
                        case 1:
                            sA(sum);
                            break;
                        case 2:
                            sC(sum);
                            break;
                        case 4:
                            sG(sum);
                            break;
                        case 8:
                            sT(sum);
                            break;
                        case 15:
                            sN(sum);
                            break;
                        default:
                            sZ(sum);
                            break;
                    }
                    sum.First = false;
                }
            }
            return;
        case CSeq_data::e_Iupacna:
            //cout << "> e_Iupacna\n";
            {
                const string& s = seq_data.GetIupacna().Get();
                for (size_t n = 0; n < len; n++, sum._Pos++) {
                    sum.EndsWithGap = false;
                    switch (s[n]) {
                        case 'A':
                            sA(sum);
                            break;
                        case 'C':
                            sC(sum);
                            break;
                        case 'G':
                            sG(sum);
                            break;
                        case 'T':
                            sT(sum);
                            break;
                        case 'N':
                            sN(sum);
                            break;
                        default:
                            sZ(sum);
                            break;
                    }
                    sum.First = false;
                }
            }
            return;
        case CSeq_data::e_Ncbi8na:
        case CSeq_data::e_Ncbipna:  // no test data available; resorting to "slow" method.
            //cout << (seq_data.Which() == CSeq_data::e_Ncbi8na ? "> e_Ncbi8na\n" : "> e_Ncbipna\n");
            {
                CSeq_data iupacna;
                if (!CSeqportUtil::Convert(seq_data, &iupacna, CSeq_data::e_Iupacna)) {
                    return;
                }
                const string& s = iupacna.GetIupacna().Get();
                for (size_t n = 0; n < len; n++, sum._Pos++) {
                    sum.EndsWithGap = false;
                    switch (s[n]) {
                        case 'A':
                            sA(sum);
                            break;
                        case 'C':
                            sC(sum);
                            break;
                        case 'G':
                            sG(sum);
                            break;
                        case 'T':
                            sT(sum);
                            break;
                        case 'N':
                            sN(sum);
                            break;
                        default:
                            sZ(sum);
                            break;
                    }
                    sum.First = false;
                }
            }
            return;
        default:
            return;    
    }
}


void CDiscrepancyContext::BuildSeqSummary(const CBioseq& bs, CSeqSummary& summary)
{
    summary.clear();

    // Length
    summary.Len = bs.GetInst().GetLength();

    // Label
    CConstRef<CSeq_id> best_id;
    int best_score = CSeq_id::kMaxScore;
    for (auto id: bs.GetId()) {
        if (id->Which() == CSeq_id::e_Genbank || id->Which() == CSeq_id::e_Ddbj || id->Which() == CSeq_id::e_Embl || id->Which() == CSeq_id::e_Other) {
            best_id = id;
            break;
        }
        else {
            if (best_score > id->BaseBestRankScore()) {
                best_id = id;
                best_score = id->BaseBestRankScore();
            }
        }
    }
    best_id->GetLabel(&summary.Label, CSeq_id::eContent);

    // Stats
    const CRef<CSeqMap> seq_map = CSeqMap::CreateSeqMapForBioseq(bs);
    SSeqMapSelector sel;
    sel.SetFlags(CSeqMap::fFindData | CSeqMap::fFindGap | CSeqMap::fFindLeafRef);
    for (CSeqMap_CI seq_iter(seq_map, &GetScope(), sel); seq_iter; ++seq_iter) {
        switch (seq_iter.GetType()) {
        case CSeqMap::eSeqData:
            CountNucleotides(seq_iter.GetData(), seq_iter.GetPosition(), seq_iter.GetLength(), summary);
            break;
        case CSeqMap::eSeqGap:
            _notN(summary);
            if (summary.First) {
                summary.First = false;
            }
            summary.Gaps += seq_iter.GetLength();
            break;
        case CSeqMap::eSeqRef:
            _notN(summary);
            summary.First = false;
            summary.EndsWithGap = false;
            summary.HasRef = true;
        default:
            _notN(summary);
            break;
        }
    }
    _notN(summary);
}


string CDiscrepancyContext::GetGenomeName(unsigned n)
{
    static vector<string> G;
    if (G.empty()) {
        G.resize(eSource_location_chromatophore + 1);
        for (int i = eSource_location_unknown; i <= eSource_location_chromatophore; i++) {
            string str = ENUM_METHOD_NAME(ESource_location)()->FindName(i, true);
            G[i] = (str == "unknown") ? kEmptyStr : ((str == "extrachrom") ? "extrachromosomal" : str);
        }
    }
    return n < G.size() ? G[n] : kEmptyStr;
}


string CDiscrepancyContext::GetAminoacidName(const CSeq_feat& obj) // from tRNA
{
    string aa;
    feature::GetLabel(obj, &aa, feature::fFGL_Content);
    size_t n = aa.find_last_of('-');            // cut off the "tRNA-" prefix
    if (n != string::npos) {
        aa = aa.substr(n + 1); // is there any better way to get the aminoacid name?
    }
    return aa;
}


bool CDiscrepancyContext::IsBadLocusTagFormat(const string& locus_tag) const
{
    // Optimization:  compile regexp only once by making it static.
    static CRegexp regexp("^[A-Za-z][0-9A-Za-z]{2,}_[0-9A-Za-z]+$");

    // Locus tag format documentation:  
    // https://www.ncbi.nlm.nih.gov/genomes/locustag/Proposal.pdf

    return !regexp.IsMatch(locus_tag); 
}


bool CDiscrepancyContext::IsRefseq() const
{
    const CBioseq& bioseq = CurrentBioseq();
    if (bioseq.IsSetId()) {
        for (auto& id : bioseq.GetId()) {
            if (id->IsOther()) {
                return true;
            }
        }
    }
    return false;
}


bool CDiscrepancyContext::IsBGPipe()
{
    for (auto& desc : GetAllSeqdesc()) {
        if (desc.IsUser()) {
            const CUser_object& user = desc.GetUser();
            if (FIELD_IS_SET_AND_IS(user, Type, Str) && NStr::EqualNocase(user.GetType().GetStr(), "StructuredComment")) {
                CConstRef<CUser_field> prefix_field = user.GetFieldRef("StructuredCommentPrefix", ".", NStr::eNocase);
                if (prefix_field && FIELD_IS_SET_AND_IS(*prefix_field, Data, Str) && NStr::EqualNocase(prefix_field->GetData().GetStr(), "##Genome-Annotation-Data-START##")) {
                    CConstRef<CUser_field> pipeline_field = user.GetFieldRef("Annotation Pipeline", ".", NStr::eNocase);
                    if (pipeline_field && FIELD_IS_SET_AND_IS(*pipeline_field, Data, Str) && NStr::EqualNocase(pipeline_field->GetData().GetStr(), "NCBI Prokaryotic Genome Annotation Pipeline")) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
#if 0
    CSeqdesc_CI user_desc_ci(m_Scope->GetBioseqHandle(*GetCurrentBioseq()), CSeqdesc::e_User);
    for(; !IsBGPipe_is_bgpipe && user_desc_ci; ++user_desc_ci) {
        const CUser_object& user_desc = user_desc_ci->GetUser();
        // only look at structured comments
        if(!FIELD_IS_SET_AND_IS(user_desc, Type, Str) || !NStr::EqualNocase(user_desc.GetType().GetStr(), "StructuredComment")) {
            continue;
        }
        CConstRef<CUser_field> struccmt_prefix_field = user_desc.GetFieldRef("StructuredCommentPrefix", ".", NStr::eNocase);
        if(!struccmt_prefix_field || !FIELD_IS_SET_AND_IS(*struccmt_prefix_field, Data, Str) || !NStr::EqualNocase(struccmt_prefix_field->GetData().GetStr(), "##Genome-Annotation-Data-START##") ) {
            continue;
        }
        CConstRef<CUser_field> annot_pipeline_field = user_desc.GetFieldRef("Annotation Pipeline", ".", NStr::eNocase);
        if (!annot_pipeline_field || !FIELD_IS_SET_AND_IS(*annot_pipeline_field, Data, Str) || !NStr::EqualNocase(annot_pipeline_field->GetData().GetStr(), "NCBI Prokaryotic Genome Annotation Pipeline")) {
            continue;
        }
        IsBGPipe_is_bgpipe = true;
        return IsBGPipe_is_bgpipe;
    }
    return IsBGPipe_is_bgpipe;
#endif
}


const CSeq_feat* CDiscrepancyContext::GetGeneForFeature(const CSeq_feat& feat)
{
    auto gene = GeneForFeature(*FindNode(feat));
    return gene ? dynamic_cast<const CSeq_feat*>(&*gene->m_Obj) : nullptr;
}


string CDiscrepancyContext::GetProdForFeature(const CSeq_feat& feat)
{
    return ProdForFeature(*FindNode(feat));
}


const CDiscrepancyContext::CParseNode* CDiscrepancyContext::GeneForFeature(const CDiscrepancyContext::CParseNode& node)
{
    if (node.m_Info & CParseNode::eKnownGene) {
        return node.m_Gene;
    }
    node.m_Info |= CParseNode::eKnownGene;
    const CSeq_feat& feat = dynamic_cast<const CSeq_feat&>(*node.m_Obj);
    auto gene = sequence::GetGeneForFeature(feat, *m_Scope);
    node.m_Gene = gene ? FindNode(*gene) : nullptr;
    return node.m_Gene;
}


string CDiscrepancyContext::ProdForFeature(const CDiscrepancyContext::CParseNode& node)
{
    if (node.m_Info & CParseNode::eKnownProduct) {
        return node.m_Product;
    }
    node.m_Info |= CParseNode::eKnownProduct;
    const CSeq_feat& feat = dynamic_cast<const CSeq_feat&>(*node.m_Obj);
    node.m_Product = GetProductName(feat, *m_Scope);
    return node.m_Product;
}


bool CDiscrepancyContext::IsPseudo(const CSeq_feat& feat)
{
    return IsPseudo(*FindNode(feat));
}


bool CDiscrepancyContext::IsPseudo(const CParseNode& node)
{
    if (node.m_Info & CParseNode::eKnownPseudo) {
        return node.m_Info & CParseNode::eIsPseudo;
    }
    node.m_Info |= CParseNode::eKnownPseudo;
    const CSeq_feat& feat = dynamic_cast<const CSeq_feat&>(*node.m_Obj);
    if (feat.IsSetPseudo() && feat.GetPseudo()) {
        node.m_Info |= CParseNode::eIsPseudo;
        return true;
    }
    if (feat.IsSetQual()) {
        for (auto& it : feat.GetQual()) {
            if (it->IsSetQual() && NStr::EqualNocase(it->GetQual(), "pseudogene")) {
                node.m_Info |= CParseNode::eIsPseudo;
                return true;
            }
        }
    }
    if (feat.GetData().IsGene()) {
        if (feat.GetData().GetGene().IsSetPseudo() && feat.GetData().GetGene().GetPseudo()) {
            node.m_Info |= CParseNode::eIsPseudo;
            return true;
        }
    }
    else {
        if (feat.IsSetXref()) {
            for (auto& it : feat.GetXref()) {
                if (it->IsSetData() && it->GetData().IsGene() && it->GetData().GetGene().IsSetPseudo() && it->GetData().GetGene().GetPseudo()) {
                    node.m_Info |= CParseNode::eIsPseudo;
                    return true;
                }
            }
        }
        auto gene = GeneForFeature(node);
        if (gene && IsPseudo(*gene)) {
            node.m_Info |= CParseNode::eIsPseudo;
            return true;
        }
    }
    return false;
}


void CDiscrepancyContext::ClearFeatureList()
{
    m_FeatAll.clear();
    m_FeatGenes.clear();
    m_FeatPseudo.clear();
    m_FeatCDS.clear();
    m_FeatMRNAs.clear();
    m_FeatRRNAs.clear();
    m_FeatTRNAs.clear();
    m_Feat_RNAs.clear();
    m_FeatExons.clear();
    m_FeatIntrons.clear();
    m_FeatMisc.clear();
}


void CDiscrepancyContext::CollectFeature(const CSeq_feat& feat)
{
    m_FeatAll.push_back(CConstRef<CSeq_feat>(&feat));
    switch (feat.GetData().GetSubtype()) {
        case CSeqFeatData::eSubtype_gene:
            m_FeatGenes.push_back(&feat);
            break;
        case CSeqFeatData::eSubtype_cdregion:
            m_FeatCDS.push_back(&feat);
            break;
        case CSeqFeatData::eSubtype_mRNA:
            m_FeatMRNAs.push_back(&feat);
            break;
        case CSeqFeatData::eSubtype_tRNA:
            m_FeatTRNAs.push_back(&feat);
            break;
        case CSeqFeatData::eSubtype_rRNA:
            m_FeatRRNAs.push_back(&feat);
            break;
        case CSeqFeatData::eSubtype_exon:
            m_FeatExons.push_back(&feat);
            break;
        case CSeqFeatData::eSubtype_intron:
            m_FeatIntrons.push_back(&feat);
            break;
        case CSeqFeatData::eSubtype_misc_feature:
            m_FeatMisc.push_back(&feat);
            break;
        default:
            break;
    }
    if (feat.GetData().IsRna()) {
        m_Feat_RNAs.push_back(&feat);
    }
    if (IsPseudo(feat)) {
        m_FeatPseudo.push_back(&feat);
    }
}


sequence::ECompare CDiscrepancyContext::Compare(const CSeq_loc& loc1, const CSeq_loc& loc2) const
{
    try {
        CSeq_loc::TRange r1 = loc1.GetTotalRange();
        CSeq_loc::TRange r2 = loc2.GetTotalRange();
        if (r1.GetFrom() >= r2.GetToOpen() || r2.GetFrom() >= r1.GetToOpen()) {
            return sequence::eNoOverlap;
        }
    }
    catch (...) {} // LCOV_EXCL_LINE
    return sequence::Compare(loc1, loc2, &GetScope(), sequence::fCompareOverlapping);
}


const CPerson_id* CDiscrepancyContext::GetPerson_id() const
{
    if (m_CurrentNode->m_Type == eSubmit) {
        const CSeq_submit* sub = static_cast<const CSeq_submit*>(&*m_CurrentNode->m_Obj);
        if (sub->IsSetSub() && sub->GetSub().IsSetContact() && sub->GetSub().GetContact().IsSetContact() && sub->GetSub().GetContact().GetContact().IsSetName()) {
            return &sub->GetSub().GetContact().GetContact().GetName();
        }
    }
    return nullptr;
}


const CSubmit_block* CDiscrepancyContext::GetSubmit_block() const
{
    if (m_CurrentNode->m_Type == eSubmit) {
        const CSeq_submit* sub = static_cast<const CSeq_submit*>(&*m_CurrentNode->m_Obj);
        if (sub->IsSetSub()) {
            return &sub->GetSub();
        }
    }
    return nullptr;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CDiscrepancyContext::CParseNode* CDiscrepancyContext::FindNode(const CObject* obj) const
{
    if (obj) {
        const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(obj);
        if (feat) {
            return FindNode(*feat);
        }
        const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(obj);
        if (desc) {
            return FindNode(*desc);
        }
        for (auto node = m_CurrentNode; node; node = node->m_Parent) {
            if (&*node->m_Obj == obj) {
                return &*node;
            }
        }
    }
    return nullptr;
}


CDiscrepancyContext::CParseNode* CDiscrepancyContext::FindLocalNode(const CParseNode& node, const CSeq_feat& feat) const
{
    auto it = node.m_FeatureMap.find(&feat);
    return it == node.m_FeatureMap.end() ? nullptr : it->second;
}


CDiscrepancyContext::CParseNode* CDiscrepancyContext::FindNode(const CSeq_feat& feat) const
{
    for (auto node = m_CurrentNode; node; node = node->m_Parent) {
        auto it = node->m_FeatureMap.find(&feat);
        if (it != node->m_FeatureMap.end()) {
            return it->second;
        }
        if (node->m_Type == eSeqSet_NucProt || node->m_Type == eSeqSet_GenProd) {
            for (auto& child : node->m_Children) {
                CDiscrepancyContext::CParseNode* found = FindLocalNode(*child, feat);
                if (found) {
                    return found;
                }
            }
        }
    }
    return nullptr;
}


CDiscrepancyContext::CParseNode* CDiscrepancyContext::FindNode(const CSeqdesc& desc) const
{
    for (auto node = m_CurrentNode; node; node = node->m_Parent) {
        auto it = node->m_DescriptorMap.find(&desc);
        if (it != node->m_DescriptorMap.end()) {
            return it->second;
        }
    }
    return nullptr;
}


CDiscrepancyContext::CRefNode* CDiscrepancyContext::ContainingSet(CDiscrepancyContext::CRefNode& ref)
{
    CRefNode* ret = nullptr;
    for (CRefNode* r = ref.m_Parent.GetPointer(); r; r = r->m_Parent.GetPointer()) {
        if (!ret && IsSeqSet(r->m_Type)) {
            ret = r;
        }
        if (r->m_Type == eSeqSet_NucProt || r->m_Type == eSeqSet_GenProd) {
            return r;
        }
    }
    return ret ? ret : ref.m_Parent.GetPointer();
}


CRef<CDiscrepancyObject> CDiscrepancyContext::BioseqObjRef(EFixType fix, const CObject* more)
{
    CRefNode* fixref = nullptr;
    if (fix == eFixSelf) {
        fixref = &*m_CurrentNode->m_Ref;
    }
    else if (fix == eFixParent) {
        fixref = &*m_CurrentNode->m_Ref->m_Parent;
    }
    else if (fix == eFixSet) {
        fixref = ContainingSet(*m_CurrentNode->m_Ref);
    }
    CRef<CDiscrepancyObject> obj(new CDiscrepancyObject(m_CurrentNode->m_Ref, fixref, more));
    return obj;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::BioseqSetObjRef(bool fix, const CObject* more)
{
    CRef<CDiscrepancyObject> obj(new CDiscrepancyObject(m_CurrentNode->m_Ref, fix ? &*m_CurrentNode->m_Ref : nullptr, more));
    return obj;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::SubmitBlockObjRef(bool fix, const CObject* more)
{
    _ASSERT(m_CurrentNode->m_Type == eSubmit);
    CRef<CDiscrepancyObject> obj(new CDiscrepancyObject(m_CurrentNode->m_Ref, fix ? &*m_CurrentNode->m_Ref : nullptr, more));
    return obj;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::SeqFeatObjRef(const CSeq_feat& feat, EFixType fix, const CObject* more)
{
    CRef<CDiscrepancyObject> obj;
    auto node = FindNode(feat);
    if (node) {
        if (node->m_Ref->m_Text.empty()) {
            node->m_Ref->m_Text = CDiscrepancyObject::GetTextObjectDescription(feat, *m_Scope);
        }
        CRefNode* fixref = nullptr;
        if (fix == eFixSelf) {
            fixref = &*node->m_Ref;
        }
        else if (fix == eFixParent) {
            fixref = &*node->m_Ref->m_Parent;
        }
        else if (fix == eFixSet) {
            fixref = ContainingSet(*node->m_Ref);
        }
        obj.Reset(new CDiscrepancyObject(node->m_Ref, fixref, more));
    }
    return obj;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::SeqFeatObjRef(const CSeq_feat& feat, const CObject* fix, const CObject* more)
{
    CRef<CDiscrepancyObject> obj;
    auto node = FindNode(feat);
    if (node) {
        if (node->m_Ref->m_Text.empty()) {
            node->m_Ref->m_Text = CDiscrepancyObject::GetTextObjectDescription(feat, *m_Scope);
        }
        obj.Reset(new CDiscrepancyObject(node->m_Ref, fix ? FindNode(fix)->m_Ref.GetPointer() : nullptr, more));
    }
    return obj;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::SeqdescObjRef(const CSeqdesc& desc, const CObject* fix, const CObject* more)
{
    CRef<CDiscrepancyObject> obj;
    auto node = FindNode(desc);
    if (node) {
        if (node->m_Ref->m_Text.empty()) {
            node->m_Ref->m_Text = CDiscrepancyObject::GetTextObjectDescription(desc);
        }
        obj.Reset(new CDiscrepancyObject(node->m_Ref, fix ? &*FindNode(fix)->m_Ref : nullptr, more));
    }
    return obj;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::BiosourceObjRef(const CBioSource& biosrc, bool fix, const CObject* more)
{
    CRef<CDiscrepancyObject> obj;
    for (auto node = m_CurrentNode; node; node = node->m_Parent) {
        auto it = node->m_BiosourceMap.find(&biosrc);
        if (it != node->m_BiosourceMap.end()) {
            if (it->second->m_Ref->m_Text.empty()) {
                if (it->second->m_Type == eSeqFeat) {
                    it->second->m_Ref->m_Text = CDiscrepancyObject::GetTextObjectDescription(static_cast<const CSeq_feat&>(*it->second->m_Obj), *m_Scope);
                }
                else {  // eSeqDesc
                    it->second->m_Ref->m_Text = CDiscrepancyObject::GetTextObjectDescription(static_cast<const CSeqdesc&>(*it->second->m_Obj));
                }
            }
            obj.Reset(new CDiscrepancyObject(it->second->m_Ref, fix ? &*it->second->m_Ref : nullptr, more));
            return obj;
        }
    }
    return obj;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::PubdescObjRef(const CPubdesc& pubdesc, bool fix, const CObject* more)
{
    CRef<CDiscrepancyObject> obj;
    auto it = m_CurrentNode->m_PubdescMap.find(&pubdesc);
    _ASSERT(it != m_CurrentNode->m_PubdescMap.end());
    if (it->second->m_Ref->m_Text.empty()) {
        if (it->second->m_Type == eSeqFeat) {
            it->second->m_Ref->m_Text = CDiscrepancyObject::GetTextObjectDescription(static_cast<const CSeq_feat&>(*it->second->m_Obj), *m_Scope);
        }
        else if (it->second->m_Type == eSeqDesc) {
            it->second->m_Ref->m_Text = CDiscrepancyObject::GetTextObjectDescription(static_cast<const CSeqdesc&>(*it->second->m_Obj));
        }
    }
    obj.Reset(new CDiscrepancyObject(it->second->m_Ref, fix ? &*it->second->m_Ref : nullptr, more));
    return obj;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::AuthorsObjRef(const CAuth_list& authors, bool fix, const CObject* more)
{
    CRef<CDiscrepancyObject> obj;
    auto it = m_CurrentNode->m_AuthorMap.find(&authors);
    _ASSERT(it != m_CurrentNode->m_AuthorMap.end());
    if (it->second->m_Ref->m_Text.empty()) {
        if (it->second->m_Type == eSeqFeat) {
            it->second->m_Ref->m_Text = CDiscrepancyObject::GetTextObjectDescription(static_cast<const CSeq_feat&>(*it->second->m_Obj), *m_Scope);
        }
        else if (it->second->m_Type == eSeqDesc) {
            it->second->m_Ref->m_Text = CDiscrepancyObject::GetTextObjectDescription(static_cast<const CSeqdesc&>(*it->second->m_Obj));
        }
        else {
            //cout << "===============================================!\n";
        }
    }
    obj.Reset(new CDiscrepancyObject(it->second->m_Ref, fix ? &*it->second->m_Ref : nullptr, more));
    return obj;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::StringObjRef(const CObject* fix, const CObject* more)
{
    CRef<CDiscrepancyObject> obj;
    obj.Reset(new CDiscrepancyObject(m_CurrentNode->m_Ref, fix ? &*FindNode(fix)->m_Ref : nullptr, more));
    return obj;
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
