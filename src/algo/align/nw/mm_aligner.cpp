/* $Id$
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
 * Authors:  Yuri Kapustin
 *
 * File Description:  CMMAligner implementation
 *                   
 * ===========================================================================
 *
 */

#include <ncbi_pch.hpp>
#include "mm_aligner_threads.hpp"

#include <corelib/ncbimtx.hpp>
#include <algo/align/nw/align_exception.hpp>

BEGIN_NCBI_SCOPE

CMMAligner::CMMAligner()
{
}


CMMAligner::CMMAligner( const char* seq1, size_t len1,
                        const char* seq2, size_t len2,
                        const SNCBIPackedScoreMatrix* scoremat )
  : CNWAligner(seq1, len1, seq2, len2, scoremat)
{
}


CMMAligner::CMMAligner(const string& seq1,
                       const string& seq2,
                       const SNCBIPackedScoreMatrix* scoremat )
  : CNWAligner(seq1, seq2, scoremat)
{
}


CNWAligner::TScore CMMAligner::x_Run()
{
    m_terminate = false;
    if(m_prg_callback) {
        m_prg_info.m_iter_total = 2*m_SeqLen1*m_SeqLen2;
        m_prg_info.m_iter_done = 0;
        m_terminate = m_prg_callback(&m_prg_info);
    }

    if(m_terminate) {
        return m_score = 0;
    }
    
    m_score = kMin_Int;
    m_TransList.clear();
    m_TransList.push_back(eTS_None);

    SCoordRect m (0, 0, m_SeqLen1 - 1, m_SeqLen2 - 1);
    x_DoSubmatrix(m, m_TransList.end(), false, false); // top-level call

    if(m_terminate) {
        return m_score = 0;
    }

    // reverse_copy not supported by some compilers
    list<ETranscriptSymbol>::const_iterator ii = m_TransList.begin();
    size_t nsize = m_TransList.size() - 1;
    m_Transcript.clear();
    m_Transcript.resize(nsize);
    for(size_t k = 1; k <= nsize; ++k)
        m_Transcript[nsize - k] = *++ii;

    return m_score;
}


// trace bit coding (four bits per value): D E Ec Fc
// D:  1 if diagonal; 0 - otherwise
// E:  1 if space in 1st sequence; 0 if space in 2nd sequence
// Ec: 1 if gap in 1st sequence was extended; 0 if it is was opened
// Fc: 1 if gap in 2nd sequence was extended; 0 if it is was opened
//

const unsigned char kMaskFc  = 0x0001;
const unsigned char kMaskEc  = 0x0002;
const unsigned char kMaskE   = 0x0004;
const unsigned char kMaskD   = 0x0008;

DEFINE_STATIC_FAST_MUTEX (masterlist_mutex);

