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
*    code to do the ms/ms search and score matches
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbifloat.h>
#include <util/math/miscmath.h>
#include <algo/blast/core/ncbi_math.h>

#include <fstream>
#include <string>
#include <list>
#include <deque>
#include <algorithm>
#include <math.h>

#include "SpectrumSet.hpp"
#include "omssa.hpp"

// #include <ncbimath.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  CSearch::
//
//  Performs the ms/ms search
//


CSearch::CSearch(void): UseRankScore(false)
{
}

int CSearch::InitBlast(const char *blastdb)
{
    if(!blastdb) return 0;
    try {
        rdfp.Reset(new CSeqDB(blastdb, CSeqDB::eProtein));
        numseq = rdfp->GetNumOIDs();
    }
    catch(const exception &e) {
        ERR_POST(Critical << "Unable to open blast library " << blastdb << " with error:" <<
                  e.what());
        return 1;
    } catch(...) {
        ERR_POST(Critical << "Unable to open blast library " << blastdb);
        return 1;
    }
    return 0;	
}


// create the ladders from sequence

int CSearch::CreateLadders(const char *Sequence,
                           int iSearch,
                           int position,
                           int endposition,
                           int *Masses, 
                           int iMissed,
                           CAA& AA, 
                           int iMod,
                           CMod ModList[],
                           int NumMod,
                           int ForwardIon,
                           int BackwardIon)
{
    if(!BLadder[iMod]->CreateLadder(ForwardIon,
                                    1,
                                    Sequence,
                                    iSearch,
                                    position,
                                    endposition,
                                    Masses[iMissed], 
                                    MassArray,
                                    AA,
                                    SetMassAndMask(iMissed, iMod).Mask,
                                    ModList,
                                    NumMod,
                                    MyRequest->GetSettings()
                                    )) return 1;
    if(!YLadder[iMod]->CreateLadder(BackwardIon,
                                    1,
                                    Sequence,
                                    iSearch,
                                    position,
                                    endposition,
                                    Masses[iMissed], 
                                    MassArray,
                                    AA, 
                                    SetMassAndMask(iMissed, iMod).Mask,
                                    ModList, 
                                    NumMod,
                                    MyRequest->GetSettings()
                                    )) return 1;
    if(!B2Ladder[iMod]->CreateLadder(ForwardIon,
                                     2,
                                     Sequence,
                                     iSearch,
                                     position,
                                     endposition, 
                                     Masses[iMissed], 
                                     MassArray, 
                                     AA, 
                                     SetMassAndMask(iMissed, iMod).Mask,
                                     ModList, 
                                     NumMod,
                                     MyRequest->GetSettings()
                                     )) return 1;
    if(!Y2Ladder[iMod]->CreateLadder(BackwardIon,
                                     2,
                                     Sequence,
                                     iSearch,
                                     position,
                                     endposition,
                                     Masses[iMissed], 
                                     MassArray,
                                     AA,
                                     SetMassAndMask(iMissed, iMod).Mask,
                                     ModList,
                                     NumMod,
                                     MyRequest->GetSettings()
                                     )) return 1;
    
    return 0;
}


// compare ladders to experiment
int CSearch::CompareLadders(int iMod,
                            CMSPeak *Peaks,
                            bool OrLadders,
                            const TMassPeak *MassPeak)
{
    if(MassPeak && MassPeak->Charge >= Peaks->GetConsiderMult()) {
		Peaks->CompareSorted(*(BLadder[iMod]), MSCULLED2, 0); 
		Peaks->CompareSorted(*(YLadder[iMod]), MSCULLED2, 0); 
		Peaks->CompareSorted(*(B2Ladder[iMod]), MSCULLED2, 0); 
		Peaks->CompareSorted(*(Y2Ladder[iMod]), MSCULLED2, 0);
	if(OrLadders) {
	    BLadder[iMod]->Or(*(B2Ladder[iMod]));
	    YLadder[iMod]->Or(*(Y2Ladder[iMod]));	
	}
    }
    else {
		Peaks->CompareSorted(*(BLadder[iMod]), MSCULLED1, 0); 
		Peaks->CompareSorted(*(YLadder[iMod]), MSCULLED1, 0); 
    }
    return 0;
}


#ifdef MSSTATRUN
// compare ladders to experiment
double CSearch::CompareLaddersPearson(int iMod,
                            CMSPeak *Peaks,
                            const TMassPeak *MassPeak)
{
    double avgMZI(0.0), avgLadder(0.0);
    int n(0);
    double numerator(0.0), normLadder(0.0), normMZI(0.0);
    double retval(0.0);
    if(MassPeak && MassPeak->Charge >= Peaks->GetConsiderMult()) {
        Peaks->CompareSortedAvg(*(BLadder[iMod]), MSCULLED2, avgMZI, avgLadder, n, true);
        Peaks->CompareSortedAvg(*(YLadder[iMod]), MSCULLED2, avgMZI, avgLadder, n, false);
        Peaks->CompareSortedAvg(*(B2Ladder[iMod]), MSCULLED2, avgMZI, avgLadder, n, false);
        Peaks->CompareSortedAvg(*(Y2Ladder[iMod]), MSCULLED2, avgMZI, avgLadder, n, false);
        avgLadder /= n;
        avgMZI /= n;

        Peaks->CompareSortedPearsons(*(BLadder[iMod]), MSCULLED2, avgMZI, avgLadder, numerator, normLadder, normMZI, true);
        Peaks->CompareSortedPearsons(*(YLadder[iMod]), MSCULLED2, avgMZI, avgLadder, numerator, normLadder, normMZI, false);
        Peaks->CompareSortedPearsons(*(B2Ladder[iMod]), MSCULLED2, avgMZI, avgLadder, numerator, normLadder, normMZI, false);
        Peaks->CompareSortedPearsons(*(Y2Ladder[iMod]), MSCULLED2, avgMZI, avgLadder, numerator, normLadder, normMZI, false);
    }
    else {
        Peaks->CompareSortedAvg(*(BLadder[iMod]), MSCULLED1, avgMZI, avgLadder, n, true);
        Peaks->CompareSortedAvg(*(YLadder[iMod]), MSCULLED1, avgMZI, avgLadder, n, false);
        avgLadder /= n;
        avgMZI /= n;

        Peaks->CompareSortedPearsons(*(BLadder[iMod]), MSCULLED1, avgMZI, avgLadder, numerator, normLadder, normMZI, true);
        Peaks->CompareSortedPearsons(*(YLadder[iMod]), MSCULLED1, avgMZI, avgLadder, numerator, normLadder, normMZI, false);
    }
    retval = numerator / (sqrt(normLadder) * sqrt(normMZI));
    return retval;
}
#endif


void CSearch::CompareLaddersRank(int iMod,
                                   CMSPeak *Peaks,
                                   const TMassPeak *MassPeak,
                                   int& N,
                                   int& M,
                                   int& Sum)
{
    if(MassPeak && MassPeak->Charge >= Peaks->GetConsiderMult()) {
        Peaks->CompareSortedRank(*(BLadder[iMod]), MSCULLED2, 0, Sum, M);
        Peaks->CompareSortedRank(*(YLadder[iMod]), MSCULLED2, 0, Sum, M);
        Peaks->CompareSortedRank(*(B2Ladder[iMod]), MSCULLED2, 0, Sum, M);
        Peaks->CompareSortedRank(*(Y2Ladder[iMod]), MSCULLED2, 0, Sum, M);
        N = Peaks->GetNum(MSCULLED2);
    }
    else {
        Peaks->CompareSortedRank(*(BLadder[iMod]), MSCULLED1, 0, Sum, M);
        Peaks->CompareSortedRank(*(YLadder[iMod]), MSCULLED1, 0, Sum, M);
        N = Peaks->GetNum(MSCULLED1);
    }
}


