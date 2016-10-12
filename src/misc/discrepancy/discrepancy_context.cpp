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
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <util/xregexp/regexp.hpp>
#include <objtools/cleanup/cleanup.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);


string CDiscrepancyContext::GetCurrentBioseqLabel(void) const 
{
    if (GetCurrentBioseqLabel_count != m_Count_Bioseq) {
        GetCurrentBioseqLabel_count = m_Count_Bioseq;
        GetCurrentBioseqLabel_str.clear();
        //CConstRef<CBioseq> bs = GetCurrentBioseq();
        if (m_Current_Bioseq) {
            const CSeq_id* wid = FindBestChoice(m_Current_Bioseq->GetId(), CSeq_id::BestRank).GetPointer();
            wid->GetLabel(&GetCurrentBioseqLabel_str, NULL, CSeq_id::eContent);
        }
        else {
            CConstRef<CBioseq_set> bss = GetCurrentBioseq_set();
            if (bss) {
                bss->GetLabel(&GetCurrentBioseqLabel_str, CBioseq_set::eContent);
            }
        }
    }
    return GetCurrentBioseqLabel_str;
}


CConstRef<CBioseq> CDiscrepancyContext::GetCurrentBioseq(void) const 
{
    //static CConstRef<CBioseq> bioseq;
    //static size_t count = 0;
    if (GetCurrentBioseq_count != m_Count_Bioseq) {
        GetCurrentBioseq_count = m_Count_Bioseq;
        if (m_Current_Bioseq) {
            GetCurrentBioseq_bioseq = m_Current_Bioseq;
        }
        else {
            GetCurrentBioseq_bioseq.Reset();
            CConstRef<CBioseq_set> BS = GetCurrentBioseq_set();
            if (BS && BS->IsSetClass()) {
                CScope &scope = GetScope();
                if (BS->GetClass() == CBioseq_set::eClass_nuc_prot) {
                    GetCurrentBioseq_bioseq.Reset(scope.GetBioseqHandle(BS->GetNucFromNucProtSet()).GetCompleteBioseq());
                }
                else if (BS->GetClass() == CBioseq_set::eClass_gen_prod_set) {
                    GetCurrentBioseq_bioseq.Reset(scope.GetBioseqHandle(BS->GetGenomicFromGenProdSet()).GetCompleteBioseq());
                }
                else if (BS->GetClass() == CBioseq_set::eClass_segset) {
                    GetCurrentBioseq_bioseq.Reset(scope.GetBioseqHandle(BS->GetMasterFromSegSet()).GetCompleteBioseq());
                }
            }
        }
    }
    return GetCurrentBioseq_bioseq;
}


void CDiscrepancyContext::SetSuspectRules(const string& name)
{
    m_SuspectRules = name;
    if (!m_ProductRules) {
        m_ProductRules = CSuspect_rule_set::GetProductRules(m_SuspectRules);
    }
}


CConstRef<CSuspect_rule_set> CDiscrepancyContext::GetProductRules()
{
    if (!m_ProductRules) {
        m_ProductRules = CSuspect_rule_set::GetProductRules(m_SuspectRules);
    }
    return m_ProductRules;
}


CConstRef<CSuspect_rule_set> CDiscrepancyContext::GetOrganelleProductRules()
{
    if (!m_OrganelleProductRules) {
        m_OrganelleProductRules = CSuspect_rule_set::GetOrganelleProductRules();
    }
    return m_OrganelleProductRules;
}


const CBioSource* CDiscrepancyContext::GetCurrentBiosource()
{
    //static const CBioSource* biosrc;
    //static size_t count = 0;
    if (GetCurrentBiosource_count != m_Count_Bioseq) {
        GetCurrentBiosource_count = m_Count_Bioseq;
        GetCurrentBiosource_biosrc = sequence::GetBioSource(m_Scope->GetBioseqHandle(*GetCurrentBioseq()));
    }
    return GetCurrentBiosource_biosrc;
}

const CMolInfo* CDiscrepancyContext::GetCurrentMolInfo()
{
    //static const CMolInfo* mol_info;
    //static size_t count = 0;
    if (GetCurrentMolInfo_count != m_Count_Bioseq) {
        GetCurrentMolInfo_count = m_Count_Bioseq;
        GetCurrentMolInfo_mol_info = sequence::GetMolInfo(m_Scope->GetBioseqHandle(*GetCurrentBioseq()));
    }
    return GetCurrentMolInfo_mol_info;
}

