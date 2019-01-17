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
#include <algo/blast/igblast/igblast.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <algo/blast/composition_adjustment/composition_constants.h>
#include <objmgr/object_manager.hpp>


/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

static int max_allowed_VJ_distance_with_D = 150;
static int max_allowed_VJ_distance_without_D = 50;
static int max_allowed_VD_distance = 120;
static int extend_length = 30;
static int max_J_length = 70;
static int max_allowed_V_end_to_J_end = max_allowed_VJ_distance_with_D + max_J_length;
static int max_v_j_overlap = 7;
static int j_wordsize = 7;

static void s_ReadLinesFromFile(const string& fn, vector<string>& lines)
{
    CNcbiIfstream fs(fn.c_str(), IOS_BASE::in);
    lines.clear();

    if (CFile(fn).Exists() && ! fs.fail()) {
        char line[256];
        while(true) {
            fs.getline(line, 256);
            if (fs.eof()) break;
            if (line[0] == '#') continue;
            string l(line);
            lines.push_back(l);
        }
    }
    fs.close();
};

CIgAnnotationInfo::CIgAnnotationInfo(CConstRef<CIgBlastOptions> &ig_opt)
{
    vector<string> lines;

    // read domain info from pdm or ndm file
    const string suffix = (ig_opt->m_IsProtein) ? ".pdm." : ".ndm.";
    string fn(SeqDB_ResolveDbPath(ig_opt->m_IgDataPath + "/" + ig_opt->m_Origin + "/" 
                               + ig_opt->m_Origin + suffix + ig_opt->m_DomainSystem));
    if (fn == "") {
        NCBI_THROW(CBlastException,  eInvalidArgument, 
              "Domain annotation data file could not be found in [internal_data] directory");
    }
    s_ReadLinesFromFile(fn, lines);
    int index = 0;
    ITERATE(vector<string>, l, lines) {
        vector<string> tokens;
        NStr::Split(*l, " \t\n\r", tokens, NStr::fSplit_Tokenize);
        if (!tokens.empty()) {
            m_DomainIndex[tokens[0]] = index;
            for (int i=1; i<11; ++i) {
                m_DomainData.push_back(NStr::StringToInt(tokens[i]));
            }
            index += 10;
            m_DomainChainType[tokens[0]] = tokens[11];
            int frame = NStr::StringToInt(tokens[12]);
            if (frame != -1) {
                m_FrameOffset[tokens[0]] = frame;
            }
        } 
    }

    // read frame info from aux files
    if (ig_opt->m_IsProtein) return;
    fn = ig_opt->m_AuxFilename;
    s_ReadLinesFromFile(fn, lines);
    if (lines.size() == 0) {
        ERR_POST(Warning << "Auxilary data file could not be found");
    }
    ITERATE(vector<string>, l, lines) {
        vector<string> tokens;
        NStr::Split(*l, " \t\n\r", tokens, NStr::fSplit_Tokenize);
        if (!tokens.empty()) {
            int frame = NStr::StringToInt(tokens[1]);
            if (frame != -1) {
                m_FrameOffset[tokens[0]] = frame;
            }
            if (tokens.size() == 3) { //just backward compatible as there was no such field
                m_DJChainType[tokens[0]] = tokens[2];
            } else if (tokens.size() == 4) { //just backward compatible as there was no such field
                m_DJChainType[tokens[0]] = tokens[2];
                m_JDomainInfo[tokens[0]] = NStr::StringToInt(tokens[3]);
            }
        }
    }
};

void CIgBlast::x_ScreenByAlignLength(CRef<CSearchResultSet> & results, int length){
    NON_CONST_ITERATE(CSearchResultSet, result, *results) {
        if ((*result)->HasAlignments()) {
            CSeq_align_set::Tdata & align_list = (*result)->SetSeqAlign()->Set();
            CSeq_align_set::Tdata::iterator it = align_list.begin();
            while (it != align_list.end()) {
                if((int)((*it)->GetAlignLength()) - (int)((*it)->GetTotalGapCount(0)) < length){
                    it = align_list.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
}


void CIgBlast::x_ExtendAlign(CRef<CSearchResultSet> & results){
    
    NON_CONST_ITERATE(CSearchResultSet, result, *results) {

        if ((*result)->HasAlignments()) {
            CSeq_align_set::Tdata & align_list = (*result)->SetSeqAlign()->Set();
            int desired_len = 0; 
            int actual_len = 0;
            int top_hit_actual_len = 0;
            int count = 0;
            ENa_strand extend_strand = eNa_strand_plus;
            int highest_score = 0; 

            NON_CONST_ITERATE(CSeq_align_set::Tdata, align, align_list) {

                // cerr << "before=" << MSerial_AsnText << **align << endl;

                //extend germline match up to some positions at 5' end.  Extend length is 
                //set by comparing to top hit or top hit equivalents

                int score = 0;
                (*align)->GetNamedScore(CSeq_align::eScore_Score, score);
                
                if (score >= highest_score)  { //top hits
                    highest_score = score;
                    extend_strand = (*align)->GetSeqStrand(0);
                    desired_len = min(extend_length, 
                                      (*align)->GetSegs().GetDenseg().GetStarts()[1]);
                    
                    if ((*align)->GetSeqStrand(0) == eNa_strand_minus) {
                        int query_len = m_Scope->GetBioseqHandle((*align)->GetSeq_id(0)).GetBioseqLength();
                        int allowed_len = min ((*align)->GetSegs().GetDenseg().GetStarts()[1],
                                               query_len - ((*align)->GetSegs().GetDenseg().GetStarts()[0] +
                                                            (int)(*align)->GetSegs().GetDenseg().GetLens()[0]));
                        top_hit_actual_len = min(desired_len, allowed_len);
                        
                        
                    } else {
                        
                        top_hit_actual_len = min(desired_len,
                                         min((*align)->GetSegs().GetDenseg().GetStarts()[0],
                                             (*align)->GetSegs().GetDenseg().GetStarts()[1]));  
                    
                    }
                } 
                    
                if ((*align)->GetSeqStrand(0) == eNa_strand_minus) {
                    int query_len = m_Scope->GetBioseqHandle((*align)->GetSeq_id(0)).GetBioseqLength();
                    int allowed_len = min ((*align)->GetSegs().GetDenseg().GetStarts()[1],
                                           query_len - ((*align)->GetSegs().GetDenseg().GetStarts()[0] +
                                                        (int)(*align)->GetSegs().GetDenseg().GetLens()[0]));
                    actual_len = min(top_hit_actual_len, min(desired_len, allowed_len));
                    
                    
                } else {
                    
                    actual_len = min(top_hit_actual_len, min(desired_len,
                                                             min((*align)->GetSegs().GetDenseg().GetStarts()[0],
                                                                 (*align)->GetSegs().GetDenseg().GetStarts()[1])));  
                                     
                }
            
                count ++;
                //only extend if it has the same strand as the top hit
                if (actual_len > 0 && (*align)->GetSeqStrand(0) == extend_strand) {
                    if (extend_strand == eNa_strand_minus) {
                       
                        (*align)->SetSegs().SetDenseg().SetStarts()[1] -= actual_len;
                        (*align)->SetSegs().SetDenseg().SetLens()[0] += actual_len; 

                    } else {
                    
                        (*align)->SetSegs().SetDenseg().SetStarts()[0] -= actual_len;
                        (*align)->SetSegs().SetDenseg().SetStarts()[1] -= actual_len;
                        (*align)->SetSegs().SetDenseg().SetLens()[0] += actual_len;
                       
                    } 
                }
                // cerr << "after=" << MSerial_AsnText << **align << endl;
            }
               
        }
    }
}

CRef<CSearchResultSet>
CIgBlast::Run()
{
    vector<CRef <CIgAnnotation> > annots;
    CRef<CSearchResultSet> final_results;
    CRef<IQueryFactory> qf;
    CRef<CBlastOptionsHandle> opts_hndl(CBlastOptionsFactory
           ::Create((m_IgOptions->m_IsProtein)? eBlastp: eBlastn));
    CRef<CSearchResultSet> results[4], result;

    /*** search V germline */
    {
        x_SetupVSearch(qf, opts_hndl);
        CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[0]);
         blast.SetNumberOfThreads(m_NumThreads);
        results[0] = blast.Run();
        if (m_IgOptions->m_ExtendAlign){
            x_ExtendAlign(results[0]);
        }
        x_ScreenByAlignLength(results[0], m_IgOptions->m_MinVLength);
        x_ConvertResultType(results[0]);
        s_SortResultsByEvalue(results[0]);
        x_AnnotateV(results[0], annots);
    }

    /*** search internal V for domain annotation */
    {
        opts_hndl->SetHitlistSize(20);  // use a larger number to ensure annotation
        CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[3]);
        blast.SetNumberOfThreads(m_NumThreads);
        results[3] = blast.Run();
        if (m_IgOptions->m_ExtendAlign){
            x_ExtendAlign(results[3]);
        }
        x_ScreenByAlignLength(results[3], m_IgOptions->m_MinVLength);
        s_SortResultsByEvalue(results[3]);
        x_AnnotateDomain(results[0], results[3], annots);
    }

    opts_hndl.Reset(CBlastOptionsFactory
                     ::Create((m_IgOptions->m_IsProtein)? eBlastp: eBlastn));

    /*** search DJ germline */
    int num_genes =  (m_IgOptions->m_IsProtein) ? 1 : 3;
    if (num_genes > 1) {
        
        for (int gene = 1; gene < num_genes; ++gene) {
            x_SetupDJSearch(annots, qf, opts_hndl, gene);
            CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[gene]);
            try {
                blast.SetNumberOfThreads(m_NumThreads);
                results[gene] = blast.Run();
                if (gene == 2){
                    x_ScreenByAlignLength(results[gene], m_IgOptions->m_MinJLength);
                }
                x_ConvertResultType(results[gene]);
            } catch(...) {
                num_genes = 1;
                break;
            }
        }
        x_ProcessDJResult(results[0], results[1], results[2], annots);
       
        if (m_IgOptions->m_DetectOverlap && m_IgOptions->m_J_penalty == -3 && m_IgOptions->m_D_penalty == -4){
            x_AnnotateDJ(results[1], results[2],  annots);
        } else {
            x_AnnotateJ(results[2],  annots);
            //redo d gene search and not allow dj overlap
            x_SetupNoOverlapDSearch(annots, results[1], qf, opts_hndl, 1);
            CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[1]);
            try {
                blast.SetNumberOfThreads(m_NumThreads);
                results[1] = blast.Run();
                
                x_ConvertResultType(results[1]);
            } catch(...) {
                cerr << "blast failed" << endl;
            }
            x_ProcessDGeneResult(results[0], results[1], results[2],annots);
            x_AnnotateD(results[1], annots);
        } 
    }

    /*** collect germline search results */
    for (int gene = 0; gene  < num_genes; ++gene) {
        s_AppendResults(results[gene], m_IgOptions->m_NumAlign[gene], gene, final_results);
    }

    /*** search user specified db */
    bool skipped = false;
    if (m_IsLocal) {
        if (&(*m_LocalDb) != &(*(m_IgOptions->m_Db[0]))) {
            x_SetupDbSearch(annots, qf);
            CLocalBlast blast(qf, m_Options, m_LocalDb);
            blast.SetNumberOfThreads(m_NumThreads);
            result = blast.Run();
        } else {
            skipped = true;
        }
    } else {
        x_SetupDbSearch(annots, qf);
        CRef<CRemoteBlast> blast;
        if (m_RemoteDb.NotEmpty()) {
            _ASSERT(m_Subject.Empty());
            blast.Reset(new CRemoteBlast(qf, m_Options, *m_RemoteDb));
            if(m_EntrezQuery != NcbiEmptyString){
                blast->SetEntrezQuery(m_EntrezQuery.c_str());
            }
        } else {
            blast.Reset(new CRemoteBlast(qf, m_Options, m_Subject));
        }
        result = blast->GetResultSet();
    }
    if (! skipped) {
        x_ConvertResultType(result);
        s_SortResultsByEvalue(result);
        s_AppendResults(result, -1, -1, final_results);
    }

    /*** set chain type info */
    x_SetChainType(final_results, annots);

    /*** attach annotation info back to the results */
    x_SetAnnotation(annots, final_results);

    return final_results;
};

