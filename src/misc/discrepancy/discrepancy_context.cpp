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
#include <sstream>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/seq_macros.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>


BEGIN_NCBI_SCOPE;
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


static bool HasLineage(const CBioSource& biosrc, const string& def_lineage, const string& type)
{
    return NStr::FindNoCase(def_lineage, type) != string::npos || def_lineage.empty() && biosrc.IsSetLineage() && NStr::FindNoCase(biosrc.GetLineage(), type) != string::npos;
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


map<char, size_t>& CDiscrepancyContext::GetNucleotideCount()
{
    static map<char, size_t> ret;
    static size_t count = 0;
    if (count == m_Count_Bioseq) {
        return ret;
    }
    count = m_Count_Bioseq;
    ret.clear();
    CSeqVector seq_vec(*GetCurrentBioseq(), &GetScope(), CBioseq_Handle::eCoding_Iupac);
    ITERATE(CSeqVector, base, seq_vec) {
        ret[*base]++;
    }
    return ret;
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
