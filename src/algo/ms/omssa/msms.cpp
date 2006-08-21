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
#include "omssascore.hpp"

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


CCleave::CCleave(void): CleaveAt(0), kCleave(0), NMethionine(false)
{
    ProtonMass = MSSCALE2INT(kProton);
    TermMass = MSSCALE2INT(kWater);
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


/** 
 * cleaves the sequence.  Note that output is 0 and the positions
 * of the aa's to be cleaved.  Should be interpreted as [0, pos1],
 * (pos1, pos2], ..., (posn, end].  This weirdness is historical --
 * the C++ string class uses an identifier for end-of-string and has
 * no identifier for before start of string.  
 * 
 * @param SeqStart pointer to start of sequence
 * @param SeqEnd pointer to end of sequence
 * @param PepStart ** to the start of peptide
 * @param Masses cumulative masses of peptides
 * @param NumMod number of variable mods
 * @param MaxNumMod upper bound on number of variable mods
 * @param EndMass the end masses of the peptides
 * @param VariableMods list of variable mods
 * @param FixedMods list of fixed modifications
 * @param ModList mod site info
 * @param IntCalcMass integer AA masses
 * @param PrecursorIntCalcMass integer precursor masses
 * @param Modset list of possible mods
 * @param Maxproductions max number of product ions to calculate
 * @return true if end of sequence
 */

bool CCleave::CalcAndCut(const char *SeqStart, 
                         const char *SeqEnd,  // the end, not beyond the end
                         const char **PepStart,  // return value
                         int *Masses,
                         int& NumMod,
                         int MaxNumMod,
                         int *EndMasses,
                         CMSMod &VariableMods,
                         CMSMod &FixedMods,
                         CMod ModList[],
                         const int *IntCalcMass,  // array of int AA masses
                         const int *PrecursorIntCalcMass, // precursor masses
                         CRef <CMSModSpecSet> Modset,
                         int Maxproductions
                         )
{
    char SeqChar(**PepStart);

    // n terminus protein mods
    // allow alternative start if n-term methionine cleavage
    if(*PepStart == SeqStart ||
       (*PepStart == SeqStart+1 && NMethionine && *SeqStart == '\x0c'))
        CheckMods(eMSModType_modn, eMSModType_modnaa, VariableMods, FixedMods,
                  NumMod, SeqChar, MaxNumMod, ModList,
                  *PepStart, Modset);

    // n terminus peptide
    CheckMods(eMSModType_modnp, eMSModType_modnpaa, VariableMods, FixedMods, NumMod, SeqChar, MaxNumMod, ModList,
              *PepStart, Modset);


    // iterate through sequence
    // note that this loop doesn't check at the end of the sequence
    for(; *PepStart < SeqEnd; (*PepStart)++) {
    	SeqChar = **PepStart;

        // if top-down enzyme
        // if > maxproductions * 2 from beginning and < maxproductions * 2 from end
        // skip checking for mods

        if(!GetTopDown()  ||
           (GetTopDown() && ( *PepStart - SeqStart < 2*Maxproductions ||
           SeqEnd - *PepStart < 2*Maxproductions))) {
            // check for mods that are type AA only, variable only
            CheckAAMods(eMSModType_modaa, VariableMods, NumMod, SeqChar, MaxNumMod, ModList,
                        *PepStart, false, Modset);
        }
    
    	CalcMass(SeqChar, Masses, PrecursorIntCalcMass);
    
    	// check for cleavage point
    	if(CheckCleave(*PepStart, SeqStart)) { 
            // check c term peptide mods
            CheckMods(eMSModType_modcp, eMSModType_modcpaa, VariableMods, FixedMods, NumMod, SeqChar, MaxNumMod, 
                      ModList, *PepStart, Modset);
    	    EndMass(EndMasses);
    	    return false;
    	}
    }

    // todo: deal with mods on the end

    CalcMass(**PepStart, Masses, PrecursorIntCalcMass);

    // check for mods that are type AA only, variable only
    CheckAAMods(eMSModType_modaa, VariableMods, NumMod, **PepStart, MaxNumMod, ModList,
                *PepStart, false, Modset);
    // check c term peptide mods
    CheckMods(eMSModType_modcp, eMSModType_modcpaa, VariableMods, FixedMods, NumMod, SeqChar, MaxNumMod, ModList,
              *PepStart, Modset);
    // check c term protein mods
    CheckMods(eMSModType_modc, eMSModType_modcaa, VariableMods, FixedMods, NumMod, SeqChar, MaxNumMod, ModList,
              *PepStart, Modset);

    EndMass(EndMasses);
    return true;  // end of sequence
}


///
///  CTrypsin
///

CTrypsin::CTrypsin(void)
{
    CleaveAt = "\x0a\x10";
    CleaveOffset = "\x00\x00";
    CheckProline = true;
    kCleave = 2;
    TopDown = false;
    NonSpecific = false;
}


///
///  CNBr
///

CCNBr::CCNBr(void) 
{
    CleaveAt = "\x0c";
    CleaveOffset = "\x00";
    CheckProline = false;
    kCleave = 1;
    TopDown = false;
    NonSpecific = false;
}


///
///  CFormicAcid
///

CFormicAcid::CFormicAcid(void)
{
    CleaveAt = "\x04";
    CleaveOffset = "\x00";
    CheckProline = false;
    kCleave = 1;
    TopDown = false;
    NonSpecific = false;
}


///
///  CArgC
///

CArgC::CArgC(void)
{
    CleaveAt = "\x010";
    CleaveOffset = "\x00";
    CheckProline = true;
    kCleave = 1;
    TopDown = false;
    NonSpecific = false;
}


///
///  CChymotrypsin
///

CChymotrypsin::CChymotrypsin(void)
{
    CleaveAt = "\x06\x16\x14\x0b";
    CleaveOffset = "\x00\x00\x00\x00";
    CheckProline = true;
    kCleave = 4;
    TopDown = false;
    NonSpecific = false;
}


///
///  CLysC
///

CLysC::CLysC(void)
{
    CleaveAt = "\x0a";
    CleaveOffset = "\x00";
    CheckProline = true;
    kCleave = 1;
    TopDown = false;
    NonSpecific = false;
}


///
///  CLysCP
///

CLysCP::CLysCP(void)
{
    CleaveAt = "\x0a";
    CleaveOffset = "\x00";
    CheckProline = false;
    kCleave = 1;
    TopDown = false;
    NonSpecific = false;
}


///
///  CCPepsinA
///

CPepsinA::CPepsinA(void)
{
    CleaveAt = "\x06\x0b";
    CleaveOffset = "\x00\x00";
    CheckProline = false;
    kCleave = 2;
    TopDown = false;
    NonSpecific = false;
}


///
///  CTrypCNBr
///

CTrypCNBr::CTrypCNBr(void)
{
    CleaveAt = "\x0a\x10\x0c";
    CleaveOffset = "\x00\x00\x00";
    CheckProline = true;
    kCleave = 3;
    TopDown = false;
    NonSpecific = false;
}


///
///  CTrypChymo
///

CTrypChymo::CTrypChymo(void)
{
    CleaveAt = "\x06\x16\x14\x0b\x0a\x10";
    CleaveOffset = "\x00\x00\x00\x00\x00\x00";
    CheckProline = true;
    kCleave = 6;
    TopDown = false;
    NonSpecific = false;
}


///
///  CTrypsinP
///

CTrypsinP::CTrypsinP(void)
{
    CleaveAt = "\x0a\x10";
    CleaveOffset = "\x00\x00";
    CheckProline = false;
    kCleave = 2;
    TopDown = false;
    NonSpecific = false;
}


//!  Whole Protein

CWholeProtein::CWholeProtein(void)
{
    CleaveAt = "\x00";
    CleaveOffset = "\x00";
    CheckProline = false;
    kCleave = 0;
    // this is *not* top down as we want to match the precursor m/z
    TopDown = false;
    NonSpecific = false;
}


//!  CAspN

CAspN::CAspN(void)
{
    CleaveAt = "\x04";
    CleaveOffset = "\x01";
    CheckProline = false;
    kCleave = 1;
    TopDown = false;
    NonSpecific = false;
}


//!  CGluC

CGluC::CGluC(void)
{
    CleaveAt = "\x05";
    CleaveOffset = "\x00";
    CheckProline = false;
    kCleave = 1;
    TopDown = false;
    NonSpecific = false;
}


//!  GluCAspN

CGluCAspN::CGluCAspN(void)
{
    CleaveAt = "\x05\x04";
    CleaveOffset = "\x00\x01";
    CheckProline = false;
    kCleave = 2;
    TopDown = false;
    NonSpecific = false;
}


/**
 *   Top-down, whole protein search
 */

CTopDown::CTopDown(void)
{
    CleaveAt = "\x00";
    CleaveOffset = "\x00";
    CheckProline = false;
    kCleave = 0;
    TopDown = true;
    NonSpecific = false;
}


/**
 *   SemiTryptic
 */

CSemiTryptic::CSemiTryptic(void)
{
    CleaveAt = "\x0a\x10";
    CleaveOffset = "\x00\x00";
    CheckProline = true;
    kCleave = 2;
    TopDown = false;
    NonSpecific = true;
}


/**
 *   NoEnzyme
 */

CNoEnzyme::CNoEnzyme(void)
{
    CleaveAt = "\x00";
    CleaveOffset = "\x00";
    CheckProline = false;
    kCleave = 0;
    TopDown = false;
    NonSpecific = true;
}


/**
 *  Chymotrypsin without proline rule
 */

CChymoP::CChymoP(void)
{
    CleaveAt = "\x06\x16\x14\x0b";
    CleaveOffset = "\x00\x00\x00\x00";
    CheckProline = false;
    kCleave = 4;
    TopDown = false;
    NonSpecific = false;
}


CAspNDE::CAspNDE(void)
{
    CleaveAt = "\x04\x05";
    CleaveOffset = "\x01\x01";
    CheckProline = false;
    kCleave = 2;
    TopDown = false;
    NonSpecific = false;
}


CGluCDE::CGluCDE(void)
{
    CleaveAt = "\x05\x04";
    CleaveOffset = "\x00\x00";
    CheckProline = false;
    kCleave = 2;
    TopDown = false;
    NonSpecific = false;
}


///
/// Simple minded factory to return back object for enzyme
///

CRef <CCleave>  CCleaveFactory::CleaveFactory(const EMSEnzymes enzyme)
{
    if (enzyme >= eMSEnzymes_max) return null;
    switch (enzyme) {
    case eMSEnzymes_trypsin:
        return CRef <CCleave> (new CTrypsin);
        break;
    case eMSEnzymes_argc:
        return CRef <CCleave> (new CArgC);
        break;
    case eMSEnzymes_cnbr:
        return CRef <CCleave> (new CCNBr);
        break;
    case eMSEnzymes_chymotrypsin:
        return CRef <CCleave> (new CChymotrypsin);
        break;
    case eMSEnzymes_formicacid:
        return CRef <CCleave> (new CFormicAcid);
        break;
    case eMSEnzymes_lysc:
        return CRef <CCleave> (new CLysC);
        break;
    case eMSEnzymes_lysc_p:
        return CRef <CCleave> (new CLysCP);
        break;
    case eMSEnzymes_pepsin_a:
        return CRef <CCleave> (new CPepsinA);
        break;
    case eMSEnzymes_tryp_cnbr:
        return CRef <CCleave> (new CTrypCNBr);
        break;
    case eMSEnzymes_tryp_chymo:
        return CRef <CCleave> (new CTrypChymo);
        break;
    case eMSEnzymes_trypsin_p:
        return CRef <CCleave> (new CTrypsinP);
        break;
    case eMSEnzymes_whole_protein:
        return CRef <CCleave> (new CWholeProtein);
        break;
    case eMSEnzymes_aspn:
        return CRef <CCleave> (new CAspN);
        break;
    case eMSEnzymes_gluc:
        return CRef <CCleave> (new CGluC);
        break;
    case eMSEnzymes_aspngluc:
        return CRef <CCleave> (new CGluCAspN);
        break;
    case eMSEnzymes_top_down:
        return CRef <CCleave> (new CTopDown);
        break;
    case eMSEnzymes_semi_tryptic:
        return CRef <CCleave> (new CSemiTryptic);
        break;
    case eMSEnzymes_no_enzyme:
        return CRef <CCleave> (new CNoEnzyme);
        break;
    case eMSEnzymes_chymotrypsin_p:
        return CRef <CCleave> (new CChymoP);
        break;
    case eMSEnzymes_aspn_de:
        return CRef <CCleave> (new CAspNDE);
        break;
    case eMSEnzymes_gluc_de:
        return CRef <CCleave> (new CGluCDE);
        break;
    default:
        return null;
        break;
    }
    return null;
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
	    IntCalcMass[i] = MSSCALE2INT(AverageMass[i]);
	}
    }
    else if(SearchType == eMSSearchType_monoisotopic || 
            SearchType == eMSSearchType_exact) {
	for(i = 0; i < kNumUniqueAA; i++ ) {
	    CalcMass[i] = MonoMass[i];
	    IntCalcMass[i] = MSSCALE2INT(MonoMass[i]);
	}
    }
    else if(SearchType == eMSSearchType_monon15) {
     for(i = 0; i < kNumUniqueAA; i++ ) {
         CalcMass[i] = MonoN15Mass[i];
         IntCalcMass[i] = MSSCALE2INT(MonoN15Mass[i]);
     }
     }
}

