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

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbi_system.hpp>

#include "mm_aligner_threads.hpp"

BEGIN_NCBI_SCOPE



CMMAligner::CMMAligner( const char* seq1, size_t len1,
                        const char* seq2, size_t len2,
                        EScoringMatrixType matrix_type )
    throw(CNWAlignerException):
    CNWAligner(seq1, len1, seq2, len2, matrix_type),
    m_mt(false), m_maxthreads(1)
{
}


void CMMAligner::EnableMultipleThreads(bool allow)
{
    m_maxthreads = (m_mt = allow)? GetCpuCount(): 1;
}


CNWAligner::TScore CMMAligner::Run()
{
    m_score = kMin_Int;
    m_TransList.clear();
    m_TransList.push_back(eNone);

    x_LoadScoringMatrix();
    SCoordRect m (0, 0, m_SeqLen1 - 1, m_SeqLen2 - 1);
    x_DoSubmatrix(m, m_TransList.end(), false, false); // top-level call

    // reverse_copy not supported by some compilers
    list<ETranscriptSymbol>::const_iterator ib = m_TransList.begin(),
        ie = m_TransList.end(), ii = ib;
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

void CMMAligner::x_DoSubmatrix( const SCoordRect& submatr,
    list<ETranscriptSymbol>::iterator translist_pos,
    bool left_top, bool right_bottom )
{
    const int dimI = submatr.i2 - submatr.i1 + 1;
    const int dimJ = submatr.j2 - submatr.j1 + 1;
    if(dimI < 1 || dimJ < 1) return;

    bool top_level = submatr.i1 == 0 && submatr.j1 == 0 &&
        submatr.i2 == m_SeqLen1-1 && submatr.j2 == m_SeqLen2-1;

    DEFINE_STATIC_FAST_MUTEX (masterlist_mutex);

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

    // locate the transition point
    size_t trans_pos;
    ETransitionType trans_type;
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
        NCBI_THROW(
                   CNWAlignerException,
                   eInternal,
                   "Assertion: LT underflow");
    }
    bool bLeft = nc0 == 0;
    bool bTop  = I == submatr.i1;
    if(bLeft && !bTop) {
        int jump = I - submatr.i1;
        subpath_left.insert(subpath_left.begin(), jump, eDelete);
    }
    if(!bLeft && bTop) {
        subpath_left.insert(subpath_left.begin(), nc0, eInsert);
    }
    if(bLeft || bTop) {
        bNoLT = true;
    }

    // check if we reached right or bottom line
    bool bNoRB = false;
    int nc1 = trans_pos + steps_right;
    if(nc1 > dimJ) {
        NCBI_THROW(
                   CNWAlignerException,
                   eInternal,
                   "Assertion: RB overflow");
    }
    bool bRight = nc1 == dimJ;
    bool bBottom  = I == submatr.i2 - 1;
    if(bRight && !bBottom) {
        int jump = submatr.i2 - I - 1;
        subpath_right.insert(subpath_right.end(), jump, eDelete);
    }
    if(!bRight && bBottom) {
        int jump = dimJ - nc1;
        subpath_right.insert(subpath_right.end(), jump, eInsert);
    }
    if(bRight || bBottom) {
        bNoRB = true;
    }

    // find out if we have special left-top and/or right-bottom
    // on the new sub-matrices
    bool rb = subpath_left.front() == eDelete;
    bool lt = subpath_right.back() == eDelete;

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
        int left_bnd = submatr.j1 + trans_pos - steps_left - 1;
        if(left_bnd >= submatr.j1) {
            rlt.i1 = submatr.i1;
            rlt.j1 = submatr.j1;
            rlt.i2 = I - 1;
            rlt.j2 = left_bnd;
        }
        else {
            NCBI_THROW(
                       CNWAlignerException,
                       eInternal,
                       "Assertion: Left boundary out of range");
        }
    }

    SCoordRect rrb;
    if(!bNoRB) {
        // right bottom
        int right_bnd = submatr.j1 + trans_pos + steps_right;
        if(right_bnd <= submatr.j2) {
            rrb.i1 = I + 2;
            rrb.j1 = right_bnd;
            rrb.i2 = submatr.i2;
            rrb.j2 = submatr.j2;
        }
        else {
            NCBI_THROW(
                       CNWAlignerException,
                       eInternal,
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

    for(size_t i = 0; i < dim ; ++i) {
        trans_alts [0] = vEtop[i] + vEbtm[i] - m_Wg;   // II
        trans_alts [1] = vFtop[i] + vEbtm[i];          // DI
        trans_alts [2] = vGtop[i] + vEbtm[i];          // GI
        trans_alts [3] = vEtop[i] + vFbtm[i];          // ID
        trans_alts [4] = vFtop[i] + vFbtm[i] - m_Wg;   // DD
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
    vector<unsigned char>::const_iterator ti0 = trace_it;

    subpath.clear();
    size_t step_counter = 0;
    if(direction) {
        while(true) {
            unsigned char Key = *trace_it;
            if (Key & kMaskD) {
                subpath.push_back(eMatch);
                ++step_counter;
                break;
            }
            else if (Key & kMaskE) {
                subpath.push_back(eInsert);
                ++step_counter;
                ++trace_it;
                while(Key & kMaskEc) {
                    Key = *trace_it++;
                    subpath.push_back(eInsert);
                    ++step_counter;
                }
            }
            else if ((Key & kMaskE) == 0) {
                subpath.push_back(eDelete);
                break;
            }
            else {
                NCBI_THROW(
                           CNWAlignerException,
                           eInternal,
                           "Assertion: incorrect backtrace symbol "
                           "(right expansion)");
            }
        }
    }
    else {
        while(true) {
            unsigned char Key = *trace_it;
            if (Key & kMaskD) {
                subpath.push_front(eMatch);
                ++step_counter;
                break;
            }
            else if (Key & kMaskE) {
                subpath.push_front(eInsert);
                ++step_counter;
                --trace_it;
                while(Key & kMaskEc) {
                    Key = *trace_it--;
                    subpath.push_front(eInsert);
                    ++step_counter;
                }
            }
            else if ((Key & kMaskE) == 0) {
                subpath.push_front(eDelete);
                break;
            }
            else {
                NCBI_THROW(
                           CNWAlignerException,
                           eInternal,
                           "Assertion: incorrect backtrace symbol "
                           "(left expansion)");
            }
        }
    }
    return step_counter;
}

void CMMAligner::x_RunTop ( const SCoordRect& rect,
             vector<TScore>& vE, vector<TScore>& vF, vector<TScore>& vG,
             vector<unsigned char>& trace, bool lt ) const
{
    const size_t dim1 = rect.i2 - rect.i1 + 1;
    const size_t dim2 = rect.j2 - rect.j1 + 1;
    const size_t N1   = dim1 + 1;
    const size_t N2   = dim2 + 1;

    TScore* rowV    = new TScore [N2];
    TScore* rowF    = new TScore [N2];

    TScore* pV = rowV - 1;

    const char* seq1 = m_Seq1 - 1 + rect.i1;
    const char* seq2 = m_Seq2 - 1 + rect.j1;

    // first row
    rowV[0] = m_Wg;
    size_t i, j;
    for (j = 1; j < N2; ++j) {
        rowV[j] = pV[j] + m_Ws;
        rowF[j] = kInfMinus;        
    }
    rowV[0] = 0;

    // recurrences
    TScore V  = 0;
    TScore V0 = lt? 0: m_Wg;
    TScore E, G, n0;

    for(i = 1;  i < N1 - 1;  ++i) {
        
        V = V0 += m_Ws;
        E = kInfMinus;
        char ci = seq1[i];

        for (j = 1; j < N2; ++j) {

            G = pV[j] + m_Matrix[ci][seq2[j]];
            pV[j] = V;

            n0 = V + m_Wg;
            if(E > n0)
                E += m_Ws;      // continue the gap
            else
                E = n0 + m_Ws;  // open a new gap

            n0 = rowV[j] + m_Wg;
            if (rowF[j] > n0)
                rowF[j] += m_Ws;
            else
                rowF[j] = n0 + m_Ws;

            V = E >= rowF[j]? (E >= G? E: G): (rowF[j] >= G? rowF[j]: G);
        }
        pV[j] = V;
    }

    // the last row (i == N1 - 1)
    {
        vG[0] = vE[0] = E = kInfMinus;
        vF[0] = V = V0 += m_Ws;
        trace[0] = kMaskFc;
        char ci = seq1[i];
        
        unsigned char tracer;
        for (j = 1; j < N2; ++j) {

            vG[j] = G = pV[j] + m_Matrix[ci][seq2[j]];
            pV[j] = V;

            n0 = V + m_Wg;
            if(E > n0) {
                E += m_Ws;      // continue the gap
                tracer = kMaskEc;
            }
            else {
                E = n0 + m_Ws;  // open a new gap
                tracer = 0;
            }
            vE[j] = E;

            n0 = rowV[j] + m_Wg;
            if(rowF[j] > n0) {
                rowF[j] += m_Ws;
                tracer |= kMaskFc;
            }
            else {
                rowF[j] = n0 + m_Ws;
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
    
    delete[] rowV;
    delete[] rowF;
}

 
void CMMAligner::x_RunBtm(const SCoordRect& rect,
             vector<TScore>& vE, vector<TScore>& vF, vector<TScore>& vG,
             vector<unsigned char>& trace, bool rb) const
{
    const size_t dim1 = rect.i2 - rect.i1 + 1;
    const size_t dim2 = rect.j2 - rect.j1 + 1;
    const size_t N1   = dim1 + 1;
    const size_t N2   = dim2 + 1;

    TScore* rowV    = new TScore [N2];
    TScore* rowF    = new TScore [N2];

    TScore* pV = rowV + 1;

    const char* seq1 = m_Seq1 + rect.i1;
    const char* seq2 = m_Seq2 + rect.j1;

    // bottom row
    rowV[N2 - 1] = m_Wg;
    int i, j;
    for (j = N2 - 2; j >= 0; --j) {
        rowV[j] = pV[j] + m_Ws;
        rowF[j] = kInfMinus;
    }
    rowV[N2 - 1] = 0;

    // recurrences
    TScore V  = 0;
    TScore V0 = rb? 0: m_Wg;
    TScore E, G, n0;

    for(i = N1 - 2;  i > 0;  --i) {
        
        V = V0 += m_Ws;
        E = kInfMinus;
        char ci = seq1[i];

        for (j = N2 - 2; j >= 0; --j) {

            G = pV[j] + m_Matrix[ci][seq2[j]];
            pV[j] = V;

            n0 = V + m_Wg;
            if(E > n0)
                E += m_Ws;      // continue the gap
            else
                E = n0 + m_Ws;  // open a new gap

            n0 = rowV[j] + m_Wg;
            if (rowF[j] > n0)
                rowF[j] += m_Ws;
            else
                rowF[j] = n0 + m_Ws;

            V = E >= rowF[j]? (E >= G? E: G): (rowF[j] >= G? rowF[j]: G);
        }

        pV[j] = V;
    }

    // the top row (i == 0)
    {
        vF[N2-1] = V = V0 += m_Ws;
        vG[N2-1] = vE[N2-1] = E = kInfMinus;
        trace[N2-1] = kMaskFc;
        char ci = seq1[i];

        unsigned char tracer;
        for (j = N2 - 2; j >= 0; --j) {

            vG[j] = G = pV[j] + m_Matrix[ci][seq2[j]];
            pV[j] = V;

            n0 = V + m_Wg;
            if(E > n0) {
                E += m_Ws;      // continue the gap
                tracer = kMaskEc;
            }
            else {
                E = n0 + m_Ws;  // open a new gap
                tracer = 0;
            }
            vE[j] = E;

            n0 = rowV[j] + m_Wg;
            if(rowF[j] > n0) {
                rowF[j] += m_Ws;
                tracer |= kMaskFc;
            }
            else {
                rowF[j] = n0 + m_Ws;
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
    
    delete[] rowV;
    delete[] rowF;
}


CNWAligner::TScore CMMAligner::x_RunTerm(const SCoordRect& rect,
                                         bool left_top, bool right_bottom,
                                         list<ETranscriptSymbol>& subpath)
{
    const int N1 = rect.i2 - rect.i1 + 2;
    const int N2 = rect.j2 - rect.j1 + 2;

    TScore* rowV    = new TScore [N2];
    TScore* rowF    = new TScore [N2];

    // index calculation: [i,j] = i*n2 + j
    unsigned char* backtrace = new unsigned char [N1*N2];

    TScore* pV = rowV - 1;

    const char* seq1 = m_Seq1 + rect.i1 - 1;
    const char* seq2 = m_Seq2 + rect.j1 - 1;

    // first row
    rowV[0] = m_Wg;
    int k;
    for (k = 1; k < N2; k++) {
        rowV[k] = pV[k] + m_Ws;
        rowF[k] = kInfMinus;
        backtrace[k] = kMaskE | kMaskEc;
    }
    rowV[0] = 0;

    // recurrences
    TScore V  = 0;
    TScore V0 = left_top? 0: m_Wg;
    TScore E, G, n0;
    unsigned char tracer;
    char ci;

    int i, j;
    for(i = 1;  i < N1;  ++i) {
        
        V = V0 += m_Ws;
        E = kInfMinus;
        backtrace[k++] = kMaskFc;
        ci = seq1[i];
        
        for (j = 1; j < N2; ++j, ++k) {

            G = pV[j] + m_Matrix[ci][seq2[j]];
            pV[j] = V;

            n0 = V + m_Wg;
            if(E > n0) {
                E += m_Ws;      // continue the gap
                tracer = kMaskEc;
            }
            else {
                E = n0 + m_Ws;  // open a new gap
                tracer = 0;
            }

            n0 = rowV[j] + ((right_bottom && j == N2 - 1)? 0: m_Wg);
            if(rowF[j] > n0) {
                rowF[j] += m_Ws;
                tracer |= kMaskFc;
            }
            else {
                rowF[j] = n0 + m_Ws;
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
    
    k = N1*N2 - 1;
    while (k != 0) {
        unsigned char Key = backtrace[k];
        if (Key & kMaskD) {
            subpath.push_front(eMatch);
            k -= N2 + 1;
        }
        else if (Key & kMaskE) {
            subpath.push_front(eInsert); --k;
            while(k > 0 && (Key & kMaskEc)) {
                subpath.push_front(eInsert);
                Key = backtrace[k--];
            }
        }
        else {
            subpath.push_front(eDelete);
            k -= N2;
            while(k > 0 && (Key & kMaskFc)) {
                subpath.push_front(eDelete);
                Key = backtrace[k];
                k -= N2;
            }
        }
    }

    delete[] backtrace;
    delete[] rowV;
    delete[] rowF;

    return V;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2003/01/28 12:37:40  kapustin
 * Move m_score to the base class
 *
 * Revision 1.3  2003/01/22 13:40:09  kapustin
 * Implement multithread algorithm
 *
 * ===========================================================================
 */
