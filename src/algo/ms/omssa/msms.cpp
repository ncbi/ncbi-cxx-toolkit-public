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
 * Authors:  Lewis Y. Geer
 *
 * File Description:
 *    Helper classes for ms search algorithms
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>

#include <fstream>

#include <msms.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  CAA::
//
//  lookup table for AA index
//

CAA::CAA(void) 
{
    int i;
    for(i = 0; i < 256; i++) AAMap[i] = kNumUniqueAA;
    for(i = 0; i < kNumUniqueAA; i++) {
	AAMap[UniqueAA[i]] = i;
	// deal with blast readdb encoding.
	AAMap[i] = i;
    }
}



/////////////////////////////////////////////////////////////////////////////
//
//  CCleave::
//
//  Classes for cleaving sequences quickly and computing masses
//


CCleave::CCleave(void): CleaveAt(0), kCleave(0)
{
    ProtonMass = static_cast <int> (kProton*MSSCALE);
    TermMass = static_cast <int> 
	((kTermMass[kCIon] + kTermMass[kXIon])*MSSCALE);
    Reverse = ReverseAA.GetMap();
}

// char based replacement for find_first_of()

int CCleave::findfirst(char* Seq, int Pos, int SeqLen)
{
    int i, j;
    for (i = Pos; i < SeqLen; i++)
	for(j = 0; j < kCleave; j++) 
	    if(Seq[i] == CleaveAt[j]) return i;
    return i;
}


void CTrypsin::Cleave(char *Seq, int SeqLen, TCleave& Positions)
{
    int Pos = 0;
    Positions.clear();
    Pos = findfirst(Seq, Pos, SeqLen);
    Positions.push_back(0);  // beginning of sequence
    while(Pos < SeqLen - 1) {
	if(Seq[Pos+1] == 'P' ||
	   Seq[Pos+1] == '\x0e' ) { // not before proline
	    Pos = findfirst(Seq, Pos+1, SeqLen);
	    continue;
	}
	Positions.push_back(Pos);
	Pos = findfirst(Seq, Pos+1, SeqLen);
    }
}


// - cuts trypsin and calculates mass using integer arithmetic
// - needs to take variable mods into account (i.e. different masses)
// - MassArray contains corrected masses for the fixed mods that
// are not position specific
// - FixedMods contain position specific mods.
//   - open question on how to deal with C terminal fixed mods.  E.g. how do
//     you know you are at the Cterm K?  Doesn't matter for mass, as ANY k will
//     increase total mass, but does matter for ladder.
// - charge is an array of ints for indeterminate charge states
// - the api will also eventually create or extend CLadders for large 
//   search datasets, as most peptides will be examined.
// dealing with missed cleavages:
// - starts with existing mass array
// - extends and shifts existing ladders
//
// - needs to be made into a general method
//

// returns true on end of sequence

// note that the coordinates are inclusive, i.e. [start, end]

bool CTrypsin::CalcAndCut(const char *SeqStart, 
			  const char *SeqEnd,  // the end, not beyond the end
			  const char **PepStart,  // return value
			  int **Masses,
			  int NumCleave,
			  int& NumMod,
			  int MaxNumMod,
			  int *EndMasses,
			  //			 vector <EMSMod>& VariableMods,
			  //			 vector <EMSMod>& FixedMods,
			  const int *IntCalcMass  // array of int AA masses
			  )
{
    char SeqChar;
    // iterate through sequence
    // note that this loop doesn't check at the end of the sequence
    for(;*PepStart < SeqEnd; (*PepStart)++) {
	SeqChar = **PepStart;

	// check for mods, methionine for now
	if((SeqChar == 'M' || SeqChar == '\x0c') && NumMod < MaxNumMod) {
	    NumMod++;
	    Masses[NumCleave][NumMod-1] = Masses[NumCleave][NumMod-2] +
		16*MSSCALE;
	}

	CalcMass(SeqChar, Masses, NumCleave, NumMod, IntCalcMass);

	// check for cleavage point
	if(SeqChar == CleaveAt[0] ||  
	   SeqChar == CleaveAt[1] ||
	   SeqChar == CleaveAt[2] ||
	   SeqChar == CleaveAt[3]
	   ) { 
	    if(*(*PepStart+1) == 'P' ||
	       *(*PepStart+1) == '\x0e' )  continue;  // not before proline
	    EndMass(EndMasses, NumCleave);
	    return false;
	}
    }

    // todo: deal with mods on the end

    CalcMass(**PepStart, Masses, NumCleave, NumMod, IntCalcMass);
    EndMass(EndMasses, NumCleave);
    return true;  // end of sequence
}

void CCNBr::Cleave(char *Seq, int SeqLen, TCleave& Positions)
{
    int Pos = 0;
    Positions.clear();
    Positions.push_back(0);  // beginning of sequence
    Pos = findfirst(Seq, Pos, SeqLen);
    while(Pos < SeqLen) {
	Positions.push_back(Pos);
	Pos = findfirst(Seq, Pos+1, SeqLen);
    }
}

void CFormicAcid::Cleave(char *Seq, int SeqLen, TCleave& Positions)
{
    int Pos = 0;
    Positions.clear();
    Pos = findfirst(Seq, Pos, SeqLen);
    Positions.push_back(0);  // beginning of sequence
    while(Pos < SeqLen - 1) {
	if(Seq[Pos+1] != 'P' &&
	   Seq[Pos+1] != '\x0e' ) { //  before proline
	    Pos = findfirst(Seq, Pos+1, SeqLen);
	    continue;
	}
	Positions.push_back(Pos);
	Pos = findfirst(Seq, Pos+1, SeqLen);
    }
}


/////////////////////////////////////////////////////////////////////////////
//
//  CMassArray::
//
//  Holds AA indexed mass array
//

CMassArray::CMassArray(bool Average)
{
    int i;
    if(Average) {
	for(i = 0; i < kNumUniqueAA; i++ ) {
	    CalcMass[i] = AverageMass[i];
	    IntCalcMass[i] = static_cast <int> 
		(AverageMass[i]*MSSCALE);
	}
    }
    else {
	for(i = 0; i < kNumUniqueAA; i++ ) {
	    CalcMass[i] = MonoMass[i];
	    IntCalcMass[i] = static_cast <int>
		(MonoMass[i]*MSSCALE);
	}
    }
}

/*
  $Log$
  Revision 1.2  2003/10/21 21:12:16  lewisg
  reorder headers

  Revision 1.1  2003/10/20 21:32:13  lewisg
  ommsa toolkit version

  Revision 1.11  2003/10/07 18:02:28  lewisg
  prep for toolkit

  Revision 1.10  2003/08/14 23:49:22  lewisg
  first pass at variable mod

  Revision 1.9  2003/07/17 18:45:49  lewisg
  multi dta support

  Revision 1.8  2003/03/21 21:14:40  lewisg
  merge ming's code, other stuff

  Revision 1.7  2003/02/10 19:37:55  lewisg
  perf and web page cleanup

  Revision 1.6  2003/01/21 21:55:51  lewisg
  fixes

  Revision 1.5  2003/01/21 21:46:13  lewisg
  *** empty log message ***

  Revision 1.4  2002/11/26 00:41:57  lewisg
  changes for msfilter

  Revision 1.3  2002/09/20 20:19:34  lewisg
  msms search update

  Revision 1.2  2002/07/16 13:26:23  lewisg
  *** empty log message ***

  Revision 1.1.1.1  2002/02/14 02:14:02  lewisg


*/





