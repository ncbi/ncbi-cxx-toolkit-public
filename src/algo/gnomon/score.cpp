/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software / database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software / database is freely available
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

#include <ncbi_pch.hpp>
#include <algo/gnomon/gnomon.hpp>
#include <algo/gnomon/gnomon_exception.hpp>
#include "score.hpp"
#include "hmm.hpp"
#include "hmm_inlines.hpp"
#include "gnomon_engine.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

bool CSeqScores::isStart(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    if(ii < 0 || ii+2 >= SeqLen()) return false;  //out of range
    else if(ss[ii] != enA || ss[ii+1] != enT || ss[ii+2] != enG) return false;
    else return true;
}

bool CSeqScores::isStop(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    if(ii < 0 || ii+2 >= SeqLen()) return false;  //out of range
    if((ss[ii] != enT || ss[ii+1] != enA || ss[ii+2] != enA) &&
        (ss[ii] != enT || ss[ii+1] != enA || ss[ii+2] != enG) &&
        (ss[ii] != enT || ss[ii+1] != enG || ss[ii+2] != enA)) return false;
    else return true;
}

bool CSeqScores::isAG(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    if(ii-1 < 0 || ii >= SeqLen()) return false;  //out of range
    if(ss[ii-1] != enA || ss[ii] != enG) return false;
    else return true;
}

bool CSeqScores::isGT(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    if(ii < 0 || ii+1 >= SeqLen()) return false;  //out of range
    if(ss[ii] != enG || ss[ii+1] != enT) return false;
    else return true;
}

bool CSeqScores::isConsensusIntron(int i, int j, int strand) const
{
    if(strand == ePlus) return (m_dscr[ePlus][i-1] != BadScore()) && (m_ascr[ePlus][j] != BadScore());
    else                return (m_ascr[eMinus][i-1] != BadScore()) && (m_dscr[eMinus][j] != BadScore());
//    if(strand == ePlus) return isGT(i,strand) && isAG(j,strand);
//    else return isAG(i,strand) && isGT(j,strand);
}

const EResidue* CSeqScores::SeqPtr(int i, int strand) const
{
    const CEResidueVec& ss = m_seq[strand];
    int ii = (strand == ePlus) ? i : SeqLen()-1-i;
    return &ss.front()+ii;
}

bool CSeqScores::StopInside(int a, int b, int strand, int frame) const
{
    return (a <= m_laststop[strand][frame][b]);
}

bool CSeqScores::OpenCodingRegion(int a, int b, int strand, int frame) const
{
    return (a > m_notinexon[strand][frame][b]);
}

double CSeqScores::CodingScore(int a, int b, int strand, int frame) const
{
    if(a > b) return 0; // for splitted start/stop
    double score = m_cdrscr[strand][frame][b];
    if(a > 0) score -= m_cdrscr[strand][frame][a-1];
    return score;
}

double CSeqScores::NonCodingScore(int a, int b, int strand) const
{
    double score = m_ncdrscr[strand][b];
    if(a > 0) score -= m_ncdrscr[strand][a-1];
    return score;
}

bool CSeqScores::OpenIntergenicRegion(int a, int b) const
{
    return (a > m_notining[b]);
} 

double CSeqScores::IntergenicScore(int a, int b, int strand) const
{
    double score = m_ingscr[strand][b];
    if(a > 0) score -= m_ingscr[strand][a-1];
    return score;
}

int CSeqScores::SeqMap(int i, EMove move, int* dellenp) const 
{
    if(dellenp != 0) *dellenp = 0;
    int l = m_seq_map[i];
    if(l >= 0) return l;
    
    if(i == 0)
    {
         NCBI_THROW(CGnomonException, eGenericError, "First base in sequence can not be a deletion");
    }
    if(i == (int)m_seq_map.size()-1)
    {
         NCBI_THROW(CGnomonException, eGenericError, "Last base in sequence can not be a deletion");
    }
    
    switch(move)
    {
        case eMoveLeft:
        {
            int dl = 0;
            while(i-dl > 0 && m_seq_map[i-dl] < 0) ++dl;
            if(dellenp != 0) *dellenp = dl;
            return m_seq_map[i-dl];
        }
        case eMoveRight:
        {
            int dl = 0;
            while(i+dl < (int)m_seq_map.size()-1 && m_seq_map[i+dl] < 0) ++dl;
            if(dellenp != 0) *dellenp = dl;
            return m_seq_map[i+dl];
        }
        default:  return l;
    }
}

TSignedSeqPos CSeqScores::RevSeqMap(TSignedSeqPos i) const 
{
    int l = m_rev_seq_map[i-From()];
    if(l >= 0) 
    {
        return l;
    }
    else
    {
         NCBI_THROW(CGnomonException, eGenericError, "Mapping of insertion");
    }
}

double AddScores(double scr1, double scr2)
{
    if(scr1 == BadScore() || scr2 == BadScore()) return BadScore();
    else return scr1+scr2;
}

struct SAlignOrder
{
    bool operator() (const CAlignVec& a, const CAlignVec& b) const
    {
        if(a.CdsLimits().NotEmpty()) return a.CdsLimits().GetFrom() < b.CdsLimits().GetFrom();
        else return a.Limits().GetFrom() < b.Limits().GetFrom();
    }
};

