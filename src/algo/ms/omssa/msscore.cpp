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

#ifdef HAVE_LIMITS
    #include <limits>
#endif
#include <math.h>
#include "msscore.hpp"

#ifdef USING_OMSSA_SCOPE
USING_OMSSA_SCOPE
#endif

// define log of gamma fcn if not defined already
#ifndef MSLNGAMMA
    #define MSLNGAMMA lgamma
#endif

// define error function
#ifndef MSERF
    #define MSERF erf
#endif


const int CMSBasicMatchedPeak::IsForwardSeries(EMSIonSeries Series)
{
    if ( Series == eMSIonTypeA ||
         Series == eMSIonTypeB ||
         Series == eMSIonTypeC)
        return 1;
    if ( Series == eMSIonTypeX ||
         Series == eMSIonTypeY ||
         Series == eMSIonTypeZ)
        return 0;
    return -1;
}

CMSMatchedPeak::CMSMatchedPeak(void):
MassTolerance(-1),
ExpIons(-1),
MatchType(eMSMatchTypeUnknown)
{
}

void CMSMatchedPeak::Assign(CMSMatchedPeak *in)
{
    if (!in || in == this) return;
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
    if (!in || in == this) return;
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

void CMSMatchedPeakSet::CreateMatchedPeakSet(int SizeIn)
{
    DeleteMatchedPeakSet();
    int i;
    for (i = 0; i < SizeIn; ++i)
        SetMatchedPeakSet().push_back(new CMSMatchedPeak);
}

void CMSMatchedPeakSet::DeleteMatchedPeakSet(void)
{         
    while (!GetMatchedPeakSet().empty()) {
        delete SetMatchedPeakSet().back();
        SetMatchedPeakSet().pop_back();
    }
}

void 
CMSMatchedPeakSet::Compare(CMSMatchedPeakSet *Other, bool SameDirection)
{
    if(!Other ||
       (GetMatchedPeakSet().size() != Other->GetMatchedPeakSet().size()) ) return;            

    unsigned i, j;

    for(i = 0; i < GetMatchedPeakSet().size(); ++i) {
        if(SameDirection) j = i;
        else j = Other->GetSize() - i - 1;
        // don't do the comparison if there are no peaks to compare!
        if(j >= Other->GetMatchedPeakSet().size()) break;

        if ((GetMatchedPeakSet()[i]->GetMatchType() == eMSMatchTypeIndependent ||
             GetMatchedPeakSet()[i]->GetMatchType() == eMSMatchTypeSemiIndependent) &&
            (Other->GetMatchedPeakSet()[j]->GetMatchType() == eMSMatchTypeIndependent ||
             Other->GetMatchedPeakSet()[j]->GetMatchType() == eMSMatchTypeSemiIndependent)) {
            Other->GetMatchedPeakSet()[j]->SetMatchType() = eMSMatchTypeDependent;
        }   
    }   
}

CMSMatchedPeakSetMap::CMSMatchedPeakSetMap(void)
{
}

CMSMatchedPeakSetMap::~CMSMatchedPeakSetMap()
{
    TIonSeriesMatchMap::iterator i;
    for ( i = SetMatchMap().begin(); i != SetMatchMap().end(); ++i)
        delete i->second;
    SetMatchMap().clear();
}

CMSMatchedPeakSet * 
    CMSMatchedPeakSetMap::CreateSeries(
        TMSCharge Charge, 
        TMSIonSeries Series,
        int Size,
        int Maxproductions)
{   
    CMSMatchedPeakSet *retval;
    int Key = ChargeSeries2Key(Charge, Series);
    unsigned Realsize = min(Size, Maxproductions);

    // find old matches
    if (GetMatchMap().find(Key) != GetMatchMap().end()) {
        // if size is same, don't bother reallocating
        if (SetMatchMap()[Key]->SetMatchedPeakSet().size() == Realsize) {
            return SetMatchMap()[Key];
        }
        // otherwise delete and reallocate
        else {
            delete SetMatchMap()[Key];
            SetMatchMap().erase(Key);
        }
    }

    retval = new CMSMatchedPeakSet;
    if (retval) {
        retval->CreateMatchedPeakSet(Realsize);
        retval->SetSize() = Size;
        SetMatchMap().insert(std::pair <int, CMSMatchedPeakSet * > (Key, retval));
    }

    return retval;
}

CMSMatchedPeakSet * CMSMatchedPeakSetMap::SetSeries(TMSCharge Charge, TMSIonSeries Series)
{
    CMSMatchedPeakSet *retval;
    const int Key = ChargeSeries2Key(Charge, Series);
    if (GetMatchMap().find(Key) != GetMatchMap().end())
        retval = SetMatchMap()[Key];
    else
        retval = 0;

    return retval;
}   

//typedef std::map <int, CMSMatchedPeakSet *> TIonSeriesMatchMap;

const int 
CMSMatchedPeakSetMap::ChargeSeries2Key(TMSCharge Charge, TMSIonSeries Series)
{ 
    return eMSIonTypeMax * Charge + Series;
}

const TMSCharge
CMSMatchedPeakSetMap::Key2Charge(int Key)
{ 
    return static_cast <TMSCharge> (Key/eMSIonTypeMax);
}

const TMSIonSeries
CMSMatchedPeakSetMap::Key2Series(int Key)
{ 
    return static_cast <TMSIonSeries> (Key - Key2Charge(Key)*eMSIonTypeMax);
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
    if (GetHits() > 0)
        HitInfo = new CMSBasicMatchedPeak[GetHits()];
}

CMSBasicMatchedPeak * CMSSpectrumMatch::Find(
                                            TMSNumber Number, 
                                            TMSCharge ChargeIn, 
                                            TMSIonSeries Series)
{
    int i;
    for (i = 0; i < GetHits(); ++i) {
        if (GetHitInfo(i).GetNumber() == Number &&
            GetHitInfo(i).GetCharge() == ChargeIn &&
            GetHitInfo(i).GetIonSeries() == Series)
            return &SetHitInfo(i);
    }
    return 0;
}


void CMSSpectrumMatch::FillMatchedPeaks(
                                       TMSCharge ChargeIn, 
                                       TMSIonSeries Series, 
                                       unsigned Size, 
                                       TMSIntensity MinIntensity, 
                                       bool Skipb1,
                                       EMSTerminalBias TerminalIon,
                                       int Maxproductions
//#if 0
                                       ,
                                       string &Sequence,
                                       bool NoProline
//#endif
                                       )
{
    // create a new match set
    CMSMatchedPeakSet *MatchPeakSet =
    SetIonSeriesMatchMap().CreateSeries(ChargeIn, Series, Size, Maxproductions);

    unsigned i;

    // iterate over series and look for matching hits
    for (i = 0; i < MatchPeakSet->GetMatchedPeakSet().size(); ++i) {
        CMSBasicMatchedPeak * FoundPeak = Find(i, ChargeIn, Series);
        if (FoundPeak && FoundPeak->GetIntensity() > MinIntensity) {
            // copy hit to match
            MatchPeakSet->SetMatchedPeakSet()[i]->Assign(FoundPeak);
            // set as untyped hit
            MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() =
            eMSMatchTypeNotTyped;

            // check for terminal match
            // n-term
            if (TerminalIon == eMSNTerminalBias || TerminalIon == eMSBothTerminalBias) {
                if ( (i==0 && CMSBasicMatchedPeak::IsForwardSeries(static_cast <EMSIonSeries> (Series))) ||
                     (i==Size-1 && !CMSBasicMatchedPeak::IsForwardSeries(static_cast <EMSIonSeries> (Series)))) {
                    MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeTerminus;
                }
            }
            // c-term
            else if (TerminalIon == eMSCTerminalBias || TerminalIon == eMSBothTerminalBias) {
                if ( (i==0 && !CMSBasicMatchedPeak::IsForwardSeries(static_cast <EMSIonSeries> (Series))) ||
                     (i==Size-1 && CMSBasicMatchedPeak::IsForwardSeries(static_cast <EMSIonSeries> (Series)))) {
                    MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeTerminus;
                }
            }
        }
        else {
            MatchPeakSet->SetMatchedPeakSet()[i]->SetCharge() = ChargeIn;
            MatchPeakSet->SetMatchedPeakSet()[i]->SetIonSeries() = Series;
            MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeUnknown;
            MatchPeakSet->SetMatchedPeakSet()[i]->SetMZ() = 0;
            MatchPeakSet->SetMatchedPeakSet()[i]->SetIntensity() = 0;
            MatchPeakSet->SetMatchedPeakSet()[i]->SetNumber() = i;
        }
        // if b1 should not be searched, mark it as so.
        if (i == 0 && Skipb1 && CMSBasicMatchedPeak::IsForwardSeries(static_cast <EMSIonSeries> (Series)))
            MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeNoSearch;
//#if 0
        // apply rule that no 
        if (NoProline && 
            CMSBasicMatchedPeak::IsForwardSeries(static_cast <EMSIonSeries> (Series)) &&
            i+1 < Sequence.size() && 
            Sequence[i+1] == 'P') {
            MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeNoSearch;
        }

        if (NoProline &&
            !CMSBasicMatchedPeak::IsForwardSeries(static_cast <EMSIonSeries> (Series)) &&
            i < Sequence.size() &&
            Sequence[Sequence.size() - i - 1] == 'P') {
            MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeNoSearch;
        }
//#endif

    }


    // iterate over array, look for unset m/z.  keep track of index of last set m/z
    // if unset, iterate until first set, then fill in mz

    // find first -1
    unsigned j;
    for (i = 0; i < MatchPeakSet->GetMatchedPeakSet().size(); ++i) {
        if (MatchPeakSet->GetMatchedPeakSet()[i]->GetMZ() != 0) break;
    }
    // if first filled hit is not the first one, fill in array
    for (j = 0; j < i && i < MatchPeakSet->GetMatchedPeakSet().size(); ++j) {
        MatchPeakSet->SetMatchedPeakSet()[j]->SetMZ() = 
        (MatchPeakSet->GetMatchedPeakSet()[i]->GetMZ()/ (i+1)) * (j+1);
    }
    // now fill out sections in the center
    unsigned LastIndex(i);
    for (; i < MatchPeakSet->GetMatchedPeakSet().size(); ++i) {
        if (MatchPeakSet->GetMatchedPeakSet()[i]->GetMZ() != 0) {
            if (i != LastIndex) {
                // fill in the blank spaces
                for (j = LastIndex+1; j < i; j++) {
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
    if (MatchPeakSet->GetMatchedPeakSet()[MatchPeakSet->GetMatchedPeakSet().size()-1]->GetMZ() == 0) {

        int StartIndex;
        TMSMZ StartMass;
        TMSMZ MassIncrement;
        // if completely empty, force the array to be completely filled out
        if (MatchPeakSet->GetMatchedPeakSet()[0]->GetMZ() == 0) {
            MassIncrement = (GetExpMass()/ChargeIn)/(Size+1);
            StartIndex = 0;
            StartMass = (GetExpMass()/ChargeIn)/(Size+1);
        }
        else {
            MassIncrement = ((GetExpMass()/ChargeIn) - MatchPeakSet->GetMatchedPeakSet()[LastIndex]->GetMZ()) /
                            (Size - LastIndex);
            StartIndex = LastIndex + 1;
            StartMass = MatchPeakSet->GetMatchedPeakSet()[LastIndex]->GetMZ() +
                        MassIncrement;
        }

        for (j = StartIndex; j < MatchPeakSet->GetMatchedPeakSet().size(); ++j) {
            MatchPeakSet->SetMatchedPeakSet()[j]->SetMZ() = 
            StartMass + MassIncrement * (j - StartIndex);
        }
    }


    // first determine if the first peak is independent.  This simplifies the loop
    if (MatchPeakSet->GetMatchedPeakSet()[0]->GetMatchType() == eMSMatchTypeNotTyped)
        MatchPeakSet->SetMatchedPeakSet()[0]->SetMatchType() = eMSMatchTypeIndependent;

    // now characterize all of the matches depending on the preceeding match
    for (i = 1; i < MatchPeakSet->GetMatchedPeakSet().size(); ++i) {
        // only type matched peaks
        if (MatchPeakSet->GetMatchedPeakSet()[i]->GetMatchType() == eMSMatchTypeNotTyped) {
            if (MatchPeakSet->GetMatchedPeakSet()[i-1]->GetMatchType() == eMSMatchTypeIndependent ||
                MatchPeakSet->GetMatchedPeakSet()[i-1]->GetMatchType() == eMSMatchTypeSemiIndependent) {
                MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeDependent;
            }
            else if (MatchPeakSet->GetMatchedPeakSet()[i-1]->GetMatchType() == eMSMatchTypeNoSearch ||
                     MatchPeakSet->GetMatchedPeakSet()[i-1]->GetMatchType() == eMSMatchTypeUnknown ||
                     MatchPeakSet->GetMatchedPeakSet()[i-1]->GetMatchType() == eMSMatchTypeNoSearch ||
                     MatchPeakSet->GetMatchedPeakSet()[i-1]->GetMatchType() == eMSMatchTypeTerminus) {
                MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeIndependent;
            }
            else if (MatchPeakSet->GetMatchedPeakSet()[i-1]->GetMatchType() == eMSMatchTypeDependent) {
                MatchPeakSet->SetMatchedPeakSet()[i]->SetMatchType() = eMSMatchTypeSemiIndependent;
            }
        }
    }

    return;
}



const double 
CMSSpectrumMatch::CalcPoissonMean(double ProbTerminal,
                                  int NumTerminalMasses, 
                                  double ProbDependent, 
                                  int NumUniqueMasses,
                                  double ToleranceAdjust
                                  ) const
{
    double Mean(0.0L);
    // scan thru entire map
    TIonSeriesMatchMap::const_iterator i;
    for (i = GetIonSeriesMatchMap().GetMatchMap().begin();
        i != GetIonSeriesMatchMap().GetMatchMap().end();
        ++i) {
        CMSMatchedPeakSet *PeakSet = i->second;
        // scan thru each match set
        TMatchedPeakSet::const_iterator j;
        for (j = PeakSet->GetMatchedPeakSet().begin();
            j != PeakSet->GetMatchedPeakSet().end();
            ++j) {
            CMSMatchedPeak *Peak = *j;

            // skip unsearched peaks
            if (Peak->GetMatchType() == eMSMatchTypeNoSearch) continue;

            // calculate single poisson
            // add it to total poisson
            Mean += 2.0 * Peak->GetMassTolerance() * Peak->GetExpIons() * ToleranceAdjust;
            _TRACE( "mean=" << Mean << " masstol=" << Peak->GetMassTolerance() << " expions=" <<  Peak->GetExpIons());

            // if a statistically dependent peak, add in dependent match probability
            if (Peak->GetMatchType() == eMSMatchTypeDependent && NumUniqueMasses != 0) {
                Mean += ProbDependent / NumUniqueMasses;
                _TRACE( "mean=" << Mean << " probdep=" << ProbDependent << " numunique=" << NumUniqueMasses);
            }

            // if a statistically biased terminal peak, add in biased match probability
            if (Peak->GetMatchType() == eMSMatchTypeTerminus && NumTerminalMasses != 0) {
                Mean += ProbTerminal / NumTerminalMasses;
            }
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

const double CMSSpectrumMatch::CalcPvalue(double Mean, int HitsIn) const
{
    if (HitsIn <= 0) return 1.0L;

    int i;
    double retval(0.0L), increment, beforeincrement(0.0L);

    for (i = 0; i < HitsIn; i++) {
        increment = CalcPoisson(Mean, i);
#ifdef HAVE_LIMITS
        // detect limit of increment if numeric_limits available
        if(increment < retval * std::numeric_limits <double> ::epsilon()) break;
#endif
        retval += increment;
        if(retval == 1.0L) break;
        beforeincrement = increment;
    }

    // if retval is calculated to the maximum precision of double
    // back off one increment to give a lower bound of the e-value
    if(retval == 1.0L)
        retval -= beforeincrement;

    retval = 1.0L - retval;
    if (retval <= 0.0L) retval = MSDOUBLELIMIT;
    return retval;
}

const double CMSSpectrumMatch::CalcPvalueTopHit(double Mean, int HitsIn, double Normal, double TopHitProb) const
{
    if (HitsIn <= 0) return 1.0L;

    int i;
    double retval(0.0L), increment, beforeretval(-1.0), beforeincrement(0.0L);

    for (i = 1; i < HitsIn; i++) {
        increment = CalcPoissonTopHit(Mean, i, TopHitProb);
#ifdef HAVE_LIMITS
        // detect limit of increment if numeric_limits available
        if(increment < retval * std::numeric_limits <double> ::epsilon()) break;
#endif
        retval += increment;
        // note that in gcc 3.4.2 on linux with -O9 -ffast-math, the following statement doesn't appear to work
        // trying to fix this with retval - beforeretval < increment also does not work -- it breaks too soon!
        if(retval == beforeretval) break;
        beforeretval = retval;
        beforeincrement = increment;
    }

    // if retval and Normal are calculated to the maximum precision of double
    // back off one increment to give a lower bound of the e-value
    if(retval == Normal) retval -= beforeincrement;

    retval /= Normal;
    retval = 1.0L - retval;
    if (retval <= 0.0L) retval = MSDOUBLELIMIT;
    return retval;
}

const double CMSSpectrumMatch::CalcRankProb(void) const
{
    double Average, StdDev, Perf(1.0L);
    if (GetM() != 0.0) {
        Average = (GetM() *(GetN()+1))/2.0;
        StdDev = sqrt(Average * (GetN() - GetM()) / 6.0);
        if ( StdDev != 0.0)
            Perf = 0.5*(1.0 + MSERF((GetSum() - Average)/(StdDev*sqrt(2.0))));
    }
    return Perf;
}


const TMSMZ 
CMSSpectrumMatch::GetMeanDelta(void) const
{
    TMSMZ Mean(0);

    int i;
    for (i = 0; i < GetHits(); ++i) {
        Mean += GetHitInfo(i).GetDelta();
    }
    return static_cast <int> (Mean/(static_cast <double> (GetHits())));
}


const TMSMZ 
CMSSpectrumMatch::GetStdDevDelta(void) const
{
    TMSMZ Mean;
    double StdDev(0.0L);

    Mean = GetMeanDelta();

    int i;
     for (i = 0; i < GetHits(); ++i) {
         StdDev += pow((double)GetHitInfo(i).GetDelta() - Mean, 2.0);
     }
     StdDev = pow((double)StdDev/GetHits(), 0.5);
     return (TMSMZ)StdDev;
}


const TMSMZ 
CMSSpectrumMatch::GetMaxDelta(void) const
{
     TMSMZ Max(0);
     int i;
     for (i = 0; i < GetHits(); ++i) {
         if(abs(GetHitInfo(i).GetDelta()) > Max)
             Max = abs(GetHitInfo(i).GetDelta());
     }
     return Max;
}

