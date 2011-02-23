#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 * ===========================================================================
 *
 * Author:  Ning Ma
 *
 */

/** @file igblast.cpp
 * Implementation of CIgBlast.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/igblast.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>


/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CRef<CSearchResultSet>
CIgBlast::Run()
{
    vector<CRef <CIgAnnotation> > annots;
    CRef<CSearchResultSet> final_results;
    CRef<IQueryFactory> qf;
    CRef<CBlastOptionsHandle> opts_hndl(CBlastOptionsFactory
           ::Create((m_IgOptions->m_IsProtein)? eBlastp: eBlastn));
    CRef<CSearchResultSet> results[3], result;

    /*** search germline database */
    int num_genes =  (m_IgOptions->m_IsProtein) ? 1 : 3;
    for (int gene = 0; gene < num_genes; ++gene) {
        x_SetupGLSearch(gene, annots, qf, opts_hndl);
        CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[gene]);
        results[gene] = blast.Run();
        s_AnnotateGene(gene, results[gene], annots);
    }

    /*** check and modify the germline search results */
    if (num_genes > 1) {
        s_CheckGeneAnnotations(results, annots);
    }

    for (int gene = 0; gene  < num_genes; ++gene) {
        s_AppendResults(results[gene], final_results);
    }
    
    /*** search user specified db */
    x_SetupDbSearch(annots, qf);
    if (m_IsLocal) {
        CLocalBlast blast(qf, m_Options, m_LocalDb);
        // blast.SetNumberOfThreads(num_threads);
        result = blast.Run();
    } else {
        CRef<CRemoteBlast> blast;
        if (m_RemoteDb.NotEmpty()) {
            _ASSERT(m_Subject.Empty());
            blast.Reset(new CRemoteBlast(qf, m_Options, *m_RemoteDb));
        } else {
            blast.Reset(new CRemoteBlast(qf, m_Options, m_Subject));
        }
        result = blast->GetResultSet();
    }
    s_AppendResults(result, final_results);

    /*** search germline db for region annotation */
    // TODO set up germline search fo db[3]
    s_AnnotateDomain(result, annots);
    s_SetAnnotation(annots, final_results);

    return final_results;
};

void CIgBlast::x_SetupGLSearch(int                            gene,
                               vector<CRef <CIgAnnotation> > &annots,
                               CRef<IQueryFactory>           &qf,
                               CRef<CBlastOptionsHandle>     &opts_hndl)
{
    // Tweak the options suited for V, D, J search
    if (m_IgOptions->m_IsProtein) {
        // TODO: no composition based.
    } else {
        CBlastOptions & opts = opts_hndl->SetOptions();
        int penalty = (gene) ? -1 : m_Options->GetOptions().GetMismatchPenalty();
        opts.SetMatchReward(1);
        opts.SetMismatchPenalty(penalty);
        opts.SetWordSize((gene)? 5: 7);
        if (gene) {
            opts.SetGapOpeningCost(5);
            opts.SetGapExtensionCost(2);
        } else if (penalty == -1) {
            opts.SetGapOpeningCost(4);
            opts.SetGapExtensionCost(1);
        }
    }
    opts_hndl->SetEvalueThreshold((gene) ? 1000.0 : 10.0);
    opts_hndl->SetFilterString("F");
    opts_hndl->SetHitlistSize(m_IgOptions->m_NumAlign[gene]);

    // Mask query for D, J search
    if (gene == 1) {
        int iq = 0;
        ITERATE(vector<CRef <CIgAnnotation> >, annot, annots) {
            CRef<CBlastSearchQuery> query = m_Query->GetBlastSearchQuery(iq);
            CSeq_id *q_id = const_cast<CSeq_id *>(&*query->GetQueryId());
            int len = query->GetLength();
            if ((*annot)->m_GeneInfo[0] == -1) {
                // This is not a germline sequence.  Mask it out
                TMaskedQueryRegions mask_list;
                CRef<CSeqLocInfo> mask(
                      new CSeqLocInfo(new CSeq_interval(*q_id, 0, len-1), 0));
                mask_list.push_back(mask);
                m_Query->SetMaskedRegions(iq, mask_list);
            } else {
                // Excluding the V gene except the last 7 bp for D and J gene search;
                // also limit the J match to 150bp beyond V gene.
                bool ms = (*annot)->m_MinusStrand;
                int begin = (ms)? 
                  (*annot)->m_GeneInfo[0] - 150: (*annot)->m_GeneInfo[1] - 7;
                int end = (ms)? 
                  (*annot)->m_GeneInfo[0] + 7: (*annot)->m_GeneInfo[1] + 150;
                if (begin > 0) {
                    CRef<CSeqLocInfo> mask(
                      new CSeqLocInfo(new CSeq_interval(*q_id, 0, begin-1), 0));
                    m_Query->AddMask(iq, mask);
                }
                if (end < len) {
                    CRef<CSeqLocInfo> mask(
                      new CSeqLocInfo(new CSeq_interval(*q_id, end, len-1), 0));
                    m_Query->AddMask(iq, mask);
                }
            }
            ++iq;
        }
    }

    // Generate query factory
    qf.Reset(new CObjMgr_QueryFactory(*m_Query));
};