bool CDiscrepancyContext::IsCurrentSequenceMrna()
{
    const CMolInfo* m = GetCurrentMolInfo();
    if (m && m->IsSetBiomol() && m->GetBiomol() == CMolInfo::eBiomol_mRNA) {
        return true;
    } else {
        return false;
    }
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
    return NStr::FindNoCase(def_lineage, type) != string::npos || def_lineage.empty() && biosrc.IsSetLineage() && NStr::FindNoCase(biosrc.GetLineage(), type) != string::npos;
}


bool CDiscrepancyContext::HasLineage(const string& lineage)
{
    //static const CBioSource* biosrc = 0;
    //static size_t count = 0;
    if (HasLineage_count != m_Count_Bioseq) {
        HasLineage_count = m_Count_Bioseq;
        HasLineage_biosrc = GetCurrentBiosource();
    }
    return HasLineage_biosrc ? HasLineage(*HasLineage_biosrc, GetLineage(), lineage) : false;
}


bool CDiscrepancyContext::IsDNA()
{
    //static bool result = false;
    //static size_t count = 0;
    if (IsDNA_count != m_Count_Bioseq) {
        IsDNA_count = m_Count_Bioseq;
        IsDNA_result = false;
        CConstRef<CBioseq> bioseq = GetCurrentBioseq();
        if (bioseq && bioseq->IsNa() && bioseq->IsSetInst()) {
            IsDNA_result = (bioseq->GetInst().IsSetMol() && bioseq->GetInst().GetMol() == CSeq_inst::eMol_dna);
        }
    }
    return IsDNA_result;
}

bool CDiscrepancyContext::IsOrganelle()
{
    //static bool result = false;
    //static size_t count = 0;
    if (IsOrganelle_count != m_Count_Bioseq) {
        IsOrganelle_count = m_Count_Bioseq;
        IsOrganelle_result = false;
        const CBioSource* biosrc = GetCurrentBiosource();
        if (biosrc && biosrc->IsSetGenome()) {
            int genome = biosrc->GetGenome();
            IsOrganelle_result = ((genome == CBioSource::eGenome_chloroplast
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
                || genome == CBioSource::eGenome_chromatophore));
        }
    }

    return IsOrganelle_result;
}


bool CDiscrepancyContext::IsEukaryotic()
{
    //static bool result = false;
    //static size_t count = 0;
    if (IsEukaryotic_count != m_Count_Bioseq) {
        IsEukaryotic_count = m_Count_Bioseq;
        const CBioSource* biosrc = GetCurrentBiosource();
        if (biosrc) {
            CBioSource::EGenome genome = (CBioSource::EGenome) biosrc->GetGenome();
            if (genome != CBioSource::eGenome_mitochondrion
                && genome != CBioSource::eGenome_chloroplast
                && genome != CBioSource::eGenome_plastid
                && genome != CBioSource::eGenome_apicoplast
                && HasLineage(*biosrc, GetLineage(), "Eukaryota")) {
                IsEukaryotic_result = true;
                return IsEukaryotic_result;
            }
        }
        IsEukaryotic_result = false;
    }
    return IsEukaryotic_result;
}


bool CDiscrepancyContext::IsBacterial()
{
    //static bool result = false;
    //static size_t count = 0;
    if (IsBacterial_count != m_Count_Bioseq) {
        IsBacterial_count = m_Count_Bioseq;
        const CBioSource* biosrc = GetCurrentBiosource();
        if (biosrc) {
            //CBioSource::EGenome genome = (CBioSource::EGenome) biosrc->GetGenome();
            if (HasLineage(*biosrc, GetLineage(), "Bacteria")) {
                IsBacterial_result = true;
                return IsBacterial_result;
            }
        }
        IsBacterial_result = false;
    }
    return IsBacterial_result;
}


bool CDiscrepancyContext::IsViral()
{
    //static bool result = false;
    //static size_t count = 0;
    if (IsViral_count != m_Count_Bioseq) {
        IsViral_count = m_Count_Bioseq;
        const CBioSource* biosrc = GetCurrentBiosource();
        if (biosrc) {
            CBioSource::EGenome genome = (CBioSource::EGenome) biosrc->GetGenome();
            if (HasLineage(*biosrc, GetLineage(), "Viruses")) {
                IsViral_result = true;
                return IsViral_result;
            }
        }
        IsViral_result = false;
    }
    return IsViral_result;
}