CSeqScores::CSeqScores (CTerminal& a, CTerminal& d, CTerminal& stt, CTerminal& stp, 
CCodingRegion& cr, CNonCodingRegion& ncr, CNonCodingRegion& ing, CResidueVec& original_sequence, 
TSignedSeqPos from, TSignedSeqPos to, const TAlignList& cls, const TFrameShifts& initial_fshifts, bool repeats, bool leftwall, 
bool rightwall, string cntg, double mpp, double consensuspenalty)
: m_acceptor(a), m_donor(d), m_start(stt), m_stop(stp), m_cdr(cr), m_ncdr(ncr), m_intrg(ing), 
  m_align_list(cls), m_fshifts(initial_fshifts), m_chunk_start(from), m_chunk_stop(to), m_contig(cntg), m_mpp(mpp)
{
    
    for(TAlignListConstIt it = cls.begin(); it != cls.end(); ++it)
    {
        const CAlignVec& align = *it;
        if (!Include(TSignedSeqRange(from,to),align.Limits())) continue;
        m_fshifts.insert(m_fshifts.end(),align.FrameShifts().begin(),align.FrameShifts().end());
    }
    sort(m_fshifts.begin(),m_fshifts.end());
    TFrameShifts::iterator fsi = m_fshifts.begin();
    while(fsi != m_fshifts.end() && fsi->Loc() < from) ++fsi;
    
    CResidueVec sequence;
    m_rev_seq_map.resize(to-from+1);
    for(TSignedSeqPos i = from; i <= to; ++i)
    {
        if(fsi != m_fshifts.end() && fsi->IsInsertion() && i == fsi->Loc())
        {
            for(TSignedSeqPos k = i; k < fsi->Loc()+fsi->Len(); ++k)  m_rev_seq_map[k-m_chunk_start] = -1;
            i = fsi->Loc()+fsi->Len()-1;
            ++fsi;
        }
        else if(fsi != m_fshifts.end() && fsi->IsDeletion() && i == fsi->Loc())
        {
            for(int k = 0; k < fsi->Len(); ++k)
            {
                sequence.push_back(fsi->DeletedValue()[k]);
                m_seq_map.push_back(-1);
            }

            --i;          // this position will be reconsidered next time (it still could be an insertion)
            ++fsi;
        }
        else
        {
            sequence.push_back(original_sequence[i]);
            m_rev_seq_map[i-m_chunk_start] = (int)m_seq_map.size();
            m_seq_map.push_back(i);
        }
    }

    TSignedSeqPos len = m_seq_map.size();
    
    try
    {
        m_notining.resize(len,-1);
        m_inalign.resize(len,numeric_limits<int>::max());
        m_protnum.resize(len,0);

        for(int strand = 0; strand < 2; ++strand)
        {
            m_seq[strand].resize(len);
            m_ascr[strand].resize(len,BadScore());
            m_dscr[strand].resize(len,BadScore());
            m_sttscr[strand].resize(len,BadScore());
            m_stpscr[strand].resize(len,BadScore());
            m_ncdrscr[strand].resize(len,BadScore());
            m_ingscr[strand].resize(len,BadScore());
            m_notinintron[strand].resize(len,-1);
            for(int frame = 0; frame < 3; ++frame)
            {
                m_cdrscr[strand][frame].resize(len,BadScore());
                m_laststop[strand][frame].resize(len,-1);
                m_notinexon[strand][frame].resize(len,-1);
            }
            for(int ph = 0; ph < 2; ++ph)
            {
                m_asplit[strand][ph].resize(len,0);
                m_dsplit[strand][ph].resize(len,0);
            }
        }
    }
    catch(bad_alloc)
    {
        NCBI_THROW(CGnomonException, eMemoryLimit, "Not enough memory in CSeqScores");
    } 

    for(TSignedSeqPos i = 0; i < len; ++i)
    {
        char c = sequence[i];
        if(repeats && c != toupper((unsigned char) c))
        {
            m_laststop[ePlus][0][i] = i;
            m_laststop[ePlus][1][i] = i;
            m_laststop[ePlus][2][i] = i;
            m_laststop[eMinus][0][i] = i;
            m_laststop[eMinus][1][i] = i;
            m_laststop[eMinus][2][i] = i;
        }

        EResidue l = fromACGT(c);
        m_seq[ePlus][i] = l;
        m_seq[eMinus][len-1-i] = k_toMinus[l];
    }
    
    set<CAlignVec,SAlignOrder> allaligns; 
    for(TAlignListConstIt it = cls.begin(); it != cls.end(); ++it)
    {
        const CAlignVec& origalign = *it;
        if(origalign.Limits().GetFrom() < from || origalign.Limits().GetTo() > to) continue;

        TSignedSeqRange limits = origalign.Limits();
        if(origalign.MaxCdsLimits().NotEmpty())   // 7 for splitted start/stop evaluation
        {
            limits.SetFrom( max(limits.GetFrom(),origalign.MaxCdsLimits().GetFrom()-7) );
            limits.SetTo( min(limits.GetTo(),origalign.MaxCdsLimits().GetTo()+7) );
        }
        int a = -1;
        int aa = limits.GetFrom();
        while(a < 0) a = m_rev_seq_map[(aa--)-From()]; // just in case we stumbled on the insertion because of 7
        int b = -1;
        int bb = limits.GetTo();
        while(b < 0) b = m_rev_seq_map[(bb++)-From()]; // just in case we stumbled on the insertion because of 7
        limits.SetFrom(a );
        limits.SetTo( b );

        if(origalign.Type() == CAlignVec::eWall)
        {
            int a = max(0,int(limits.GetFrom())-CIntergenic::MinIntergenic()+1);
            int b = min(len-1,limits.GetTo()+CIntergenic::MinIntergenic()-1);
            for(int pnt = a; pnt <= b; ++pnt)
            {
                m_notinexon[ePlus][0][pnt] = pnt;
                m_notinexon[ePlus][1][pnt] = pnt;
                m_notinexon[ePlus][2][pnt] = pnt;
                m_notinexon[eMinus][0][pnt] = pnt;
                m_notinexon[eMinus][1][pnt] = pnt;
                m_notinexon[eMinus][2][pnt] = pnt;

                m_notinintron[ePlus][pnt] = pnt;
                m_notinintron[eMinus][pnt] = pnt;
            }
            continue;
        }

        if(origalign.Type() == CAlignVec::eNested)
        {
            int a = max(0,int(limits.GetFrom())-CIntergenic::MinIntergenic()+1);
            int b = min(len-1,limits.GetTo()+CIntergenic::MinIntergenic()-1);
            for(int pnt = a; pnt <= b; ++pnt)
            {
                m_notinexon[ePlus][0][pnt] = pnt;
                m_notinexon[ePlus][1][pnt] = pnt;
                m_notinexon[ePlus][2][pnt] = pnt;
                m_notinexon[eMinus][0][pnt] = pnt;
                m_notinexon[eMinus][1][pnt] = pnt;
                m_notinexon[eMinus][2][pnt] = pnt;
            }
            continue;
        }


        CAlignVec algn(origalign.Strand(),origalign.ID(),origalign.Type());
        algn.SetConfirmedStart(origalign.ConfirmedStart());

        for(unsigned int k = 0; k < origalign.size(); ++k)
        {
            TSignedSeqPos a = RevSeqMap(origalign[k].GetFrom());
            while(a > 0 && m_seq_map[a-1] < 0) --a;
            TSignedSeqPos b = RevSeqMap(origalign[k].GetTo());
            while(b < len-1 && m_seq_map[b+1] < 0) ++b;
            a = max(limits.GetFrom(),a);
            b = min(limits.GetTo(),b);
            if(a > b) continue;

            algn.Insert(CAlignExon(a,b,origalign[k].m_fsplice,origalign[k].m_ssplice));

            for(TSignedSeqPos i = a; i <= b; ++i)   // ignoring repeats in alignmnets
            {
                m_laststop[ePlus][0][i] = -1;
                m_laststop[ePlus][1][i] = -1;
                m_laststop[ePlus][2][i] = -1;
                m_laststop[eMinus][0][i] = -1;
                m_laststop[eMinus][1][i] = -1;
                m_laststop[eMinus][2][i] = -1;
            }
        }
        algn.front().m_fsplice = false;
        algn.back().m_ssplice = false;

        if(origalign.CdsLimits().NotEmpty())
        {
            TSignedSeqPos a = RevSeqMap(origalign.CdsLimits().GetFrom());
            while(a > origalign.Limits().GetFrom())
            {
                TSignedSeqPos aprev = a-1;
                for(unsigned int k = 1; k < origalign.size(); ++k)
                {
                    if(origalign[k].GetFrom() == a) aprev = origalign[k-1].GetTo();
                    else if(origalign[k].GetFrom() > a) break;
                }
                if(m_seq_map[aprev] < 0) a = aprev;
                else break;
            }
            
            TSignedSeqPos b = RevSeqMap(origalign.CdsLimits().GetTo());
            while(b < origalign.Limits().GetTo())
            {
                TSignedSeqPos bnext = b+1;
                for(unsigned int k = 0; k < origalign.size()-1; ++k)
                {
                    if(origalign[k].GetTo() == b) bnext = origalign[k+1].GetFrom();
                    else if(origalign[k].GetTo() > b) break;
                }
                if(m_seq_map[bnext] < 0) b = bnext;
                else break;
            }
            
            algn.SetCdsLimits(TSignedSeqRange(a,b));
        }
        if(origalign.MaxCdsLimits().NotEmpty())
        {
            algn.SetMaxCdsLimits(TSignedSeqRange(RevSeqMap(origalign.MaxCdsLimits().GetFrom()),RevSeqMap(origalign.MaxCdsLimits().GetTo())));
        }

        allaligns.insert(algn);
        
        if(algn.Type() == CAlignVec::eProt) m_protnum[algn.CdsLimits().GetFrom()] = 1;

        if(algn.CdsLimits().NotEmpty())   // enforcing frames of known CDSes and protein pieces
        {
            EStrand strand = algn.Strand();
            TSignedSeqPos right = algn.CdsLimits().GetTo();
            int ff = (strand == ePlus) ? 2-right%3 : right%3;               // see cdrscr
            for(int frame = 0; frame < 3; ++frame)
            {
                if(frame != ff) m_notinexon[strand][frame][right] = right;
            }

            for(unsigned int i = 1; i < algn.size(); ++i)
            {
                right = algn[i-1].GetTo();
                ff = (strand == ePlus) ? 2-right%3 : right%3;
                for(int frame = 0; frame < 3; ++frame)
                {
                    if(frame != ff && !algn[i-1].m_ssplice) m_notinexon[strand][frame][right] = right;
                }
            }
        }

        if(algn.ConfirmedStart()) {
            int pnt;
            if(algn.Strand() == ePlus) {
                pnt = algn.CdsLimits().GetFrom()-1;
            } else {
                pnt = algn.CdsLimits().GetTo()+1;
            }
            m_notinexon[ePlus][0][pnt] = pnt;
            m_notinexon[ePlus][1][pnt] = pnt;
            m_notinexon[ePlus][2][pnt] = pnt;
            m_notinexon[eMinus][0][pnt] = pnt;
            m_notinexon[eMinus][1][pnt] = pnt;
            m_notinexon[eMinus][2][pnt] = pnt;
            
            m_notinintron[ePlus][pnt] = pnt;
            m_notinintron[eMinus][pnt] = pnt;
        }

        if(it->MaxCdsLimits().NotEmpty())
        {
            if(it->MaxCdsLimits().GetFrom() > it->Limits().GetFrom())
            {
                m_notinexon[ePlus][0][limits.GetFrom()] = limits.GetFrom();
                m_notinexon[ePlus][1][limits.GetFrom()] = limits.GetFrom();
                m_notinexon[ePlus][2][limits.GetFrom()] = limits.GetFrom();
                m_notinintron[ePlus][limits.GetFrom()] = limits.GetFrom();
                m_notinexon[eMinus][0][limits.GetFrom()] = limits.GetFrom();
                m_notinexon[eMinus][1][limits.GetFrom()] = limits.GetFrom();
                m_notinexon[eMinus][2][limits.GetFrom()] = limits.GetFrom();
                m_notinintron[eMinus][limits.GetFrom()] = limits.GetFrom();
            }
            if(it->MaxCdsLimits().GetTo() < it->Limits().GetTo())
            {
                m_notinexon[ePlus][0][limits.GetTo()] = limits.GetTo();
                m_notinexon[ePlus][1][limits.GetTo()] = limits.GetTo();
                m_notinexon[ePlus][2][limits.GetTo()] = limits.GetTo();
                m_notinintron[ePlus][limits.GetTo()] = limits.GetTo();
                m_notinexon[eMinus][0][limits.GetTo()] = limits.GetTo();
                m_notinexon[eMinus][1][limits.GetTo()] = limits.GetTo();
                m_notinexon[eMinus][2][limits.GetTo()] = limits.GetTo();
                m_notinintron[eMinus][limits.GetTo()] = limits.GetTo();
            }
        }
    }
    
    for(TSignedSeqPos i = 1; i < len; ++i)
    {
        m_protnum[i] += m_protnum[i-1];
    }
    
    if(!allaligns.empty())
    {
        set<CAlignVec,SAlignOrder>::iterator it_prev = allaligns.begin();
        
        for(set<CAlignVec,SAlignOrder>::iterator it = ++allaligns.begin(); it != allaligns.end(); it_prev = it, ++it)
        {
            const CAlignVec& algn_prev(*it_prev);
            const CAlignVec& algn(*it);
            
            if(algn.Limits().IntersectingWith(algn_prev.Limits()))
            {
                CNcbiOstrstream ost;
                ost << "MaxCDS of alignment " << algn.ID() << " intersects MaxCDS of alignment " << algn_prev.ID();
                NCBI_THROW(CGnomonException, eGenericError, CNcbiOstrstreamToString(ost));
            }
        }
    }
    
    for(set<CAlignVec,SAlignOrder>::iterator it = allaligns.begin(); it != allaligns.end(); ++it)
    {
        const CAlignVec& algn(*it);
        int strand = algn.Strand();
        int otherstrand = (strand == ePlus) ? eMinus : ePlus;
                
        for(unsigned int k = 1; k < algn.size(); ++k)  // accept NONCONSENSUS alignment splices
        {
            if(algn[k-1].m_ssplice)
            {
                int i = algn[k-1].GetTo();
                if(strand == ePlus) m_dscr[strand][i] = 0;  // donor score on the last base of exon
                else m_ascr[strand][i] = 0;                 // acceptor score on the last base of exon
            }
            if(algn[k].m_fsplice)
            {
                int i = algn[k].GetFrom();
                if(strand == ePlus) m_ascr[strand][i-1] = 0; // acceptor score on the last base of intron
                else m_dscr[strand][i-1] = 0;                // donor score on the last base of intron 
            }
        }
            
        for(unsigned int k = 1; k < algn.size(); ++k)
        {
            int introna = algn[k-1].GetTo()+1;
            int intronb = algn[k].GetFrom()-1;
            
            if(algn[k-1].m_ssplice)
            {
                int pnt = introna;
                m_notinexon[strand][0][pnt] = pnt;
                m_notinexon[strand][1][pnt] = pnt;
                m_notinexon[strand][2][pnt] = pnt;
            }
            
            if(algn[k].m_fsplice)
            {
                int pnt = intronb;
                m_notinexon[strand][0][pnt] = pnt;
                m_notinexon[strand][1][pnt] = pnt;
                m_notinexon[strand][2][pnt] = pnt;
            }
                    
            if(algn[k-1].m_ssplice && algn[k].m_fsplice)
            {            
                for(int pnt = introna; pnt <= intronb; ++pnt)
                {
                    m_notinexon[strand][0][pnt] = pnt;
                    m_notinexon[strand][1][pnt] = pnt;
                    m_notinexon[strand][2][pnt] = pnt;
                }
            }
        }

        for(unsigned int k = 0; k < algn.size(); ++k)
        {
            int exona = algn[k].GetFrom();
            int exonb = algn[k].GetTo();
            for(int pnt = exona; pnt <= exonb; ++pnt)
            {
                m_notinintron[strand][pnt] = pnt;
            }
        }
        
        TSignedSeqPos a = algn.Limits().GetFrom();
        TSignedSeqPos b = algn.Limits().GetTo();
        
        for(TSignedSeqPos i = a; i <= b && i < len-1; ++i)  // first and last points are conserved to deal with intergenics going outside
        {
            m_inalign[i] = max(TSignedSeqPos(1),a);
            m_notinintron[otherstrand][i] = i;
            m_notinexon[otherstrand][0][i] = i;
            m_notinexon[otherstrand][1][i] = i;
            m_notinexon[otherstrand][2][i] = i;
        }

        if(strand == ePlus) a += 3;    // start now is a part of intergenic!!!!
        else b -= 3;
            
        m_notining[b] = a;
        if(algn.CdsLimits().NotEmpty())
        {
            for(TSignedSeqPos i = algn.CdsLimits().GetFrom(); i <= algn.CdsLimits().GetTo(); ++i) m_notining[i] = i;
        }
    }
    
    const int RepeatMargin = 25;
    for(TSignedSeqPos i = 0; i < len-1; ++i)  // repeat trimming
    {
        bool rpta = m_laststop[ePlus][0][i] >= 0;
        bool rptb = m_laststop[ePlus][0][i+1] >= 0;
        
        if(!rpta && rptb)  // b - first repeat
        {
            TSignedSeqPos j = i+1;
            for(; j <= min(i+RepeatMargin,len-1);  ++j) 
            {
                m_laststop[ePlus][0][j] = -1;
                m_laststop[ePlus][1][j] = -1;
                m_laststop[ePlus][2][j] = -1;
                m_laststop[eMinus][0][j] = -1;
                m_laststop[eMinus][1][j] = -1;
                m_laststop[eMinus][2][j] = -1;
            }
            i = j-1;
        }
        else if(rpta && !rptb) // a - last repeat
        {
            TSignedSeqPos j = max(0,int(i)-RepeatMargin+1);
            for(; j <= i; ++j)
            {
                m_laststop[ePlus][0][j] = -1;
                m_laststop[ePlus][1][j] = -1;
                m_laststop[ePlus][2][j] = -1;
                m_laststop[eMinus][0][j] = -1;
                m_laststop[eMinus][1][j] = -1;
                m_laststop[eMinus][2][j] = -1;
            }
        }
    }
    
    const int NLimit = 6;
    CEResidueVec::iterator it = search_n(m_seq[ePlus].begin(),m_seq[ePlus].end(),NLimit,enN);
    while(it != m_seq[ePlus].end())           // masking big chunks of Ns
    {
        for(; it != m_seq[ePlus].end() && *it == enN; ++it) 
        {
            int j = it-m_seq[ePlus].begin();
            m_laststop[ePlus][0][j] = j;
            m_laststop[ePlus][1][j] = j;
            m_laststop[ePlus][2][j] = j;
            m_laststop[eMinus][0][j] = j;
            m_laststop[eMinus][1][j] = j;
            m_laststop[eMinus][2][j] = j;
        }
        it = search_n(it,m_seq[ePlus].end(),NLimit,enN); 
    } 
    

    if(leftwall) m_notinintron[ePlus][0] = m_notinintron[eMinus][0] = 0;
    if(rightwall) m_notinintron[ePlus][len-1] = m_notinintron[eMinus][len-1] = len-1;

    for(int strand = 0; strand < 2; ++strand)
    {
        TIVec& nin = m_notinintron[strand];
        for(TSignedSeqPos i = 1; i < len; ++i) nin[i] = max(nin[i-1],nin[i]);

        for(int frame = 0; frame < 3; ++frame)
        {
            TIVec& nex = m_notinexon[strand][frame];
            for(TSignedSeqPos i = 1; i < len; ++i) nex[i] = max(nex[i-1],nex[i]); 
        }
    }
    for(TSignedSeqPos i = 1; i < len; ++i) m_notining[i] = max(m_notining[i-1],m_notining[i]);

    const int stpT = 1, stpTA = 2, stpTG = 4;

    for(int strand = 0; strand < 2; ++strand)
    {
        CEResidueVec& s = m_seq[strand];
        
        m_anum[strand] = 0;
        m_dnum[strand] = 0;
        m_sttnum[strand] = 0;
        m_stpnum[strand] = 0;

        if(strand == ePlus)
        {
            for(TSignedSeqPos i = 0; i < len; ++i)
            {
                m_ascr[strand][i] = max(m_ascr[strand][i],m_acceptor.Score(s,i));
                if(m_ascr[strand][i] != BadScore())
                {
                    if(s[i+1] == enA && s[i+2] == enA) m_asplit[strand][0][i] |= stpT;
                    if(s[i+1] == enA && s[i+2] == enG) m_asplit[strand][0][i] |= stpT; 
                    if(s[i+1] == enG && s[i+2] == enA) m_asplit[strand][0][i] |= stpT;
                    if(s[i+1] == enA) m_asplit[strand][1][i] |= stpTA|stpTG;
                    if(s[i+1] == enG) m_asplit[strand][1][i] |= stpTA;
                }
                m_dscr[strand][i] = max(m_dscr[strand][i],m_donor.Score(s,i));
                if(m_dscr[strand][i] != BadScore())
                {
                    if(s[i] == enT) m_dsplit[strand][0][i] |= stpT;
                    if(s[i-1] == enT && s[i] == enA) m_dsplit[strand][1][i] |= stpTA;
                    if(s[i-1] == enT && s[i] == enG) m_dsplit[strand][1][i] |= stpTG;
                }
                m_sttscr[strand][i] = m_start.Score(s,i);
                m_stpscr[strand][i] = m_stop.Score(s,i);
                if(m_ascr[strand][i] != BadScore()) ++m_anum[strand];
                if(m_dscr[strand][i] != BadScore()) ++m_dnum[strand];
                if(m_sttscr[strand][i] != BadScore()) ++m_sttnum[strand];
            }
        }
        else
        {
            for(TSignedSeqPos i = 0; i < len; ++i)
            {
                int ii = len-2-i;   // extra -1 because ii is point on the "right"
                m_ascr[strand][i] = max(m_ascr[strand][i],m_acceptor.Score(s,ii));
                if(m_ascr[strand][i] != BadScore())
                {
                    if(s[ii+1] == enA && s[ii+2] == enA) m_asplit[strand][0][i] |= stpT;
                    if(s[ii+1] == enA && s[ii+2] == enG) m_asplit[strand][0][i] |= stpT; 
                    if(s[ii+1] == enG && s[ii+2] == enA) m_asplit[strand][0][i] |= stpT;
                    if(s[ii+1] == enA) m_asplit[strand][1][i] |= stpTA|stpTG;
                    if(s[ii+1] == enG) m_asplit[strand][1][i] |= stpTA;
                }
                m_dscr[strand][i] = max(m_dscr[strand][i],m_donor.Score(s,ii));
                if(m_dscr[strand][i] != BadScore())
                {
                    if(s[ii] == enT) m_dsplit[strand][0][i] |= stpT;
                    if(s[ii-1] == enT && s[ii] == enA) m_dsplit[strand][1][i] |= stpTA;
                    if(s[ii-1] == enT && s[ii] == enG) m_dsplit[strand][1][i] |= stpTG;
                }
                m_sttscr[strand][i] = m_start.Score(s,ii);
                m_stpscr[strand][i] = m_stop.Score(s,ii);
                if(m_ascr[strand][i] != BadScore()) ++m_anum[strand];
                if(m_dscr[strand][i] != BadScore()) ++m_dnum[strand];
            }
        }
    }		

    for(set<CAlignVec,SAlignOrder>::iterator it = allaligns.begin(); it != allaligns.end(); ++it)
    {
        const CAlignVec& algn(*it);
        int strand = algn.Strand();

        CEResidueVec& s = m_seq[ePlus];
        CEResidueVec mRNA;
        mRNA.reserve(algn.AlignLen());
        for(int k = 0; k < (int)algn.size(); ++k) 
        {
            int exona = algn[k].GetFrom();
            int exonb = algn[k].GetTo();
            copy(&s[exona],&s[exonb]+1,back_inserter(mRNA));
        }
                    
        if(strand == ePlus)
        {
            int shft = algn.front().GetFrom();
            for(int k = 0; k < (int)algn.size()-1; ++k) 
            {
                int exonb = algn[k].GetTo();
                        
                if(algn[k].m_ssplice && algn[k+1].m_fsplice)
                {
                    for(int i = max(0,exonb-2); i <= exonb; ++i)    // if i==exonb, the stop is in the next "exon"
                    {
                        m_stpscr[strand][i] = m_stop.Score(mRNA,i-shft);
                    }
                }
                        
                TSignedSeqPos exona = algn[k+1].GetFrom();   //next exon
                shft += exona-exonb-1;         //intron length
                        
                if(algn[k].m_ssplice && algn[k+1].m_fsplice)
                {
                    for(TSignedSeqPos i = exona-1; i <= min(len-1,exona+1); ++i)
                    {
                        m_sttscr[strand][i] = m_start.Score(mRNA,i-shft);
                    }
                }
            }
        }
        else
        {
            reverse(mRNA.begin(),mRNA.end());
            for(int i = 0; i < (int)mRNA.size(); ++i) mRNA[i] = k_toMinus[(int)mRNA[i]];
                    
            int shft = algn.back().GetTo()-1;
            for(int k = (int)algn.size()-1; k > 0; --k) 
            {
                TSignedSeqPos exona = algn[k].GetFrom();
                        
                if(algn[k-1].m_ssplice && algn[k].m_fsplice)
                {
                    for(TSignedSeqPos i = exona-1; i <= min(len-1,exona+1); ++i)
                    {
                        m_stpscr[strand][i] = m_stop.Score(mRNA,shft-i);
                    }
                }
                        
                TSignedSeqPos exonb = algn[k-1].GetTo();  //prev exon
                shft -= exona-exonb-1;         //intron length

                if(algn[k-1].m_ssplice && algn[k].m_fsplice)
                {
                    for(TSignedSeqPos i = max(0,int(exonb)-2); i <= exonb; ++i)
                    {
                        m_sttscr[strand][i] = m_start.Score(mRNA,shft-i);
                    }
                }
            }
        }
    }


    for(set<CAlignVec,SAlignOrder>::iterator it = allaligns.begin(); it != allaligns.end(); ++it)
    {
        const CAlignVec& algn(*it);
        int strand = algn.Strand();
        TSignedSeqRange cds_lim = algn.CdsLimits();
        if(cds_lim.Empty()) continue;
        
        if(strand == ePlus)
        {
            for(unsigned int k = 0; k < algn.size()-1; ++k)   // suppressing splitted STOPS in CDS
            {
                TSignedSeqPos b = algn[k].GetTo();
                if(algn[k].m_ssplice && algn[k+1].m_fsplice && b >= cds_lim.GetFrom() && b < cds_lim.GetTo())
                {
                    m_dsplit[strand][0][b] = 0;
                    m_dsplit[strand][1][b] = 0;
                }
            }
            
            for(unsigned int k = 0; k < algn.size(); ++k)   // suppressing STOPS in CDS
            {
                TSignedSeqPos a = algn[k].GetFrom();
                TSignedSeqPos b = algn[k].GetTo();
                if(!algn[k].m_ssplice) --b;
                for(TSignedSeqPos i = a; i <= b; ++i)
                {
                    if(cds_lim.GetFrom() <= i && i < cds_lim.GetTo()) m_stpscr[strand][i] = BadScore();  // score on last coding base
                }
            }
        }
        else
        {
            for(unsigned int k = 1; k < algn.size(); ++k)   // suppressing splitted STOPS in CDS
            {
                TSignedSeqPos a = algn[k].GetFrom();
                if(algn[k-1].m_ssplice && algn[k].m_fsplice && a > cds_lim.GetFrom() && a < cds_lim.GetTo())
                {
                    m_dsplit[strand][0][a] = 0;
                    m_dsplit[strand][1][a] = 0;
                }
            }
            
            for(unsigned int k = 0; k < algn.size(); ++k)   // suppressing STOPS in CDS
            {
                TSignedSeqPos a = algn[k].GetFrom();
                TSignedSeqPos b = algn[k].GetTo();
                for(TSignedSeqPos i = a; i <= b; ++i)
                {
                    if(i >= cds_lim.GetFrom() && i <= cds_lim.GetTo()) m_stpscr[strand][i] = BadScore();  // score on last intergenic (first stop) base
                }
            }
        }
    }

    for(int strand = 0; strand < 2; ++strand)
    {
        for(TSignedSeqPos i = 0; i < len; ++i)
        {
            if(m_sttscr[strand][i] != BadScore()) ++m_sttnum[strand];
            
            if(m_stpscr[strand][i] != BadScore())
            {
                if(strand == ePlus)
                {
                    int& lstp = m_laststop[strand][2-i%3][i+3];
                    lstp = max(int(i)+1,lstp);
                    ++m_stpnum[strand];
                }
                else
                {
                    int& lstp = m_laststop[strand][i%3][i];
                    lstp = max(int(i)-2,lstp);
                    ++m_stpnum[strand];
                }
            }
        }
    }


    for(int strand = 0; strand < 2; ++strand)
    {
        CEResidueVec& s = m_seq[strand];
        
        for(TSignedSeqPos i = 0; i < len; ++i)
        {
            TSignedSeqPos ii = strand == ePlus ? i : len-1-i;
            
            double score = m_ncdr.Score(s,ii);
            if(score == BadScore()) score = 0;
            m_ncdrscr[strand][i] = score;
            if(i > 0) m_ncdrscr[strand][i] += m_ncdrscr[strand][i-1];

            score = m_intrg.Score(s,ii);
            if(score == BadScore()) score = 0;
            m_ingscr[strand][i] = score;
            if(i > 0) m_ingscr[strand][i] += m_ingscr[strand][i-1];
        }

        for(int frame = 0; frame < 3; ++frame)
        {
            for(TSignedSeqPos i = 0; i < len; ++i)
            {
                int codonshift,ii;
                if(strand == ePlus)     // left end of codon is shifted by frame bases to left
                {
                    codonshift = (frame+i)%3;
                    ii = i;
                }
                else                  // right end of codone is shifted by frame bases to right
                {
                    codonshift = (frame-(int)i)%3;
                    if(codonshift < 0) codonshift += 3;
                    ii = len-1-i;
                }

                double score = m_cdr.Score(s,ii,codonshift);
                if(score == BadScore()) score = 0;

                m_cdrscr[strand][frame][i] = score;
                if(i > 0) 
                {
                    m_cdrscr[strand][frame][i] += m_cdrscr[strand][frame][i-1];
                    
                    TIVec& lstp = m_laststop[strand][frame];
                    lstp[i] = max(lstp[i-1],lstp[i]);
                }
            }
        }
    }

    for(int strand = 0; strand < 2; ++strand)
    {
        for(int frame = 0; frame < 3; ++frame)
        {
            for(TSignedSeqPos i = 0; i < len; ++i)
            {
                m_cdrscr[strand][frame][i] -= m_ncdrscr[strand][i];
            }
        }
        for(TSignedSeqPos i = 0; i < len; ++i)
        {
            m_ingscr[strand][i] -= m_ncdrscr[strand][i];

            int left, right;
            if(m_dscr[strand][i] != BadScore())
            {
                CTerminal& t = m_donor;
                left = i+1-(strand==ePlus ? t.Left():t.Right());
                right = i+(strand==ePlus ? t.Right():t.Left());
                m_dscr[strand][i] -= NonCodingScore(left,right,strand);
            }
            if(m_ascr[strand][i] != BadScore())
            {
                CTerminal& t = m_acceptor;
                left = i+1-(strand==ePlus ? t.Left():t.Right());
                right = i+(strand==ePlus ? t.Right():t.Left());
                m_ascr[strand][i] -= NonCodingScore(left,right,strand);
            }
            if(m_sttscr[strand][i] != BadScore())
            {
                CTerminal& t = m_start;
                left = i+1-(strand==ePlus ? t.Left():t.Right());
                right = i+(strand==ePlus ? t.Right():t.Left());
                m_sttscr[strand][i] -= NonCodingScore(left,right,strand);
            }
            if(m_stpscr[strand][i] != BadScore())
            {
                CTerminal& t = m_stop;
                left = i+1-(strand==ePlus ? t.Left():t.Right());
                right = i+(strand==ePlus ? t.Right():t.Left());
                m_stpscr[strand][i] -= NonCodingScore(left,right,strand);
            }
        }
    }
    
    const int NonConsensusMargin = 1500;
    for(set<CAlignVec,SAlignOrder>::iterator it = allaligns.begin(); consensuspenalty != BadScore() && it != allaligns.end(); ++it)
    {
        const CAlignVec& algn(*it);
        if(algn.Type() != CAlignVec::eProt) continue;
        
        int strand = algn.Strand();
        TSignedSeqRange lim = algn.Limits();
        
        for(unsigned int k = 1; k < algn.size(); ++k)
        {
            if(algn[k-1].m_ssplice && algn[k].m_fsplice) continue;
            
            int a = algn[k-1].GetTo();
            int b = algn[k].GetFrom();
            for(TSignedSeqPos i = a; i <= b; ++i)
            {
                if(i <= a+NonConsensusMargin || i >= b-NonConsensusMargin)
                {
                    if(m_dscr[strand][i] == BadScore())
                    {
                        m_dscr[strand][i] = consensuspenalty;
                        ++m_dnum[strand];
                    }
                    
                    if(m_ascr[strand][i] == BadScore())
                    {
                        m_ascr[strand][i] = consensuspenalty;
                        ++m_anum[strand];
                    }
                }
            }
        }
        
        if(strand == ePlus)
        {
            if(!algn.FullFiveP())
            {
                int a = max(2,lim.GetFrom()-NonConsensusMargin);
                int b = lim.GetFrom()-1;
                for(TSignedSeqPos i = a; i <= b; ++i)
                {
                    if(m_sttscr[strand][i] == BadScore())
                    {
                        m_sttscr[strand][i] = consensuspenalty;
                        ++m_sttnum[strand];
                    }
                    
                    if(m_dscr[strand][i] == BadScore())
                    {
                        m_dscr[strand][i] = consensuspenalty;
                        ++m_dnum[strand];
                    }
                    
                    if(m_ascr[strand][i] == BadScore())
                    {
                        m_ascr[strand][i] = consensuspenalty;
                        ++m_anum[strand];
                    }
                }
            }
            
            if(!algn.FullThreeP())
            {
                int a = lim.GetTo();
                int b = min(len-4,lim.GetTo()+NonConsensusMargin);
                for(TSignedSeqPos i = a; i <= b; ++i)
                {
                    if(m_stpscr[strand][i] == BadScore())
                    {
                        m_stpscr[strand][i] = consensuspenalty;
                        ++m_stpnum[strand];
                    }
                    
                    if(m_dscr[strand][i] == BadScore())
                    {
                        m_dscr[strand][i] = consensuspenalty;
                        ++m_dnum[strand];
                    }
                    
                    if(m_ascr[strand][i] == BadScore())
                    {
                        m_ascr[strand][i] = consensuspenalty;
                        ++m_anum[strand];
                    }
                }
            }
        }
        else
        {
            if(!algn.FullFiveP())
            {
                int a = lim.GetTo();
                int b = min(len-4,lim.GetTo()+NonConsensusMargin);
                for(TSignedSeqPos i = a; i <= b; ++i)
                {
                    if(m_sttscr[strand][i] == BadScore())
                    {
                        m_sttscr[strand][i] = consensuspenalty;
                        ++m_sttnum[strand];
                    }
                    
                    if(m_dscr[strand][i] == BadScore())
                    {
                        m_dscr[strand][i] = consensuspenalty;
                        ++m_dnum[strand];
                    }
                    
                    if(m_ascr[strand][i] == BadScore())
                    {
                        m_ascr[strand][i] = consensuspenalty;
                        ++m_anum[strand];
                    }
                }
            }
            
            if(!algn.FullThreeP())
            {
                int a = max(2,lim.GetFrom()-NonConsensusMargin);
                int b = lim.GetFrom()-1;
                for(TSignedSeqPos i = a; i <= b; ++i)
                {
                    if(m_stpscr[strand][i] == BadScore())
                    {
                        m_stpscr[strand][i] = consensuspenalty;
                        ++m_stpnum[strand];
                    }
                    
                    if(m_dscr[strand][i] == BadScore())
                    {
                        m_dscr[strand][i] = consensuspenalty;
                        ++m_dnum[strand];
                    }
                    
                    if(m_ascr[strand][i] == BadScore())
                    {
                        m_ascr[strand][i] = consensuspenalty;
                        ++m_anum[strand];
                    }
                }
            }
        }
    }
    
}

