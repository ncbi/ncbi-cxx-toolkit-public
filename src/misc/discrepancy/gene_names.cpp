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
#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(gene_names);


// BAD_GENE_NAME
static bool HasBadWord(const string& s)
{
    static const string BadWords[] = { "putative", "fragment", "gene", "orf", "like" };
    for (size_t i = 0; i < ArraySize(BadWords); i++) {
        if (NStr::FindNoCase(s, BadWords[i]) != string::npos) {
            return true;
        }
    }
    return false;
}


static bool Has4Numbers(const string& s)
{
    size_t n = 0;
    for (size_t i = 0; i < s.size() && n < 4; i++) {
        n = isdigit(s[i]) ? n+1 : 0;
    }
    return n >= 4;
};


DISCREPANCY_CASE(BAD_GENE_NAME, CSeqFeatData, eAll, "Bad gene name")
{
    if (!obj.IsGene() || !obj.GetGene().CanGetLocus()) {
        return;
    }
    string locus = obj.GetGene().GetLocus();
    if (locus.size() > 10 || Has4Numbers(locus) || HasBadWord(locus)) {
        m_Objs["[n] gene[s] contain[S] suspect phrase or characters"].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), true));
    }
}


DISCREPANCY_SUMMARIZE(BAD_GENE_NAME)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// BAD_BACTERIAL_GENE_NAME
DISCREPANCY_CASE(BAD_BACTERIAL_GENE_NAME, CSeqFeatData, eAll, "Bad bacterial gene name")
{
    if (!obj.IsGene() || !obj.GetGene().CanGetLocus() || context.IsEukaryotic()) {
        return;
    }
    string locus = obj.GetGene().GetLocus();
    if (!isalpha(locus[0]) || !islower(locus[0])) {
        m_Objs["[n] bacterial gene[s] [does] not start with lowercase letter"].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), true)); // maybe false
    }
}


DISCREPANCY_SUMMARIZE(BAD_BACTERIAL_GENE_NAME)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
