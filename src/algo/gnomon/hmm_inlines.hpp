#ifndef ALGO_GNOMON___HMM_INLINES__HPP
#define ALGO_GNOMON___HMM_INLINES__HPP

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
#include <algo/gnomon/gnomon_exception.hpp>
#include "score.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

template<int order> void CMarkovChain<order>::InitScore(CNcbiIstream& from)
{
    Init(from);
    if(from) toScore();
}

template<int order> void CMarkovChain<order>::Init(CNcbiIstream& from)
{
    m_next[enA].Init(from);
    m_next[enC].Init(from);
    m_next[enG].Init(from);
    m_next[enT].Init(from);
    if(from) m_next[enN].Average(m_next[enA],m_next[enC],m_next[enG],m_next[enT]);
}

template<int order> void CMarkovChain<order>::Average(Type& mc0, Type& mc1, Type& mc2, Type& mc3)
{
    m_next[enA].Average(mc0.m_next[enA],mc1.m_next[enA],mc2.m_next[enA],mc3.m_next[enA]);
    m_next[enC].Average(mc0.m_next[enC],mc1.m_next[enC],mc2.m_next[enC],mc3.m_next[enC]);
    m_next[enG].Average(mc0.m_next[enG],mc1.m_next[enG],mc2.m_next[enG],mc3.m_next[enG]);
    m_next[enT].Average(mc0.m_next[enT],mc1.m_next[enT],mc2.m_next[enT],mc3.m_next[enT]);
    m_next[enN].Average(m_next[enA],m_next[enC],m_next[enG],m_next[enT]);
}

template<int order> void CMarkovChain<order>::toScore()
{
    m_next[enA].toScore();
    m_next[enC].toScore();
    m_next[enG].toScore();
    m_next[enT].toScore();
    m_next[enN].toScore();
}

template<int order> double CMarkovChainArray<order>::Score(const EResidue* seq) const
{
    double score = 0;
    for(int i = 0; i < m_length; ++i)
    {
        double s = m_mc[i].Score(seq+i);
        if(s == BadScore()) return BadScore();
        score += s;
    }
    return score;
}

template<int order> void CMarkovChainArray<order>::InitScore(int l, CNcbiIstream& from)
{
    m_length = l;
    m_mc.resize(m_length);
    for(int i = 0; i < m_length; ++i) m_mc[i].InitScore(from);
}

template<int order>
double CWAM_Donor<order>::Score(const CEResidueVec& seq, int i) const
{
    int first = i-m_left+1;
    int last = i+m_right;
    if(first-order < 0 || last >= (int)seq.size()) return BadScore();    // out of range
    if(seq[i+1] != enG || (seq[i+2] != enT && seq[i+2] != enC)) return BadScore();   // no GT/GC
//    if(seq[i+1] != enG || seq[i+2] != enT) return BadScore();   // no GT
    
    return m_matrix.Score(&seq[first]);
}

template<int order>
CWAM_Donor<order>::CWAM_Donor(CNcbiIstream& from)
{

    string str;
    from >> str;
    if(str != "InExon:") Error(class_id());
    from >> m_inexon;
    if(!from) Error(class_id());
    from >> str;
    if(str != "InIntron:") Error(class_id());
    from >> m_inintron;
    if(!from) Error(class_id());
    
    m_left = m_inexon;
    m_right = m_inintron;
    
    m_matrix.InitScore(m_inexon+m_inintron,from);
    if(!from) Error(class_id());
}


template<int order>
double CWAM_Acceptor<order>::Score(const CEResidueVec& seq, int i) const
{
    int first = i-m_left+1;
    int last = i+m_right;
    if(first-order < 0 || last >= (int)seq.size()) return BadScore();  // out of range
    if(seq[i-1] != enA || seq[i] != enG) return BadScore();   // no AG
    
    return m_matrix.Score(&seq[first]);
}

template<int order>
CWAM_Acceptor<order>::CWAM_Acceptor(CNcbiIstream& from)
{
    string str;
    from >> str;
    if(str != "InExon:") Error(class_id());
    from >> m_inexon;
    if(!from) Error(class_id());
    from >> str;
    if(str != "InIntron:") Error(class_id());
    from >> m_inintron;
    if(!from) Error(class_id());
    
    m_left = m_inintron;
    m_right = m_inexon;
    
    m_matrix.InitScore(m_inexon+m_inintron,from);
    if(!from) Error(class_id());
}


template<int order>
CMC3_CodingRegion<order>::CMC3_CodingRegion(CNcbiIstream& from)
{
    m_matrix[0].InitScore(from);
    m_matrix[1].InitScore(from);
    m_matrix[2].InitScore(from);
    if(!from) Error(class_id());
}

template<int order>
double CMC3_CodingRegion<order>::Score(const CEResidueVec& seq, int i, int codonshift) const
{
    if(i-order < 0) return BadScore();  // out of range
//    else if(seq[i] == enN) return -2.;  // discourage making exons of Ns
    else return m_matrix[codonshift].Score(&seq[i]);
}

template<int order>
CMC_NonCodingRegion<order>::CMC_NonCodingRegion(CNcbiIstream& from)
{
    m_matrix.InitScore(from);
    if(!from) Error(class_id());
}

template<int order>
double CMC_NonCodingRegion<order>::Score(const CEResidueVec& seq, int i) const
{
    if(i-order < 0) return BadScore();  // out of range
    return m_matrix.Score(&seq[i]);
}

template<class State> void EvaluateInitialScore(State& r)
{
    int len = r.Stop()-r.Start()+1;
    if(len >= r.MaxLen() || r.StopInside()) return;   // >= makes sence here
    
    int minlen = 1;
    /*
    if(!r.NoRightEnd())
    {
        if(r.isPlus()) minlen += r.TerminalPtr()->Left();
        else  minlen += r.TerminalPtr()->Right();
    }
    */
    if(len < minlen) return;   // states can go beyon the start
                               // so we shouldn't enforce MinLen

    double score;
    if(r.NoRightEnd()) 
    {
        score = r.ThroughLengthScore();
    }
    else 
    {
        score = r.InitialLengthScore();
    }
    if(score == BadScore()) return;

    double scr;
    scr = r.RgnScore();
    if(scr == BadScore()) return;
    score += scr;

    if(!r.NoRightEnd())
    {
        scr = r.TermScore();
        if(scr == BadScore()) return;
        score += scr;
    }

    if(r.OpenRgn()) r.UpdateScore(score);
}

END_SCOPE(gnomon)
END_NCBI_SCOPE

#endif  // ALGO_GNOMON___HMM_INLINES__HPP
