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
 *
 * File Description:
 *    code to deal with spectra and m/z ladders
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <algorithm>

#include <mspeak.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  CMSHit::
//
//  Used by CMSPeak class to hold hits
//

// helper function for RecordHits that scans thru a single ladder
void CMSHit::RecordMatchesScan(CLadder& Ladder, int& iHitInfo, CMSPeak *Peaks,
			       int Which)
{
    unsigned *Intensity = new unsigned [Ladder.size()];
    Peaks->CompareSorted(Ladder, Which, Intensity);

    // examine hits array
    unsigned i;
    for(i = 0; i < Ladder.size(); i++) {
	// if hit, add to hitlist
	if(Ladder.GetHit()[i] > 0) {
	    HitInfo[iHitInfo].SetCharge() = (char) Ladder.GetCharge();
	    HitInfo[iHitInfo].SetIon() = (char) Ladder.GetType();
	    HitInfo[iHitInfo].SetNumber() = (short) i;
	    HitInfo[iHitInfo].SetIntensity() = Intensity[i];
	    iHitInfo++;
	}
    }
    delete [] Intensity;
}

// make a record of the ions hit
void CMSHit::RecordMatches(CLadder& BLadder, CLadder& YLadder,
			   CLadder& B2Ladder,
			   CLadder& Y2Ladder, CMSPeak *Peaks)
{
    // create hitlist
    HitInfo = new CMSHitInfo[Hits];
    // increment thru hithist
    int iHitInfo(0); 
    int Which = Peaks->GetWhich(Charge);

    // scan thru each ladder
    if(Charge > 2) {
	RecordMatchesScan(BLadder, iHitInfo, Peaks, Which);
	RecordMatchesScan(YLadder, iHitInfo, Peaks, Which);
	RecordMatchesScan(B2Ladder, iHitInfo, Peaks, Which);
	RecordMatchesScan(Y2Ladder, iHitInfo, Peaks, Which);
    }
    else {
	RecordMatchesScan(BLadder, iHitInfo, Peaks, Which);
	RecordMatchesScan(YLadder, iHitInfo, Peaks, Which);
    }

}

// return number of hits above threshold
int CMSHit::GetHits(double Threshold, int MaxI)
{
    int i, retval(0);

    for(i = 0; i < Hits; i++)
        if(HitInfo[i].GetIntensity() > MaxI*Threshold)
            retval++;
    return retval;
}



/////////////////////////////////////////////////////////////////////////////
//
//  Comparison functions used in CMSPeak for sorting
//

// compares m/z.  Lower m/z first in sort.
struct CMZICompare {
    bool operator() (CMZI x, CMZI y)
    {
	if(x.MZ < y.MZ) return true;
	return false;
    }
};

// compares m/z.  High m/z first in sort.
struct CMZICompareHigh {
    bool operator() (CMZI x, CMZI y)
    {
	if(x.MZ > y.MZ) return true;
	return false;
    }
};

// compares intensity.  Higher intensities first in sort.
struct CMZICompareIntensity {
    bool operator() (CMZI x, CMZI y)
    {
	if(x.Intensity > y.Intensity) return true;
	return false;
    }
};



/////////////////////////////////////////////////////////////////////////////
//
//  CMSPeak::
//
//  Class used to hold spectra and convert to mass ladders
//


void CMSPeak::xCMSPeak(void)
{
    AAMap = AA.GetMap();
    Sorted[0] = Sorted[1] = false;
    MZI[MSCULLED1] = 0;
    MZI[MSCULLED2] = 0;
    MZI[MSORIGINAL] = 0;
    PlusOne = 0.8L;
    ComputedCharge = eChargeUnknown;
    Error = eMSHitError_none;
}

CMSPeak::CMSPeak(void)
{ 
    xCMSPeak(); 
}

CMSPeak::CMSPeak(int HitListSizeIn): HitListSize(HitListSizeIn)
{  
    xCMSPeak();
}

