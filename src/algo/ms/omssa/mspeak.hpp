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
 *    code to deal with spectra and m/z ladders
 *
 * ===========================================================================
 */

#ifndef MSPEAK__HPP
#define MSPEAK__HPP

#include <corelib/ncbimisc.hpp>
#include <objects/omssa/omssa__.hpp>

#include <set>
#include <iostream>
#include <vector>
#include <deque>
#include <map>
#include <string.h>

#include "msms.hpp"
#include "msladder.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)


/////////////////////////////////////////////////////////////////////////////
//
//  CMSHit::
//
//  Used by CMSPeak class to hold hits
//

// forward declaration needed for CMSHitInfo
class CMSPeak;

// class for recording ion peak type
class NCBI_XOMSSA_EXPORT CMSHitInfo {
public:
    char& SetCharge(void) { return Charge; }
    const char GetCharge(void) { return Charge; }
    char& SetIon(void) { return Ion; }
    const char GetIon(void) { return Ion; }
    short int& SetNumber(void) { return Number; }
    const short  GetNumber(void) { return Number; }
    short int& SetMod(void) { return Mod; }
    const short GetMod(void) { return Mod; }
    unsigned& SetIntensity(void) { return Intensity; }
    const unsigned GetIntensity(void) { return Intensity; }
    
private:
    char Charge, Ion;
    short Number, Mod;
    unsigned Intensity;
};

// typedef for holding hit information
typedef AutoPtr <CMSHitInfo, ArrayDeleter<CMSHitInfo> > THitInfo;

// class to contain preliminary hits.  memory footprint must be kept small.
class CMSHit {
public:
    // tor's
    CMSHit(void);
    CMSHit(int StartIn, int StopIn, int IndexIn);
    CMSHit(int StartIn, int StopIn, int IndexIn, int MassIn, int HitsIn,
	   int ChargeIn);
    ~CMSHit();

    // getter-setters
    int GetStart(void);
    void SetStart(int StartIn);
    int GetStop(void);
    void SetStop(int StopIn);
    int GetSeqIndex(void);
    void SetSeqIndex(int IndexIn);
    int GetMass(void);
    void SetMass(int MassIn);
    int GetHits(void);
    void SetHits(int HitsIn);
    int GetCharge(void);
    void SetCharge(int ChargeIn);
    CMSHitInfo& SetHitInfo(int n);

    // return number of hits above threshold
    int GetHits(double Threshold, int MaxI);

    // make a record of the ions hit
    void RecordMatches(CLadder& BLadder, CLadder& YLadder, CLadder& B2Ladder,
		       CLadder& Y2Ladder, CMSPeak *Peaks);

    // copy operator
    CMSHit& operator= (CMSHit& in);

protected:

    // helper function for RecordMatches
    void RecordMatchesScan(CLadder& Ladder, int& iHitInfo, CMSPeak *Peaks,
			   int Which);

private:
    int Start, Stop;
    int Index, Mass;
    int Hits;  // number of peaks hit
    int Charge;  // the charge of the hit
    THitInfo HitInfo;
};


/////////////////// CMSHit inline methods

inline CMSHit::CMSHit(void): Hits(0)
{}

inline CMSHit::CMSHit(int StartIn, int StopIn, int IndexIn):
    Start(StartIn), Stop(StopIn), Index(IndexIn), Hits(0)
{}

inline CMSHit::CMSHit(int StartIn, int StopIn, int IndexIn, int MassIn, int HitsIn,
		      int ChargeIn):
    Start(StartIn), Stop(StopIn), Index(IndexIn), Mass(MassIn),
    Hits(HitsIn), Charge(ChargeIn)
{}

inline CMSHit::~CMSHit() 
{ 
    //    delete [] HitInfo; 
}

inline int CMSHit::GetStart(void) 
{ 
    return Start;
}

inline void CMSHit::SetStart(int StartIn) 
{ 
    Start = StartIn;
}

