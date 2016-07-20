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
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/validator/utilities.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(seqdesc_per_bioseq);

// RETROVIRIDAE_DNA
static const std::string kGenomeNotProviral = "[n] Retroviridae biosource[s] on DNA sequences [is] not proviral";

DISCREPANCY_CASE(RETROVIRIDAE_DNA, CSeqdesc_BY_BIOSEQ, eOncaller, "Retroviridae DNA")
{
    if (!obj.IsSource() || !context.IsDNA()) {
        return;
    }
    
    const CBioSource& src = obj.GetSource();
    if (src.IsSetLineage() && context.HasLineage(src, src.GetLineage(), "Retroviridae")) {
        if (!src.IsSetGenome() || src.GetGenome() != CBioSource::eGenome_proviral) {

            m_Objs[kGenomeNotProviral].Add(*context.NewDiscObj(CConstRef<CSeqdesc>(&obj), eKeepRef));
        }
    }
}

DISCREPANCY_SUMMARIZE(RETROVIRIDAE_DNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

DISCREPANCY_AUTOFIX(RETROVIRIDAE_DNA)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (desc) {

            CSeqdesc* desc_handle = const_cast<CSeqdesc*>(desc); // Is there a way to do this without const_cast???
            desc_handle->SetSource().SetGenome(CBioSource::eGenome_proviral);

            n++;
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("RETROVIRIDAE_DNA: Genome set to proviral for [n] sequence[s]", n) : 0);
}


// NON_RETROVIRIDAE_PROVIRAL
static const std::string kGenomeProviral = "[n] non-Retroviridae biosource[s] [is] proviral";

DISCREPANCY_CASE(NON_RETROVIRIDAE_PROVIRAL, CSeqdesc_BY_BIOSEQ, eOncaller, "Non-Retroviridae biosources that are proviral")
{
    if (!obj.IsSource() || !context.IsDNA()) {
        return;
    }

    const CBioSource& src = obj.GetSource();
    if (src.IsSetLineage() && !context.HasLineage(src, src.GetLineage(), "Retroviridae")) {
        if (src.IsSetGenome() && src.GetGenome() == CBioSource::eGenome_proviral) {

            m_Objs[kGenomeProviral].Add(*context.NewDiscObj(CConstRef<CSeqdesc>(&obj), eKeepRef));
        }
    }
}


DISCREPANCY_SUMMARIZE(NON_RETROVIRIDAE_PROVIRAL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// BAD_MRNA_QUAL

DISCREPANCY_CASE(BAD_MRNA_QUAL, CSeqdesc_BY_BIOSEQ, eOncaller, "mRNA sequence contains rearranged or germline")
{
    if (!obj.IsSource() || !context.IsCurrentSequenceMrna()) {
        return;
    }
    const CBioSource& src = obj.GetSource();
    if (!src.IsSetSubtype()) {
        return;
    }
    ITERATE(CBioSource::TSubtype, s, src.GetSubtype()) {
        if ((*s)->IsSetSubtype() && ((*s)->GetSubtype() == CSubSource::eSubtype_germline || (*s)->GetSubtype() == CSubSource::eSubtype_rearranged)) {
            m_Objs["[n] mRNA sequence[s] [has] germline or rearranged qualifier"].Add(*context.NewDiscObj(CConstRef<CSeqdesc>(&obj)));
            return;
        }
    }
}


DISCREPANCY_SUMMARIZE(BAD_MRNA_QUAL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// ORGANELLE_NOT_GENOMIC

static const std::string kOrganelleNotGenomic = "[n] non-genomic sequence[s] [is] organelle[s]";

DISCREPANCY_CASE(ORGANELLE_NOT_GENOMIC, CSeqdesc_BY_BIOSEQ, eDisc | eOncaller, "Organelle location should have genomic moltype")
{
    if (!obj.IsSource() || context.GetCurrentBioseq()->IsAa()) {
        return;
    }

    const CMolInfo* molinfo = context.GetCurrentMolInfo();
    if (molinfo == nullptr ||
        (!molinfo->IsSetBiomol() || molinfo->GetBiomol() == CMolInfo::eBiomol_genomic) && context.IsDNA())
        return;

    if (context.IsOrganelle())
        m_Objs[kOrganelleNotGenomic].Add(*context.NewDiscObj(CConstRef<CSeqdesc>(&obj)));
}


DISCREPANCY_SUMMARIZE(ORGANELLE_NOT_GENOMIC)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