CMSPeak::~CMSPeak(void)
{  
    delete [] MZI[MSCULLED1];
    delete [] MZI[MSCULLED2];
    delete [] MZI[MSORIGINAL];
    int iCharges;
    for(iCharges = 0; iCharges < GetNumCharges(); iCharges++)
	delete [] HitList[iCharges];
}

// add hit to hitlist.  returns true if successful
bool CMSPeak::AddHit(CMSHit& in, CMSHit *& out)
{
    out = 0;
    // initialize index using charge
    int Index = in.GetCharge() - Charges[0];
    if(in.GetHits() < LastHitNum[Index]) return false;  
    int i, min(0);  // min is the minimum number of hits in the list
    // find the minimum in the hitslist
    for(i = 0; i < HitListIndex[Index] && 
	    in.GetHits() <= HitList[Index][i].GetHits(); i++)
        if(min > HitList[Index][i].GetHits())
	    min = HitList[Index][i].GetHits();
    if(i >= HitListSize) {
        LastHitNum[Index] = min;
        return false;
    }
    // add to end of list
    if(i >= HitListIndex[Index]) {
        HitList[Index][i] = in;
        out = & (HitList[Index][i]);
        HitListIndex[Index]++;
	return true;
    } 
    // otherwise replace in list
    else {
        HitList[Index][i] = in;
        out =  & (HitList[Index][i]);
	return true;
    } 
}


void CMSPeak::AddTotalMass(int massin, int tolin)
{
    TotalMass = massin;
    tol = tolin;
}


void CMSPeak::Sort(int Which)
{
    if((Which != MSORIGINAL && Which != MSCULLED1 && Which != MSCULLED2) || 
       !MZI[Which]) return;
    sort(MZI[Which], MZI[Which] + Num[Which], CMZICompare());
    Min = MZI[Which][0].MZ;
    Max = MZI[Which][Num[Which]-1].MZ;
    Sorted[Which] = true;
}

bool CMSPeak::Contains(int value, int Which)
{
    CMZI precursor;
    precursor.MZ = value -  tol; // /2;
    CMZI *p = lower_bound(MZI[Which], MZI[Which] + Num[Which], precursor, CMZICompare());
    if(p == MZI[Which] + Num[Which]) return false;
    if(p->MZ < value + tol/*/2*/) return true;
    return false;
}

		
bool CMSPeak::ContainsFast(int value, int Which)
{

    int x, l, r;
    
    l = 0;
    r = Num[Which] - 1;

    while(l <= r) {
        x = (l + r)/2;
        if (MZI[Which][x].MZ < value - tol) 
	    l = x + 1;
        else if (MZI[Which][x].MZ > value + tol)
	    r = x - 1;
	else return true;
    } 
    
    if (MZI[Which][x+1].MZ < value + tol && MZI[Which][x+1].MZ > value - tol) 
	return true;
    return false;
}

// compare assuming all lists are sorted
int CMSPeak::CompareSorted(CLadder& Ladder, int Which, unsigned* Intensity)
{
    unsigned i(0), j(0);
    int retval(0);
    if(Ladder.size() == 0 ||  Num[Which] == 0) return 0;

    do {
	if(MZI[Which][j].MZ > Ladder[i] + tol) {
	    i++;
	    if(i >= Ladder.size()) break;
	    continue;
	}
	else if (MZI[Which][j].MZ < Ladder[i] - tol) {
	    j++;
	    if(j >= Num[Which]) break;
	    continue;
	}
	else {
	    retval++;
	    Ladder.GetHit()[i] = Ladder.GetHit()[i] + 1;
	    // record the intensity if requested, used for auto adjust
	    if(Intensity) {
		Intensity[i] = MZI[Which][j].Intensity;
	    }
	    j++;
	    if(j >= Num[Which]) break;
	    i++;
	    if(i >= Ladder.size()) break;
	}
    } while(true);
    return retval;
}