// Compare two seqaligns according to their evalue and coverage and name 
//compare name since blast does not guarantee order of same score hits
static bool s_CompareSeqAlignByScoreAndName(const CRef<CSeq_align> &x, const CRef<CSeq_align> &y)
{
    int sx, sy;
    x->GetNamedScore(CSeq_align::eScore_Score, sx);
    y->GetNamedScore(CSeq_align::eScore_Score, sy);
    if (sx != sy) return (sx > sy);

    sx = x->GetAlignLength();
    sy = y->GetAlignLength();
    if (sx != sy) {
        return (sx >= sy);
    }
    
    string x_id = NcbiEmptyString;
    string y_id = NcbiEmptyString;
    x->GetSeq_id(1).GetLabel(&x_id, CSeq_id::eContent);
    y->GetSeq_id(1).GetLabel(&y_id, CSeq_id::eContent);
    return (x_id < y_id);
    
};



void CIgBlast::x_SetupVSearch(CRef<IQueryFactory>       &qf,
                              CRef<CBlastOptionsHandle> &opts_hndl)
{
    CBlastOptions & opts = opts_hndl->SetOptions();
    if (m_IgOptions->m_IsProtein) {
        opts.SetCompositionBasedStats(eNoCompositionBasedStats);
    } else {
        int penalty = m_Options->GetOptions().GetMismatchPenalty();
        opts.SetMatchReward(1);
        opts.SetMismatchPenalty(penalty);
        opts.SetWordSize(m_Options->GetOptions().GetWordSize());
        if (penalty == -1) {
            opts.SetGapOpeningCost(4);
            opts.SetGapExtensionCost(1);
        }
    }
    opts_hndl->SetEvalueThreshold(m_Options->GetOptions().GetEvalueThreshold());
    opts_hndl->SetFilterString("F");
    opts_hndl->SetHitlistSize(15+ m_IgOptions->m_NumAlign[0]);
    qf.Reset(new CObjMgr_QueryFactory(*m_Query));

};

