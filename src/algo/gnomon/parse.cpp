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
#include <algo/gnomon/gnomon_model.hpp>
#include <algo/gnomon/gnomon_exception.hpp>
#include "hmm.hpp"
#include "parse.hpp"
#include "hmm_inlines.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

template<class L, class R> inline 
bool s_TooFar(const L& left, const R& right, double score) 
{
    if(left.MScore() == BadScore()) return true;
    int len = right.Stop()-left.Stop();
    return (len > kTooFarLen && score+left.MScore() < right.Score());
}

struct SRState
{
    SRState(const CHMM_State& l, const CHMM_State& r)
    {
        lsp = r.LeftState();
        rsp = const_cast<CHMM_State*>(&r);
        rsp->UpdateLeftState(l);
    }
    ~SRState() { rsp->UpdateLeftState(*lsp); } 
        
    const CHMM_State* lsp;
    CHMM_State* rsp;
};

template<class Left, class Right> inline
bool s_EvaluateNewScore(const Left& left, const Right& right, double& rscore, bool& openrgn, bool rightanchor = false)
{

    rscore = BadScore();

    SRState rst(left,right);
    
    int len = right.Stop()-left.Stop();
    if(len > right.MaxLen()) return false;
    if(!right.NoRightEnd() && len < right.MinLen()) return true;
    
    double scr, score = 0;
    if(left.isPlus())
    {
        scr = left.BranchScore(right);
        if(scr == BadScore()) return true;
    }
    else
    {
        scr = right.BranchScore(left);
        if(scr == BadScore()) return true;
        scr += right.DenScore()-left.DenScore();
    }
    score += scr;

    // this check is frame-dependent and MUST be after BranchScore
    if(right.StopInside()) return false;     
    
    if(right.NoRightEnd() && !rightanchor) scr = right.ClosingLengthScore();
    else scr = right.LengthScore();
    if(scr == BadScore()) return true;
    score += scr;
    
    scr = right.RgnScore();
    if(scr == BadScore()) return true;
    score += scr;
    
    if(!right.NoRightEnd())
    {
        scr = right.TermScore();
        if(scr == BadScore()) return true;
        score += scr;
    }
    
    openrgn = right.OpenRgn();
    rscore = score;
    return true;
}

// leftprot   0 no protein, 1 there is a protein
// rightprot  0 no protein, 1 must be a protein, -1 don't care 
template<class L, class R> // template for all exons
inline bool s_ForwardStep(const L& left, R& right, int leftprot, int rightprot)
{
    double score;
    bool openrgn;
    if(!s_EvaluateNewScore(left,right,score,openrgn)) return false;
    else if(score == BadScore()) return true;
    
    int protnum = right.GetSeqScores()->ProtNumber(left.Stop(),right.Stop());
    
    if(rightprot == 0 && (leftprot != 0 || protnum != 0))    // there is a protein 
    {
        return true;
    }
    
    if(leftprot == 0)
    {
        if(rightprot == 1 && protnum == 0) return true;      // no proteins
        else if(protnum > 0) --protnum;                      // for first protein no penalty
    }
    
    if(left.Score() != BadScore() && openrgn)
    {
         double scr = score-protnum*right.GetSeqScores()->MultiProtPenalty()+left.Score();
        if(scr > right.Score())
        {
            right.UpdateLeftState(left);
            right.UpdateScore(scr);
        }
    }
        
    return openrgn;
}

template<class L> 
inline bool s_ForwardStep(const L& left, CIntron& right)
{
    double score;
    bool openrgn;
    if(!s_EvaluateNewScore(left,right,score,openrgn)) return false;
    else if(score == BadScore()) return true;
    
    if(left.Score() != BadScore() && openrgn)
    {
        double scr = score+left.Score();
        if(scr > right.Score())
        {
            right.UpdateLeftState(left);
            right.UpdateScore(scr);
        }
    }
        
    return (openrgn && !s_TooFar(left, right, score));
}

template<class L> 
inline bool s_ForwardStep(const L& left, CIntergenic& right, bool rightanchor)
{
    double score;
    bool openrgn;
    if(!s_EvaluateNewScore(left,right,score,openrgn,rightanchor)) return false;
    else if(score == BadScore()) return true;
    
    if(left.Score() != BadScore() && openrgn)
    {
        double scr = score+left.Score();
        if(scr > right.Score())
        {
            right.UpdateLeftState(left);
            right.UpdateScore(scr);
        }
    }
        
    return (openrgn && !s_TooFar(left, right, score));
}


template<class L, class R> // template for all exons
inline void s_MakeStep(vector<L>& lvec, vector<R>& rvec, int leftprot, int rightprot)
{
    if(lvec.empty()) return;
    typename vector<L>::reverse_iterator i = lvec.rbegin();
    if(i->Stop() == rvec.back().Stop()) ++i;
    for(;i != lvec.rend() && s_ForwardStep(*i,rvec.back(),leftprot,rightprot);++i);
    
    if(rvec.size() > 1) {
        rvec.back().UpdatePrevExon(rvec[rvec.size()-2]);
    }
}

