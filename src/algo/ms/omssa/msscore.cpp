/* 
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
*  Please cite the authors in any work or product based on this material.
*
* ===========================================================================
*
* Author:  Lewis Y. Geer
*  
* File Description:
*    code to do mass spec scoring
*
* ===========================================================================
*/

//
//
// 
// Please keep this file clear of NCBI toolkit dependent code as it
// is meant to be used externally from the toolkit.
// 
// NCBI dependent code can go into omssascore.cpp
// 
// 
// 

// precompiled headers #include found in msscore.cpp
#if 0
#include <ncbi_pch.hpp>
#endif

#include <math.h>
#include "msscore.hpp"

// define log of gamma fcn if not defined already
#ifndef MSLNGAMMA
#define MSLNGAMMA lgamma
#endif

// define error function
#ifndef MSERF
#define MSERF erf
#endif


CMSMatchedPeak::CMSMatchedPeak(void):
    MassTolerance(-1),
    ExpIons(-1),
    MatchType(eMSMatchTypeUnknown)
{
}

void CMSMatchedPeak::Assign(CMSMatchedPeak *in)
{
    if(!in || in == this) return;
    SetCharge() = in->GetCharge();
    SetExpIons() = in->GetExpIons();
    SetIntensity() = in->GetIntensity();
    SetIonSeries() = in->GetIonSeries();
    SetMassTolerance() = in->GetMassTolerance();
    SetMatchType() = in->GetMatchType();
    SetMZ() = in->GetMZ();
    SetNumber() = in->GetNumber();
}

void CMSMatchedPeak::Assign(CMSBasicMatchedPeak *in)
{
    if(!in || in == this) return;
     SetCharge() = in->GetCharge();
     SetIntensity() = in->GetIntensity();
     SetIonSeries() = in->GetIonSeries();
     SetMZ() = in->GetMZ();
     SetNumber() = in->GetNumber();

     SetMassTolerance() = -1;
     SetExpIons() = -1;
     SetMatchType() = eMSMatchTypeUnknown;
}


CMSMatchedPeakSet::CMSMatchedPeakSet(void)
{
}

CMSMatchedPeakSet::~CMSMatchedPeakSet()
{
    DeleteMatchedPeakSet();
}

void CMSMatchedPeakSet::CreateMatchedPeakSet(int Size)
{
    DeleteMatchedPeakSet();
    int i;
    for(i = 0; i < Size; ++i)
        SetMatchedPeakSet().push_back(new CMSMatchedPeak);
}

void CMSMatchedPeakSet::DeleteMatchedPeakSet(void)
{         
    while(!GetMatchedPeakSet().empty()) {
        delete SetMatchedPeakSet().back();
        SetMatchedPeakSet().pop_back();
    }
}

CMSMatchedPeakSetMap::CMSMatchedPeakSetMap(void)
{
}

CMSMatchedPeakSetMap::~CMSMatchedPeakSetMap()
{
    TIonSeriesMatchMap::iterator i;
    for( i = SetMatchMap().begin(); i != SetMatchMap().end(); ++i)
        delete i->second;
    SetMatchMap().clear();
}

CMSMatchedPeakSet * CMSMatchedPeakSetMap::CreateSeries(
    TMSCharge Charge, 
    TMSIonSeries Series,
    int Size)
{
    CMSMatchedPeakSet *retval;
    int Key = ChargeSeries2Key(Charge, Series);

    // delete any old matches
    if(GetMatchMap().find(Key) != GetMatchMap().end()) {
        delete SetMatchMap()[Key];
        SetMatchMap().erase(Key);
    }

    retval = new CMSMatchedPeakSet;
    if(retval) {
        retval->CreateMatchedPeakSet(Size);
        SetMatchMap().insert(pair <int, CMSMatchedPeakSet * > (Key, retval));
    }

    return retval;
}

CMSMatchedPeakSet * CMSMatchedPeakSetMap::SetSeries(TMSCharge Charge, TMSIonSeries Series)
{
    CMSMatchedPeakSet *retval;
    const int Key = ChargeSeries2Key(Charge, Series);
    if(GetMatchMap().find(Key) != GetMatchMap().end())
        retval = SetMatchMap()[Key];
    else 
        retval = 0;
    
    return retval;
}   

//typedef std::map <int, CMSMatchedPeakSet *> TIonSeriesMatchMap;

const int CMSMatchedPeakSetMap::ChargeSeries2Key(TMSCharge Charge, TMSIonSeries Series) const
{ 
    return eMSIonTypeMax * Charge + Series;
}




CMSSpectrumMatch::CMSSpectrumMatch(void):
    HitInfo(0),
    Hits(0),
    ExpMass(-1),
    TheoreticalMass(-1),
    Sum(0),
    M(0),
    N(0)
{
}

