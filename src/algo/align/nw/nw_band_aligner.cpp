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

CBandAligner::CBandAligner( const char* seq1, size_t len1,
                            const char* seq2, size_t len2,
                            const SNCBIPackedScoreMatrix* scoremat,
                            size_t band):
    CNWAligner(seq1, len1, seq2, len2, scoremat),
    m_band(band)
{
}


CBandAligner::CBandAligner(const string& seq1,
                           const string& seq2,
                           const SNCBIPackedScoreMatrix* scoremat,
                           size_t band):
    CNWAligner(seq1, seq2, scoremat),
    m_band(band)
{
}


// evaluate score for each possible alignment;
// fill out backtrace matrix limited to the band
// bit coding (four bits per value): D E Ec Fc
// D:  1 if diagonal; 0 - otherwise
// E:  1 if space in 1st sequence; 0 if space in 2nd sequence
// Ec: 1 if gap in 1st sequence was extended; 0 if it is was opened
// Fc: 1 if gap in 2nd sequence was extended; 0 if it is was opened
//

const unsigned char kMaskFc  = 0x01;
const unsigned char kMaskEc  = 0x02;
const unsigned char kMaskE   = 0x04;
const unsigned char kMaskD   = 0x08;
const unsigned char kVoid    = 0xFF;

CNWAligner::TScore CBandAligner::x_Align(SAlignInOut* data)
{
    if(size_t(abs(int(data->m_len1) - int(data->m_len2))) > m_band) {

        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "Difference in sequence lengths "
                   "should not exceed the band");
    }

    if(m_band >= data->m_len1 || m_band >= data->m_len2) {

        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   "Band should be less than the sequence lengths");
    }

    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = 2*(m_band + 1) + 1;
    const size_t n2 = data->m_len2 + 1;

    vector<TScore> stl_rowV (n2), stl_rowF(n2);

    TScore* rowV    = &stl_rowV.front();
    TScore* rowF    = &stl_rowF.front();

    TScore* pV = rowV - 1;

    const char* seq1 = m_Seq1 + data->m_offset1 - 1;
    const char* seq2 = m_Seq2 + data->m_offset2 - 1;

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    m_terminate = false;
    if(m_prg_callback) {

        m_prg_info.m_iter_total = N1*N2;
        m_prg_info.m_iter_done = 0;
        if(m_terminate = m_prg_callback(&m_prg_info)) {
	  return 0;
	}
    }

    TScore wgleft1   = m_Wg;
    TScore wsleft1   = m_Ws;
    TScore wg1 = m_Wg, ws1 = m_Ws;

    // index calculation: [i,j] = i*N2 + j
    
    vector<unsigned char> stl_bm (N1*N2, kVoid);
    unsigned char* backtrace_matrix = &stl_bm[0];

    // first row
    size_t k = m_band + 2;
    rowV[0] = wgleft1;
    for(size_t i = 1; i <= m_band + 1; ++i, ++k) {

        rowV[i] = pV[i] + wsleft1;
        rowF[i] = kInfMinus;
        backtrace_matrix[k] = kMaskE | kMaskEc;
    }
    rowV[0] = 0;
	
    if(m_prg_callback) {
        m_prg_info.m_iter_done = N2;
        m_terminate = m_prg_callback(&m_prg_info);
    }

    // recurrences
    TScore wgleft2   = m_Wg;
    TScore wsleft2   = m_Ws;
    TScore V = 0;
    TScore V0 = wgleft2;
    TScore E, G, n0;
    unsigned char tracer = 0;

    size_t i, j;
    for(i = 1;  i < N1 && !m_terminate;  ++i) {
        
        int di = int(i) - (m_band + 1);
        if(di <= 0) {

            V = V0 += wsleft2;
            E = kInfMinus;
            k = N2*i - di;
            backtrace_matrix[k] = kMaskFc;
        }
        else {

            k = N2*i;
            V = V0 = E = kInfMinus;
        }
        unsigned char ci = seq1[i];
        ++k;

        TScore wg2 = m_Wg, ws2 = m_Ws;

        size_t jstart = (di <= 0)? 1: (di + 1);
        size_t jend = i + m_band;
        if(jend >= n2) {
            jend = n2 - 1;
        }

        for (j = jstart; j <= jend; ++j, ++k) {
            
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

            if(i == 1 || j < m_band + i) {

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
            } else {
                if(rowF[j] >= G) {
                    V = rowF[j];
                }
                else {
                    V = G;
                    tracer |= kMaskD;
                }
            }
            backtrace_matrix[k] = tracer;
        }

        pV[j] = V;

        if(m_prg_callback) {
            m_prg_info.m_iter_done = k;
            if(m_terminate = m_prg_callback(&m_prg_info)) {
                break;
            }
        }
       
    }

