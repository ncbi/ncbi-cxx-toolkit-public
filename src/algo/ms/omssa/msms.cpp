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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <fstream>

#include "msms.hpp"

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


// - cuts and calculates mass using integer arithmetic
// - MassArray contains corrected masses for the fixed mods that
// are not position specific
// - FixedMods contain position specific mods.
//   - open question on how to deal with C terminal fixed mods.  E.g. how do
//     you know you are at the Cterm K?  Doesn't matter for mass, as ANY k will
//     increase total mass, but does matter for ladder.
// dealing with missed cleavages:
// - starts with existing mass array
// - returns true on end of sequence
// - note that the coordinates are inclusive, i.e. [start, end]

bool CCleave::CalcAndCut(const char *SeqStart, 
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
			  const int *IntCalcMass,  // array of int AA masses
              const int *PrecursorIntCalcMass, // precursor masses
              int *ModEnum,       // the mod type at each site
              int *IsFixed
			  )
{
    char SeqChar(**PepStart);

    // n terminus protein
    if(*PepStart == SeqStart) CheckMods(eModN, eModNAA, VariableMods, FixedMods,
                                        NumMod, SeqChar, MaxNumMod, Site,
                                        DeltaMass, *PepStart, ModEnum, IsFixed);

    // n terminus peptide
    CheckMods(eModNP, eModNPAA, VariableMods, FixedMods, NumMod, SeqChar, MaxNumMod, Site,
    				 DeltaMass, *PepStart, ModEnum, IsFixed);


    // iterate through sequence
    // note that this loop doesn't check at the end of the sequence
    for(; *PepStart < SeqEnd; (*PepStart)++) {
    	SeqChar = **PepStart;
    
    	// check for mods that are type AA only
    	CheckAAMods(eModAA, VariableMods, NumMod, SeqChar, MaxNumMod, Site,
    		    DeltaMass, *PepStart, ModEnum, IsFixed, false);
//    	CheckAAMods(eModAA, FixedMods, NumMod, SeqChar, MaxNumMod, Site,
//    		    DeltaMass, *PepStart, ModEnum, IsFixed, true);
    
    
    	CalcMass(SeqChar, Masses, PrecursorIntCalcMass);
    
    	// check for cleavage point
    	if(CheckCleave(SeqChar, *PepStart)) { 
            // check c term peptide mods
            CheckMods(eModCP, eModCPAA, VariableMods, FixedMods, NumMod, SeqChar, MaxNumMod, Site,
    				 DeltaMass, *PepStart, ModEnum, IsFixed);
    	    EndMass(EndMasses);
    	    return false;
    	}
    }

    // todo: deal with mods on the end

    CalcMass(**PepStart, Masses, PrecursorIntCalcMass);

    // check c term peptide mods
    CheckMods(eModCP, eModCPAA, VariableMods, FixedMods, NumMod, SeqChar, MaxNumMod, Site,
             DeltaMass, *PepStart, ModEnum, IsFixed);
    // check c term protein mods
    CheckMods(eModC, eModCAA, VariableMods, FixedMods, NumMod, SeqChar, MaxNumMod, Site,
             DeltaMass, *PepStart, ModEnum, IsFixed);

    EndMass(EndMasses);
    return true;  // end of sequence
}


///
///  CTrypsin
///

CTrypsin::CTrypsin(void)
{
    CleaveAt = "\x0a\x10";
    kCleave = 2;
}

bool CTrypsin::CheckCleave(char SeqChar, const char *iPepStart)
{
    // check for cleavage point
    if(SeqChar == CleaveAt[0] || SeqChar == CleaveAt[1] ) { 
        if(*(iPepStart+1) == '\x0e' )  return false;  // not before proline
        return true;
    }
    return false;
}


///
///  CNBr
///

CCNBr::CCNBr(void) 
{
    CleaveAt = "\x0c";
    kCleave = 1;
}

bool CCNBr::CheckCleave(char SeqChar, const char *iPepStart)
{
    // check for cleavage point
    if(SeqChar == CleaveAt[0]) { 
        return true;
    }
    return false;
}


///
///  CFormicAcid
///

CFormicAcid::CFormicAcid(void)
{
    CleaveAt = "\x04";
    kCleave = 1;
}

bool CFormicAcid::CheckCleave(char SeqChar, const char *iPepStart)
{
    // check for cleavage point
    if(SeqChar == CleaveAt[0]) { 
        return true;
    }
    return false;
}