CMSSpectrumMatch::~CMSSpectrumMatch()
{
    delete [] HitInfo;    
}

void CMSSpectrumMatch::CreateHitInfo(void)
{
    delete [] HitInfo;
    HitInfo = 0;
    if(GetHits() > 0)
        HitInfo = new CMSBasicMatchedPeak[GetHits()];
}

CMSBasicMatchedPeak * CMSSpectrumMatch::Find(
    TMSNumber Number, 
    TMSCharge Charge, 
    TMSIonSeries Series)
{
    int i;
    for(i = 0; i < GetHits(); ++i) {
        if(GetHitInfo(i).GetNumber() == Number &&
           GetHitInfo(i).GetCharge() == Charge &&
           GetHitInfo(i).GetIonSeries() == Series)
            return &SetHitInfo(i);
    }
    return 0;
}

void CMSSpectrumMatch::FillMatchedPeaks(
    TMSCharge Charge, 
    TMSIonSeries Series, 
    int Size, 
    TMSIntensity MinIntensity, 
    TMSMZ MH)
{
    // create a new match set
    CMSMatchedPeakSet *MatchPeakSet =
    SetIonSeriesMatchMap().CreateSeries(Charge, Series, Size);

    int i;

    // iterate over series and look for matching hits
    for(i = 0; i < Size; ++i) {
        CMSBasicMatchedPeak * FoundPeak = Find(i, Charge, Series);
        if(FoundPeak && FoundPeak->GetIntensity() > MinIntensity) {
                // copy hit to match
                MatchPeakSet->SetMatchedPeakSet()[i]->Assign(FoundPeak);
                // set as untyped hit
                MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() =
                    eMSMatchTypeNotTyped;
        }
        else {
            MatchPeakSet->SetMatchedPeakSet()[i]->SetCharge() = Charge;
            MatchPeakSet->SetMatchedPeakSet()[i]->SetIonSeries() = Series;
            MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeUnknown;
            MatchPeakSet->SetMatchedPeakSet()[i]->SetMZ() = 0;
            MatchPeakSet->SetMatchedPeakSet()[i]->SetIntensity() = 0;
            MatchPeakSet->SetMatchedPeakSet()[i]->SetNumber() = i;
        }
    }    


    // iterate over array, look for unset m/z.  keep track of index of last set m/z
    // if unset, iterate until first set, then fill in mz

    // find first -1
    int j;
    for (i = 0; i < Size; ++i) {
        if(MatchPeakSet->GetMatchedPeakSet()[i]->GetMZ() != 0) break;
    }
    // if first filled hit is not the first one, fill in array
    for(j = 0; j < i && i < Size; ++j) {
        MatchPeakSet->SetMatchedPeakSet()[j]->SetMZ() = 
            (MatchPeakSet->GetMatchedPeakSet()[i]->GetMZ()/ (i+1)) * (j+1);
    }
    // now fill out sections in the center
    int LastIndex(i);
    for(; i < Size; ++i) {
        if(MatchPeakSet->GetMatchedPeakSet()[i]->GetMZ() != 0) {
            if(i != LastIndex) {
                // fill in the blank spaces
                for(j = LastIndex+1; j < i; j++) {
                    MatchPeakSet->SetMatchedPeakSet()[j]->SetMZ() =
                        MatchPeakSet->GetMatchedPeakSet()[LastIndex]->GetMZ() +
                        ((MatchPeakSet->GetMatchedPeakSet()[i]->GetMZ() -
                         MatchPeakSet->GetMatchedPeakSet()[LastIndex]->GetMZ()) /
                        (i - LastIndex )) * (j - LastIndex) ;
                }
            }
            LastIndex = i;
        }
    }

    // now fill out last section
    if(MatchPeakSet->GetMatchedPeakSet()[Size-1]->GetMZ() == 0) {

        int StartIndex;
        TMSMZ StartMass;
        TMSMZ MassIncrement;
        // if completely empty, force the array to be completely filled out
        if(MatchPeakSet->GetMatchedPeakSet()[0]->GetMZ() == 0) {
            MassIncrement = (MH/Charge)/(Size+1);
            StartIndex = 0;
            StartMass = (MH/Charge)/(Size+1);
        }
        else {
            MassIncrement = ((MH/Charge) - MatchPeakSet->GetMatchedPeakSet()[LastIndex]->GetMZ()) /
                (Size - LastIndex);
            StartIndex = LastIndex + 1;
            StartMass = MatchPeakSet->GetMatchedPeakSet()[LastIndex]->GetMZ() +
                MassIncrement;
        }

        for(j = StartIndex; j < Size; ++j) {
            MatchPeakSet->SetMatchedPeakSet()[j]->SetMZ() = 
                StartMass + MassIncrement * (j - StartIndex);
        }
    }



#if 0

    // todo: if sb1 or sct, mark as don't search

    for (i = 1; i < LadderArray.size(); ++i) {
        cerr << "i=" << i << " start=" << LadderArray[i]->GetStart() << " stop=" <<
            LadderArray[i]->GetStop() << " center=" << LadderArray[i]->GetCenter() << endl;
    }
#endif


    // first determine if the first peak is independent.  This simplifies the loop
    if(MatchPeakSet->GetMatchedPeakSet()[0]->GetMatchType() == eMSMatchTypeNotTyped)
         MatchPeakSet->SetMatchedPeakSet()[0]->SetMatchType() = eMSMatchTypeIndependent;

    // now characterize all of the matches depending on the preceeding match
    for (i = 1; i < Size; ++i) {
        // only type matched peaks
        if(MatchPeakSet->GetMatchedPeakSet()[i]->GetMatchType() == eMSMatchTypeNotTyped) {
            if(MatchPeakSet->GetMatchedPeakSet()[i-1]->GetMatchType() == eMSMatchTypeIndependent ||
               MatchPeakSet->GetMatchedPeakSet()[i-1]->GetMatchType() == eMSMatchTypeSemiIndependent) {
                MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeDependent;
            }
            else if(MatchPeakSet->GetMatchedPeakSet()[i-1]->GetMatchType() == eMSMatchTypeNoSearch ||
                    MatchPeakSet->GetMatchedPeakSet()[i-1]->GetMatchType() == eMSMatchTypeUnknown) {
                MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeIndependent;
            }
            else if(MatchPeakSet->GetMatchedPeakSet()[i-1]->GetMatchType() == eMSMatchTypeDependent) {
                MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeSemiIndependent;
            }
        }
    }

    return;
}


