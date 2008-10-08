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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <algorithm>
#include <vector>
#include <math.h>

#include "mspeak.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  CMSHit::
//
//  Used by CMSPeak class to hold hits
//


void CMSHit::RecordMatchesScan(CLadder& Ladder,
                               int& iHitInfo,
                               CMSPeak *Peaks,
                               EMSPeakListTypes Which,
                               int NOffset,
                               int COffset)
{
    try {
        SetSum() += Ladder.GetSum();
        SetM() += Ladder.GetM();
        
        // examine hits array
        int i;
        for(i = 0; i < Ladder.size(); i++) {
            // if hit, add to hitlist
            if(Ladder.GetHit()[i] > 0) {
                SetHitInfo(iHitInfo).SetCharge() = (char) Ladder.GetCharge();
                SetHitInfo(iHitInfo).SetIonSeries() = (char) Ladder.GetType();
//                if(kIonDirection[Ladder.GetType()] == 1) SetHitInfo(iHitInfo).SetNumber() = (short) i + NOffset;
//                else SetHitInfo(iHitInfo).SetNumber() = (short) i + COffset;
                SetHitInfo(iHitInfo).SetNumber() = Ladder.GetLadderNumber()[i];
                SetHitInfo(iHitInfo).SetIntensity() = Ladder.GetIntensity()[i];
                //  for poisson test
                SetHitInfo(iHitInfo).SetMZ() = Ladder[i];
                SetHitInfo(iHitInfo).SetDelta() = Ladder.GetDelta()[i];
                //
                iHitInfo++;
            }
        }
    } catch (NCBI_NS_STD::exception& e) {
	ERR_POST(Info << "Exception caught in CMSHit::RecordMatchesScan: " << e.what());
	throw;
    }

}


///
///  Count Modifications in Mask
///

int CMSHit::CountMods(unsigned ModMask, int NumMod)
{
	int i, retval(0);
	for(i = 0; i < NumMod; i++)
		if(ModMask & (1 << i)) retval++;
	return retval;
}


/**
 * Record the modifications used in the hit
 * Note that fixed aa modifications are *not* recorded
 * as these are dealt with by modifying the aa mass
 * and the positions are not recorded anywhere
 */

void CMSHit::RecordModInfo(unsigned ModMask,
						   CMod ModList[],
						   int NumMod,
						   const char *PepStart
						   )
{
	int i, j(0);
	for(i = 0; i < NumMod; i++) {
		if(ModMask & (1 << i)) {
			SetModInfo(j).SetModEnum() = ModList[i].GetEnum();
			SetModInfo(j).SetSite() = ModList[i].GetSite() - PepStart;
            SetModInfo(j).SetIsFixed() = ModList[i].GetFixed();
			j++;
		}
	}
}

void CMSHit::RecordMatches(CLadderContainer& LadderContainer,
                           int iMod,
						   CMSPeak *Peaks,
						   unsigned ModMask,
						   CMod ModList[],
						   int NumMod,
						   const char *PepStart,
                           int Searchctermproduct,
                           int Searchb1,
                           int TheoreticalMassIn
						   )
{
    // create hitlist.  note that this is deleted in the assignment operator
    CreateHitInfo();


	// need to calculate the number of mods from mask and NumMod(?)
	NumModInfo = CountMods(ModMask,NumMod);
	ModInfo.reset(new CMSModInfo[NumModInfo]);

    SetTheoreticalMass() = TheoreticalMassIn;

    // increment thru hithist
    int iHitInfo(0); 
    EMSPeakListTypes Which = Peaks->GetWhich(GetCharge());

    SetM() = 0;
    SetSum() = 0;
    // scan thru each ladder

    int ChargeLimit(0);
    if (GetCharge() < Peaks->GetConsiderMult()) 
        ChargeLimit = 1;
    TLadderMap::iterator Iter;
    LadderContainer.Begin(Iter, ChargeLimit, ChargeLimit);
    while(Iter != LadderContainer.SetLadderMap().end()) {
        RecordMatchesScan(*((*(Iter->second))[iMod]),iHitInfo, Peaks, Which, Searchb1, Searchctermproduct);
        LadderContainer.Next(Iter, ChargeLimit, ChargeLimit);
    }
    SetN() = (Peaks->GetPeakLists())[Which]->GetNum();

	// need to make function to save the info in ModInfo
	RecordModInfo(ModMask,
				  ModList,
				  NumMod,
				  PepStart
				  );
}

// return number of hits above threshold
int CMSHit::CountHits(double Threshold, int MaxI)
{
    int i, retval(0);

    for(i = 0; i < GetHits(); i++)
        if(SetHitInfo(i).GetIntensity() > MaxI*Threshold)
            retval++;
    return retval;
}