//#define NWB_DUMP_DPM
#ifdef  NWB_DUMP_DPM
    for(size_t i = 0; i < N1; ++i) {
        for(size_t j = 0; j < N2; ++j) {
            unsigned int a = backtrace_matrix[i*N2 + j];
            cerr << hex << a << '\t';
        }
        cerr << endl;
    }
    cerr << dec;
#endif

    if(!m_terminate) {
        x_DoBackTrace(backtrace_matrix, data);
    }

    return V;
}


void CBandAligner::x_DoBackTrace(const unsigned char* backtrace,
                                 CNWAligner::SAlignInOut* data)
{
    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = 2*(m_band + 1) + 1;

    data->m_transcript.clear();
    data->m_transcript.reserve(N1 + N2);

    size_t k = N1*N2 - 1, k_end = m_band + 1;

    size_t i1 = data->m_offset1 + data->m_len1 - 1;
    size_t i2 = data->m_offset2 + data->m_len2 - 1;

    while(backtrace[k] == kVoid) {
        --k;
        --i2;
    }

    while (k != k_end) {

        unsigned char Key = backtrace[k];

        if(Key == kVoid) {

            NCBI_THROW(CAlgoAlignException, eInternal,
                       g_msg_InvalidBacktraceData);
        }

        if (Key & kMaskD) {
            data->m_transcript.push_back(x_GetDiagTS(i1--, i2--));
            k -= N2;
        }
        else if (Key & kMaskE) {
            data->m_transcript.push_back(eTS_Insert);
            --k;
            --i2;
            while(k != k_end && (Key & kMaskEc)) {
                data->m_transcript.push_back(eTS_Insert);
                Key = backtrace[k--];
                --i2;
            }
        }
        else {
            data->m_transcript.push_back(eTS_Delete);
            k -= (N2-1);
            --i1;
            while(k != k_end && (Key & kMaskFc)) {
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
    const size_t elem_size = GetElemSize();
    const size_t gdim = m_guides.size();
    const size_t max_mem = kMax_UInt / 2;
    if(gdim) {

        size_t dim1 = m_guides[0], dim2 = m_guides[2];
        double mem = double(max(dim1, dim2))*m_band*elem_size;
        if(mem >= max_mem) {
            return false;
        }
        for(size_t i = 4; i < gdim; i += 4) {
            dim1 = m_guides[i] - m_guides[i-3] + 1;
            dim2 = m_guides[i + 2] - m_guides[i-1] + 1;
            mem = double(max(dim1, dim2))*m_band*elem_size;
            if(mem >= max_mem) {
                return false;
            }
        }
        dim1 = m_SeqLen1 - m_guides[gdim-3];
        dim2 = m_SeqLen2 - m_guides[gdim-1];
        mem = double(max(dim1, dim2))*m_band*elem_size;
        if(mem >= max_mem) {
            return false;
        }

        return true;
    }
    else {

        size_t max_len = max(m_SeqLen1, m_SeqLen2);
        double mem = double(max_len)*m_band*elem_size;
        return mem < max_mem;
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2005/04/04 16:34:13  kapustin
 * Specify precise type of diags in raw alignment transcripts where feasible
 *
 * Revision 1.5  2005/03/16 15:48:26  jcherry
 * Allow use of std::string for specifying sequences
 *
 * Revision 1.4  2005/03/02 14:26:16  kapustin
 * A few tweaks to mute GCC and MSVC warnings
 *
 * Revision 1.3  2004/12/16 22:42:22  kapustin
 * Move to algo/align/nw
 *
 * Revision 1.2  2004/11/29 14:37:15  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two additional parameters to specify starting coordinates.
 *
 * Revision 1.1  2004/09/16 19:27:43  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