// compare ladders to experiment
bool CSearch::CompareLaddersTop(int iMod,
                                CMSPeak *Peaks,
                                const TMassPeak *MassPeak)
{
    if(MassPeak && MassPeak->Charge >=  Peaks->GetConsiderMult() ) {
		if(Peaks->CompareTop(*(BLadder[iMod]))) return true; 
		if(Peaks->CompareTop(*(YLadder[iMod]))) return true; 
		if(Peaks->CompareTop(*(B2Ladder[iMod]))) return true; 
		if(Peaks->CompareTop(*(Y2Ladder[iMod]))) return true;
    }
    else {
		if(Peaks->CompareTop(*(BLadder[iMod]))) return true; 
		if(Peaks->CompareTop(*(YLadder[iMod]))) return true; 
    }
    return false;
}


// loads spectra into peaks
void CSearch::Spectrum2Peak(CMSPeakSet& PeakSet)
{
    CSpectrumSet::Tdata::const_iterator iSpectrum;
    CMSPeak* Peaks;
	
    iSpectrum = MyRequest->GetSpectra().Get().begin();
    for(; iSpectrum != MyRequest->GetSpectra().Get().end(); iSpectrum++) {
	CRef <CMSSpectrum> Spectrum =  *iSpectrum;
	if(!Spectrum) {
	    ERR_POST(Error << "omssa: unable to find spectrum");
	    return;
	}
		
	Peaks = new CMSPeak(MyRequest->GetSettings().GetHitlistlen());
	if(!Peaks) {
	    ERR_POST(Error << "omssa: unable to allocate CMSPeak");
	    return;
	}
	if(Peaks->Read(*Spectrum, MyRequest->GetSettings()) != 0) {
	    ERR_POST(Error << "omssa: unable to read spectrum into CMSPeak");
	    return;
	}
    Peaks->SetName().clear();
	if(Spectrum->CanGetIds()) Peaks->SetName() = Spectrum->GetIds();
	if(Spectrum->CanGetNumber())
	    Peaks->SetNumber() = Spectrum->GetNumber();
		
	Peaks->Sort();
	Peaks->SetComputedCharge(MyRequest->GetSettings().GetChargehandling());
	Peaks->InitHitList(MyRequest->GetSettings().GetMinhit());
	Peaks->CullAll(MyRequest->GetSettings());
		
	if(Peaks->GetNum(MSCULLED1) < MyRequest->GetSettings().GetMinspectra())
	    {
	    ERR_POST(Info << "omssa: not enough peaks in spectra");
	    Peaks->SetError(eMSHitError_notenuffpeaks);
	}
	PeakSet.AddPeak(Peaks);
		
    }
    MaxMZ = PeakSet.SortPeaks(MSSCALE2INT(MyRequest->GetSettings().GetPeptol()),
                      MyRequest->GetSettings().GetZdep());
	
}

// compares TMassMasks.  Lower m/z first in sort.
struct CMassMaskCompare {
    bool operator() (const TMassMask& x, const TMassMask& y)
    {
	if(x.Mass < y.Mass) return true;
	return false;
    }
};

/**
 *  delete variable mods that overlap with fixed mods
 * @param NumMod the number of modifications
 * @param ModList modification information
 */
void CSearch::DeleteVariableOverlap(int& NumMod,
                      CMod ModList[])
{
    int i, j;
    for (i = 0; i < NumMod; i++) {
        // if variable mod
        if(ModList[i].GetFixed() != 1) {
            // iterate thru all mods for comparison
            for(j = 0; j < NumMod; j++) {
                // if fixed and at same site
                if(ModList[j].GetFixed() == 1 && 
                   ModList[i].GetSite() == ModList[j].GetSite()) {
                    // mark mod for deletion
                    ModList[i].SetFixed() = -1;
                }
            } // j loop
        } // IsFixed
    } // i loop

    // now do the deletion
    for (i = 0; i < NumMod;) {
        if(ModList[i].GetFixed() == -1) {
            NumMod--;
            // if last mod, then just return
            if(i == NumMod) return;
            // otherwise, delete the modification
            ModList[i] = ModList[i+1];
        }
        else i++;
    }
    return;
}

// update sites and masses for new peptide
void CSearch::UpdateWithNewPep(int Missed,
			       const char *PepStart[],
			       const char *PepEnd[], 
			       int NumMod[], 
                               CMod ModList[][MAXMOD],
			       int Masses[],
			       int EndMasses[],
                               int NumModSites[],
                               CRef <CMSModSpecSet> Modset)
{
    // iterate over missed cleavages
    int iMissed;
    // maximum mods allowed
    //int ModMax; 
    // iterate over mods
    int iMod;
    

    // update the longer peptides to add the new peptide (Missed-1) on the end
    for(iMissed = 0; iMissed < Missed - 1; iMissed++) {
	// skip start
	if(PepStart[iMissed] == (const char *)-1) continue;
	// reset the end sequences
	PepEnd[iMissed] = PepEnd[Missed - 1];
				
	// update new mod masses to add in any new mods from new peptide

	// first determine the maximum value for updated mod list
	//if(NumMod[iMissed] + NumMod[Missed-1] >= MAXMOD)
	//    ModMax = MAXMOD - NumMod[iMissed];
	//else ModMax = NumMod[Missed-1];

	// now interate thru the new entries
    const char *OldSite(0);
    int NumModSitesCount(0), NumModCount(0);
	for(iMod = 0; iMod < NumMod[Missed-1]; iMod++) {

        // don't do more than the maximum number of modifications
        if(NumModCount + NumMod[iMissed] >= MAXMOD) break;

        // if n-term protein mod and not at the start of the peptide, don't copy
        if ((Modset->GetModType(ModList[Missed-1][iMod].GetEnum()) == eMSModType_modn || 
            Modset->GetModType(ModList[Missed-1][iMod].GetEnum()) == eMSModType_modnaa) &&
            PepStart[iMissed] != ModList[Missed-1][iMod].GetSite()) {
            continue;
        }

        // copy the mod to the old peptide
        ModList[iMissed][NumModCount + NumMod[iMissed]] = 
            ModList[Missed-1][iMod];

        // increment site count if not fixed mod and not the same site
        if(OldSite != ModList[iMissed][NumModCount + NumMod[iMissed]].GetSite() &&
            ModList[iMissed][NumModCount + NumMod[iMissed]].GetFixed() != 1) {
            NumModSitesCount++;
            OldSite = ModList[iMissed][NumModCount + NumMod[iMissed]].GetSite();
        }

        // increment number of mods
        NumModCount++;


	}
				
	// update old masses
	Masses[iMissed] += Masses[Missed - 1];
				
	// update end masses
	EndMasses[iMissed] = EndMasses[Missed - 1];
			
	// update number of Mods
	NumMod[iMissed] += NumModCount;

    // update number of Modification Sites
    NumModSites[iMissed] += NumModSitesCount;
    }	    
}


/**
 *  count the number of unique sites modified
 * 
 * @param NumModSites the number of unique mod sites
 * @param NumMod the number of mods
 * @param ModList modification information
 */
void CSearch::CountModSites(int &NumModSites,
              int NumMod,
              CMod ModList[])
{
    NumModSites = 0;
    int i;
    const char *OldSite(0);

    for(i = 0; i < NumMod; i++) {
        // skip repeated sites and fixed mods
        if(ModList[i].GetSite() != OldSite && ModList[i].GetFixed() != 1 ) {
            NumModSites++;
            OldSite = ModList[i].GetSite();
        }
    }
}


