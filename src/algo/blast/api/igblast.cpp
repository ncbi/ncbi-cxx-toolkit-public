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

    /*** search V germline */
    x_SetupVSearch(qf, opts_hndl);
    CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[0]);
    results[0] = blast.Run();
    s_AnnotateV(results[0], annots);

    /*** search germline database */
    int num_genes =  (m_IgOptions->m_IsProtein) ? 1 : 3;
    if (num_genes > 1) {
        x_SetupDJSearch(annots, qf, opts_hndl);
        for (int gene = 1; gene < num_genes; ++gene) {
            CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[gene]);
            results[gene] = blast.Run();
        }
        s_AnnotateDJ(results[1], results[2], annots);
    }

    for (int gene = 0; gene  < num_genes; ++gene) {
        s_AppendResults(results[gene], m_IgOptions->m_NumAlign[gene], final_results);
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
    s_AppendResults(result, -1, final_results);

    /*** search germline db for region annotation */
    // TODO set up germline search for db[3]
    s_AnnotateDomain(result, annots);
    s_SetAnnotation(annots, final_results);

    return final_results;
};

void CIgBlast::x_SetupVSearch(CRef<IQueryFactory>       &qf,
                              CRef<CBlastOptionsHandle> &opts_hndl)
{
    if (m_IgOptions->m_IsProtein) {
        // TODO: no composition based.
    } else {
        CBlastOptions & opts = opts_hndl->SetOptions();
        int penalty = m_Options->GetOptions().GetMismatchPenalty();
        opts.SetMatchReward(1);
        opts.SetMismatchPenalty(penalty);
        opts.SetWordSize(7);
        if (penalty == -1) {
            opts.SetGapOpeningCost(4);
            opts.SetGapExtensionCost(1);
        }
    }
    opts_hndl->SetEvalueThreshold(10.0);
    opts_hndl->SetFilterString("F");
    opts_hndl->SetHitlistSize(5);
    qf.Reset(new CObjMgr_QueryFactory(*m_Query));
};