///
///  CArgC
///

CArgC::CArgC(void)
{
    CleaveAt = "\x010";
    kCleave = 1;
}

bool CArgC::CheckCleave(char SeqChar, const char *iPepStart)
{
    // check for cleavage point
    if(SeqChar == CleaveAt[0] ) { 
        if(*(iPepStart+1) == '\x0e' )  return false;  // not before proline
        return true;
    }
    return false;
}


///
///  CChymotrypsin
///

CChymotrypsin::CChymotrypsin(void)
{
    CleaveAt = "\x06\x16\x15\x0b\x09\x13\x0c";
    kCleave = 7;
}

bool CChymotrypsin::CheckCleave(char SeqChar, const char *iPepStart)
{
    // check for cleavage point
    if(SeqChar == CleaveAt[0] ||
       SeqChar == CleaveAt[1] ||
       SeqChar == CleaveAt[2] ||
       SeqChar == CleaveAt[3] ||
       SeqChar == CleaveAt[4] ||
       SeqChar == CleaveAt[5] ||
       SeqChar == CleaveAt[6]) { 
        if(*(iPepStart+1) == '\x0e' )  return false;  // not before proline
        return true;
    }
    return false;
}


///
///  CLysC
///

CLysC::CLysC(void)
{
    CleaveAt = "\x0a";
    kCleave = 1;
}

bool CLysC::CheckCleave(char SeqChar, const char *iPepStart)
{
    // check for cleavage point
    if(SeqChar == CleaveAt[0] ) { 
        if(*(iPepStart+1) == '\x0e' )  return false;  // not before proline
        return true;
    }
    return false;
}


///
///  CLysCP
///

CLysCP::CLysCP(void)
{
    CleaveAt = "\x0a";
    kCleave = 1;
}

bool CLysCP::CheckCleave(char SeqChar, const char *iPepStart)
{
    // check for cleavage point
    if(SeqChar == CleaveAt[0]) { 
        return true;
    }
    return false;
}


///
///  CCPepsinA
///

CPepsinA::CPepsinA(void)
{
    CleaveAt = "\x06\x0b";
    kCleave = 2;
}

bool CPepsinA::CheckCleave(char SeqChar, const char *iPepStart)
{
    // check for cleavage point
    if(SeqChar == CleaveAt[0] || SeqChar == CleaveAt[1]) { 
        return true;
    }
    return false;
}


///
///  CTrypCNBr
///

CTrypCNBr::CTrypCNBr(void)
{
    CleaveAt = "\x0a\x10\x0c";
    kCleave = 3;
}

bool CTrypCNBr::CheckCleave(char SeqChar, const char *iPepStart)
{
    // check for cleavage point
    if(SeqChar == CleaveAt[0] ||
       SeqChar == CleaveAt[1] ||
       SeqChar == CleaveAt[2]) { 
        if(*(iPepStart+1) == '\x0e' )  return false;  // not before proline
        return true;
    }
    return false;
}


///
///  CTrypChymo
///

CTrypChymo::CTrypChymo(void)
{
    CleaveAt = "\x06\x16\x15\x0b\x0a\10";
    kCleave = 6;
}

bool CTrypChymo::CheckCleave(char SeqChar, const char *iPepStart)
{
    // check for cleavage point
    if(SeqChar == CleaveAt[0] ||
       SeqChar == CleaveAt[1] ||
       SeqChar == CleaveAt[2] ||
       SeqChar == CleaveAt[3] ||
       SeqChar == CleaveAt[4] ||
       SeqChar == CleaveAt[5]) { 
        if(*(iPepStart+1) == '\x0e' )  return false;  // not before proline
        return true;
    }
    return false;
}


///
///  CTrypsinP
///

CTrypsinP::CTrypsinP(void)
{
    CleaveAt = "\x0a\x10";
    kCleave = 2;
}

bool CTrypsinP::CheckCleave(char SeqChar, const char *iPepStart)
{
    // check for cleavage point
    if(SeqChar == CleaveAt[0] || SeqChar == CleaveAt[1] ) { 
        return true;
    }
    return false;
}

///
/// Simple minded factory to return back object for enzyme
///