void CIgBlast::x_SetupDJSearch(const vector<CRef <CIgAnnotation> > &annots,
                               CRef<IQueryFactory>           &qf,
                               CRef<CBlastOptionsHandle>     &opts_hndl,
                               int db_type)
{
    // Only igblastn will search DJ
    CBlastOptions & opts = opts_hndl->SetOptions();
    opts.SetMatchReward(1);
    if (db_type == 2){ //J genes are longer so if can afford more reliable identification
        opts.SetWordSize(j_wordsize);
        opts.SetMismatchPenalty(m_IgOptions->m_J_penalty);
    } else {
        opts.SetWordSize(m_IgOptions->m_Min_D_match);
        opts.SetMismatchPenalty(m_IgOptions->m_D_penalty);
    }

    opts.SetGapOpeningCost(5);
    opts.SetGapExtensionCost(2);
    opts_hndl->SetEvalueThreshold((db_type == 2) ? 1000.0 : 100000.0);
    opts_hndl->SetFilterString("F");
    opts_hndl->SetHitlistSize(max(max(50, 
               m_IgOptions->m_NumAlign[1]), 
               m_IgOptions->m_NumAlign[2]));

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
            // Excluding the V gene except the last max_v_j_overlap bp for D and J gene search;
            // also limit the J match to max_allowed_V_end_to_J_end beyond V gene.
            int v_overlap;
            if (m_IgOptions->m_DetectOverlap && m_IgOptions->m_J_penalty == -3 && m_IgOptions->m_D_penalty == -4) {
                v_overlap = max_v_j_overlap;  
            } else {
                v_overlap = 0;
            }
            bool ms = (*annot)->m_MinusStrand;
            int begin = (ms)? 
              (*annot)->m_GeneInfo[0] - max_allowed_V_end_to_J_end: (*annot)->m_GeneInfo[1] - 1 - v_overlap;
            int end = (ms)? 
              (*annot)->m_GeneInfo[0] + v_overlap: (*annot)->m_GeneInfo[1] + max_allowed_V_end_to_J_end;
            if (begin > 0 && begin <= len-1) {
                CRef<CSeqLocInfo> mask(
                  new CSeqLocInfo(new CSeq_interval(*q_id, 0, begin), 0));
                m_Query->AddMask(iq, mask);
            }
            if (end < len -1 && end >= 0) {
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


void CIgBlast::x_SetupNoOverlapDSearch(const vector<CRef <CIgAnnotation> > &annots,
                                       CRef<CSearchResultSet>        &previous_d_results,
                                       CRef<IQueryFactory>           &qf,
                                       CRef<CBlastOptionsHandle>     &opts_hndl,
                                       int db_type)
{
    // Only igblastn will search DJ
    CBlastOptions & opts = opts_hndl->SetOptions();
    opts.SetMatchReward(1);
    opts.SetWordSize(m_IgOptions->m_Min_D_match);
    opts.SetMismatchPenalty(m_IgOptions->m_D_penalty);
    opts.SetGapOpeningCost(5);
    opts.SetGapExtensionCost(2);
    opts_hndl->SetEvalueThreshold(100000.0);
    opts_hndl->SetFilterString("F");
    opts_hndl->SetHitlistSize(max(max(50, 
                                      m_IgOptions->m_NumAlign[1]), 
                                  m_IgOptions->m_NumAlign[2]));
    
    // Mask query for D
    int iq = 0;
    ITERATE(vector<CRef <CIgAnnotation> >, annot, annots) {
        CRef<CBlastSearchQuery> query = m_Query->GetBlastSearchQuery(iq);
        CSeq_id *q_id = const_cast<CSeq_id *>(&*query->GetQueryId());
        int len = query->GetLength();
        CRef<CSeq_align_set> align_d(0);
        if ((*previous_d_results)[iq].HasAlignments()){
            align_d =  (*previous_d_results)[iq].SetSeqAlign();
        }
        
        if ((*annot)->m_GeneInfo[0] == -1 || !align_d || align_d.Empty() || align_d->IsEmpty()) {
            // This is not a ig sequence or there is no d gene per previous search.  Mask it out
            TMaskedQueryRegions mask_list;
            CRef<CSeqLocInfo> mask(
                  new CSeqLocInfo(new CSeq_interval(*q_id, 0, len-1), 0));
            mask_list.push_back(mask);
            m_Query->SetMaskedRegions(iq, mask_list);
        } else {
            // Excluding the V gene and J gene
            bool ms = (*annot)->m_MinusStrand;
            int v_end_or_j_begin = (ms)? 
                max((*annot)->m_GeneInfo[0] - max_allowed_V_end_to_J_end, (*annot)->m_GeneInfo[5] - 1): (*annot)->m_GeneInfo[1] -1;
            int j_begin_or_v_end = (ms)? 
                (*annot)->m_GeneInfo[0]: min((*annot)->m_GeneInfo[4], (*annot)->m_GeneInfo[1] + max_allowed_V_end_to_J_end);
            if (v_end_or_j_begin > 0) {
                CRef<CSeqLocInfo> mask(
                  new CSeqLocInfo(new CSeq_interval(*q_id, 0, v_end_or_j_begin), 0));
                m_Query->AddMask(iq, mask);
            }
            if (j_begin_or_v_end < len-1 && j_begin_or_v_end > 0) {
                CRef<CSeqLocInfo> mask(
                  new CSeqLocInfo(new CSeq_interval(*q_id, j_begin_or_v_end, len-1), 0));
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

// Compare the second seqalign to see if it is as good as the first one
static bool s_IsSeqAlignAsGood(const CRef<CSeq_align> &x, 
                               const CRef<CSeq_align> &y)
{
    double sx, sy;
    x->GetNamedScore(CSeq_align::eScore_EValue, sx);
    y->GetNamedScore(CSeq_align::eScore_EValue, sy);
    if (sx < 0.999999 * sy || sy < 0.999999 * sx) return false;
    int ix, iy;
    x->GetNamedScore(CSeq_align::eScore_Score, ix);
    y->GetNamedScore(CSeq_align::eScore_Score, iy);
    if (ix > iy) return false;
    int dx, dy;
    dx = x->GetAlignLength();
    dy = y->GetAlignLength();
    return (dx <= dy);
}

// Remove lcl| from seqid label
static string s_RemoveLocalPrefix(const string & sid) 
{
    if (sid.substr(0, 4) == "lcl|") return(sid.substr(4, sid.length()));
    return sid;
}

static string s_MakeTopHitsId(const CSeq_align_set::Tdata& align_list, int num_align) {
    string ids = NcbiEmptyString;
    CRef<CSeq_align> align = align_list.front();
    int count = 0;
    ITERATE(CSeq_align_set::Tdata, it, align_list) {
        
        if (count < num_align && s_IsSeqAlignAsGood(align, (*it))) {
            string this_id = s_RemoveLocalPrefix((*it)->GetSeq_id(1).AsFastaString());
            if (ids.find(this_id) == string::npos) {
                
                //no redundant id
                if (ids != NcbiEmptyString) {
                    ids += ",";
                }
                ids += this_id;
                count ++;
            }
        } else {
            break;
        }
    }
    return ids;
}

void CIgBlast::x_AnnotateV(CRef<CSearchResultSet>        &results, 
                           vector<CRef <CIgAnnotation> > &annots)
{
    ITERATE(CSearchResultSet, result, *results) {

        CIgAnnotation *annot = new CIgAnnotation();
        annots.push_back(CRef<CIgAnnotation>(annot));
 
        if ((*result)->HasAlignments()) {
            const CSeq_align_set::Tdata & align_list = (*result)->GetSeqAlign()->Get();
            CRef<CSeq_align> align = align_list.front();
            annot->m_GeneInfo[0] = align->GetSeqStart(0);
            annot->m_GeneInfo[1] = align->GetSeqStop(0)+1;
            annot->m_TopGeneIds[0] = s_MakeTopHitsId(align_list, m_IgOptions->m_NumAlign[0]);
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
static bool s_CompareSeqAlignByEvalue(const CRef<CSeq_align> &x, 
                                      const CRef<CSeq_align> &y)
{
    double sx, sy;
    x->GetNamedScore(CSeq_align::eScore_EValue, sx);
    y->GetNamedScore(CSeq_align::eScore_EValue, sy);
    if (sx < 0.999999 * sy) return true;
    if (sy < 0.999999 * sx) return false;
    int ix, iy;
    x->GetNamedScore(CSeq_align::eScore_Score, ix);
    y->GetNamedScore(CSeq_align::eScore_Score, iy);
    if (ix != iy) return (ix > iy);

    int dx, dy;
    dx = x->GetAlignLength();
    dy = y->GetAlignLength();
    if (dx != dy) {
        return (dx >= dy);
    }
    string x_id = NcbiEmptyString;
    string y_id = NcbiEmptyString;
    x->GetSeq_id(1).GetLabel(&x_id, CSeq_id::eContent);
    y->GetSeq_id(1).GetLabel(&y_id, CSeq_id::eContent);
    return (x_id < y_id);
};

// Compare two seqaligns according to their evalue and coverage
static bool s_CompareSeqAlignByScore(const CRef<CSeq_align> &x, const CRef<CSeq_align> &y)
{
    int sx, sy;
    x->GetNamedScore(CSeq_align::eScore_Score, sx);
    y->GetNamedScore(CSeq_align::eScore_Score, sy);
    if (sx != sy) return (sx > sy);
    sx = x->GetAlignLength();
    sy = y->GetAlignLength();
    return (sx >= sy);
    
};



// Test if D and J annotation not compatible
static bool s_DJNotCompatible(const CSeq_align &d, const CSeq_align &j, bool ms, int margin)
{
    int ds = d.GetSeqStart(0);
    int de = d.GetSeqStop(0);
    int js = j.GetSeqStart(0);
    int je = j.GetSeqStop(0);

    //D gene needs to have minimal match in addition to overlap with J gene
    //D gene needs to end before J gene ends
    if (ms) {
        if (ds < js || de < je + margin) return true;
    } else { 
        if (ds > js - margin || de > je) return true;
    }
    return false;
};

/*
static bool s_IsTopMatchJD(CSearchResults& res_J, CIgAnnotationInfo& annotation_info){
    bool result = true; //default
    CRef<CSeq_align_set> align_J;
    if (res_J.HasAlignments()) {
        align_J.Reset(const_cast<CSeq_align_set *>
                      (&*(res_J.GetSeqAlign())));
        CSeq_align_set::Tdata & align_list = align_J->Set();
        CSeq_align_set::Tdata::iterator it = align_list.begin();
        int prev_score = 0;
        result = false;
        while (it != align_list.end()) {
            int current_score;
            (*it)->GetNamedScore(CSeq_align::eScore_Score, current_score);
            if(current_score >= prev_score){
                string j_id;
                (*it)->GetSeq_id(1).GetLabel(&j_id, CSeq_id::eContent);
                string j_chain_type = annotation_info.GetDJChainType(j_id);
                if (j_chain_type == "N/A"){
                    //assume J gene id style 
                    
                    string sid = NStr::ToUpper(j_id);
                    if (sid.substr(0, 2) == "TR" && sid[3] == 'J') {
                        j_chain_type = "J" + sid.substr(2,1);
                    } else if (sid[0] == 'J') {
                        j_chain_type = sid.substr(0,2);
                    }
                }
                if (j_chain_type == "JD"){
                    result = true;
                    break;
                }
                
            } else {
                break;
            } 
            prev_score = current_score;
            ++it;
        }
           
    }
    return result;
};
*/
void CIgBlast::x_FindDJAln(CRef<CSeq_align_set>& align_D,
                           CRef<CSeq_align_set>& align_J,
                           string q_ct,
                           bool q_ms,
                           ENa_strand q_st,
                           int q_ve,
                           int iq,
                           bool va_or_vd_as_heavy_chain) {
    
    int allowed_VJ_distance = max_allowed_VJ_distance_with_D;
        /* preprocess D */
        if (align_D && !align_D->Get().empty()) {
            CSeq_align_set::Tdata & align_list = align_D->Set();
            CSeq_align_set::Tdata::iterator it = align_list.begin();
            /* chain type test */
            if (q_ct!="VH" && q_ct!="VD" && q_ct!="VA" && q_ct!="VB" && q_ct!="N/A") {
                while (it != align_list.end()) {
                    it = align_list.erase(it);
                }
                allowed_VJ_distance = max_allowed_VJ_distance_without_D;
            } else if (q_ct =="VA" || q_ct =="VD") {
                if (va_or_vd_as_heavy_chain) {
                    //VA could behave like VD and is allowed to rearrange to JA or DD/JD
                    q_ct = "VD";
                    //annot->m_ChainType[0] = "VD";
                } else {
                    q_ct = "VA";
                    while (it != align_list.end()) {
                        it = align_list.erase(it);
                    } 
                    allowed_VJ_distance = max_allowed_VJ_distance_without_D;
                }
            }
            //test compatability between V and D
            it = align_list.begin();
            while (it != align_list.end()) {
                bool keep = true;
                /* chain type test */
                if (q_ct!="N/A") {
                    char s_ct = q_ct[1];
                    string d_id;
                    (*it)->GetSeq_id(1).GetLabel(&d_id, CSeq_id::eContent);
                    string d_chain_type = m_AnnotationInfo.GetDJChainType(d_id);
                    if (d_chain_type != "N/A"){
                        if (d_chain_type[1] != q_ct[1]) keep = false;
                    } else { //assume D gene id style 
                        string sid = (*it)->GetSeq_id(1).AsFastaString();
                        sid = NStr::ToUpper(sid);
                        if (sid.substr(0, 4) == "LCL|") sid = sid.substr(4, sid.length());
                        if ((sid.substr(0, 2) == "IG" || sid.substr(0, 2) == "TR")
                            && sid[3] == 'D') {
                            s_ct = sid[2];
                        }
                        if (s_ct!='B' && s_ct!='D') s_ct = q_ct[1];
                        if (s_ct != q_ct[1]) keep = false;
                    }
                }
                
                /* remove failed seq_align */
                if (!keep) it = align_list.erase(it);
                else ++it;
            }


            /* strand test */
            bool strand_found = false;
            ITERATE(CSeq_align_set::Tdata, it, align_list) {
                if ((*it)->GetSeqStrand(0) == q_st) {
                    strand_found = true;
                    break;
                }
            }
            if (strand_found) {
                it = align_list.begin();
                while (it != align_list.end()) {
                    if ((*it)->GetSeqStrand(0) != q_st) {
                        it = align_list.erase(it);
                    } else ++it;
                }
            }
            /* v end test */
            it = align_list.begin();
            while (it != align_list.end()) {
                bool keep = false;
                int q_ds = (*it)->GetSeqStart(0);
                int q_de = (*it)->GetSeqStop(0);
                if (q_ms) keep = (q_de >= q_ve - max_allowed_VD_distance && q_ds <= q_ve - m_IgOptions->m_Min_D_match);
                else      keep = (q_ds <= q_ve + max_allowed_VD_distance && q_de >= q_ve + m_IgOptions->m_Min_D_match);
                if (!keep) it = align_list.erase(it);
                else ++it;
            }
            /* sort according to score */
            align_list.sort(s_CompareSeqAlignByScoreAndName);
        }

        /* preprocess J */
        if (align_J && !align_J->Get().empty()) {
            CSeq_align_set::Tdata & align_list = align_J->Set();
            CSeq_align_set::Tdata::iterator it = align_list.begin();
            while (it != align_list.end()) {
                bool keep = true;
                /* chain type test */
                if (q_ct!="N/A") {
                    char s_ct = q_ct[1];
                    string j_id;
                    (*it)->GetSeq_id(1).GetLabel(&j_id, CSeq_id::eContent);
                    string j_chain_type = m_AnnotationInfo.GetDJChainType(j_id);
                    if (j_chain_type != "N/A"){
                        if (j_chain_type[1] != q_ct[1]) keep = false;
                    } else { //assume J gene id style 
                        string sid = (*it)->GetSeq_id(1).AsFastaString();
                        sid = NStr::ToUpper(sid);
                        if (sid.substr(0, 4) == "LCL|") sid = sid.substr(4, sid.length());
                        if ((sid.substr(0, 2) == "IG" || sid.substr(0, 2) == "TR")
                            && sid[3] == 'J') {
                            s_ct = sid[2];
                        } else if (sid[0] == 'J') {
                            s_ct = sid[1];
                        }
                        if (s_ct!='H' && s_ct!='L' && s_ct!='K' &&
                            s_ct!='A' && s_ct!='B' && s_ct!='D' && s_ct!='G') s_ct = q_ct[1];
                        if (s_ct != q_ct[1]) keep = false;
                    }
                }
                /* strand test */
                if ((*it)->GetSeqStrand(0) != q_st) keep = false;
                /* subject start test */
                if ((*it)->GetSeqStart(1) > 32) keep = false;
                /* v end test */
                int q_js = (*it)->GetSeqStart(0);
                int q_je = (*it)->GetSeqStop(0);
                if (q_ms) { 
                    if (q_je < q_ve - allowed_VJ_distance  || q_js > q_ve - j_wordsize) keep = false;
                } else {
                    if (q_js > q_ve + allowed_VJ_distance || q_je < q_ve + j_wordsize) keep = false;
                }
                /* remove failed seq_align */
                if (!keep) it = align_list.erase(it);
                else ++it;
            }
            /* sort according to score */
            align_list.sort(s_CompareSeqAlignByScoreAndName);
        }

        /* which one to keep, D or J? */
        if (align_D.NotEmpty() && !align_D->IsEmpty() &&
            align_J.NotEmpty() && !align_J->IsEmpty()) {
            CSeq_align_set::Tdata & al_D = align_D->Set();
            CSeq_align_set::Tdata & al_J = align_J->Set();
            CSeq_align_set::Tdata::iterator it;
            bool keep_J = s_CompareSeqAlignByScore(*(al_J.begin()), *(al_D.begin()));
            if (keep_J) {
                it = al_D.begin();
                while (it != al_D.end()) {
                    if (s_DJNotCompatible(**it, **(al_J.begin()), q_ms, m_IgOptions->m_Min_D_match)) {
                        it = al_D.erase(it);
                    } else ++it;
                }

                if (m_IgOptions->m_DetectOverlap && m_IgOptions->m_J_penalty == -3 &&
                    m_IgOptions->m_D_penalty == -4) { 
                    //deleting j only for overlap case otherwise it's handeled later
                    if (align_D.NotEmpty() && !align_D->IsEmpty()){
                        it = al_J.begin();
                        while (it != al_J.end()) {
                            if (s_DJNotCompatible(**(al_D.begin()), **it, q_ms, m_IgOptions->m_Min_D_match)) {
                                it = al_J.erase(it);
                            } else ++it;
                        }
                    }
                }
            } else {
                it = al_J.begin();
                
                while (it != al_J.end()) {
                    if (s_DJNotCompatible(**(al_D.begin()), **it, q_ms, m_IgOptions->m_Min_D_match)) {
                        it = al_J.erase(it);
                    } else ++it;
                }
                if (align_J.NotEmpty() && !align_J->IsEmpty()) {
                    it = al_D.begin();
                    while (it != al_D.end()) {
                        if (s_DJNotCompatible(**it, **(al_J.begin()), q_ms, m_IgOptions->m_Min_D_match)) {
                            it = al_D.erase(it);
                        } else ++it;
                    }
 
                }
            }

        }
            
}


void CIgBlast::x_FindDJ(CRef<CSearchResultSet>& results_D,
                        CRef<CSearchResultSet>& results_J,
                        CRef <CIgAnnotation>& annot,
                        CRef<CSeq_align_set>& align_D,
                        CRef<CSeq_align_set>& align_J,
                        string q_ct,
                        bool q_ms,
                        ENa_strand q_st,
                        int q_ve,
                        int iq) {
    
    CRef<CSeq_align_set> original_align_D(new CSeq_align_set);
    CRef<CSeq_align_set> original_align_J(new CSeq_align_set);
    
        /* preprocess D */
        CSearchResults& res_D = (*results_D)[iq];
        if (res_D.HasAlignments()) {

            align_D.Reset(const_cast<CSeq_align_set *>
                                           (&*(res_D.GetSeqAlign())));
            original_align_D->Assign(*align_D);
           
        }

        /* preprocess J */
        CSearchResults& res_J = (*results_J)[iq];
        if (res_J.HasAlignments()) {
            align_J.Reset(const_cast<CSeq_align_set *>
                                           (&*(res_J.GetSeqAlign())));
            original_align_J->Assign(*align_J);
           
        } 
        //try as VA
        x_FindDJAln(align_D, align_J, q_ct, q_ms, q_st, q_ve, iq, false);
        if ((original_align_D.NotEmpty() && !original_align_D->Get().empty()) && (q_ct =="VA" || q_ct =="VD")) {
            
            annot->m_ChainType[0] = "VA";
            //try as VD
            x_FindDJAln(original_align_D, original_align_J, q_ct, q_ms, q_st, q_ve, iq, true);
            int as_heavy_chain_score = 0;
            int as_light_chain_score = 0;
            int d_score = 0;
            if(original_align_J.NotEmpty() && !original_align_J->Get().empty()){
                original_align_J->Get().front()->GetNamedScore(CSeq_align::eScore_Score, as_heavy_chain_score);
            }
           
            if(original_align_D.NotEmpty() && !original_align_D->Get().empty()){
                original_align_D->Get().front()->GetNamedScore(CSeq_align::eScore_Score, d_score);
            }
            if (align_J.NotEmpty() && !align_J->Get().empty()){
                align_J->Get().front()->GetNamedScore(CSeq_align::eScore_Score, as_light_chain_score);
            }

           
            if (as_heavy_chain_score + d_score> as_light_chain_score){
                if (align_D.NotEmpty() && original_align_D.NotEmpty()){
                    align_D->Assign(*original_align_D);
                }
                if (align_J.NotEmpty() && original_align_J.NotEmpty()){
                    align_J->Assign(*original_align_J);
                }
              
                annot->m_ChainType[0] = "VD";
            }
            
        }
        
}

void CIgBlast::x_FillJDomain(CRef<CSeq_align> & align, CRef <CIgAnnotation> & annot){
    string sid = s_RemoveLocalPrefix(align->GetSeq_id(1).AsFastaString());
    int j_cdr3end = m_AnnotationInfo.GetJDomain(sid);
    int subject_start = align->GetSeqStart(1);
    //don't try if j starts after cdr3 ends as we don't know for sure where the boundry is
    if (j_cdr3end > 0 && subject_start - j_cdr3end <= 1) {
        int subject_end = align->GetSeqStop(1);
        CAlnMap j_map(align->GetSegs().GetDenseg());
        
        //+1 actaully is in fwr4 already...need to do this so that a insertion right in front
        // of fwr4 can be handled.
        annot->m_JDomain[1] = j_map.GetSeqPosFromSeqPos(0, 1, 
                                                        max(subject_start, min(j_cdr3end + 1, 
                                                                               subject_end)), 
                                                        IAlnExplorer::eRight);
        
        if (align->GetSeqStrand(0) == eNa_strand_minus) {
            annot->m_JDomain[1] = m_Scope->GetBioseqHandle(align->GetSeq_id(0)).GetBioseqLength() - annot->m_JDomain[1] - 1;
        } 
           
        //deduct one back to in CDR3
        if (subject_end > j_cdr3end) {
            annot->m_JDomain[1] --;
        }
    }
}

void CIgBlast::x_ProcessDGeneResult(CRef<CSearchResultSet>& results_V, 
                                    CRef<CSearchResultSet>& results_D,
                                    CRef<CSearchResultSet>& results_J,
                                    vector<CRef <CIgAnnotation> > &annots) {
    
    int iq = 0;
    NON_CONST_ITERATE(vector<CRef <CIgAnnotation> >, annot, annots) { 
        string q_ct = (*annot)->m_ChainType[0];
        bool q_ms = (*annot)->m_MinusStrand;
        ENa_strand q_st = (q_ms) ? eNa_strand_minus : eNa_strand_plus;
        int q_ve = (q_ms) ? (*annot)->m_GeneInfo[0] : (*annot)->m_GeneInfo[1] - 1;

        CRef<CSeq_align_set> align_D (0);
        CSearchResults& res_D = (*results_D)[iq];
        if (res_D.HasAlignments()) {
            align_D = res_D.SetSeqAlign();
        }

        // preprocess D 
        if (align_D && !align_D.Empty() && !align_D->IsEmpty()) {
            CSeq_align_set::Tdata & align_list = align_D->Set();
            CSeq_align_set::Tdata::iterator it = align_list.begin();
            
            //test compatability between V and D
            it = align_list.begin();
            while (it != align_list.end()) {
                bool keep = true;
                // chain type test 
                if (q_ct!="N/A") {
                    char s_ct = q_ct[1];
                    string d_id;
                    (*it)->GetSeq_id(1).GetLabel(&d_id, CSeq_id::eContent);
                    string d_chain_type = m_AnnotationInfo.GetDJChainType(d_id);
                    if (d_chain_type != "N/A"){
                        if (d_chain_type[1] != q_ct[1]) keep = false;
                    } else { //assume D gene id style 
                        string sid = (*it)->GetSeq_id(1).AsFastaString();
                        sid = NStr::ToUpper(sid);
                        if (sid.substr(0, 4) == "LCL|") sid = sid.substr(4, sid.length());
                        if ((sid.substr(0, 2) == "IG" || sid.substr(0, 2) == "TR")
                            && sid[3] == 'D') {
                            s_ct = sid[2];
                        }
                        if (s_ct!='B' && s_ct!='D') s_ct = q_ct[1];
                        if (s_ct != q_ct[1]) keep = false;
                    }
                }
                
                //remove failed seq_align 
                if (!keep) it = align_list.erase(it);
                else ++it;
            }

            
            //strand test 
            bool strand_found = false;
            ITERATE(CSeq_align_set::Tdata, it, align_list) {
                if ((*it)->GetSeqStrand(0) == q_st) {
                    strand_found = true;
                    break;
                }
            }
            if (strand_found) {
                it = align_list.begin();
                while (it != align_list.end()) {
                    if ((*it)->GetSeqStrand(0) != q_st) {
                        it = align_list.erase(it);
                    } else ++it;
                }
            }
            //v end test 
            it = align_list.begin();
            while (it != align_list.end()) {
                bool keep = false;
                int q_ds = (*it)->GetSeqStart(0);
                int q_de = (*it)->GetSeqStop(0);
                if (q_ms) keep = (q_de >= q_ve - max_allowed_VD_distance && q_ds <= q_ve - m_IgOptions->m_Min_D_match);
                else      keep = (q_ds <= q_ve + max_allowed_VD_distance && q_de >= q_ve + m_IgOptions->m_Min_D_match);
                if (!keep) it = align_list.erase(it);
                else ++it;
            }
            // sort according to score 
            align_list.sort(s_CompareSeqAlignByScoreAndName);

            /* process J */
            CRef<CSeq_align_set> align_J (0);
            CSearchResults& res_J = (*results_J)[iq];
            if (res_J.HasAlignments()) {
                align_J = res_J.SetSeqAlign();
            }
            if (align_J && align_J.NotEmpty() && !align_J->IsEmpty() && !align_list.empty()) {
                
                CSeq_align_set::Tdata & al_J = align_J->Set();
                CSeq_align_set::Tdata::iterator it = al_J.begin();
                while (it != al_J.end()) {
                    if (s_DJNotCompatible(*(align_list.front()), **it, q_ms, m_IgOptions->m_Min_D_match)) {
                        it = al_J.erase(it);
                    } else ++it;
                }                
            }
        }
                
        iq ++;
    }
}

void CIgBlast::x_ProcessDJResult(CRef<CSearchResultSet>& results_V, 
                                 CRef<CSearchResultSet>& results_D,
                                 CRef<CSearchResultSet>& results_J,
                                 vector<CRef <CIgAnnotation> > &annots) {
 
    int iq = 0;
    NON_CONST_ITERATE(vector<CRef <CIgAnnotation> >, annot, annots) { 
        string q_ct = (*annot)->m_ChainType[0];
        bool q_ms = (*annot)->m_MinusStrand;
        ENa_strand q_st = (q_ms) ? eNa_strand_minus : eNa_strand_plus;
        int q_ve = (q_ms) ? (*annot)->m_GeneInfo[0] : (*annot)->m_GeneInfo[1] - 1;
        
        CRef<CSeq_align_set> align_D, align_J;

        x_FindDJ( results_D, results_J, *annot, align_D, align_J, q_ct, q_ms, q_st, q_ve, iq); 
        iq ++;
    }
}

void CIgBlast::x_AnnotateD(CRef<CSearchResultSet>        &results_D,
                           vector<CRef <CIgAnnotation> > &annots)
{
 
    int iq = 0;
    NON_CONST_ITERATE(vector<CRef <CIgAnnotation> >, annot, annots) {
      
        string q_ct = (*annot)->m_ChainType[0];
        CSearchResults& res_d = (*results_D)[iq];
        CConstRef<CSeq_align_set> align_D = res_d.GetSeqAlign();
        if (align_D && !align_D.Empty() && !align_D->IsEmpty()) {
            const CSeq_align_set::Tdata& align_list = align_D->Get();
            CRef<CSeq_align> align = align_list.front();
            (*annot)->m_GeneInfo[2] = align->GetSeqStart(0);
            (*annot)->m_GeneInfo[3] = align->GetSeqStop(0)+1;
            (*annot)->m_TopGeneIds[1] = s_MakeTopHitsId(align_list, m_IgOptions->m_NumAlign[1]);
        }

     
        /* next set of results */
        ++iq;
    }
};

void CIgBlast::x_AnnotateJ(CRef<CSearchResultSet>        &results_J,
                           vector<CRef <CIgAnnotation> > &annots)
{
    int iq = 0;
    NON_CONST_ITERATE(vector<CRef <CIgAnnotation> >, annot, annots) {
      
        string q_ct = (*annot)->m_ChainType[0];
        bool q_ms = (*annot)->m_MinusStrand;

        const CSearchResults& res_j = (*results_J)[iq];
        CConstRef<CSeq_align_set> align_J = res_j.GetSeqAlign();
       
        /* annotate J */    
        if (align_J.NotEmpty() && !align_J->IsEmpty()) {
            const CSeq_align_set::Tdata & align_list = align_J->Get();
            CRef<CSeq_align> align = align_list.front();
            x_FillJDomain(align, *annot);
            (*annot)->m_GeneInfo[4] = align->GetSeqStart(0);
            (*annot)->m_GeneInfo[5] = align->GetSeqStop(0)+1;
            string sid = s_RemoveLocalPrefix(align->GetSeq_id(1).AsFastaString());
            int frame_offset = m_AnnotationInfo.GetFrameOffset(sid);
            if (frame_offset >= 0) {
                int frame_adj = (align->GetSeqStart(1) + 3 - frame_offset) % 3;
                (*annot)->m_FrameInfo[2] = (q_ms) ?
                                           align->GetSeqStop(0)  + frame_adj 
                                         : align->GetSeqStart(0) - frame_adj;
            } 
            (*annot)->m_TopGeneIds[2] = s_MakeTopHitsId(align_list, m_IgOptions->m_NumAlign[2]);
        }
     
        /* next set of results */
        ++iq;
    }
};

void CIgBlast::x_AnnotateDJ(CRef<CSearchResultSet>        &results_D,
                            CRef<CSearchResultSet>        &results_J,
                            vector<CRef <CIgAnnotation> > &annots)
{
    int iq = 0;
    NON_CONST_ITERATE(vector<CRef <CIgAnnotation> >, annot, annots) {
      
        string q_ct = (*annot)->m_ChainType[0];
        bool q_ms = (*annot)->m_MinusStrand;

        const CSearchResults& res_j = (*results_J)[iq];
        const CSearchResults& res_d = (*results_D)[iq];
        CConstRef<CSeq_align_set> align_D = res_d.GetSeqAlign();
        CConstRef<CSeq_align_set> align_J = res_j.GetSeqAlign();
       
        /* annotate D */    
        if (align_D.NotEmpty() && !align_D->IsEmpty()) {
            const CSeq_align_set::Tdata & align_list = align_D->Get();
            CRef<CSeq_align> align = align_list.front();
            (*annot)->m_GeneInfo[2] = align->GetSeqStart(0);
            (*annot)->m_GeneInfo[3] = align->GetSeqStop(0)+1;
            (*annot)->m_TopGeneIds[1] = s_MakeTopHitsId(align_list, m_IgOptions->m_NumAlign[1]);

        }
            
        /* annotate J */    
        if (align_J.NotEmpty() && !align_J->IsEmpty()) {
            const CSeq_align_set::Tdata & align_list = align_J->Get();
            CRef<CSeq_align> align = align_list.front();
            x_FillJDomain(align, *annot);
            (*annot)->m_GeneInfo[4] = align->GetSeqStart(0);
            (*annot)->m_GeneInfo[5] = align->GetSeqStop(0)+1;
            string sid = s_RemoveLocalPrefix(align->GetSeq_id(1).AsFastaString());
            int frame_offset = m_AnnotationInfo.GetFrameOffset(sid);
            if (frame_offset >= 0) {
                int frame_adj = (align->GetSeqStart(1) + 3 - frame_offset) % 3;
                (*annot)->m_FrameInfo[2] = (q_ms) ?
                                           align->GetSeqStop(0)  + frame_adj 
                                         : align->GetSeqStart(0) - frame_adj;
            } 
            (*annot)->m_TopGeneIds[2] = s_MakeTopHitsId(align_list, m_IgOptions->m_NumAlign[2]);
        }
     
        /* next set of results */
        ++iq;
    }
};

// query chain type and domain is annotated by germline alignment
void CIgBlast::x_AnnotateDomain(CRef<CSearchResultSet>        &gl_results,
                                CRef<CSearchResultSet>        &dm_results, 
                                vector<CRef <CIgAnnotation> > &annots)
{
    CRef<CObjectManager> mgr = CObjectManager::GetInstance();
    CScope scope_q(*mgr), scope_s(*mgr);
    CRef<CSeqDB> db_V, db_domain;
    bool annotate_subject = false;
    if (m_IgOptions->m_Db[0]->IsBlastDb()) {
        string db_name_V = m_IgOptions->m_Db[0]->GetDatabaseName(); 
        string db_name_domain = m_IgOptions->m_Db[3]->GetDatabaseName(); 
        CSeqDB::ESeqType db_type = (m_IgOptions->m_IsProtein)? 
                                   CSeqDB::eProtein : CSeqDB::eNucleotide;
        db_V.Reset(new CSeqDB(db_name_V, db_type));
        if (db_name_V == db_name_domain) {
            db_domain.Reset(&(*db_V));
        } else {
            db_domain.Reset(new CSeqDB(db_name_domain, db_type));
        }
        annotate_subject = true;
    }

    int iq = 0;
    ITERATE(CSearchResultSet, result, *dm_results) {

        CIgAnnotation *annot = &*(annots[iq]);
        annot->m_ChainType.push_back("NON");  // Assuming non-ig sequence first
        annot->m_ChainTypeToShow = "NON";
        if ((*result)->HasAlignments() && (*gl_results)[iq].HasAlignments()) {


            CConstRef<CSeq_align> master_align = 
                            (*gl_results)[iq].GetSeqAlign()->Get().front();
            CAlnMap q_map(master_align->GetSegs().GetDenseg());

            if (master_align->GetSeqStrand(0) == eNa_strand_minus) {
                annot->m_MinusStrand = true;
            }

            int q_ends[2], q_dir;

            if (annot->m_MinusStrand) {
                q_ends[1] = master_align->GetSeqStart(0);
                q_ends[0] = master_align->GetSeqStop(0);
                q_dir = -1;

            } else {
                q_ends[0] = master_align->GetSeqStart(0);
                q_ends[1] = master_align->GetSeqStop(0);
                q_dir = 1;
            }

            const CSeq_align_set::Tdata & align_list = (*result)->GetSeqAlign()->Get();

            ITERATE(CSeq_align_set::Tdata, it, align_list) {

                string sid = s_RemoveLocalPrefix((*it)->GetSeq_id(1).AsFastaString());
                annot->m_ChainType[0] = m_AnnotationInfo.GetDomainChainType(sid);
                annot->m_ChainTypeToShow = annot->m_ChainType[0];
                int domain_info[10];

                if (m_AnnotationInfo.GetDomainInfo(sid, domain_info)) {


                    CAlnMap s_map((*it)->GetSegs().GetDenseg());
                    int s_start = (*it)->GetSeqStart(1);
                    int s_stop = (*it)->GetSeqStop(1);

                    CRef<CAlnMap> d_map;
                    int d_start = -1;
                    int d_stop = -1;

                    int start, stop;

                    if (annotate_subject) {
                        CRef<CBioseq> seq_q = db_domain->SeqidToBioseq((*it)->GetSeq_id(1));
                        CBioseq_Handle hdl_q = scope_q.AddBioseq(*seq_q);
                        CRef<CBioseq> seq_s = db_V->SeqidToBioseq(master_align->GetSeq_id(1));
                        CBioseq_Handle hdl_s = scope_s.AddBioseq(*seq_s);
                        CSeq_loc query, subject;
                        query.SetWhole();
                        query.SetId((*it)->GetSeq_id(1));
                        subject.SetWhole();
                        subject.SetId(master_align->GetSeq_id(1));
                        SSeqLoc q_loc(&query, &scope_q);
                        SSeqLoc s_loc(&subject, &scope_s);
                        CBl2Seq bl2seq(q_loc, s_loc, (m_IgOptions->m_IsProtein)? eBlastp: eBlastn);
                        const CSearchResults& result = (*(bl2seq.RunEx()))[0];
                        if (result.HasAlignments()) {
                            CConstRef<CSeq_align> subject_align = result.GetSeqAlign()->Get().front();
                            d_map.Reset(new CAlnMap(subject_align->GetSegs().GetDenseg()));
                            d_start = subject_align->GetSeqStart(0);
                            d_stop = subject_align->GetSeqStop(0);
                        }
                        scope_q.RemoveBioseq(hdl_q);
                        scope_s.RemoveBioseq(hdl_s);
                    }

                    for (int i =0; i<10; i+=2) {

                        start = domain_info[i] - 1;
                        stop = domain_info[i+1] - 1;

                        if (start <= d_stop && stop >= d_start) {
                            int start_copy = start;
                            int stop_copy = stop;
                            if (start_copy < d_start) start_copy = d_start;
                            if (stop_copy > d_stop) stop_copy = d_stop;
                            if (start_copy <= stop_copy) {
                                if (i>0 && annot->m_DomainInfo_S[i-1]>=0) {
                                    annot->m_DomainInfo_S[i] = annot->m_DomainInfo_S[i-1] + 1;
                                } else {
                                    annot->m_DomainInfo_S[i] = 
                                       d_map->GetSeqPosFromSeqPos(1, 0, start_copy, IAlnExplorer::eForward);
                                }
                                annot->m_DomainInfo_S[i+1] = 
                                   d_map->GetSeqPosFromSeqPos(1, 0, stop_copy, IAlnExplorer::eBackwards);
                            }
                        }
                    
                        if (start > s_stop || stop < s_start) continue;

                        if (start < s_start) start = s_start;

                        if (stop > s_stop) stop = s_stop;

                        if (start > stop) continue;

                        start = s_map.GetSeqPosFromSeqPos(0, 1, start, IAlnExplorer::eForward);
                        stop = s_map.GetSeqPosFromSeqPos(0, 1, stop, IAlnExplorer::eBackwards);

                        if ((start - q_ends[1])*q_dir > 0 || (stop - q_ends[0])*q_dir < 0) continue;

                        if ((start - q_ends[0])*q_dir < 0) start = q_ends[0];

                        if ((stop - q_ends[1])*q_dir > 0) stop = q_ends[1];

                        if ((start - stop)*q_dir > 0) continue;

                        start = q_map.GetSeqPosFromSeqPos(1, 0, start, IAlnExplorer::eForward);
                        stop = q_map.GetSeqPosFromSeqPos(1, 0, stop, IAlnExplorer::eBackwards);

                        start = q_map.GetSeqPosFromSeqPos(0, 1, start);
                        stop = q_map.GetSeqPosFromSeqPos(0, 1, stop);
 
                        if ((start - stop)*q_dir > 0) continue;

                        annot->m_DomainInfo[i] = start;
                        annot->m_DomainInfo[i+1] = stop;
                    }

                    // any extra alignments after FWR3 are attributed to CDR3
                    start = annot->m_DomainInfo[9];

                    if (start >= 0 && (start - q_ends[1])*q_dir < 0) {
                        start = q_map.GetSeqPosFromSeqPos(1, 0, start+q_dir, IAlnExplorer::eForward);
                        start = q_map.GetSeqPosFromSeqPos(0, 1, start);
 
                        if ((start - q_ends[1])*q_dir <= 0) {
                            annot->m_DomainInfo[10] = start;
                            annot->m_DomainInfo[11] = q_ends[1];
                        }
                    }

                    // extension of the first and last annotated domain (if any)
                    int i = 0;
                    while (i<10 && annot->m_DomainInfo[i] < 0) i+=2;
                    annot->m_DomainInfo[i] += (domain_info[i] - 1 -
                                       s_map.GetSeqPosFromSeqPos(1, 0, annot->m_DomainInfo[i],
                                                                 IAlnExplorer::eBackwards))*q_dir;
                    if (annot->m_DomainInfo[i] < 0) annot->m_DomainInfo[i] = 0;
                    i+=2;
                    while (i<10 && annot->m_DomainInfo[i] >=0) {
                        annot->m_DomainInfo[i] = annot->m_DomainInfo[i-1] + q_dir;
                        i+=2;
                    }
                    i = 9;
                    while (i>0 && annot->m_DomainInfo[i] < 0) i-=2;
                    if (i >= 0) {
                        annot->m_DomainInfo[i] += (domain_info[i] - 1 -
                                                   s_map.GetSeqPosFromSeqPos(1, 0, annot->m_DomainInfo[i],
                                                                             IAlnExplorer::eForward))*q_dir;
                        if (annot->m_DomainInfo[i] < 0) annot->m_DomainInfo[i] = 0;
                    }

                 
                    // annotate the query frame offset
                    int frame_offset = m_AnnotationInfo.GetFrameOffset(sid);
                    
                    if (frame_offset >= 0) {
                        int q_start = (*it)->GetSeqStart(0); 
                        int q_stop = (*it)->GetSeqStop(0); 
                        int q_mid = q_start + q_stop;
                        int q_dif = q_stop - q_start;
                        int frame_adj = (3 - ((*it)->GetSeqStart(1) + 3 - frame_offset) % 3) %3;
                        annot->m_FrameInfo[0] = (q_mid - q_dir *q_dif)/2 + q_dir * frame_adj;
                        
                        //counting frame from fwr3 end, not the V end since we need to ignore a few bases 
                        //in the CDR3 to allow any insertion or deletion at V gene end
                        if (annot->m_DomainInfo[9] > 0) {
                            int fwr3_stop = annot->m_DomainInfo[9]; 
                           
                            if (annot->m_MinusStrand) {
                                
                                q_start = max(q_start, fwr3_stop);
                                q_mid = q_start + q_stop;
                                q_dif = q_stop - q_start;
                                frame_adj = (s_map.GetSeqPosFromSeqPos(1, 0, q_start, IAlnExplorer::eBackwards) + 3 - frame_offset) % 3;
                            } else {
                                q_stop = min(q_stop, fwr3_stop);
                                q_mid = q_start + q_stop;
                                q_dif = q_stop - q_start;
                                frame_adj = (s_map.GetSeqPosFromSeqPos(1, 0, q_stop, IAlnExplorer::eBackwards) + 3 - frame_offset) % 3;
                            }
                        } else {
                            frame_adj = ((*it)->GetSeqStop(1) + 3 - frame_offset) % 3;
                        }
                       
                        annot->m_FrameInfo[1] = (q_mid + q_dir *q_dif)/2 - q_dir * frame_adj;
                    }
                    break;

                }
            }
        }
        ++iq;
    }
};

void CIgBlast::x_SetChainType(CRef<CSearchResultSet> &results,
                              vector<CRef <CIgAnnotation> > &annots)
{
    int iq = 0;
    ITERATE(CSearchResultSet, result, *results) {

        CIgAnnotation *annot = &*(annots[iq++]);

        if ((*result)->HasAlignments()) {
            int num_aligns = (*result)->GetSeqAlign()->Size();
            CIgBlastResults *ig_result = dynamic_cast<CIgBlastResults *>
                                     (const_cast<CSearchResults *>(&**result));
            for (int i=0; i<ig_result->m_NumActualV; ++i, --num_aligns) {
                 annot->m_ChainType.push_back("V");
            }
            for (int i=0; i<ig_result->m_NumActualD; ++i, --num_aligns) {
                 annot->m_ChainType.push_back("D");
            }
            for (int i=0; i<ig_result->m_NumActualJ; ++i, --num_aligns) {
                 annot->m_ChainType.push_back("J");
            }
            for (int i=0; i<num_aligns; ++i) {
                 annot->m_ChainType.push_back("N/A");
            }
        }
    }
};

void CIgBlast::s_SortResultsByEvalue(CRef<CSearchResultSet>& results)
{
    ITERATE(CSearchResultSet, result, *results) {
        if ((*result)->HasAlignments()) {
            CRef<CSeq_align_set> align(const_cast<CSeq_align_set *>
                                   (&*((*result)->GetSeqAlign())));
            CSeq_align_set::Tdata & align_list = align->Set();
            align_list.sort(s_CompareSeqAlignByEvalue);
        }
    }
};

// convert sequencecomparison to database mode
void CIgBlast::x_ConvertResultType(CRef<CSearchResultSet> &result) 
{
    if (result->GetResultType() != eSequenceComparison) {
        return;
    }

    int num_queries = m_Query->Size();
    int num_results = result->GetNumResults();
    int ir = 0;
    CSearchResultSet *retv = new CSearchResultSet();

    for (int iq = 0; iq< num_queries && ir< num_results; ++iq) {

        CSearchResults &res = (*result)[ir++];
        CRef<CBlastAncillaryData> ancillary = res.GetAncillaryData();
        TQueryMessages errmsg = res.GetErrors();
        CConstRef<CSeq_id> rid = res.GetSeqId();
        CRef<CSeq_align_set> align(const_cast<CSeq_align_set *>
                          (&*(res.GetSeqAlign())));
        CSeq_align_set::Tdata & align_list = align->Set();

        CConstRef<CSeq_id> qid = m_Query->GetBlastSearchQuery(iq)->GetQueryId();
        while(!qid->Match(*rid)) {
            CRef<CSeq_align_set> empty;
            CRef<CSearchResults> r(new CSearchResults(qid, empty, errmsg, ancillary));
            retv->push_back(r);
            qid = m_Query->GetBlastSearchQuery(++iq)->GetQueryId();
        }

        while(ir < num_results && (*result)[ir].GetSeqId()->Match(*qid)) {
            CSearchResults &add_res = (*result)[ir++];
            CRef<CSeq_align_set> add;
            add.Reset(const_cast<CSeq_align_set *>
                          (&*(add_res.GetSeqAlign())));
            CSeq_align_set::Tdata & add_list = add->Set();
            align_list.insert(align_list.end(), add_list.begin(), add_list.end());
        }
        CRef<CSearchResults> r(new CSearchResults(qid, align, errmsg, ancillary));
        retv->push_back(r);
    }
    
    result.Reset(retv);
};

void CIgBlast::s_AppendResults(CRef<CSearchResultSet> &results,
                               int                     num_aligns,
                               int                     gene,
                               CRef<CSearchResultSet> &final_results)
{
    bool  new_result = (final_results.Empty());
    if (new_result) {
        final_results.Reset(new CSearchResultSet());
    }

    int iq = 0;
    ITERATE(CSearchResultSet, result, *results) {

        CRef<CSeq_align_set> align;
        int actual_align = 0;

        if ((*result)->HasAlignments()) {
            align.Reset(const_cast<CSeq_align_set *>
                                   (&*((*result)->GetSeqAlign())));

            // keep only the first num_alignments
            if (num_aligns >= 0) {
                CSeq_align_set::Tdata & align_list = align->Set();
                if (align_list.size() > (CSeq_align_set::Tdata::size_type)num_aligns) {
                    CSeq_align_set::Tdata::iterator it = align_list.begin();
                    for (int i=0; i<num_aligns; ++i) ++it;
                    align_list.erase(it, align_list.end());
                    actual_align = num_aligns;
                } else {
                    actual_align = align_list.size();
                }
            }
        }

        TQueryMessages errmsg = (*result)->GetErrors();
        CConstRef<CSeq_id> query = (*result)->GetSeqId();

        CIgBlastResults *ig_result;
        if (new_result) {
            // TODO maybe we need the db ancillary instead?
            CRef<CBlastAncillaryData> ancillary = (*result)->GetAncillaryData();
            ig_result = new CIgBlastResults(query, align, errmsg, ancillary);
            CRef<CSearchResults> r(ig_result);
            final_results->push_back(r);
        } else {
            while( !(*final_results)[iq].GetSeqId()->Match(*query)) ++iq;
            ig_result = dynamic_cast<CIgBlastResults *> (&(*final_results)[iq]);
            if (!align.Empty()) {
                CSeq_align_set::Tdata & ig_list = ig_result->SetSeqAlign()->Set();
                CSeq_align_set::Tdata & align_list = align->Set();

                if (gene < 0) {
                    // Remove duplicate seq_aligns
                    CSeq_align_set::Tdata::iterator it = align_list.begin();
                    while (it != align_list.end()) {
                        if (s_SeqAlignInSet(ig_list, *it)) it = align_list.erase(it);
                        else ++it;
                    }
                }

                if (!align_list.empty()) {
                    ig_list.insert(ig_list.end(), align_list.begin(), align_list.end());
                    ig_result->GetErrors().Combine(errmsg);
                }
            }
        }

        switch(gene) {
        case 0: ig_result->m_NumActualV = actual_align; break;
        case 1: ig_result->m_NumActualD = actual_align; break;
        case 2: ig_result->m_NumActualJ = actual_align; break;
        default: break;
        }
    }
};

void CIgBlast::x_SetAnnotation(vector<CRef <CIgAnnotation> > &annots,
                               CRef<CSearchResultSet> &final_results)
{
    int iq = 0;
    NON_CONST_ITERATE(CSearchResultSet, result, *final_results) {
        CIgBlastResults *ig_result = dynamic_cast<CIgBlastResults *>
            (const_cast<CSearchResults *>(&**result));
        CIgAnnotation *annot = &*(annots[iq++]);
        ig_result->SetIgAnnotation().Reset(annot);
        if (annot->m_GeneInfo[4] < 0 && m_IgOptions->m_MinJLength > 0) { //no J
            if ((*result)->HasAlignments()){
                (*result)->SetSeqAlign()->Set().clear();
            }
        } 
    }
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