bool CDiscrepancyContext::IsPubMed()
{
    //static bool result = false;
    //static size_t count = 0;
    if (IsPubMed_count != m_Count_Pub_equiv) {
        IsPubMed_count = m_Count_Pub_equiv;
        IsPubMed_result = false;
        const CPub_equiv* equiv = GetCurrentPub_equiv();
        if (equiv) {
            const CPub_equiv::Tdata& list = equiv->Get();
            ITERATE (CPub_equiv::Tdata, it, list) {
                if ((*it)->IsPmid()) {
                    IsPubMed_result = true;
                    break;
                }
            }
        }
    }
    return IsPubMed_result;
}


bool CDiscrepancyContext::IsCurrentRnaInGenProdSet()
{
    static bool result = false;
    static size_t count = 0;
    if (count != m_Count_Bioseq) {
        count = m_Count_Bioseq;
        result = IsmRNASequenceInGenProdSet(GetCurrentBioseq(), Get_Bioseq_set_Stack());
    }
    return result;
}


CBioSource::TGenome CDiscrepancyContext::GetCurrentGenome()
{
    //static CBioSource::TGenome genome;
    //static size_t count = 0;
    if (GetCurrentGenome_count != m_Count_Bioseq) {
        GetCurrentGenome_count = m_Count_Bioseq;
        const CBioSource* biosrc = GetCurrentBiosource();
        GetCurrentGenome_genome = biosrc ? biosrc->GetGenome() : CBioSource::eGenome_unknown;
    }
    return GetCurrentGenome_genome;
}


bool CDiscrepancyContext::SequenceHasFarPointers()
{
    //static bool result = false;
    //static size_t count = 0;
    if (SequenceHasFarPointers_count != m_Count_Bioseq) {
        SequenceHasFarPointers_count = m_Count_Bioseq;
        SequenceHasFarPointers_result = false;
        if (!GetCurrentBioseq()->CanGetInst() || !GetCurrentBioseq()->GetInst().CanGetExt() || !GetCurrentBioseq()->GetInst().GetExt().IsDelta()) {
            return SequenceHasFarPointers_result;
        }
        FOR_EACH_DELTASEQ_IN_DELTAEXT(it, GetCurrentBioseq()->GetInst().GetExt().GetDelta()) {
            if ((*it)->IsLoc()) {
                SequenceHasFarPointers_result = true;
                return SequenceHasFarPointers_result;
            }
        }
    }
    return SequenceHasFarPointers_result;
}


static void CountNucleotides(const CSeq_data& seq_data, CSeqSummary& ret)
{
    if (seq_data.Which() != CSeq_data::e_Iupacna &&
        seq_data.Which() != CSeq_data::e_Ncbi2na &&
        seq_data.Which() != CSeq_data::e_Ncbi4na &&
        seq_data.Which() != CSeq_data::e_Ncbi8na &&
        seq_data.Which() != CSeq_data::e_Ncbipna) {
        return;
    }
        //e_Iupacaa,      ///< IUPAC 1 letter amino acid code
        //e_Ncbi8aa,      ///< 8 bit extended amino acid codes
        //e_Ncbieaa,      ///< extended ASCII 1 letter aa codes
        //e_Ncbipaa,      ///< amino acid probabilities
        //e_Ncbistdaa,    ///< consecutive codes for std aas

    CSeq_data as_iupacna;
    TSeqPos nconv = CSeqportUtil::Convert(seq_data, &as_iupacna, CSeq_data::e_Iupacna);
    if (nconv == 0) {
        return;
    }
    const string& iupacna_str = as_iupacna.GetIupacna().Get();
    ITERATE(string, base, iupacna_str) {
        switch (*base)
        {
            case 'A':
                ret.A++;
                break;
            case 'C':
                ret.C++;
                break;
            case 'G':
                ret.G++;
                break;
            case 'T':
                ret.T++;
                break;
            case 'N':
                ret.N++;
                break;
        }
    }
}


