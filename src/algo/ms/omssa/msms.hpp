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

#ifndef MSMS__HPP
#define MSMS__HPP

#ifdef WIN32
#pragma warning(disable:4786)
#endif

#include <list>
#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <deque>
#include <map>
#include <objects/omssa/MSSearchSettings.hpp>
#include "Mod.hpp"

// #include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)

const int kNumUniqueAA = 26;
// U is selenocystine
// this is NCBIstdAA.  Doesn't seem to be a central location to get this.
// all blast protein databases are encoded with this.
// U is selenocystine
const char * const UniqueAA = "-ABCDEFGHIKLMNPQRSTVWXYZU*";
//                             01234567890123456789012345
//                             0123456789abcdef0123456789

// non-redundified integer intervals of amino acids
const int kNumAAIntervals = 19;

// ABCXYZ ion mass calculation constants.  See Papayannopoulos, pg 63.
// need to add proton * charge also.
const int kAIon = 0, kBIon = 1, kCIon = 2, kXIon = 3, kYIon = 4,
    kZIon = 5;
const int kIonTypes = 6;
// proton mass
const double kProton = 1.008;
// direction.  1 = N->C, -1 = C->N
const double kWater = 18.015; 


const double AAAbundance[] = {1.0, 0.0758, 1.0, 0.0167, 0.0528, 0.0635, 0.0408, 0.0683, 0.0224, 0.058, 0.0593, 0.0943, 0.0237, 0.0447, 0.0491, 0.0399, 0.0514, 0.0715, 0.0569, 0.0656, 0.0124, 1.0, 0.0318, 1.0, 1.0, 1.0, 0.0}; 

// masses taken from Papayannopoulos, IA, Mass Spectrometry Reviews, 1995, 14, 49-73.
// monoisotopic mass
// C = 103.00919
const double MonoMass[] = {0.0, 71.03711, 115.02694, 103.00919, 115.02694, 129.04259, 147.06841, 57.02147, 137.05891, 113.08406, 128.09496, 113.08406, 131.04049, 114.04293, 97.05276, 128.05858, 156.10111, 87.03203, 101.04768, 99.06841, 186.07931, 0.0, 163.06333, 128.05858, 149.903 , 0.0, 0.0 };
// average mass
const double AverageMass[] = {0.0, 71.08, 0.0, 103.15, 115.09, 129.12, 147.18, 57.05, 137.14, 113.16, 128.17, 113.16, 131.20, 114.10, 97.12, 128.13, 156.19, 87.08, 101.11, 99.13, 186.21, 0.0, 163.18, 128.13, 150.034, 0.0, 0.0 };

const int AAIntervals[] = { 57, 71, 87, 97, 99, 101, 103, 113, 114, 115, 128, 129, 131, 137, 147, 150, 156, 163, 186 };

const double kTermMass[] =  {1.008, 1.008, 1.008, 17.007, 17.007, 17.007};
const double kIonTypeMass[] = { -28.01, 0.0, 16.023, 28.01, 2.016, 15.015 };
// direction.  1 = N->C, -1 = C->N
const int kIonDirection[] = { 1, 1, 1, -1, -1, -1 };

/////////////////////////////////////////////////////////////////////////////
//
//  CMassArray::
//
//  Holds AA indexed mass array
//

class NCBI_XOMSSA_EXPORT CMassArray {
public:
    CMassArray(void) {};

    const double* GetMass(void);
    const int* GetIntMass(void);

    // initialize mass arrays
    void Init(const CMSSearchSettings::TSearchtype &SearchType);
    // initialize mass arrays with fixed mods
    void Init(const CMSMod &Mods, 
	      const CMSSearchSettings::TSearchtype &SearchType);
private:
    // inits mass arrays
    void x_Init(const CMSSearchSettings::TSearchtype &SearchType);
    // masses as doubles
    double CalcMass[kNumUniqueAA];
    // mass in scaled integer Daltons
    int IntCalcMass[kNumUniqueAA];
    // Se mass is 78.96, S is 32.066
};

///////////////////   CMassArray inline methods

inline const double* CMassArray::GetMass(void) 
{ 
    return CalcMass; 
}

inline const int* CMassArray::GetIntMass(void) 
{ 
    return IntCalcMass; 
}