template<class L> 
inline void s_MakeStep(vector<L>& lvec, vector<CIntron>& rvec)
{
    if(lvec.empty()) return;
    CIntron& right = rvec.back();
    typename vector<L>::reverse_iterator i = lvec.rbegin();

    int rlimit = right.Stop();
    if(i->Stop() == rlimit) ++i;
    int nearlimit = max(0,rlimit-kTooFarLen);
    while(i != lvec.rend() && i->Stop() >= nearlimit) 
    {
        if(!s_ForwardStep(*i,right)) return; 
        ++i;
    }
    if(i == lvec.rend()) return; 
    for(const L* p = &*i; p !=0; p = p->PrevExon())
    {
        if(!s_ForwardStep(*p,right)) return; 
    }
}


template<class L> 
inline void s_MakeStep(vector<L>& lvec, vector<CIntergenic>& rvec, bool rightanchor = false)
{
    if(lvec.empty()) return;
    CIntergenic& right = rvec.back();
    int i = lvec.size()-1;
    int rlimit = right.Stop();
    if(lvec[i].Stop() == rlimit) --i;
    while(i >= 0 && lvec[i].Stop() >= CHMM_State::GetSeqScores()->LeftAlignmentBoundary(right.Stop())) --i;  // no intergenics in alignment
    int nearlimit = max(0,rlimit-kTooFarLen);
    while(i >= 0 && lvec[i].Stop() >= nearlimit) 
    {
        if(!s_ForwardStep(lvec[i--],right,rightanchor)) return;
    }
    if(i < 0) return;
    for(const L* p = &lvec[i]; p !=0; p = p->PrevExon())
    {
        if(!s_ForwardStep(*p,right,rightanchor)) return;
    }
}

inline void s_MakeStep(EStrand strand, int point, vector<CIntergenic>& lvec, vector<CSingleExon>& rvec)            // L - intergenic, R - single exon
{
    rvec.push_back(CSingleExon(strand,point));
    s_MakeStep(lvec, rvec, 0, -1);
    if(rvec.back().Score() == BadScore()) rvec.pop_back();
}

template<class R> 
inline void s_MakeStep(EStrand strand, int point, vector<CIntergenic>& lvec, vector<R> rvec[][3])                 // L - intergenic, R - first/last exon
{
    for(int kr = 0; kr < 2; ++kr)
    {
        for(int phr = 0; phr < 3; ++phr)
        {
            rvec[kr][phr].push_back(R(strand,phr,point));
            s_MakeStep(lvec, rvec[kr][phr], 0, kr);
            if(rvec[kr][phr].back().Score() == BadScore()) rvec[kr][phr].pop_back();
        }
    }
}

template<class R> 
inline void s_MakeStep(EStrand strand, int point, vector<CIntron> lvec[][3], vector<R>& rvec)                 // L - intron R - first/last exon
{
    rvec.push_back(R(strand,0,point));          // phase is bogus, it will be redefined in constructor
    for(int kl = 0; kl < 2; ++kl)
    {
        for(int phl = 0; phl < 3; ++phl)
        {
            s_MakeStep(lvec[kl][phl], rvec, kl, -1);
        }
    }
    if(rvec.back().Score() == BadScore()) rvec.pop_back();
}

inline void s_MakeStep(EStrand strand, int point, vector<CIntron> lvec[][3], vector<CInternalExon> rvec[][3])   // L - intron, R - internal exon
{
    for(int phr = 0; phr < 3; ++phr)
    {
        for(int kr = 0; kr < 2; ++kr)
        {
            rvec[kr][phr].push_back(CInternalExon(strand,phr,point));
            for(int phl = 0; phl < 3; ++phl)
            {
                for(int kl = 0; kl < 2; ++kl)
                {
                    s_MakeStep(lvec[kl][phl], rvec[kr][phr], kl, kr);
                }
            }
            if(rvec[kr][phr].back().Score() == BadScore()) rvec[kr][phr].pop_back();
        }
    }
}

template<class L1, class L2> 
inline void s_MakeStep(EStrand strand, int point, vector<L1> lvec1[][3], vector<L2> lvec2[][3], vector<CIntron> rvec[][3], int shift)    // L1,L2 - exons, R -intron
{
    for(int k = 0; k < 2; ++k)
    {
        for(int phl = 0; phl < 3; ++phl)
        {
            int phr = (shift+phl)%3;
            rvec[k][phr].push_back(CIntron(strand,phr,point));
            if(k == 1) rvec[k][phr].back().UpdateScore(BadScore());   // proteins don't come from outside
            s_MakeStep(lvec1[k][phl], rvec[k][phr]);
            s_MakeStep(lvec2[k][phl], rvec[k][phr]);
            if(rvec[k][phr].back().Score() == BadScore()) rvec[k][phr].pop_back();
        }
    }
}

double AddProbabilities(double scr1, double scr2)
{
    if(scr1 == BadScore()) return scr2;
    else if(scr2 == BadScore()) return scr1;
    else if(scr1 >= scr2)  return scr1+log(1+exp(scr2-scr1));
    else return scr2+log(1+exp(scr1-scr2));
}

