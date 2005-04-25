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
#include <corelib/ncbi_limits.hpp>

#include <fstream>
#include <string>
#include <list>
#include <deque>
#include <algorithm>
#include <math.h>

#include "SpectrumSet.hpp"
#include "omssa.hpp"

#include "nrutil.h"
#include <ncbimath.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  CSearch::
//
//  Performs the ms/ms search
//


CSearch::CSearch(void): rdfp(0)
{
}

int CSearch::InitBlast(const char *blastdb, bool InitDb)
{
    if(!blastdb) return 0;
    if(rdfp) readdb_destruct(rdfp);
    rdfp = readdb_new_ex2((char *)blastdb,TRUE, READDB_NEW_DO_TAXDB | 
			  READDB_NEW_INDEX, NULL, NULL);
    if(!rdfp) return 1;
	
    numseq = readdb_get_num_entries(rdfp);
	
    if(InitDb) {
	int iSearch, length;
	char testchar;
	unsigned char *Sequence;
		
	for(iSearch = 0; iSearch < numseq; iSearch++) {
	    length = readdb_get_sequence(rdfp, iSearch, &Sequence);
	    testchar = Sequence[0];
	}
    }
    return 0;
	
}


// create the ladders from sequence

int CSearch::CreateLadders(unsigned char *Sequence, int iSearch, int position,
			   int endposition,
			   int *Masses, int iMissed, CAA& AA, 
			   CLadder& BLadder,
			   CLadder& YLadder, CLadder& B2Ladder,
			   CLadder& Y2Ladder,
			   unsigned ModMask,
			   const char **Site,
			   int *DeltaMass,
			   int NumMod,
               int ForwardIon,
               int BackwardIon)
{
    if(!BLadder.CreateLadder(ForwardIon, 1, (char *)Sequence, iSearch,
			     position, endposition, Masses[iMissed], 
			     MassArray, AA, ModMask, Site, DeltaMass,
			     NumMod)) return 1;
    if(!YLadder.CreateLadder(BackwardIon, 1, (char *)Sequence, iSearch,
			 position, endposition, Masses[iMissed], 
			     MassArray, AA, ModMask, Site, DeltaMass,
			     NumMod)) return 1;
    if(!B2Ladder.CreateLadder(ForwardIon, 2, (char *)Sequence, iSearch,
			  position, endposition, 
			  Masses[iMissed], 
			  MassArray, AA, ModMask, Site, DeltaMass,
			  NumMod)) return 1;
    if(!Y2Ladder.CreateLadder(BackwardIon, 2, (char *)Sequence, iSearch,
			  position, endposition,
			  Masses[iMissed], 
			  MassArray, AA, ModMask, Site, DeltaMass,
			  NumMod)) return 1;
    
    return 0;
}


// compare ladders to experiment
int CSearch::CompareLadders(CLadder& BLadder,
			    CLadder& YLadder, CLadder& B2Ladder,
			    CLadder& Y2Ladder, CMSPeak *Peaks,
			    bool OrLadders,  const TMassPeak *MassPeak)
{
    if(MassPeak && MassPeak->Charge >= Peaks->GetConsiderMult()) {
		Peaks->CompareSorted(BLadder, MSCULLED2, 0); 
		Peaks->CompareSorted(YLadder, MSCULLED2, 0); 
		Peaks->CompareSorted(B2Ladder, MSCULLED2, 0); 
		Peaks->CompareSorted(Y2Ladder, MSCULLED2, 0);
	if(OrLadders) {
	    BLadder.Or(B2Ladder);
	    YLadder.Or(Y2Ladder);	
	}
    }
    else {
		Peaks->CompareSorted(BLadder, MSCULLED1, 0); 
		Peaks->CompareSorted(YLadder, MSCULLED1, 0); 
    }
    return 0;
}

// compare ladders to experiment
bool CSearch::CompareLaddersTop(CLadder& BLadder,
			    CLadder& YLadder, CLadder& B2Ladder,
			    CLadder& Y2Ladder, CMSPeak *Peaks,
			    const TMassPeak *MassPeak)
{
    if(MassPeak && MassPeak->Charge >=  Peaks->GetConsiderMult() ) {
		if(Peaks->CompareTop(BLadder)) return true; 
		if(Peaks->CompareTop(YLadder)) return true; 
		if(Peaks->CompareTop(B2Ladder)) return true; 
		if(Peaks->CompareTop(Y2Ladder)) return true;
    }
    else {
		if(Peaks->CompareTop(BLadder)) return true; 
		if(Peaks->CompareTop(YLadder)) return true; 
    }
    return false;
}