/////////////////// end of CMassArray inline methods


/////////////////////////////////////////////////////////////////////////////
//
//  CAA::
//
//  lookup table for AA index
//

// lookup table for reversing an AA character to AA number
class NCBI_XOMSSA_EXPORT CAA {
public:
    CAA(void);
    char *GetMap(void);
private:
    char AAMap[256];
};


///////////////////    CAA inline methods

inline char *CAA::GetMap(void) 
{
    return AAMap; 
}

/////////////////// end of CAA inline methods


/////////////////////////////////////////////////////////////////////////////
//
//  CCleave::
//
//  Classes for cleaving sequences quickly and computing masses
//
	
typedef std::deque <int> TCleave;

class NCBI_XOMSSA_EXPORT CCleave {
public:
    CCleave();
    virtual ~CCleave() {};

    // cleaves the sequence.  Note that output is 0 and the positions
    // of the aa's to be cleaved.  Should be interpreted as [0, pos1],
    // (pos1, pos2], ..., (posn, end].  This weirdness is historical --
    // the C++ string class uses an identifier for end-of-string and has
    // no identifier for before start of string.  
    virtual void Cleave(char *Seq, int SeqLen, TCleave& Positions) = 0;
    virtual bool CalcAndCut(const char *SeqStart, 
			    const char *SeqEnd,  // the end, not beyond the end
			    const char **PepStart,  // return value
			    int *Masses,  // Masses, indexed by miss cleav, mods
			    int& NumMod,   // num Mods
			    int MaxNumMod, // max num mods 
			    int *EndMasses,
			    CMSMod &VariableMods,
			    CMSMod &FixedMods,
			    const char **Site,
			    int *DeltaMass,
			    const int *IntCalcMass  // array of int AA masses
			    ) { return false; }
    
    void CalcMass(char SeqChar,
		  int *Masses,
		  const int *IntCalcMass
		  );
    
    void EndMass(int *Masses
		 );

    int findfirst(char* Seq, int Pos, int SeqLen);

    ///
    /// looks for non-specific ptms
    ///
    void CheckNonSpecificMods(EMSModType ModType, // the type of mod
			      CMSMod &VariableMods, // list of mods to look for
			      int& NumMod, // number of mods applied to peptide
			      int MaxNumMod,  // maximum mods for a peptide
			      const char **Site,  // list of mod sites
			      int *DeltaMass, // mass of mod
			      const char *iPepStart);  // position in protein

    ///
    /// looks for amino acid specific ptms
    ///
    void CheckAAMods(EMSModType ModType,  // the type of mod
		     CMSMod &VariableMods, // list of mods to look for
		     int& NumMod, // number of mods applied to peptide
		     char SeqChar,  // the amino acid
		     int MaxNumMod,  // maximum mods for a peptide
		     const char **Site,  // list of mod sites
		     int *DeltaMass, // mass of mod
		     const char *iPepStart);  // position in protein

protected:
    int ProtonMass; // mass of the proton
    int TermMass;  // mass of h2o
    CAA ReverseAA;
    char *Reverse;

    // where to cleave.  last two letters are in readdb format, assuming 
    // it uses the UniqueAA alphabet
    char *CleaveAt;
    int kCleave;
};


///////////////////    CCleave inline methods

inline
void CCleave::CalcMass(char SeqChar,
		       int *Masses,
		       const int *IntCalcMass
		       )
{
    //    int j;
    //  for(i = 0; i < NumMasses; i++)
    //    for(j = 0; j < NumMod; j++)
    *Masses += IntCalcMass[Reverse[SeqChar]];
}


inline
void CCleave::EndMass( int *EndMasses
		       )
{
    //  int i;
    //  for(i = 0; i < NumEndMasses; i++)   
    *EndMasses = TermMass;
}


inline
void CCleave::CheckAAMods(EMSModType ModType, CMSMod &VariableMods, int& NumMod,
			 char SeqChar, int MaxNumMod, const char **Site,
			 int *DeltaMass, const char *iPepStart)
{
    // iterator thru mods
    CMSSearchSettings::TVariable::const_iterator iMods;
    int iChar;

    for(iMods = VariableMods.GetAAMods(ModType).begin();
	iMods !=  VariableMods.GetAAMods(ModType).end(); iMods++) {
	for(iChar = 0; iChar < NumModChars[*iMods]; iChar++) {
	    if (SeqChar == ModChar[iChar][*iMods] && NumMod < MaxNumMod) {
		Site[NumMod] = iPepStart;
		DeltaMass[NumMod] = ModMass[*iMods];
		NumMod++; 
	    }
	}
    }
}