void CIgBlast::x_SetupDJSearch(vector<CRef <CIgAnnotation> > &annots,
                               CRef<IQueryFactory>           &qf,
                               CRef<CBlastOptionsHandle>     &opts_hndl)
{
    // Only igblastn will search DJ
    CBlastOptions & opts = opts_hndl->SetOptions();
    opts.SetMatchReward(1);
    opts.SetMismatchPenalty(-3);
    opts.SetWordSize(5);
    opts.SetGapOpeningCost(5);
    opts.SetGapExtensionCost(2);
    opts_hndl->SetEvalueThreshold(1000.0);
    opts_hndl->SetFilterString("F");
    opts_hndl->SetHitlistSize(10);

    // Mask query for D, J search
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

void CIgBlast::s_AnnotateV(CRef<CSearchResultSet>        &results, 
                           vector<CRef <CIgAnnotation> > &annots)
{
    ITERATE(CSearchResultSet, result, *results) {

        CIgAnnotation *annot = new CIgAnnotation();
        annots.push_back(CRef<CIgAnnotation>(annot));
 
        if ((*result)->HasAlignments()) {
            CRef<CSeq_align> align = (*result)->GetSeqAlign()->Get().front();
            annot->m_GeneInfo[0] = align->GetSeqStart(0);
            annot->m_GeneInfo[1] = align->GetSeqStop(0)+1;
            if (align->GetSeqStrand(0) == eNa_strand_minus) {
                annot->m_MinusStrand = true;
            }
        } 
    }
};

// Test if the alignment is already in the align_list
static bool s_SeqAlignInSet(CSeq_align_set::Tdata & align_list, CRef<CSeq_align> &align)
{
    ITERATE(CSeq_align_set::Tdata, it, align_list) {
        if ((*it)->GetSeq_id(1).Match(align->GetSeq_id(1)) &&
            (*it)->GetSeqStart(1) == align->GetSeqStart(1) &&
            (*it)->GetSeqStop(1) == align->GetSeqStop(1)) return true;
    }
    return false;
};

// Compare two seqaligns according to their evalue and coverage
static bool s_CompareSeqAlign(CRef<CSeq_align> &x, CRef<CSeq_align> &y)
{
    int sx, sy;
    x->GetNamedScore(CSeq_align::eScore_Score, sx);
    y->GetNamedScore(CSeq_align::eScore_Score, sy);
    if (sx != sy) return (sx > sy);
    x->GetNamedScore(CSeq_align::eScore_IdentityCount, sx);
    y->GetNamedScore(CSeq_align::eScore_IdentityCount, sy);
    return (sx <= sy);
};

void CIgBlast::s_AnnotateDJ(CRef<CSearchResultSet>        &results_D,
                            CRef<CSearchResultSet>        &results_J,
                            vector<CRef <CIgAnnotation> > &annots)
{
    /* Here is the D J identification logic from Jian:
       First we find J segment based on the following rule (we keep 5 hits at this stage):

    1.  J segment needs to be on the plus strand (assuming query is the strand that 
        contains the plus strand of germline V genes).
    2.  J segment stop must be after the V gene end.  J start must be within 60 bp after V gene end.
    3.  Subject J segment sequence should contain at least the first 32 bp.

    To find D segment (5 hits at this stage):

    1.  D segment stop must be after V gene end and D segment start must be within 30 bp 
        after V gene end.
    2.  Query (assuming query is the strand that contains the plus strand of germline V genes) 
        can only have D match on minus strand if no any plus strand match is found.

    Apply position consistency rule:
    To resolve consistency between D and J gene match, we need to make sure that D start is at 
    least 3 bp before J start and D stop is at least 3 bp before J end.  So, if ALL D genes and 
    J genes are found to satisfy this rule, then we report D and J genes as is (numbers can be 
    specified by user).  */

    int iq = 0;
    ITERATE(CSearchResultSet, result, *results_J) {

        CIgAnnotation *annot = &*(annots[iq++]);
 
        if ((*result)->HasAlignments()) {

            CRef<CSeq_align_set> align(const_cast<CSeq_align_set *>
                                           (&*((*result)->GetSeqAlign())));
            CSeq_align_set::Tdata & align_list = align->Set();
            align_list.sort(s_CompareSeqAlign);

            CSeq_align_set::Tdata::iterator it = align_list.begin();

            int V_end = (annot->m_MinusStrand) ? 
                         annot->m_GeneInfo[0] : annot->m_GeneInfo[1] - 1;
            ENa_strand strand = (annot->m_MinusStrand) ? eNa_strand_minus : eNa_strand_plus;

            bool annotated = false;
            while (it != align_list.end()) {
                
                bool keep = true;
                if (annot->m_MinusStrand) {
                    if ((*it)->GetSeqStrand(0) != strand                ||
                        (int)(*it)->GetSeqStop(0)   <  V_end - 60       ||
                        (int)(*it)->GetSeqStart(0)  >  V_end            ||
                        (int)(*it)->GetSeqStart(1)  >  32) keep = false;
                } else {
                    if ((*it)->GetSeqStrand(0) != strand                ||
                        (int)(*it)->GetSeqStart(0)  >  V_end + 60       ||
                        (int)(*it)->GetSeqStop(0)   <  V_end            ||
                        (int)(*it)->GetSeqStart(1)  >  32) keep = false;
                }

                if (!keep) {
                     it = align_list.erase(it);
                } else {
                     if (!annotated) {
                         annot->m_GeneInfo[4] = (*it)->GetSeqStart(0);
                         annot->m_GeneInfo[5] = (*it)->GetSeqStop(0)+1;
                         annotated = true;
                     }
                     ++it;
                }
            }
        }
    }

    iq = 0;
    ITERATE(CSearchResultSet, result, *results_D) {

        CIgAnnotation *annot = &*(annots[iq++]);
 
        if ((*result)->HasAlignments()) {

            CRef<CSeq_align_set> align(const_cast<CSeq_align_set *>
                                           (&*((*result)->GetSeqAlign())));
            CSeq_align_set::Tdata & align_list = align->Set();
            align_list.sort(s_CompareSeqAlign);

            ENa_strand strand = (annot->m_MinusStrand) ? eNa_strand_minus : eNa_strand_plus;
            bool strand_found = false;
            ITERATE(CSeq_align_set::Tdata, itt, align_list) {
                if ((*itt)->GetSeqStrand(0) == strand) {
                    strand_found = true;
                    break;
                }
            }
            CSeq_align_set::Tdata::iterator it = align_list.begin();
            if (strand_found) {
                while (it != align_list.end()) {
                    if ((*it)->GetSeqStrand(0) != strand) {
                        it = align_list.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        
            int V_end = (annot->m_MinusStrand) ? 
                         annot->m_GeneInfo[0] : annot->m_GeneInfo[1] - 1;
            int J_begin = annot->m_GeneInfo[4];
            int J_end = annot->m_GeneInfo[5] - 1;
            bool annotated = false;
            it = align_list.begin();
            while (it != align_list.end()) {
                
                bool keep = true;
                if (annot->m_MinusStrand) {
                    if ((int)(*it)->GetSeqStop(0)   <  V_end - 30 ||
                        (int)(*it)->GetSeqStart(0)  >  V_end) keep = false;
                    if (J_begin>=0 &&
                        ((int)(*it)->GetSeqStart(0) <  J_begin + 3 ||
                         (int)(*it)->GetSeqStop(0)  <  J_end + 3)) keep = false;
                } else {
                    if ((int)(*it)->GetSeqStart(0)  >  V_end + 30 ||
                        (int)(*it)->GetSeqStop(0)   <  V_end) keep = false;
                    if (J_begin>=0 &&
                        ((int)(*it)->GetSeqStart(0) >  J_begin - 3 ||
                         (int)(*it)->GetSeqStop(0)  >  J_end - 3)) keep = false; 
                }

                if (!keep) {
                     it = align_list.erase(it);
                } else {
                     if (!annotated) {
                         annot->m_GeneInfo[2] = (*it)->GetSeqStart(0);
                         annot->m_GeneInfo[3] = (*it)->GetSeqStop(0)+1;
                         annotated = true;
                     }
                     ++it;
                }
            }
        }
    }
};

void CIgBlast::s_AnnotateDomain(CRef<CSearchResultSet>        &results, 
                                vector<CRef <CIgAnnotation> > &annots)
{
    //TODO fill up m_ChainType, m_FrameInfo, m_DomainInfo
};

void CIgBlast::s_AppendResults(CRef<CSearchResultSet> &results,
                               int                     num_aligns,
                               CRef<CSearchResultSet> &final_results)
{
    bool  new_result = (final_results.Empty());
    if (new_result) {
        final_results.Reset(new CSearchResultSet());
    }

    int iq = 0;
    ITERATE(CSearchResultSet, result, *results) {

        CRef<CSeq_align_set> align;

        if ((*result)->HasAlignments()) {

            // keep only the first num_alignments
            align.Reset(const_cast<CSeq_align_set *>
                                   (&*((*result)->GetSeqAlign())));
            if (num_aligns >= 0) {
                CSeq_align_set::Tdata & align_list = align->Set();
                if (align_list.size() > num_aligns) {
                    CSeq_align_set::Tdata::iterator it = align_list.begin();
                    for (int i=0; i<num_aligns; ++i) ++it;
                    align_list.erase(it, align_list.end());
                }
            }
        }

        TQueryMessages errmsg = (*result)->GetErrors();

        if (new_result) {
            CConstRef<CSeq_id> query = (*result)->GetSeqId();
            // TODO maybe we need the db ancillary instead?
            CRef<CBlastAncillaryData> ancillary = (*result)->GetAncillaryData();
            CRef<CSearchResults> ig_result(
                  new CIgBlastResults(query, align, errmsg, ancillary));
            final_results->push_back(ig_result);
        } else if (!align.Empty()) {
            CIgBlastResults *ig_result = dynamic_cast<CIgBlastResults *>
                                         (&(*final_results)[iq++]);
            CSeq_align_set::Tdata & ig_list = ig_result->SetSeqAlign()->Set();
            // Remove duplicates first
            CSeq_align_set::Tdata & align_list = align->Set();
            CSeq_align_set::Tdata::iterator it = align_list.begin();
            while (it != align_list.end() && s_SeqAlignInSet(ig_list, *it)) ++it;
            if (it != align_list.begin())  align_list.erase(align_list.begin(), it);

            if (!align_list.empty()) {
                ig_list.insert(ig_list.end(), align_list.begin(), align_list.end());
                ig_result->GetErrors().Combine(errmsg);
            }
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
