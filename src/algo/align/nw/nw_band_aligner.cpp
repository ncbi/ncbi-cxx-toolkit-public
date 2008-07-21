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
 * File Description:  CBandAligner implementation
 *                   
 * ===========================================================================
 *
 */


#include <ncbi_pch.hpp>

#include "messages.hpp"

#include <algo/align/nw/align_exception.hpp>
#include <algo/align/nw/nw_band_aligner.hpp>

BEGIN_NCBI_SCOPE

static const size_t kMax_size_t = numeric_limits<size_t>::max();

CBandAligner::CBandAligner( const char* seq1, size_t len1,
                            const char* seq2, size_t len2,
                            const SNCBIPackedScoreMatrix* scoremat,
                            size_t band):
    CNWAligner(seq1, len1, seq2, len2, scoremat),
    m_band(band),
    m_Shift(0)
{
}


CBandAligner::CBandAligner(const string& seq1,
                           const string& seq2,
                           const SNCBIPackedScoreMatrix* scoremat,
                           size_t band):
    CNWAligner(seq1, seq2, scoremat),
    m_band(band),
    m_Shift(0)
{
}


void CBandAligner::SetShift(Uint1 where, size_t offset)
{
    switch(where) {
    case 0: m_Shift = offset;  break;
    case 1: m_Shift = -offset; break;
    default:
        NCBI_THROW(CAlgoAlignException, eBadParameter, 
                   "CBandAligner::SetShift(): Incorrect sequence index specified");
    }
}


pair<Uint1,size_t> CBandAligner::GetShift(void) const
{
    Uint1 where;
    size_t offset;
    if(m_Shift < 0) {
        where = 1;
        offset = size_t(-m_Shift);
    }
    else {
        where = 0;
        offset = size_t(m_Shift);
    }
    return pair<Uint1,size_t>(where, offset);
}


// evaluate score for each possible alignment;
// fill out backtrace matrix limited to the band
// bit coding (four bits per value): D E Ec Fc
// D:  1 if diagonal; 0 - otherwise
// E:  1 if space in 1st sequence; 0 if space in 2nd sequence
// Ec: 1 if gap in 1st sequence was extended; 0 if it is was opened
// Fc: 1 if gap in 2nd sequence was extended; 0 if it is was opened
//

const Uint1 kMaskFc (0x01);
const Uint1 kMaskEc (0x02);
const Uint1 kMaskE  (0x04);
const Uint1 kMaskD  (0x08);

