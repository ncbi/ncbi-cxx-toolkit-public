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


CConstRef<CBioseq> CDiscrepancyContext::GetCurrentBioseq(void) const 
{
    if (m_Current_Bioseq)
    {
        return m_Current_Bioseq;
    }
    else {
        CConstRef<CBioseq_set> BS = GetCurrentBioseq_set();
        if (BS && BS->IsSetClass())
        {
            CConstRef<CBioseq> bioseq;
            CScope &scope = GetScope();
            if (BS->GetClass() == CBioseq_set::eClass_nuc_prot)
            {
                bioseq.Reset(scope.GetBioseqHandle(BS->GetNucFromNucProtSet()).GetCompleteBioseq());
                return bioseq;
            }
            if (BS->GetClass() == CBioseq_set::eClass_gen_prod_set)
            {
                bioseq.Reset(scope.GetBioseqHandle(BS->GetGenomicFromGenProdSet()).GetCompleteBioseq());
                return bioseq;
            }
            if (BS->GetClass() == CBioseq_set::eClass_segset)
            {
                bioseq.Reset(scope.GetBioseqHandle(BS->GetMasterFromSegSet()).GetCompleteBioseq());
                return bioseq;
            }
        }
    }
    return m_Current_Bioseq;
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
    static const CBioSource* biosrc;
    static size_t count = 0;
    if (count != m_Count_Bioseq) {
        count = m_Count_Bioseq;
        biosrc = sequence::GetBioSource(m_Scope->GetBioseqHandle(*GetCurrentBioseq()));
    }
    return biosrc;
}

const CMolInfo* CDiscrepancyContext::GetCurrentMolInfo()
{
    static const CMolInfo* mol_info;
    static size_t count = 0;
    if (count != m_Count_Bioseq) {
        count = m_Count_Bioseq;
        mol_info = sequence::GetMolInfo(m_Scope->GetBioseqHandle(*GetCurrentBioseq()));
    }
    return mol_info;
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
    static const CBioSource* biosrc = 0;
    static size_t count = 0;
    if (count != m_Count_Bioseq) {
        count = m_Count_Bioseq;
        biosrc = GetCurrentBiosource();
    }
    return biosrc ? HasLineage(*biosrc, GetLineage(), lineage) : false;
}


bool CDiscrepancyContext::IsEukaryotic()
{
    static bool result = false;
    static size_t count = 0;
    if (count != m_Count_Bioseq) {
        count = m_Count_Bioseq;
        const CBioSource* biosrc = GetCurrentBiosource();
        if (biosrc) {
            CBioSource::EGenome genome = (CBioSource::EGenome) biosrc->GetGenome();
            if (genome != CBioSource::eGenome_mitochondrion
                && genome != CBioSource::eGenome_chloroplast
                && genome != CBioSource::eGenome_plastid
                && genome != CBioSource::eGenome_apicoplast
                && HasLineage(*biosrc, GetLineage(), "Eukaryota")) {
                result = true;
                return result;
            }
        }
        result = false;
    }
    return result;
}


bool CDiscrepancyContext::IsBacterial()
{
    static bool result = false;
    static size_t count = 0;
    if (count != m_Count_Bioseq) {
        count = m_Count_Bioseq;
        const CBioSource* biosrc = GetCurrentBiosource();
        if (biosrc) {
            //CBioSource::EGenome genome = (CBioSource::EGenome) biosrc->GetGenome();
            if (HasLineage(*biosrc, GetLineage(), "Bacteria")) {
                result = true;
                return result;
            }
        }
        result = false;
    }
    return result;
}


bool CDiscrepancyContext::IsViral()
{
    static bool result = false;
    static size_t count = 0;
    if (count != m_Count_Bioseq) {
        count = m_Count_Bioseq;
        const CBioSource* biosrc = GetCurrentBiosource();
        if (biosrc) {
            CBioSource::EGenome genome = (CBioSource::EGenome) biosrc->GetGenome();
            if (HasLineage(*biosrc, GetLineage(), "Viruses")) {
                result = true;
                return result;
            }
        }
        result = false;
    }
    return result;
}


bool CDiscrepancyContext::IsPubMed()
{
    static bool result = false;
    static size_t count = 0;
    if (count != m_Count_Pub_equiv) {
        count = m_Count_Pub_equiv;
        result = false;
        const CPub_equiv* equiv = GetCurrentPub_equiv();
        if (equiv) {
            const CPub_equiv::Tdata& list = equiv->Get();
            ITERATE (CPub_equiv::Tdata, it, list) {
                if ((*it)->IsPmid()) {
                    result = true;
                    break;
                }
            }
        }
    }
    return result;
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
    static CBioSource::TGenome genome;
    static size_t count = 0;
    if (count != m_Count_Bioseq) {
        count = m_Count_Bioseq;
        const CBioSource* biosrc = GetCurrentBiosource();
        genome = biosrc ? biosrc->GetGenome() : CBioSource::eGenome_unknown;
    }
    return genome;
}


bool CDiscrepancyContext::SequenceHasFarPointers()
{
    static bool result = false;
    static size_t count = 0;
    if (count != m_Count_Bioseq) {
        count = m_Count_Bioseq;
        result = false;
        if (!GetCurrentBioseq()->CanGetInst() || !GetCurrentBioseq()->GetInst().CanGetExt() || !GetCurrentBioseq()->GetInst().GetExt().IsDelta()) {
            return result;
        }
        FOR_EACH_DELTASEQ_IN_DELTAEXT(it, GetCurrentBioseq()->GetInst().GetExt().GetDelta()) {
            if ((*it)->IsLoc()) {
                result = true;
                return result;
            }
        }
    }
    return result;
}


