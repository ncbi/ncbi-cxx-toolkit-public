#ifndef __MODELS__HPP
#define __MODELS__HPP

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

BEGIN_NCBI_SCOPE


template<int order> void MarkovChain<order>::InitScore(ifstream& from)
{
    Init(from);
    if(from) toScore();
}

template<int order> void MarkovChain<order>::Init(ifstream& from)
{
    next[nA].Init(from);
    next[nC].Init(from);
    next[nG].Init(from);
    next[nT].Init(from);
    if(from) next[nN].Average(next[nA],next[nC],next[nG],next[nT]);
}

template<int order> void MarkovChain<order>::Average(Type& mc0, Type& mc1, Type& mc2, Type& mc3)
{
    next[nA].Average(mc0.next[nA],mc1.next[nA],mc2.next[nA],mc3.next[nA]);
    next[nC].Average(mc0.next[nC],mc1.next[nC],mc2.next[nC],mc3.next[nC]);
    next[nG].Average(mc0.next[nG],mc1.next[nG],mc2.next[nG],mc3.next[nG]);
    next[nT].Average(mc0.next[nT],mc1.next[nT],mc2.next[nT],mc3.next[nT]);
    next[nN].Average(next[nA],next[nC],next[nG],next[nT]);
}

template<int order> void MarkovChain<order>::toScore()
{
    next[nA].toScore();
    next[nC].toScore();
    next[nG].toScore();
    next[nT].toScore();
    next[nN].toScore();
}

template<int order> inline double MarkovChainArray<order>::Score(const SeqType* seq) const
{
    double score = 0;
    for(int i = 0; i < length; ++i)
    {
        double s = mc[i].Score(seq+i);
        if(s == BadScore) return BadScore;
        score += s;
    }
    return score;
}

template<int order> void MarkovChainArray<order>::InitScore(int l, ifstream& from)
{
    length = l;
    mc.resize(length);
    for(int i = 0; i < length; ++i) mc[i].InitScore(from);
}

template<int order>
double WAM_Donor<order>::Score(const CVec& seq, int i) const
{
    int first = i-left+1;
    int last = i+right;
    if(first-order < 0 || last >= (int)seq.size()) return BadScore;    // out of range
    if(seq[i+1] != nG || (seq[i+2] != nT && seq[i+2] != nC)) return BadScore;   // no GT/GC
    //	if(seq[i+1] != nG || seq[i+2] != nT) return BadScore;   // no GT

    return matrix.Score(&seq[first]);
}

template<int order>
WAM_Donor<order>::WAM_Donor(const string& file, int cgcontent)
{
    CNcbiOstrstream ost;
    ost << "[WAM_Donor_" << order << "]";
    string label = ost.str();
    ifstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label);

    string str;
    from >> str;
    if(str != "InExon:") Error(label);
    from >> inexon;
    if(!from) Error(label);
    from >> str;
    if(str != "InIntron:") Error(label);
    from >> inintron;
    if(!from) Error(label);

    left = inexon;
    right = inintron;

    matrix.InitScore(inexon+inintron,from);
    if(!from) Error(label);
}


template<int order>
double WAM_Acceptor<order>::Score(const CVec& seq, int i) const
{
    int first = i-left+1;
    int last = i+right;
    if(first-order < 0 || last >= (int)seq.size()) return BadScore;  // out of range
    if(seq[i-1] != nA || seq[i] != nG) return BadScore;   // no AG

    return matrix.Score(&seq[first]);
}

template<int order>
WAM_Acceptor<order>::WAM_Acceptor(const string& file, int cgcontent)
{
    CNcbiOstrstream ost;
    ost << "[WAM_Acceptor_" << order << "]";
    string label = ost.str();
    ifstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label);

    string str;
    from >> str;
    if(str != "InExon:") Error(label);
    from >> inexon;
    if(!from) Error(label);
    from >> str;
    if(str != "InIntron:") Error(label);
    from >> inintron;
    if(!from) Error(label);

    left = inintron;
    right = inexon;

    matrix.InitScore(inexon+inintron,from);
    if(!from) Error(label);
}


template<int order>
MC3_CodingRegion<order>::MC3_CodingRegion(const string& file, int cgcontent)
{
    CNcbiOstrstream ost;
    ost << "[MC3_CodingRegion_" << order << "]";
    string label = ost.str();
    ifstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label);

    matrix[0].InitScore(from);
    matrix[1].InitScore(from);
    matrix[2].InitScore(from);
    if(!from) Error(label);
}

template<int order>
double MC3_CodingRegion<order>::Score(const CVec& seq, int i, int codonshift) const
{
    if(i-order < 0) return BadScore;  // out of range
    else if(seq[i] == nN) return -2.;  // discourage making exons of Ns
    else return matrix[codonshift].Score(&seq[i]);
}

template<int order>
MC_NonCodingRegion<order>::MC_NonCodingRegion(const string& file, int cgcontent)
{
    CNcbiOstrstream ost;
    ost << "[MC_NonCodingRegion_" << order << "]";
    string label = ost.str();
    ifstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label);

    matrix.InitScore(from);
    if(!from) Error(label);
}

