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
* File Description:  Base class for spliced aligners.
*                   
* ===========================================================================
*
*/


#include <ncbi_pch.hpp>
#include "messages.hpp"
#include <algo/align/nw/nw_spliced_aligner.hpp>
#include <algo/align/nw/align_exception.hpp>

BEGIN_NCBI_SCOPE


CSplicedAligner::CSplicedAligner():
    m_IntronMinSize(GetDefaultIntronMinSize())
{
    SetEndSpaceFree(true, true, false, false);
}
    

CSplicedAligner::CSplicedAligner(const char* seq1, size_t len1,
                                 const char* seq2, size_t len2)
    : CNWAligner(seq1, len1, seq2, len2),
      m_IntronMinSize(GetDefaultIntronMinSize())
{
    SetEndSpaceFree(true, true, false, false);
}


CSplicedAligner::CSplicedAligner(const string& seq1, const string& seq2)
    : CNWAligner(seq1, seq2),
      m_IntronMinSize(GetDefaultIntronMinSize())
{
    SetEndSpaceFree(true, true, false, false);
}


void CSplicedAligner::SetWi  (unsigned char splice_type, TScore value)
{
    if(splice_type < GetSpliceTypeCount()) {
        x_GetSpliceScores()[splice_type]  = value;
    }
    else 
    {
        NCBI_THROW(CAlgoAlignException,
                   eInvalidSpliceTypeIndex,
                   g_msg_InvalidSpliceTypeIndex);
    }
}


CSplicedAligner::TScore CSplicedAligner::GetWi  (unsigned char splice_type)
{
    if(splice_type < GetSpliceTypeCount()) {
        return x_GetSpliceScores()[splice_type];
    }
    else 
    {
        NCBI_THROW(CAlgoAlignException,
                   eInvalidSpliceTypeIndex,
                   g_msg_InvalidSpliceTypeIndex);
    }
}


unsigned char CSplicedAligner::x_CalcFingerPrint64(
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


const char* CSplicedAligner::x_FindFingerPrint64(
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

size_t CSplicedAligner::MakePattern(const size_t guide_size)
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
    const size_t offs = 10;
    for(size_t k = 0; k < guides_dim; ++k) {
        size_t q0 = (guides[k].q0 + guides[k].q1) / 2;
        size_t s0 = (guides[k].s0 + guides[k].s1) / 2;
        m_guides[4*k]         = q0 - offs;
        m_guides[4*k + 1]     = q0 + offs;
        m_guides[4*k + 2]     = s0 - offs;
        m_guides[4*k + 3]     = s0 + offs;
    }
 
    return m_guides.size();   
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/03/16 15:48:26  jcherry
 * Allow use of std::string for specifying sequences
 *
 * Revision 1.10  2004/12/16 22:42:22  kapustin
 * Move to algo/align/nw
 *
 * Revision 1.9  2004/11/29 14:37:15  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two additional parameters to specify starting coordinates.
 *
 * Revision 1.8  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.7  2004/05/17 14:50:56  kapustin
 * Add/remove/rearrange some includes and object declarations
 *
 * Revision 1.6  2004/04/23 14:39:47  kapustin
 * Add Splign library and other changes
 *
 * Revision 1.4  2003/10/27 21:00:17  kapustin
 * Set intron penalty defaults differently for 16- and 32-bit versions according to the expected quality of sequences those variants are supposed to be used with.
 *
 * Revision 1.3  2003/09/30 19:50:04  kapustin
 * Make use of standard score matrix interface
 *
 * Revision 1.2  2003/09/26 14:43:18  kapustin
 * Remove exception specifications
 *
 * Revision 1.1  2003/09/02 22:34:49  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