int CMSPeak::Compare(CLadder& Ladder, int Which)
{
    unsigned i;
    int retval(0);
    for(i = 0; i < Ladder.size(); i++) {
	if(ContainsFast(Ladder[i], Which)) {
	    retval++;
	    Ladder.GetHit()[i] = Ladder.GetHit()[i] + 1;
	}
    }
    return retval;
}


void CMSPeak::Read(std::istream& FileIn, double ToleranceIn, CMSPeak::EFileType FileType)
{
    double inputval, totalmass, intensity;
    deque <int> temp1;
    deque <unsigned> temp2;
	
    if(!FileIn || FileType != eDTA) return;
    SetTolerance(ToleranceIn);
    FileIn >> totalmass >> Charge;
    TotalMass = static_cast <int> (totalmass * MSSCALE);
    Num[MSORIGINAL] = 0;
	
    while(FileIn)
	{
	    inputval = 0;
	    FileIn >> inputval >> intensity;
	    if(inputval == 0) break;
	    // attenuate the really big peaks
	    if(intensity > kMax_UInt) intensity = kMax_UInt;
	    temp1.push_back(static_cast <int>(inputval*MSSCALE));
	    temp2.push_back(static_cast <unsigned> (intensity));
	    Num[MSORIGINAL]++;
	} 
    MZI[MSORIGINAL] = new CMZI [Num[MSORIGINAL]];
    unsigned i;
    for(i = 0; i < Num[MSORIGINAL]; i++)
	{
	    MZI[MSORIGINAL][i].MZ = temp1[i];
	    MZI[MSORIGINAL][i].Intensity = temp2[i];
	}
    Sort(MSORIGINAL);
}


// read in an asn.1 spectrum and initialize peak values
int CMSPeak::Read(CMSSpectrum& Spectrum, double MSMSTolerance)
{
    int Scale = Spectrum.GetScale();
    TotalMass = Spectrum.GetPrecursormz()*MSSCALE/Scale;
    SetTolerance(MSMSTolerance);
    Charge = *(Spectrum.GetCharge().begin());
    Num[MSORIGINAL] = 0;   
    
    const CMSSpectrum::TMz& Mz = Spectrum.GetMz();
    const CMSSpectrum::TAbundance& Abundance = Spectrum.GetAbundance();
    MZI[MSORIGINAL] = new CMZI [Mz.size()];
    Num[MSORIGINAL] = Mz.size();

    int i;
    for(i = 0; i < Num[MSORIGINAL]; i++) {
	MZI[MSORIGINAL][i].MZ = Mz[i]*MSSCALE/Scale;
	MZI[MSORIGINAL][i].Intensity = Abundance[i]/Scale;
    }
    Sort(MSORIGINAL);
    return 0;
}

// return the number of peaks in a range
int CMSPeak::CountRange(double StartFraction, double StopFraction)
{
    CMZI Start, Stop;
    int Precursor = static_cast <int> (TotalMass/Charge + tol/2.0);
    Start.MZ = static_cast <int> (StartFraction * Precursor);
    Stop.MZ = static_cast <int> (StopFraction * Precursor);
    CMZI *LoHit = lower_bound(MZI[MSORIGINAL], MZI[MSORIGINAL] +
			      Num[MSORIGINAL], Start, CMZICompare());
    CMZI *HiHit = upper_bound(MZI[MSORIGINAL], MZI[MSORIGINAL] + 
			      Num[MSORIGINAL], Stop, CMZICompare());

    return HiHit - LoHit;
}

// return the percentage of peaks below and at the precursor
int CMSPeak::PercentBelow(void)
{
    CMZI precursor;
    precursor.MZ = static_cast <int> (TotalMass/Charge + tol/2.0);
    CMZI *Hit = upper_bound(MZI[MSORIGINAL], MZI[MSORIGINAL] + Num[MSORIGINAL],
			    precursor, CMZICompare());
    Hit++;  // go above the peak
    if(Hit >= MZI[MSORIGINAL] + Num[MSORIGINAL]) return Num[MSORIGINAL];
    return Hit-MZI[MSORIGINAL];
}