inline int CMSHit::GetStop(void) 
{ 
    return Stop; 
}

inline void CMSHit::SetStop(int StopIn) 
{ 
    Stop = StopIn; 
}

inline int CMSHit::GetSeqIndex(void) 
{ 
    return Index; 
}

inline void CMSHit::SetSeqIndex(int IndexIn) 
{ 
    Index = IndexIn; 
}

inline int CMSHit::GetMass(void) 
{ 
    return Mass; 
}

inline void CMSHit::SetMass(int MassIn) 
{ 
    Mass = MassIn; 
}

inline int CMSHit::GetHits(void) 
{
    return Hits;
}

inline void CMSHit::SetHits(int HitsIn) 
{ 
    Hits = HitsIn;
}

inline int CMSHit::GetCharge(void)
{
    return Charge;
}

inline void CMSHit::SetCharge(int ChargeIn)
{
    Charge = ChargeIn;
}

inline CMSHitInfo& CMSHit::SetHitInfo(int n)
{
    return *(HitInfo.get() + n);
}

inline CMSHit& CMSHit::operator= (CMSHit& in) 
{ 
    Start = in.Start; 
    Stop = in.Stop;
    Index = in.Index; 
    Mass = in.Mass;
    Hits = in.Hits;
    Charge = in.Charge;
    HitInfo.reset();
    if(in.HitInfo) {
	HitInfo.reset(new CMSHitInfo[Hits]);
	int i;
	for(i = 0; i < Hits; i++) SetHitInfo(i) = in.SetHitInfo(i);
    }
    return *this;
}

/////////////////// end of CMSHit inline methods



/////////////////////////////////////////////////////////////////////////////
//
//  CMZI::
//
//  Used by CMSPeak class to spectral data
//

// a class for holding an m/z value and intensity
class NCBI_XOMSSA_EXPORT CMZI {
public:
    int MZ;
    unsigned Intensity;
    CMZI(void);
    CMZI(int MZIn, unsigned IntensityIn);
    CMZI(double MZIn, double IntensityIn);
};

///////////////////  CMZI inline methods

inline CMZI::CMZI(void) 
{}

inline CMZI::CMZI(int MZIn, unsigned IntensityIn): MZ(MZIn), Intensity(IntensityIn) 
{}

inline CMZI::CMZI(double MZIn, double IntensityIn)
{
    MZ = static_cast <int> (MZIn * MSSCALE);
    Intensity = static_cast <unsigned> (IntensityIn);
}

/////////////////// end of CMZI inline methods


/////////////////////////////////////////////////////////////////////////////
//
//  CMSPeak::
//
//  Class used to hold spectra and convert to mass ladders
//


// for containing hits in mspeak class
// first index is charge
typedef CMSHit * TMSHitList;

// min number of peaks to be considered a hit
#define MSHITMIN 6

// min number of peaks to consider a spectra
// two is absolute minimum in order to get m/z range
#define MSPEAKMIN 5

// size of histogram bin in Daltons
#define MSBIN 100

// culled for single charges
#define MSCULLED1 1
// culled for double charges
#define MSCULLED2 2
// original data
#define MSORIGINAL 0
// only the few most intense peaks
#define MSTOPHITS 3
// number of above cull states
#define MSNUMDATA 4

// the number of top hits to retain -- will be replaced by dynamic value
#define MSNUMTOP 3

// the maximum charge state that can be considered
#define MSMAXCHARGE 4

// the precursor charge state at which to begin considering 2+ product ions
const int kConsiderMult = 3;

// function object for cull iterate
typedef bool (*TMZIbool) (const CMZI&, const CMZI&, int tol);

enum EChargeState {
    eChargeUnknown, // charge has not been computed
    eCharge1,
    eChargeNot1,  // charge is not +1, but one of +2, +3 ...
    eCharge2,
    eCharge3,
    eCharge4,
    eCharge5 };

// typedef for holding intensity
typedef AutoPtr <unsigned, ArrayDeleter<unsigned> > TIntensity;