CParse::CParse(const CSeqScores& ss, bool leftanchor, bool rightanchor) : m_seqscr(ss) {
    try {
        m_igplus.reserve(m_seqscr.StartNumber(ePlus)+1);
        m_igminus.reserve(m_seqscr.StopNumber(eMinus)+1);
        m_feminus.reserve(m_seqscr.StartNumber(eMinus)+1);
        m_leplus.reserve(m_seqscr.StopNumber(ePlus)+1);
        m_seplus.reserve(m_seqscr.StopNumber(ePlus)+2);
        m_seminus.reserve(m_seqscr.StartNumber(eMinus)+1);
        for(int k = 0; k < 2; ++k) {
            for(int phase = 0; phase < 3; ++phase) {
                m_inplus[k][phase].reserve(m_seqscr.AcceptorNumber(ePlus)+1);
                m_inminus[k][phase].reserve(m_seqscr.DonorNumber(eMinus)+1);
                m_feplus[k][phase].reserve(m_seqscr.DonorNumber(ePlus)+1);
                m_ieplus[k][phase].reserve(m_seqscr.DonorNumber(ePlus)+1);
                m_ieminus[k][phase].reserve(m_seqscr.AcceptorNumber(eMinus)+1);
                m_leminus[k][phase].reserve(m_seqscr.AcceptorNumber(eMinus)+1);
            }
        }
    }
    catch(bad_alloc) {
        NCBI_THROW(CGnomonException, eMemoryLimit, "Not enough memory in CParse");
    }
    
    int len = m_seqscr.SeqLen();
    
    double BigScore = 10000;
    if(leftanchor) {
        m_seplus.push_back(CSingleExon(ePlus,0));
        m_seplus.back().UpdateScore(BigScore);               // this is a bogus anchor to unforce intergenic start at 0 
    }
    
    for(int i = 0; i < len; ++i) {
        if(m_seqscr.AcceptorScore(i,ePlus) != BadScore()) {
            s_MakeStep(ePlus,i,m_ieplus,m_feplus,m_inplus,1);
        }

        if(m_seqscr.AcceptorScore(i,eMinus) != BadScore()) {
            s_MakeStep(eMinus,i,m_inminus,m_ieminus);
            s_MakeStep(eMinus,i,m_igminus,m_leminus);
        }

        if(m_seqscr.DonorScore(i,ePlus) != BadScore()) {
            s_MakeStep(ePlus,i,m_inplus,m_ieplus);
            s_MakeStep(ePlus,i,m_igplus,m_feplus);
        }

        if(m_seqscr.DonorScore(i,eMinus) != BadScore()) {
            s_MakeStep(eMinus,i,m_leminus,m_ieminus,m_inminus,0);
        }

        if(m_seqscr.StartScore(i,ePlus) != BadScore()) {
            m_igplus.push_back(CIntergenic(ePlus,i));
            s_MakeStep(m_seplus,m_igplus);
            s_MakeStep(m_seminus,m_igplus);
            s_MakeStep(m_leplus,m_igplus);
            s_MakeStep(m_feminus,m_igplus);
        }
        
        if(m_seqscr.StartScore(i,eMinus) != BadScore()) {
            s_MakeStep(eMinus,i,m_inminus,m_feminus);
            s_MakeStep(eMinus,i,m_igminus,m_seminus);
        }

        if(m_seqscr.StopScore(i,ePlus) != BadScore()) {
            s_MakeStep(ePlus,i,m_inplus,m_leplus); 
            s_MakeStep(ePlus,i,m_igplus,m_seplus); 
        } 

        if(m_seqscr.StopScore(i,eMinus) != BadScore()) {
            m_igminus.push_back(CIntergenic(eMinus,i));
            s_MakeStep(m_seplus,m_igminus);
            s_MakeStep(m_seminus,m_igminus);
            s_MakeStep(m_leplus,m_igminus);
            s_MakeStep(m_feminus,m_igminus);
        }
    }

    m_igplus.push_back(CIntergenic(ePlus,-1));                // never is popped    
    s_MakeStep(m_seplus,m_igplus,rightanchor);
    s_MakeStep(m_seminus,m_igplus,rightanchor);
    s_MakeStep(m_leplus,m_igplus,rightanchor);
    s_MakeStep(m_feminus,m_igplus,rightanchor);
    
    m_igminus.push_back(CIntergenic(eMinus,-1));               // never is popped   
    s_MakeStep(m_seplus,m_igminus,rightanchor);
    s_MakeStep(m_seminus,m_igminus,rightanchor);
    s_MakeStep(m_leplus,m_igminus,rightanchor);
    s_MakeStep(m_feminus,m_igminus,rightanchor);
    
    m_igplus.back().UpdateScore(AddProbabilities(m_igplus.back().Score(),m_igminus.back().Score()));
    
    const CHMM_State* p = &m_igplus.back();

    if(!rightanchor) {
        s_MakeStep(ePlus,-1,m_ieplus,m_feplus,m_inplus,1);        // may be popped  
    
        s_MakeStep(eMinus,-1,m_leminus,m_ieminus,m_inminus,0);    // may be popped

        for(int k = 0; k < 2; ++k) {
            for(int ph = 0; ph < 3; ++ph) {
                if(!m_inplus[k][ph].empty() && m_inplus[k][ph].back().NoRightEnd() && m_inplus[k][ph].back().Score() > p->Score()) p = &m_inplus[k][ph].back();
                if(!m_inminus[k][ph].empty() && m_inminus[k][ph].back().NoRightEnd() && m_inminus[k][ph].back().Score() > p->Score()) p = &m_inminus[k][ph].back();
            }
        }
    }

    m_path = p;

    if(leftanchor) {                                       // return scores to normal and forget the connection to the left bogus anchor    
        for(p = m_path ; p != 0; p = p->LeftState()) {
            const_cast<CHMM_State*>(p)->UpdateScore(p->Score()-BigScore);
            if(p->LeftState() == &m_seplus[0]) const_cast<CHMM_State*>(p)->ClearLeftState();
        }
    }
}


