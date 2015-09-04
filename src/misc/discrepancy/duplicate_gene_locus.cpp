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
 * Authors: Igor Filippov based on Sema Kachalo's template
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include <objmgr/feat_ci.hpp>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(duplicate_gene_locus);


// DUPLICATE_GENE_LOCUS

DISCREPANCY_CASE(DUPLICATE_GENE_LOCUS, CBioseq, eNormal, "Duplicate Gene Locus")
{
// TODO Skip any Bioseqs that are an mRNA sequence in a GenProdSet
    CScope &scope =  context.GetScope();
    CBioseq_Handle bsh = scope.GetBioseqHandle(obj);
    map<string,int> count;

    for (CFeat_CI feat_ci(bsh, SAnnotSelector(CSeqFeatData::e_Gene)); feat_ci; ++feat_ci)
    {

        if (feat_ci->GetData().GetGene().IsSetLocus())
        {
            string locus = feat_ci->GetData().GetGene().GetLocus();

            count[locus]++;
        }
    }
    for (map<string,int>::const_iterator i = count.begin(); i != count.end(); ++i)
    {
        if (i->second > 1)
        {
            string str = "[n] genes have the same locus ";
            str += i->first;
            str += " as another gene";
            m_Objs["[n] genes have the same locus as another gene on the same Bioseq."][str].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
        }
    }
}


DISCREPANCY_SUMMARIZE(DUPLICATE_GENE_LOCUS)
{
    if (!m_Objs.empty())
    {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
    m_Objs.clear();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
