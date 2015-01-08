/*  $Id$
 * ===========================================================================
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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Colleen Bollin
 * Created:  17/09/2013 15:38:53
 */


#ifndef _MISSING_GENES_H_
#define _MISSING_GENES_H_

#include <corelib/ncbistd.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <misc/discrepancy_report/analysis_process.hpp>


BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
BEGIN_SCOPE(DiscRepNmSpc);


class CMissingGenes : public CAnalysisProcess
{
public:
    CMissingGenes();
    ~CMissingGenes() {};

    virtual void CollectData(CSeq_entry_Handle seh, const CReportMetadata& metadata);
    virtual void CollateData();
    virtual void ResetData();
    virtual TReportItemList GetReportItems(CReportConfig& cfg) const;
    virtual vector<string> Handles() const;
    virtual void DropReferences(CScope& scope);

    void CollectData(CBioseq_Handle bh, const string& filename);

protected:
    typedef map<const CSeq_feat*, bool > TGenesUsedMap;

    void x_CheckGenesForFeatureType(CBioseq_Handle bh, 
                                    TGenesUsedMap& genes,
                                    CSeqFeatData::ESubtype subtype,
                                    bool makes_gene_not_superfluous, 
                                    const string& filename);

    void x_CheckGenesForFeatureType(CBioseq_Handle bh, 
                                    TGenesUsedMap& genes,
                                    CSeqFeatData::E_Choice choice,
                                    bool makes_gene_not_superfluous, 
                                    const string& filename);

    bool x_IsOkSuperfluousGene (const CSeq_feat& gene);
    bool x_IsPseudoGene(const CSeq_feat& gene);
    bool x_GeneHasNoteOrDesc(const CSeq_feat& gene);

    TReportObjectList m_SuperfluousPseudoGenes;
    TReportObjectList m_SuperfluousNonPseudoNonFrameshiftGenes;
    TReportObjectList m_MissingGenes;
};


END_SCOPE(DiscRepNmSpc);
END_NCBI_SCOPE

#endif 