const CSeqSummary& CDiscrepancyContext::GetSeqSummary()
{
    //static CSafeStatic<CSeqSummary> ret;
    //static size_t count = 0;
    if (GetSeqSummary_count == m_Count_Bioseq) {
        return GetSeqSummary_ret;
    }
    GetSeqSummary_count = m_Count_Bioseq;
    GetSeqSummary_ret.clear();

    // Make a Seq Map so that we can explicitly look at the gaps vs. the unknowns.
    const CRef<CSeqMap> seq_map = CSeqMap::CreateSeqMapForBioseq(*GetCurrentBioseq());
    SSeqMapSelector sel;
    sel.SetFlags(CSeqMap::fFindData | CSeqMap::fFindGap);
    CSeqMap_CI seq_iter(seq_map, &GetScope(), sel);
    for (; seq_iter; ++seq_iter) {
        switch (seq_iter.GetType()) {
            case CSeqMap::eSeqData:
                GetSeqSummary_ret.Len += seq_iter.GetLength();
                CountNucleotides(seq_iter.GetData(), GetSeqSummary_ret);
                break;
            case CSeqMap::eSeqGap:
                GetSeqSummary_ret.Len += seq_iter.GetLength();
                break;
            default:
                break;
        }
    }
    return GetSeqSummary_ret;
}


string CDiscrepancyContext::GetGenomeName(int n)
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


bool CDiscrepancyContext::IsBadLocusTagFormat(const string& locus_tag)
{
    // Optimization:  compile regexp only once by making it static.
    static CRegexp regexp("^[[:alpha:]][[:alnum:]]{2,}_[[:alnum:]]+$");

    // Locus tag format documentation:  
    // https://www.ncbi.nlm.nih.gov/genomes/locustag/Proposal.pdf

    return !regexp.IsMatch(locus_tag); 
}


const CSeq_id* CDiscrepancyContext::GetProteinId()
{
    //static const CSeq_id* protein_id = NULL;
    //static size_t count = 0;
    if (GetProteinId_count == m_Count_Bioseq) {
        return GetProteinId_protein_id;
    }
    GetProteinId_count = m_Count_Bioseq;
    GetProteinId_protein_id = NULL;
    if( ! GetCurrentBioseq()->IsSetId() ) {
        // NULL
        return GetProteinId_protein_id;
    }
    const CBioseq::TId& bioseq_ids = GetCurrentBioseq()->GetId();
    ITERATE(CBioseq::TId, id_it, bioseq_ids) {
        const CSeq_id & seq_id = **id_it;
        if( seq_id.IsGeneral() && ! seq_id.GetGeneral().IsSkippable() ) {
            GetProteinId_protein_id = &seq_id;
            break;
        }
    }
    return GetProteinId_protein_id;
}


bool CDiscrepancyContext::IsRefseq()
{
    //static bool is_refseq = false;
    //static size_t count = 0;
    if (IsRefseq_count == m_Count_Bioseq) {
        return IsRefseq_is_refseq;
    }
    IsRefseq_count = m_Count_Bioseq;

    IsRefseq_is_refseq = false;
    if( ! GetCurrentBioseq()->IsSetId() ) {
        // false
        return IsRefseq_is_refseq;
    }
    const CBioseq::TId& bioseq_ids = GetCurrentBioseq()->GetId();
    ITERATE(CBioseq::TId, id_it, bioseq_ids) {
        const CSeq_id & seq_id = **id_it;
        if( seq_id.IsOther() ) {
            // for historical reasons, "other" means "refseq"
            IsRefseq_is_refseq = true;
            break;
        }
    }

    return IsRefseq_is_refseq;
}


bool CDiscrepancyContext::IsBGPipe()
{
    //static bool is_bgpipe = false;
    //static size_t count = 0;
    if (IsBGPipe_count == m_Count_Bioseq) {
        return IsBGPipe_is_bgpipe;
    }
    IsBGPipe_count = m_Count_Bioseq;
    IsBGPipe_is_bgpipe = false;

    CSeqdesc_CI user_desc_ci(
        m_Scope->GetBioseqHandle(*GetCurrentBioseq()), CSeqdesc::e_User);
    for( ; ! IsBGPipe_is_bgpipe && user_desc_ci; ++user_desc_ci) {
        const CUser_object & user_desc  = user_desc_ci->GetUser();
        // only look at structured comments
        if( ! FIELD_IS_SET_AND_IS(user_desc, Type, Str) ||
            ! NStr::EqualNocase(
                user_desc.GetType().GetStr(), "StructuredComment") )
        {
            continue;
        }

        CConstRef<CUser_field> struccmt_prefix_field =
            user_desc.GetFieldRef(
                "StructuredCommentPrefix", ".", NStr::eNocase);
        if( ! struccmt_prefix_field ||
            ! FIELD_IS_SET_AND_IS(*struccmt_prefix_field, Data, Str) ||
            ! NStr::EqualNocase(
                struccmt_prefix_field->GetData().GetStr(),
                "##Genome-Annotation-Data-START##") )
        {
            continue;
        }

        CConstRef<CUser_field> annot_pipeline_field =
            user_desc.GetFieldRef(
                "Annotation Pipeline", ".", NStr::eNocase);
        if( ! annot_pipeline_field ||
            ! FIELD_IS_SET_AND_IS(*annot_pipeline_field, Data, Str) ||
            ! NStr::EqualNocase(
                annot_pipeline_field->GetData().GetStr(),
                "NCBI Prokaryotic Genome Annotation Pipeline") )
        {
            continue;
        }

        IsBGPipe_is_bgpipe = true;
        return IsBGPipe_is_bgpipe;
    }

    return IsBGPipe_is_bgpipe;
}


