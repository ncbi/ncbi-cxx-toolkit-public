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
			LadderSize(kMSLadderMax)
{ 
}

// constructor
CLadder::CLadder(int SizeIn): LadderIndex(0),
			      Ladder(new int[SizeIn]),
			      Hit(new int[SizeIn]),
			      LadderSize(SizeIn)
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

    LadderIndex = Old.LadderIndex;
  
    int i;
    for(i = 0; i < size(); i++) {
	(*this)[i] = *(Old.Ladder.get() + i);
	GetHit()[i] = *(Old.Hit.get() + i);
    }
}


// destructor
CLadder::~CLadder()
{
}



// creates ladder
// note that mass input is only used later on in another routine
// to calculate score.
bool CLadder::CreateLadder(int IonType, int ChargeIn, char *Sequence,
			   int SeqIndex,
			   int start, int stop, int mass,
			   CMassArray& MassArray, CAA &AA,
			   unsigned ModMask,
			   const char **Site,
			   int *DeltaMass,
			   int NumMod)
{
    Charge = ChargeIn;
    int i, ion = static_cast <int> ((kTermMass[IonType] + Charge - 1) / 
				    Charge*MSSCALE + 
				    kIonTypeMass[IonType]/Charge*MSSCALE);
    const int *IntMassArray = MassArray.GetIntMass();
    char *AAMap = AA.GetMap();

    Start = start;
    Stop = stop;
    Index = SeqIndex;  // gi or position in blastdb
    Type = IonType;
    Mass = static_cast <int> (mass - Charge*kProton*MSSCALE);

    int delta;
    int ModIndex;  // index into number of possible mod sites
    int Direction;
    int Offset;

    if(kIonDirection[IonType] == 1) {
        ModIndex = 0;
        Direction = 1;
        Offset = start;
    }
    else {
        ModIndex = NumMod - 1;
        Direction = -1;
        Offset = stop;
    }

    LadderIndex = stop - start;
    // make the ladder fit in memory for long peptides/proteins
    if(LadderIndex > LadderSize) LadderIndex = LadderSize;
    for(i = 0; i < LadderIndex; i++) {
        GetHit()[i] = 0;
        if(!CalcDelta(delta, IntMassArray, AAMap, Sequence,
              Offset, Direction, NumMod, ModIndex,
              Site, ModMask, DeltaMass, i)) return false;
        ion += delta/Charge;
        (*this)[i] = ion;
    }
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
    return false;
}