template<class T> void Out(T t, int w, CNcbiOstream& to = cout)
{
    to.width(w);
    to.setf(IOS_BASE::right,IOS_BASE::adjustfield);
    to << t;
}

void Out(double t, int w, CNcbiOstream& to = cout, int prec = 1)
{
    to.width(w);
    to.setf(IOS_BASE::right,IOS_BASE::adjustfield);
    to.setf(IOS_BASE::fixed,IOS_BASE::floatfield);
    to.precision(prec);

    if(t > 1000000000) to << "+Inf";
    else if(t < -1000000000) to << "-Inf";
    else to << t;
}

void CGene::Print(int gnum, int mnum, CNcbiOstream& to, CNcbiOstream& toprot) const
{
    int gene_start = -1;
    int gene_stop = 0;

    const TFrameShifts& fshifts(FrameShifts());
    TFrameShifts::const_iterator fsi = fshifts.begin(); 

    ITERATE(CGene, i, *this)
    {
        const CExonData& exon(*i);

        to << m_contig << '\t';
        to << gnum << '\t' << mnum << '\t';
        to << exon.Type() << '\t';
        to << ((Strand() == ePlus) ? '+' : '-') << '\t';
        
        TSignedSeqPos estart = exon.GetFrom();
        TSignedSeqPos estop = exon.GetTo();
        int len = estop-estart+1;
        TSignedSeqPos aa = estart;
        for( ; fsi != fshifts.end() && fsi->Loc() <= aa; ++fsi)        // insertions/deletions after first splice
        {
            if(fsi->IsDeletion()) 
            {
                to << 'D' << fsi->DeletedValue() << '-' << fsi->Loc() << ' ';
                len += fsi->Len();
            }
            else
            {
                to << 'I' << fsi->Loc() << '-' << fsi->Loc()+fsi->Len()-1 << ' ';
                len -= fsi->Len();
            }
        }
        while(aa <= estop)
        {

            to << aa << ' ';
            TSignedSeqPos bb = (fsi != fshifts.end() && fsi->Loc() <= estop) ? fsi->Loc()-1 : estop;
            to << bb;

            for( ; fsi != fshifts.end() && fsi->Loc() == bb+1; ++fsi)    // insertions/deletions in the middle and before the second splice
            {
                if(fsi->IsDeletion()) 
                {
                    to << " D" << fsi->DeletedValue() << '-' << fsi->Loc();
                    len += fsi->Len();
                }
                else
                {
                    bb = fsi->Loc()+fsi->Len()-1;
                    to << " I" << fsi->Loc() << '-' << bb;
                    len -= fsi->Len();
                }
            }
            if(bb != estop) to << ' ';
            aa = bb+1;
        }

        to << '\t';
        
        to << len << '\t' << exon.SupportedLen() << '\t';
        
        int lframe = exon.lFrame();
        int rframe = exon.rFrame();
        if(lframe < 0)
        {
            to << "-\t";
        }
        else
        {
            to << lframe << '/' << rframe << '\t';
            if(gene_start < 0) gene_start = estart;
            gene_stop = estop;
        }
        
        if(exon.Score() != BadScore()) to << exon.Score() << '\t';
        else to << "-\t";
        
        if(exon.ChainID().empty())
        {
            to << '-';
        }
        else 
        {
            set<int>::const_iterator it = exon.ChainID().begin();
            to << *it++;
            while(it != exon.ChainID().end()) to << '+' << *it++;
        }

        to << '\n';
    }        
        
    toprot << '>' << m_contig << "." << gnum << "." << mnum << ".";
    toprot << gene_start << "." << gene_stop << "_"
           << ((Strand() == ePlus) ? "plus" : "minus")
           << " hmm" << mnum << '\n';
            
    int ii;
    for(ii = CDS_Shift(); ii < (int)m_cds.size()-2; ii +=3)
    {
        toprot << k_aa_table[fromACGT(m_cds[ii])*25+fromACGT(m_cds[ii+1])*5+fromACGT(m_cds[ii+2])];
        if((ii+3-CDS_Shift())%150 == 0) toprot << '\n';
    }
    if((ii-CDS_Shift())%150 != 0) toprot << '\n';
}