// is the data charge +1?
bool CMSPeak::IsPlus1(double PercentBelowIn)
{
    if(PercentBelowIn/Num[MSORIGINAL] > PlusOne) return true;
    return false;
}

// takes the ratio, low/high, of two ranges in the spectrum
double CMSPeak::RangeRatio(double Start, double Middle, double Stop)
{
    int Lo = CountRange(Start, Middle);
    int Hi = CountRange(Middle, Stop);
    return (double)Lo/Hi;
}

// calculate the charge
void CMSPeak::SetComputedCharge(int MaxCharge)
{
    if(IsPlus1(PercentBelow())) {
	ComputedCharge = eCharge1;
	Charges[0] = 1;
	NumCharges = 1;
    }
    else {
#if 0
	NumCharges = 1;
	if(RangeRatio(0.5, 1.0, 1.5) > 1.25 ) {
	    Charges[0] = 2;
	    ComputedCharge = eCharge2;
	}
	else if(RangeRatio(1.0, 1.5, 2.0) > 1.25) {
	    Charges[0] = 3;
	    ComputedCharge = eCharge3;
	}
	else {  // assume +4
	    Charges[0] = 4;
	    ComputedCharge = eCharge4;
	}
#endif
	ComputedCharge = eChargeNot1; 
	Charges[0] = 2;
	Charges[1] = 3;
	NumCharges = 2;
    }
}

// initializes arrays used to track hits
void CMSPeak::InitHitList(void)
{
    int iCharges;
    for(iCharges = 0; iCharges < GetNumCharges(); iCharges++) {
	LastHitNum[iCharges] = MSHITMIN;
	HitListIndex[iCharges] = 0;
	HitList[iCharges] = new CMSHit[HitListSize];
	PeptidesExamined[iCharges] = 0;
    }
}

void CMSPeak::xWrite(std::ostream& FileOut, CMZI *Temp, int Num)
{
    FileOut << (double)TotalMass/MSSCALE << " " << Charge << endl;

    int i;
    unsigned Intensity;
    for(i = 0; i < Num; i++) {
        Intensity = Temp[i].Intensity;
        if(Intensity == 0.0) Intensity = 1;  // make Mascot happy
        FileOut << (double)Temp[i].MZ/MSSCALE << " " << 
	    Intensity << endl;
    }
}

void CMSPeak::Write(std::ostream& FileOut, CMSPeak::EFileType FileType, int Which)
{
    if(!FileOut || FileType != CMSPeak::eDTA) return;
    xWrite(FileOut, MZI[Which], Num[Which]);
}


// counts the number of peaks above % of maximum peak

int CMSPeak::AboveThresh(double Threshold, int Which)
{
    CMZI *MZISort = new CMZI[Num[Which]];
    unsigned i;
    for(i = 0; i < Num[Which]; i++) MZISort[i] = MZI[Which][i];

    // sort so higher intensities first
    sort(MZISort, MZISort+Num[Which], CMZICompareIntensity());

    for(i = 1; i < Num[Which] &&
	    (double)MZISort[i].Intensity/MZISort[0].Intensity > Threshold; i++);
    delete [] MZISort;
    return i;
}


// Truncate the at the precursor if plus one and set charge to 1
// if charge is erronously set to 1, set it to 2

void CMSPeak::TruncatePlus1(void)
{
    int PercentBelowVal = PercentBelow();
    if(IsPlus1(PercentBelowVal)) {
        Num[MSORIGINAL] = PercentBelowVal;
        Charge = 1;
    }
    else if(Charge == 1) Charge = 2;
}

// functions used in SmartCull
// take out original peaks below a threshold.  assumes original peaks sorted by intensity
void CMSPeak::CullBaseLine(double Threshold, CMZI *Temp, int& TempLen)
{
    unsigned iMZI;
    for(iMZI = 0; iMZI < Num[MSORIGINAL] && MZI[MSORIGINAL][iMZI].Intensity > Threshold * MZI[MSORIGINAL][0].Intensity; iMZI++);
    copy(&MZI[MSORIGINAL][0], &MZI[MSORIGINAL][iMZI], Temp);
    TempLen = iMZI;
}