void CIgBlast::x_SetupDbSearch(vector<CRef <CIgAnnotation> > &annots,
                               CRef<IQueryFactory>           &qf)
{
    // Options already passed in as m_Options.  Only set up (mask) the query
    int iq = 0;
    ITERATE(vector<CRef <CIgAnnotation> >, annot, annots) {
        CRef<CBlastSearchQuery> query = m_Query->GetBlastSearchQuery(iq);
        CSeq_id *q_id = const_cast<CSeq_id *>(&*query->GetQueryId());
        int len = query->GetLength();
        TMaskedQueryRegions mask_list;
        if ((*annot)->m_GeneInfo[0] ==-1) {
            // This is not a germline sequence.  Mask it out
            CRef<CSeqLocInfo> mask(
                      new CSeqLocInfo(new CSeq_interval(*q_id, 0, len-1), 0));
            mask_list.push_back(mask);
        } else if (m_IgOptions->m_FocusV) {
            // Restrict to V gene 
            int begin = (*annot)->m_GeneInfo[0];
            int end = (*annot)->m_GeneInfo[1];
            if (begin > 0) {
                CRef<CSeqLocInfo> mask(
                      new CSeqLocInfo(new CSeq_interval(*q_id, 0, begin-1), 0));
                mask_list.push_back(mask);
            }
            if (end < len) {
                CRef<CSeqLocInfo> mask(
                      new CSeqLocInfo(new CSeq_interval(*q_id, end, len-1), 0));
                mask_list.push_back(mask);
            }
        }
        m_Query->SetMaskedRegions(iq, mask_list);
        ++iq;
    }
    qf.Reset(new CObjMgr_QueryFactory(*m_Query));
};

void CIgBlast::s_AnnotateGene(int                            gene,
                              CRef<CSearchResultSet>        &results, 
                              vector<CRef <CIgAnnotation> > &annots)
{
    int iq = 0;
    ITERATE(CSearchResultSet, result, *results) {

        CIgAnnotation *annot;
        if (gene == 0) {
            // create new one for V annotation
            annot = new CIgAnnotation();
            annots.push_back(CRef<CIgAnnotation>(annot));
        } else {
            annot = &*(annots[iq++]);
        }
 
        if ((*result)->HasAlignments()) {
            CRef<CSeq_align> align = (*result)->GetSeqAlign()->Get().front();
            annot->m_GeneInfo[gene*2] = align->GetSeqStart(0);
            annot->m_GeneInfo[gene*2+1] = align->GetSeqStop(0)+1;
            if (gene == 0 && align->GetSeqStrand(0) == eNa_strand_minus) {
                annot->m_MinusStrand = true;
            }
        } 
    }
};

void CIgBlast::s_CheckGeneAnnotations(CRef<CSearchResultSet> results[3], 
                                vector<CRef <CIgAnnotation> > &annots)
{
    //TODO
};

void CIgBlast::s_AnnotateDomain(CRef<CSearchResultSet>        &results, 
                                vector<CRef <CIgAnnotation> > &annots)
{
    //TODO fill up m_ChainType, m_FrameInfo, m_DomainInfo
};

void CIgBlast::s_AppendResults(CRef<CSearchResultSet> &results,
                               CRef<CSearchResultSet> &final_results)
{
    bool  new_result = (final_results.Empty());
    if (new_result) {
        final_results.Reset(new CSearchResultSet());
    }

    int iq = 0;
    ITERATE(CSearchResultSet, result, *results) {

        CRef<CSeq_align_set> align(const_cast<CSeq_align_set *>
                                   (&*((*result)->GetSeqAlign())));
        /*
        if (gene >= 0) {
            // For V, D and J, only keep the best alignment
            string sid = "";
            CSeq_align_set::Tdata & align_list = align->Set();
            CSeq_align_set::Tdata::iterator it = align_list.begin();
            while (it != align_list.end()) {
                const string new_sid = (*it)->GetSeq_id(1).AsFastaString();
                if (new_sid == sid) {
                    it = align_list.erase(it);
                } else {
                    sid = new_sid;
                    ++it;
                }
            }
        }*/

        TQueryMessages errmsg = (*result)->GetErrors();

        if (new_result) {
            CConstRef<CSeq_id> query = (*result)->GetSeqId();
            // TODO maybe we need the db ancillary instead?
            CRef<CBlastAncillaryData> ancillary = (*result)->GetAncillaryData();
            CRef<CSearchResults> ig_result(
                  new CIgBlastResults(query, align, errmsg, ancillary));
            final_results->push_back(ig_result);
        } else {
            CSearchResults *ig_result = const_cast<CSearchResults *>
                                         (&(*final_results)[iq++]);
            CSeq_align_set *ig_align = const_cast<CSeq_align_set *>
                                         (&*(ig_result->GetSeqAlign()));
            CSeq_align_set_Base::Tdata & align_list = ig_align->Set();
            align_list.insert(align_list.end(), align->Get().begin(), align->Get().end());
            ig_result->GetErrors().Combine(errmsg);
        } 
    }
};

void CIgBlast::s_SetAnnotation(vector<CRef <CIgAnnotation> > &annots,
                               CRef<CSearchResultSet> &final_results)
{
    int iq = 0;
    ITERATE(CSearchResultSet, result, *final_results) {
        CIgBlastResults *ig_result = dynamic_cast<CIgBlastResults *>
                                     (const_cast<CSearchResults *>(&**result));
        CIgAnnotation *annot = &*(annots[iq++]);
        ig_result->SetIgAnnotation().Reset(annot);
    }
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