// set up the mass array with fixed mods
void CMassArray::Init(const CMSMod &Mods, 
		      const CMSSearchSettings::TProductsearchtype &SearchType,
                      CRef <CMSModSpecSet> Modset)
{
    if(!Modset || !Modset->IsArrayed()) {
        ERR_POST(Error << "CMassArray::Init: unable to use modification set");
        return;
    }
    x_Init(SearchType);
    CMSSearchSettings::TVariable::const_iterator i;  // iterate thru fixed mods
    int j; // the number of characters affected by the fixed mod

    for(i = Mods.GetAAMods(eMSModType_modaa).begin(); i != Mods.GetAAMods(eMSModType_modaa).end(); i++) {
    	for(j = 0; j < Modset->GetModNumChars(*i); j++) {
    	    CalcMass[Modset->GetModChar(*i, j)] +=  MSSCALE2DBL(Modset->GetModMass(*i));
    	    IntCalcMass[Modset->GetModChar(*i, j)] += Modset->GetModMass(*i);
    	}
    }
}


/*
  $Log$
  Revision 1.34  2006/08/21 15:18:21  lewisg
  asn.1 changes, bug fixes

  Revision 1.33  2006/07/28 14:21:05  lewisg
  new ladder container

  Revision 1.32  2006/05/25 17:10:18  lewisg
  one filtered spectrum per precursor charge state

  Revision 1.31  2006/01/23 17:47:37  lewisg
  refactor scoring

  Revision 1.30  2005/12/06 18:51:02  lewisg
  arg-c cuts at c terminus

  Revision 1.29  2005/11/07 19:57:20  lewisg
  iterative search

  Revision 1.28  2005/10/24 21:46:13  lewisg
  exact mass, peptide size limits, validation, code cleanup

  Revision 1.27  2005/09/15 21:29:24  lewisg
  filter out n-term protein mods

  Revision 1.26  2005/09/14 17:46:11  lewisg
  treat n-term methionine cut as cleavage

  Revision 1.25  2005/09/14 15:30:17  lewisg
  neutral loss

  Revision 1.24  2005/08/15 14:24:56  lewisg
  new mod, enzyme; stat test

  Revision 1.23  2005/08/03 17:59:29  lewisg
  *** empty log message ***

  Revision 1.22  2005/08/01 13:44:18  lewisg
  redo enzyme classes, no-enzyme, fix for fixed mod enumeration

  Revision 1.21  2005/05/19 16:59:17  lewisg
  add top-down searching, fix variable mod bugs

  Revision 1.20  2005/05/13 17:57:17  lewisg
  one mod per site and bug fixes

  Revision 1.19  2005/04/21 21:54:03  lewisg
  fix Jeri's mem bug, split off mod file, add aspn and gluc

  Revision 1.18  2005/04/05 21:02:52  lewisg
  increase number of mods, fix gi problem, fix empty scan bug

  Revision 1.17  2005/03/14 22:29:54  lewisg
  add mod file input

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





