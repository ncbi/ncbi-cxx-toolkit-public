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
#include "SpectrumSet.hpp"

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

// direction.  1 = N->C, -1 = C->N
const double kWater = 18.015; 


const double AAAbundance[] = {1.0, 0.0758, 1.0, 0.0167, 0.0528, 0.0635, 0.0408, 0.0683, 0.0224, 0.058, 0.0593, 0.0943, 0.0237, 0.0447, 0.0491, 0.0399, 0.0514, 0.0715, 0.0569, 0.0656, 0.0124, 1.0, 0.0318, 1.0, 1.0, 1.0, 0.0}; 

// masses taken from Papayannopoulos, IA, Mass Spectrometry Reviews, 1995, 14, 49-73.
// selenocysteine calculated by using cysteine mass and adding difference between Se and S from webelements.  
// monoisotopic mass
const double MonoMass[] = {0.0, 71.03711, 0.0, 103.00919, 115.02694, 129.04259, 147.06841, 57.02147, 137.05891, 113.08406, 128.09496, 113.08406, 131.04049, 114.04293, 97.05276, 128.05858, 156.10111, 87.03203, 101.04768, 99.06841, 186.07931, 0.0, 163.06333, 0.0, 149.903 , 0.0, 0.0 };
// average mass
const double AverageMass[] = {0.0, 71.08, 0.0, 103.15, 115.09, 129.12, 147.18, 57.05, 137.14, 113.16, 128.17, 113.16, 131.20, 114.10, 97.12, 128.13, 156.19, 87.08, 101.11, 99.13, 186.21, 0.0, 163.18, 0.0, 150.044, 0.0, 0.0 };
// n15 enriched monoisotopic mass
const double MonoN15Mass[] = {0.0, 72.034144893, 0.0, 104.006224893, 116.023974893, 130.039624893, 148.065444893, 58.018494893, 140.050014679, 114.081094893, 130.089029786, 114.081094893, 132.037524893, 116.036999786, 98.049794893, 130.052649786, 160.089239572, 88.029064893, 102.044714893, 100.065444893, 188.073379786, 0.0, 164.060364893, 0.0, 150.8964, 0.0, 0.0 };


const int AAIntervals[] = { 57, 71, 87, 97, 99, 101, 103, 113, 114, 115, 128, 129, 131, 137, 147, 150, 156, 163, 186 };

const double kTermMass[] =  {1.008, 1.008, 1.008, 17.007, 17.007, 17.007};
const double kIonTypeMass[] = { -28.01, 0.0, 17.01, 30.026, 2.016, -13.985 };
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
    void Init(const CMSSearchSettings::TProductsearchtype &SearchType);
    // initialize mass arrays with fixed mods
    void Init(const CMSMod &Mods, 
	      const CMSSearchSettings::TProductsearchtype &SearchType);
private:
    // inits mass arrays
    void x_Init(const CMSSearchSettings::TProductsearchtype &SearchType);
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
    CCleave(void);

    // cleaves the sequence.  Note that output is 0 and the positions
    // of the aa's to be cleaved.  Should be interpreted as [0, pos1],
    // (pos1, pos2], ..., (posn, end].  This weirdness is historical --
    // the C++ string class uses an identifier for end-of-string and has
    // no identifier for before start of string.  

    bool CalcAndCut(const char *SeqStart, 
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
			    const int *IntCalcMass,  // array of int AA masses
                const int *PrecursorIntCalcMass, // precursor masses
                int *ModEnum,     // the mod type at each site
                int *IsFixed
		    );

    ///
    ///  Check to see if we are at a cleavage point
    ///  Used by CalcAndCut
    ///

    virtual bool CheckCleave(char SeqChar, const char *iPepStart) = 0;

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
			      const char *iPepStart, // position in protein
                  int *ModEnum,   // enum of mod
                  int *IsFixed,
                  bool setfixed
                  ); 

    ///
    /// looks for amino acid specific ptms
    ///
    void CheckAAMods(EMSModType ModType, // the type of mod
		     CMSMod &VariableMods, // list of mods to look for
		     int& NumMod, // number of mods applied to peptide
		     char SeqChar,  // the amino acid
		     int MaxNumMod,  // maximum mods for a peptide
		     const char **Site,  // list of mod sites
		     int *DeltaMass, // mass of mod
		     const char *iPepStart,  // position in protein
             int *ModEnum,   // enum of mod
             int *IsFixed,
             bool setfixed
             );

///
/// checks all mods for a particular type
///
    void CheckMods(EMSModType NonSpecific, EMSModType Specific,
                   CMSMod &VariableMods, CMSMod &FixedMods,
				   int& NumMod, char SeqChar, int MaxNumMod,
				   const char **Site,
				   int *DeltaMass, const char *iPepStart,
                   int *ModEnum, int *IsFixed );

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
			 int *DeltaMass, const char *iPepStart,
        int *ModEnum, int *IsFixed, bool setfixed)
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
        ModEnum[NumMod] = *iMods;
        if(setfixed) IsFixed[NumMod] = 1;
        else IsFixed[NumMod] = 0;
		NumMod++; 
	    }
	}
    }
}


