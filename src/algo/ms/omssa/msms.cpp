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
	AAMap[static_cast <int> (UniqueAA[i])] = i;
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
                         CRef <CMSModSpecSet> &Modset,
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


CLysN::CLysN(void)
{
    CleaveAt = "\x0a";
    CleaveOffset = "\x01";
    CheckProline = false;
    kCleave = 1;
    TopDown = false;
    NonSpecific = false;
}

 
CThermolysinP::CThermolysinP(void) 
{ 
    CleaveAt = "\x01\x06\x09\x0b\x0c\x13"; 
    CleaveOffset = "\x01\x01\x01\x01\x01\x01"; 
    CheckProline = true; 
    kCleave = 6; 
    TopDown = false; 
    NonSpecific = false; 
} 


CSemiChymotrypsin::CSemiChymotrypsin(void)
{
    CleaveAt = "\x06\x16\x14\x0b";
    CleaveOffset = "\x00\x00\x00\x00";
    CheckProline = true;
    kCleave = 4;
    TopDown = false;
    NonSpecific = true;
}


CSemiGluC::CSemiGluC(void)
{
    CleaveAt = "\x05";
    CleaveOffset = "\x00";
    CheckProline = false;
    kCleave = 1;
    TopDown = false;
    NonSpecific = true;
}


/// 

///
/// Simple factory to return back object for enzyme
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
    case eMSEnzymes_lysn:
        return CRef <CCleave> (new CLysN);
        break;
    case eMSEnzymes_thermolysin_p: 
        return CRef <CCleave> (new CThermolysinP); 
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
    CMSMod::TModLists::const_iterator i;  // iterate thru fixed mods
    int j; // the number of characters affected by the fixed mod

    for(i = Mods.GetAAMods(eMSModType_modaa).begin(); i != Mods.GetAAMods(eMSModType_modaa).end(); i++) {
    	for(j = 0; j < Modset->GetModNumChars(*i); j++) {
    	    CalcMass[static_cast <int> (Modset->GetModChar(*i, j))] +=  MSSCALE2DBL(Modset->GetModMass(*i));
    	    IntCalcMass[static_cast <int> (Modset->GetModChar(*i, j))] += Modset->GetModMass(*i);
    	}
    }
}