// class to hold experimental data and manipulate

class NCBI_XOMSSA_EXPORT CMSPeak {
public:
    CMSPeak(void);
    CMSPeak(int HitListSize);
private:
    // c'tor helper
    void xCMSPeak(void);
    // writes out dta format
    void xWrite(std::ostream& FileOut, CMZI *Temp, int Num);

public:
    ~CMSPeak(void);
	
    void AddTotalMass(int massin, int tolin);
    void Sort(int Which = MSORIGINAL);
	
    // Read a spectrum set into a CMSPeak
    int Read(CMSSpectrum& Spectrum, double MSMSTolerance);
    // Write out a CMSPeak in dta format (useful for debugging)
    enum EFileType { eDTA, eASC, ePKL, ePKS, eSCIEX, eUnknown };
    void Write(std::ostream& FileOut, EFileType FileType = eDTA, int Which = MSORIGINAL);

    // functions used in SmartCull
    // iterate thru peaks, deleting ones that pass the test
    void CullIterate(CMZI *Temp, int& TempLen, TMZIbool FCN);
    // cull precursors
    void CullPrecursor(CMZI *Temp, int& TempLen, double Precursor);
    // take out peaks below a threshold
    void CullBaseLine(double Threshold, CMZI *Temp, int& TempLen);
    // cull isotopes using the Markey Method
    void CullIsotope(CMZI *Temp, int& TempLen);
    // cull peaks that are water or ammonia loss
    // note that this only culls the water or ammonia loss if these peaks have a lesser
    // less intensity
    void CullH20NH3(CMZI *Temp, int& TempLen);
    // recursively culls the peaks
    void SmartCull(double Threshold, int Charge,
		   int SingleWindow,  // size of the charge 1 window in Da
		   int DoubleWindow,  // size of the charge 2 window in Da
		   int SingleNum,     // number of peaks allowed in charge 1 window
		   int DoubleNum);    // number of peaks allowed in charge 2 window
    // use smartcull on all charges
    void CullAll(double Threshold, 
		 int SingleWindow,
		 int DoubleWindow,
		 int SingleNum,
		 int DoubleNum);


    // return the lowest culled peak and the highest culled peak less than the
    // precursor mass passed in
    void HighLow(int& High, int& Low, int& NumPeaks, int PrecursorMass, int Charge,
		 double Threshold,
		 int& NumLo,  // number of peak below mh/2
		 int& NumHi   // number of peaks above mh/2 and below mh
		 );

    // count number of AA intervals in spectum.
    int CountAAIntervals(CMassArray& MassArray, bool Nodup=true, int Which = MSCULLED1);
	
    // counts the number of peaks above % of maximum peak
    int AboveThresh(double Threshold, int Which = MSORIGINAL);
	
    // the number of peaks at and below the precursor ion
    int PercentBelow(void);
    //  return the number of peaks in a range. range is in fractions of MH
    int CountRange(double StartFraction, double StopFraction);
    // takes the ratio, low/high, of two ranges in the spectrum
    double RangeRatio(double Start, double Middle, double Stop);

    // various charge functions, some depreciated
    
    void SetPlusOne(double PlusIn);
    // is the data charge +1?
    bool IsPlus1(double PercentBelowIn);
    // calculates charge based on threshold and sets charge value 
    void SetComputedCharge(int MaxCharge);
    EChargeState GetComputedCharge(void);
    // return allowed computed charges
    int* GetCharges(void) { return Charges;}
    // return number of allowed computed charges
    int GetNumCharges(void) { return NumCharges; }
    // Truncate the at the precursor if plus one and set charge to 1
    // if charge is erronously set to 1, set it to 2
    void TruncatePlus1(void);
    
    unsigned GetNum(int Which = MSORIGINAL);
    CMZI *GetMZI(int Which = MSORIGINAL);