void 
CMSHit::CountHitsByType(int& Independent,
                        int& Dependent, 
                        double Threshold, 
                        int MaxI) const
{
	int i;
    Independent = Dependent = 0;
    int LastIon(-1), LastCharge(-1), LastNumber(-1);
	for (i = 0; i < GetHits(); i++) {
        if(GetHitInfo(i).GetIntensity() > MaxI*Threshold) {
            if(GetHitInfo(i).GetIonSeries() == LastIon && GetHitInfo(i).GetCharge() == LastCharge) {
                if(LastNumber + 1 == GetHitInfo(i).GetNumber())
                    Dependent++;
                else 
                    Independent++;
            }
            else
                Independent++;
            LastIon = GetHitInfo(i).GetIonSeries();
            LastCharge = GetHitInfo(i).GetCharge();
            LastNumber = GetHitInfo(i).GetNumber();
        }
    }
}


  // for poisson test
// return number of hits above threshold scaled by m/z positions
int CMSHit::CountHits(double Threshold, int MaxI, int High)
{
    int i ;
    float retval(0);

    for(i = 0; i < GetHits(); i++)
      if(SetHitInfo(i).GetIntensity() > MaxI*Threshold) {
	if (SetHitInfo(i).GetMZ() > High/2)
	  retval += 0.5 + 2.0*(High - SetHitInfo(i).GetMZ())/(float)High;
	else
	  retval += 1.5 - 2.0*SetHitInfo(i).GetMZ()/(float)High;
      }
    return (int)(retval+0.5);
}
  //

/////////////////////////////////////////////////////////////////////////////
//
//  Comparison functions used in CMSPeak for sorting
//

// compares m/z.  Lower m/z first in sort.
struct CMZICompare {
    bool operator() (CMZI x, CMZI y)
    {
	if(x.GetMZ() < y.GetMZ()) return true;
	return false;
    }
};

// compares m/z.  High m/z first in sort.
struct CMZICompareHigh {
    bool operator() (CMZI x, CMZI y)
    {
	if(x.GetMZ() > y.GetMZ()) return true;
	return false;
    }
};

// compares intensity.  Higher intensities first in sort.
struct CMZICompareIntensity {
    bool operator() (CMZI x, CMZI y)
    {
	if(x.GetIntensity() > y.GetIntensity()) return true;
	return false;
    }
};



CMSPeakList::CMSPeakList(void):
Num(0), Sorted(eMSPeakListSortNone)
{
}

void CMSPeakList::CreateLists(int Size)
{
    SetSorted() = eMSPeakListSortNone;
    SetNum() = Size;
    SetMZI(new CMZI[Size]);
}


void CMSPeakList::Sort(EMSPeakListSort SortType)
{
    if(SortType == eMSPeakListSortMZ &&
        GetSorted() != eMSPeakListSortMZ) {
        sort(GetMZI(),GetMZI() + GetNum(), CMZICompare());
        SetSorted() = eMSPeakListSortMZ;
    }
    else if(SortType == eMSPeakListSortIntensity &&
              GetSorted() != eMSPeakListSortIntensity) {
        sort(GetMZI(),GetMZI() + GetNum(), CMZICompareIntensity());
        SetSorted() = eMSPeakListSortIntensity;
    }
}