// create the various combinations of mods
void CSearch::CreateModCombinations(int Missed,
                				    const char *PepStart[],
                				    int Masses[],
                				    int EndMasses[],
                				    int NumMod[],
                				    unsigned NumMassAndMask[],
                                    int NumModSites[],
                                    CMod ModList[][MAXMOD]
                                    )
{
    // need to iterate thru combinations that have iMod.
    // i.e. iMod = 3 and NumMod=5
    // 00111, 01011, 10011, 10101, 11001, 11010, 11100, 01101,
    // 01110
    // i[0] = 0 --> 5-3, i[1] = i[0]+1 -> 5-2, i[3] = i[1]+1 -> 5-1
    // then construct bool mask
	    
    // holders for calculated modification mask and modified peptide masses
    unsigned Mask, MassOfMask;
    // iterate thru active mods
    int iiMod;
    // keep track of the number of unique masks created.  each corresponds to a ladder
    int iModCount;
    // missed cleavage
    int iMissed;
    // number of mods to consider
    int iMod;
    // positions of mods
    int ModIndex[MAXMOD];

    // go thru missed cleaves
    for(iMissed = 0; iMissed < Missed; iMissed++) {
	// skip start
	if(PepStart[iMissed] == (const char *)-1) continue;
	iModCount = 0;

	// set up non-modified mass
	SetMassAndMask(iMissed, iModCount).Mass = 
	    Masses[iMissed] + EndMasses[iMissed];
	SetMassAndMask(iMissed, iModCount).Mask = 0;

    int NumVariable(NumMod[iMissed]);  // number of variable mods
    int NumFixed;
    // add in fixed mods
    for(iMod = 0; iMod < NumMod[iMissed]; iMod++) {
        if(ModList[iMissed][iMod].GetFixed()) {
            SetMassAndMask(iMissed, iModCount).Mass += ModList[iMissed][iMod].GetPrecursorDelta();
            SetMassAndMask(iMissed, iModCount).Mask |= 1 << iMod;
            NumVariable--;
        }
    }
	iModCount++;
    NumFixed = NumMod[iMissed] - NumVariable;
 
	// go thru number of mods allowed
//	for(iMod = 0; iMod < NumVariable && iModCount < MaxModPerPep; iMod++) {
        for(iMod = 0; iMod < NumModSites[iMissed] && iModCount < MaxModPerPep; iMod++) {
		    
	    // initialize ModIndex that points to mod sites

        // todo: ModIndex must always include fixed mods

	    InitModIndex(ModIndex, iMod, NumMod[iMissed],
                     NumModSites[iMissed], ModList[iMissed]);
	    do {
    			
    		// calculate mass
    		MassOfMask = SetMassAndMask(iMissed, 0).Mass;
    		for(iiMod = 0; iiMod <= iMod; iiMod++ ) 
    		    MassOfMask += ModList[iMissed][ModIndex[iiMod + NumFixed]].GetPrecursorDelta();
    		// make bool mask
    		Mask = MakeBoolMask(ModIndex, iMod + NumFixed);
    		// put mass and mask into storage
    		SetMassAndMask(iMissed, iModCount).Mass = MassOfMask;
    		SetMassAndMask(iMissed, iModCount).Mask = Mask;
#if 0
printf("NumMod = %d iMod = %d, Mask = \n", NumMod[iMissed], iMod);
int iii;
for (iii=NumMod[iMissed]-1; iii >= 0; iii--) {
if(Mask & 1 << iii) printf("1");
else printf("0");
}
printf("\n");
#endif
    		// keep track of the  number of ladders
    		iModCount++;

	    } while(iModCount < MaxModPerPep &&
		    CalcModIndex(ModIndex, iMod, NumMod[iMissed], NumFixed,
                         NumModSites[iMissed], ModList[iMissed]));
	} // iMod

    // if exact mass, add neutrons as appropriate
    if(MyRequest->GetSettings().GetPrecursorsearchtype() == eMSSearchType_exact) {
        int ii;
        for(ii = 0; ii < iModCount; ++ii) {
            SetMassAndMask(iMissed, ii).Mass += 
                SetMassAndMask(iMissed, ii).Mass /
                MSSCALE2INT(MyRequest->GetSettings().GetExactmass()) * 
                MSSCALE2INT(kNeutron);
        }
    }


	// sort mask and mass by mass
	sort(MassAndMask.get() + iMissed*MaxModPerPep, MassAndMask.get() + iMissed*MaxModPerPep + iModCount,
	     CMassMaskCompare());
	// keep track of number of MassAndMask
	NumMassAndMask[iMissed] = iModCount;

    } // iMissed
}


// set up the ions to search
void CSearch::SetIons(int& ForwardIon, int& BackwardIon)
{
    if(MyRequest->GetSettings().GetIonstosearch().size() != 2) {
        ERR_POST(Fatal << "omssa: two search ions need to be specified");
    }
    CMSSearchSettings::TIonstosearch::const_iterator i;
    i = MyRequest->GetSettings().GetIonstosearch().begin();
    ForwardIon = *i;
    i++;
    BackwardIon = *i;
}


/**
 * set up modifications from both user input and mod file data
 * 
 * @param MyRequest the user search params and spectra
 * @param Modset list of modifications
 */

void CSearch::SetupMods(CRef <CMSModSpecSet> Modset)
{
    //Modset->Append(MyRequest);
    Modset->CreateArrays();
}


/**
 * initialize mass ladders
 * 
 * @param MaxLadderSize the number of ions per ladder
 * @param NumLadders the number of ladders per series
 */
void CSearch::InitLadders(void)
{
    BLadder.clear();
    YLadder.clear();
    B2Ladder.clear();
    Y2Ladder.clear();

    int MaxLadderSize = MyRequest->GetSettings().GetMaxproductions();
    int i;
    if (MaxLadderSize == 0) MaxLadderSize = kMSLadderMax;
    CRef <CLadder> newLadder;
    for (i = 0; i < MaxModPerPep; i++) {
        newLadder.Reset(new CLadder(MaxLadderSize));
        BLadder.push_back(newLadder);
        newLadder.Reset(new CLadder(MaxLadderSize));
        YLadder.push_back(newLadder);
        newLadder.Reset(new CLadder(MaxLadderSize));
        B2Ladder.push_back(newLadder);
        newLadder.Reset(new CLadder(MaxLadderSize));
        Y2Ladder.push_back(newLadder);
    }
}