void CMMAligner::x_DoSubmatrix( const SCoordRect& submatr,
    list<ETranscriptSymbol>::iterator translist_pos,
    bool left_top, bool right_bottom )
{

    if( m_terminate ) {
        return;
    }

    const int dimI = submatr.i2 - submatr.i1 + 1;
    const int dimJ = submatr.j2 - submatr.j1 + 1;
    if(dimI < 1 || dimJ < 1) return;

    bool top_level = submatr.i1 == 0 && submatr.j1 == 0 &&
        submatr.i2 == m_SeqLen1-1 && submatr.j2 == m_SeqLen2-1;


    if(dimI < 3 || dimJ < 3) { // terminal case
        CFastMutexGuard guard (masterlist_mutex);
        list<ETranscriptSymbol> lts;
        TScore score = x_RunTerm(submatr, left_top, right_bottom, lts);
        if(top_level) m_score = score;
        m_TransList.splice(translist_pos, lts);
        return;
    }

    const size_t I = submatr.i1 + dimI / 2;
    const size_t dim = dimJ + 1;

    // run Needleman-Wunsch without storing full traceback matrix
    SCoordRect rtop (submatr.i1, submatr.j1, I, submatr.j2);
    vector<TScore> vEtop (dim), vFtop (dim), vGtop (dim);
    vector<unsigned char> trace_top (dim);

    SCoordRect rbtm (I + 1, submatr.j1, submatr.i2 , submatr.j2);
    vector<TScore> vEbtm (dim), vFbtm (dim), vGbtm (dim);
    vector<unsigned char> trace_btm (dim);
    
    if( m_mt && m_maxthreads > 1 && MM_RequestNewThread(m_maxthreads) ) {
        CThreadRunOnTop* thr2 = new CThreadRunOnTop ( this, &rtop,
                                            &vEtop, &vFtop, &vGtop,
                                            &trace_top, left_top );
        thr2->Run();
        x_RunBtm(rbtm, vEbtm, vFbtm, vGbtm, trace_btm, right_bottom);
        thr2->Join(0);
    }
    else {
        x_RunTop(rtop, vEtop, vFtop, vGtop, trace_top, left_top);
        x_RunBtm(rbtm, vEbtm, vFbtm, vGbtm, trace_btm, right_bottom);
    }

    if( m_terminate ) {
        return;
    }

    // locate the transition point
    size_t trans_pos = 0;
    ETransitionType trans_type = eGG;
    TScore score1 = x_FindBestJ ( vEtop, vFtop, vGtop, vEbtm, vFbtm, vGbtm,
                                  trans_pos, trans_type );
    if(top_level)
        m_score = score1;

    // modify traces at the transition point if applicable
    if(trans_type == eII) {
        unsigned char mask_top = kMaskE;
        if(trace_top[trans_pos] & kMaskEc)
            mask_top |= kMaskEc;
        trace_top[trans_pos] = mask_top;
        trace_btm[trans_pos] = kMaskE | kMaskEc;
    }
    if(trans_type == eDD) {
        trace_top[trans_pos] = (trace_top[trans_pos] & kMaskFc)? kMaskFc: 0;
        trace_btm[trans_pos] = kMaskFc;
    }

    // extend a subpath to both directions
    vector<unsigned char>::const_iterator trace_it_top = 
        trace_top.begin() + trans_pos;
    list<ETranscriptSymbol> subpath_left;
    size_t steps_left = x_ExtendSubpath(trace_it_top, false, subpath_left);

    vector<unsigned char>::const_iterator trace_it_btm = 
        trace_btm.begin() + trans_pos;
    list<ETranscriptSymbol> subpath_right;
    size_t steps_right = x_ExtendSubpath(trace_it_btm, true, subpath_right);

    // check if we reached left or top line
    bool bNoLT = false;
    int nc0 = trans_pos - steps_left;
    if(nc0 < 0) {
        NCBI_THROW(CAlgoAlignException, eInternal,
                   "Assertion: LT underflow");
    }
    bool bLeft = nc0 == 0;
    bool bTop  = I == submatr.i1;
    if(bLeft && !bTop) {
        int jump = I - submatr.i1;
        subpath_left.insert(subpath_left.begin(), jump, eTS_Delete);
    }
    if(!bLeft && bTop) {
        subpath_left.insert(subpath_left.begin(), nc0, eTS_Insert);
    }
    if(bLeft || bTop) {
        bNoLT = true;
    }

    // check if we reached right or bottom line
    bool bNoRB = false;
    int nc1 = trans_pos + steps_right;
    if(nc1 > dimJ) {
        NCBI_THROW(CAlgoAlignException, eInternal,
                   "Assertion: RB overflow");
    }
    bool bRight = nc1 == dimJ;
    bool bBottom  = I == submatr.i2 - 1;
    if(bRight && !bBottom) {
        int jump = submatr.i2 - I - 1;
        subpath_right.insert(subpath_right.end(), jump, eTS_Delete);
    }
    if(!bRight && bBottom) {
        int jump = dimJ - nc1;
        subpath_right.insert(subpath_right.end(), jump, eTS_Insert);
    }
    if(bRight || bBottom) {
        bNoRB = true;
    }

    // find out if we have special left-top and/or right-bottom
    // on the new sub-matrices
    bool rb = subpath_left.front() == eTS_Delete;
    bool lt = subpath_right.back() == eTS_Delete;

    // coalesce the pieces together and insert into the master list
    subpath_left.splice( subpath_left.end(), subpath_right );

    list<ETranscriptSymbol>::iterator ti0, ti1;
    {{
        CFastMutexGuard  guard (masterlist_mutex);
        ti0 = translist_pos;
        --ti0;
        m_TransList.splice( translist_pos, subpath_left );
        ++ti0;
        ti1 = translist_pos;
    }}

    // Recurse submatrices:
    SCoordRect rlt;
    if(!bNoLT) {
        // left top
        size_t left_bnd = submatr.j1 + trans_pos - steps_left - 1;
        if(left_bnd >= submatr.j1) {
            rlt.i1 = submatr.i1;
            rlt.j1 = submatr.j1;
            rlt.i2 = I - 1;
            rlt.j2 = left_bnd;
        }
        else {
            NCBI_THROW(CAlgoAlignException, eInternal,
                       "Assertion: Left boundary out of range");
        }
    }

    SCoordRect rrb;
    if(!bNoRB) {
        // right bottom
        size_t right_bnd = submatr.j1 + trans_pos + steps_right;
        if(right_bnd <= submatr.j2) {
            rrb.i1 = I + 2;
            rrb.j1 = right_bnd;
            rrb.i2 = submatr.i2;
            rrb.j2 = submatr.j2;
        }
        else {
            NCBI_THROW(CAlgoAlignException, eInternal,
                       "Assertion: Right boundary out of range");
        }
    }

    if(!bNoLT && !bNoRB) {
        if( m_mt && m_maxthreads > 1 && MM_RequestNewThread(m_maxthreads) ) {
            // find out which rect is larger
            if(rlt.GetArea() > rrb.GetArea()) {
                // do rb in a second thread
                CThread* thr2 = new CThreadDoSM( this, &rrb, ti1,
                                                 lt, right_bottom);
                thr2->Run();
                // do lt in this thread
                x_DoSubmatrix(rlt, ti0, left_top, rb);
                thr2->Join(0);
            }
            else {
                // do lt in a second thread
                CThread* thr2 = new CThreadDoSM( this, &rlt, ti0,
                                                 left_top, rb);
                thr2->Run();
                // do rb in this thread
                x_DoSubmatrix(rrb, ti1, lt, right_bottom);
                thr2->Join(0);
            }
        }
        else {  // just do both
            x_DoSubmatrix(rlt, ti0, left_top, rb);
            x_DoSubmatrix(rrb, ti1, lt, right_bottom);
        }
    }
    else if(!bNoLT) {
        x_DoSubmatrix(rlt, ti0, left_top, rb);
    }
    else if(!bNoRB) {
        x_DoSubmatrix(rrb, ti1, lt, right_bottom);
    }
}