inline
void CCleave::CheckNonSpecificMods(EMSModType ModType, CMSMod &VariableMods,
				   int& NumMod, int MaxNumMod,
				   const char **Site,
				   int *DeltaMass, const char *iPepStart)
{
    // iterator thru mods
    CMSSearchSettings::TVariable::const_iterator iMods;

    for(iMods = VariableMods.GetAAMods(ModType).begin();
	iMods !=  VariableMods.GetAAMods(ModType).end(); iMods++) {
	if (NumMod < MaxNumMod) {
	    Site[NumMod] = iPepStart;
	    DeltaMass[NumMod] = ModMass[*iMods];
	    NumMod++; 
	}
    }
}

/////////////////// end of CCleave inline methods



class NCBI_XOMSSA_EXPORT CCNBr: public CCleave {
public:
    CCNBr();
    ~CCNBr();
    virtual void Cleave(char *Seq, int SeqLen, TCleave& Positions);
};

///////////////////   CCNBr inline methods

inline CCNBr::CCNBr() 
{
    CleaveAt = "\x0c";
    kCleave = 1;
}

inline CCNBr::~CCNBr() 
{}

/////////////////// end of  CCNBr inline methods



class NCBI_XOMSSA_EXPORT CFormicAcid: public CCleave {
public:
    CFormicAcid();
    ~CFormicAcid();
    virtual void Cleave(char *Seq, int SeqLen, TCleave& Positions);
};

///////////////////   CCNBr inline methods

inline CFormicAcid::CFormicAcid()
{
    CleaveAt = "\x04";
    kCleave = 1;
}

inline CFormicAcid::~CFormicAcid()
{}

/////////////////// end of  CCNBr inline methods


class NCBI_XOMSSA_EXPORT CTrypsin: public CCleave {
public:
    CTrypsin();
    ~CTrypsin();

    virtual void Cleave(char *Seq, int SeqLen, TCleave& Positions);
    virtual bool CalcAndCut(const char *SeqStart, 
			    const char *SeqEnd,  // the end, not beyond the end
			    const char **PepStart,  // return value
			    int *Masses,
			    int& NumMod,
			    int MaxNumMod,
			    int *EndMasses,
			    CMSMod &VariableMods,
			    CMSMod &FixedMods,
			    const char **Site,
			    int *DeltaMass,
			    const int *IntCalcMass  // array of int AA masses
			    );
};

///////////////////  CCNBr  inline methods

inline CTrypsin::CTrypsin() 
{
    CleaveAt = "\x0a\x10";
    kCleave = 2;
}

inline CTrypsin::~CTrypsin()
{}

/////////////////// end of  CCNBr inline methods



// used to scale all float masses into ints
#define MSSCALE 100

END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

/*
  $Log$
  Revision 1.8  2004/06/21 21:19:27  lewisg
  new mods (including n term) and sample perl parser

  Revision 1.7  2004/06/08 19:46:21  lewisg
  input validation, additional user settable parameters

  Revision 1.6  2004/03/30 19:36:59  lewisg
  multiple mod code

  Revision 1.5  2004/03/16 20:18:54  gorelenk
  Changed includes of private headers.

  Revision 1.4  2004/03/01 18:24:07  lewisg
  better mod handling

  Revision 1.3  2003/10/24 21:28:41  lewisg
  add omssa, xomssa, omssacl to win32 build, including dll

  Revision 1.2  2003/10/21 21:12:17  lewisg
  reorder headers

  Revision 1.1  2003/10/20 21:32:13  lewisg
  ommsa toolkit version

  Revision 1.12  2003/10/07 18:02:28  lewisg
  prep for toolkit

  Revision 1.11  2003/08/14 23:49:22  lewisg
  first pass at variable mod

  Revision 1.10  2003/07/17 18:45:49  lewisg
  multi dta support

  Revision 1.9  2003/05/01 14:52:10  lewisg
  fixes to scoring

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


