//! Performs the ms/ms search
/*!
\param MyRequest the user search params and spectra
\param MyResponse the results of the search
\param Modset list of modifications
*/
int CSearch::Search(CRef <CMSRequest> MyRequestIn,
                    CRef <CMSResponse> MyResponseIn,
                    CRef <CMSModSpecSet> Modset)
{

    try {
        MyRequest = MyRequestIn;
        MyResponse = MyResponseIn;

        SetupMods(Modset);

        CCleave* enzyme =
		  CCleaveFactory::CleaveFactory(static_cast <EMSEnzymes> 
				 (MyRequest->GetSettings().GetEnzyme()));


	// set maximum number of ladders to calculate per peptide
	MaxModPerPep = MyRequest->GetSettings().GetMaxmods();
	if(MaxModPerPep > MAXMOD2) MaxModPerPep = MAXMOD2;

    int ForwardIon(1), BackwardIon(4);
    SetIons(ForwardIon, BackwardIon);

    InitLadders();

    LadderCalc.reset(new Int1[MaxModPerPep]);
	CAA AA;
		
	int Missed;  // number of missed cleaves allowed + 1
    if(enzyme->GetNonSpecific()) Missed = 1;
    else Missed = MyRequest->GetSettings().GetMissedcleave()+1;

	int iMissed; // iterate thru missed cleavages
    
	int iSearch, hits;
	int endposition, position;

    // initialize fixed mods
	FixedMods.Init(MyRequest->GetSettings().GetFixed(), Modset);
	MassArray.Init(FixedMods, MyRequest->GetSettings().GetProductsearchtype(), Modset);
	PrecursorMassArray.Init(FixedMods, 
                            MyRequest->GetSettings().GetPrecursorsearchtype(), Modset);
    // initialize variable mods and set enzyme to use n-term methionine cleavage
	enzyme->SetNMethionine() = 
        VariableMods.Init(MyRequest->GetSettings().GetVariable(), Modset);

	const int *IntMassArray = MassArray.GetIntMass();
	const int *PrecursorIntMassArray = PrecursorMassArray.GetIntMass();
	const char *PepStart[MAXMISSEDCLEAVE];
	const char *PepEnd[MAXMISSEDCLEAVE];

    // contains informations on individual mod sites
    CMod ModList[MAXMISSEDCLEAVE][MAXMOD];

	int NumMod[MAXMISSEDCLEAVE];
    // the number of modification sites.  always less than NumMod.
	int NumModSites[MAXMISSEDCLEAVE];


	// calculated masses and masks
    MassAndMask.reset(new TMassMask[MAXMISSEDCLEAVE*MaxModPerPep]);

	// the number of masses and masks for each peptide
	unsigned NumMassAndMask[MAXMISSEDCLEAVE];
	
	// set up mass array, indexed by missed cleavage
	// note that EndMasses is the end mass of peptide, kept separate to allow
	// reuse of Masses array in missed cleavage calc
	int Masses[MAXMISSEDCLEAVE];
	int EndMasses[MAXMISSEDCLEAVE];


	int iMod;   // used to iterate thru modifications
	
	bool SequenceDone;  // are we done iterating through the sequences?

    const CMSSearchSettings::TTaxids& Tax = MyRequest->GetSettings().GetTaxids();
	CMSSearchSettings::TTaxids::const_iterator iTax;

	CMSHit NewHit;  // a new hit of a ladder to an m/z value
	CMSHit *NewHitOut;  // copy of new hit
	
	
	CMSPeakSet PeakSet;
	const TMassPeak *MassPeak; // peak currently in consideration
	CMSPeak* Peaks;
    CIntervalTree::const_iterator im; // iterates over interval tree
	
	Spectrum2Peak(PeakSet);

#ifdef MSSTATRUN
	ofstream histos("histo.txt");
#endif

    vector <int> taxids;
    vector <int>::iterator itaxids;
    bool TaxInfo(false);  // check to see if any tax information in blast library

	// iterate through sequences
	for(iSearch = 0; iSearch < numseq; iSearch++) {
	    if(iSearch/10000*10000 == iSearch) ERR_POST(Info << "sequence " << 
							iSearch);

	    if(MyRequest->GetSettings().IsSetTaxids()) {
            rdfp->GetTaxIDs(iSearch, taxids, false);
            for(itaxids = taxids.begin(); itaxids != taxids.end(); ++itaxids) {    
                if(*itaxids == 0) continue;
                TaxInfo = true;
                for(iTax = Tax.begin(); iTax != Tax.end(); ++iTax) {
                    if(*itaxids == *iTax) goto TaxContinue;
                } 
            }
            continue;
        }
        TaxContinue:
        CSeqDBSequence Sequence(rdfp.GetPointer(), iSearch);
        SequenceDone = false;
        
        // initialize missed cleavage matrix
        for(iMissed = 0; iMissed < Missed; iMissed++) {
		PepStart[iMissed] = (const char *)-1; // mark start
		PepEnd[iMissed] = Sequence.GetData();
		Masses[iMissed] = 0;
		EndMasses[iMissed] = 0;
		NumMod[iMissed] = 0;
		NumModSites[iMissed] = 0;

        ModList[iMissed][0].Reset();
	    }
	    PepStart[Missed - 1] = Sequence.GetData();

        // if non-specific enzyme, set stop point
        if(enzyme->GetNonSpecific()) {
            enzyme->SetStop() = Sequence.GetData() + MyRequest->GetSettings().GetMinnoenzyme() - 1;
        }

	    // iterate thru the sequence by digesting it
	    while(!SequenceDone) {

			
		// zero out no missed cleavage peptide mass and mods
		// note that Masses and EndMass are separate to reuse
		// masses during the missed cleavage calculation
		Masses[Missed - 1] = 0;
		EndMasses[Missed - 1] = 0;
		NumMod[Missed - 1] = 0;
		NumModSites[Missed - 1] = 0;
		// init no modification elements
        ModList[Missed - 1][0].Reset();

		// calculate new stop and mass
		SequenceDone = 
		    enzyme->CalcAndCut(Sequence.GetData(),
                               Sequence.GetData() + Sequence.GetLength() - 1, 
                               &(PepEnd[Missed - 1]),
                               &(Masses[Missed - 1]),
                               NumMod[Missed - 1],
                               MAXMOD,
                               &(EndMasses[Missed - 1]),
                               VariableMods, FixedMods,
                               ModList[Missed - 1],
                               IntMassArray,
                               PrecursorIntMassArray,
                               Modset,
                               MyRequest->GetSettings().GetMaxproductions()
                               );

        // delete variable mods that overlap with fixed mods
        DeleteVariableOverlap(NumMod[Missed - 1],
                              ModList[Missed - 1]);

        // count the number of unique sites modified
        CountModSites(NumModSites[Missed - 1],
                      NumMod[Missed - 1],
                      ModList[Missed - 1]);

        UpdateWithNewPep(Missed, PepStart, PepEnd, NumMod, ModList,
                         Masses, EndMasses, NumModSites, Modset);
	
        CreateModCombinations(Missed, PepStart, Masses,
				      EndMasses, NumMod, NumMassAndMask,
				      NumModSites, ModList);


		int OldMass;  // keeps the old peptide mass for comparison
		bool NoMassMatch;  // was there a match to the old mass?

		for(iMissed = 0; iMissed < Missed; iMissed++) {	    
		    if(PepStart[iMissed] == (const char *)-1) continue;  // skip start
				
		    // get the start and stop position, inclusive, of the peptide
		    position =  PepStart[iMissed] - Sequence.GetData();
		    endposition = PepEnd[iMissed] - Sequence.GetData();
		
		    // init bool for "Has ladder been calculated?"
			ClearLadderCalc(NumMassAndMask[iMissed]);
		
		    OldMass = 0;
		    NoMassMatch = true;
				
		    // go thru total number of mods
		    for(iMod = 0; iMod < NumMassAndMask[iMissed]; iMod++) {
		    
			// have we seen this mass before?
			if(SetMassAndMask(iMissed, iMod).Mass == OldMass &&
			   NoMassMatch) continue;
			NoMassMatch = true;
			OldMass = SetMassAndMask(iMissed, iMod).Mass;

			// return peaks where theoretical mass is <= precursor mass + tol
			// and >= precursor mass - tol
            if(!enzyme->GetTopDown())
                im = PeakSet.SetIntervalTree().IntervalsContaining(OldMass);
            // if top-down enzyme, skip the interval tree match
            else
                im = PeakSet.SetIntervalTree().AllIntervals();

            for(; im; ++im ) {
                MassPeak = static_cast <const TMassPeak *> (im.GetValue().GetPointerOrNull());

			    Peaks = MassPeak->Peak;
			    // make sure we look thru other mod masks with the same mass
			    NoMassMatch = false;
						
						
						
			    if(!GetLadderCalc(iMod)) {
				if(CreateLadders(Sequence.GetData(), 
                                 iSearch,
                                 position,
                                 endposition,
                                 Masses,
                                 iMissed, 
                                 AA,
                                 iMod,
                                 ModList[iMissed],
                                 NumMod[iMissed],
                                 ForwardIon,
                                 BackwardIon
                                 ) != 0) continue;
                SetLadderCalc(iMod) = true;	
				// continue to next sequence if ladders not successfully made
			    }
			    else {
    				BLadder[iMod]->ClearHits();
    				YLadder[iMod]->ClearHits();
    				B2Ladder[iMod]->ClearHits();
    				Y2Ladder[iMod]->ClearHits();
			    }

                            if(UseRankScore) Peaks->SetPeptidesExamined(MassPeak->Charge)++;
				
			    if(/*UseRankScore || */
                   CompareLaddersTop(iMod, 
                                     Peaks,
                                     MassPeak)
                   ) {
				// end of new addition

                                if(!UseRankScore) Peaks->SetPeptidesExamined(MassPeak->Charge)++;
				Peaks->ClearUsedAll();
				CompareLadders(iMod, 
                               Peaks,
                               false,
                               MassPeak);
				hits = BLadder[iMod]->HitCount() + 
				    YLadder[iMod]->HitCount() +
				    B2Ladder[iMod]->HitCount() +
				    Y2Ladder[iMod]->HitCount();
#ifdef MSSTATRUN
//                histos << CompareLaddersPearson(iMod, Peaks, MassPeak) << endl;
                {
                int N(0), M(0), Sum(0);
                double Average, StdDev;
				Peaks->ClearUsedAll();

                CompareLaddersRank(iMod, Peaks, MassPeak, N,
                                   M,
                                   Sum);
                Average = ( M *(N+1))/2.0;
                StdDev = sqrt(Average * (N - M) / 6.0);
                histos << "N=" << N << " M=" << M << " Sum=" << Sum << " Ave=" << Average << " SD=" << StdDev << " erf=" << 0.5*(1.0 + erf((Sum - Average)/(StdDev*sqrt(2.0)))) << endl;
                }

#endif
				if(hits >= MyRequest->GetSettings().GetMinhit()) {
				    // need to save mods.  bool map?
				    NewHit.SetHits(hits);	
				    NewHit.SetCharge(MassPeak->Charge);
				    // only record if hit kept
				    if(Peaks->AddHit(NewHit, NewHitOut)) {
					NewHitOut->SetStart(position);
					NewHitOut->SetStop(endposition);
					NewHitOut->SetSeqIndex(iSearch);
					NewHitOut->SetMass(MassPeak->Mass);
					// record the hits
					Peaks->ClearUsedAll();
					NewHitOut->
					    RecordMatches(*(BLadder[iMod]),
									  *(YLadder[iMod]),
									  *(B2Ladder[iMod]), 
									  *(Y2Ladder[iMod]),
									  Peaks,
									  SetMassAndMask(iMissed, iMod).Mask,
									  ModList[iMissed],
									  NumMod[iMissed],
									  PepStart[iMissed],
                                      MyRequest->GetSettings().GetSearchctermproduct(),
                                      MyRequest->GetSettings().GetSearchb1(),
                                      SetMassAndMask(iMissed, iMod).Mass
									  );
				    }
				}
			    } // new addition
			} // MassPeak
		    } //iMod
		} // iMissed
        if (enzyme->GetNonSpecific()) {
            int NonSpecificMass(Masses[0] + EndMasses[0]);
            PartialLoop:

                // check that stop is within bounds
                //// upper bound is max precursor mass divided by lightest AA
                ////      if(enzyme->GetStop() - PepStart[0] < MaxMZ/MonoMass[7]/MSSCALE &&
                // upper bound redefined so that minimum mass of existing peptide
                // is less than the max precursor mass minus the mass of glycine
                // assumes that any mods have positive mass

                // argghh, doesn't work for semi-tryptic, which resets the mass
                // need to use different criterion if semi-tryptic and  start position was
                // moved.  otherwise this criterion is OK
                if(NonSpecificMass < MaxMZ /*- MSSCALE2INT(MonoMass[7]) */&&
                   enzyme->GetStop() < Sequence.GetData() + Sequence.GetLength() &&
                   (MyRequest->GetSettings().GetMaxnoenzyme() == 0 ||
                    enzyme->GetStop() - PepStart[0] + 1 < MyRequest->GetSettings().GetMaxnoenzyme())
                   ) {
                    enzyme->SetStop()++;
                    NonSpecificMass += PrecursorIntMassArray[AA.GetMap()[*(enzyme->GetStop())]];
                }
                // reset to new start with minimum size
                else if ( PepStart[0] < Sequence.GetData() + Sequence.GetLength() - 
                          MyRequest->GetSettings().GetMinnoenzyme()) {
                    PepStart[0]++;
                    enzyme->SetStop() = PepStart[0] + MyRequest->GetSettings().GetMinnoenzyme() - 1;

                    // reset mass
                    NonSpecificMass = 0;
                    const char *iSeqChar;
                    for(iSeqChar = PepStart[0]; iSeqChar <= enzyme->GetStop(); iSeqChar++)
                        PrecursorIntMassArray[AA.GetMap()[*iSeqChar]];
                    // reset sequence done flag if at end of sequence
                    SequenceDone = false;
                }
                else SequenceDone = true;
                
                // if this is partial tryptic, loop back if one end or the other is not tryptic
                // for start, need to check sequence before (check for start of seq)
                // for end, need to deal with end of protein case
                if(!SequenceDone && enzyme->GetCleaveNum() > 0 &&
                   PepStart[0] != Sequence.GetData() &&
                   enzyme->GetStop() != Sequence.GetData() + Sequence.GetLength() ) {
                    if (!enzyme->CheckCleaveChar(PepStart[0]-1) &&
                        !enzyme->CheckCleaveChar(enzyme->GetStop())) 
                        goto PartialLoop;
                }
                
                PepEnd[0] = PepStart[0];
        }
        else {
            if (!SequenceDone) {                
                int NumModCount;
                const char *OldSite;
                int NumModSitesCount;
                // get rid of longest peptide and move the other peptides down the line
                for (iMissed = 0; iMissed < Missed - 1; iMissed++) {
                    // move masses to next missed cleavage
                    Masses[iMissed] = Masses[iMissed + 1];
                    // don't move EndMasses as they are recalculated
                    
                    // move the modification data
                    NumModCount = 0;
                    OldSite = 0;
                    NumModSitesCount = 0;
                    for (iMod = 0; iMod < NumMod[iMissed + 1]; iMod++) {
                        // throw away the c term peptide mods as we have a new c terminus
                        if (Modset->GetModType(ModList[iMissed + 1][iMod].GetEnum()) != eMSModType_modcp  && 
                            Modset->GetModType(ModList[iMissed + 1][iMod].GetEnum()) != eMSModType_modcpaa) {
                            ModList[iMissed][NumModCount] = ModList[iMissed + 1][iMod];
                            NumModCount++;
                            // increment mod site count if new site and not fixed mod
                            if (OldSite != ModList[iMissed + 1][iMod].GetSite() &&
                                ModList[iMissed + 1][iMod].GetFixed() != 1) {
                                NumModSitesCount++;
                                OldSite = ModList[iMissed + 1][iMod].GetSite();
                            }
                        }
                    }
                    NumMod[iMissed] = NumModCount;
                    NumModSites[iMissed] = NumModSitesCount;
                    
                    // copy starts to next missed cleavage
                    PepStart[iMissed] = PepStart[iMissed + 1];
                }
                
                // init new start from old stop
                PepEnd[Missed-1] += 1;
                PepStart[Missed-1] = PepEnd[Missed-1];
            }
        }

    }


	}

   
	if(MyRequest->GetSettings().IsSetTaxids() && !TaxInfo)
        ERR_POST(Error << 
                 "Taxonomically restricted search specified and no taxonomy information found in sequence library");
	
	// read out hits
	SetResult(PeakSet);
	
    } catch (NCBI_NS_STD::exception& e) {
	ERR_POST(Info << "Exception caught in CSearch::Search: " << e.what());
	throw;
    }


    return 0;
}