// cull precursors
void CMSPeak::CullPrecursor(CMZI *Temp, int& TempLen, double Precursor1, double Precursor2)
{
    // chop out precursors
    int iTemp(0), iMZI;
    
    for(iMZI = 0; iMZI < TempLen; iMZI++) { 
	if((Temp[iMZI].MZ > Precursor1 - tol && 
	    Temp[iMZI].MZ < Precursor1 + tol) ||   
	   (Temp[iMZI].MZ > Precursor2 - tol && 
	    Temp[iMZI].MZ < Precursor2 + tol)) continue;
	
	Temp[iTemp] = Temp[iMZI];
	iTemp++;
    }
    TempLen = iTemp;
}

// iterate thru peaks, deleting ones that pass the test
void CMSPeak::CullIterate(CMZI *Temp, int& TempLen, TMZIbool FCN)
{
    if(!FCN) return;
    int iTemp(0), iMZI, jMZI;
    set <int> Deleted;
    
    // scan for isotopes, starting at highest peak
    for(iMZI = 0; iMZI < TempLen - 1; iMZI++) { 
	if(Deleted.count(iMZI) != 0) continue;
	for(jMZI = iMZI + 1; jMZI < TempLen; jMZI++) { 
	    if(Deleted.count(jMZI) != 0) continue;
	    if((*FCN)(Temp[iMZI], Temp[jMZI], tol)) {
		Deleted.insert(jMZI);
		//		Temp[iMZI].Intensity += Temp[jMZI].Intensity; // keep the intensity
	    }
	}
    }
    
    for(iMZI = 0; iMZI < TempLen; iMZI++) {
	if(Deleted.count(iMZI) == 0) {
	    Temp[iTemp] = Temp[iMZI];
	    iTemp++;
	}
    }
    TempLen = iTemp;
}

// object for looking for isotopes.  true if isotope
static bool FCompareIsotope(const CMZI& x, const CMZI& y, int tol)
{
    if(y.MZ < x.MZ + 2*MSSCALE + tol && y.MZ > x.MZ - tol) return true;
    return false;
}

// cull isotopes using the Markey Method

void CMSPeak::CullIsotope(CMZI *Temp, int& TempLen)
{
    CullIterate(Temp, TempLen, &FCompareIsotope);
}


// function for looking for H2O or NH3.  true if is.
static bool FCompareH2ONH3(const CMZI& x, const CMZI& y, int tol)
{
    if((y.MZ > x.MZ - 18*MSSCALE - tol && y.MZ < x.MZ - 18*MSSCALE + tol ) ||
       (y.MZ > x.MZ - 17*MSSCALE - tol && y.MZ < x.MZ - 17*MSSCALE + tol))
	return true;
    return false;
}

// cull peaks that are water or ammonia loss
// note that this only culls the water or ammonia loss if these peaks have a lesser
// less intensity
void CMSPeak::CullH20NH3(CMZI *Temp, int& TempLen)
{
    // need to start at top of m/z ladder, work way down.  sort
    CullIterate(Temp, TempLen, &FCompareH2ONH3);
}


// use smartcull on all charges
void CMSPeak::CullAll(double Threshold, int SingleWindow,
		      int DoubleWindow)
{    
    int iCharges;
    for(iCharges = 0; iCharges < GetNumCharges(); iCharges++)
	SmartCull(Threshold, GetCharges()[iCharges], SingleWindow,
		  DoubleWindow);
}

// check to see if TestMZ is Diff away from BigMZ
bool CMSPeak::IsAtMZ(int BigMZ, int TestMZ, int Diff, int tol)
{
    if(TestMZ < BigMZ - Diff*MSSCALE + tol && 
       TestMZ > BigMZ - Diff*MSSCALE - tol)
	return true;
    return false;
}