namespace {
    EResidue atg[3] = { enA, enT, enG };
    EResidue taa[3] = { enT, enA, enA };
    EResidue tag[3] = { enT, enA, enG };
    EResidue tga[3] = { enT, enG, enA };
    EResidue ag[2] = { enA, enG };
}    

bool SafeOpenEnd(const CEResidueVec& seq_strand, int start, int shift) {
    int cds_start = start+shift;
    for(int k = cds_start-2; k >= 0; k -= 1) {
        if(k < start-1 && equal(ag,ag+2,&seq_strand[k])) return true;   // splice must be outside of alignment
        if((cds_start-k)%3 != 0) continue;
        if(equal(atg,atg+3,&seq_strand[k])) return true;
        if(equal(taa,taa+3,&seq_strand[k]) || equal(tag,tag+3,&seq_strand[k]) || equal(tga,tga+3,&seq_strand[k])) return false;
    }

    return false;
}

void CGnomonEngine::GetScore(CAlignVec& model, bool uselims, bool allowopen) const
{
    const CTerminal& acceptor    = *m_data->m_acceptor;
    const CTerminal& donor       = *m_data->m_donor;
    const CTerminal& stt         = *m_data->m_start;
    const CTerminal& stp         = *m_data->m_stop;
    const CCodingRegion& cdr     = *m_data->m_cdr;
    const CNonCodingRegion& ncdr = *m_data->m_ncdr;

    int len = m_data->m_seq.size();
    int num = model.size();
    EStrand strand = model.Strand();
    const vector<CAlignExon>& exons = model;

    const CDoubleStrandSeq& ds = m_data->m_ds;
    CEResidueVec cds;
    TIVec cdsmap;
    model.GetSequence(ds[ePlus], cds, &cdsmap);   // due to historical reasons cds here is actually the whole mRNA

    int lim_start = -1;
    int lim_stop = -1;
    if(model.CdsLimits().Empty()) 
    {
        uselims = false;
    }
    else
    {
        for(unsigned int k = 0; k < cdsmap.size(); ++k)
        {
            if(int(model.CdsLimits().GetFrom()) == cdsmap[k]) lim_start = k;  
            if(int(model.CdsLimits().GetTo()) == cdsmap[k]) lim_stop = k;
        }
    }
    
    if(strand == eMinus) std::swap(lim_start,lim_stop);

    TIVec starts[3], stops[3];

    if(uselims) {
        for(int i = lim_start; i <= lim_stop-2; i += 3) {
            if(equal(taa,taa+3,&cds[i]) || equal(tag,tag+3,&cds[i]) || equal(tga,tga+3,&cds[i])) model.SetPStop( true );
        }
    }

    int lim_frame = lim_start%3;
    int cds_start = (strand == ePlus) ? cdsmap[0] : len-1-cdsmap[0];
    const CEResidueVec& seq_strand = ds[strand];

    if(allowopen && !equal(atg,atg+3,&cds[0]) && (!uselims || lim_frame == 0) && SafeOpenEnd(seq_strand,cds_start,0)) starts[0].push_back(0);  // those with atg will be included in the below loop
    if(allowopen && !equal(atg,atg+3,&cds[1]) && (!uselims || lim_frame == 1) && SafeOpenEnd(seq_strand,cds_start,1)) starts[1].push_back(1);
    if(allowopen && !equal(atg,atg+3,&cds[2]) && (!uselims || lim_frame == 2) && SafeOpenEnd(seq_strand,cds_start,2)) starts[2].push_back(2);
    
    CEResidueVec::iterator pos;
    pos = search(cds.begin(),cds.end(),atg,atg+3);
    while(pos != cds.end()) {
        int l = pos-cds.begin();
        int frame = l%3;
        if(!uselims || (frame == lim_frame && l <= lim_start)) starts[frame].push_back(l); // with uselims only earlier starts should be considered
        pos = search(pos+1,cds.end(),atg,atg+3);
    }


    pos = search(cds.begin(),cds.end(),taa,taa+3);
    while(pos != cds.end()) {
        int l = pos-cds.begin();
        int frame = l%3;
        if(!uselims || l < lim_start || l > lim_stop) stops[frame].push_back(l);   // if uselims stops in CDS are ignored
        pos = search(pos+1,cds.end(),taa,taa+3);
    }
    pos = search(cds.begin(),cds.end(),tag,tag+3);
    while(pos != cds.end()) {
        int l = pos-cds.begin();
        int frame = l%3;
        if(!uselims || l < lim_start || l > lim_stop) stops[frame].push_back(l);
        pos = search(pos+1,cds.end(),tag,tag+3);
    }
    pos = search(cds.begin(),cds.end(),tga,tga+3);
    while(pos != cds.end()) {
        int l = pos-cds.begin();
        int frame = l%3;
        if(!uselims || l < lim_start || l > lim_stop) stops[frame].push_back(l);
        pos = search(pos+1,cds.end(),tga,tga+3);
    }
    sort(stops[0].begin(),stops[0].end());    // three types of stops come independently in different order
    sort(stops[1].begin(),stops[1].end());
    sort(stops[2].begin(),stops[2].end());
    stops[cds.size()%3].push_back(cds.size());
    stops[(cds.size()-1)%3].push_back(cds.size()-1);
    stops[(cds.size()-2)%3].push_back(cds.size()-2);
    
    
    TDVec splicescr(cds.size(),0);
    
    if(strand == ePlus)
    {
        int shift = -1;
        for(int i = 1; i < num; ++i)
        {
            shift += model.FShiftedLen(exons[i-1].GetFrom(),exons[i-1].GetTo());
            
            if(exons[i-1].m_ssplice)
            {
                int l = exons[i-1].GetTo();
                double scr = donor.Score(ds[ePlus],l);
                if(scr == BadScore()) 
                {
                    scr = 0;
                }
                else
                {
                    for(int k = l-donor.Left()+1; k <= l+donor.Right(); ++k)
                    {
                        double s = ncdr.Score(ds[ePlus],k);
                        if(s == BadScore()) s = 0;
                        scr -= s;
                    }
                }
                splicescr[shift] = scr;
            }

            if(exons[i].m_fsplice)
            {
                int l = exons[i].GetFrom()-1;
                double scr = acceptor.Score(ds[ePlus],l);
                if(scr == BadScore()) 
                {
                    scr = 0;
                }
                else
                {
                    for(int k = l-acceptor.Left()+1; k <= l+acceptor.Right(); ++k)
                    {
                        double s = ncdr.Score(ds[ePlus],k);
                        if(s == BadScore()) s = 0;
                        scr -= s;
                    }
                }
                splicescr[shift] += scr;
            }
        }
    }
    else
    {
        int shift = cds.size()-1;
        for(int i = 1; i < num; ++i)
        {
            shift -= model.FShiftedLen(exons[i-1].GetFrom(),exons[i-1].GetTo());
            
            if(exons[i-1].m_ssplice)
            {
                int l = len-2-exons[i-1].GetTo();
                double scr = acceptor.Score(ds[eMinus],l);
                if(scr == BadScore()) 
                {
                    scr = 0;
                }
                else
                {
                    for(int k = l-acceptor.Left()+1; k <= l+acceptor.Right(); ++k)
                    {
                        double s = ncdr.Score(ds[eMinus],k);
                        if(s == BadScore()) s = 0;
                        scr -= s;
                    }
                }
                splicescr[shift] = scr;
            }

            if(exons[i].m_fsplice)
            {
                int l = len-1-exons[i].GetFrom();
                double scr = donor.Score(ds[eMinus],l);
                if(scr == BadScore()) 
                {
                    scr = 0;
                }
                else
                {
                    for(int k = l-donor.Left()+1; k <= l+donor.Right(); ++k)
                    {
                        double s = ncdr.Score(ds[eMinus],k);
                        if(s == BadScore()) s = 0;
                        scr -= s;
                    }
                }
                splicescr[shift] += scr;
            }
        }
    }

    TDVec cdrscr[3];
    for(int frame = 0; frame < 3; ++frame)
    {
        cdrscr[frame].resize(cds.size(),BadScore());
        for(int i = 0; i < (int)cds.size(); ++i)
        {
            int codonshift = (i-frame)%3;
            if(codonshift < 0) codonshift += 3;
                
            double scr = cdr.Score(cds,i,codonshift);
            if(scr == BadScore()) 
            {
                scr = 0;
            }
            else
            {
                double s = ncdr.Score(cds,i);
                if(s == BadScore()) s = 0;
                scr -= s;
            }

            cdrscr[frame][i] = scr+splicescr[i];
            if(i > 0) cdrscr[frame][i] += cdrscr[frame][i-1];
        }
    }
    
    if(!uselims)
    {
        model.SetCdsLimits(TSignedSeqRange::GetEmpty());
        model.SetMaxCdsLimits(TSignedSeqRange::GetEmpty());
    }
    model.SetScore(BadScore());

    for(int frame = 0; frame < 3; ++frame) {
        for(int i = (int)starts[frame].size()-1; i >= 0; --i) {
            int start = starts[frame][i];
            
            if(model.ConfirmedStart() && start != lim_start-3) continue;

            TIVec::iterator it_stop = lower_bound(stops[frame].begin(),stops[frame].end(),start);
            int stop = *it_stop-1;

            if(uselims && stop < lim_stop) continue;
            if(stop-start+1 < 75) continue;
            
            double s = cdrscr[frame][stop]-cdrscr[frame][start+2];
            
            double stt_score = BadScore();
            if(start >= stt.Left()+2) {   // 5 extra bases for ncdr
                int pnt = start+2;
                stt_score = stt.Score(cds,pnt);
                if(stt_score != BadScore()) {
                    for(int k = pnt-stt.Left()+1; k <= pnt+stt.Right(); ++k) {
                        double sn = ncdr.Score(cds,k);
                        if(sn != BadScore()) stt_score -= sn;
                    }
                }
            } else {
                int pnt = (strand == ePlus) ? cdsmap[start]+2 : len-1-cdsmap[start]+2;
                stt_score = stt.Score(ds[strand],pnt);
                if(stt_score != BadScore()) {
                    for(int k = pnt-stt.Left()+1; k <= pnt+stt.Right(); ++k) {
                        double sn = ncdr.Score(ds[strand],k);
                        if(sn != BadScore()) stt_score -= sn;
                    }
                }
            }
            if(stt_score != BadScore()) s += stt_score;
            
            double stp_score = stp.Score(cds,stop);
            if(stp_score != BadScore()) {
                for(int k = stop-stp.Left()+1; k <= stop+stp.Right(); ++k) {
                    double sn = ncdr.Score(cds,k);
                    if(sn != BadScore()) stp_score -= sn;
                }
                s += stp_score;
            }
            
            if(s > model.Score()) {
                bool opencds = false;
                if(!equal(atg,atg+3,&cds[start])) {
                    opencds = true;
                    if(starts[frame].size() > 1) {
                        int new_start = starts[frame][1];
                        int newlen = stop-new_start+1;
                        if(newlen > 75) start = new_start;
                    }
                }
                
                if(equal(atg,atg+3,&cds[start])) start += 3;
            
                model.SetOpenCds(opencds);
                
                int upstream_stop = -1;
                if(it_stop != stops[frame].begin()) upstream_stop = *(--it_stop);
                int utr_start = *lower_bound(starts[frame].begin(),starts[frame].end(),upstream_stop+1);
                if(utr_start < 3) utr_start = 0;
                int utr_stop = min((int)cds.size()-1,stop+3);
                
                if(strand == ePlus) {
                    model.SetMaxCdsLimits(TSignedSeqRange(cdsmap[utr_start],cdsmap[utr_stop]));
                } else {
                    model.SetMaxCdsLimits(TSignedSeqRange(cdsmap[utr_stop],cdsmap[utr_start]));
                }

                model.SetScore(s);
                
                while(cdsmap[start] < 0 && start < (int)cds.size()-1) ++start;  // in case they point to deletion  
                while(cdsmap[stop] < 0 && stop > 0) --stop; 
                if(strand == ePlus) {
                    model.SetCdsLimits(TSignedSeqRange(cdsmap[start],cdsmap[stop]));
                } else {
                    model.SetCdsLimits(TSignedSeqRange(cdsmap[stop],cdsmap[start]));
                }
            }
        }
    }
}


END_SCOPE(gnomon)
END_NCBI_SCOPE
