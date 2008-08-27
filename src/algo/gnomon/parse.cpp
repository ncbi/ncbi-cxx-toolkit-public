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
#include <objects/general/Object_id.hpp>
#include "hmm.hpp"
#include "parse.hpp"
#include "hmm_inlines.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

USING_SCOPE(objects);

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
    
    int protnum = right.GetSeqScores().ProtNumber(left.Stop(),right.Stop());
    
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
         double scr = score-protnum*right.GetSeqScores().MultiProtPenalty()+left.Score();
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
    if(!s_EvaluateNewScore(left,right,score,openrgn, rightanchor)) return false;
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
inline void s_MakeStep(const CSeqScores& seqscr, vector<L>& lvec, vector<CIntergenic>& rvec, bool rightanchor = false)
{
    if(lvec.empty()) return;
    CIntergenic& right = rvec.back();
    int i = lvec.size()-1;
    int rlimit = right.Stop();
    if(lvec[i].Stop() == rlimit) --i;
    while(i >= 0 && lvec[i].Stop() >= seqscr.LeftAlignmentBoundary(right.Stop())) --i;  // no intergenics in alignment
    int nearlimit = max(0,rlimit-kTooFarLen);
    while(i >= 0 && lvec[i].Stop() >= nearlimit) 
    {
        if(!s_ForwardStep(lvec[i--],right, rightanchor)) return;
    }
    if(i < 0) return;
    for(const L* p = &lvec[i]; p !=0; p = p->PrevExon())
    {
        if(!s_ForwardStep(*p,right, rightanchor)) return;
    }
}

inline void s_MakeStep(const CSeqScores& seqscr, const CExonParameters& exon_params, EStrand strand, int point, vector<CIntergenic>& lvec, vector<CSingleExon>& rvec)            // L - intergenic, R - single exon
{
    rvec.push_back(CSingleExon(strand, point, seqscr, exon_params));
    s_MakeStep(lvec, rvec, 0, -1);
    if(rvec.back().Score() == BadScore()) rvec.pop_back();
}

template<class R> 
inline void s_MakeStep(const CSeqScores& seqscr, const CExonParameters& exon_params, EStrand strand, int point, vector<CIntergenic>& lvec, vector<R> rvec[][3])                 // L - intergenic, R - first/last exon
{
    for(int kr = 0; kr < 2; ++kr)
    {
        for(int phr = 0; phr < 3; ++phr)
        {
            rvec[kr][phr].push_back(R(strand,phr,point,seqscr,exon_params));
            s_MakeStep(lvec, rvec[kr][phr], 0, kr);
            if(rvec[kr][phr].back().Score() == BadScore()) rvec[kr][phr].pop_back();
        }
    }
}

template<class R> 
inline void s_MakeStep(const CSeqScores& seqscr, const CExonParameters& exon_params, EStrand strand, int point, vector<CIntron> lvec[][3], vector<R>& rvec)                 // L - intron R - first/last exon
{
    rvec.push_back(R(strand,0,point,seqscr,exon_params));          // phase is bogus, it will be redefined in constructor
    for(int kl = 0; kl < 2; ++kl)
    {
        for(int phl = 0; phl < 3; ++phl)
        {
            s_MakeStep(lvec[kl][phl], rvec, kl, -1);
        }
    }
    if(rvec.back().Score() == BadScore()) rvec.pop_back();
}