CNWAligner::TScore CBandAligner::x_Align(SAlignInOut* data)
{
    x_CheckParameters(data);

    const size_t N1 (data->m_len1);
    const size_t N2 (data->m_len2);
    const size_t fullrow (2*m_band + 1);
    vector<TScore> stl_rowV (N2), stl_rowF (N2);
    TScore  * rowV (&stl_rowV.front());
    TScore  * rowF (&stl_rowF.front());
    TScore* pV (rowV - 1);
    const char* seq1 (m_Seq1 + data->m_offset1);
    const char* seq2 (m_Seq2 + data->m_offset2);
    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;
    
    m_terminate = false;

    if(m_prg_callback) {
        m_prg_info.m_iter_total = N1*fullrow;
        m_prg_info.m_iter_done = 0;
        if(m_terminate = m_prg_callback(&m_prg_info)) {
            return 0;
        }
    }
    
    CBacktraceMatrix4 backtrace_matrix (N1*fullrow);

    TScore V (0);
    TScore E (kInfMinus), V2 (kInfMinus), G, n0;
    Uint1 tracer (0);

    const size_t ibeg ((m_Shift >= 0 && m_Shift > long(m_band))?
                       (m_Shift - m_band): 0);

    const TScore wgleft1 (data->m_esf_L1? 0: m_Wg);
    const TScore wsleft1 (data->m_esf_L1? 0: m_Ws);
    const TScore wgleft2 (data->m_esf_L2? 0: m_Wg);
    const TScore wsleft2 (data->m_esf_L2? 0: m_Ws);

    TScore wg1 (m_Wg), ws1 (m_Ws);
    TScore V1 (wgleft2 + wsleft2 * ibeg);

    const long d1 (N2 + m_band + m_Shift);
    const size_t iend (d1 <= 0? 0: (d1 < long(N1)? d1: N1));

    m_LastCoordSeq1 = m_LastCoordSeq2 = m_TermK = kMax_size_t;

    size_t jendcnt (0);
    for(size_t i (ibeg); i < iend && !m_terminate; ++i) {

        TScore wg2 (m_Wg), ws2 (m_Ws);

        const long   d2   (i - m_Shift - m_band);
        const size_t jbeg (d2 < 0? 0: d2);
        const long   d3   (i - m_Shift + m_band + 1);
        const size_t jend (d3 < long(N2)? d3: N2);
        if(jend == N2) ++jendcnt;

        const Uint1 ci (seq1[i]);
        
        if(i == 0) {
            V2 = wgleft1 + wsleft1 * jbeg;
            TScore s (wgleft1);
            for(size_t j (0); j < N2; ++j) {
                s += wsleft1;
                rowV[j] = s;
            }
        }

        if(i + 1 == N1 && data->m_esf_R1) {
            wg1 = ws1 = 0;
        }
        
        size_t j;
        size_t k (fullrow * (i - ibeg));
        const long jbeg0 (i - m_Shift - m_band);
        if(jbeg0 < 0) {
            k += jbeg - jbeg0;
        }
        if(size_t(k - 1) > m_TermK && (k & 1)) {
            backtrace_matrix.SetAt(k - 1, 0);
        }

        for(j = jbeg; j < jend; ++j) {            

            if(j > 0) {
                G = sm[ci][Uint1(seq2[j])] + pV[j];
                pV[j] = V;
            }
            else {
                G = sm[ci][(Uint1)seq2[j]];
                if(i > 0) G += V1;                
            }

            if(j > jbeg) {
                n0 = V + wg1;
                if(E >= n0) {
                    E += ws1;      // gap extension
                    tracer = kMaskEc;
                }
                else {
                    E = n0 + ws1;  // gap open
                    tracer = 0;
                }
            }
            else if(j == 0 && i < m_Shift + m_band) {
                V1 += wsleft2;
                E = V1 + wg1 + ws1;
                tracer = 0;
            }
            else {
                E = kInfMinus;
                tracer = 0; // to fire during the backtrace
            }

            if(j + 1 == N2 && data->m_esf_R2) {
                wg2 = ws2 = 0;
            }

            if(i == 0) {
                if(j + 1 < jend) {
                    V2 += wsleft1;
                    rowF[j] = V2 + wg2 + ws2;
                }
                else {
                    rowF[j] = kInfMinus;
                }
            }
            else if(j + 1 < jend || jendcnt > 1) {
                n0 = rowV[j] + wg2;
                if(rowF[j] >= n0) {
                    rowF[j] += ws2;
                    tracer |= kMaskFc;
                }
                else {
                    rowF[j] = n0 + ws2;
                }
            }
            else {
                rowF[j] = kInfMinus;
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
            }
            else {
                if(rowF[j] >= G) {
                    V = rowF[j];
                }
                else {
                    V = G;
                    tracer |= kMaskD;
                }
            }
            backtrace_matrix.SetAt(k++,tracer);
        }

        backtrace_matrix.Purge(k);

        pV[j] = V;

        m_TermK = k - 1;
        m_LastCoordSeq2 = jend - 1;

        if(m_prg_callback) {
            m_prg_info.m_iter_done = k;
            m_terminate = m_prg_callback(&m_prg_info);
        }
    }

    m_LastCoordSeq1 = iend - 1;

    const bool uninit (m_TermK == kMax_size_t 
                       || m_LastCoordSeq1 == kMax_size_t 
                       || m_LastCoordSeq2 == kMax_size_t);

    if(uninit && !m_terminate) {
        NCBI_THROW(CAlgoAlignException, eInternal, g_msg_UnexpectedTermIndex);
    }

    if(!m_terminate) {
        x_DoBackTrace(backtrace_matrix, data);
    }

    return V;
}


void CBandAligner::x_CheckParameters(const SAlignInOut* data) const
{
    if(data->m_len1 < 2 || data->m_len2 < 2) {        
        NCBI_THROW(CAlgoAlignException, eBadParameter, 
                   "Input sequence interval too small.");        
    }

    if(m_Shift > 0 && m_Shift > long(data->m_len1 + m_band)) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "Shift is greater than the first sequence's length.");
    }

    if(m_Shift < 0 && -m_Shift > long(data->m_len2 + m_band)) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "Shift is greater than the second sequence's length.");
    }

    // Each of the following checks will verify if a corresponding end 
    // is within the band and, if it is not, whether the end-space free
    // mode was specified for that end.
    // When doing restrained alignment, the exception may be caused by improper
    // adjustment of the band offset.

    string msg_head;

    // seq 1, left
    if(m_Shift >= 0 && m_band < size_t(m_Shift) && !data->m_esf_L2) {
        msg_head = "Left end of first sequence ";
    }

    // seq 1,2, right
    if(m_Shift + long(data->m_len2 + m_band) < long(data->m_len1) && !data->m_esf_R2)
    {
        msg_head = "Right end of first sequence ";
    }
    else if(long(data->m_len1 + m_band) - m_Shift < long(data->m_len2)
            && !data->m_esf_R1)
    {
        msg_head = "Right end of second sequence ";
    }

    // seq 2, left
    if(m_Shift < 0 && m_band < size_t(-m_Shift) && !data->m_esf_L1) {
        msg_head = "Left end of second sequence ";
    }

    if(msg_head.size() > 0) {
        const string msg_tail ("out of band and end-space free flag not set.");
        const string msg = msg_head + msg_tail;
        NCBI_THROW(CAlgoAlignException, eBadParameter, msg.c_str());
    }
}


