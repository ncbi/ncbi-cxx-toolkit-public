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


const SeqScores* HMM_State::seqscr = 0;

bool Lorentz::Init(CNcbiIstream& from, const string& label)
{
    string s;
    from >> s;
    if(s != label) return false;

    from >> s;
    if(s != "A:") return false;
    if(!(from >> A)) return false;

    from >> s;
    if(s != "L:") return false;
    if(!(from >> L)) return false;

    from >> s;
    if(s != "MinL:") return false;
    if(!(from >> minl)) return false;

    from >> s;
    if(s != "MaxL:") return false;
    if(!(from >> maxl)) return false;

    from >> s;
    if(s != "Step:") return false;
    if(!(from >> step)) return false;

    int num = (maxl-1)/step+1;
    try
    {
        score.resize(num,0);
        clscore.resize(num,0);
    }
    catch(bad_alloc)
    {
        cerr << "No memory in Lorentz\n";
        exit(1);
    }

    int i = 0;
    while(from >> score[i]) 
    {
        if((i+1)*step < minl) score[i] = 0;
        i++;
    }
    from.clear();
    while(i < num) 
    {
        score[i] = A/(L*L+pow((i+0.5)*step,2));
        i++;
    }

    double sum = 0;
    avlen = 0;
    for(int l = minl; l <= maxl; ++l) 
    {
        double scr = score[(l-1)/step];
        sum += scr;
        avlen += l*scr;
    }

    avlen /= sum;
    for(int i = 0; i < num; ++i) score[i] /= sum;

    clscore[num-1] = 0;
    for(int i = num-2; i >= 0; --i) clscore[i] = clscore[i+1]+score[i+1]*step;

    for(int i = 0; i < num; ++i) score[i] = (score[i] == 0) ? BadScore : log(score[i]);

    return true;
}

double Lorentz::Through(int seqlen) const
{
    double through = 0;
    for(int l = seqlen+1; l <= MaxLen(); ++l) 
    {
        int i = (l-1)/step;
        int delx = min((i+1)*step,MaxLen())-l;
        double dely = (i == 0 ? 1 : clscore[i-1])-clscore[i];
        through += dely/step*delx+clscore[i];
    }
    return through;
}

double Exon::firstphase[3], Exon::internalphase[3][3];
Lorentz Exon::firstlen, Exon::internallen, Exon::lastlen, Exon::singlelen; 
bool Exon::initialised = false;

void Exon::Init(const string& file, int cgcontent)
{
    string label = "[Exon]";
    ifstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label+" 1");

    string str;
    from >> str;
    if(str != "FirstExonPhase:") Error(label+" 2");
    for(int i = 0; i < 3; ++i) 
    {
        from >> firstphase[i];
        if(!from) Error(label);
        firstphase[i] = log(firstphase[i]);
    }

    from >> str;
    if(str != "InternalExonPhase:") Error(label+" 3");
    for(int i = 0; i < 3; ++i)
    {
        for(int j = 0; j < 3; ++j) 
        {
            from >> internalphase[i][j];
            if(!from) Error(label+" 4");
            internalphase[i][j] = log(internalphase[i][j]);
        }
    }

    if(!firstlen.Init(from,"FirstExonDistribution:")) Error(label+" 5");
    if(!internallen.Init(from,"InternalExonDistribution:")) Error(label+" 6");
    if(!lastlen.Init(from,"LastExonDistribution:")) Error(label+" 7");
    if(!singlelen.Init(from,"SingleExonDistribution:")) Error(label+" 8");

    initialised = true;
    from.close();
}


double Intron::lnThrough[3], Intron::lnDen[3];
double Intron::lnTerminal, Intron::lnInternal;
Lorentz Intron::intronlen;
bool Intron::initialised = false;

void Intron::Init(const string& file, int cgcontent, int seqlen)
{
    string label = "[Intron]";
    ifstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label);

    string str;
    from >> str;
    if(str != "InitP:") Error(label);
    double initp, phasep[3];
    from >> initp >> phasep[0] >> phasep[1] >> phasep[2];
    if(!from) Error(label);
    initp = initp/2;      // two strands

    from >> str;
    if(str != "toTerm:") Error(label);
    double toterm;
    from >> toterm;
    if(!from) Error(label);
    lnTerminal = log(toterm);
    lnInternal = log(1-toterm);

    if(!intronlen.Init(from,"IntronDistribution:")) Error(label);

    double lnthrough = intronlen.Through(seqlen);
    lnthrough = (lnthrough == 0) ? BadScore : log(lnthrough);
    for(int i = 0; i < 3; ++i)
    {
        lnDen[i] = log(initp*phasep[i]/intronlen.AvLen());
        lnThrough[i] = (lnthrough == BadScore) ? BadScore : lnDen[i]+lnthrough;
    }

    initialised = true;
    from.close();
}


double Intergenic::lnThrough;
double Intergenic::lnDen;
double Intergenic::lnSingle, Intergenic::lnMulti;
Lorentz Intergenic::intergeniclen;
bool Intergenic::initialised = false;

void Intergenic::Init(const string& file, int cgcontent, int seqlen)
{
    string label = "[Intergenic]";
    ifstream from(file.c_str());
    pair<int,int> cgrange = FindContent(from,label,cgcontent);
    if(cgrange.first < 0) Error(label);

    string str;
    from >> str;
    if(str != "InitP:") Error(label);
    double initp;
    from >> initp;
    if(!from) Error(label);
    initp = initp/2;      // two strands

    from >> str;
    if(str != "toSingle:") Error(label);
    double tosingle;
    from >> tosingle;
    if(!from) Error(label);
    lnSingle = log(tosingle);
    lnMulti = log(1-tosingle);

    if(!intergeniclen.Init(from,"IntergenicDistribution:")) Error(label);

    double lnthrough = intergeniclen.Through(seqlen);
    lnthrough = (lnthrough == 0) ? BadScore : log(lnthrough);
    lnDen = log(initp/intergeniclen.AvLen());
    lnThrough = (lnthrough == BadScore) ? BadScore : lnDen+lnthrough;

    initialised = true;
    from.close();
}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/07/28 12:33:19  dicuccio
 * Sync with Sasha's working tree
 *
 * Revision 1.3  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.2  2003/11/06 15:02:21  ucko
 * Use iostream interface from ncbistre.hpp for GCC 2.95 compatibility.
 *
 * Revision 1.1  2003/10/24 15:07:25  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