list<CGene> CParse::GetGenes() const
{
    list<CGene> genes;
    vector< vector<const CExon*> > gen_exons;
    
    if(dynamic_cast<const CIntron*>(Path()) && Path()->LeftState()) 
    {
        genes.push_front(CGene(Path()->Strand(),false,false));
        gen_exons.push_back(vector<const CExon*>());
    }

    for(const CHMM_State* p = Path(); p != 0; p = p->LeftState()) 
    {
        if(const CExon* pe = dynamic_cast<const CExon*>(p))
        {
            if(dynamic_cast<const CSingleExon*>(pe) ||
               (dynamic_cast<const CLastExon*>(pe) && pe->isPlus()) ||
               (dynamic_cast<const CFirstExon*>(pe) && pe->isMinus())) gen_exons.push_back(vector<const CExon*>());
            
            gen_exons.back().push_back(pe);
        }
    }
    
    const TFrameShifts& fs = m_seqscr.SeqTFrameShifts();
    int cur_fs = fs.size()-1;
    
    for(int gnum = 0; gnum < (int)gen_exons.size(); ++gnum)
    {
        TSignedSeqRange gene_cds_limits(gen_exons[gnum].back()->Start(),gen_exons[gnum].front()->Stop());
        gene_cds_limits.SetFrom( m_seqscr.SeqMap(gene_cds_limits.GetFrom(),CSeqScores::eMoveRight) );
        gene_cds_limits.SetTo( m_seqscr.SeqMap(gene_cds_limits.GetTo(),CSeqScores::eMoveLeft) );
        
        const TAlignList& align_list = m_seqscr.Alignments();
        CClusterSet cls;
        for(TAlignListConstIt it = align_list.begin(); it != align_list.end(); ++it)
        {
            const CAlignVec& algn(*it);
            
            if(algn.Type() == CAlignVec::eWall || algn.Type() == CAlignVec::eNested) continue;
            if(!Include(TSignedSeqRange(m_seqscr.From(),m_seqscr.To()),algn.Limits())) continue;
            
            
            if(algn.CdsLimits().Empty())
            {
                if(algn.Limits().IntersectingWith(gene_cds_limits)) cls.InsertAlignment(algn);
            }
            else
            {
                if(algn.CdsLimits().IntersectingWith(gene_cds_limits)) cls.InsertAlignment(algn);
            }
        }
        
        for(int exnum = 0; exnum < (int)gen_exons[gnum].size(); ++exnum)
        {
            const CExon* pe = gen_exons[gnum][exnum];

            bool stopcodon = false;
            bool startcodon = false;
            double score = pe->RgnScore();
            
            if(dynamic_cast<const CSingleExon*>(pe))
            {
                genes.push_front(CGene(pe->Strand(),true,true));
                startcodon = true;
                stopcodon = true;
                
                score += dynamic_cast<const CIntergenic*>(pe->LeftState())->TermScore();
                score += dynamic_cast<const CSingleExon*>(pe)->TermScore();
            }
            else if(dynamic_cast<const CLastExon*>(pe))
            {
                score += dynamic_cast<const CLastExon*>(pe)->TermScore();
                if(pe->isPlus()) 
                {
                    score += dynamic_cast<const CIntron*>(pe->LeftState())->TermScore();
                    genes.push_front(CGene(pe->Strand(),false,true));
                }
                else 
                {
                    score += dynamic_cast<const CIntergenic*>(pe->LeftState())->TermScore();
                    genes.front().SetLeftComplete(true);
                }
                stopcodon = true;
            }
            else if(dynamic_cast<const CFirstExon*>(pe))
            {
                score += dynamic_cast<const CFirstExon*>(pe)->TermScore();
                if(pe->isMinus()) 
                {
                    score += dynamic_cast<const CIntron*>(pe->LeftState())->TermScore();
                    genes.push_front(CGene(pe->Strand(),false,true));
                }
                else 
                {
                    score += dynamic_cast<const CIntergenic*>(pe->LeftState())->TermScore();
                    genes.front().SetLeftComplete(true);
                }
                startcodon = true;
            }
            else
            {
                score += dynamic_cast<const CInternalExon*>(pe)->TermScore();
                score += dynamic_cast<const CIntron*>(pe->LeftState())->TermScore();
            }
            
            CGene& curgen = genes.front();
            
            curgen.SetContigName(m_seqscr.Contig());
            
            int estart = pe->Start();
            int estop = pe->Stop();
            
            int rframe = pe->Phase();
            int lframe = pe->isPlus() ? (rframe-(estop-estart))%3 : (rframe+(estop-estart))%3;
            if(lframe < 0) lframe += 3;
            
            if(pe->isPlus())
            {
                curgen.SetCdsShift((3-lframe)%3);  // first complete codon for partial CDS
            }
            else if(curgen.empty())
            {
                curgen.SetCdsShift((3-rframe)%3);
            }
/*            
            if(pe->isMinus() && startcodon)
            {
                curgen.CDS().push_back('T');
                curgen.CDS().push_back('A');
                curgen.CDS().push_back('C');
            }
            for(int i = estop; i >= estart; --i) curgen.CDS().push_back(toACGT(*m_seqscr.SeqPtr(i,ePlus)));
            if(pe->isPlus() && startcodon)
            {
                curgen.CDS().push_back('G');
                curgen.CDS().push_back('T');
                curgen.CDS().push_back('A');
            }
*/
            
            // a is first point on the original sequence which is not insertion
            // b is last point on the original sequence which is not insertion
            // a_deletion, b_deletion are total deletion length
            
            int a_deletion, b_deletion;
            int a = m_seqscr.SeqMap(estart,CSeqScores::eMoveRight,&a_deletion); 
            int b = m_seqscr.SeqMap(estop,CSeqScores::eMoveLeft,&b_deletion);   
            TSignedSeqRange ab_limits(a,b);
            
            pair<CClusterSet::TConstIt,CClusterSet::TConstIt> lim = cls.equal_range(CCluster(a,b));
            CClusterSet::TConstIt first = lim.first;
            CClusterSet::TConstIt last = lim.second;
            if(lim.first != lim.second) --last;
            
            int lastid = 0;
            int margin = numeric_limits<int>::min();   // difference between end of exon and end of alignment exon
            bool rightexon = false;
            string right_utr = pe->isPlus() ? "3'_Utr" : "5'_Utr";
            if(((startcodon && pe->isMinus()) || (stopcodon && pe->isPlus())) && lim.first != lim.second)   // rightside UTR
            {
                for(CCluster::TConstIt it = last->begin(); it != last->end(); ++it)
                {
                    const CAlignVec& algn(*it);
                    if(!algn.Limits().IntersectingWith(ab_limits)) continue;
                    
                    for(int i = (int)algn.size()-1; i >= 0; --i)
                    {
                        if(b < (int)algn[i].GetFrom()) 
                        {
                            curgen.push_back(CExonData(algn[i].GetFrom(),algn[i].GetTo(),-1,-1,right_utr));
                            curgen.back().ChainID().insert(algn.ID()); 
                            rightexon = true;
                        }
                        else if((int)algn[i].GetTo() >= a && (int)algn[i].GetTo() > b+margin)
                        {
                            margin = algn[i].GetTo()-b;
                            lastid = algn.ID();
                        }
                    }
                }
            }
            
            int remaina = 0;
            if((pe->isMinus() && startcodon) || (pe->isPlus() && stopcodon))   // start/stop position correction
            {
                if(margin >= 3)
                {
                    b += 3;
                    margin -= 3;
                }
                else if(margin >= 0 && rightexon)
                {
                    b += margin;
                    rframe = pe->isPlus() ? (rframe+margin)%3 : (rframe-margin+3)%3;
                    remaina = 3-margin;
                    margin = 0;
                }
                else
                {
                    b = m_seqscr.SeqMap(estop+3,CSeqScores::eMoveLeft,&b_deletion);
                    margin = 0;
                }
            }
            
            bool splitted_start = false;
            if(margin > 0)
            {
                curgen.push_back(CExonData(b+1,b+margin,-1,-1,right_utr)); 
                curgen.back().ChainID().insert(lastid); 
            }
            else if(remaina > 0)
            {
                CExonData& e = curgen.back();
                TSignedSeqPos aa = e.GetFrom();
                e.SetFrom( aa + remaina );
                TSignedSeqPos bb = aa+remaina-1;
                int lf, rf;
                string etype;
                if(pe->isMinus())    // splitted start/stop
                {
                    lf = remaina-1;
                    rf = 0;
                    etype = "First_Cds";
                    if(startcodon) splitted_start = true;
                }
                else
                {
                    lf = 3-remaina;
                    rf = 2;
                    etype = "Last_Cds";
                }
                CExonData er(aa,bb,lf,rf,etype);
                er.ChainID() = e.ChainID();
                
                if(e.GetFrom() <= e.GetTo())
                {
                    curgen.push_back(er);
                }
                else
                {
                    e = er;     // nothing left of e
                }
            }
            
            if(pe->isMinus() && startcodon)
            {
                if(splitted_start)
                {
                    curgen.CDS().push_back('T');
                    curgen.CDS().push_back('A');
                    curgen.CDS().push_back('C');
                }
                else
                {
                    curgen.CDS().push_back(toACGT(*m_seqscr.SeqPtr(estop+3,ePlus)));
                    curgen.CDS().push_back(toACGT(*m_seqscr.SeqPtr(estop+2,ePlus)));
                    curgen.CDS().push_back(toACGT(*m_seqscr.SeqPtr(estop+1,ePlus)));
                }
            }
            for(int i = estop; i >= estart; --i) curgen.CDS().push_back(toACGT(*m_seqscr.SeqPtr(i,ePlus)));
            
            curgen.push_back(CExonData(a,b,lframe,rframe,"Internal_Cds"));
            CExonData& exon = curgen.back();
            
            exon.SetScore( score );
            
            margin = numeric_limits<int>::min();
            bool leftexon = false;
            lastid = 0;
            for(CClusterSet::TConstIt cls_it = lim.first; cls_it != lim.second; ++cls_it)
            {
                for(CCluster::TConstIt it = cls_it->begin(); it != cls_it->end(); ++it)
                {
                    const CAlignVec& algn(*it);
                    if(!algn.Limits().IntersectingWith(ab_limits)) continue;
                    
                    for(int i = 0; i < (int)algn.size(); ++i)
                    {
                        if(ab_limits.GetTo() < algn[i].GetFrom()) 
                        {
                            break;
                        }
                        else if(algn[i].GetTo() < ab_limits.GetFrom()) 
                        {
                            leftexon = true;
                            continue;
                        }
                        else 
                        {
                            exon.ChainID().insert(algn.ID());
                            if (a > int(algn[i].GetFrom())+margin)
                            {
                                margin = a-algn[i].GetFrom();
                                lastid = algn.ID();
                            }
                        }
                    }
                }
            }
            
            int remainb = 0;
            if((pe->isPlus() && startcodon) || (pe->isMinus() && stopcodon))   // start/stop position correction
            {
                if(margin >= 3)
                {
                    exon.SetFrom(exon.GetFrom() - 3);
                    margin -= 3;
                }
                else if(margin >= 0 && leftexon)
                {
                    exon.SetFrom(exon.GetFrom() - margin);
                    exon.SetlFrame( pe->isPlus() ? (exon.lFrame()-margin+3)%3 : (exon.lFrame()+margin)%3 );
                    remainb = 3-margin;
                    margin = 0;
                }
                else
                {
                    exon.SetFrom( m_seqscr.SeqMap(estart-3,CSeqScores::eMoveRight,&a_deletion) );
                    margin = 0;
                }
            }
            
            if(cur_fs >= 0)
            {
                int ifs = (int)fs.size();
                while(cur_fs >= 0 && fs[cur_fs].Loc() > exon.GetFrom()) 
                {
                    ifs = cur_fs--;
                }
                
                TSignedSeqPos aa = exon.GetFrom();
                for(; cur_fs >= 0 && (fs[cur_fs].IsDeletion() ? fs[cur_fs].Loc() : fs[cur_fs].Loc()+fs[cur_fs].Len()) == aa; --cur_fs)
                {
                    const CFrameShiftInfo& cfs = fs[cur_fs];
                    if(cfs.IsDeletion())
                    {
                        int dl = min(cfs.Len(),a_deletion);
                        string del_v = cfs.DeletedValue().substr(cfs.Len()-dl);
                        curgen.FrameShifts().push_back(CFrameShiftInfo(cfs.Loc(),dl,false,del_v));
                        a_deletion -= dl;
                        if(cfs.Len() > dl) break;
                    }
                    else
                    {
                        curgen.FrameShifts().push_back(cfs);
                        aa = cfs.Loc();
                    }
                }
                
                for(; ifs < (int)fs.size() && fs[ifs].Loc() <= exon.GetTo() && fs[ifs].Loc() > exon.GetFrom(); ++ifs)
                {
                    curgen.FrameShifts().push_back(fs[ifs]);
                }
                
                TSignedSeqPos bb = exon.GetTo();
                for(; ifs < (int)fs.size() && fs[ifs].Loc() == bb+1; ++ifs)
                {
                    const CFrameShiftInfo& cfs = fs[ifs];
                    if(cfs.IsDeletion())
                    {
                        int dl = min(cfs.Len(),b_deletion);
                        string del_v = cfs.DeletedValue().substr(0,dl);
                        curgen.FrameShifts().push_back(CFrameShiftInfo(cfs.Loc(),dl,false,del_v));
                        b_deletion -= dl;
                        if(cfs.Len() > dl) break;
                    }
                    else
                    {
                        curgen.FrameShifts().push_back(cfs);
                        bb = cfs.Loc()+cfs.Len()-1;
                    }
                }
                
                sort(curgen.FrameShifts().begin(),curgen.FrameShifts().end());
            }

            if(startcodon && stopcodon)   // maybe Single
            {
                if(remaina == 0 && remainb == 0)
                {
                    exon.SetType( "Single_Cds" );
                }
                else if(remainb == 0)
                {
                    exon.SetType( pe->isPlus() ? "First_Cds" : "Last_Cds" );
                }
                else if(remaina == 0)
                {
                    exon.SetType( pe->isPlus() ? "Last_Cds" : "First_Cds" );
                }
            }
            else if(startcodon)          // maybe First
            {
                if((pe->isPlus() && remainb == 0) || (pe->isMinus() && remaina == 0)) exon.SetType( "First_Cds" );
            }
            else if(stopcodon)          // maybe Last
            {
                if((pe->isMinus() && remainb == 0) || (pe->isPlus() && remaina == 0)) exon.SetType( "Last_Cds" );
            }
            
            
            string left_utr = pe->isPlus() ? "5'_Utr" : "3'_Utr";
            if(((startcodon && pe->isPlus()) || (stopcodon && pe->isMinus())) && lim.first != lim.second)   // leftside UTR
            {
                TSignedSeqPos estt = exon.GetFrom();
            
                if(margin > 0)
                {
                    curgen.push_back(CExonData(estt-margin,estt-1,-1,-1,left_utr)); 
                    curgen.back().ChainID().insert(lastid); 
                }
                
                for(CCluster::TConstIt it = first->begin(); it != first->end(); ++it)
                {
                    const CAlignVec& algn(*it);

                    if(!algn.Limits().IntersectingWith(ab_limits)) continue;
                    
                    for(int i = (int)algn.size()-1; i >= 0; --i)
                    {
                        if(estt > algn[i].GetTo()) 
                        {
                            int aa = algn[i].GetFrom();
                            int bb = algn[i].GetTo();
                            if(remainb > 0)
                            {
                                string etype;
                                int lf, rf;
                                if(pe->isPlus())     // splitted start/stop
                                {
                                    lf = 0;
                                    rf = remainb-1;
                                    etype = "First_Cds";
                                    if(startcodon) splitted_start = true;
                                }
                                else
                                {
                                    lf = 2;
                                    rf = 3-remainb;
                                    etype = "Last_Cds";
                                }
                                curgen.push_back(CExonData(bb-remainb+1,bb,lf,rf,etype));
                                curgen.back().ChainID().insert(algn.ID()); 
                                bb -= remainb;
                                remainb = 0;
                            }
                            if(aa <= bb)
                            {
                                curgen.push_back(CExonData(aa,bb,-1,-1,left_utr)); 
                                curgen.back().ChainID().insert(algn.ID());
                            } 
                        }
                    }
                }
            }
            
            if(pe->isPlus() && startcodon)
            {
                if(splitted_start)
                {
                    curgen.CDS().push_back('G');
                    curgen.CDS().push_back('T');
                    curgen.CDS().push_back('A');
                }
                else
                {
                    curgen.CDS().push_back(toACGT(*m_seqscr.SeqPtr(estart-1,ePlus)));
                    curgen.CDS().push_back(toACGT(*m_seqscr.SeqPtr(estart-2,ePlus)));
                    curgen.CDS().push_back(toACGT(*m_seqscr.SeqPtr(estart-3,ePlus)));
                }
            }
            
            
        }
        
        CGene& curgen = genes.front();
        for(unsigned int i = 0; i < curgen.size(); ++i)
        {
            CExonData& exon = curgen[i];
            TSignedSeqPos estart = exon.GetFrom();
            TSignedSeqPos estop = exon.GetTo();
            for(CClusterSet::TConstIt cls_it = cls.begin(); cls_it != cls.end(); ++cls_it)
            {
                for(CCluster::TConstIt it = cls_it->begin(); it != cls_it->end(); ++it)
                {
                    const CAlignVec& algn(*it);
                    for(unsigned int j = 0; j < algn.size(); ++j)
                    {
                        TSignedSeqPos astart = max(estart,algn[j].GetFrom());
                        TSignedSeqPos astop = min(estop,algn[j].GetTo());
                        int intersect = astop-astart+1;
                        if(intersect > 0) 
                        {
                            exon.AddSupportedLen( intersect );
            
                            for(int i = 0; i < (int)fs.size(); ++i)
                            {
                                if(fs[i].IsDeletion() && (fs[i].Loc() == astart || fs[i].Loc() == astop+1))
                                {
                                    exon.AddSupportedLen( fs[i].Len() );
                                }
                                else if(fs[i].Loc() <= astop && fs[i].Loc() > astart) 
                                {
                                    exon.AddSupportedLen( (fs[i].IsDeletion() ? fs[i].Len() : -fs[i].Len()) );
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    for(list<CGene>::iterator it = genes.begin(); it != genes.end(); ++it)
    {
        CGene& g(*it);
        reverse(g.begin(),g.end());
        
        if(g.Strand() == ePlus) 
        {
            reverse(g.CDS().begin(),g.CDS().end());
        }
        else
        {
            for(int i = 0; i < (int)g.CDS().size(); ++i)
            {
                g.CDS()[i] = Complement(g.CDS()[i]);
            }
        }
        
    }

    return genes;
}

void CParse::PrintInfo() const
{
    vector<const CHMM_State*> states;
    for(const CHMM_State* p = Path(); p != 0; p = p->LeftState()) states.push_back(p);
    reverse(states.begin(),states.end());

    Out(" ",15);
    Out("Start",11);
    Out("Stop",11);
    Out("Score",8);
    Out("BrScr",8);
    Out("LnScr",8);
    Out("RgnScr",8);
    Out("TrmScr",8);
    Out("AccScr",8);
    cout << endl;
    
    for(int i = 0; i < (int)states.size(); ++i)
    {
        const CHMM_State* p = states[i];
        if(dynamic_cast<const CIntergenic*>(p)) cout << endl;
        
        Out(p->GetStateName(),13);
        Out(p->isPlus() ? '+' : '-',2);
        int a = m_seqscr.SeqMap(p->Start(),CSeqScores::eNoMove);
        int b = m_seqscr.SeqMap(p->Stop(),CSeqScores::eNoMove);
        Out(a,11);
        Out(b,11);
        SStateScores sc = p->GetStateScores();
        Out(sc.m_score,8);
        Out(sc.m_branch,8);
        Out(sc.m_length,8);
        Out(sc.m_region,8);
        Out(sc.m_term,8);
        Out(p->Score(),8);
        cout << endl;
    }
}


END_SCOPE(gnomon)
END_NCBI_SCOPE