    int Compare(CLadder& Ladder, int Which = MSCULLED1);
    bool Contains(int value, int Which);
    bool ContainsFast(int value, int Which);
    // compares only the top hits
    bool CompareTop(CLadder& Ladder);
    int GetMaxI(int Which = MSORIGINAL);
    // returns the cull array index
    int GetWhich(int Charge);


    // compare assuming all lists are sorted
    // Intensity is optional argument that allows recording of the intensity
    int CompareSorted(CLadder& Ladder, int Which, TIntensity * Intensity);


    // initializes arrays used to track hits
    void CMSPeak::InitHitList(void);
    TMSHitList& GetHitList(int Index);
    int GetHitListIndex(int Index);
    // add hit to hitlist.  returns true and the added hit if successful
    bool AddHit(CMSHit& in, CMSHit*& out);


    // keep track of the number of peptides examine for each charge state
    int GetPeptidesExamined(int ChargeIn);
    int& SetPeptidesExamined(int ChargeIn);


    // getter-setters
    int GetMass(void);
    // get charge that came from input file
    int GetCharge(void);
    EMSHitError GetError(void);
    void SetError(EMSHitError ErrorIn);
    string& SetName(void);
    const string& GetName(void) const;
    int& SetNumber(void);
    const int GetNumber(void) const;
    // set the mass tolerance.  input in Daltons.
    void SetTolerance(double tolin);
    int GetTol(void);
    char *SetUsed(int Which);

    // clear used arrays for one cull type
    void ClearUsed(int Which);
    // clear used arrays for all cull types
    void ClearUsedAll(void);

    // functions for testing if peaks are h2o or nh3 losses
    // check to see if TestMZ is Diff away from BigMZ
    bool IsAtMZ(int BigMZ, int TestMZ, int Diff, int tol);
    // see if TestMZ can be associated with BigMZ, e.g. water loss, etc.
    bool IsMajorPeak(int BigMZ, int TestMZ, int tol);

private:
    CMZI *MZI[MSNUMDATA]; // m/z values and intensities, sorted by m/z.  first is original, second is culled
    char *Used[MSNUMDATA];  // used to mark m/z values as used in a match
    unsigned Num[MSNUMDATA]; // number of CMZI.  first is original, second is culled
    bool Sorted[MSNUMDATA]; // have the CMZI been sorted?
    bool *Match;    // is a peak matched or not?
    CMZI ** IntensitySort;  // points to CMZI original, sorted.
    int TotalMass;  // singly protonated m/z
    int Charge;    // Charge from input file
    int Charges[MSMAXCHARGE];  // Computed allowed charges
    int NumCharges;  // array size of Charges[]
    int tol;        // error tolerance of peptide
    double PlusOne;  // value used to determine if spectra is +1
    EChargeState ComputedCharge;  // algorithmically calculated 
    CAA AA;
    char *AAMap;

    string Name;  // name taken from spectrum
    int Number;  // spectrum number taken from spectrum

    // list of hits
    TMSHitList HitList[MSMAXCHARGE];
    int HitListSize;  // max size of hit list
    int HitListIndex[MSMAXCHARGE];  // current size of HitList
    int LastHitNum[MSMAXCHARGE];  // the smallest hit currently in List
    int PeptidesExamined[MSMAXCHARGE];  // the number of peptides examined in search

    EMSHitError Error; // errors that have occurred in processing

};


///////////////////   CMSPeak inline methods

inline void CMSPeak::SetPlusOne(double PlusIn) 
{ 
    PlusOne = PlusIn; 
}

inline EChargeState CMSPeak::GetComputedCharge(void) 
{ 
    return ComputedCharge; 
}

inline unsigned CMSPeak::GetNum(int Which) 
{ 
    return Num[Which];
}

inline CMZI * CMSPeak::GetMZI(int Which) 
{ 
    return MZI[Which];
}


inline TMSHitList& CMSPeak::GetHitList(int Index) 
{ 
    return HitList[Index]; 
}

inline int CMSPeak::GetHitListIndex(int Index) 
{ 
    return HitListIndex[Index]; 
}