CNWAligner::TScore CMMAligner::x_FindBestJ (
    const vector<TScore>& vEtop, const vector<TScore>& vFtop,
    const vector<TScore>& vGtop, const vector<TScore>& vEbtm,
    const vector<TScore>& vFbtm, const vector<TScore>& vGbtm,
    size_t& pos,
    ETransitionType& trans_type ) const
{
    const size_t dim = vEtop.size();
    TScore score = kMin_Int;
    TScore trans_alts [9];

    bool bFreeGapLeft2  = m_esf_L2 && dim == m_SeqLen2 + 1;
    bool bFreeGapRight2 = m_esf_R2 && dim == m_SeqLen2 + 1;

    for(size_t i = 0; i < dim ; ++i) {
        trans_alts [0] = vEtop[i] + vEbtm[i] - m_Wg;   // II
        trans_alts [1] = vFtop[i] + vEbtm[i];          // DI
        trans_alts [2] = vGtop[i] + vEbtm[i];          // GI
        trans_alts [3] = vEtop[i] + vFbtm[i];          // ID
        TScore wg = (bFreeGapLeft2 && i == 0 || ( bFreeGapRight2 && i == dim -1) )?
                    0: m_Wg;
        trans_alts [4] = vFtop[i] + vFbtm[i] - wg;     // DD
        trans_alts [5] = vGtop[i] + vFbtm[i];          // GD
        trans_alts [6] = vEtop[i] + vGbtm[i];          // IG
        trans_alts [7] = vFtop[i] + vGbtm[i];          // DG
        trans_alts [8] = vGtop[i] + vGbtm[i];          // GG

        for(size_t k = 0; k < 9; ++k) {
            if(trans_alts[k] > score) {
                score = trans_alts[k];
                pos = i;
                trans_type = (ETransitionType)k;
            }
        }
    }

    return score;
}