///
///  Adds modification information to hitset
///

void CSearch::AddModsToHit(CMSHits *Hit, CMSHit *MSHit)
{
	int i;
	for (i = 0; i < MSHit->GetNumModInfo(); i++) {
        // screen out fixed mods
        if(MSHit->GetModInfo(i).GetIsFixed() == 1) continue;
		CRef< CMSModHit > ModHit(new CMSModHit);
		ModHit->SetSite() = MSHit->GetModInfo(i).GetSite();
		ModHit->SetModtype() = MSHit->GetModInfo(i).GetModEnum() ;
		Hit->SetMods().push_back(ModHit);
	}
}


///
///  Adds ion information to hitset
///

void CSearch::AddIonsToHit(CMSHits *Hit, CMSHit *MSHit)
{
	int i;
	for (i = 0; i < MSHit->GetHits(); i++) {
		CRef<CMSMZHit> IonHit(new CMSMZHit);
		IonHit->SetIon() = MSHit->GetHitInfo(i).GetIon();
        IonHit->SetCharge() = MSHit->GetHitInfo(i).GetCharge();
        IonHit->SetNumber() = MSHit->GetHitInfo(i).GetNumber();
        IonHit->SetMz() = MSHit->GetHitInfo(i).GetMz();
		Hit->SetMzhits().push_back(IonHit);
	}
}


///
///  Makes a string hashed out of the sequence plus mods
///

void CSearch::MakeModString(string& seqstring, string& modseqstring, CMSHit *MSHit)
{
	int i;
	modseqstring = seqstring;
	for (i = 0; i < MSHit->GetNumModInfo(); i++) {
		modseqstring += NStr::IntToString(MSHit->GetModInfo(i).GetSite()) + ":" +
			NStr::IntToString(MSHit->GetModInfo(i).GetModEnum()) + ",";
	}
}