#ifdef _DEBUG
#define CHECKGI
#endif
#ifdef CHECKGI
bool CheckGi(int gi)
{
    if( gi == 6323138){
	ERR_POST(Info << "test seq");
	return true;
    }
    return false;
}
#endif


// loads spectra into peaks
void CSearch::Spectrum2Peak(CMSRequest& MyRequest, CMSPeakSet& PeakSet)
{
    CSpectrumSet::Tdata::const_iterator iSpectrum;
    CMSPeak* Peaks;
	
    iSpectrum = MyRequest.GetSpectra().Get().begin();
    for(; iSpectrum != MyRequest.GetSpectra().Get().end(); iSpectrum++) {
	CRef <CMSSpectrum> Spectrum =  *iSpectrum;
	if(!Spectrum) {
	    ERR_POST(Error << "omssa: unable to find spectrum");
	    return;
	}
		
	Peaks = new CMSPeak(MyRequest.GetSettings().GetHitlistlen());
	if(!Peaks) {
	    ERR_POST(Error << "omssa: unable to allocate CMSPeak");
	    return;
	}
	if(Peaks->Read(*Spectrum, MyRequest.GetSettings().GetMsmstol(), 
		       MyRequest.GetSettings().GetScale()) != 0) {
	    ERR_POST(Error << "omssa: unable to read spectrum into CMSPeak");
	    return;
	}
    Peaks->SetName().clear();
	if(Spectrum->CanGetIds()) Peaks->SetName() = Spectrum->GetIds();
	if(Spectrum->CanGetNumber())
	    Peaks->SetNumber() = Spectrum->GetNumber();
		
	Peaks->Sort();
	Peaks->SetComputedCharge(MyRequest.GetSettings().GetChargehandling());
	Peaks->InitHitList(MyRequest.GetSettings().GetMinhit());
	Peaks->CullAll(MyRequest.GetSettings().GetCutlo(),
		       MyRequest.GetSettings().GetSinglewin(),
		       MyRequest.GetSettings().GetDoublewin(),
		       MyRequest.GetSettings().GetSinglenum(),
		       MyRequest.GetSettings().GetDoublenum(),
		       MyRequest.GetSettings().GetTophitnum());
		
	if(Peaks->GetNum(MSCULLED1) < MyRequest.GetSettings().GetMinspectra())
	    {
	    ERR_POST(Info << "omssa: not enough peaks in spectra");
	    Peaks->SetError(eMSHitError_notenuffpeaks);
	}
	PeakSet.AddPeak(Peaks);
		
    }
    PeakSet.SortPeaks(static_cast <int> (MyRequest.GetSettings().GetPeptol()*MSSCALE),
                      MyRequest.GetSettings().GetZdep());
	
}

// compares TMassMasks.  Lower m/z first in sort.
struct CMassMaskCompare {
    bool operator() (const TMassMask& x, const TMassMask& y)
    {
	if(x.Mass < y.Mass) return true;
	return false;
    }
};

// update sites and masses for new peptide
void CSearch::UpdateWithNewPep(int Missed,
			       const char *PepStart[],
			       const char *PepEnd[], 
			       int NumMod[], 
			       const char *Site[][MAXMOD],
			       int DeltaMass[][MAXMOD],
			       int Masses[],
			       int EndMasses[],
                               int ModEnum[][MAXMOD],
                               int IsFixed[][MAXMOD])
{
    // iterate over missed cleavages
    int iMissed;
    // maximum mods allowed
    int ModMax; 
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
	if(NumMod[iMissed] + NumMod[Missed-1] >= MAXMOD)
	    ModMax = MAXMOD - NumMod[iMissed];
	else ModMax = NumMod[Missed-1];

	// now interatu thru the new entries
	for(iMod = NumMod[iMissed]; 
	    iMod < NumMod[iMissed] + ModMax;
	    iMod++) {
	    Site[iMissed][iMod] = 
		Site[Missed-1][iMod - NumMod[iMissed]];

	    DeltaMass[iMissed][iMod] = 
		DeltaMass[Missed-1][iMod - NumMod[iMissed]];
        
	    ModEnum[iMissed][iMod] = 
		ModEnum[Missed-1][iMod - NumMod[iMissed]];

	    IsFixed[iMissed][iMod] = 
		IsFixed[Missed-1][iMod - NumMod[iMissed]];       

	}
				
	// update old masses
	Masses[iMissed] += Masses[Missed - 1];
				
	// update end masses
	EndMasses[iMissed] = EndMasses[Missed - 1];
			
	// update number of Mods
	NumMod[iMissed] += ModMax;
    }	    
}