// see if TestMZ can be associated with BigMZ, e.g. water loss, etc.
bool CMSPeak::IsMajorPeak(int BigMZ, int TestMZ, int tol)
{
    if (IsAtMZ(BigMZ, TestMZ, 1, tol) || 
	IsAtMZ(BigMZ, TestMZ, 16, tol) ||
	IsAtMZ(BigMZ, TestMZ, 17, tol) ||
	IsAtMZ(BigMZ, TestMZ, 18, tol)) return true;
    return false;
}

// recursively culls the peaks
void CMSPeak::SmartCull(double Threshold, int Charge, int SingleWindow,
			int DoubleWindow)
{
    sort(MZI[MSORIGINAL], MZI[MSORIGINAL] + Num[MSORIGINAL], CMZICompare());
    sort(MZI[MSORIGINAL], MZI[MSORIGINAL] + Num[MSORIGINAL], CMZICompareIntensity());
    Sorted[MSORIGINAL] = false;
    double Precursor1, Precursor2;
    int Which;

    if(Charge == 1) {
	Precursor1 = GetMass();
	Precursor2 = Precursor1;
	Which = MSCULLED1;
    }
    else if (Charge == 2) {
	Precursor1 = GetMass()/2.0L;
	Precursor2 = Precursor1;
	Which = MSCULLED1;
    }
    else {
	Precursor1 = GetMass()/3.0L;
	Precursor2 = Precursor1;
	Which = MSCULLED2;
    }

    int iMZI = 0;  // starting point

    int TempLen(0);
    CMZI *Temp(new CMZI [Num[MSORIGINAL]]); // temporary holder

    // prep the data
    CullBaseLine(Threshold, Temp, TempLen);
#ifdef DEBUG_PEAKS1
    {
	sort(Temp, Temp+TempLen , CMZICompare());
	ofstream FileOut("afterbase.dta");
	xWrite(FileOut, Temp, TempLen);
	sort(Temp, Temp+TempLen , CMZICompareIntensity());
    }
#endif
    CullPrecursor(Temp, TempLen, Precursor1, Precursor2);
#ifdef DEBUG_PEAKS1
    {
	sort(Temp, Temp+TempLen , CMZICompare());
	ofstream FileOut("afterprecurse.dta");
	xWrite(FileOut, Temp, TempLen);
	sort(Temp, Temp+TempLen , CMZICompareIntensity());
    }
#endif
    CullIsotope(Temp, TempLen);
#ifdef DEBUG_PEAKS1
    {
	sort(Temp, Temp+TempLen , CMZICompare());
	ofstream FileOut("afteriso.dta");
	xWrite(FileOut, Temp, TempLen);
	sort(Temp, Temp+TempLen , CMZICompareIntensity());
    }
#endif
    // h20 and nh3 loss ignored until this cut can be studied
#if 0
    sort(Temp, Temp + TempLen, CMZICompareHigh());
    CullH20NH3(Temp, TempLen);
    sort(Temp, Temp + TempLen, CMZICompareIntensity());
#endif

    // now do the recursion
    int iTemp(0), jMZI;
    set <int> Deleted;
    map <int, int> MajorPeak; // maps a peak to it's "major peak"
    int HitsAllowed; // the number of hits allowed in a window
    int Window; // the m/z range used to limit the number of peaks
    int HitCount;  // number of nearby peaks
   
    // scan for isotopes, starting at highest peak
    for(iMZI = 0; iMZI < TempLen - 1; iMZI++) { 
	if(Deleted.count(iMZI) != 0) continue;
	HitCount = 0;
	if(Charge < 3 || Temp[iMZI].MZ > Precursor1) {
	    // if charge 1 region, allow fewer peaks
	    Window = SingleWindow; //27;
	    HitsAllowed = 2;
	}
	else {
	    // otherwise allow a greater number
	    Window = DoubleWindow; //14;
	    HitsAllowed = 2;
	}
	// go over smaller peaks
	set <int> Considered;
	for(jMZI = iMZI + 1; jMZI < TempLen; jMZI++) { 
	    if(Deleted.count(jMZI) != 0) continue;
	    if(Temp[jMZI].MZ < Temp[iMZI].MZ + Window*MSSCALE + tol &&
	       Temp[jMZI].MZ > Temp[iMZI].MZ - Window*MSSCALE - tol) {
		if(IsMajorPeak(Temp[iMZI].MZ, Temp[jMZI].MZ, tol)) {
		    // link little peak to big peak
		    MajorPeak[jMZI] = iMZI;
		    // ignore for deletion
		    continue;
		}
		// if linked to a major peak, skip
		if(MajorPeak.find(jMZI) != MajorPeak.end()) {
		    if(Considered.find(jMZI) != Considered.end())
			continue;
		    Considered.insert(jMZI);
		    HitCount++;
		    if(HitCount <= HitsAllowed)  continue;
		}
		// if the first peak, skip
		HitCount++;
		if(HitCount <= HitsAllowed)  continue;
		Deleted.insert(jMZI);
	    }
	}
    }
    
    // delete the culled peaks
    for(iMZI = 0; iMZI < TempLen; iMZI++) {
	if(Deleted.count(iMZI) == 0) {
	    Temp[iTemp] = Temp[iMZI];
	    iTemp++;
	}
    }
    TempLen = iTemp;


    // make the array of culled peaks
    if(MZI[Which]) delete [] MZI[Which];
    MZI[Which] = new CMZI [TempLen];
    Num[Which] = TempLen;
    copy(Temp, Temp+TempLen, MZI[Which]);
    Sort(Which);
#ifdef DEBUG_PEAKS1
    {
	ofstream FileOut("aftercull.dta");
	xWrite(FileOut, MZI[Which], Num[Which]);
    }
#endif

    delete [] Temp;
}