template<int order>
double MC_NonCodingRegion<order>::Score(const CVec& seq, int i) const
{
    if(i-order < 0) return BadScore;  // out of range
    return matrix.Score(&seq[i]);
}

inline bool SeqScores::isStart(int i, int strand) const
{
    const CVec& ss = seq[strand];
    int ii = (strand == Plus) ? i : SeqLen()-1-i;
    if(ii < 0 || ii+2 >= SeqLen()) return false;  //out of range
    else if(ss[ii] != nA || ss[ii+1] != nT || ss[ii+2] != nG) return false;
    else return true;
}

inline bool SeqScores::isStop(int i, int strand) const
{
    const CVec& ss = seq[strand];
    int ii = (strand == Plus) ? i : SeqLen()-1-i;
    if(ii < 0 || ii+2 >= SeqLen()) return false;  //out of range
    if((ss[ii] != nT || ss[ii+1] != nA || ss[ii+2] != nA) &&
       (ss[ii] != nT || ss[ii+1] != nA || ss[ii+2] != nG) &&
       (ss[ii] != nT || ss[ii+1] != nG || ss[ii+2] != nA)) return false;
    else return true;
}

inline bool SeqScores::isAG(int i, int strand) const
{
    const CVec& ss = seq[strand];
    int ii = (strand == Plus) ? i : SeqLen()-1-i;
    if(ii-1 < 0 || ii >= SeqLen()) return false;  //out of range
    if(ss[ii-1] != nA || ss[ii] != nG) return false;
    else return true;
}

inline bool SeqScores::isGT(int i, int strand) const
{
    const CVec& ss = seq[strand];
    int ii = (strand == Plus) ? i : SeqLen()-1-i;
    if(ii < 0 || ii+1 >= SeqLen()) return false;  //out of range
    if(ss[ii] != nG || ss[ii+1] != nT) return false;
    else return true;
}

inline bool SeqScores::isConsensusIntron(int i, int j, int strand) const
{
    if(strand == Plus) return (dscr[Plus][i-1] != BadScore) && (ascr[Plus][j] != BadScore);
    else return (ascr[Minus][i-1] != BadScore) && (dscr[Minus][j] != BadScore);
    //	if(strand == Plus) return isGT(i,strand) && isAG(j,strand);
    //	else return isAG(i,strand) && isGT(j,strand);
}

inline const SeqType* SeqScores::SeqPtr(int i, int strand) const
{
    const CVec& ss = seq[strand];
    int ii = (strand == Plus) ? i : SeqLen()-1-i;
    return &ss.front()+ii;
}

inline bool SeqScores::StopInside(int a, int b, int strand, int frame) const
{
    return (a <= laststop[strand][frame][b]);
}

inline bool SeqScores::OpenCodingRegion(int a, int b, int strand, int frame) const
{
    return (a > notinexon[strand][frame][b]);
}

inline double SeqScores::CodingScore(int a, int b, int strand, int frame) const
{
    if(a > b) return 0; // for splitted start/stop
    double score = cdrscr[strand][frame][b];
    if(a > 0) score -= cdrscr[strand][frame][a-1];
    return score;
}

inline bool SeqScores::OpenNonCodingRegion(int a, int b, int strand) const
{
    return (a > notinintron[strand][b]);
}

inline double SeqScores::NonCodingScore(int a, int b, int strand) const
{
    double score = ncdrscr[strand][b];
    if(a > 0) score -= ncdrscr[strand][a-1];
    return score;
}

inline bool SeqScores::OpenIntergenicRegion(int a, int b) const
{
    return (a > notining[b]);
} 

inline double SeqScores::IntergenicScore(int a, int b, int strand) const
{
    double score = ingscr[strand][b];
    if(a > 0) score -= ingscr[strand][a-1];
    return score;
}

inline int SeqScores::SeqMap(int i, bool forwrd) const 
{ 
    int l = seq_map[i];
    if(l < 0)
    {
        if(forwrd) return seq_map[i+1];
        else return seq_map[i-1];
    }
    else return l;
}

inline int SeqScores::RevSeqMap(int i, bool forwrd) const 
{ 
    int l = rev_seq_map[i-shift];
    if(l < 0)
    {
        if(forwrd) return rev_seq_map[i-shift+1];
        else return rev_seq_map[i-shift-1];
    }
    else return l;
}


inline int ACGT(SeqType c)
{
    switch(c)
    {
    case 'A': 
    case 'a': 
        return nA;
    case 'C': 
    case 'c': 
        return nC;
    case 'G': 
    case 'g': 
        return nG;
    case 'T': 
    case 't': 
        return nT;
    default:
        return nN;
    }
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/07/28 12:33:19  dicuccio
 * Sync with Sasha's working tree
 *
 * Revision 1.1  2003/10/24 15:07:25  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // __MODELS__HPP