CCleave *  CCleaveFactory::CleaveFactory(const EMSEnzymes enzyme)
{
  if(enzyme >= eMSEnzymes_max) return 0;
  switch(enzyme) {
  case eMSEnzymes_trypsin:
    return new CTrypsin;
    break;
  case eMSEnzymes_argc:
    return new CArgC;
    break;
  case eMSEnzymes_cnbr:
    return new CCNBr;
    break;
  case eMSEnzymes_chymotrypsin:
    return new CChymotrypsin;
    break;
  case eMSEnzymes_formicacid:
    return new CFormicAcid;
    break;
  case eMSEnzymes_lysc:
    return new CLysC;
    break;
  case eMSEnzymes_lysc_p:
    return new CLysCP;
    break;
  case eMSEnzymes_pepsin_a:
    return new CPepsinA;
    break;
  case eMSEnzymes_tryp_cnbr:
    return new CTrypCNBr;
    break;
  case eMSEnzymes_tryp_chymo:
    return new CTrypChymo;
    break;
  case eMSEnzymes_trypsin_p:
    return new CTrypsinP;
    break;
  default:
    return 0;
    break;
  }
  return 0;
}
     


/////////////////////////////////////////////////////////////////////////////
//
//  CMassArray::
//
//  Holds AA indexed mass array
//

void CMassArray::Init(const CMSSearchSettings::TProductsearchtype &SearchType)
{
    x_Init(SearchType);
}

void CMassArray::x_Init(const CMSSearchSettings::TProductsearchtype &SearchType)
{
    int i;
    if(SearchType == eMSSearchType_average) {
	for(i = 0; i < kNumUniqueAA; i++ ) {
	    CalcMass[i] = AverageMass[i];
	    IntCalcMass[i] = static_cast <int> 
		(AverageMass[i]*MSSCALE);
	}
    }
    else if(SearchType == eMSSearchType_monoisotopic) {
	for(i = 0; i < kNumUniqueAA; i++ ) {
	    CalcMass[i] = MonoMass[i];
	    IntCalcMass[i] = static_cast <int>
		(MonoMass[i]*MSSCALE);
	}
    }
    else if(SearchType == eMSSearchType_monon15) {
     for(i = 0; i < kNumUniqueAA; i++ ) {
         CalcMass[i] = MonoN15Mass[i];
         IntCalcMass[i] = static_cast <int>
         (MonoN15Mass[i]*MSSCALE);
     }
     }
}

// set up the mass array with fixed mods
void CMassArray::Init(const CMSMod &Mods, 
		      const CMSSearchSettings::TProductsearchtype &SearchType)
{
    x_Init(SearchType);
    CMSSearchSettings::TVariable::const_iterator i;  // iterate thru fixed mods
    int j; // the number of characters affected by the fixed mod

    for(i = Mods.GetAAMods(eModAA).begin(); i != Mods.GetAAMods(eModAA).end(); i++) {
	for(j = 0; j < NumModChars[*i]; j++) {
	    CalcMass[ModChar[j][*i]] +=  ModMass[*i]/(double)MSSCALE;
	    IntCalcMass[ModChar[j][*i]] += ModMass[*i];
	}
    }
}


/*
  $Log$
  Revision 1.16  2005/01/31 17:30:57  lewisg
  adjustable intensity, z dpendence of precursor mass tolerance

  Revision 1.15  2005/01/11 21:08:43  lewisg
  average mass search

  Revision 1.14  2004/11/22 23:10:36  lewisg
  add evalue cutoff, fix fixed mods

  Revision 1.13  2004/11/17 23:42:11  lewisg
  add cterm pep mods, fix prob for tophitnum

  Revision 1.12  2004/11/01 22:04:12  lewisg
  c-term mods

  Revision 1.11  2004/09/29 19:43:09  lewisg
  allow setting of ions

  Revision 1.10  2004/07/22 22:22:58  lewisg
  output mods

  Revision 1.9  2004/06/23 22:34:36  lewisg
  add multiple enzymes

  Revision 1.8  2004/06/21 21:19:27  lewisg
  new mods (including n term) and sample perl parser

  Revision 1.7  2004/06/08 19:46:21  lewisg
  input validation, additional user settable parameters

  Revision 1.6  2004/05/21 21:41:03  gorelenk
  Added PCH ncbi_pch.hpp

  Revision 1.5  2004/03/30 19:36:59  lewisg
  multiple mod code

  Revision 1.4  2004/03/16 20:18:54  gorelenk
  Changed includes of private headers.

  Revision 1.3  2004/03/01 18:24:07  lewisg
  better mod handling

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