void CMSPeakList::Rank(void)
{
    int i;
    for(i = 1; i <= GetNum(); i++)
        GetMZI()[i-1].SetRank() = i;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CMSPeak::
//
//  Class used to hold spectra and convert to mass ladders
//


void CMSPeak::xCMSPeak(void)
{
    int i;

    // initialize data structure that hold the peaks filtered in various ways
    for(i = 0; i < eMSPeakListChargeMax; ++i) {
        CRef < CMSPeakList > NewList(new CMSPeakList);
        SetPeakLists().push_back(NewList);
    }

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
    int iCharges;
    for(iCharges = 0; iCharges < GetNumCharges(); iCharges++)
	delete [] HitList[iCharges];
}


const bool CMSPeak::AddHit(CMSHit& in, CMSHit *& out)
{
    out = 0;
    // initialize index using charge
    int Index = in.GetCharge() - Charges[0];
    // above min number of hits?
    if(in.GetHits() < GetMinhit()) return false;
    // check to see if hitlist is full

    if (HitListIndex[Index] >= HitListSize) {
        // if less or equal hits than recorded min, don't bother
        if (in.GetHits() <= LastHitNum[Index]) return false;
        int i;
        // find the minimum in the hitlist
        for (i = 0; i < HitListSize; i++) {
            if (LastHitNum[Index] == HitList[Index][i].GetHits()) {
                break;
                }
            }
        // keep record of min
//        LastHitNum[Index] = in.GetHits();    
        // replace in list
        HitList[Index][i] = in;
        out =  & (HitList[Index][i]);
        // find min
        int minimum = in.GetHits();
        for (i = 0; i < HitListSize; i++) {
             if (minimum > HitList[Index][i].GetHits()) {
                 minimum = HitList[Index][i].GetHits();
                 }
             }
        LastHitNum[Index] = minimum;
        return true;
        }
    else {
        // hit list not full, add to end of list
        // if first one, record the minhit
        if(HitListIndex[Index] == 0) {
            LastHitNum[Index] = in.GetHits();
        }
        // if less hits than minhits, reset minhits
        else if(in.GetHits() < LastHitNum[Index]) {
            LastHitNum[Index] = in.GetHits();
        }
        // add it to the back
        HitList[Index][HitListIndex[Index]] = in;
        out = & (HitList[Index][HitListIndex[Index]]);
        HitListIndex[Index]++;
        return true;
    }
}


const bool CMSPeak::Contains(const int value, 
                             const EMSPeakListTypes Which) const
{
    CMZI precursor;
    precursor.SetMZ() = value -  tol; // /2;
    CMZI *p = lower_bound(GetPeakLists()[Which]->GetMZI(),
                          GetPeakLists()[Which]->GetMZI() + GetPeakLists()[Which]->GetNum(),
                          precursor, CMZICompare());
    if(p == GetPeakLists()[Which]->GetMZI() + GetPeakLists()[Which]->GetNum()) return false;
    if(p->GetMZ() < value + tol/*/2*/) return true;
    return false;
}

		
const bool CMSPeak::ContainsFast(const int value,
                                 const EMSPeakListTypes Which) const
{

    int x(0), l, r;
    CRef <CMSPeakList> PeakList = GetPeakLists()[Which];

    l = 0;
    r = PeakList->GetNum() - 1;

    while(l <= r) {
        x = (l + r)/2;
        if (PeakList->GetMZI()[x].GetMZ() < value - tol) 
	    l = x + 1;
        else if (PeakList->GetMZI()[x].GetMZ() > value + tol)
	    r = x - 1;
        else return true;
    } 
    
    if (x < PeakList->GetNum() - 1 && 
        PeakList->GetMZI()[x+1].GetMZ() < value + tol &&
        PeakList->GetMZI()[x+1].GetMZ() > value - tol) 
        return true;
    return false;
}


int CMSPeak::CompareSortedRank(CLadder& Ladder,
                               EMSPeakListTypes Which,
                               vector<bool>& usedPeaks)
{

    int i(0), j(0);
    int retval(0);
    CRef <CMSPeakList> PeakList = SetPeakLists()[Which];

    if (Ladder.size() == 0 ||  PeakList->GetNum() == 0) return 0;
    
    Ladder.SetM() = 0;
    Ladder.SetSum() = 0;

    do {
        if (PeakList->GetMZI()[j].GetMZ() < Ladder[i] - tol) {
            j++;
            if (j >= PeakList->GetNum()) break;
            continue;
        } else if (PeakList->GetMZI()[j].GetMZ() > Ladder[i] + tol) {
            i++;
            if (i >= Ladder.size()) break;
            continue;
        } else {
            // avoid double count
            if (!usedPeaks[j]) {
                usedPeaks[j] = true;
                Ladder.GetHit()[i] = Ladder.GetHit()[i] + 1;
                // sum up the ranks
                Ladder.SetSum() += PeakList->GetMZI()[j].GetRank();
                Ladder.SetM()++;
            }
            retval++;
            // record the intensity  for scoring
            Ladder.SetIntensity()[i] = PeakList->GetMZI()[j].GetIntensity();
            Ladder.SetDelta()[i] = PeakList->GetMZI()[j].GetMZ() - Ladder[i];
            j++;
            if (j >= PeakList->GetNum()) break;
            i++;
            if (i >= Ladder.size()) break;
        }
    } while (true);
    return retval;
}


const int CMSPeak::Compare(CLadder& Ladder, const EMSPeakListTypes Which) const
{
    int i;
    int retval(0);
    for(i = 0; i < Ladder.size(); i++) {
	if(ContainsFast(Ladder[i], Which)) {
	    retval++;
	    Ladder.GetHit()[i] = Ladder.GetHit()[i] + 1;
	}
    }
    return retval;
}


const bool CMSPeak::CompareTop(CLadder& Ladder)
{
    int i;
    CRef <CMSPeakList> PeakList = SetPeakLists()[eMSPeakListTop];

    for(i = 0; i < PeakList->GetNum(); i++) {
        if(Ladder.ContainsFast(PeakList->GetMZI()[i].GetMZ(), tol)) return true;
    }
    return false;
}


int CMSPeak::Read(const CMSSpectrum& Spectrum,
                  const CMSSearchSettings& Settings)
{
    try {
        SetTolerance(Settings.GetMsmstol());
        SetPrecursorTol(Settings.GetPeptol());
        //  note that there are two scales -- input scale and computational scale
        //  Precursormz = MSSCALE * (Spectrum.GetPrecursormz()/(double)MSSCALE);  
        // for now, we assume one scale
        Precursormz = Spectrum.GetPrecursormz();  

        const CMSSpectrum::TMz& Mz = Spectrum.GetMz();
        const CMSSpectrum::TAbundance& Abundance = Spectrum.GetAbundance();
        SetPeakLists()[eMSPeakListOriginal]->CreateLists(Mz.size());

        int i;
        for (i = 0; i < SetPeakLists()[eMSPeakListOriginal]->GetNum(); i++) {
            // for now, we assume one scale
            SetPeakLists()[eMSPeakListOriginal]->GetMZI()[i].SetMZ() = Mz[i];
            SetPeakLists()[eMSPeakListOriginal]->GetMZI()[i].SetIntensity() = Abundance[i];
        }
        SetPeakLists()[eMSPeakListOriginal]->Sort(eMSPeakListSortMZ);
    }
    catch (NCBI_NS_STD::exception& e) {
        ERR_POST(Info << "Exception in CMSPeak::Read: " << e.what());
        throw;
    }

    return 0;
}

void 
CMSPeak::ReadAndProcess(const CMSSpectrum& Spectrum,
                        const CMSSearchSettings& Settings)
{
	if(Read(Spectrum, Settings) != 0) {
	    ERR_POST(Error << "omssa: unable to read spectrum into CMSPeak");
	    return;
	}
    SetName().clear();
	if(Spectrum.CanGetIds()) SetName() = Spectrum.GetIds();
	if(Spectrum.CanGetNumber())
	    SetNumber() = Spectrum.GetNumber();
		
	SetPeakLists()[eMSPeakListOriginal]->Sort(eMSPeakListSortMZ);
	SetComputedCharge(Settings.GetChargehandling(), Spectrum);
	InitHitList(Settings.GetMinhit());
	CullAll(Settings);

    // this used to look at the filtered list.  changed to look at original
    // to simplify the code.
	if(GetPeakLists()[eMSPeakListOriginal]->GetNum() < Settings.GetMinspectra())
	    {
	    ERR_POST(Info << "omssa: not enough peaks in spectra");
	    SetError(eMSHitError_notenuffpeaks);
	}

}

const int CMSPeak::CountRange(const double StartFraction,
                              const double StopFraction) const
{
    CMZI Start, Stop;
    int Precursor = static_cast <int> (Precursormz + tol/2.0);
    Start.SetMZ() = static_cast <int> (StartFraction * Precursor);
    Stop.SetMZ() = static_cast <int> (StopFraction * Precursor);
    CMZI *LoHit = lower_bound(GetPeakLists()[eMSPeakListOriginal]->GetMZI(),
       GetPeakLists()[eMSPeakListOriginal]->GetMZI() +
       GetPeakLists()[eMSPeakListOriginal]->GetNum(),
       Start, CMZICompare());
    CMZI *HiHit = upper_bound(
       GetPeakLists()[eMSPeakListOriginal]->GetMZI(),
       GetPeakLists()[eMSPeakListOriginal]->GetMZI() +
       GetPeakLists()[eMSPeakListOriginal]->GetNum(),
       Stop, CMZICompare());

    return HiHit - LoHit;
}

const int CMSPeak::CountMZRange(const int StartIn,
                                const int StopIn,
                                const double MinIntensity,
                                const int Which) const
{
    CMZI Start, Stop;
    Start.SetMZ() = StartIn;
    Stop.SetMZ() = StopIn;
    // inclusive lower
    CMZI *LoHit = lower_bound(
       GetPeakLists()[Which]->GetMZI(),
       GetPeakLists()[Which]->GetMZI() +
       GetPeakLists()[Which]->GetNum(),
       Start, CMZICompare());
    // exclusive upper
    CMZI *HiHit = upper_bound(
       GetPeakLists()[Which]->GetMZI(),
       GetPeakLists()[Which]->GetMZI() +
       GetPeakLists()[Which]->GetNum(),
       Stop, CMZICompare());
    int retval(0);

    if(LoHit != GetPeakLists()[Which]->GetMZI() +
       GetPeakLists()[Which]->GetNum() &&
       LoHit < HiHit) {
       for(; LoHit != HiHit; ++LoHit)
        if(LoHit->GetIntensity() >= MinIntensity ) 
            retval++;
    }

    return retval;
}


const int CMSPeak::PercentBelow(void) const
{
    CMZI precursor;
    precursor.SetMZ() = static_cast <int> (Precursormz + tol/2.0);
    CMZI *Hit = upper_bound(
       GetPeakLists()[eMSPeakListOriginal]->GetMZI(),
       GetPeakLists()[eMSPeakListOriginal]->GetMZI() +
       GetPeakLists()[eMSPeakListOriginal]->GetNum(),
			    precursor, CMZICompare());
    Hit++;  // go above the peak
    if(Hit >= GetPeakLists()[eMSPeakListOriginal]->GetMZI() +
       GetPeakLists()[eMSPeakListOriginal]->GetNum()) 
       return GetPeakLists()[eMSPeakListOriginal]->GetNum();
    return Hit-GetPeakLists()[eMSPeakListOriginal]->GetMZI();
}


const bool CMSPeak::IsPlus1(const double PercentBelowIn) const
{
    if(PercentBelowIn / 
        GetPeakLists()[eMSPeakListOriginal]->GetNum() > PlusOne) return true;
    return false;
}

const double CMSPeak::RangeRatio(const double Start, 
                                 const double Middle, 
                                 const double Stop) const
{
    int Lo = CountRange(Start, Middle);
    int Hi = CountRange(Middle, Stop);
    return (double)Lo/Hi;
}


void CMSPeak::SetComputedCharge(const CMSChargeHandle& ChargeHandle,
                                const CMSSpectrum& Spectrum)
{
	ConsiderMult = min(ChargeHandle.GetConsidermult(), 
            eMSPeakListChargeMax - eMSPeakListCharge1);

    // if the user wants us to believe the file charge state, believe it
    if(ChargeHandle.GetCalccharge() == 1) {
        if(Spectrum.GetCharge().size() == 0)
            ERR_POST(Fatal << "There are no charges specified for spectrum number " << Spectrum.GetNumber());
        MinCharge = *(min_element(Spectrum.GetCharge().begin(), Spectrum.GetCharge().end()));
        MaxCharge = *(max_element(Spectrum.GetCharge().begin(), Spectrum.GetCharge().end()));
    }
    // otherwise search the specified range of charges
    else {
        MinCharge = min(ChargeHandle.GetMincharge(),
                        eMSPeakListChargeMax - eMSPeakListCharge1);
        MaxCharge = min(ChargeHandle.GetMaxcharge(),
                        eMSPeakListChargeMax - eMSPeakListCharge1);
    }
    PlusOne = ChargeHandle.GetPlusone();

    if (ChargeHandle.GetCalccharge() != 1 && 
        ChargeHandle.GetCalcplusone() == eMSCalcPlusOne_calc) {
        if (MinCharge <= 1 && IsPlus1(PercentBelow())) {
            ComputedCharge = eCharge1;
            Charges[0] = 1;
            NumCharges = 1;
        } else {

#if 0
            // ratio search.  doesn't work that well.
            NumCharges = 1;
            if (RangeRatio(0.5, 1.0, 1.5) > 1.25 ) {
                Charges[0] = 2;
                ComputedCharge = eCharge2;
            } else if (RangeRatio(1.0, 1.5, 2.0) > 1.25) {
                Charges[0] = 3;
                ComputedCharge = eCharge3;
            } else {  // assume +4
                Charges[0] = 4;
                ComputedCharge = eCharge4;
            }
#endif

            ComputedCharge = eChargeNot1; 
            int i, NewMinCharge(MinCharge);
            if(MinCharge <= 1) NewMinCharge = 2;
            NumCharges = MaxCharge - NewMinCharge + 1;
            for (i = 0; i < NumCharges; i++) {
                Charges[i] = i + NewMinCharge;
            }
        }
    }
    // don't compute charges
    else {
        ComputedCharge = eChargeUnknown;
        int i;
        NumCharges = MaxCharge - MinCharge + 1;
        for (i = 0; i < NumCharges; i++) {
            Charges[i] = i + MinCharge;
        }
    }
}

// initializes arrays used to track hits
void CMSPeak::InitHitList(const int Minhitin)
{
    int iCharges;
    SetMinhit() = Minhitin;
    for(iCharges = 0; iCharges < GetNumCharges(); iCharges++) {
	LastHitNum[iCharges] = Minhit - 1;
	HitListIndex[iCharges] = 0;
	HitList[iCharges] = new CMSHit[HitListSize];
	PeptidesExamined[iCharges] = 0;
    }
}


void CMSPeak::xWrite(std::ostream& FileOut, const CMZI * const Temp, const int Num) const
{
    FileOut << MSSCALE2DBL(CalcPrecursorMass(1)) + kProton << " " << 1 << endl;

    int i;
    unsigned Intensity;
    for(i = 0; i < Num; i++) {
        Intensity = Temp[i].GetIntensity();
        if(Intensity == 0.0) Intensity = 1;  // make Mas cot happy
        FileOut << MSSCALE2DBL(Temp[i].GetMZ()) << " " << 
	    Intensity << endl;
    }
}


void CMSPeak::Write(std::ostream& FileOut, const EMSSpectrumFileType FileType,
 const EMSPeakListTypes Which) const
{
    if(!FileOut || FileType != eMSSpectrumFileType_dta) return;
    xWrite(FileOut, GetPeakLists()[Which]->GetMZI(),
        GetPeakLists()[Which]->GetNum());
}

const int CMSPeak::AboveThresh(const double Threshold,
 const EMSPeakListTypes Which) const
{
    CMZI *MZISort = new CMZI[GetPeakLists()[Which]->GetNum()];
    int i;
    for(i = 0; i < GetPeakLists()[Which]->GetNum(); i++)
        MZISort[i] = GetPeakLists()[Which]->GetMZI()[i];

    // sort so higher intensities first
    sort(MZISort, MZISort+GetPeakLists()[Which]->GetNum(),
        CMZICompareIntensity());

    for(i = 1; i < GetPeakLists()[Which]->GetNum() &&
	    (double)MZISort[i].GetIntensity()/MZISort[0].GetIntensity() > Threshold; i++);
    delete [] MZISort;
    return i;
}


// functions used in SmartCull

void CMSPeak::CullBaseLine(const double Threshold, CMZI *Temp, int& TempLen)
{
    int iMZI;
    for(iMZI = 0; iMZI < TempLen && Temp[iMZI].GetIntensity() > Threshold * Temp[0].GetIntensity(); iMZI++);
    TempLen = iMZI;
}


void CMSPeak::CullPrecursor(CMZI *Temp,
                            int& TempLen,
                            const int Precursor,
                            const int Charge,
                            bool PrecursorCull)
{
    // chop out precursors
    int iTemp(0), iMZI;
    
    for(iMZI = 0; iMZI < TempLen; iMZI++) { 
        // only -19 : 195,1 205,2
        // -19 to +5: 196,1 205,2
        // everything: 186,1 191,2
        //precursor charge dep with -19, 5: 191, 1 198,2
        // nothing: 196,1 205, 2
        // 5 5: 189 1, 196 2
        if(Temp[iMZI].GetMZ() > Precursor - 5*tol/Charge && 
           Temp[iMZI].GetMZ() < Precursor + 5*tol/Charge) continue;

#if 0
// not as good as dina's clean peaks
    if(Temp[iMZI].GetMZ() > Precursor - 19*tol/Charge && 
          Temp[iMZI].GetMZ() < Precursor + 5*tol/Charge) continue;
    if(Temp[iMZI].GetMZ() > Precursor/2 - 10 *tol/Charge && 
         Temp[iMZI].GetMZ() < Precursor/2 + 3*tol/Charge) continue;
     if(Temp[iMZI].GetMZ() > Precursor/3 - 7*tol/Charge && 
          Temp[iMZI].GetMZ() < Precursor/3 + 2*tol/Charge) continue;
     if(Temp[iMZI].GetMZ() > Precursor/4 - 5*tol/Charge && 
           Temp[iMZI].GetMZ() < Precursor/4 + 1*tol/Charge) continue;
#endif
#if 0
// 196 1, 205 2
    if(Temp[iMZI].GetMZ() > Precursor - 12*tol/Charge && 
         Temp[iMZI].GetMZ() < Precursor + 12*tol/Charge) continue;
#endif
        if(PrecursorCull) {
            // dina's clean peaks.  same results *except* for a high scoring false positive */
            //
            // 1. exclude all masses above (parent mass - 60)
            // 2. exclude all masses from (+2 parent mass + 3) to (+2 parent mass - 30)
            // 3. exclude all masses from (+3 parent mass + 3) to (+3 parent mass - 20)
            // 4. exclude all masses from (+4 parent mass + 3) to (+4 parent mass - 3)
            if(Temp[iMZI].GetMZ() > Precursor - 60*tol) continue;
            if(Temp[iMZI].GetMZ() > Precursor/2 - 30*tol && 
               Temp[iMZI].GetMZ() < Precursor/2 + 3*tol) continue;
            if(Temp[iMZI].GetMZ() > Precursor/3 - 20*tol && 
               Temp[iMZI].GetMZ() < Precursor/3 + 3*tol) continue;
            if(Temp[iMZI].GetMZ() > Precursor/4 - 3*tol && 
               Temp[iMZI].GetMZ() < Precursor/4 + 3*tol) continue;
        }

        Temp[iTemp] = Temp[iMZI];
        iTemp++;
    }
    TempLen = iTemp;
}

// iterate thru peaks, deleting ones that pass the test
void CMSPeak::CullIterate(CMZI *Temp, int& TempLen, const TMZIbool FCN)
{
    if(!FCN) return;
    int iTemp(0), iMZI, jMZI;
    set <int> Deleted;
    
    // scan for isotopes, starting at highest peak
    for(iMZI = 0; iMZI < TempLen - 1; iMZI++) { 
	if(Deleted.find(iMZI) != Deleted.end()) continue;
	for(jMZI = iMZI + 1; jMZI < TempLen; jMZI++) { 
//	    if(Deleted.find(jMZI) != Deleted.end()) continue;  // this is expensive
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
    if(y.GetMZ() < x.GetMZ() + MSSCALE2INT(2) + tol && y.GetMZ() > x.GetMZ() - tol) return true;
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
    if((y.GetMZ() > x.GetMZ() - MSSCALE2INT(18) - tol && y.GetMZ() < x.GetMZ() - MSSCALE2INT(18) + tol ) ||
       (y.GetMZ() > x.GetMZ() - MSSCALE2INT(17) - tol && y.GetMZ() < x.GetMZ() - MSSCALE2INT(17) + tol))
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


void CMSPeak::CullChargeAndWhich(const CMSSearchSettings& Settings)
{
    int iCharges;
	for(iCharges = 0; iCharges < GetNumCharges(); iCharges++){
        int TempLen(0);
        CMZI *Temp = new CMZI [GetPeakLists()[eMSPeakListOriginal]->GetNum()]; // temporary holder
        copy(SetPeakLists()[eMSPeakListOriginal]->GetMZI(), 
             SetPeakLists()[eMSPeakListOriginal]->GetMZI() + 
             GetPeakLists()[eMSPeakListOriginal]->GetNum(), Temp);
        TempLen = GetPeakLists()[eMSPeakListOriginal]->GetNum();
        bool ConsiderMultProduct(false);

		CullPrecursor(Temp,
                      TempLen,
                      CalcPrecursorMass(GetCharges()[iCharges]),
                      GetCharges()[iCharges],
                      Settings.GetPrecursorcull() != 0);

//#define DEBUG_PEAKS1
#ifdef DEBUG_PEAKS1
    {
	sort(Temp, Temp+TempLen , CMZICompare());
	ofstream FileOut("afterprecurse.dta");
	xWrite(FileOut, Temp, TempLen);
	sort(Temp, Temp+TempLen , CMZICompareIntensity());
    }
#endif

        if(GetCharges()[iCharges] >= ConsiderMult) ConsiderMultProduct = true;

    	SmartCull(Settings,
    			  Temp, TempLen, ConsiderMultProduct);
    
        // make the array of culled peaks
    	EMSPeakListTypes Which = GetWhich(GetCharges()[iCharges]);
        SetPeakLists()[Which]->CreateLists(TempLen);
        copy(Temp, Temp+TempLen, SetPeakLists()[Which]->GetMZI());
        SetPeakLists()[Which]->Sort(eMSPeakListSortIntensity);
        SetPeakLists()[Which]->Rank();
        SetPeakLists()[Which]->Sort(eMSPeakListSortMZ);

        delete [] Temp;
	}
}


// use smartcull on all charges
void CMSPeak::CullAll(const CMSSearchSettings& Settings)
{    
    int iCharges;
    SetPeakLists()[eMSPeakListOriginal]->Sort(eMSPeakListSortIntensity);

    CullChargeAndWhich(Settings);

    // make the high intensity list
    // todo: need to look at all of the filtered lists to pick out peaks 
    // found in all filtered list -- otherwise you'll get precursors
    // also, SmartCull has to adjust for charge!
    iCharges = GetNumCharges() - 1;
    int Which = GetWhich(GetCharges()[iCharges]);
    CMZI *Temp = new CMZI [SetPeakLists()[Which]->GetNum()]; // temporary holder
    copy(SetPeakLists()[Which]->GetMZI(), SetPeakLists()[Which]->GetMZI() + 
         SetPeakLists()[Which]->GetNum(), Temp);
    sort(Temp, Temp + SetPeakLists()[Which]->GetNum(), CMZICompareIntensity());

    if(SetPeakLists()[Which]->GetNum() > Settings.GetTophitnum())  
        SetPeakLists()[eMSPeakListTop]->CreateLists(Settings.GetTophitnum());
    else 
        SetPeakLists()[eMSPeakListTop]->CreateLists(SetPeakLists()[Which]->GetNum());

    copy(Temp, Temp + SetPeakLists()[eMSPeakListTop]->GetNum(), 
         SetPeakLists()[eMSPeakListTop]->GetMZI());
    SetPeakLists()[eMSPeakListTop]->Sort(eMSPeakListSortMZ);

    delete [] Temp;
}


const bool CMSPeak::IsAtMZ(const int BigMZ,
                           const int TestMZ, 
                           const int Diff, 
                           const int tolin) const
{
    if(TestMZ < BigMZ - MSSCALE2INT(Diff) + tolin && 
       TestMZ > BigMZ - MSSCALE2INT(Diff) - tolin)
	return true;
    return false;
}


const bool CMSPeak::IsMajorPeak(const int BigMZ,
                                const int TestMZ, 
                                const int tolin) const
{
    if (IsAtMZ(BigMZ, TestMZ, 1, tolin) || 
	IsAtMZ(BigMZ, TestMZ, 16, tolin) ||
	IsAtMZ(BigMZ, TestMZ, 17, tolin) ||
	IsAtMZ(BigMZ, TestMZ, 18, tolin)) return true;
    return false;
}

// recursively culls the peaks
void CMSPeak::SmartCull(const CMSSearchSettings& Settings,
						CMZI *Temp, 
                        int& TempLen, 
                        const bool ConsiderMultProduct)
{
    int iMZI = 0;  // starting point

    // prep the data
    CullBaseLine(Settings.GetCutlo(), Temp, TempLen);
#ifdef DEBUG_PEAKS1
    {
	sort(Temp, Temp+TempLen , CMZICompare());
	ofstream FileOut("afterbase.dta");
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
        if(!ConsiderMultProduct ||
           Temp[iMZI].GetMZ() > 
           Precursormz) {
            // if charge 1 region, allow fewer peaks
            Window = Settings.GetSinglewin(); //27;
            HitsAllowed = Settings.GetSinglenum();
        }
        else {
            // otherwise allow a greater number
            Window = Settings.GetDoublewin(); //14;
            HitsAllowed = Settings.GetDoublenum();
        }
        // go over smaller peaks
        set <int> Considered;
        for(jMZI = iMZI + 1; jMZI < TempLen; jMZI++) { 
            if(Temp[jMZI].GetMZ() < Temp[iMZI].GetMZ() + MSSCALE2INT(Window) + tol &&
               Temp[jMZI].GetMZ() > Temp[iMZI].GetMZ() - MSSCALE2INT(Window) - tol &&
               Deleted.find(jMZI) == Deleted.end()) {
                if(IsMajorPeak(Temp[iMZI].GetMZ(), Temp[jMZI].GetMZ(), tol)) {
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

#ifdef DEBUG_PEAKS1
    {
	sort(Temp, Temp+TempLen , CMZICompare());
	ofstream FileOut("aftercull.dta");
	xWrite(FileOut, Temp, TempLen);
	sort(Temp, Temp+TempLen , CMZICompareIntensity());
    }
#endif

}

// return the lowest culled peak and the highest culled peak less than the
// +1 precursor mass
void CMSPeak::HighLow(int& High, int& Low, int& NumPeaks, const int PrecursorMass,
		      const int Charge, const double Threshold, int& NumLo, int& NumHi)
{
    EMSPeakListTypes Which = GetWhich(Charge);

    SetPeakLists()[Which]->Sort(eMSPeakListSortMZ);
    
    if(GetPeakLists()[Which]->GetNum() < 2) {
	High = Low = -1;
	NumPeaks = NumLo = NumHi = 0;
	return;
    }

    Low = PrecursorMass;
    High = 0;
    NumPeaks = 0;
    NumLo = 0;
    NumHi = 0;

    int MaxI = GetMaxI(Which);

    int iMZI;
    for(iMZI = 0; iMZI < GetPeakLists()[Which]->GetNum(); iMZI++) {
	if(GetPeakLists()[Which]->GetMZI()[iMZI].GetIntensity() > Threshold*MaxI &&
	   GetPeakLists()[Which]->GetMZI()[iMZI].GetMZ() <= PrecursorMass) {
	    if(GetPeakLists()[Which]->GetMZI()[iMZI].GetMZ() > High) {
		High = GetPeakLists()[Which]->GetMZI()[iMZI].GetMZ();
	    }
	    if(GetPeakLists()[Which]->GetMZI()[iMZI].GetMZ() < Low) {
		Low = GetPeakLists()[Which]->GetMZI()[iMZI].GetMZ();
	    }
	    NumPeaks++;
	    if(GetPeakLists()[Which]->GetMZI()[iMZI].GetMZ() < PrecursorMass/2.0) NumLo++;
	    else NumHi++;
	}
    }
}


const int CMSPeak::CountAAIntervals(const CMassArray& MassArray,
                                    const bool Nodup, 
                                    const EMSPeakListTypes Which) const
{	
    int ipeaks, low;
    int i;
    int PeakCount(0);
    if(GetPeakLists()[Which]->GetSorted() != eMSPeakListSortMZ) return -1;
    const int *IntMassArray = MassArray.GetIntMass();

    //	list <int> intensity;
    //	int maxintensity = 0;

    for(ipeaks = 0 ; ipeaks < GetPeakLists()[Which]->GetNum() - 1; ipeaks++) {
	//		if(maxintensity < MZI[ipeaks].Intensity) maxintensity = MZI[ipeaks].Intensity;
	low = ipeaks;
	low++;
	//		if(maxintensity < MZI[low].Intensity) maxintensity = MZI[low].Intensity;
	for(; low < GetPeakLists()[Which]->GetNum(); low++) {
	    for(i = 0; i < kNumUniqueAA; i++) {
		if(IntMassArray[i] == 0) continue;  // skip gaps, etc.
		if(GetPeakLists()[Which]->GetMZI()[low].GetMZ()- GetPeakLists()[Which]->GetMZI()[ipeaks].GetMZ() < IntMassArray[i] + tol/2.0 &&
		   GetPeakLists()[Which]->GetMZI()[low].GetMZ() - GetPeakLists()[Which]->GetMZI()[ipeaks].GetMZ() > IntMassArray[i] - tol/2.0 ) {	  
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


const int CMSPeak::GetMaxI(const EMSPeakListTypes Which) const
{
    unsigned Intensity(0);
    int i;
    for(i = 0; i < GetPeakLists()[Which]->GetNum(); i++) {
        if(Intensity < GetPeakLists()[Which]->GetMZI()[i].GetIntensity())
	    Intensity = GetPeakLists()[Which]->GetMZI()[i].GetIntensity();
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
}


/**
 *  put the pointers into an array sorted by mass
 *
 * @param Peptol the precursor mass tolerance
 * @param Zdep should the tolerance be charge dependent?
 * @return maximum m/z value
 */
int CMSPeakSet::SortPeaks(int Peptol, int Zdep)
{
    int iCharges;
    CMSPeak* Peaks;
    TPeakSet::iterator iPeakSet;
    int CalcMass; // the calculated mass
    TMassPeak *temp;
    int ptol; // charge corrected mass tolerance
    int MaxMZ(0);

    MassIntervals.Clear();

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
    	    ptol = (Zdep * (Peaks->GetCharges()[iCharges] - 1) + 1) * Peptol;
            CalcMass = Peaks->GetPrecursormz() * Peaks->GetCharges()[iCharges] -
                MSSCALE2INT(Peaks->GetCharges()[iCharges]*kProton);
            temp = new TMassPeak;
    	    temp->Mass = CalcMass;
    	    temp->Peptol = ptol;
    	    temp->Charge = Peaks->GetCharges()[iCharges];
    	    temp->Peak = Peaks;
            // save the TMassPeak info
            const CRange<ncbi::TSignedSeqPos> myrange(temp->Mass - temp->Peptol, temp->Mass + temp->Peptol);
            const ncbi::CConstRef<ncbi::CObject> myobject(static_cast <CObject *> (temp));   
            MassIntervals.Insert(myrange,
                                         myobject);
            // keep track of maximum m/z
            if(temp->Mass + temp->Peptol > MaxMZ)
                MaxMZ = temp->Mass + temp->Peptol;
    	}
    } 

    return MaxMZ;
}