size_t CMMAligner::x_ExtendSubpath (
    vector<unsigned char>::const_iterator trace_it,
    bool direction,
    list<ETranscriptSymbol>& subpath ) const
{
    subpath.clear();
    size_t step_counter = 0;
    if(direction) {
        while(true) {
            unsigned char Key = *trace_it;
            if (Key & kMaskD) {
                subpath.push_back(eTS_Match);
                ++step_counter;
                break;
            }
            else if (Key & kMaskE) {
                subpath.push_back(eTS_Insert);
                ++step_counter;
                ++trace_it;
                while(Key & kMaskEc) {
                    Key = *trace_it++;
                    subpath.push_back(eTS_Insert);
                    ++step_counter;
                }
            }
            else if ((Key & kMaskE) == 0) {
                subpath.push_back(eTS_Delete);
                break;
            }
            else {
                NCBI_THROW(CAlgoAlignException, eInternal,
                           "Assertion: incorrect backtrace symbol "
                           "(right expansion)");
            }
        }
    }
    else {
        while(true) {
            unsigned char Key = *trace_it;
            if (Key & kMaskD) {
                subpath.push_front(eTS_Match);
                ++step_counter;
                break;
            }
            else if (Key & kMaskE) {
                subpath.push_front(eTS_Insert);
                ++step_counter;
                --trace_it;
                while(Key & kMaskEc) {
                    Key = *trace_it--;
                    subpath.push_front(eTS_Insert);
                    ++step_counter;
                }
            }
            else if ((Key & kMaskE) == 0) {
                subpath.push_front(eTS_Delete);
                break;
            }
            else {
                NCBI_THROW(CAlgoAlignException, eInternal,
                           "Assertion: incorrect backtrace symbol "
                           "(left expansion)");
            }
        }
    }
    return step_counter;
}


#ifdef NCBI_THREADS
DEFINE_STATIC_FAST_MUTEX (progress_mutex);
#endif