// create the various combinations of mods
void CSearch::CreateModCombinations(int Missed,
				    const char *PepStart[],
				    TMassMask MassAndMask[][MAXMOD2],
				    int Masses[],
				    int EndMasses[],
				    int NumMod[],
				    int DeltaMass[][MAXMOD],
				    unsigned NumMassAndMask[],
				    int MaxModPerPep,
                    int IsFixed[][MAXMOD]
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
	MassAndMask[iMissed][iModCount].Mass = 
	    Masses[iMissed] + EndMasses[iMissed];
	MassAndMask[iMissed][iModCount].Mask = 0;
    int NumVariable(NumMod[iMissed]);  // number of variable mods
    int NumFixed;
    // add in fixed mods
    for(iMod = 0; iMod < NumMod[iMissed]; iMod++) {
        if(IsFixed[iMissed][iMod]) {
            MassAndMask[iMissed][iModCount].Mass += DeltaMass[iMissed][iMod];
            MassAndMask[iMissed][iModCount].Mask |= 1 << iMod;
            NumVariable--;
        }
    }
	iModCount++;
    NumFixed = NumMod[iMissed] - NumVariable;
		
	// go thru number of mods allowed
	for(iMod = 0; iMod < NumVariable && iModCount < MaxModPerPep; iMod++) {
		    
	    // initialize ModIndex that points to mod sites

        // todo: ModIndex must always include fixed mods

	    InitModIndex(ModIndex, iMod, NumMod[iMissed], IsFixed[iMissed]);
	    do {
    			
    		// calculate mass
    		MassOfMask = MassAndMask[iMissed][0].Mass;
    		for(iiMod = 0; iiMod <= iMod; iiMod++ ) 
    		    MassOfMask += DeltaMass[iMissed][ModIndex[iiMod + NumFixed]];
    		// make bool mask
    		Mask = MakeBoolMask(ModIndex, iMod + NumFixed);
    		// put mass and mask into storage
    		MassAndMask[iMissed][iModCount].Mass = MassOfMask;
    		MassAndMask[iMissed][iModCount].Mask = Mask;
    		// keep track of the  number of ladders
    		iModCount++;

// todo: calcmodindex must ignore fixed mods

	    } while(iModCount < MaxModPerPep &&
		    CalcModIndex(ModIndex, iMod, NumMod[iMissed], NumFixed, IsFixed[iMissed]));
	} // iMod

	// sort mask and mass by mass
	sort(MassAndMask[iMissed], MassAndMask[iMissed] + iModCount,
	     CMassMaskCompare());
	// keep track of number of MassAndMask
	NumMassAndMask[iMissed] = iModCount;

    } // iMissed
}

//#define MSSTATRUN

// set up the ions to search
void CSearch::SetIons(CMSRequest& MyRequest, int& ForwardIon, int& BackwardIon)
{
    if(MyRequest.GetSettings().GetIonstosearch().size() != 2) {
        ERR_POST(Fatal << "omssa: two search ions need to be specified");
    }
    CMSSearchSettings::TIonstosearch::const_iterator i;
    i = MyRequest.GetSettings().GetIonstosearch().begin();
    ForwardIon = *i;
    i++;
    BackwardIon = *i;
}


//! set up modifications from both user input and mod file data
/*!
\param MyRequest the user search params and spectra
\param Modset list of modifications
*/
void CSearch::SetupMods(CMSRequest& MyRequest, CRef <CMSModSpecSet>& Modset)
{
    //Modset->Append(MyRequest.);
    Modset->CreateArrays();
}