void CBandAligner::x_DoBackTrace(const CBacktraceMatrix4 & backtrace,
                                 CNWAligner::SAlignInOut* data)
{
    const size_t N1 (data->m_len1);
    const size_t N2 (2*m_band + 1);

    data->m_transcript.clear();
    data->m_transcript.reserve(N1 + N2);

    size_t k (m_TermK);

    size_t i1 (m_LastCoordSeq1);
    size_t i2 (m_LastCoordSeq2);

    if(m_LastCoordSeq1 + 1 < data->m_len1 && data->m_esf_R2) {
        data->m_transcript.insert(data->m_transcript.begin(),
                                  data->m_len1 - m_LastCoordSeq1 - 1, 
				  eTS_Delete);
    }

    if(m_LastCoordSeq2 + 1 < data->m_len2 && data->m_esf_R1) {
        data->m_transcript.insert(data->m_transcript.begin(),
                                  data->m_len2 - m_LastCoordSeq2 - 1, 
				  eTS_Insert);
    }

    while (true) {

        const size_t kOverflow (kMax_size_t - 256), kMax (kMax_size_t);

        const bool invalid_backtrace_data (
            (i1 > kOverflow && i1 != kMax || i2 > kOverflow && i2 != kMax) ||
            ((size_t)abs(m_Shift - long(i1) + long(i2))) > m_band );

        if(invalid_backtrace_data) {
            NCBI_THROW(CAlgoAlignException, eInternal, g_msg_InvalidBacktraceData);
        }

        if(i1 == kMax) {
            if(i2 == kMax) break;
            do {
                data->m_transcript.push_back(eTS_Insert);
                --i2;
            } while (i2 != kMax);
            break;
        }

        if(i2 == kMax) {
            do {
                data->m_transcript.push_back(eTS_Delete);
                --i1;
            } while (i1 != kMax);
            break;
        }

        Uint1 Key (backtrace[k]);

        if (Key & kMaskD) {
            data->m_transcript.push_back(
               x_GetDiagTS(data->m_offset1 + i1--, data->m_offset2 + i2--));
            k -= N2;
        }
        else if (Key & kMaskE) {
            data->m_transcript.push_back(eTS_Insert);
            --k;
            --i2;
            while(i2 != kMax && (Key & kMaskEc)) {
                data->m_transcript.push_back(eTS_Insert);
                Key = backtrace[k--];
                --i2;
            }
        }
        else {
            data->m_transcript.push_back(eTS_Delete);
            k -= (N2-1);
            --i1;
            while(i1 != kMax && (Key & kMaskFc)) {
                data->m_transcript.push_back(eTS_Delete);
                Key = backtrace[k];
                k -= (N2-1);
                --i1;
            }
        }
    }
}


bool CBandAligner::x_CheckMemoryLimit()
{
    const size_t elem_size (GetElemSize());
    const size_t gdim (m_guides.size());

    if(gdim) {

        size_t dim1 = m_guides[0], dim2 = m_guides[2];
        double mem = double(max(dim1, dim2))*m_band*elem_size;
        if(mem >= m_MaxMem) {
            return false;
        }
        for(size_t i = 4; i < gdim; i += 4) {
            dim1 = m_guides[i] - m_guides[i-3] + 1;
            dim2 = m_guides[i + 2] - m_guides[i-1] + 1;
            mem = double(max(dim1, dim2))*m_band*elem_size;
            if(mem >= m_MaxMem) {
                return false;
            }
        }
        dim1 = m_SeqLen1 - m_guides[gdim-3];
        dim2 = m_SeqLen2 - m_guides[gdim-1];
        mem = double(max(dim1, dim2))*m_band*elem_size;
        if(mem >= m_MaxMem) {
            return false;
        }

        return true;
    }
    else {

        size_t max_len = max(m_SeqLen1, m_SeqLen2);
        double mem = double(max_len)*m_band*elem_size;
        return mem < m_MaxMem;
    }
}


END_NCBI_SCOPE