const double CMSSpectrumMatch::CalcPoissonMean(void) const
{
    double Mean(0.0L);
    // scan thru entire map
    TIonSeriesMatchMap::const_iterator i;
    for(i = GetIonSeriesMatchMap().GetMatchMap().begin();
         i != GetIonSeriesMatchMap().GetMatchMap().end();
         ++i) {
        CMSMatchedPeakSet *PeakSet = i->second;
        // scan thru each match set
        TMatchedPeakSet::const_iterator j;
        for(j = PeakSet->GetMatchedPeakSet().begin();
             j != PeakSet->GetMatchedPeakSet().end();
             ++j) {
            CMSMatchedPeak *Peak = *j;
            // calculate single poisson
            // add it to total poisson
            Mean += 2.0 * Peak->GetMassTolerance() * Peak->GetExpIons();
        }
    }
    return Mean;
}

const double CMSSpectrumMatch::CalcPoisson(double Mean, int i) const
{
    return exp(-Mean) * pow(Mean, i) / exp(MSLNGAMMA(i+1));
}

const double CMSSpectrumMatch::CalcPoissonTopHit(double Mean, int i, double TopHitProb) const
{
    double retval;
    retval =  exp(-Mean) * pow(Mean, i) / exp(MSLNGAMMA(i+1));
    if (TopHitProb != 1.0L) retval *= 1.0L - pow((1.0L-TopHitProb), i);
    return retval;
}

const double CMSSpectrumMatch::CalcNormalTopHit(double Mean, double TopHitProb) const
{
    int i;
    double retval(0.0L), before(-1.0L), increment;

    for (i = 1; i < 1000; i++) {
        increment = CalcPoissonTopHit(Mean, i, TopHitProb);
        // convergence hack -- on linux (at least) convergence test doesn't work
        // for gcc release build
        if (increment <= MSDOUBLELIMIT && i > 10) break;
        retval += increment;
        if (retval == before) break;  // convergence
        before = retval;
    }
    return retval;
}

const double CMSSpectrumMatch::CalcPvalue(double Mean, int Hits) const
{
    if (Hits <= 0) return 1.0L;

    int i;
    double retval(0.0L);

    for (i = 0; i < Hits; i++)
        retval += CalcPoisson(Mean, i);

    retval = 1.0L - retval;
    if (retval <= 0.0L) retval = MSDOUBLELIMIT;
    return retval;
}

const double CMSSpectrumMatch::CalcPvalueTopHit(double Mean, int Hits, double Normal, double TopHitProb) const
{
    if (Hits <= 0) return 1.0L;

    int i;
    double retval(0.0L), increment;

    for (i = 1; i < Hits; i++) {
        increment = CalcPoissonTopHit(Mean, i, TopHitProb);
        retval += increment;
    }

    retval /= Normal;
    retval = 1.0L - retval;
    if (retval <= 0.0L) retval = MSDOUBLELIMIT;
    return retval;
}

