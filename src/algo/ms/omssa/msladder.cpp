/* $Id$
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
 *    Classes to deal with ladders of m/z values
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <algorithm>

#include <math.h>

#include "msladder.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  CLadder::
//
//  Contains a theoretical m/z ladder
//

// constructor
CLadder::CLadder(void): LadderIndex(0), 
    Ladder(new int[kMSLadderMax]),
    Hit(new int[kMSLadderMax]),
    LadderNumber(new TMSNumber[kMSLadderMax]),
    Intensity(new unsigned[kMSLadderMax]),
    Delta(new int[kMSLadderMax]),
    LadderSize(kMSLadderMax),
    M(0),
    Sum(0)
{ 
}

// constructor
CLadder::CLadder(int SizeIn): LadderIndex(0),
    Ladder(new int[SizeIn]),
    Hit(new int[SizeIn]),
    LadderNumber(new TMSNumber[SizeIn]),
    Intensity(new unsigned[SizeIn]),
    Delta(new int[SizeIn]),
    LadderSize(SizeIn),
    M(0),
    Sum(0)
{
}


// copy constructor
CLadder::CLadder(const CLadder& Old) : LadderIndex(0), 
			Ladder(new int[Old.LadderSize]),
			Hit(new int[Old.LadderSize]),
			LadderSize(Old.LadderSize)
{
//    CLadder();
    Start = Old.Start;
    Stop = Old.Stop;
    Index = Old.Index;
    Type = Old.Type;
    Mass = Old.Mass;
    M = Old.M;
    Sum = Old.Sum;

    LadderIndex = Old.LadderIndex;
  
    int i;
    for(i = 0; i < size(); i++) {
        (*this)[i] = *(Old.Ladder.get() + i);
        GetHit()[i] = *(Old.Hit.get() + i);
        SetIntensity()[i] = Old.GetIntensity()[i];
        SetDelta()[i] = Old.GetDelta()[i];
        SetLadderNumber()[i] = Old.GetLadderNumber()[i];
    }
}


// destructor
CLadder::~CLadder()
{
}


const bool 
CLadder::CreateLadder(const int IonType,
                      const int ChargeIn,
                      const char * const Sequence,
                      const int SeqIndex,
                      const int start,
                      const int stop,
                      const int mass,
                      const CMassArray& MassArray,
                      const CAA &AA,
                      const unsigned ModMask,
                      const CMod ModList[],
                      const int NumMod, 
                      const CMSSearchSettings& Settings,
                      const bool NoProline
                      )
{
    Charge = ChargeIn;
    int i, j, ion = MSSCALE2INT((kTermMass[IonType] + (Charge * Settings.GetChargehandling().GetNegative() - 1) * kProton) / Charge + 
				    kIonTypeMass[IonType]/Charge);
    const int *IntMassArray = MassArray.GetIntMass();
    const char * const AAMap(AA.GetMap());
    EMSSearchType SearchType = 
        static_cast <EMSSearchType> (Settings.GetProductsearchtype());
    double ExactMass = Settings.GetExactmass();

    Start = start;
    Stop = stop;
    Index = SeqIndex;  // gi or position in blastdb
    Type = IonType;
    Mass = static_cast <int> (mass - MSSCALE2INT(Charge*kProton) * Settings.GetChargehandling().GetNegative());

    int ModIndex;  // index into number of possible mod sites
    int Direction;
    int Offset;
    int iSkip;  // number to skip at beginning of sequence
    int ProlineOffset(0);  // offset used to calculate proline rule

    LadderIndex = stop - start;
    if(Settings.GetSearchctermproduct() == 1) LadderIndex--;

    // make the ladder fit in memory for long peptides/proteins
    if(LadderIndex > LadderSize) LadderIndex = LadderSize;

    if(kIonDirection[IonType] == 1) {
        ModIndex = 0;
        Direction = 1;
        Offset = start;
        iSkip = 0;
        if(Settings.GetSearchb1() == 1) {
            if(!CalcDelta(IntMassArray, AAMap, Sequence,
                          Offset, Direction, NumMod, ModIndex,
                          ModList, ModMask, 0, ion, Charge, SearchType,
                          ExactMass)) return false;
            iSkip = 1;
            LadderIndex--;
        }
        // set forward proline rule
        ProlineOffset = 1;
    }
    else {
        ModIndex = NumMod - 1;
        Direction = -1;
        Offset = stop;
        iSkip = 0;
         if(Settings.GetSearchctermproduct() == 1) {
             if(!CalcDelta(IntMassArray, AAMap, Sequence,
                           Offset, Direction, NumMod, ModIndex,
                           ModList, ModMask, 0, ion, Charge, SearchType,
                           ExactMass)) return false;
               iSkip = 1;
         }
         // set reverse proline rule
         ProlineOffset = 0;
    }

    j = 0;
    for(i = 0; i < LadderIndex; i++) {
        if(!CalcDelta(IntMassArray, AAMap, Sequence,
                      Offset, Direction, NumMod, ModIndex,
                      ModList, ModMask, i+iSkip, ion, Charge, SearchType,
                      ExactMass)) return false;
        // proline is '\x0e'
        // if (!prolinerule || Sequence[i+iSkip] != '\x0e') {
        // 
        if(!NoProline || 
           (i + iSkip + ProlineOffset <= stop - start &&
           Sequence[Offset + Direction * (i + iSkip + ProlineOffset)] != '\x0e')) {
            GetHit()[j] = 0;
            (*this)[j] = ion;
               cout << IonType << " " << j << " " << ion << endl;
            SetLadderNumber()[j] = i+iSkip;
            j++;
        }
    }
    LadderIndex = j;
    return true;
}
    

// or's the hitlist
void CLadder::Or(CLadder& LadderIn)
{
    int i;
    if(kIonDirection[Type] ==  LadderIn.GetType()) {
    	for(i = 0; i < LadderIndex; i++) {
    	    GetHit()[i] = GetHit()[i] + LadderIn.GetHit()[i];
    	}
    }
    else if( size() != static_cast <int> (Stop - Start)) return;  // unusable chars
    else {  // different direction
    	for(i = 0; i < LadderIndex && i < LadderSize; i++) {
    	    GetHit()[i] = GetHit()[i] + LadderIn.GetHit()[LadderIndex - i - 1];
    	}
    }
}


// sees if ladder contains the given mass value
bool CLadder::Contains(int MassIndex, int Tolerance)
{
    int i;
    // go thru ladder 
    for(i = 0; i < LadderIndex; i++) {
    	if((*this)[i] <= MassIndex + Tolerance && 
    	   (*this)[i] > MassIndex - Tolerance ) return true;
    }
    return false;
}


bool CLadder::ContainsFast(int MassIndex, int Tolerance)
{
    int x, l(0), r(LadderIndex - 1);
    
    while(l <= r) {
        x = (l + r)/2;
        if ((*this)[x] < MassIndex - Tolerance) 
	    l = x + 1;
        else if ((*this)[x] > MassIndex + Tolerance)
	    r = x - 1;
	else return true;
    } 
#if 0    
    if (x < LadderIndex - 1 && x >= 0 &&
	(*this)[x+1] < MassIndex + Tolerance && (*this)[x+1] > 
	MassIndex - Tolerance) 
	return true;
#endif
    cout << MassIndex << " ladder " << (*this)[x] << endl;
    return false;
}


void     
CLadderContainer::CreateLadderArrays(int MaxModPerPep, int MaxLadderSize)
{
    TSeriesChargePairList::iterator Iter;
    for( Iter = SetSeriesChargePairList().begin();
         Iter != SetSeriesChargePairList().end();
         ++Iter) {
        int Key = CMSMatchedPeakSetMap::ChargeSeries2Key(Iter->first, Iter->second);
        CRef <CLadder> newLadder;
        TLadderListPtr newLadderListPtr(new TLadderList);
        int i;
        for (i = 0; i < MaxModPerPep; i++) {
            newLadder.Reset(new CLadder(MaxLadderSize));
            newLadderListPtr->push_back(newLadder);
        }
        SetLadderMap().insert(TLadderMap::value_type(Key, newLadderListPtr));
    }
}

bool 
CLadderContainer::MatchIter(TLadderMap::iterator& Iter,
                       TMSCharge BeginCharge,
                       TMSCharge EndCharge,
                       TMSIonSeries SeriesType)
{
    bool retval = true;
    if(BeginCharge != 0 && EndCharge != 0) {
        if(CMSMatchedPeakSetMap::Key2Charge(Iter->first) < BeginCharge ||
           CMSMatchedPeakSetMap::Key2Charge(Iter->first) > EndCharge)
            retval = false;
    }
    if(SeriesType != eMSIonType_unknown) {
        if(CMSMatchedPeakSetMap::Key2Series(Iter->first) != SeriesType)
            retval = false;
    }

    return retval;
}

void 
CLadderContainer::Next(TLadderMap::iterator& Iter,
                       TMSCharge BeginCharge,
                       TMSCharge EndCharge,
                       TMSIonSeries SeriesType)
{
    if(Iter == SetLadderMap().end()) return;
    Iter++;
    while(Iter != SetLadderMap().end() && 
          !MatchIter(Iter, BeginCharge, EndCharge, SeriesType))
        Iter++;
}
    
    
void     
CLadderContainer::Begin(TLadderMap::iterator& Iter,
                        TMSCharge BeginCharge,
                        TMSCharge EndCharge,
                        TMSIonSeries SeriesType)
{
    Iter = SetLadderMap().begin();
    if(MatchIter(Iter, BeginCharge, EndCharge, SeriesType)) return;
    Next(Iter, BeginCharge ,EndCharge, SeriesType);
}
        


