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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include "gene_finder.hpp"


BEGIN_NCBI_SCOPE


const double BadScore    = -numeric_limits<double>::max();
const double LnHalf      = log(0.5);
const double LnThree     = log(3.);
const SeqType toMinus[5] = { nT, nG, nC, nA, nN };
const int TooFarLen      = 500;
const char* aa_table     = "KNKNXTTTTTRSRSXIIMIXXXXXXQHQHXPPPPPRRRRRLLLLLXXXXXEDEDXAAAAAGGGGGVVVVVXXXXX*Y*YXSSSSS*CWCXLFLFXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

void MarkovChain<0>::InitScore(ifstream& from)
{
    Init(from);
    if(from) toScore();
}

void MarkovChain<0>::Init(ifstream& from)
{
    from >> score[nA];
    from >> score[nC];
    from >> score[nG];
    from >> score[nT];
    score[nN] = 0.25*(score[nA]+score[nC]+score[nG]+score[nT]);
}

void MarkovChain<0>::Average(Type& mc0, Type& mc1, Type& mc2, Type& mc3)
{
    score[nA] = 0.25*(mc0.score[nA]+mc1.score[nA]+mc2.score[nA]+mc3.score[nA]);
    score[nC] = 0.25*(mc0.score[nC]+mc1.score[nC]+mc2.score[nC]+mc3.score[nC]);
    score[nG] = 0.25*(mc0.score[nG]+mc1.score[nG]+mc2.score[nG]+mc3.score[nG]);
    score[nT] = 0.25*(mc0.score[nT]+mc1.score[nT]+mc2.score[nT]+mc3.score[nT]);
    score[nN] = 0.25*(score[nA]+score[nC]+score[nG]+score[nT]);
}

void MarkovChain<0>::toScore()
{
    score[nA] = (score[nA] <= 0) ? BadScore : log(4*score[nA]);
    score[nC] = (score[nC] <= 0) ? BadScore : log(4*score[nC]);
    score[nG] = (score[nG] <= 0) ? BadScore : log(4*score[nG]);
    score[nT] = (score[nT] <= 0) ? BadScore : log(4*score[nT]);
    score[nN] = (score[nN] <= 0) ? BadScore : log(4*score[nN]);
}

pair<int,int> InputModel::FindContent(ifstream& from, const string& label, int cgcontent)
{
    string str;
    while(from >> str)
    {
        int low,high;
        if(str == label) 
        {
            from >> str;
            if(str != "CG:") return make_pair(-1,-1);
            from >> low >> high;
            if(!from) return make_pair(-1,-1);
            if( cgcontent >= low && cgcontent < high) return make_pair(low,high);
        }
    }
    return make_pair(-1,-1);
}

InputModel::~InputModel() {}

double MDD_Donor::Score(const CVec& seq, int i) const
{
    int first = i-left+1;
    int last = i+right;
    if(first < 0 || last >= (int)seq.size()) return BadScore;    // out of range
    if(seq[i+1] != nG || seq[i+2] != nT) return BadScore;   // no GT

    int mat = 0;
    for(int j = 0; j < (int)position.size(); ++j)
    {
        if(seq[first+position[j]] != consensus[j]) break;
        ++mat;
    }

    return matrix[mat].Score(&seq[first]);
}

MDD_Donor::MDD_Donor(const string& file, int cgcontent)
{
    string label = "[MDD_Donor]";
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

    do
    {
        from >> str;
        if(!from) Error(label);
        if(str == "Position:")
        {
            int i;
            from >> i;
            if(!from) Error(label);
            position.push_back(i);
            from >> str;
            if(!from) Error(label);
            if(str != "Consensus:") Error(label);
            char c;
            from >> c;
            if(!from) Error(label);
            i = ACGT(c);
            if(i == nN) Error(label); 
            consensus.push_back(i);
        }
        matrix.push_back(MarkovChainArray<0>());
        matrix.back().InitScore(inexon+inintron,from);
        if(!from) Error(label);
    }
    while(str != "End");
}