inline
void CCleave::CheckNonSpecificMods(EMSModType ModType, CMSMod &VariableMods,
				   int& NumMod, int MaxNumMod,
				   const char **Site,
				   int *DeltaMass, const char *iPepStart,
        int *ModEnum, int *IsFixed, bool setfixed)
{
    // iterator thru mods
    CMSSearchSettings::TVariable::const_iterator iMods;

    for(iMods = VariableMods.GetAAMods(ModType).begin();
	iMods !=  VariableMods.GetAAMods(ModType).end(); iMods++) {
	if (NumMod < MaxNumMod) {
	    Site[NumMod] = iPepStart;
	    DeltaMass[NumMod] = ModMass[*iMods];
        ModEnum[NumMod] = *iMods;
        if(setfixed) IsFixed[NumMod] = 1;
        else IsFixed[NumMod] = 0;
	    NumMod++; 
	}
    }
}

inline
void CCleave::CheckMods(EMSModType NonSpecific, EMSModType Specific,
                        CMSMod &VariableMods, CMSMod &FixedMods,
                        int& NumMod, char SeqChar, int MaxNumMod,
                        const char **Site,
                        int *DeltaMass, const char *iPepStart,
                        int *ModEnum, int *IsFixed)
{
    // check non-specific mods
    CheckNonSpecificMods(NonSpecific, VariableMods, NumMod, MaxNumMod, Site,
                 DeltaMass, iPepStart, ModEnum, IsFixed, false);
    CheckNonSpecificMods(NonSpecific, FixedMods, NumMod, MaxNumMod, Site,
                 DeltaMass, iPepStart, ModEnum, IsFixed, true);
    // check specific mods
    CheckAAMods(Specific, VariableMods, NumMod, SeqChar, MaxNumMod,
            Site, DeltaMass, iPepStart, ModEnum, IsFixed, false);
}


/////////////////// end of CCleave inline methods



class NCBI_XOMSSA_EXPORT CCNBr: public CCleave {
public:
    CCNBr(void);

    virtual bool CheckCleave(char SeqChar, const char *iPepStart);
};


class NCBI_XOMSSA_EXPORT CFormicAcid: public CCleave {
public:
    CFormicAcid(void);

    virtual bool CheckCleave(char SeqChar, const char *iPepStart);
};


class NCBI_XOMSSA_EXPORT CTrypsin: public CCleave {
public:
    CTrypsin(void);

    virtual bool CheckCleave(char SeqChar, const char *iPepStart);
};


class NCBI_XOMSSA_EXPORT CArgC: public CCleave {
public:
    CArgC(void);

    virtual bool CheckCleave(char SeqChar, const char *iPepStart);
};


class NCBI_XOMSSA_EXPORT CChymotrypsin: public CCleave {
public:
    CChymotrypsin(void);

    virtual bool CheckCleave(char SeqChar, const char *iPepStart);
};


class NCBI_XOMSSA_EXPORT CLysC: public CCleave {
public:
    CLysC(void);

    virtual bool CheckCleave(char SeqChar, const char *iPepStart);
};


class NCBI_XOMSSA_EXPORT CLysCP: public CCleave {
public:
    CLysCP(void);

    virtual bool CheckCleave(char SeqChar, const char *iPepStart);
};


class NCBI_XOMSSA_EXPORT CPepsinA: public CCleave {
public:
    CPepsinA(void);

    virtual bool CheckCleave(char SeqChar, const char *iPepStart);
};


class NCBI_XOMSSA_EXPORT CTrypCNBr: public CCleave {
public:
    CTrypCNBr(void);

    virtual bool CheckCleave(char SeqChar, const char *iPepStart);
};


class NCBI_XOMSSA_EXPORT CTrypChymo: public CCleave {
public:
    CTrypChymo(void);

    virtual bool CheckCleave(char SeqChar, const char *iPepStart);
};


class NCBI_XOMSSA_EXPORT CTrypsinP: public CCleave {
public:
    CTrypsinP(void);

    virtual bool CheckCleave(char SeqChar, const char *iPepStart);
};


///
/// factory to return back object for enzyme
///

class NCBI_XOMSSA_EXPORT CCleaveFactory
{
public:
  static CCleave *CleaveFactory(const EMSEnzymes enzyme);

};


///
/// the names of the enzymes codified in the asn.1
///

char const * const kEnzymeNames[eMSEnzymes_max] = {
    "Trypsin",
    "Arg-C",
    "CNBr",
    "Chymotrypsin",
    "Formic Acid",
    "Lys-C",
    "Lys-C/P",
    "Pepsin A",
    "Trypsin-CNBr",
    "Trypsin-Chymotrypsin",
    "Trypsin/P"
};


char const * const kIonLabels[eMSIonType_max] = { 
    "a",
    "b", 
    "c",
    "x", 
    "y", 
    "z"
};

// used to scale all float masses into ints
#define MSSCALE 100

END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

/*
  $Log$
  Revision 1.18  2005/01/31 17:30:57  lewisg
  adjustable intensity, z dpendence of precursor mass tolerance

  Revision 1.17  2005/01/11 21:08:43  lewisg
  average mass search

  Revision 1.16  2004/12/06 22:57:34  lewisg
  add new file formats

  Revision 1.15  2004/12/03 21:14:16  lewisg
  file loading code

  Revision 1.14  2004/11/22 23:10:36  lewisg
  add evalue cutoff, fix fixed mods

  Revision 1.13  2004/11/17 23:42:11  lewisg
  add cterm pep mods, fix prob for tophitnum

  Revision 1.12  2004/09/29 19:43:09  lewisg
  allow setting of ions

  Revision 1.11  2004/09/15 18:35:00  lewisg
  cz ions

  Revision 1.10  2004/07/22 22:22:58  lewisg
  output mods

  Revision 1.9  2004/06/23 22:34:36  lewisg
  add multiple enzymes

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


