void CSearch::SetResult(CMSPeakSet& PeakSet)
{

    double ThreshStart = MyRequest->GetSettings().GetCutlo(); 
    double ThreshEnd = MyRequest->GetSettings().GetCuthi();
    double ThreshInc = MyRequest->GetSettings().GetCutinc();
    double Evalcutoff = MyRequest->GetSettings().GetCutoff();

    CMSPeak* Peaks;
    TPeakSet::iterator iPeakSet;
	
    TScoreList ScoreList;
    TScoreList::iterator iScoreList;
    CMSHit * MSHit;

    // set the search library version
    MyResponse->SetDbversion(numseq);
	
    for(iPeakSet = PeakSet.GetPeaks().begin();
	iPeakSet != PeakSet.GetPeaks().end();
	iPeakSet++ ) {
	Peaks = *iPeakSet;
		
	// add to hitset
	CRef< CMSHitSet > HitSet (new CMSHitSet);
	if(!HitSet) {
	    ERR_POST(Error << "omssa: unable to allocate hitset");
	    return;
	}
	HitSet->SetNumber(Peaks->GetNumber());
	HitSet->SetIds() = Peaks->GetName();
	MyResponse->SetHitsets().push_back(HitSet);
		
	if(Peaks->GetError() == eMSHitError_notenuffpeaks) {
	    _TRACE("empty set");
	    HitSet->SetError(eMSHitError_notenuffpeaks);
	    continue;
	}
		
	double Threshold, MinThreshold(ThreshStart), MinEval(1000000.0L);
    if(!UseRankScore)
        {
            	// now calculate scores and sort
    	for(Threshold = ThreshStart; Threshold <= ThreshEnd; 
    	    Threshold += ThreshInc) {
    	    CalcNSort(ScoreList, Threshold, Peaks, false);
    	    if(!ScoreList.empty()) {
    		_TRACE("Threshold = " << Threshold <<
    		       "EVal = " << ScoreList.begin()->first);
    	    }
    	    if(!ScoreList.empty() && ScoreList.begin()->first < MinEval) {
                MinEval = ScoreList.begin()->first;
                MinThreshold = Threshold;
    	    }
    	    ScoreList.clear();
    	}
    }
    _TRACE("Min Threshold = " << MinThreshold);
    CalcNSort(ScoreList,
              MinThreshold,
              Peaks,
              !UseRankScore );
		
	const CMSSearchSettings::TTaxids& Tax = MyRequest->GetSettings().GetTaxids();
	CMSSearchSettings::TTaxids::const_iterator iTax;
		
	// keep a list of redundant peptides
	map <string, CMSHits * > PepDone;
	// add to hitset by score
	for(iScoreList = ScoreList.begin();
	    iScoreList != ScoreList.end();
	    iScoreList++) {

        double Score = iScoreList->first;
        if(Score > Evalcutoff) continue;
	    CMSHits * Hit;
	    CMSPepHit * Pephit;
			
	    MSHit = iScoreList->second;

        CBlast_def_line_set::Tdata::const_iterator iDefLine;
        CRef<CBlast_def_line_set> Hdr = rdfp->GetHdr(MSHit->GetSeqIndex());
        // scan taxids
        for(iDefLine = Hdr->Get().begin();
             iDefLine != Hdr->Get().end();
             ++iDefLine) {
        if(MyRequest->GetSettings().IsSetTaxids()) {
            for(iTax = Tax.begin(); iTax != Tax.end(); iTax++) {
                if((*iDefLine)->GetTaxid() == *iTax) goto TaxContinue2;
            } 
            continue;
        }
TaxContinue2:
		string seqstring, modseqstring;

        CSeqDBSequence Sequence(rdfp.GetPointer(), MSHit->GetSeqIndex());

        string tempstartstop;
		int iseq;
				
		for (iseq = MSHit->GetStart(); iseq <= MSHit->GetStop();
		     iseq++) {
		    seqstring += UniqueAA[Sequence.GetData()[iseq]];
		}
		MakeModString(seqstring, modseqstring, MSHit);

		if(PepDone.find(modseqstring) != PepDone.end()) {
		    Hit = PepDone[modseqstring];
		}
		else {
		    Hit = new CMSHits;
            Hit->SetTheomass(MSHit->GetTheoreticalMass());
            Hit->SetOid(MSHit->GetSeqIndex());
		    Hit->SetPepstring(seqstring);
            // set the start AA, if there is one
            if(MSHit->GetStart() > 0) {
                tempstartstop = UniqueAA[Sequence.GetData()[MSHit->GetStart()-1]];
                Hit->SetPepstart(tempstartstop);
            }
            else Hit->SetPepstart("");

            // set the end AA, if there is one
            if(MSHit->GetStop() < Sequence.GetLength() - 1) {
                tempstartstop = UniqueAA[Sequence.GetData()[MSHit->GetStop()+1]];
                Hit->SetPepstop(tempstartstop);
            }
            else Hit->SetPepstop("");

            if(isnan(Score)) {
                ERR_POST(Info << "Not a number in hitset " << 
                        HitSet->GetNumber() <<
                        " peptide " << modseqstring);
                Score = kHighEval;
            }
            else if(!finite(Score)) {
                ERR_POST(Info << "Infinite number in hitset " << 
                         HitSet->GetNumber() <<
                         " peptide " << modseqstring);
                Score = kHighEval;
            }
		    Hit->SetEvalue(Score);
		    Hit->SetPvalue(Score/Peaks->
				   GetPeptidesExamined(MSHit->
						       GetCharge()));	   
		    Hit->SetCharge(MSHit->GetCharge());
		    Hit->SetMass(MSHit->GetMass());
			// insert mods here
			AddModsToHit(Hit, MSHit);
            // insert ions here
            AddIonsToHit(Hit, MSHit);
		    CRef<CMSHits> hitref(Hit);
		    HitSet->SetHits().push_back(hitref);  
		    PepDone[modseqstring] = Hit;

		}
		
		Pephit = new CMSPepHit;

        if ((*iDefLine)->CanGetSeqid()) {
            // find a gi
            ITERATE(list< CRef<CSeq_id> >, seqid, (*iDefLine)->GetSeqid()) {
                if ((**seqid).IsGi()) {
                    Pephit->SetGi((**seqid).GetGi());
                    break;
                }
            }

            Pephit->SetAccession(
                FindBestChoice((*iDefLine)->GetSeqid(), CSeq_id::Score)->
                GetSeqIdString(false));
        }
            

		Pephit->SetStart(MSHit->GetStart());
		Pephit->SetStop(MSHit->GetStop());;
		Pephit->SetDefline((*iDefLine)->GetTitle());
        Pephit->SetProtlength(Sequence.GetLength());
		CRef<CMSPepHit> pepref(Pephit);
		Hit->SetPephits().push_back(pepref);

	    }
	}
	ScoreList.clear();
    }
}