// returns the cull array index
int CMSPeak::GetWhich(int Charge)
{
    if(Charge < 3) return MSCULLED1;
    else return MSCULLED2;
}

// return the lowest culled peak and the highest culled peak less than the
// +1 precursor mass
void CMSPeak::HighLow(int& High, int& Low, int& NumPeaks, int PrecursorMass,
		      int Charge, double Threshold)
{
    int Which = GetWhich(Charge);

    if(!Sorted[Which]) Sort(Which);
    if(Num[Which] < 2) {
	High = Low = -1;
	NumPeaks = 0;
	return;
    }

    Low = PrecursorMass;
    High = 0;
    NumPeaks = 0;

    int MaxI = GetMaxI(Which);

    unsigned iMZI;
    for(iMZI = 0; iMZI < Num[Which]; iMZI++) {
	if(MZI[Which][iMZI].Intensity > Threshold*MaxI &&
	   MZI[Which][iMZI].MZ <= PrecursorMass) {
	    if(MZI[Which][iMZI].MZ > High) {
		High = MZI[Which][iMZI].MZ;
	    }
	    if(MZI[Which][iMZI].MZ < Low) {
		Low = MZI[Which][iMZI].MZ;
	    }
	    NumPeaks++;
	}
    }
}

// count the number of putative AA intervals.
// Nodup true means don't count the peaks twice
int CMSPeak::CountAAIntervals(CMassArray& MassArray, bool Nodup, int Which)
{	
    unsigned ipeaks, low;
    int i;
    int PeakCount(0);
    if(!Sorted[Which]) return -1;
    const int *IntMassArray = MassArray.GetIntMass();

    //	list <int> intensity;
    //	int maxintensity = 0;

    for(ipeaks = 0 ; ipeaks < Num[Which] - 1; ipeaks++) {
	//		if(maxintensity < MZI[ipeaks].Intensity) maxintensity = MZI[ipeaks].Intensity;
	low = ipeaks;
	low++;
	//		if(maxintensity < MZI[low].Intensity) maxintensity = MZI[low].Intensity;
	for(; low < Num[Which]; low++) {
	    for(i = 0; i < kNumUniqueAA; i++) {
		if(IntMassArray[i] == 0) continue;  // skip gaps, etc.
		if(MZI[Which][low].MZ- MZI[Which][ipeaks].MZ < IntMassArray[i] + tol/2.0 &&
		   MZI[Which][low].MZ - MZI[Which][ipeaks].MZ > IntMassArray[i] - tol/2.0 ) {	  
		    PeakCount++;
		    //					intensity.push_back(MZI[ipeaks].Intensity);
		    if(Nodup) goto newpeak;
		    else goto oldpeak;
		}
	    }
	oldpeak:
	    i = i;
	}
    newpeak:
	i = i;
    }

    return PeakCount;
}