//! Performs the ms/ms search
/*!
\param MyRequest the user search params and spectra
\param MyResponse the results of the search
\param Modset list of modifications
*/
int CSearch::Search(CMSRequest& MyRequest, CMSResponse& MyResponse, CRef <CMSModSpecSet>& Modset)
{

    try {

        SetupMods(MyRequest, Modset);
	int length;
	CCleave* enzyme =
		  CCleaveFactory::CleaveFactory(static_cast <EMSEnzymes> 
				 (MyRequest.GetSettings().GetEnzyme()));
	unsigned header;

#ifdef CHECKGI
	char *blastdefline;
	SeqId *sip;
#endif

	// set maximum number of ladders to calculate per peptide
	int MaxModPerPep = MyRequest.GetSettings().GetMaxmods();
	if(MaxModPerPep > MAXMOD2) MaxModPerPep = MAXMOD2;

    int ForwardIon(1), BackwardIon(4);
    SetIons(MyRequest, ForwardIon, BackwardIon);
	CLadder BLadder[MAXMOD2], YLadder[MAXMOD2], B2Ladder[MAXMOD2],
	    Y2Ladder[MAXMOD2];
	bool LadderCalc[MAXMOD2];  // have the ladders been calculated?
	CAA AA;
		
	int Missed = MyRequest.GetSettings().GetMissedcleave()+1;  // number of missed cleaves allowed + 1
	int iMissed; // iterate thru missed cleavages
    
	int iSearch, hits;
	unsigned char *Sequence;  // start position
	int endposition, position;
	FixedMods.Init(MyRequest.GetSettings().GetFixed(), Modset);
	MassArray.Init(FixedMods, MyRequest.GetSettings().GetProductsearchtype(), Modset);
	PrecursorMassArray.Init(FixedMods, 
                            MyRequest.GetSettings().GetPrecursorsearchtype(), Modset);
	VariableMods.Init(MyRequest.GetSettings().GetVariable(), Modset);
	const int *IntMassArray = MassArray.GetIntMass();
	const int *PrecursorIntMassArray = PrecursorMassArray.GetIntMass();
	const char *PepStart[MAXMISSEDCLEAVE];
	const char *PepEnd[MAXMISSEDCLEAVE];

	// the position within the peptide of a variable modification
	const char *Site[MAXMISSEDCLEAVE][MAXMOD];
	// the modification mass at the Site
	int DeltaMass[MAXMISSEDCLEAVE][MAXMOD];
	// the modification type (used for saving for output)
	int ModEnum[MAXMISSEDCLEAVE][MAXMOD];
	// track fixed mods
	int IsFixed[MAXMISSEDCLEAVE][MAXMOD];
	// the number of modified sites + 1 unmodified
	int NumMod[MAXMISSEDCLEAVE];


	// calculated masses and masks
	TMassMask MassAndMask[MAXMISSEDCLEAVE][MAXMOD2];
	// the number of masses and masks for each peptide
	unsigned NumMassAndMask[MAXMISSEDCLEAVE];
	
	// set up mass array, indexed by missed cleavage
	// note that EndMasses is the end mass of peptide, kept separate to allow
	// reuse of Masses array in missed cleavage calc
	int Masses[MAXMISSEDCLEAVE];
	int EndMasses[MAXMISSEDCLEAVE];


	int iMod;   // used to iterate thru modifications
	
	bool SequenceDone;  // are we done iterating through the sequences?
	int taxid;
	const CMSSearchSettings::TTaxids& Tax = MyRequest.GetSettings().GetTaxids();
	CMSSearchSettings::TTaxids::const_iterator iTax;
	CMSHit NewHit;  // a new hit of a ladder to an m/z value
	CMSHit *NewHitOut;  // copy of new hit
	
	
	CMSPeakSet PeakSet;
	const TMassPeak *MassPeak; // peak currently in consideration
	CMSPeak* Peaks;
	
	Spectrum2Peak(MyRequest, PeakSet);

#ifdef MSSTATRUN
	ofstream charge1("charge1.txt");
	ofstream charge2("charge2.txt");
	ofstream charge3("charge3.txt");
#endif

	// iterate through sequences
	for(iSearch = 0; iSearch < numseq; iSearch++) {
	    if(iSearch/10000*10000 == iSearch) ERR_POST(Info << "sequence " << 
							iSearch);
	    header = 0;
#ifdef CHECKGI
	    int testgi;
#endif 
	    if(MyRequest.GetSettings().IsSetTaxids()) {
		while(readdb_get_header_ex(rdfp, iSearch, &header, 
#ifdef CHECKGI
					   &sip, &blastdefline,
#else
					   NULL, NULL,
#endif

					   &taxid, NULL, NULL)) {
			
#ifdef CHECKGI
		    SeqId *bestid = SeqIdFindBest(sip, SEQID_GI);
		    CheckGi(bestid->data.intvalue);
		    testgi = bestid->data.intvalue;
		    //			    cout << testgi << endl;
#endif 
			
#ifdef CHECKGI
		    MemFree(blastdefline);
		    SeqIdSetFree(sip);
#endif
		    for(iTax = Tax.begin(); iTax != Tax.end(); iTax++) {
			if(taxid == *iTax) goto TaxContinue;
		    } 
		    //else goto TaxContinue;
		}
		continue;
	    }
	TaxContinue:
	    length = readdb_get_sequence(rdfp, iSearch, &Sequence);
	    SequenceDone = false;
	    //	PepStart = (const char *)Sequence[0];
		
	    // initialize missed cleavage matrix
	    for(iMissed = 0; iMissed < Missed; iMissed++) {
		PepStart[iMissed] = (const char *)-1; // mark start
		PepEnd[iMissed] = (const char *)Sequence;
		Masses[iMissed] = 0;
		EndMasses[iMissed] = 0;
		NumMod[iMissed] = 0;

		DeltaMass[iMissed][0] = 0;
		ModEnum[iMissed][0] = 0;
        IsFixed[iMissed][0] = 0;
		Site[iMissed][0] = (const char *)-1;
	    }
	    PepStart[Missed - 1] = (const char *)Sequence;
		
	    // iterate thru the sequence by digesting it
	    while(!SequenceDone) {
			
		// zero out no missed cleavage peptide mass and mods
		// note that Masses and EndMass are separate to reuse
		// masses during the missed cleavage calculation
		Masses[Missed - 1] = 0;
		EndMasses[Missed - 1] = 0;
		NumMod[Missed - 1] = 0;
		// init no modification elements
		Site[Missed - 1][0] = (const char *)-1;
		DeltaMass[Missed - 1][0] = 0;
        ModEnum[Missed - 1][0] = 0;
        IsFixed[Missed - 1][0] = 0;

#ifdef CHECKGI
{

			    SeqId *bestid;
						
			    header = 0;
			    readdb_get_header(rdfp, iSearch, &header, &sip, &blastdefline);
			    bestid = SeqIdFindBest(sip, SEQID_GI);
			    if(CheckGi(bestid->data.intvalue) /*&& ((PepStart[iMissed] - (const char *)Sequence) == 355) */ )
				cerr << "hello" << endl;
			    //			int testgi = bestid->data.intvalue;
			    MemFree(blastdefline);
			    SeqIdSetFree(sip);
}
#endif

		// calculate new stop and mass
		SequenceDone = 
		    enzyme->CalcAndCut((const char *)Sequence,
				       (const char *)Sequence + length - 1, 
				       &(PepEnd[Missed - 1]),
				       &(Masses[Missed - 1]),
				       NumMod[Missed - 1],
				       MAXMOD,
				       &(EndMasses[Missed - 1]),
				       VariableMods, FixedMods,
				       Site[Missed - 1],
				       DeltaMass[Missed - 1],
				       IntMassArray,
                       PrecursorIntMassArray,
                       ModEnum[Missed - 1],
                       IsFixed[Missed - 1],
                               Modset
                               );
			

		UpdateWithNewPep(Missed, PepStart, PepEnd, NumMod, Site,
				 DeltaMass, Masses, EndMasses, ModEnum, IsFixed);
	
		CreateModCombinations(Missed, PepStart, MassAndMask, Masses,
				      EndMasses, NumMod, DeltaMass, NumMassAndMask,
				      MaxModPerPep, IsFixed);


		int OldMass;  // keeps the old peptide mass for comparison
		bool NoMassMatch;  // was there a match to the old mass?
        CIntervalTree::const_iterator im; // iterates over interval tree

		for(iMissed = 0; iMissed < Missed; iMissed++) {	    
		    if(PepStart[iMissed] == (const char *)-1) continue;  // skip start
				
		    // get the start and stop position, inclusive, of the peptide
		    position =  PepStart[iMissed] - (const char *)Sequence;
		    endposition = PepEnd[iMissed] - (const char *)Sequence;
		
		    // init bool for "Has ladder been calculated?"
		    for(iMod = 0; iMod < MaxModPerPep; iMod++) 
		      LadderCalc[iMod] = false;
		
		    OldMass = 0;
		    NoMassMatch = true;
				
		    // go thru total number of mods
		    for(iMod = 0; iMod < NumMassAndMask[iMissed]; iMod++) {
		    
			// have we seen this mass before?
			if(MassAndMask[iMissed][iMod].Mass == OldMass &&
			   NoMassMatch) continue;
			NoMassMatch = true;
			OldMass = MassAndMask[iMissed][iMod].Mass;
			// return peak where theoretical mass is < precursor mass + tol
            for(im = PeakSet.SetIntervalTree().IntervalsContaining(OldMass); im; ++im )
            {
                MassPeak = static_cast <const TMassPeak *> (im.GetValue().GetPointerOrNull());

			    Peaks = MassPeak->Peak;
			    // make sure we look thru other mod masks with the same mass
			    NoMassMatch = false;
//                if(Peaks->GetNumber() == 1338) cout << "1338:index=" << iSearch << /*" hits=" <<
//                     hits << */ " start=" << position << " stop=" << endposition << endl;
						
#ifdef CHECKGI
{

			    SeqId *bestid;
						
			    header = 0;
			    readdb_get_header(rdfp, iSearch, &header, &sip, &blastdefline);
			    bestid = SeqIdFindBest(sip, SEQID_GI);
			    if(/*Peaks->GetNumber() == 245 && */CheckGi(bestid->data.intvalue))
				cerr << "hello" << endl;
			    //			int testgi = bestid->data.intvalue;
			    MemFree(blastdefline);
			    SeqIdSetFree(sip);
}
#endif
						
						
			    if(!LadderCalc[iMod]) {
				if(CreateLadders(Sequence, iSearch, position,
						 endposition,
						 Masses,
						 iMissed, AA,
						 BLadder[iMod],
						 YLadder[iMod],
						 B2Ladder[iMod],
						 Y2Ladder[iMod],
						 MassAndMask[iMissed][iMod].Mask,
						 Site[iMissed],
						 DeltaMass[iMissed],
						 NumMod[iMissed],
                         ForwardIon,
                         BackwardIon
                         ) != 0) continue;
				LadderCalc[iMod] = true;	
				// continue to next sequence if ladders not successfully made
			    }
			    else {
				BLadder[iMod].ClearHits();
				YLadder[iMod].ClearHits();
				B2Ladder[iMod].ClearHits();
				Y2Ladder[iMod].ClearHits();
			    }
				
			    if(
#ifdef MSSTATRUN
			       true
#else
			       CompareLaddersTop(BLadder[iMod], 
						 YLadder[iMod],
						 B2Ladder[iMod], 
						 Y2Ladder[iMod],
						 Peaks, MassPeak)
#endif
			       ) {
				// end of new addition


				Peaks->SetPeptidesExamined(MassPeak->Charge)++;
#ifdef CHECKGI
				/*if(Peaks->GetNumber() == 245)*/
				//			    cout << testgi << " " << position << " " << endposition << endl;
#endif
				Peaks->ClearUsedAll();
				CompareLadders(BLadder[iMod], 
					       YLadder[iMod],
					       B2Ladder[iMod], 
					       Y2Ladder[iMod],
					       Peaks, false, MassPeak);
				hits = BLadder[iMod].HitCount() + 
				    YLadder[iMod].HitCount() +
				    B2Ladder[iMod].HitCount() +
				    Y2Ladder[iMod].HitCount();
#ifdef MSSTATRUN
				switch (MassPeak->Charge)
				    {
				    case 1:
					charge1 << hits << endl;
					break;
				    case 2:
					charge2 << hits << endl;
					break;
				    case 3:
					charge3 << hits << endl;
					break;
				    default:
					break;
				    }
#endif
				if(hits >= MyRequest.GetSettings().GetMinhit()) {
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
					    RecordMatches(BLadder[iMod],
									  YLadder[iMod],
									  B2Ladder[iMod], 
									  Y2Ladder[iMod],
									  Peaks,
									  MassAndMask[iMissed][iMod].Mask,
									  Site[iMissed],
									  ModEnum[iMissed],
									  NumMod[iMissed],
									  PepStart[iMissed],
                                      IsFixed[iMissed]
									  );
				    }
				}
			    } // new addition
			} // MassPeak
		    } //iMod
		} // iMissed
		if(!SequenceDone) {
            int NumModCount;
		    // get rid of longest peptide and move the other peptides down the line
		    for(iMissed = 0; iMissed < Missed - 1; iMissed++) {
    			// move masses to next missed cleavage
    			Masses[iMissed] = Masses[iMissed + 1];
    			// don't move EndMasses as they are recalculated
    
    			// move the modification data
                NumModCount = 0;
    			for(iMod = 0; iMod < NumMod[iMissed + 1]; iMod++) {
                    // throw away the c term peptide mods as we have a new c terminus
                    if(Modset->GetModType(ModEnum[iMissed + 1][iMod]) != eMSModType_modcp  && 
                       Modset->GetModType(ModEnum[iMissed + 1][iMod]) != eMSModType_modcpaa) {
        			    DeltaMass[iMissed][NumModCount] = 
        				DeltaMass[iMissed + 1][iMod];
        			    Site[iMissed][NumModCount] = 
        				Site[iMissed + 1][iMod];
        			    ModEnum[iMissed][NumModCount] = 
        				ModEnum[iMissed + 1][iMod];
        			    IsFixed[iMissed][NumModCount] = 
        				IsFixed[iMissed + 1][iMod];
                        NumModCount++;
                    }
    			}
    			NumMod[iMissed] = NumModCount;
    
    			// copy starts to next missed cleavage
    			PepStart[iMissed] = PepStart[iMissed + 1];
		    }
			
		    // init new start from old stop
		    PepEnd[Missed-1] += 1;
		    PepStart[Missed-1] = PepEnd[Missed-1];
		    // PepStart = PepEnd + 1;
            } 
	    }
	}
	
	// read out hits
	SetResult(PeakSet, MyResponse, 
                  MyRequest);
	
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


void CSearch::SetResult(CMSPeakSet& PeakSet, CMSResponse& MyResponse,
                        CMSRequest& MyRequest)
{

    double ThreshStart = MyRequest.GetSettings().GetCutlo(); 
    double ThreshEnd = MyRequest.GetSettings().GetCuthi();
    double ThreshInc = MyRequest.GetSettings().GetCutinc();
    double Tophitnum = MyRequest.GetSettings().GetTophitnum();
    double Evalcutoff = MyRequest.GetSettings().GetCutoff();

    CMSPeak* Peaks;
    TPeakSet::iterator iPeakSet;
	
    TScoreList ScoreList;
    TScoreList::iterator iScoreList;
    CMSHit * MSHit;
    unsigned header;
    char *blastdefline;
    SeqId *sip, *bestid;
    int length;
    unsigned char *Sequence;
    char accession[kAccLen]; // temp holder for accession
    int iSearch; // counter for the length of hit list

    // set the search library version
    MyResponse.SetDbversion(numseq);
	
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
	MyResponse.SetHitsets().push_back(HitSet);
		
	if(Peaks->GetError() == eMSHitError_notenuffpeaks) {
	    ERR_POST(Info << "empty set");
	    HitSet->SetError(eMSHitError_notenuffpeaks);
	    continue;
	}
		
	double Threshold, MinThreshold(ThreshStart), MinEval(1000000.0L);
	// now calculate scores and sort
	for(Threshold = ThreshStart; Threshold <= ThreshEnd; 
	    Threshold += ThreshInc) {
	    CalcNSort(ScoreList, Threshold, Peaks, false, Tophitnum);
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
	_TRACE("Min Threshold = " << MinThreshold);
	CalcNSort(ScoreList,
#ifdef MSSTATRUN
 ThreshStart
#else
 MinThreshold
#endif
, Peaks, true, Tophitnum);
		
	int taxid;
	const CMSSearchSettings::TTaxids& Tax = MyRequest.GetSettings().GetTaxids();
	CMSSearchSettings::TTaxids::const_iterator iTax;
		
	// keep a list of redundant peptides
	map <string, CMSHits * > PepDone;
	// add to hitset by score
	for(iScoreList = ScoreList.begin(), iSearch = 0;
	    iScoreList != ScoreList.end();
	    iScoreList++, iSearch++) {

        double Score = iScoreList->first;
        if(Score > Evalcutoff) continue;
	    CMSHits * Hit;
	    CMSPepHit * Pephit;
			
	    MSHit = iScoreList->second;
	    header = 0;


	    while (readdb_get_header_ex(rdfp, MSHit->GetSeqIndex(), &header,
                                    &sip,
                                    &blastdefline, &taxid, NULL, NULL)) {

            if(MyRequest.GetSettings().IsSetTaxids()) {
                for(iTax = Tax.begin(); iTax != Tax.end(); iTax++) {
                    if(taxid == *iTax) goto TaxContinue2;
                } 
                continue;
            }
TaxContinue2:
		string seqstring, modseqstring;
		string deflinestring(blastdefline);
		int iseq;
		length = readdb_get_sequence(rdfp, MSHit->GetSeqIndex(),
					     &Sequence);
				
		for (iseq = MSHit->GetStart(); iseq <= MSHit->GetStop();
		     iseq++) {
		    seqstring += UniqueAA[Sequence[iseq]];
		}
		MakeModString(seqstring, modseqstring, MSHit);

		if(PepDone.find(modseqstring) != PepDone.end()) {
		    Hit = PepDone[modseqstring];
		}
		else {
		    Hit = new CMSHits;
		    Hit->SetPepstring(seqstring);
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

		accession[0] = 0;
		SeqIdWrite(SeqIdFindBestAccession(sip), accession,
			   PRINTID_TEXTID_ACCESSION, kAccLen);
		accession[kAccLen-1] = 0;
		if(accession[0] != '\0') Pephit->SetAccession(accession);

		bestid = SeqIdFindBest(sip, SEQID_GI);
		if(bestid && bestid->choice == SEQID_GI)
             Pephit->SetGi(bestid->data.intvalue);

		Pephit->SetStart(MSHit->GetStart());
		Pephit->SetStop(MSHit->GetStop());;
		Pephit->SetDefline(deflinestring);
		CRef<CMSPepHit> pepref(Pephit);
		Hit->SetPephits().push_back(pepref);

		MemFree(blastdefline);
		SeqIdSetFree(sip);
	    }
	}
	ScoreList.clear();
    }
}


// calculate the evalues of the top hits and sort
void CSearch::CalcNSort(TScoreList& ScoreList, double Threshold, CMSPeak* Peaks, bool NewScore, int Tophitnum)
{
    int iCharges;
    int iHitList;
    int length;
    unsigned char *Sequence;
	
    for(iCharges = 0; iCharges < Peaks->GetNumCharges(); iCharges++) {
		
	TMSHitList& HitList = Peaks->GetHitList(iCharges);   
	for(iHitList = 0; iHitList != Peaks->GetHitListIndex(iCharges);
	    iHitList++) {
#ifdef CHECKGI
	    unsigned header;
	    char *blastdefline;
	    SeqId *sip, *bestid;
			
	    header = 0;
	    readdb_get_header(rdfp, HitList[iHitList].GetSeqIndex(),
			      &header, &sip,&blastdefline);
	    bestid = SeqIdFindBest(sip, SEQID_GI);
	    if(!bestid) continue;
	    CheckGi(bestid->data.intvalue);
	    MemFree(blastdefline);
	    SeqIdSetFree(sip);
#endif
			
	    // recompute ladder
	    length = readdb_get_sequence(rdfp, 
					 HitList[iHitList].GetSeqIndex(),
					 &Sequence);
			
	    int tempMass = HitList[iHitList].GetMass();
	    int tempStart = HitList[iHitList].GetStart();
	    int tempStop = HitList[iHitList].GetStop();
	    int Charge = HitList[iHitList].GetCharge();
	    int Which = Peaks->GetWhich(Charge);
			
	    double a = CalcPoissonMean(tempStart, tempStop, tempMass, Peaks,
				       Charge, 
				       Threshold);
	    if ( a < 0) continue;  // error return;
	    if(HitList[iHitList].GetHits() < a) continue;
	    double pval;
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
		pval = CalcPvalueTopHit(a, 
					numhits,
					Peaks->GetPeptidesExamined(Charge), Normal, TopHitProb);
#ifdef MSSTATRUN
		cout << pval << " " << a << " " << Charge << endl;
		//		cerr << "pval = " << pval << " Normal = " << Normal << endl;
#endif
	    }
	    else {
		pval =
		    CalcPvalue(a, HitList[iHitList].
			       GetHits(Threshold, Peaks->GetMaxI(Which)),
			       Peaks->GetPeptidesExamined(Charge));
	    }
	    double eval = pval * Peaks->GetPeptidesExamined(Charge);
	    ScoreList.insert(pair<const double, CMSHit *> 
			     (eval, &(HitList[iHitList])));
	}   
    } 
}


//calculates the poisson times the top hit probability
double CSearch::CalcPoissonTopHit(double Mean, int i, double TopHitProb)
{
double retval;
#ifdef NCBI_OS_MSWIN
    retval =  exp(-Mean) * pow(Mean, i) / exp(LnGamma(i+1));
#else
    retval =  exp(-Mean) * pow(Mean, i) / exp(lgamma(i+1));
#endif
// conditional needed because zero to the zeroth is zero, not one
if(TopHitProb != 1.0L) retval *= 1.0L - pow((1.0L-TopHitProb), i);
return retval;
}

double CSearch::CalcPoisson(double Mean, int i)
{
#ifdef NCBI_OS_MSWIN
    return exp(-Mean) * pow(Mean, i) / exp(LnGamma(i+1));
#else
    return exp(-Mean) * pow(Mean, i) / exp(lgamma(i+1));
#endif
}

double CSearch::CalcPvalue(double Mean, int Hits, int n)
{
    if(Hits <= 0) return 1.0L;
	
    int i;
    double retval(0.0L);
	
    for(i = 0; i < Hits; i++)
		retval += CalcPoisson(Mean, i);
	
    retval = 1.0L - pow(retval, n);
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
	
    retval = 1.0L - pow(retval, n);
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
    o = (double)Low/MSSCALE;
    r = (double)High/MSSCALE;
    v1 = NumHi;
    v2 = NumLo;
    v = NumPeaks;
    m = Mass/(double)MSSCALE;
    h = Stop - Start;
    t = (Peaks->GetTol())/(double)MSSCALE;
    // see 12/13/02 notebook, pg. 127
	
    retval = 4.0 * t * h * v / m;
    if(Charge >= Peaks->GetConsiderMult()) {
	retval *= (m - 3*o + r)/(r - o);
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
    if(rdfp) readdb_destruct(rdfp);
}

/*
$Log$
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
