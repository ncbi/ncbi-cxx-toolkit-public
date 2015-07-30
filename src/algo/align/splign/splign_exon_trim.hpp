#ifndef ALGO_ALIGN_SPLIGN_EXON_TRIM_HPP
#define ALGO_ALIGN_SPLIGN_EXON_TRIM_HPP

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
* Author: Boris Kiyutin
*
* File Description: exon trimming (alignment post processing step)
*                   
* ===========================================================================
*/


#include <algo/align/nw/nw_formatter.hpp>
#include <algo/align/nw/nw_spliced_aligner.hpp>
#include <algo/align/splign/splign.hpp>

BEGIN_NCBI_SCOPE

//trimming of spliced alignments
class CSplignTrim {
public:

    typedef CNWFormatter::SSegment TSeg;

    CSplignTrim(const char *seq, int seqlen, CConstRef<CSplicedAligner> aligner, double max_part_exon_drop) : m_seq(seq), m_seqlen(seqlen), m_aligner(aligner), m_MaxPartExonIdentDrop(max_part_exon_drop)
    {
    }

    //legacy check
    //if two short throws away and reterns true
    //otherwise returns false   
    bool ThrowAwayShortExon(TSeg& s);

    bool ThrowAway20_28_90(TSeg& s);

    //cut len bases from left
    //len is length on alignment to cut
    void CutFromLeft(size_t len, TSeg& s);

    void CutToMatchLeft(TSeg& s);
    void Cut50FromLeft(TSeg& s);
    void ImproveFromLeft(TSeg& s);

    //cut len bases from right
    //len is length on alignment to cut
    void CutFromRight(size_t len, TSeg& s);

    void CutToMatchRight(TSeg& s);
    void Cut50FromRight(TSeg& s);
    void ImproveFromRight(TSeg& s);

    void AdjustGaps(vector<TSeg>& segments);

private:
    const char *m_seq;//genomic sequence
    const int m_seqlen;
    CConstRef<CSplicedAligner> m_aligner;
    const double m_MaxPartExonIdentDrop; 
};

END_NCBI_SCOPE

#endif