WMM_Start::WMM_Start(const string& file, int cgcontent)
{
    string label = "[WMM_Start]";
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

double WMM_Start::Score(const CVec& seq, int i) const
{
    int first = i-left+1;
    int last = i+right;
    if(first < 0 || last >= (int)seq.size()) return BadScore;  // out of range
    if(seq[i-2] != nA || seq[i-1] != nT || seq[i] != nG) return BadScore; // no ATG

    return matrix.Score(&seq[first]);
}

WAM_Stop::WAM_Stop(const string& file, int cgcontent)
{
    string label = "[WAM_Stop_1]";
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

double WAM_Stop::Score(const CVec& seq, int i) const
{
    int first = i-left+1;
    int last = i+right;
    if(first-1 < 0 || last >= (int)seq.size()) return BadScore;  // out of range
    if((seq[i+1] != nT || seq[i+2] != nA || seq[i+3] != nA) &&
       (seq[i+1] != nT || seq[i+2] != nA || seq[i+3] != nG) &&
       (seq[i+1] != nT || seq[i+2] != nG || seq[i+3] != nA)) return BadScore; // no stop-codon

    return matrix.Score(&seq[first]);
}

SeqScores::SeqScores (Terminal& a, Terminal& d, Terminal& stt, Terminal& stp, 
                      CodingRegion& cr, NonCodingRegion& ncr, NonCodingRegion& ing, CVec& original_sequence, 
                      int from, int to, const CClusterSet& cls, const FrameShifts& initial_fshifts, bool repeats, bool leftwall, 
                      bool rightwall, string cntg)
: acceptor(a), donor(d), start(stt), stop(stp), cdr(cr), ncdr(ncr), intrg(ing), 
cluster_set(cls), fshifts(initial_fshifts), shift(from), contig(cntg)
{
    for(CClusterSet::ConstIt it_cls = cls.begin(); it_cls != cls.end(); ++it_cls)
    {
        if(it_cls->Limits().first <= from || it_cls->Limits().second >= to) continue;

        for(Cluster::ConstIt it = it_cls->begin(); it != it_cls->end(); ++it)
        {
            const AlignVec& algn = *it;
            if(algn.Type() == AlignVec::Prot)
            {
                for(int i = 0; i < (int)algn.size()-1; ++i)
                {
                    int a = algn[i].second+1;
                    int b = algn[i+1].first-1;
                    int len = b-a+1;
                    int res = len%3;
                    if(res < 0) res += 3;
                    int loc = (a+b)/2;
                    if(len < Intron::MinIntron() && res != 0)
                    {
                        if(res == 1) fshifts.push_back(CFrameShiftInfo(loc,false));  // local copy
                        else  fshifts.push_back(CFrameShiftInfo(loc,true,'N'));
                    }
                }
            }
        }
    }
    sort(fshifts.begin(),fshifts.end());
    FrameShifts::iterator fsi = fshifts.begin();
    while(fsi != fshifts.end() && fsi->Loc() < from) ++fsi;

    CVec sequence;
    rev_seq_map.resize(to-from+1);
    for(int i = from; i <= to; ++i)
    {
        if(fsi == fshifts.end() || i != fsi->Loc())
        {
            sequence.push_back(original_sequence[i]);
            rev_seq_map[i-shift] = (int)seq_map.size();
            seq_map.push_back(i);
        }
        else if(fsi->IsInsertion())   // Insertion
        {                                                                            
            sequence.push_back(fsi->InsertValue());
            seq_map.push_back(-1); 
            sequence.push_back(original_sequence[i]);
            rev_seq_map[i-shift] = -1;
            seq_map.push_back(i);
            ++fsi;
        }
        else    // Deletion
        {
            rev_seq_map[i-shift] = -1;
            ++fsi;
        }
    }

    int len = (int)seq_map.size();
    try
    {
        notining.resize(len,-1);
        inalign.resize(len,numeric_limits<int>::max());

        for(int strand = 0; strand < 2; ++strand)
        {
            seq[strand].resize(len);
            ascr[strand].resize(len,BadScore);
            dscr[strand].resize(len,BadScore);
            sttscr[strand].resize(len,BadScore);
            stpscr[strand].resize(len,BadScore);
            ncdrscr[strand].resize(len,BadScore);
            ingscr[strand].resize(len,BadScore);
            notinintron[strand].resize(len,-1);
            for(int frame = 0; frame < 3; ++frame)
            {
                cdrscr[strand][frame].resize(len,BadScore);
                laststop[strand][frame].resize(len,-1);
                notinexon[strand][frame].resize(len,-1);
            }
            for(int ph = 0; ph < 2; ++ph)
            {
                asplit[strand][ph].resize(len,0);
                dsplit[strand][ph].resize(len,0);
            }
        }
    }
    catch(bad_alloc)
    {
        cerr << from << ' ' << to << " Not enough memory in SeqScores\n";
        exit(1);
    } 

    for(int i = 0; i < len; ++i)
    {
        char c = sequence[i];
        if(repeats && c != toupper(c))
        {
            laststop[Plus][0][i] = i;
            laststop[Plus][1][i] = i;
            laststop[Plus][2][i] = i;
            laststop[Minus][0][i] = i;
            laststop[Minus][1][i] = i;
            laststop[Minus][2][i] = i;
        }

        int l = ACGT(c);
        seq[Plus][i] = (SeqType)l;
        seq[Minus][len-1-i] = toMinus[l];
    }

    CClusterSet cls_local(cntg);
    for(CClusterSet::ConstIt it_cls = cls.begin(); it_cls != cls.end(); ++it_cls)
    {
        if(it_cls->Limits().first < from || it_cls->Limits().second > to) continue;

        for(Cluster::ConstIt it = it_cls->begin(); it != it_cls->end(); ++it)
        {
            AlignVec algn(it->Strand(),it->ID(),it->Type());
            if(it->CdsLimits().first >= 0)
            {
                algn.SetCdsLimits(IPair(RevSeqMap(it->CdsLimits().first,true),RevSeqMap(it->CdsLimits().second,false)));
            }
            for(int k = 0; k < (int)it->size(); ++k)
            {
                int a = (*it)[k].first;
                int b = (*it)[k].second;
                a = RevSeqMap(a,true);
                b = RevSeqMap(b,false);
                algn.Insert(IPair(a,b));

                for(int i = a; i <= b; ++i)   // ignoring repeats in alignmnets
                {
                    laststop[Plus][0][i] = -1;
                    laststop[Plus][1][i] = -1;
                    laststop[Plus][2][i] = -1;
                    laststop[Minus][0][i] = -1;
                    laststop[Minus][1][i] = -1;
                    laststop[Minus][2][i] = -1;
                }
            }

            cls_local.InsertAlignment(algn);
        }
    }

    for(CClusterSet::ConstIt it_cls = cls_local.begin(); it_cls != cls_local.end(); ++it_cls)
    {
        for(Cluster::ConstIt it = it_cls->begin(); it != it_cls->end(); ++it)
        {
            const AlignVec& algn(*it);
            int strand = algn.Strand();
            int otherstrand = (strand == Plus) ? Minus : Plus;
            int type = algn.Type();

            if(type != AlignVec::Prot)
            {
                if(strand == Plus)               // accept NONCONSENSUS alignment splices
                {
                    for(int k = 0; k < (int)algn.size(); ++k)
                    {
                        int i = algn[k].first;
                        if(k != 0) ascr[strand][i-1] = 0;
                        i = algn[k].second;
                        if(k != (int)algn.size()-1) dscr[strand][i] = 0;
                    }
                }
                else
                {
                    for(int k = 0; k < (int)algn.size(); ++k)
                    {
                        int i = algn[k].first;
                        if(k != 0) dscr[strand][i-1] = 0;
                        i = algn[k].second;
                        if(k != (int)algn.size()-1) ascr[strand][i] = 0;
                    }
                }

                for(int k = 0; k < (int)algn.size()-1; ++k)
                {
                    int introna = algn[k].second+1;
                    int intronb = algn[k+1].first-1;

                    for(int pnt = introna; pnt <= intronb; ++pnt)
                    {
                        for(int frame = 0; frame < 3; ++frame) 
                            notinexon[strand][frame][pnt] = pnt;
                    }
                }

                for(int k = 0; k < (int)algn.size(); ++k)
                {
                    int exona = algn[k].first;
                    int exonb = algn[k].second;
                    //					if(algn.Type() != AlignVec::RefSeq)
                    //					{
                    //						if(k == 0) exona = exonb;             // for the first and last exons we 
                    //						if(k == algn.size()-1) exonb = exona; // respect only one point for non RefSeq
                    //					}
                    for(int pnt = exona; pnt <= exonb; ++pnt)
                    {
                        notinintron[strand][pnt] = pnt;
                    }
                }
            }
            else
            {
                for(int k = 0; k < (int)algn.size(); ++k)
                {
                    int exona = algn[k].first;
                    int exonb = algn[k].second;

                    int pnt = (exona+exonb)/2;
                    pnt += exonb%3-pnt%3;        // this is the right codon end that was checked when alignments were computed

                    // enforcing stop if present or collapsing to one point
                    if(strand == Plus && k == (int)algn.size()-1 && isStop(exonb+1,strand))
                    {
                        exona = pnt;
                    }
                    else if(strand == Minus && k == 0 && isStop(exona-1,strand))
                    {
                        exonb = pnt;
                    }
                    else
                    {
                        exona = exonb = pnt;
                    }

                    int codon_end = exonb%3;
                    int f_beg = (strand == Plus) ? 2-codon_end : codon_end;
                    for(int pnt = exona; pnt <= exonb; ++pnt)
                    {
                        notinintron[strand][pnt] = pnt;
                        for(int frame = 0; frame < 3; ++frame)
                        {
                            if(frame != f_beg) notinexon[strand][frame][pnt] = pnt;
                        }
                    }
                }
            }

            int a = algn.Limits().first;
            int b = algn.Limits().second;

            for(int i = a; i <= b && i < len-1; ++i)  // first and last points are conserved to deal with intergenics going outside
            {
                if(algn.Type() != AlignVec::Prot) inalign[i] = max(1,a);
                notinintron[otherstrand][i] = i;
                for(int frame = 0; frame < 3; ++frame) notinexon[otherstrand][frame][i] = i;
            }

            if(strand == Plus) a += 3;    // start now is a part of intergenic!!!!
            else b -= 3;

            notining[b] = a;
            if(algn.Type() == AlignVec::Prot)
            {
                for(int i = a; i <= b; ++i) notining[i] = i;
            }
            else if(algn.CdsLimits().first >= 0)
            {
                for(int i = algn.CdsLimits().first; i <= algn.CdsLimits().second; ++i) notining[i] = i;
            }
        }
    }

    const int RepeatMargin = 15;
    for(int i = 0; i < len-1; ++i)  // repeat trimming
    {
        bool rpta = laststop[Plus][0][i] < 0;
        bool rptb = laststop[Plus][0][i+1] < 0;

        if(!rpta && rptb)  // b - first repeat
        {
            int j = i+1;
            for(; j <= min(i+RepeatMargin,len-1);  ++j) 
            {
                laststop[Plus][0][j] = -1;
                laststop[Plus][1][j] = -1;
                laststop[Plus][2][j] = -1;
                laststop[Minus][0][j] = -1;
                laststop[Minus][1][j] = -1;
                laststop[Minus][2][j] = -1;
            }
            i = j-1;
        }
        else if(rpta && !rptb) // a - last repeat
        {
            int j = max(0,i-RepeatMargin+1);
            for(; j <= i; ++j)
            {
                laststop[Plus][0][j] = -1;
                laststop[Plus][1][j] = -1;
                laststop[Plus][2][j] = -1;
                laststop[Minus][0][j] = -1;
                laststop[Minus][1][j] = -1;
                laststop[Minus][2][j] = -1;
            }
        }
    }


    if(leftwall) notinintron[Plus][0] = notinintron[Minus][0] = 0;
    if(rightwall) notinintron[Plus][len-1] = notinintron[Minus][len-1] = len-1;

    for(int strand = 0; strand < 2; ++strand)
    {
        IVec& nin = notinintron[strand];
        for(int i = 1; i < len; ++i) nin[i] = max(nin[i-1],nin[i]);

        for(int frame = 0; frame < 3; ++frame)
        {
            IVec& nex = notinexon[strand][frame];
            for(int i = 1; i < len; ++i) nex[i] = max(nex[i-1],nex[i]); 
        }
    }
    for(int i = 1; i < len; ++i) notining[i] = max(notining[i-1],notining[i]);

    const int stpT = 1, stpTA = 2, stpTG = 4;

    for(int strand = 0; strand < 2; ++strand)
    {
        CVec& s = seq[strand];

        anum[strand] = 0;
        dnum[strand] = 0;
        sttnum[strand] = 0;
        stpnum[strand] = 0;

        if(strand == Plus)
        {
            for(int i = 0; i < len; ++i)
            {
                ascr[strand][i] = max(ascr[strand][i],acceptor.Score(s,i));
                if(ascr[strand][i] != BadScore)
                {
                    if(s[i+1] == nA && s[i+2] == nA) asplit[strand][0][i] |= stpT;
                    if(s[i+1] == nA && s[i+2] == nG) asplit[strand][0][i] |= stpT; 
                    if(s[i+1] == nG && s[i+2] == nA) asplit[strand][0][i] |= stpT;
                    if(s[i+1] == nA) asplit[strand][1][i] |= stpTA|stpTG;
                    if(s[i+1] == nG) asplit[strand][1][i] |= stpTA;
                }
                dscr[strand][i] = max(dscr[strand][i],donor.Score(s,i));
                if(dscr[strand][i] != BadScore)
                {
                    if(s[i] == nT) dsplit[strand][0][i] |= stpT;
                    if(s[i-1] == nT && s[i] == nA) dsplit[strand][1][i] |= stpTA;
                    if(s[i-1] == nT && s[i] == nG) dsplit[strand][1][i] |= stpTG;
                }
                sttscr[strand][i] = start.Score(s,i);
                stpscr[strand][i] = stop.Score(s,i);
                if(ascr[strand][i] != BadScore) ++anum[strand];
                if(dscr[strand][i] != BadScore) ++dnum[strand];
                if(sttscr[strand][i] != BadScore) ++sttnum[strand];
            }
        }
        else
        {
            for(int i = 0; i < len; ++i)
            {
                int ii = len-2-i;   // extra -1 because ii is point on the "right"
                ascr[strand][i] = max(ascr[strand][i],acceptor.Score(s,ii));
                if(ascr[strand][i] != BadScore)
                {
                    if(s[ii+1] == nA && s[ii+2] == nA) asplit[strand][0][i] |= stpT;
                    if(s[ii+1] == nA && s[ii+2] == nG) asplit[strand][0][i] |= stpT; 
                    if(s[ii+1] == nG && s[ii+2] == nA) asplit[strand][0][i] |= stpT;
                    if(s[ii+1] == nA) asplit[strand][1][i] |= stpTA|stpTG;
                    if(s[ii+1] == nG) asplit[strand][1][i] |= stpTA;
                }
                dscr[strand][i] = max(dscr[strand][i],donor.Score(s,ii));
                if(dscr[strand][i] != BadScore)
                {
                    if(s[ii] == nT) dsplit[strand][0][i] |= stpT;
                    if(s[ii-1] == nT && s[ii] == nA) dsplit[strand][1][i] |= stpTA;
                    if(s[ii-1] == nT && s[ii] == nG) dsplit[strand][1][i] |= stpTG;
                }
                sttscr[strand][i] = start.Score(s,ii);
                stpscr[strand][i] = stop.Score(s,ii);
                if(ascr[strand][i] != BadScore) ++anum[strand];
                if(dscr[strand][i] != BadScore) ++dnum[strand];
            }
        }
    }		

    for(CClusterSet::ConstIt it_cls = cls_local.begin(); it_cls != cls_local.end(); ++it_cls)
    {
        for(Cluster::ConstIt it = it_cls->begin(); it != it_cls->end(); ++it)
        {
            const AlignVec& algn(*it);
            int strand = algn.Strand();
            int type = algn.Type();

            if(type != AlignVec::Prot)
            {
                CVec& s = seq[Plus];
                CVec mRNA;
                for(int k = 0; k < (int)algn.size(); ++k) 
                {
                    int exona = algn[k].first;
                    int exonb = algn[k].second;
                    copy(&s[exona],&s[exonb]+1,back_inserter(mRNA));
                }

                if(strand == Plus)
                {
                    int shft = algn.front().first;
                    for(int k = 0; k < (int)algn.size()-1; ++k) 
                    {
                        int exonb = algn[k].second;

                        for(int i = max(0,exonb-2); i <= exonb; ++i)    // if i==exonb, the stop is in the next "exon"
                        {
                            stpscr[strand][i] = stop.Score(mRNA,i-shft);
                        }

                        int exona = algn[k+1].first;   //next exon
                        shft += exona-exonb-1;         //intron length

                        for(int i = exona-1; i <= min(len-1,exona+1); ++i)
                        {
                            sttscr[strand][i] = start.Score(mRNA,i-shft);
                        }
                    }
                }
                else
                {
                    reverse(mRNA.begin(),mRNA.end());
                    for(int i = 0; i < (int)mRNA.size(); ++i) mRNA[i] = toMinus[mRNA[i]];

                    int shft = algn.back().second-1;
                    for(int k = (int)algn.size()-1; k > 0; --k) 
                    {
                        int exona = algn[k].first;

                        for(int i = exona-1; i <= min(len-1,exona+1); ++i)
                        {
                            stpscr[strand][i] = stop.Score(mRNA,shft-i);
                        }

                        int exonb = algn[k-1].second;  //prev exon
                        shft -= exona-exonb-1;         //intron length

                        for(int i = max(0,exonb-2); i <= exonb; ++i)
                        {
                            sttscr[strand][i] = start.Score(mRNA,shft-i);
                        }
                    }
                }
            }
        }
    }

    for(CClusterSet::ConstIt it_cls = cls_local.begin(); it_cls != cls_local.end(); ++it_cls)
    {
        for(Cluster::ConstIt it = it_cls->begin(); it != it_cls->end(); ++it)
        {
            const AlignVec& algn(*it);
            int strand = algn.Strand();
            int type = algn.Type();

            if(type == AlignVec::Prot)
            {
                for(int k = 0; k < (int)algn.size(); ++k)   // suppressing STOPS in prot align
                {
                    int exona = algn[k].first;
                    int exonb = algn[k].second;
                    for(int i = exona; i < exonb; i += 3)
                    {
                        if(strand == Plus)
                        {
                            if(i > 0) stpscr[strand][i-1] = BadScore;  // score on last coding base
                        }
                        else
                        {
                            if(i+2 < len-1) stpscr[strand][i+2] = BadScore;  // score on last intergenic (first stop) base
                        }
                    }
                }
            }
        }
    }

    for(int strand = 0; strand < 2; ++strand)
    {
        for(int i = 0; i < len; ++i)
        {
            if(sttscr[strand][i] != BadScore) ++sttnum[strand];

            if(stpscr[strand][i] != BadScore)
            {
                if(strand == Plus)
                {
                    int& lstp = laststop[strand][2-i%3][i+3];
                    lstp = max(i+1,lstp);
                    ++stpnum[strand];
                }
                else
                {
                    int& lstp = laststop[strand][i%3][i];
                    lstp = max(i-2,lstp);
                    ++stpnum[strand];
                }
            }
        }
    }


    for(int strand = 0; strand < 2; ++strand)
    {
        CVec& s = seq[strand];

        for(int i = 0; i < len; ++i)
        {
            int ii = strand == Plus ? i : len-1-i;

            double score = ncdr.Score(s,ii);
            if(score == BadScore) score = 0;
            ncdrscr[strand][i] = score;
            if(i > 0) ncdrscr[strand][i] += ncdrscr[strand][i-1];

            score = intrg.Score(s,ii);
            if(score == BadScore) score = 0;
            ingscr[strand][i] = score;
            if(i > 0) ingscr[strand][i] += ingscr[strand][i-1];
        }

        for(int frame = 0; frame < 3; ++frame)
        {
            for(int i = 0; i < len; ++i)
            {
                int codonshift,ii;
                if(strand == Plus)     // left end of codon is shifted by frame bases to left
                {
                    codonshift = (frame+i)%3;
                    ii = i;
                }
                else                  // right end of codone is shifted by frame bases to right
                {
                    codonshift = (frame-i)%3;
                    if(codonshift < 0) codonshift += 3;
                    ii = len-1-i;
                }

                double score = cdr.Score(s,ii,codonshift);
                if(score == BadScore) score = 0;

                cdrscr[strand][frame][i] = score;
                if(i > 0) 
                {
                    cdrscr[strand][frame][i] += cdrscr[strand][frame][i-1];

                    IVec& lstp = laststop[strand][frame];
                    lstp[i] = max(lstp[i-1],lstp[i]);
                }
            }
        }
    }

    for(int strand = 0; strand < 2; ++strand)
    {
        for(int frame = 0; frame < 3; ++frame)
        {
            for(int i = 0; i < len; ++i)
            {
                cdrscr[strand][frame][i] -= ncdrscr[strand][i];
            }
        }
        for(int i = 0; i < len; ++i)
        {
            ingscr[strand][i] -= ncdrscr[strand][i];

            int left, right;
            if(dscr[strand][i] != BadScore)
            {
                Terminal& t = donor;
                left = i+1-(strand==Plus ? t.Left():t.Right());
                right = i+(strand==Plus ? t.Right():t.Left());
                dscr[strand][i] -= NonCodingScore(left,right,strand);
            }
            if(ascr[strand][i] != BadScore)
            {
                Terminal& t = acceptor;
                left = i+1-(strand==Plus ? t.Left():t.Right());
                right = i+(strand==Plus ? t.Right():t.Left());
                ascr[strand][i] -= NonCodingScore(left,right,strand);
            }
            if(sttscr[strand][i] != BadScore)
            {
                Terminal& t = start;
                left = i+1-(strand==Plus ? t.Left():t.Right());
                right = i+(strand==Plus ? t.Right():t.Left());
                sttscr[strand][i] -= NonCodingScore(left,right,strand);
            }
            if(stpscr[strand][i] != BadScore)
            {
                Terminal& t = stop;
                left = i+1-(strand==Plus ? t.Left():t.Right());
                right = i+(strand==Plus ? t.Right():t.Left());
                stpscr[strand][i] -= NonCodingScore(left,right,strand);
            }
        }
    }
}

END_NCBI_SCOPE

/*
 * ==========================================================================
 * $Log$
 * Revision 1.4  2004/07/28 12:33:19  dicuccio
 * Sync with Sasha's working tree
 *
 * Revision 1.3  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.2  2003/11/06 00:52:38  ucko
 * Don't try to take the log of an integer constant.
 *
 * Revision 1.1  2003/10/24 15:07:25  dicuccio
 * Initial revision
 * ==========================================================================
 */