void CMMAligner::x_RunTop ( const SCoordRect& rect,
             vector<TScore>& vE, vector<TScore>& vF, vector<TScore>& vG,
             vector<unsigned char>& trace, bool lt ) const
{
    if( m_terminate ) {
        return;
    }

    const size_t dim1 = rect.i2 - rect.i1 + 1;
    const size_t dim2 = rect.j2 - rect.j1 + 1;
    const size_t N1   = dim1 + 1;
    const size_t N2   = dim2 + 1;

    vector<TScore> stl_rowV (N2), stl_rowF (N2);
    TScore* rowV    = &stl_rowV [0];
    TScore* rowF    = &stl_rowF [0];

    TScore* pV = rowV - 1;

    const char* seq1 = m_Seq1 - 1 + rect.i1;
    const char* seq2 = m_Seq2 - 1 + rect.j1;

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    bool bFreeGapLeft1  = m_esf_L1 && rect.i1 == 0;
    bool bFreeGapLeft2  = m_esf_L2 && rect.j1 == 0;
    bool bFreeGapRight2  = m_esf_R2 && rect.j2 == m_SeqLen2 - 1;

    // progress reporting
    const size_t prg_rep_rate = 100;
    const size_t prg_rep_increment = prg_rep_rate*N2;

    // first row

    TScore wg = bFreeGapLeft1? 0: m_Wg;
    TScore ws = bFreeGapLeft1? 0: m_Ws;

    rowV[0] = wg;
    size_t i, j;
    for (j = 1; j < N2; ++j) {
        rowV[j] = pV[j] + ws;
        rowF[j] = kInfMinus;        
    }
    rowV[0] = 0;

    // recurrences

    wg = bFreeGapLeft2? 0: m_Wg;
    ws = bFreeGapLeft2? 0: m_Ws;

    TScore V  = 0;
    TScore V0 = lt? 0: wg;
    TScore E, G, n0;

    for(i = 1;  i < N1 - 1;  ++i) {
        
        V = V0 += ws;
        E = kInfMinus;
        unsigned char ci = seq1[i];

        TScore wg2 = m_Wg, ws2 = m_Ws;

        for (j = 1; j < N2; ++j) {

            G = pV[j] + sm[ci][(unsigned char)seq2[j]];
            pV[j] = V;

            n0 = V + m_Wg;
            if(E >= n0)
                E += m_Ws;      // continue the gap
            else
                E = n0 + m_Ws;  // open a new gap

            if(j == N2 - 1 && bFreeGapRight2)
                wg2 = ws2 = 0;

            n0 = rowV[j] + wg2;
            if (rowF[j] > n0)
                rowF[j] += ws2;
            else
                rowF[j] = n0 + ws2;

            V = (E >= rowF[j])? (E >= G? E: G): (rowF[j] >= G? rowF[j]: G);
        }
        pV[j] = V;

        if( m_prg_callback && i % prg_rep_rate == 0 ) {
#ifdef NCBI_THREADS
            CFastMutexGuard guard (progress_mutex);
#endif
            m_prg_info.m_iter_done += prg_rep_increment;
            if( (m_terminate = m_prg_callback(&m_prg_info)) ) {
                break;
            }
        }
    }

    // the last row (i == N1 - 1)
    if(!m_terminate) {

        vG[0] = vE[0] = E = kInfMinus;
        vF[0] = V = V0 += ws;
        trace[0] = kMaskFc;
        unsigned char ci = seq1[i];

        TScore wg2 = m_Wg, ws2 = m_Ws;

        unsigned char tracer;
        for (j = 1; j < N2; ++j) {

            vG[j] = G = pV[j] + sm[ci][(unsigned char)seq2[j]];
            pV[j] = V;

            n0 = V + m_Wg;
            if(E >= n0) {
                E += m_Ws;      // continue the gap
                tracer = kMaskEc;
            }
            else {
                E = n0 + m_Ws;  // open a new gap
                tracer = 0;
            }
            vE[j] = E;

            if(j == N2 - 1 && bFreeGapRight2) {
                wg2 = ws2 = 0;
            }

            n0 = rowV[j] + wg2;
            if(rowF[j] >= n0) {
                rowF[j] += ws2;
                tracer |= kMaskFc;
            }
            else {
                rowF[j] = n0 + ws2;
            }
            vF[j] = rowF[j];

            if (E >= rowF[j]) {
                if(E >= G) {
                    V = E;
                    tracer |= kMaskE;
                }
                else {
                    V = G;
                    tracer |= kMaskD;
                }
            } else {
                if(rowF[j] >= G) {
                    V = rowF[j];
                }
                else {
                    V = G;
                    tracer |= kMaskD;
                }
            }
            trace[j] = tracer;
        }
    }

    if( m_prg_callback ) {
#ifdef NCBI_THREADS
        CFastMutexGuard guard (progress_mutex);
#endif
        m_prg_info.m_iter_done += (i % prg_rep_rate)*N2;
        m_terminate = m_prg_callback(&m_prg_info);
    }
}

 
void CMMAligner::x_RunBtm(const SCoordRect& rect,
             vector<TScore>& vE, vector<TScore>& vF, vector<TScore>& vG,
             vector<unsigned char>& trace, bool rb) const
{
    if( m_terminate ) {
        return;
    }

    const size_t dim1 = rect.i2 - rect.i1 + 1;
    const size_t dim2 = rect.j2 - rect.j1 + 1;
    const size_t N1   = dim1 + 1;
    const size_t N2   = dim2 + 1;

    vector<TScore> stl_rowV (N2), stl_rowF (N2);
    TScore* rowV    = &stl_rowV [0];
    TScore* rowF    = &stl_rowF [0];

    TScore* pV = rowV + 1;

    const char* seq1 = m_Seq1 + rect.i1;
    const char* seq2 = m_Seq2 + rect.j1;

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    bool bFreeGapRight1  = m_esf_R1 && rect.i2 == m_SeqLen1 - 1;
    bool bFreeGapRight2  = m_esf_R2 && rect.j2 == m_SeqLen2 - 1;
    bool bFreeGapLeft2  =  m_esf_L2 && rect.j1 == 0;

    // progress reporting

    const size_t prg_rep_rate = 100;
    const size_t prg_rep_increment = prg_rep_rate*N2;

    // bottom row

    TScore wg = bFreeGapRight1? 0: m_Wg;
    TScore ws = bFreeGapRight1? 0: m_Ws;

    rowV[N2 - 1] = wg;
    int i, j;
    for (j = N2 - 2; j >= 0; --j) {
        rowV[j] = pV[j] + ws;
        rowF[j] = kInfMinus;
    }
    rowV[N2 - 1] = 0;

    // recurrences

    wg = bFreeGapRight2? 0: m_Wg;
    ws = bFreeGapRight2? 0: m_Ws;

    TScore V  = 0;
    TScore V0 = rb? 0: wg;
    TScore E, G, n0;

    for(i = N1 - 2;  i > 0;  --i) {
        
        V = V0 += ws;
        E = kInfMinus;
        unsigned char ci = seq1[i];

        TScore wg2 = m_Wg, ws2 = m_Ws;

        for (j = N2 - 2; j >= 0; --j) {

            G = pV[j] + sm[ci][(unsigned char)seq2[j]];
            pV[j] = V;

            n0 = V + m_Wg;
            if(E >= n0)
                E += m_Ws;      // continue the gap
            else
                E = n0 + m_Ws;  // open a new gap

            if(j == 0 && bFreeGapLeft2) {
                wg2 = ws2 = 0;
            }

            n0 = rowV[j] + wg2;
            if (rowF[j] > n0)
                rowF[j] += ws2;
            else
                rowF[j] = n0 + ws2;

            V = (E >= rowF[j])? (E >= G? E: G): (rowF[j] >= G? rowF[j]: G);
        }
        pV[j] = V;

        if( m_prg_callback && (N1 - i) % prg_rep_rate == 0 ) {
#ifdef NCBI_THREADS
            CFastMutexGuard guard (progress_mutex);
#endif
            m_prg_info.m_iter_done += prg_rep_increment;
            if( (m_terminate = m_prg_callback(&m_prg_info)) ) {
                break;
            }
        }
    }

    // the top row (i == 0)
    if(!m_terminate) {

        vF[N2-1] = V = V0 += ws;
        vG[N2-1] = vE[N2-1] = E = kInfMinus;
        trace[N2-1] = kMaskFc;
        unsigned char ci = seq1[i];

        TScore wg2 = m_Wg, ws2 = m_Ws;

        unsigned char tracer;
        for (j = N2 - 2; j >= 0; --j) {

            vG[j] = G = pV[j] + sm[ci][(unsigned char)seq2[j]];
            pV[j] = V;

            n0 = V + m_Wg;
            if(E >= n0) {
                E += m_Ws;      // continue the gap
                tracer = kMaskEc;
            }
            else {
                E = n0 + m_Ws;  // open a new gap
                tracer = 0;
            }
            vE[j] = E;

            if(j == 0 && bFreeGapLeft2) {
                wg2 = ws2 = 0;
            }

            n0 = rowV[j] + wg2;
            if(rowF[j] >= n0) {
                rowF[j] += ws2;
                tracer |= kMaskFc;
            }
            else {
                rowF[j] = n0 + ws2;
            }
            vF[j] = rowF[j];

            if (E >= rowF[j]) {
                if(E >= G) {
                    V = E;
                    tracer |= kMaskE;
                }
                else {
                    V = G;
                    tracer |= kMaskD;
                }
            } else {
                if(rowF[j] >= G) {
                    V = rowF[j];
                }
                else {
                    V = G;
                    tracer |= kMaskD;
                }
            }
            trace[j] = tracer;
        }
    }

    if( m_prg_callback ) {
#ifdef NCBI_THREADS
        CFastMutexGuard guard (progress_mutex);
#endif
        m_prg_info.m_iter_done += (N1 - i) % prg_rep_rate;
        m_terminate = m_prg_callback(&m_prg_info);
    }
}