const CSeq_feat* CDiscrepancyContext::GetCurrentGene() // todo: optimize
{
    return CCleanup::GetGeneForFeature(*m_Current_Seq_feat, *m_Scope);
}


void CDiscrepancyContext::ClearFeatureList(void)
{
    m_FeatGenes.clear();
    m_FeatPseudo.clear();
    m_FeatCDS.clear();
    m_FeatMRNAs.clear();
    m_FeatRRNAs.clear();
    m_FeatTRNAs.clear();
    m_Feat_RNAs.clear();
    m_FeatExons.clear();
    m_FeatIntrons.clear();
}


void CDiscrepancyContext::CollectFeature(const CSeq_feat& feat)
{
    switch (feat.GetData().GetSubtype()) {
        case CSeqFeatData::eSubtype_gene:
            m_FeatGenes.push_back(CConstRef<CSeq_feat>(&feat));
            break;
        case CSeqFeatData::eSubtype_cdregion:
            m_FeatCDS.push_back(CConstRef<CSeq_feat>(&feat));
            break;
        case CSeqFeatData::eSubtype_mRNA:
            m_FeatMRNAs.push_back(CConstRef<CSeq_feat>(&feat));
            break;
        case CSeqFeatData::eSubtype_tRNA:
            m_FeatTRNAs.push_back(CConstRef<CSeq_feat>(&feat));
            break;
        case CSeqFeatData::eSubtype_rRNA:
            m_FeatRRNAs.push_back(CConstRef<CSeq_feat>(&feat));
            break;
        case CSeqFeatData::eSubtype_exon:
            m_FeatExons.push_back(CConstRef<CSeq_feat>(&feat));
            break;
        case CSeqFeatData::eSubtype_intron:
            m_FeatIntrons.push_back(CConstRef<CSeq_feat>(&feat));
            break;
        case CSeqFeatData::eSubtype_misc_feature:
            m_FeatMisc.push_back(CConstRef<CSeq_feat>(&feat));
            break;
        default:
            break;
    }
    if (feat.GetData().IsRna()) {
        m_Feat_RNAs.push_back(CConstRef<CSeq_feat>(&feat));
    }
    if (CCleanup::IsPseudo(feat, *m_Scope)) {
        m_FeatPseudo.push_back(CConstRef<CSeq_feat>(&feat));
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CRef<CDiscrepancyObject> CDiscrepancyContext::NewBioseqObj(CConstRef<CBioseq> obj, const CSeqSummary* info, EKeepRef keep_ref, bool autofix, CObject* more)
{
    bool keep = keep_ref || m_KeepRef;
    CRef<CDiscrepancyObject> D(new CDiscrepancyObject(obj, *m_Scope, m_File, keep, autofix, more));
    const CSerialObject* ser = &*obj;
    if (m_TextMap.find(ser) == m_TextMap.end()) {
        D->SetText(*m_Scope);
        m_TextMap[ser] = D->GetText();
        m_TextMapShort[ser] = D->GetShort();
    }
    else {
        D->SetText(m_TextMap[ser]);
        D->SetShort(m_TextMapShort[ser]);
    }
    if (!keep) {
        D->DropReference();
    }
    return D;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::NewSeqdescObj(CConstRef<CSeqdesc> obj, const string& bslabel, EKeepRef keep_ref, bool autofix, CObject* more)
{
    bool keep = keep_ref || m_KeepRef;
    CRef<CDiscrepancyObject> D(new CDiscrepancyObject(obj, *m_Scope, m_File, keep, autofix, more));
    const CSerialObject* ser = &*obj;
    if (m_TextMap.find(ser) == m_TextMap.end()) {
        D->SetText(*m_Scope, bslabel);
        m_TextMap[ser] = D->GetText();
    }
    else {
        D->SetText(m_TextMap[ser]);
    }
    if (!keep) {
        D->DropReference();
    }
    return D;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::NewDiscObj(CConstRef<CSeq_feat> obj, EKeepRef keep_ref, bool autofix, CObject* more)
{
    bool keep = keep_ref || m_KeepRef;
    CRef<CDiscrepancyObject> D(new CDiscrepancyObject(obj, *m_Scope, m_File, keep, autofix, more));
    const CSerialObject* ser = &*obj;
    if (m_TextMap.find(ser) == m_TextMap.end()) {
        D->SetText(*m_Scope);
        m_TextMap[ser] = D->GetText();
    }
    else {
        D->SetText(m_TextMap[ser]);
    }
    if (!keep) {
        D->DropReference();
    }
    return D;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::NewDiscObj(CConstRef<CBioseq_set> obj, EKeepRef keep_ref, bool autofix, CObject* more)
{
    bool keep = keep_ref || m_KeepRef;
    CRef<CDiscrepancyObject> D(new CDiscrepancyObject(obj, *m_Scope, m_File, keep, autofix, more));
    const CSerialObject* ser = &*obj;
    if (m_TextMap.find(ser) == m_TextMap.end()) {
        D->SetText(*m_Scope);
        m_TextMap[ser] = D->GetText();
    }
    else {
        D->SetText(m_TextMap[ser]);
    }
    if (!keep) {
        D->DropReference();
    }
    return D;
}


CRef<CDiscrepancyObject> CDiscrepancyContext::NewSubmitBlockObj(EKeepRef keep_ref, bool autofix, CObject* more)
{
    return CRef<CDiscrepancyObject>(new CSubmitBlockDiscObject(m_Current_Submit_block, m_Current_Submit_block_StringObj, *m_Scope, "Submit Block", m_File, keep_ref || m_KeepRef, autofix, more));
}


CRef<CDiscrepancyObject> CDiscrepancyContext::NewCitSubObj(EKeepRef keep_ref, bool autofix, CObject* more)
{
    return CRef<CDiscrepancyObject>(new CSubmitBlockDiscObject(m_Current_Submit_block, m_Current_Cit_sub_StringObj, *m_Scope, "Cit-sub", m_File, keep_ref || m_KeepRef, autofix, more));
}


CRef<CDiscrepancyObject> CDiscrepancyContext::NewFeatOrDescObj(EKeepRef keep_ref, bool autofix, CObject* more)
{
    CConstRef<CSeq_feat> feat = GetCurrentSeq_feat();
    CConstRef<CSeqdesc> desc = GetCurrentSeqdesc();
    _ASSERT(!feat != !desc);
    if (feat) {
        return NewDiscObj(feat, keep_ref, autofix, more);
    }
    else if (desc) {
        return NewSeqdescObj(desc, GetCurrentBioseqLabel(), keep_ref, autofix, more);
    }
}


CRef<CDiscrepancyObject> CDiscrepancyContext::NewFeatOrDescOrSubmitBlockObj(EKeepRef keep_ref, bool autofix, CObject* more)
{
    CConstRef<CSeq_feat> feat = GetCurrentSeq_feat();
    CConstRef<CSeqdesc> desc = GetCurrentSeqdesc();
    _ASSERT(!feat || !desc);
    if (feat) {
        return NewDiscObj(feat, keep_ref, autofix, more);
    }
    else if (desc) {
        return NewSeqdescObj(desc, GetCurrentBioseqLabel(), keep_ref, autofix, more);
    }
    else {
        return NewSubmitBlockObj(keep_ref, autofix, more);
    }
}


CRef<CDiscrepancyObject> CDiscrepancyContext::NewFeatOrDescOrCitSubObj(EKeepRef keep_ref, bool autofix, CObject* more)
{
    CConstRef<CSeq_feat> feat = GetCurrentSeq_feat();
    CConstRef<CSeqdesc> desc = GetCurrentSeqdesc();
    _ASSERT(!feat || !desc);
    if (feat) {
        return NewDiscObj(feat, keep_ref, autofix, more);
    }
    else if (desc) {
        return NewSeqdescObj(desc, GetCurrentBioseqLabel(), keep_ref, autofix, more);
    }
    else {
        return NewCitSubObj(keep_ref, autofix, more);
    }
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
