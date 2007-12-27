#ifndef ALGO_GNOMON___SCORE__HPP
#define ALGO_GNOMON___SCORE__HPP

/*  $Id$
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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>

#include "gnomon_seq.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

class CTerminal;
class CCodingRegion;
class CNonCodingRegion;

class CSeqScores {
public:
    CSeqScores (const CTerminal& a, const CTerminal& d,const  CTerminal& stt, const CTerminal& stp, 
                const CCodingRegion& cr, const CNonCodingRegion& ncr, const CNonCodingRegion& ing, 
                TSignedSeqPos from, TSignedSeqPos to, const TAlignList& cls, 
                const TFrameShifts& initial_fshifts, double mpp, const CGnomonEngine& gnomon);
    void Init(CResidueVec& original_sequence, bool repeats, bool leftwall, 
              bool rightwall, double consensuspenalty);
        
    TSignedSeqPos From() const { return m_chunk_start; }
    TSignedSeqPos To() const { return m_chunk_stop; }
    int AcceptorNumber(int strand) const { return m_anum[strand]; }
    int DonorNumber(int strand) const { return m_dnum[strand]; }
    int StartNumber(int strand) const { return m_sttnum[strand]; }
    int StopNumber(int strand) const { return m_stpnum[strand]; }
    double AcceptorScore(int i, int strand) const { return m_ascr[strand][i]; }
    double DonorScore(int i, int strand) const { return m_dscr[strand][i]; }
    double StartScore(int i, int strand) const { return m_sttscr[strand][i]; }
    double StopScore(int i, int strand) const { return m_stpscr[strand][i]; }
    const CTerminal& Acceptor() const { return m_acceptor; }
    const CTerminal& Donor() const { return m_donor; }
    const CTerminal& Start() const { return m_start; }
    const CTerminal& Stop() const { return m_stop; }
    const TAlignList& Alignments() const { return m_align_list; }
    const TFrameShifts& SeqTFrameShifts() const { return m_fshifts; }
    const CFrameShiftedSeqMap& FrameShiftedSeqMap() const { return m_map; }
    bool StopInside(int a, int b, int strand, int frame) const;
    bool OpenCodingRegion(int a, int b, int strand, int frame) const;
    double CodingScore(int a, int b, int strand, int frame) const;
    int ProtNumber(int a, int b) const { return (m_protnum[b]-m_protnum[a]); }
    double MultiProtPenalty() const { return m_mpp; }
    bool OpenNonCodingRegion(int a, int b, int strand) const { return (a > m_notinintron[strand][b]); }
    double NonCodingScore(int a, int b, int strand) const;
    bool OpenIntergenicRegion(int a, int b) const;
    int LeftAlignmentBoundary(int b) const { return m_inalign[b]; }
    double IntergenicScore(int a, int b, int strand) const;
    int SeqLen() const { return (int)m_seq[0].size(); }
    bool SplittedStop(int id, int ia, int strand, int ph) const 
    { return (m_dsplit[strand][ph][id]&m_asplit[strand][ph][ia]) != 0; }
    bool isStart(int i, int strand) const;
    bool isStop(int i, int strand) const;
    bool isReadingFrameLeftEnd(int i, int strand) const;
    bool isReadingFrameRightEnd(int i, int strand) const;
    bool isAG(int i, int strand) const;
    bool isGT(int i, int strand) const;
    bool isConsensusIntron(int i, int j, int strand) const;
    const EResidue* SeqPtr(int i, int strand) const;

private:
    CSeqScores& operator=(const CSeqScores&);
    const CTerminal &m_acceptor, &m_donor, &m_start, &m_stop;
    const CCodingRegion &m_cdr;
    const CNonCodingRegion &m_ncdr, &m_intrg;
    TAlignList m_align_list;
    TFrameShifts m_fshifts;
    CEResidueVec m_seq[2];
    TIVec m_laststop[2][3], m_notinexon[2][3], m_notinintron[2], m_notining;
    CFrameShiftedSeqMap m_map;
    TDVec m_ascr[2], m_dscr[2], m_sttscr[2], m_stpscr[2], m_ncdrscr[2], m_ingscr[2], m_cdrscr[2][3];
    TIVec m_asplit[2][2], m_dsplit[2][2];
    TIVec m_inalign;
    TIVec m_protnum;
    int m_anum[2], m_dnum[2], m_sttnum[2], m_stpnum[2];
    TSignedSeqPos m_chunk_start, m_chunk_stop;
    double m_mpp;

    CResidueVec ConstructSequenceAndMaps(const TAlignList& aligns, const CResidueVec& original_sequence);
};

END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // ALGO_GNOMON___SCORE__HPP