static void CountNucleotides(const CSeq_data& seq_data, CSeqSummary& ret)
{
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


const CSeqSummary& CDiscrepancyContext::GetNucleotideCount()
{
    static CSafeStatic<CSeqSummary> ret;
    static size_t count = 0;
    if (count == m_Count_Bioseq) {
        return ret.Get();
    }
    count = m_Count_Bioseq;
    ret->clear();

    // Make a Seq Map so that we can explicitly look at the gaps vs. the unknowns.
    const CRef<CSeqMap> seq_map = CSeqMap::CreateSeqMapForBioseq(*GetCurrentBioseq());
    SSeqMapSelector sel;
    sel.SetFlags(CSeqMap::fFindData | CSeqMap::fFindGap);
    CSeqMap_CI seq_iter(seq_map, &GetScope(), sel);
    for (; seq_iter; ++seq_iter) {
        switch (seq_iter.GetType()) {
            case CSeqMap::eSeqData:
                ret->Len += seq_iter.GetLength();
                CountNucleotides(seq_iter.GetData(), ret.Get());
                break;
            case CSeqMap::eSeqGap:
                ret->Len += seq_iter.GetLength();
                break;
            default:
                break;
        }
    }
    return ret.Get();
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

const CSeq_id *
CDiscrepancyContext::GetProteinId(void)
{
    static const CSeq_id * protein_id = NULL;
    static size_t count = 0;
    if (count == m_Count_Bioseq) {
        return protein_id;
    }
    count = m_Count_Bioseq;
    protein_id = NULL;
    if( ! GetCurrentBioseq()->IsSetId() ) {
        // NULL
        return protein_id;
    }
    const CBioseq::TId& bioseq_ids = GetCurrentBioseq()->GetId();
    ITERATE(CBioseq::TId, id_it, bioseq_ids) {
        const CSeq_id & seq_id = **id_it;
        if( seq_id.IsGeneral() && ! seq_id.GetGeneral().IsSkippable() ) {
            protein_id = &seq_id;
            break;
        }
    }
    return protein_id;
}

bool CDiscrepancyContext::IsRefseq(void)
{
    static bool is_refseq = false;
    static size_t count = 0;
    if (count == m_Count_Bioseq) {
        return is_refseq;
    }
    count = m_Count_Bioseq;

    is_refseq = false;
    if( ! GetCurrentBioseq()->IsSetId() ) {
        // false
        return is_refseq;
    }
    const CBioseq::TId& bioseq_ids = GetCurrentBioseq()->GetId();
    ITERATE(CBioseq::TId, id_it, bioseq_ids) {
        const CSeq_id & seq_id = **id_it;
        if( seq_id.IsOther() ) {
            // for historical reasons, "other" means "refseq"
            is_refseq = true;
            break;
        }
    }

    return is_refseq;
}

bool CDiscrepancyContext::IsBGPipe(void)
{
    static bool is_bgpipe = false;
    static size_t count = 0;
    if (count == m_Count_Bioseq) {
        return is_bgpipe;
    }
    count = m_Count_Bioseq;
    is_bgpipe = false;

    CSeqdesc_CI user_desc_ci(
        m_Scope->GetBioseqHandle(*GetCurrentBioseq()), CSeqdesc::e_User);
    for( ; ! is_bgpipe && user_desc_ci; ++user_desc_ci) {
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

        is_bgpipe = true;
        return is_bgpipe;
    }

    return is_bgpipe;
}


const CSeq_feat* CDiscrepancyContext::GetCurrentGene() // todo: optimize
{
    return CCleanup::GetGeneForFeature(*m_Current_Seq_feat, *m_Scope);
}


void CDiscrepancyContext::ClearFeatureList(void)
{
    m_FeatGenes.clear();
    m_FeatCDS.clear();
    m_FeatMRNAs.clear();
    m_FeatRRNAs.clear();
    m_FeatTRNAs.clear();
    m_Feat_RNAs.clear();
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
            //m_FeatMRNAs.push_back(CConstRef<CSeq_feat>(&feat)); // will add when needed...
            break;
        case CSeqFeatData::eSubtype_tRNA:
            m_FeatTRNAs.push_back(CConstRef<CSeq_feat>(&feat));
            break;
        case CSeqFeatData::eSubtype_rRNA:
            m_FeatRRNAs.push_back(CConstRef<CSeq_feat>(&feat));
            break;
        default:
            if (feat.GetData().IsRna()) {
                m_Feat_RNAs.push_back(CConstRef<CSeq_feat>(&feat));
            }
    }
}


CDiscrepancyObject* CDiscrepancyContext::NewSubmitBlockObj(EKeepRef keep_ref, bool autofix, CObject* more)
{
    string label;
    if (m_Current_Bioseq) {
        m_Current_Bioseq->GetLabel(&label, CBioseq::eContent);
    }
    else {
        CConstRef<CBioseq_set> seqset = GetCurrentBioseq_set();
        if (seqset) {
            seqset->GetLabel(&label, CBioseq_set::eContent);
        }
    }
    string text = "Cit-sub";
    if (!label.empty()) {
        text = text + " for " + label;
    }
    return new CDiscrepancyObject(m_Current_Submit_block, *m_Scope, text, m_File, keep_ref || m_KeepRef, autofix, more);
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