//! calculate the evalues of the top hits and sort
void CSearch::CalcNSort(TScoreList& ScoreList,
						 double Threshold,
						 CMSPeak* Peaks,
						 bool NewScore
                        )
{
    int iCharges;
    int iHitList;
    int Tophitnum = MyRequest->GetSettings().GetTophitnum();
	
    for(iCharges = 0; iCharges < Peaks->GetNumCharges(); iCharges++) {
		
	TMSHitList& HitList = Peaks->GetHitList(iCharges);   
	for(iHitList = 0; iHitList != Peaks->GetHitListIndex(iCharges);
	    iHitList++) {
			
	    int tempMass = HitList[iHitList].GetMass();
	    int tempStart = HitList[iHitList].GetStart();
	    int tempStop = HitList[iHitList].GetStop();
	    int Charge = HitList[iHitList].GetCharge();
	    int Which = Peaks->GetWhich(Charge);
			
	    double a = CalcPoissonMean(tempStart, tempStop, tempMass, Peaks,
				       Charge, 
				       Threshold);        
	    if (a == 0) {
            // threshold probably too high
            continue;
        }
        if(a < 0 ) {
            _TRACE("poisson mean is < 0");
            continue;
        }
        else if(isnan(a) || !finite(a)) {
             ERR_POST(Info << "poisson mean is NaN or is infinite");
             continue;
        }
	    if(HitList[iHitList].GetHits() < a) continue;

	    double pval; // statistical p-value
        int N; // number of peptides
        N = Peaks->GetPeptidesExamined(Charge) + 
            (MyRequest->GetSettings().GetZdep() * (Charge - 1) + 1) *
             MyRequest->GetSettings().GetPseudocount();

	    if(NewScore) {
		int High, Low, NumPeaks, NumLo, NumHi;
		Peaks->HighLow(High, Low, NumPeaks, tempMass, Charge, Threshold, NumLo, NumHi);
 
		double TopHitProb = ((double)Tophitnum)/NumPeaks;
        // correct for situation where more tophits than experimental peaks
        if(TopHitProb > 1.0) TopHitProb = 1.0;
		double Normal = CalcNormalTopHit(a, TopHitProb);
		int numhits = HitList[iHitList].GetHits(Threshold, Peaks->GetMaxI(Which));
  // for poisson test
		//		int numhits = HitList[iHitList].GetHits(Threshold, Peaks->GetMaxI(Which), tempMass);
        // need to modify to turn off charge dependence when selected off

		pval = CalcPvalueTopHit(a, 
                                numhits,
                                N,
                                Normal,
                                TopHitProb
                                );       


	    }
	    else {

            pval = CalcPvalue(a, HitList[iHitList].
                           GetHits(Threshold, Peaks->GetMaxI(Which)),
                           Peaks->GetPeptidesExamined(Charge));
	    }
    if(UseRankScore) {
            if(HitList[iHitList].GetM() != 0.0) {
                double Average, StdDev, Perf;
                Average = ( HitList[iHitList].GetM() *(HitList[iHitList].GetN()+1))/2.0;
                StdDev = sqrt(Average * (HitList[iHitList].GetN() - HitList[iHitList].GetM()) / 6.0);
#ifdef HAVE_ERF
                Perf = 0.5*(1.0 + erf((HitList[iHitList].GetSum() - Average)/(StdDev*sqrt(2.0))));
#else
                Perf = 0.5*(1.0 + NCBI_Erf((HitList[iHitList].GetSum() - Average)/(StdDev*sqrt(2.0))));
#endif
                _TRACE( "N=" << HitList[iHitList].GetN() << " M=" << HitList[iHitList].GetM() <<
                     " Sum=" << HitList[iHitList].GetSum() << " Ave=" << Average << " SD=" << StdDev <<
                     " Perf=" << Perf  << " (1-Perf)/1000*pval=" << (1-Perf)/1000*pval <<
                    " pval*perf=" << pval*Perf);
//                pval *= (1 - Perf)/1000;
                pval *= Perf;

            }
            else ERR_POST(Info << "M is zero");
    }
	    double eval = /*2e5*/ 1e5 * pval * N;
        _TRACE( " pval=" << pval << " eval=" << eval );
	    ScoreList.insert(pair<const double, CMSHit *> 
			     (eval, &(HitList[iHitList])));
	}   
    } 
}


//calculates the poisson times the top hit probability
double CSearch::CalcPoissonTopHit(double Mean, int i, double TopHitProb)
{
double retval;
    retval =  exp(-Mean) * pow(Mean, i) / exp(BLAST_LnGammaInt(i+1));
//    retval =  exp(-Mean) * pow(Mean, i) / exp(lgamma(i+1));
// conditional needed because zero to the zeroth is zero, not one
if(TopHitProb != 1.0L) retval *= 1.0L - pow((1.0L-TopHitProb), i);
return retval;
}

double CSearch::CalcPoisson(double Mean, int i)
{
    return exp(-Mean) * pow(Mean, i) / exp(BLAST_LnGammaInt(i+1));
//    return exp(-Mean) * pow(Mean, i) / exp(lgamma(i+1));
}

double CSearch::CalcPvalue(double Mean, int Hits, int n)
{
    if(Hits <= 0) return 1.0L;
	
    int i;
    double retval(0.0L);
	
    for(i = 0; i < Hits; i++)
		retval += CalcPoisson(Mean, i);

    retval = 1.0L - retval;
//    retval = 1.0L - pow(retval, n);
    if(retval <= 0.0L) retval = numeric_limits<double>::min();
    return retval;
}

#define MSDOUBLELIMIT 0.0000000000000001L
// do full summation of probability distribution
double CSearch::CalcNormalTopHit(double Mean, double TopHitProb)
{
    int i;
    double retval(0.0L), before(-1.0L), increment;
	
    for(i = 1; i < 1000; i++) {
	increment = CalcPoissonTopHit(Mean, i, TopHitProb);
	// convergence hack -- on linux (at least) convergence test doesn't work
	// for gcc release build
	if(increment <= MSDOUBLELIMIT && i > 10) break;
	//	if(increment <= numeric_limits<double>::epsilon()) break;
	retval += increment;
	if(retval == before) break;  // convergence
	before = retval;
    }
    return retval;
}


double CSearch::CalcPvalueTopHit(double Mean, int Hits, int n, double Normal, double TopHitProb)
{
    if(Hits <= 0) return 1.0L;
	
    int i;
    double retval(0.0L), increment;
	
    for(i = 1; i < Hits; i++) {
	increment = CalcPoissonTopHit(Mean, i, TopHitProb);
//	if(increment <= MSDOUBLELIMIT) break;
	retval += increment;
    }

    retval /= Normal;
	
//    retval = 1.0L - pow(retval, n);
    retval = 1.0L - retval;
    if(retval <= 0.0L) retval = numeric_limits<double>::min();
    return retval;
}


double CSearch::CalcPoissonMean(int Start, int Stop, int Mass, CMSPeak *Peaks,
								int Charge, double Threshold)
{
    double retval(1.0);
    
    double m; // mass of ladder
    double o; // min m/z value of peaks
    double r; // max m/z value of peaks
    double h; // number of calculated m/z values in one ion series
    double v1; // number of m/z values above mh/2 and below mh (+1 product ions)
    double v2; // number of m/z value below mh/2
    double v;  // number of peaks
    double t; // mass tolerance width
	
    int High, Low, NumPeaks, NumLo, NumHi;
    Peaks->HighLow(High, Low, NumPeaks, Mass, Charge, Threshold, NumLo, NumHi);
    if(High == -1) return -1.0; // error
    o = MSSCALE2DBL(Low);
    r = MSSCALE2DBL(High);
    v1 = NumHi;
    v2 = NumLo;
    v = NumPeaks;
    m = MSSCALE2DBL(Mass);

    // limit h by maxproductions
    // todo: add correction for Charge > +2
    if(MyRequest->GetSettings().GetMaxproductions() != 0)
        h = min(Stop-Start, MyRequest->GetSettings().GetMaxproductions());
    else
        h = Stop - Start;
    // and by requests not to search b1 and bn/y1 ions
    h = 2*(h - MyRequest->GetSettings().GetSearchctermproduct()) -
         MyRequest->GetSettings().GetSearchb1();
    t = MSSCALE2DBL(Peaks->GetTol());
    // see 12/13/02 notebook, pg. 127
	
    retval = 2.0 * t * h * v / m;
    if(Charge >= Peaks->GetConsiderMult()) {
        if (r - o == 0.0) retval = 0.0;
        else retval *= (m - 3*o + r)/(r - o);
    }    
#if 0
    // variation that counts +1 and +2 separately
    // see 8/19/03 notebook, pg. 71
    retval = 4.0 * t * h / m;
    if(Charge >= Peaks->GetConsiderMult()) {
	//		retval *= (m - 3*o + r)/(r - o);
	retval *= (3 * v2 + v1);
    }
    else retval *= (v2 + v1);
#endif
    return retval;
}



CSearch::~CSearch()
{
}