inline void s_MakeStep(const CSeqScores& seqscr, const CExonParameters& exon_params, EStrand strand, int point, vector<CIntron> lvec[][3], vector<CInternalExon> rvec[][3])   // L - intron, R - internal exon
{
    for(int phr = 0; phr < 3; ++phr)
    {
        for(int kr = 0; kr < 2; ++kr)
        {
            rvec[kr][phr].push_back(CInternalExon(strand,phr,point,seqscr,exon_params));
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
inline void s_MakeStep(const CSeqScores& seqscr, const CIntronParameters& intron_params, EStrand strand, int point, vector<L1> lvec1[][3], vector<L2> lvec2[][3], vector<CIntron> rvec[][3], int shift)    // L1,L2 - exons, R -intron
{
    for(int k = 0; k < 2; ++k)
    {
        for(int phl = 0; phl < 3; ++phl)
        {
            int phr = (shift+phl)%3;
            rvec[k][phr].push_back(CIntron(strand,phr,point,seqscr,intron_params));
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

CParse::CParse(const CSeqScores& ss,
               const CIntronParameters&     intron_params,
               const CIntergenicParameters& intergenic_params,
               const CExonParameters&       exon_params,
               bool leftanchor, bool rightanchor)
    : m_seqscr(ss)
{
    try
    {
        m_igplus.reserve(m_seqscr.StartNumber(ePlus)+1);
        m_igminus.reserve(m_seqscr.StopNumber(eMinus)+1);
        m_feminus.reserve(m_seqscr.StartNumber(eMinus)+1);
        m_leplus.reserve(m_seqscr.StopNumber(ePlus)+1);
        m_seplus.reserve(m_seqscr.StopNumber(ePlus)+2);
        m_seminus.reserve(m_seqscr.StartNumber(eMinus)+1);
        for(int k = 0; k < 2; ++k)
        {
            for(int phase = 0; phase < 3; ++phase)
            {
                m_inplus[k][phase].reserve(m_seqscr.AcceptorNumber(ePlus)+1);
                m_inminus[k][phase].reserve(m_seqscr.DonorNumber(eMinus)+1);
                m_feplus[k][phase].reserve(m_seqscr.DonorNumber(ePlus)+1);
                m_ieplus[k][phase].reserve(m_seqscr.DonorNumber(ePlus)+1);
                m_ieminus[k][phase].reserve(m_seqscr.AcceptorNumber(eMinus)+1);
                m_leminus[k][phase].reserve(m_seqscr.AcceptorNumber(eMinus)+1);
            }
        }
    }
    catch(bad_alloc)
    {
        NCBI_THROW(CGnomonException, eMemoryLimit, "Not enough memory in CParse");
    }
    
    int len = m_seqscr.SeqLen();
    
    double BigScore = 10000;
    if (leftanchor) {
        m_seplus.push_back(CSingleExon(ePlus,0,m_seqscr,exon_params));
        m_seplus.back().UpdateScore(BigScore);               // this is a bogus anchor to unforce intergenic start at 0
    }

    for(int i = 0; i < len; ++i)
    {
        if(m_seqscr.AcceptorScore(i,ePlus) != BadScore())
        {
            s_MakeStep(m_seqscr, intron_params, ePlus,i,m_ieplus,m_feplus,m_inplus,1);
        }

        if(m_seqscr.AcceptorScore(i,eMinus) != BadScore())
        {
            s_MakeStep(m_seqscr, exon_params, eMinus,i,m_inminus,m_ieminus);
            s_MakeStep(m_seqscr, exon_params, eMinus,i,m_igminus,m_leminus);
        }

        if(m_seqscr.DonorScore(i,ePlus) != BadScore())
        {
            s_MakeStep(m_seqscr, exon_params, ePlus,i,m_inplus,m_ieplus);
            s_MakeStep(m_seqscr, exon_params, ePlus,i,m_igplus,m_feplus);
        }

        if(m_seqscr.DonorScore(i,eMinus) != BadScore())
        {
            s_MakeStep(m_seqscr, intron_params, eMinus,i,m_leminus,m_ieminus,m_inminus,0);
        }

        if(m_seqscr.StartScore(i,ePlus) != BadScore())
        {
            m_igplus.push_back(CIntergenic(ePlus,i,m_seqscr,intergenic_params));
            s_MakeStep(m_seqscr, m_seplus,m_igplus);
            s_MakeStep(m_seqscr, m_seminus,m_igplus);
            s_MakeStep(m_seqscr, m_leplus,m_igplus);
            s_MakeStep(m_seqscr, m_feminus,m_igplus);
        }

        if(m_seqscr.StartScore(i,eMinus) != BadScore())
        {
            s_MakeStep(m_seqscr, exon_params, eMinus,i,m_inminus,m_feminus);
            s_MakeStep(m_seqscr, exon_params, eMinus,i,m_igminus,m_seminus);
        }

        if(m_seqscr.StopScore(i,ePlus) != BadScore())
        {
            s_MakeStep(m_seqscr, exon_params, ePlus,i,m_inplus,m_leplus);
            s_MakeStep(m_seqscr, exon_params, ePlus,i,m_igplus,m_seplus);
        }

        if(m_seqscr.StopScore(i,eMinus) != BadScore())
        {
            m_igminus.push_back(CIntergenic(eMinus,i,m_seqscr,intergenic_params));
            s_MakeStep(m_seqscr, m_seplus,m_igminus);
            s_MakeStep(m_seqscr, m_seminus,m_igminus);
            s_MakeStep(m_seqscr, m_leplus,m_igminus);
            s_MakeStep(m_seqscr, m_feminus,m_igminus);
        }
    }

    m_igplus.push_back(CIntergenic(ePlus,-1,m_seqscr,intergenic_params));                // never is popped    
    s_MakeStep(m_seqscr, m_seplus,m_igplus,rightanchor);
    s_MakeStep(m_seqscr, m_seminus,m_igplus,rightanchor);
    s_MakeStep(m_seqscr, m_leplus,m_igplus,rightanchor);
    s_MakeStep(m_seqscr, m_feminus,m_igplus,rightanchor);

    m_igminus.push_back(CIntergenic(eMinus,-1,m_seqscr,intergenic_params));               // never is popped
    s_MakeStep(m_seqscr, m_seplus,m_igminus,rightanchor);
    s_MakeStep(m_seqscr, m_seminus,m_igminus,rightanchor);
    s_MakeStep(m_seqscr, m_leplus,m_igminus,rightanchor);
    s_MakeStep(m_seqscr, m_feminus,m_igminus,rightanchor);
    
    m_igplus.back().UpdateScore(AddProbabilities(m_igplus.back().Score(),m_igminus.back().Score()));

    const CHMM_State* p = &m_igplus.back();

    if (!rightanchor) {
        s_MakeStep(m_seqscr, intron_params, ePlus,-1,m_ieplus,m_feplus,m_inplus,1);        // may be popped
        s_MakeStep(m_seqscr, intron_params, eMinus,-1,m_leminus,m_ieminus,m_inminus,0);    // may be popped	

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
            if(p->LeftState() == &m_seplus[0])
                const_cast<CHMM_State*>(p)->ClearLeftState();
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

void AddSupport(const TGeneModelList& align_list, TSignedSeqRange inside_range, CGeneModel& gene, TSignedSeqRange reading_frame)
{
    CGeneModel support;
    support.SetStrand(gene.Strand());
    const CGeneModel* supporting_align = NULL;

    ITERATE(TGeneModelList, it, align_list) {
        const CGeneModel& algn(*it);
            
        if ((algn.Type() & (CGeneModel::eWall | CGeneModel::eNested))!=0) continue;
        if(!Include(inside_range,algn.MaxCdsLimits())) continue;
            
        if ((algn.ReadingFrame().Empty()?algn.Limits():algn.ReadingFrame()).IntersectingWith(reading_frame)) {
            support.Extend(algn, false);
            support.Support().insert(CSupportInfo(algn.Seqid(),false));
            supporting_align = &algn;

            if((algn.Status()&CGeneModel::ePolyA) != 0)
                gene.Status() |= CGeneModel::ePolyA; 
        }
    }

    if (!support.Exons().empty()) {
        _ASSERT( supporting_align != NULL );
        gene.FrameShifts() = support.FrameShifts();
        CCDSInfo cds_info;
        cds_info.SetReadingFrame(reading_frame);
        gene.SetCdsInfo(cds_info);

        gene.Extend(support, support.Continuous());
        gene.Support() = support.Support();
        if ( Include(support.Limits(),gene.Limits()) && support.Continuous() ) {
            if (gene.Support().size()==1)
                gene = *supporting_align;
            gene.Status() |= CGeneModel::eFullSupCDS;
        }
    }
#ifdef _DEBUG
    ITERATE(TGeneModelList, it, align_list) {
        const CGeneModel& algn(*it);
            
        if ((algn.Type() & (CGeneModel::eWall | CGeneModel::eNested))!=0) continue;
        if(!Include(inside_range,algn.MaxCdsLimits())) continue;
            
        if ((algn.ReadingFrame().Empty()?algn.Limits():algn.ReadingFrame()).IntersectingWith(reading_frame)) {
            _ASSERT( algn.isCompatible(gene) );
        }
    }
#endif
}

list<CGeneModel> CParse::GetGenes() const
{
    list<CGeneModel> genes;
    vector< vector<const CExon*> > gen_exons;
    const CAlignMap& seq_map = m_seqscr.FrameShiftedSeqMap();
    
    if(dynamic_cast<const CIntron*>(Path()) && Path()->LeftState())
        gen_exons.push_back(vector<const CExon*>());

    for(const CHMM_State* p = Path(); p != 0; p = p->LeftState())
        if(p->isExon()) {
            if(p->isGeneRightEnd())
                gen_exons.push_back(vector<const CExon*>());
            
            gen_exons.back().push_back(static_cast<const CExon*>(p));
        }
    
    ITERATE(vector< vector<const CExon*> >, g_it, gen_exons) {
        EStrand strand = g_it->back()->Strand();
        genes.push_front(CGeneModel(strand,0,CGeneModel::eGnomon));
        CGeneModel& gene = genes.front();

        double score = 0;

        CGeneModel local_gene(strand);
        for (vector<const CExon*>::const_reverse_iterator e_it = g_it->rbegin(); e_it != g_it->rend(); ++e_it) {
            const CExon* pe = *e_it;

            TSignedSeqRange local_exon(pe->Start(),pe->Stop());
            local_gene.AddExon(local_exon);
            TSignedSeqRange exon = seq_map.MapRangeEditedToOrig(local_exon);
            gene.AddExon(exon);

            score += pe->RgnScore()+pe->ExonScore();
        }
        CAlignMap localmap(local_gene.Exons(), local_gene.FrameShifts(), local_gene.Strand());

        TSignedSeqRange local_reading_frame(g_it->back()->Start(), g_it->front()->Stop());

        if (!g_it->back()->isGeneLeftEnd()) {
            const CExon* pe = g_it->back();
            int estart = pe->Start();
            int estop = pe->Stop();
            int rframe = pe->Phase();
            int lframe = pe->isPlus() ? (rframe-(estop-estart))%3 : (rframe+(estop-estart))%3;
            if (lframe < 0)
                lframe += 3;
            int del = pe->isPlus() ? (3-lframe)%3 : (1+lframe)%3;
            
            if(localmap.FShiftedLen(local_reading_frame) <= del) {
                genes.pop_front();
                continue;
            }

            local_reading_frame.SetFrom(localmap.FShiftedMove(local_reading_frame.GetFrom(),del));    // do it hard way in case we will get into next exon
        }
        if (!g_it->front()->isGeneRightEnd()) {
            const CExon* pe = g_it->front();
            int rframe = pe->Phase();
            int del = pe->isPlus() ? (1+rframe)%3 : (3-rframe)%3;
            
            if(localmap.FShiftedLen(local_reading_frame) <= del) {
                genes.pop_front();
                continue;
            }

            local_reading_frame.SetTo(localmap.FShiftedMove(local_reading_frame.GetTo(),-del));      // do it hard way in case we will get into next exon
        }

        if (local_reading_frame.Empty()) {
            genes.pop_front();
            continue;
        }
        
        TSignedSeqRange reading_frame = seq_map.MapRangeEditedToOrig(local_reading_frame, true);
        _ASSERT(reading_frame.NotEmpty() && reading_frame.GetFrom() >= 0 && reading_frame.GetTo() >= 0);
        _ASSERT(gene.Limits().GetFrom() <= reading_frame.GetFrom() && gene.FShiftedLen(gene.Limits().GetFrom(),reading_frame.GetFrom(),false) <=3);
        _ASSERT(gene.Limits().GetTo() >= reading_frame.GetTo() && gene.FShiftedLen(reading_frame.GetTo(),gene.Limits().GetTo(),false) <= 3);

        AddSupport(m_seqscr.Alignments(),
                   TSignedSeqRange(m_seqscr.From(),m_seqscr.To()),
                   gene, reading_frame);

        TSignedSeqRange start, stop;
        CAlignMap gene_map = gene.GetAlignMap();
        if (g_it->back()->isGeneLeftEnd()) {
            int utr_len = gene_map.FShiftedLen(TSignedSeqRange(gene.Limits().GetFrom(),reading_frame.GetFrom()), CAlignMap::eSinglePoint, CAlignMap::eLeftEnd) - 1; 
            if(utr_len > 0 || m_seqscr.isReadingFrameLeftEnd(local_reading_frame.GetFrom(),strand)) {                                       // extend gene only if start/stop is conventional
                if (utr_len < 3 ) {
                    gene.ExtendLeft(3-utr_len);
                    gene_map = gene.GetAlignMap();
                }
                
                TSignedSeqRange rf = gene_map.MapRangeOrigToEdited(reading_frame, true);
                TSignedSeqRange stt(rf.GetFrom()-3,rf.GetFrom()-1);
                TSignedSeqRange stp(rf.GetTo()+1,rf.GetTo()+3);
                if (strand == eMinus)
                    swap(stt,stp);
                start = gene_map.MapRangeEditedToOrig(stt, false);
            }
        }

        if (g_it->front()->isGeneRightEnd()) {
            int utr_len = gene_map.FShiftedLen(TSignedSeqRange(reading_frame.GetTo(),gene.Limits().GetTo()), CAlignMap::eRightEnd, CAlignMap::eSinglePoint) - 1; 
            if(utr_len > 0 || m_seqscr.isReadingFrameRightEnd(local_reading_frame.GetTo(),strand)) {                                        // extend gene only if start/stop is conventional
                if (utr_len < 3 ) {
                    gene.ExtendRight(3-utr_len);
                    gene_map = gene.GetAlignMap();
                }
                
                CAlignMap gene_map = gene.GetAlignMap();
                TSignedSeqRange rf = gene_map.MapRangeOrigToEdited(reading_frame, true);
                TSignedSeqRange stt(rf.GetFrom()-3,rf.GetFrom()-1);
                TSignedSeqRange stp(rf.GetTo()+1,rf.GetTo()+3);
                if (strand == eMinus)
                    swap(stt,stp);
                stop = gene_map.MapRangeEditedToOrig(stp, false);
            }
        }

        if (strand == eMinus)
            swap(start,stop);

        CCDSInfo cds_info;
        if (gene.GetCdsInfo().ProtReadingFrame().NotEmpty())
            cds_info.SetReadingFrame(gene.GetCdsInfo().ProtReadingFrame(), true);
        cds_info.SetReadingFrame(reading_frame);
        if (start.NotEmpty())
            cds_info.SetStart(start, gene.ConfirmedStart() && start == gene.GetCdsInfo().Start());
        if (stop.NotEmpty())
            cds_info.SetStop(stop, gene.ConfirmedStop());

        ITERATE(CCDSInfo::TPStops,s,gene.GetCdsInfo().PStops())
            cds_info.AddPStop(*s);

        cds_info.SetScore(score);

        gene.SetCdsInfo(cds_info);
    }
    
    return genes;
}

void CParse::PrintInfo() const
{
    vector<const CHMM_State*> states;
    for(const CHMM_State* p = Path(); p != 0; p = p->LeftState()) states.push_back(p);
    reverse(states.begin(),states.end());
    const CAlignMap& seq_map = m_seqscr.FrameShiftedSeqMap();

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
        int a = seq_map.MapEditedToOrig(p->Start());
        int b = seq_map.MapEditedToOrig(p->Stop());
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
