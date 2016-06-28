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

DISCREPANCY_CASE(RETROVIRIDAE_DNA, CSeqdesc_BY_BIOSEQ, eOncaller, "Retroviridae DNA")
{
//cout<<"CSeqdesc_BY_BIOSEQ\n";
}


DISCREPANCY_SUMMARIZE(RETROVIRIDAE_DNA)
{
}


// NON_RETROVIRIDAE_PROVIRAL

DISCREPANCY_CASE(NON_RETROVIRIDAE_PROVIRAL, CSeqdesc_BY_BIOSEQ, eOncaller, "Non-Retroviridae biosources that are proviral")
{
//cout<<"CSeqdesc_BY_BIOSEQ\n";
}


DISCREPANCY_SUMMARIZE(NON_RETROVIRIDAE_PROVIRAL)
{
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

DISCREPANCY_CASE(ORGANELLE_NOT_GENOMIC, CSeqdesc_BY_BIOSEQ, eDisc|eOncaller, "Organelle location should have genomic moltype")
{
//cout<<"CSeqdesc_BY_BIOSEQ\n";
}


DISCREPANCY_SUMMARIZE(ORGANELLE_NOT_GENOMIC)
{
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
