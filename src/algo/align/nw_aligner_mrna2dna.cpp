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
* File Description:  CNWAlignerMrna2Dna
*                   
* ===========================================================================
*
*/


#include <algo/align/nw_aligner_mrna2dna.hpp>
#include <corelib/ncbi_limits.h>

BEGIN_NCBI_SCOPE

static unsigned char donor[splice_type_count][2] = {
    {'G','T'}, {'G','C'}, {'A','T'}, {'?','?'}
};

static unsigned char acceptor[splice_type_count][2] = {
    {'A','G'}, {'A','G'}, {'A','C'}, {'?','?'}
};


CNWAlignerMrna2Dna::CNWAlignerMrna2Dna(const char* seq1, size_t len1,
                                       const char* seq2, size_t len2)
    throw(CAlgoAlignException)
    : CNWAligner(seq1, len1, seq2, len2, eNucl),
    m_IntronMinSize(GetDefaultIntronMinSize())
{
    for(unsigned char st = 0; st < splice_type_count; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
    
    // default for spliced alignment
    SetEndSpaceFree(true, true, false, false);

}


CNWAligner::TScore CNWAlignerMrna2Dna::GetDefaultWi(unsigned char splice_type)
    throw(CAlgoAlignException)
{
    switch(splice_type) {
    case 0: return -30;
    case 1: return -30;
    case 2: return -30;
    case 3: return -40; // arbitrary splice (??/??)
    }

    NCBI_THROW(CAlgoAlignException,
               eInvalidSpliceTypeIndex,
               "Invalid splice type index");
}


void CNWAlignerMrna2Dna::SetWi  (unsigned char splice_type, TScore value) {

    if(splice_type < splice_type_count) {
        m_Wi[splice_type]  = value;
    }
    else {
        NCBI_THROW(CAlgoAlignException,
                   eInvalidSpliceTypeIndex,
                   "Invalid splice type index");
    }
}


// Bit coding (eleven bits per value) for backtrace.
// --------------------------------------------------
// [11-8] donors (bitwise OR for multiple types)
//        1000     ??    (??/??) - arbitrary pair
//        0100     AT    (AT/AC)
//        0010     GC    (GC/AG)
//        0001     GT    (GT/AG)
// [7-5]  acceptor type
//        100      ?? (??/??)
//        011      AC (AT/AC)
//        010      AG (GC/AG)
//        001      AG (GT/AG)
//        000      no acceptor
// [4]    Fc:      1 if gap in 2nd sequence was extended; 0 if it is was opened
// [3]    Ec:      1 if gap in 1st sequence was extended; 0 if it is was opened
// [2]    E:       1 if space in 1st sequence; 0 if space in 2nd sequence
// [1]    D:       1 if diagonal; 0 - otherwise

const unsigned char kMaskFc       = 0x0001;
const unsigned char kMaskEc       = 0x0002;
const unsigned char kMaskE        = 0x0004;
const unsigned char kMaskD        = 0x0008;

// Evaluate dynamic programming matrix. Create transcript.
CNWAligner::TScore CNWAlignerMrna2Dna::x_Align (
                         const char* seg1, size_t len1,
                         const char* seg2, size_t len2,
                         vector<ETranscriptSymbol>* transcript )
{
    const size_t N1 = len1 + 1;
    const size_t N2 = len2 + 1;

    TScore* rowV    = new TScore [N2];
    TScore* rowF    = new TScore [N2];

    // index calculation: [i,j] = i*n2 + j
    Uint2* backtrace_matrix = new Uint2 [N1*N2];

    TScore* pV = rowV - 1;

    const char* seq1   = seg1 - 1;
    const char* seq2   = seg2 - 1;

    bool bFreeGapLeft1  = m_esf_L1 && seg1 == m_Seq1;
    bool bFreeGapRight1 = m_esf_R1 && m_Seq1 + m_SeqLen1 - len1 == seg1;
    bool bFreeGapLeft2  = m_esf_L2 && seg2 == m_Seq2;
    bool bFreeGapRight2 = m_esf_R2 && m_Seq2 + m_SeqLen2 - len2 == seg2;

    TScore wgleft1   = bFreeGapLeft1? 0: m_Wg;
    TScore wsleft1   = bFreeGapLeft1? 0: m_Ws;
    TScore wg1 = wgleft1, ws1 = wsleft1;

    // recurrences
    TScore wgleft2   = bFreeGapLeft2? 0: m_Wg;
    TScore wsleft2   = bFreeGapLeft2? 0: m_Ws;
    TScore V  = 0, V_max, vAcc;
    TScore V0 = 0;
    TScore E, G, n0;
    Uint2 tracer;

    // store candidate donors
    size_t* jAllDonors [splice_type_count];
    TScore* vAllDonors [splice_type_count];
    for(unsigned char st = 0; st < splice_type_count; ++st) {
        jAllDonors[st] = new size_t [N2];
        vAllDonors[st] = new TScore [N2];
    }
    size_t  jTail[splice_type_count], jHead[splice_type_count];
    TScore  vBestDonor [splice_type_count];
    size_t  jBestDonor [splice_type_count];

    // fake row
    rowV[0] = kInfMinus;
    size_t k;
    for (k = 0; k < N2; k++) {
        rowV[k] = rowF[k] = kInfMinus;
    }
    k = 0;

    size_t i, j = 0, k0;
    char ci;
    for(i = 0;  i < N1;  ++i, j = 0) {
       
        V = i > 0? (V0 += wsleft2) : 0;
        E = kInfMinus;
        k0 = k;
        backtrace_matrix[k++] = kMaskFc;
        ci = i > 0? seq1[i]: 'N';

        for(unsigned char st = 0; st < splice_type_count; ++st) {
            jTail[st] = jHead[st] = 0;
            vBestDonor[st] = kInfMinus;
        }

        if(i == N1 - 1 && bFreeGapRight1) {
                wg1 = ws1 = 0;
        }

        TScore wg2 = m_Wg, ws2 = m_Ws;
            
        // detect donor candidate
        for( char st = splice_type_count - 2; st >= 0 && N2 > 2; --st ) {
            if( seq2[1] == donor[st][0] && seq2[2] == donor[st][1]) {
                jAllDonors[st][jTail[st]] = j;
                vAllDonors[st][jTail[st]] = V;
                ++(jTail[st]);
            }
        }
        // the first max value
        jAllDonors[3][jTail[3]] = j;
        vAllDonors[3][jTail[3]] = V_max = V;
        ++(jTail[3]);

        for (j = 1; j < N2; ++j, ++k) {
            
            G = pV[j] + m_Matrix[ci][seq2[j]];
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
            n0 = rowV[j] + wg2;
            if(rowF[j] >= n0) {
                rowF[j] += ws2;
                tracer |= kMaskFc;
            }
            else {
                rowF[j] = n0 + ws2;
            }

            // evaluate the score (V)
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

            // find out if there are new donors
            for(unsigned char st = 0; st < splice_type_count; ++st) {

                if(jTail[st] > jHead[st])  {
                    if(j - jAllDonors[st][jHead[st]] >= m_IntronMinSize) {
                        if(vAllDonors[st][jHead[st]] > vBestDonor[st]) {
                            vBestDonor[st] = vAllDonors[st][jHead[st]];
                            jBestDonor[st] = jAllDonors[st][jHead[st]];
                        }
                        ++(jHead[st]);
                    }
                }
            }
                
            // check splice signal
            size_t dnr_pos = 0;
            Uint2 tracer_dnr = 0xFFFF;
            Uint2 tracer_acc = 0;
            for(char st = splice_type_count - 1; st >= 0; --st) {
                if(seq2[j-1] == acceptor[st][0] && seq2[j] == acceptor[st][1]
                   && vBestDonor[st] > kInfMinus || st == 3) {
                    vAcc = vBestDonor[st] + m_Wi[st];
                    if(vAcc > V) {
                        V = vAcc;
                        tracer_acc = (st+1) << 4;
                        dnr_pos = k0 + jBestDonor[st];
                        tracer_dnr = 0x0080 << st;
                    }
                }
            }

            if(tracer_dnr != 0xFFFF) {
                backtrace_matrix[dnr_pos] |= tracer_dnr;
                tracer |= tracer_acc;
            }

            backtrace_matrix[k] = tracer;

            // detect donor candidate
            for( char st = splice_type_count - 1; 
                 j < N2 - 2 && st >= 0; --st ) {

                if( seq2[j+1] == donor[st][0] && seq2[j+2] == donor[st][1] ) {
                    jAllDonors[st][jTail[st]] = j;
                    vAllDonors[st][jTail[st]] = V;
                    ++(jTail[st]);
                }
            }

            // detect new best value
            if(V > V_max) {
                jAllDonors[3][jTail[3]] = j;
                vAllDonors[3][jTail[3]] = V_max = V;
                ++(jTail[3]);
            }
        }

        pV[j] = V;

        if(i == 0) {
            V0 = wgleft2;
            wg1 = m_Wg;
            ws1 = m_Ws;
        }

    }

    for(unsigned char st = 0; st < splice_type_count; ++st) {
        delete[] jAllDonors[st];
        delete[] vAllDonors[st];
    }
    delete[] rowV;
    delete[] rowF;

    x_DoBackTrace(backtrace_matrix, N1, N2, transcript);

    delete[] backtrace_matrix;

    return V;
}


// perform backtrace step;
void CNWAlignerMrna2Dna::x_DoBackTrace ( const Uint2* backtrace_matrix,
                                         size_t N1, size_t N2,
                                         vector<ETranscriptSymbol>* transcript)
{
   
    transcript->clear();
    transcript->reserve(N1 + N2);

    size_t k = N1*N2 - 1;
    while (k != 0) {
        Uint2 Key = backtrace_matrix[k];
        while(Key & 0x0070) {  // acceptor

            unsigned char acc_type = (Key & 0x0070) >> 4;
            Uint2 dnr_mask = 0x0040 << acc_type;
            ETranscriptSymbol ets =
                ETranscriptSymbol (eIntron_GT_AG + acc_type - 1);
            do {
                transcript->push_back(ets);
                Key = backtrace_matrix[--k];
            }
            while(!(Key & dnr_mask));

            if(Key & 0x0070) {
                NCBI_THROW(CAlgoAlignException,
                           eInternal,
                           "Adjacent splices encountered during backtrace");
            }
        }

        if(k == 0) continue;
        
        if (Key & kMaskD) {
            transcript->push_back(eMatch);  // or eReplace
            k -= N2 + 1;
        }
        else if (Key & kMaskE) {
            transcript->push_back(eInsert); --k;
            while(k > 0 && (Key & kMaskEc)) {
                transcript->push_back(eInsert);
                Key = backtrace_matrix[k--];
            }
        }
        else {
            transcript->push_back(eDelete);
            k -= N2;
            while(k > 0 && (Key & kMaskFc)) {
                transcript->push_back(eDelete);
                Key = backtrace_matrix[k];
                k -= N2;
            }
        }
    }
}


CNWAligner::TScore CNWAlignerMrna2Dna::x_ScoreByTranscript() const
    throw (CAlgoAlignException)
{
    const size_t dim = m_Transcript.size();
    if(dim == 0) return 0;

    TScore score = 0;

    const char* p1 = m_Seq1;
    const char* p2 = m_Seq2;

    char state1;   // 0 = normal, 1 = gap, 2 = intron
    char state2;   // 0 = normal, 1 = gap

    switch( m_Transcript[dim - 1] ) {

    case eMatch:
        state1 = state2 = 0;
        break;

    case eInsert:
        state1 = 1; state2 = 0; score += m_Wg;
        break;

    case eIntron_GT_AG:
    case eIntron_GC_AG:
    case eIntron_AT_AC:
    case eIntron_Generic:
        state1 = 0; state2 = 0;
        break; // intron flag set later

    case eDelete:
        state1 = 0; state2 = 1; score += m_Wg;
        break;

    default: {
        NCBI_THROW(
                   CAlgoAlignException,
                   eInternal,
                   "Invalid transcript symbol");
        }
    }

    for(int i = dim - 1; i >= 0; --i) {

        char c1 = m_Seq1? *p1: 0;
        char c2 = m_Seq2? *p2: 0;
        switch(m_Transcript[i]) {

        case eMatch: {
            state1 = state2 = 0;
            score += m_Matrix[c1][c2];
            ++p1; ++p2;
        }
        break;

        case eInsert: {
            if(state1 != 1) score += m_Wg;
            state1 = 1; state2 = 0;
            score += m_Ws;
            ++p2;
        }
        break;

        case eDelete: {
            if(state2 != 1) score += m_Wg;
            state1 = 0; state2 = 1;
            score += m_Ws;
            ++p1;
        }
        break;

        case eIntron_GT_AG:
        case eIntron_GC_AG:
        case eIntron_AT_AC:
        case eIntron_Generic: {

            if(state1 != 2) {
                for(unsigned char i = 0; i < splice_type_count; ++i) {
                    if(*p2 == donor[i][0] && *(p2 + 1) == donor[i][1]
                       || i == splice_type_count - 1) {
                        score += m_Wi[i];
                        break;
                    }
                }
            }
            state1 = 2; state2 = 0;
            ++p2;
        }
        break;

        default: {
        NCBI_THROW(
                   CAlgoAlignException,
                   eInternal,
                   "Invalid transcript symbol");
        }
        }
    }

    if(m_esf_L1) {
        size_t g = 0;
        for(int i = dim - 1; i >= 0; --i) {
            if(m_Transcript[i] == eInsert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_L2) {
        size_t g = 0;
        for(int i = dim - 1; i >= 0; --i) {
            if(m_Transcript[i] == eDelete) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R1) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(m_Transcript[i] == eInsert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R2) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(m_Transcript[i] == eDelete) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    return score;
}


unsigned char CNWAlignerMrna2Dna::x_CalcFingerPrint64(
    const char* beg, const char* end, size_t& err_index)
{
    if(beg >= end) {
        return 0xFF;
    }

    unsigned char fp = 0, code;
    for(const char* p = beg; p < end; ++p) {
        switch(*p) {
        case 'A': code = 0;    break;
        case 'G': code = 0x01; break;
        case 'T': code = 0x02; break;
        case 'C': code = 0x03; break;
        default:  err_index = p - beg; return 0x40; // incorrect char
        }
        fp = 0x3F & ((fp << 2) | code);
    }

    return fp;
}


const char* CNWAlignerMrna2Dna::x_FindFingerPrint64(
    const char* beg, const char* end,
    unsigned char fingerprint, size_t size,
    size_t& err_index)
{

    if(beg + size > end) {
        err_index = 0;
        return 0;
    }

    const char* p0 = beg;

    size_t err_idx = 0; --p0;
    unsigned char fp = 0x40;
    while(fp == 0x40 && p0 < end) {
        p0 += err_idx + 1;
        fp = x_CalcFingerPrint64(p0, p0 + size, err_idx);
    }

    if(p0 >= end) {
        return end;  // not found
    }
    
    unsigned char code;
    while(fp != fingerprint && ++p0 < end) {

        switch(*(p0 + size - 1)) {
        case 'A': code = 0;    break;
        case 'G': code = 0x01; break;
        case 'T': code = 0x02; break;
        case 'C': code = 0x03; break;
        default:  err_index = p0 + size - 1 - beg;
                  return 0;
        }
        
        fp = 0x3F & ((fp << 2) | code );
    }

    return p0;
}

struct nwaln_mrnaseg {
    nwaln_mrnaseg(size_t i1, size_t i2, unsigned char fp0):
        a(i1), b(i2), fp(fp0) {}
    size_t a, b;
    unsigned char fp;
};

struct nwaln_mrnaguide {
    nwaln_mrnaguide(size_t i1, size_t i2, size_t i3, size_t i4):
        q0(i1), q1(i2), s0(i3), s1(i4) {}
    size_t q0, q1, s0, s1;
};

size_t CNWAlignerMrna2Dna::MakeGuides(const size_t guide_size)
{
    vector<nwaln_mrnaseg> segs;

    size_t err_idx;
    for(size_t i = 0; i + guide_size <= m_SeqLen1; ) {
        const char* beg = m_Seq1 + i;
        const char* end = m_Seq1 + i + guide_size;
        unsigned char fp = x_CalcFingerPrint64(beg, end, err_idx);
        if(fp != 0x40) {
            segs.push_back(nwaln_mrnaseg(i, i + guide_size - 1, fp));
            i += guide_size;
        }
        else {
            i += err_idx + 1;
        }
    }

    vector<nwaln_mrnaguide> guides;
    size_t idx = 0;
    const char* beg = m_Seq2 + idx;
    const char* end = m_Seq2 + m_SeqLen2;
    for(size_t i = 0, seg_count = segs.size();
        beg + guide_size <= end && i < seg_count; ++i) {

        const char* p = 0;
        const char* beg0 = beg;
        while( p == 0 && beg + guide_size <= end ) {

            p = x_FindFingerPrint64( beg, end, segs[i].fp,
                                     guide_size, err_idx );
            if(p == 0) { // incorrect char
                beg += err_idx + 1; 
            }
            else if (p < end) {// fingerprints match but check actual sequences
                const char* seq1 = m_Seq1 + segs[i].a;
                const char* seq2 = p;
                size_t k;
                for(k = 0; k < guide_size; ++k) {
                    if(seq1[k] != seq2[k]) break;
                }
                if(k == guide_size) { // real match
                    size_t i1 = segs[i].a;
                    size_t i2 = segs[i].b;
                    size_t i3 = seq2 - m_Seq2;
                    size_t i4 = i3 + guide_size - 1;
                    size_t guides_dim = guides.size();
                    if( guides_dim == 0 ||
                        i1 - 1 > guides[guides_dim - 1].q1 ||
                        i3 - 1 > guides[guides_dim - 1].s1    ) {
                        guides.push_back(nwaln_mrnaguide(i1, i2, i3, i4));
                    }
                    else { // expand the last guide
                        guides[guides_dim - 1].q1 = i2;
                        guides[guides_dim - 1].s1 = i4;
                    }
                    beg0 = p + guide_size;
                }
                else {  // spurious match
                    beg = p + 1;
                    p = 0;
                }
            }
        }
        beg = beg0; // restore start pos in genomic sequence
    }

    // initialize m_guides
    size_t guides_dim = guides.size();
    m_guides.clear();
    m_guides.resize(4*guides_dim);
    for(size_t k = 0; k < guides_dim; ++k) {
        size_t q0 = (guides[k].q0 + guides[k].q1) / 2;
        size_t s0 = (guides[k].s0 + guides[k].s1) / 2;
        m_guides[4*k]         = q0 - 5;
        m_guides[4*k + 1]     = q0 + 5;
        m_guides[4*k + 2]     = s0 - 5;
        m_guides[4*k + 3]     = s0 + 5;
    }
 
    return m_guides.size();   
}


void CNWAlignerMrna2Dna::FormatAsText(string* output, 
                              EFormat type, size_t line_width) const
    throw(CAlgoAlignException)

{
    CNcbiOstrstream ss;
    ss.precision(3);
    switch (type) {
            
    case eFormatExonTable: {
        const char* p1 = m_Seq1;
        const char* p2 = m_Seq2;
        int tr_idx_hi0 = m_Transcript.size() - 1, tr_idx_hi = tr_idx_hi0;
        int tr_idx_lo0 = 0, tr_idx_lo = tr_idx_lo0;

        if(m_esf_L1 && m_Transcript[tr_idx_hi0] == eInsert) {
            while(m_esf_L1 && m_Transcript[tr_idx_hi] == eInsert) {
                --tr_idx_hi;
                ++p2;
            }
        }

        if(m_esf_L2 && m_Transcript[tr_idx_hi0] == eDelete) {
            while(m_esf_L2 && m_Transcript[tr_idx_hi] == eDelete) {
                --tr_idx_hi;
                ++p1;
            }
        }

        if(m_esf_R1 && m_Transcript[tr_idx_lo0] == eInsert) {
            while(m_esf_R1 && m_Transcript[tr_idx_lo] == eInsert) {
                ++tr_idx_lo;
            }
        }

        if(m_esf_R2 && m_Transcript[tr_idx_lo0] == eDelete) {
            while(m_esf_R2 && m_Transcript[tr_idx_lo] == eDelete) {
                ++tr_idx_lo;
            }
        }

        bool last_intron_generic = false, generic_intron = false;

        for(int tr_idx = tr_idx_hi; tr_idx >= tr_idx_lo;) {
            const char* p1_beg = p1;
            const char* p2_beg = p2;
            size_t matches = 0, exon_size = 0;

            while(tr_idx >= tr_idx_lo && 
                  m_Transcript[tr_idx] < eIntron_GT_AG) {
                
                bool noins = m_Transcript[tr_idx] != eInsert;
                bool nodel = m_Transcript[tr_idx] != eDelete;
                if(noins && nodel) {
                    if(*p1++ == *p2++) ++matches;
                } else if( noins ) {
                    ++p1;
                } else {
                    ++p2;
                }
                --tr_idx;
                ++exon_size;
            }

            if(exon_size > 0) {

                generic_intron = m_Transcript[tr_idx] == eIntron_Generic;
                if(m_Seq1Id.size() && m_Seq2Id.size()) {
                    ss << m_Seq1Id << '\t' << m_Seq2Id << '\t';
                }
                float identity = float(matches) / exon_size;
                ss << identity << '\t' << exon_size << '\t';
                size_t beg1  = p1_beg - m_Seq1, end1 = p1 - m_Seq1 - 1;
                size_t beg2  = p2_beg - m_Seq2, end2 = p2 - m_Seq2 - 1;
                ss << beg1 << '\t'<< end1<< '\t'<< beg2 << '\t'<< end2<< '\t';
                char c1 = (p2_beg >= m_Seq2 + 2)? *(p2_beg - 2): ' ';
                char c2 = (p2_beg >= m_Seq2 + 1)? *(p2_beg - 1): ' ';
                char c3 = (p2 < m_Seq2 + m_SeqLen2)? *(p2): ' ';
                char c4 = (p2 < m_Seq2 + m_SeqLen2 - 1)? *(p2+1): ' ';
                ss << c1 << c2;
                ss << (last_intron_generic? '(': '<') << "exon";
                ss << (generic_intron? ')': '>');
                ss << c3 << c4 << '\t';
                ss << endl;
                last_intron_generic = generic_intron;
            }
            // find next exon
            while(tr_idx >= tr_idx_lo &&
                  (m_Transcript[tr_idx] >= eIntron_GT_AG)) {

                --tr_idx;
                ++p2;
            }
        }
    }
    break;
    
    default: {
        CNWAligner::FormatAsText(output, type, line_width);
        return;
    }
    }
    
    *output = CNcbiOstrstreamToString(ss);
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.21  2003/07/09 15:11:23  kapustin
 * Update plain text output with seq ids and use inequalities instead of binary operation to verify intron boundaries
 *
 * Revision 1.20  2003/06/17 17:20:44  kapustin
 * CNWAlignerException -> CAlgoAlignException
 *
 * Revision 1.19  2003/06/17 14:51:04  dicuccio
 * Fixed after algo/ rearragnement
 *
 * Revision 1.18  2003/06/02 14:04:49  kapustin
 * Progress indication-related updates
 *
 * Revision 1.17  2003/05/23 18:27:57  kapustin
 * Use weak comparisons in core recurrences. Adjust for new transcript identifiers. Introduce new (generic) splice type.
 *
 * Revision 1.16  2003/04/30 18:12:42  kapustin
 * Account for second sequence's esf when formatting output exon table
 *
 * Revision 1.15  2003/04/30 16:06:17  kapustin
 * Fix error in guide generation routine
 *
 * Revision 1.14  2003/04/14 19:00:55  kapustin
 * Add guide creation facility.  x_Run() -> x_Align()
 *
 * Revision 1.13  2003/04/10 19:15:16  kapustin
 * Introduce guiding hits approach
 *
 * Revision 1.12  2003/04/02 20:53:21  kapustin
 * Add exon table output format
 *
 * Revision 1.11  2003/03/31 15:32:05  kapustin
 * Calculate score independently from transcript
 *
 * Revision 1.10  2003/03/25 22:06:01  kapustin
 * Support non-canonical splice signals
 *
 * Revision 1.9  2003/03/18 15:15:51  kapustin
 * Implement virtual memory checking function. Allow separate free end
 * gap specification
 *
 * Revision 1.8  2003/03/17 13:42:24  kapustin
 * Make donor/acceptor characters uppercase
 *
 * Revision 1.7  2003/02/11 16:06:54  kapustin
 * Add end-space free alignment support
 *
 * Revision 1.6  2003/01/30 20:32:06  kapustin
 * Call x_LoadScoringMatrix() from the base class constructor.
 *
 * Revision 1.5  2003/01/21 12:41:38  kapustin
 * Use class neg infinity constant to specify least possible score
 *
 * Revision 1.4  2003/01/08 15:42:59  kapustin
 * Fix initialization for the first column of the backtrace matrix
 *
 * Revision 1.3  2002/12/24 18:29:06  kapustin
 * Remove sequence size verification since a part of Dna could be submitted
 *
 * Revision 1.2  2002/12/17 21:50:05  kapustin
 * Remove unnecesary seq type parameter from the constructor
 *
 * Revision 1.1  2002/12/12 17:57:41  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