// keep track of the number of peptides examine for each charge state
inline int CMSPeak::GetPeptidesExamined(int ChargeIn) 
{ 
    return PeptidesExamined[ChargeIn - Charges[0]];
}

inline int& CMSPeak::SetPeptidesExamined(int ChargeIn) 
{ 
    return PeptidesExamined[ChargeIn - Charges[0]];
}

inline int CMSPeak::GetMass(void) 
{ 
    return TotalMass; 
}

// get charge that came from input file
inline int CMSPeak::GetCharge(void) 
{ 
    return Charge; 
}

inline EMSHitError CMSPeak::GetError(void) 
{
    return Error; 
}

inline void CMSPeak::SetError(EMSHitError ErrorIn) 
{
    Error = ErrorIn; 
}

inline string& CMSPeak::SetName(void) 
{ 
    return Name; 
}

inline const string& CMSPeak::GetName(void) const 
{ 
    return Name; 
}

inline int& CMSPeak::SetNumber(void) 
{ 
    return Number; 
}

inline const int CMSPeak::GetNumber(void) const 
{ 
    return Number; 
}

// set the mass tolerance.  input in Daltons.
inline void CMSPeak::SetTolerance(double tolin)
{
    tol = static_cast <int> (tolin*MSSCALE);
}

inline int CMSPeak::GetTol(void) 
{ 
    return tol; 
}

inline char *CMSPeak::SetUsed(int Which)
{
    return Used[Which];
}

inline void CMSPeak::ClearUsed(int Which)
{
    memset(Used[Which], 0, Num[Which]);
}

inline void CMSPeak::ClearUsedAll(void)
{
    int iCharges;
    for(iCharges = 0; iCharges < GetNumCharges(); iCharges++)
	ClearUsed(GetWhich(GetCharges()[iCharges]));
    ClearUsed(MSTOPHITS);
}

// returns the cull array index
inline int CMSPeak::GetWhich(int Charge)
{
    if(Charge <  kConsiderMult) return MSCULLED1;
    else return MSCULLED2;
}

/////////////////// end of  CMSPeak  inline methods


/////////////////////////////////////////////////////////////////////////////
//
//  CMSPeakSet::
//
//  Class used to hold sets of CMSPeak and access them quickly
//


typedef deque <CMSPeak *> TPeakSet;

typedef struct _MassPeak {
    int Mass, Peptol;
    int Charge;
    CMSPeak *Peak;
} TMassPeak;

typedef AutoPtr <TMassPeak, ArrayDeleter<TMassPeak> > TAPMassPeak;

typedef multimap <int, TMassPeak> TMassPeakMap;

class NCBI_XOMSSA_EXPORT CMSPeakSet {
public:
    // tor's
    CMSPeakSet(void);
    ~CMSPeakSet();

    void AddPeak(CMSPeak *PeakIn);

    // put the pointers into an array sorted by mass
    void SortPeaks(
		   int Peptol  // the precursor mass tolerance
		   );

    int GetArraySize(void);
    // Get the first index into the sorted array where the mass
    // is >= the given mass.  Remember to subtract the tolerance and
    // check for out of bounds

    TMassPeak *GetIndexLo(int Mass);
    // get peak for sorted list by index into list

    CMSPeak *GetPeak(int Index);  
    TMassPeak *GetEndMassPeak(void); 
    // get a particular MassPeak
    TMassPeak& GetMassPeak(int i);
    TPeakSet& GetPeaks(void);

private:
    TPeakSet PeakSet;  // peak list for deletion
    TMassPeakMap MassMap;
    TAPMassPeak MassPeak; // array of neutral masses
    int ArraySize;  // size of above array
};

///////////////////   CMSPeakSet inline methods

inline CMSPeakSet::CMSPeakSet(void)
{}

inline void CMSPeakSet::AddPeak(CMSPeak *PeakIn)
{ 
    PeakSet.push_back(PeakIn); 
}