CNWAligner::TScore CMMAligner::x_RunTerm(const SCoordRect& rect,
                                         bool left_top, bool right_bottom,
                                         list<ETranscriptSymbol>& subpath)
{
    if( m_terminate ) {
        return 0;
    }

    const size_t N1 = rect.i2 - rect.i1 + 2;
    const size_t N2 = rect.j2 - rect.j1 + 2;

    vector<TScore> stl_rowV (N2), stl_rowF (N2);
    TScore* rowV    = &stl_rowV [0];
    TScore* rowF    = &stl_rowF [0];

    // index calculation: [i,j] = i*n2 + j
    vector<unsigned char> stl_bm (N1*N2);
    unsigned char* backtrace = &stl_bm[0];

    TScore* pV = rowV - 1;

    const char* seq1 = m_Seq1 + rect.i1 - 1;
    const char* seq2 = m_Seq2 + rect.j1 - 1;

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    bool bFreeGapLeft1  = m_esf_L1 && rect.i1 == 0;
    bool bFreeGapRight1 = m_esf_R1 && rect.i2 == m_SeqLen1 - 1;
    bool bFreeGapLeft2  = m_esf_L2 && rect.j1 == 0;
    bool bFreeGapRight2 = m_esf_R2 && rect.j2 == m_SeqLen2 - 1;

    TScore wgleft1   = bFreeGapLeft1? 0: m_Wg;
    TScore wsleft1   = bFreeGapLeft1? 0: m_Ws;
    TScore wg1 = m_Wg, ws1 = m_Ws;

    // first row
    size_t k;
    {
        rowV[0] = wgleft1;
        for (k = 1; k < N2; k++) {
            rowV[k] = pV[k] + wsleft1;
            rowF[k] = kInfMinus;
            backtrace[k] = kMaskE | kMaskEc;
        }
        rowV[0] = 0;
    }

    // recurrences
    TScore wgleft2   = bFreeGapLeft2? 0: m_Wg;
    TScore wsleft2   = bFreeGapLeft2? 0: m_Ws;
    TScore V  = 0;
    TScore V0 = left_top? 0: wgleft2;
    TScore E, G, n0;
    unsigned char tracer;

    size_t i, j;
    for(i = 1;  i < N1;  ++i) {
        
        V = V0 += wsleft2;
        E = kInfMinus;
        backtrace[k++] = kMaskFc;
        unsigned char ci = seq1[i];

        if(i == N1 - 1 && bFreeGapRight1) {
                wg1 = ws1 = 0;
        }

        TScore wg2 = m_Wg, ws2 = m_Ws;

        for (j = 1; j < N2; ++j, ++k) {

            G = pV[j] + sm[ci][(unsigned char)seq2[j]];
            pV[j] = V;

            n0 = V + wg1;
            if(E >= n0) {
                E += ws1;      // continue the gap
                tracer = kMaskEc;
            }
            else {
                E = n0 + ws1;  // open a new gap
                tracer = 0;
            }

            if(j == N2 - 1 && bFreeGapRight2) {
                wg2 = ws2 = 0;
            }
            n0 = rowV[j] + ((right_bottom && j == N2 - 1)? 0: wg2);
            if(rowF[j] >= n0) {
                rowF[j] += ws2;
                tracer |= kMaskFc;
            }
            else {
                rowF[j] = n0 + ws2;
            }

            if (E >= rowF[j]) {
                if(E >= G) {
                    V = E;
                    tracer |= kMaskE;
                }
                else {
                    V = G;
                    tracer |= kMaskD;
                }
            } else {
                if(rowF[j] >= G) {
                    V = rowF[j];
                }
                else {
                    V = G;
                    tracer |= kMaskD;
                }
            }
            backtrace[k] = tracer;
        }

        pV[j] = V;
    }

    // fill the subpath
    subpath.clear();
    
    // run backtrace
    k = N1*N2 - 1;
    while (k != 0) {
        unsigned char Key = backtrace[k];
        if (Key & kMaskD) {
            subpath.push_front(eTS_Match);
            k -= N2 + 1;
        }
        else if (Key & kMaskE) {
            subpath.push_front(eTS_Insert); --k;
            while(k > 0 && (Key & kMaskEc)) {
                subpath.push_front(eTS_Insert);
                Key = backtrace[k--];
            }
        }
        else {
            subpath.push_front(eTS_Delete);
            k -= N2;
            while(k > 0 && (Key & kMaskFc)) {
                subpath.push_front(eTS_Delete);
                Key = backtrace[k];
                k -= N2;
            }
        }
    }

    return V;
}


bool CMMAligner::x_CheckMemoryLimit()
{
    return true;
}



END_NCBI_SCOPE