// return maximum intensity value
int CMSPeak::GetMaxI(int Which)
{
    unsigned Intensity(0), i;
    for(i = 0; i < Num[Which]; i++) {
        if(Intensity < MZI[Which][i].Intensity)
	    Intensity = MZI[Which][i].Intensity;
    }
    return Intensity;
}



/////////////////////////////////////////////////////////////////////////////
//
//  CMSPeakSet::
//
//  Class used to hold sets of CMSPeak and access them quickly
//


CMSPeakSet::~CMSPeakSet() 
{
    while(!PeakSet.empty()) {
	delete *PeakSet.begin();
	PeakSet.pop_front();
    }
    delete [] MassPeak;
}

// compares m/z.  Lower m/z first in sort.
struct CMassPeakCompare {
    bool operator() (TMassPeak x, TMassPeak y)
    {
	if(x.Mass < y.Mass) return true;
	return false;
    }
};

void CMSPeakSet::SortPeaks(void)
{
    int iCharges;
    CMSPeak* Peaks;
    TPeakSet::iterator iPeakSet;
    int CalcMass; // the calculated mass
    TMassPeak temp;

    // first sort
    for(iPeakSet = GetPeaks().begin();
	iPeakSet != GetPeaks().end();
	iPeakSet++ ) {
	Peaks = *iPeakSet;
	// skip empty spectra
	if(Peaks->GetError() == eMSHitError_notenuffpeaks) continue;

	// loop thru possible charges
	for(iCharges = 0; iCharges < Peaks->GetNumCharges(); iCharges++) {
	    // correction for incorrect charge determination.
	    // see 12/13/02 notebook, pg. 135
	    CalcMass = static_cast <int> ((Peaks->GetMass() +
					   Peaks->GetCharge()*kProton*MSSCALE) * 
					  Peaks->GetCharges()[iCharges]/(double)(Peaks->GetCharge()) - 
					  Peaks->GetCharges()[iCharges]*kProton*MSSCALE);
	    temp.Mass = CalcMass;
	    temp.Charge = Peaks->GetCharges()[iCharges];
	    temp.Peak = Peaks;
	    MassMap.insert(pair <const int, TMassPeak>(CalcMass, temp)); 
	}
    } 

    // then create static array

    ArraySize = MassMap.size();
    MassPeak = new TMassPeak[ArraySize];

    TMassPeakMap::iterator iMassMap;
    int i(0);
    for(iMassMap = MassMap.begin(); iMassMap != MassMap.end(); iMassMap++, i++) {
	MassPeak[i].Mass = iMassMap->second.Mass;
	MassPeak[i].Peak = iMassMap->second.Peak;
	MassPeak[i].Charge = iMassMap->second.Charge;
    }

    MassMap.clear();

}


// Get the first index into the sorted array where the mass
// is >= the given mass.  Remember to subtract the tolerance and
// check for out of bounds
TMassPeak *CMSPeakSet::GetIndexLo(int Mass)
{
    TMassPeak temp;
    TMassPeak *retval;
    temp.Mass = Mass;
    retval = lower_bound(MassPeak, MassPeak + ArraySize, temp, 
			 CMassPeakCompare());
    return retval;
}

// get peak for sorted list by index into list
CMSPeak *CMSPeakSet::GetPeak(int Index)
{
    if(Index < 0 || Index >= ArraySize) return 0;
    return MassPeak[Index].Peak;
}