inline int CMSPeakSet::GetArraySize(void) 
{ 
    return ArraySize; 
}

inline TMassPeak * CMSPeakSet::GetEndMassPeak(void) 
{ 
    return MassPeak.get()+ArraySize; 
}

inline TMassPeak& CMSPeakSet::GetMassPeak(int i) 
{ 
    return *(MassPeak.get()+i); 
}

inline TPeakSet& CMSPeakSet::GetPeaks(void) 
{ 
    return PeakSet; 
}

/////////////////// end of CMSPeakSet inline methods

END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

/*
  $Log$
  Revision 1.14  2004/05/27 20:52:15  lewisg
  better exception checking, use of AutoPtr, command line parsing

  Revision 1.13  2004/04/06 19:53:20  lewisg
  allow adjustment of precursor charges that allow multiply charged product ions

  Revision 1.12  2004/03/30 19:36:59  lewisg
  multiple mod code

  Revision 1.11  2004/03/16 20:18:54  gorelenk
  Changed includes of private headers.

  Revision 1.10  2003/12/22 23:03:18  lewisg
  top hit code and variable mod fixes

  Revision 1.9  2003/12/08 17:37:20  ucko
  #include <string.h> rather than <cstring>, since MIPSpro lacks the latter.

  Revision 1.8  2003/12/05 13:10:32  lewisg
  delete GetUsed

  Revision 1.7  2003/12/04 23:39:08  lewisg
  no-overlap hits and various bugfixes

  Revision 1.6  2003/11/14 20:28:05  lewisg
  scale precursor tolerance by charge

  Revision 1.5  2003/11/10 22:24:12  lewisg
  allow hitlist size to vary

  Revision 1.4  2003/10/24 21:28:41  lewisg
  add omssa, xomssa, omssacl to win32 build, including dll

  Revision 1.3  2003/10/21 21:12:17  lewisg
  reorder headers

  Revision 1.2  2003/10/21 03:43:20  lewisg
  fix double default

  Revision 1.1  2003/10/20 21:32:13  lewisg
  ommsa toolkit version

  Revision 1.20  2003/10/07 18:02:28  lewisg
  prep for toolkit

  Revision 1.19  2003/10/06 18:14:17  lewisg
  threshold vary

  Revision 1.18  2003/08/14 23:49:22  lewisg
  first pass at variable mod

  Revision 1.17  2003/08/06 18:29:11  lewisg
  support for filenames, numbers using regex

  Revision 1.16  2003/07/21 20:25:03  lewisg
  fix missing peak bug

  Revision 1.15  2003/07/19 15:07:38  lewisg
  indexed peaks

  Revision 1.14  2003/07/18 20:50:34  lewisg
  *** empty log message ***

  Revision 1.13  2003/07/17 18:45:50  lewisg
  multi dta support

  Revision 1.12  2003/07/07 16:17:51  lewisg
  new poisson distribution and turn off histogram

  Revision 1.11  2003/04/24 18:45:55  lewisg
  performance enhancements to ladder creation and peak compare

  Revision 1.10  2003/04/18 20:46:52  lewisg
  add graphing to omssa

  Revision 1.9  2003/04/02 18:49:51  lewisg
  improved score, architectural changes

  Revision 1.8  2003/03/21 21:14:40  lewisg
  merge ming's code, other stuff

  Revision 1.7  2003/02/07 16:18:23  lewisg
  bugfixes for perf and to work on exp data

  Revision 1.6  2003/02/03 20:39:02  lewisg
  omssa cgi

  Revision 1.5  2003/01/21 22:19:26  lewisg
  get rid of extra 2 aa function

  Revision 1.4  2003/01/21 21:46:12  lewisg
  *** empty log message ***

  Revision 1.3  2002/11/27 00:07:52  lewisg
  fix conflicts

  Revision 1.2  2002/11/26 00:53:34  lewisg
  sync versions

  Revision 1.1  2002/07/16 13:27:09  lewisg
  *** empty log message ***

  
	
*/