/*
$Log$
Revision 1.65  2005/10/24 21:46:13  lewisg
exact mass, peptide size limits, validation, code cleanup

Revision 1.64  2005/10/03 18:05:03  lewisg
reverse rank score

Revision 1.63  2005/09/21 18:05:59  lewisg
speed up non-specific search, add fields to result

Revision 1.62  2005/09/20 21:07:57  lewisg
get rid of c-toolkit dependencies and nrutil

Revision 1.61  2005/09/16 21:40:09  lewisg
fix mod count

Revision 1.60  2005/09/15 21:29:24  lewisg
filter out n-term protein mods

Revision 1.59  2005/09/14 18:50:56  lewisg
add theoretical mass to hit

Revision 1.58  2005/09/14 17:46:11  lewisg
treat n-term methionine cut as cleavage

Revision 1.57  2005/09/14 15:30:17  lewisg
neutral loss

Revision 1.56  2005/09/07 21:30:50  lewisg
force fixed and variable mods not to overlap

Revision 1.55  2005/08/16 02:52:51  lewisg
adjust n dependence

Revision 1.54  2005/08/15 14:24:56  lewisg
new mod, enzyme; stat test

Revision 1.53  2005/08/03 17:59:29  lewisg
*** empty log message ***

Revision 1.52  2005/08/01 13:44:18  lewisg
redo enzyme classes, no-enzyme, fix for fixed mod enumeration

Revision 1.51  2005/06/16 21:22:11  lewisg
fix n dependence

Revision 1.50  2005/05/27 20:23:38  lewisg
top-down charge handling

Revision 1.49  2005/05/23 19:07:34  lewisg
improve perf of ladder calculation

Revision 1.48  2005/05/19 22:17:16  lewisg
move arrays to AutoPtr

Revision 1.47  2005/05/19 21:19:28  lewisg
add ncbifloat.h include for msvc

Revision 1.46  2005/05/19 20:26:41  lewisg
make irix happy

Revision 1.45  2005/05/19 16:59:17  lewisg
add top-down searching, fix variable mod bugs

Revision 1.44  2005/05/13 17:59:41  lewisg
*** empty log message ***

Revision 1.43  2005/05/13 17:57:17  lewisg
one mod per site and bug fixes

Revision 1.42  2005/04/25 21:50:33  lewisg
fix bug in tax filter

Revision 1.41  2005/04/22 15:31:21  lewisg
fix tax filter on output

Revision 1.40  2005/04/05 21:02:52  lewisg
increase number of mods, fix gi problem, fix empty scan bug

Revision 1.39  2005/03/17 23:37:08  lewisg
more tinkering with the poisson

Revision 1.38  2005/03/16 23:01:34  lewisg
fix i=0 poisson summation

Revision 1.37  2005/03/14 22:29:54  lewisg
add mod file input

Revision 1.36  2005/01/31 17:30:57  lewisg
adjustable intensity, z dpendence of precursor mass tolerance

Revision 1.35  2005/01/11 21:08:43  lewisg
average mass search

Revision 1.34  2004/12/20 20:24:16  lewisg
fix uncalc ladder bug, cleanup

Revision 1.33  2004/12/07 23:38:22  lewisg
add modification handling code

Revision 1.32  2004/12/06 23:35:16  lewisg
get rid of file charge

Revision 1.31  2004/11/30 23:39:57  lewisg
fix interval query

Revision 1.30  2004/11/22 23:10:36  lewisg
add evalue cutoff, fix fixed mods

Revision 1.29  2004/11/17 23:42:11  lewisg
add cterm pep mods, fix prob for tophitnum

Revision 1.28  2004/11/15 15:32:40  lewisg
memory overwrite fixes

Revision 1.27  2004/09/29 19:43:09  lewisg
allow setting of ions

Revision 1.26  2004/09/15 18:35:00  lewisg
cz ions

Revision 1.25  2004/07/22 22:22:58  lewisg
output mods

Revision 1.24  2004/07/06 22:38:05  lewisg
tax list input and user settable modmax

Revision 1.23  2004/06/23 22:34:36  lewisg
add multiple enzymes

Revision 1.22  2004/06/21 21:19:27  lewisg
new mods (including n term) and sample perl parser

Revision 1.21  2004/06/08 19:46:21  lewisg
input validation, additional user settable parameters

Revision 1.20  2004/05/27 20:52:15  lewisg
better exception checking, use of AutoPtr, command line parsing

Revision 1.19  2004/05/21 21:41:03  gorelenk
Added PCH ncbi_pch.hpp

Revision 1.18  2004/04/06 19:53:20  lewisg
allow adjustment of precursor charges that allow multiply charged product ions

Revision 1.17  2004/04/05 20:49:16  lewisg
fix missed mass bug and reorganize code

Revision 1.16  2004/03/31 02:00:26  ucko
+<algorithm> for sort()

Revision 1.15  2004/03/30 19:36:59  lewisg
multiple mod code

Revision 1.14  2004/03/16 20:18:54  gorelenk
Changed includes of private headers.

Revision 1.13  2004/03/01 18:24:08  lewisg
better mod handling

Revision 1.12  2003/12/22 21:58:00  lewisg
top hit code and variable mod fixes

Revision 1.11  2003/12/04 23:39:08  lewisg
no-overlap hits and various bugfixes

Revision 1.10  2003/11/20 22:32:50  lewisg
fix end of sequence X bug

Revision 1.9  2003/11/20 15:40:53  lewisg
fix hitlist bug, change defaults

Revision 1.8  2003/11/17 18:36:56  lewisg
fix cref use to make sure ref counts are decremented at loop termination

Revision 1.7  2003/11/14 20:28:06  lewisg
scale precursor tolerance by charge

Revision 1.6  2003/11/13 19:07:38  lewisg
bugs: iterate completely over nr, don't initialize blastdb by iteration

Revision 1.5  2003/11/10 22:24:12  lewisg
allow hitlist size to vary

  Revision 1.4  2003/10/24 21:28:41  lewisg
  add omssa, xomssa, omssacl to win32 build, including dll
  
	Revision 1.3  2003/10/22 15:03:32  lewisg
	limits and string compare changed for gcc 2.95 compatibility
	
	  Revision 1.2  2003/10/21 21:12:17  lewisg
	  reorder headers
	  
		Revision 1.1  2003/10/20 21:32:13  lewisg
		ommsa toolkit version
		
		  Revision 1.18  2003/10/07 18:02:28  lewisg
		  prep for toolkit
		  
			Revision 1.17  2003/10/06 18:14:17  lewisg
			threshold vary
			
			  Revision 1.16  2003/08/14 23:49:22  lewisg
			  first pass at variable mod
			  
				Revision 1.15  2003/08/06 18:29:11  lewisg
				support for filenames, numbers using regex
				
				  Revision 1.14  2003/07/21 20:25:03  lewisg
				  fix missing peak bug
				  
					Revision 1.13  2003/07/19 15:07:38  lewisg
					indexed peaks
					
					  Revision 1.12  2003/07/18 20:50:34  lewisg
					  *** empty log message ***
					  
						Revision 1.11  2003/07/17 18:45:50  lewisg
						multi dta support
						
						  Revision 1.10  2003/07/07 16:17:51  lewisg
						  new poisson distribution and turn off histogram
						  
							Revision 1.9  2003/05/01 14:52:10  lewisg
							fixes to scoring
							
							  Revision 1.8  2003/04/18 20:46:52  lewisg
							  add graphing to omssa
							  
								Revision 1.7  2003/04/07 16:47:16  lewisg
								pc fixes
								
								  Revision 1.6  2003/04/04 18:43:51  lewisg
								  tax cut
								  
									Revision 1.5  2003/04/02 18:49:51  lewisg
									improved score, architectural changes
									
									  Revision 1.4  2003/03/21 21:14:40  lewisg
									  merge ming's code, other stuff
									  
										Revision 1.3  2003/02/10 19:37:56  lewisg
										perf and web page cleanup
										
										  Revision 1.2  2003/02/07 16:18:23  lewisg
										  bugfixes for perf and to work on exp data
										  
											Revision 1.1  2003/02/03 20:39:02  lewisg
											omssa cgi
											
											  
												
*/
