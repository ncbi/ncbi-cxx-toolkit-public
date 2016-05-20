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

bool CDiscrepancyContext::HasLineage(const CBioSource& biosrc, const string& def_lineage, const string& type)
{
    return NStr::FindNoCase(def_lineage, type) != string::npos || def_lineage.empty() && biosrc.IsSetLineage() && NStr::FindNoCase(biosrc.GetLineage(), type) != string::npos;
}


bool CDiscrepancyContext::HasLineage(const string& lineage)
{
    static bool result = false;
    static size_t count = 0;
    if (count != m_Count_Bioseq) {
        count = m_Count_Bioseq;
        const CBioSource* biosrc = GetCurrentBiosource();
        if (biosrc) {
            result = HasLineage(*biosrc, GetLineage(), lineage);
            return result;
        }
        result = false;
    }
    return result;
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
    const CGene_ref* gene = m_Current_Seq_feat->GetGeneXref();
    if (gene && gene->IsSuppressed()) {
        return CConstRef <CSeq_feat>();
    }

    if (gene) {
        //CBioseq_Handle bioseq_hl = sequence::GetBioseqFromSeqLoc(feat.GetLocation(), scope);
        //if (!bioseq_hl) {
        //    return (CConstRef <CSeq_feat>());
        //}

        CTSE_Handle tse_hl = m_Current_Bioseq_Handle.GetTSE_Handle();
        if (gene->CanGetLocus_tag() && !(gene->GetLocus_tag().empty())) {
            CSeq_feat_Handle seq_feat_hl = tse_hl.GetGeneWithLocus(gene->GetLocus_tag(), true);
            if (seq_feat_hl) {
                return seq_feat_hl.GetOriginalSeq_feat();
            }
        }
        else if (gene->CanGetLocus() && !(gene->GetLocus().empty())) {
            CSeq_feat_Handle seq_feat_hl = tse_hl.GetGeneWithLocus(gene->GetLocus(), false);
            if (seq_feat_hl) {
                return seq_feat_hl.GetOriginalSeq_feat();
            }
        }
        else return CConstRef <CSeq_feat>();
    }
    else {
        return CConstRef <CSeq_feat>(sequence::GetBestOverlappingFeat(m_Current_Seq_feat->GetLocation(), CSeqFeatData::e_Gene, sequence::eOverlap_Contained, *m_Scope));
    }
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
