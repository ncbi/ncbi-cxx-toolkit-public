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
 *    Classes to deal with ladders of m/z values
 *
 * ===========================================================================
 */

#ifndef MSLADDER__HPP
#define MSLADDER__HPP

#include <corelib/ncbimisc.hpp>
#include <objects/omssa/omssa__.hpp>

#include <set>
#include <iostream>
#include <vector>

#include "msms.hpp"
#include "omssascore.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)


// container for mass ladders
typedef int THit;

// max size of ladder
const int kMSLadderMax = 10000;

/** container for intensity */
typedef AutoArray <unsigned> TIntensity;

/** container for mass deltas */
typedef AutoArray <int> TDelta;

/** container for ion series number */
typedef AutoArray <TMSNumber> TLadderNumber;

/////////////////////////////////////////////////////////////////////////////
//
//  CLadder::
//
//  Contains a theoretical m/z ladder
//

class NCBI_XOMSSA_EXPORT CLadder : public CObject {
public:

    // 'tor's
    ~CLadder();
    CLadder(void);
    CLadder(int SizeIn);
    CLadder(const CLadder& Old);

    // vector operations on the ladder
    int& operator [] (int n);
    int size(void);
    void clear(void);

    /**
     *  make a ladder
     * 
     * @param IonType the ion series to create
     * @param Charge the charge of the series
     * @param Sequence the protein sequence
     * @param SeqIndex the position in the blast library
     * @param start start position in the sequence
     * @param stop the stop position in the sequence
     * @param MassArray AA masses
     * @param AA used for mass calculation
     * @param ModMask bit mask of modifications to use
     * @param ModList modification information
     * @param NumMod the total number of mods
     * @param Settings search settings
     * @param NoProline do not create ions nterm to prolines
     * 
     * @return false if fails
     */
    const bool CreateLadder(const int IonType,
                            const int Charge,
                            const char * const Sequence,
                            const int SeqIndex,
                            const int start,
                            const int stop,
                            const int mass,
                            const CMassArray& MassArray, 
                            const CAA &AA,
                            const unsigned ModMask,
                            const CMod ModList[],
                            const int NumMod,
                            const CMSSearchSettings& Settings,
                            const bool NoProline
                            );

    /**
     * calculate the mass difference
     *
     * @param IntMassArray amino acid masses
     * @param AAMap convert aa to index
     * @param Sequence the sequence
     * @param Offset start position in the sequence
     * @param Direction the direction of the ion series
     * @param NumMod number of modifications
     * @param ModIndex position of the modifications
     * @param ModList modification info
     * @param ModMask modification mask
     * @param i index into sequence
     * @param ion the ladder
     * @param ChargeIn charge state
     * @param SearchType what type of mass search (exact, ...)
     * @param ExactMass exact mass threshold
     */
    bool CalcDelta(const int *IntMassArray,
                   const char * const AAMap,
                   const char *Sequence,
                   int Offset, 
                   int Direction, 
                   int NumMod, 
                   int &ModIndex,
                   const CMod ModList[],
                   unsigned ModMask,
                   int i,
                   int& ion,
                   const int ChargeIn, 
                   EMSSearchType SearchType,
                   double ExactMass);


    // check if modification mask position is set
    bool MaskSet(unsigned ModMask, int ModIndex);

    // getter setters
    THit * GetHit(void);
    int GetStart(void);
    int GetStop(void);
    int GetSeqIndex(void);
    int GetType(void);
    int GetMass(void);
    int GetCharge(void);

    // sees if ladder contains the given mass value
    bool Contains(int MassIndex, int Tolerance);
    bool ContainsFast(int MassIndex, int Tolerance);

    // or's hitlist with hitlist from other ladder
    // takes into account direction
    void Or(CLadder& LadderIn);

    // count the number of matches
    int HitCount(void);

    // clear the Hitlist
    void ClearHits(void);

    /**
     * Get the intensity array
     */
    const TIntensity& GetIntensity(void) const;

    /**
     * Set the intensity array
     */
    TIntensity& SetIntensity(void);

     /**
     * Get the mass delta array
     */
    const TDelta& GetDelta(void) const;

    /**
     * Set the mass delta array
     */
    TDelta& SetDelta(void);

   /**
    * Get the number of matches
    */
    const int GetM(void) const;

    /**
    * Set the number of matches
    */
    int& SetM(void);

    /**
     * Get the sum of ranks of matched peaks
     */
    const int GetSum(void) const;

    /**
     * Set the sum of ranks of matched peaks
     */
    int& SetSum(void);

    /**
     * Return the array containing the number of the ions
     * 
     * @return const TMSNumber
     */
    const TLadderNumber& GetLadderNumber(void) const;

    /**
     * Return the array containing the number of the ions
     * 
     * @return TMSNumber&
     */
    TLadderNumber& SetLadderNumber(void);

private:
    int LadderIndex; // current end of the ladder

    /** mass ladder */
    AutoPtr <int, ArrayDeleter<int> > Ladder;

    /** hit count for a given m/z value */
    AutoPtr <THit, ArrayDeleter<THit> > Hit;

    /** number of ion in the series */
    TLadderNumber LadderNumber;

    /** intensity of matched peaks */
    TIntensity Intensity;

    /** mass deltas between theoretical and experimental */
    TDelta Delta;

    int LadderSize;  // size of allocated buffer
    int Start, Stop;  // inclusive start and stop position in sequence
    int Index;  // gi or position in blastdb
    int Type;  // ion type
    int Mass;  // *neutral* precursor mass (Mr)
    int Charge;
    /** number of matched peaks */
    int M;
    /** sum of ranks of matched peaks */
    int Sum;
};


/////////////////// CLadder inline methods

inline
bool CLadder::CalcDelta(const int *IntMassArray, 
                        const char * const AAMap, 
                        const char *Sequence,
                        int Offset, 
                        int Direction, 
                        int NumMod, 
                        int &ModIndex,
                        const CMod ModList[], 
                        unsigned ModMask, 
                        int i,
                        int& ion,
                        const int ChargeIn, 
                        EMSSearchType SearchType,
                        double ExactMass)
{
    int delta = IntMassArray[AAMap[Sequence[Offset + Direction*i]]];
    if(!delta) return false; // unusable char (-BXZ*)

    // check for mods
    while(NumMod > 0 && ModIndex >= 0 && ModIndex < NumMod &&
        ModList[ModIndex].GetSite() == &(Sequence[Offset + Direction*i])) {
        if (MaskSet(ModMask, ModIndex)) { 
            delta += ModList[ModIndex].GetProductDelta();
        }
        ModIndex += Direction;
    }

    // increment the ladder

    // first check to see if exact mass increment needed
    if(SearchType == eMSSearchType_exact) {
        if ((ion * ChargeIn)/MSSCALE2INT(ExactMass) != 
            (ion * ChargeIn + delta) / MSSCALE2INT(ExactMass))
            ion += MSSCALE2INT(kNeutron)/ChargeIn;
    }

    ion += delta/ChargeIn;
    return true;
}

inline int& CLadder::operator [] (int n) 
{ 
    return *(Ladder.get() + n); 
}

inline int CLadder::size(void) 
{ 
    return LadderIndex; 
}

inline void CLadder::clear(void) 
{ 
    LadderIndex = 0; 
}

inline THit * CLadder::GetHit(void) 
{ 
    return Hit.get(); 
}

inline int CLadder::GetStart(void) 
{ 
    return Start; 
}

inline int CLadder::GetStop(void) 
{ 
    return Stop;
}

inline int CLadder::GetSeqIndex(void) 
{ 
    return Index; 
}

inline int CLadder::GetType(void) 
{ 
    return Type; 
}

inline int CLadder::GetMass(void) 
{ 
    return Mass;
}

inline int CLadder::GetCharge(void) 
{ 
    return Charge;
}

// count the number of matches
inline int CLadder::HitCount(void)
{
    int i, retval(0);
    for(i = 0; i < LadderIndex && i < LadderSize; i++)
	retval += *(Hit.get() + i);
    return retval;
}

// clear the Hitlist
inline void CLadder::ClearHits(void)
{
    int i;
    for(i = 0; i < LadderIndex && i < LadderSize; i++)
	*(Hit.get() + i) = 0;
}

inline bool CLadder::MaskSet(unsigned ModMask, int ModIndex)
{
    return (bool) (ModMask & (1 << ModIndex));
}

inline
const TIntensity& CLadder::GetIntensity(void) const
{
    return Intensity;
}

inline
TIntensity& CLadder::SetIntensity(void)
{
    return Intensity;
}

inline
const TDelta& CLadder::GetDelta(void) const
{
    return Delta;
}

inline
TDelta& CLadder::SetDelta(void)
{
    return Delta;
}

inline
const int CLadder::GetM(void) const
{
    return M;
}

inline
int& CLadder::SetM(void)
{
    return M;
}

inline
const int CLadder::GetSum(void) const
{
    return Sum;
}

inline
int& CLadder::SetSum(void)
{
    return Sum;
}

inline
const TLadderNumber& CLadder::GetLadderNumber(void) const
{
    return LadderNumber;
}

inline
TLadderNumber& CLadder::SetLadderNumber(void)
{
    return LadderNumber;
}

/////////////////// end of CLadder inline methods



typedef vector < CRef <CLadder> > TLadderList;
typedef AutoPtr <TLadderList> TLadderListPtr;
typedef pair <int,  TLadderListPtr > TLadderMapPair;
typedef map <int, TLadderListPtr > TLadderMap;
typedef list <TSeriesChargePair> TSeriesChargePairList;


class NCBI_XOMSSA_EXPORT CLadderContainer {
    public:

        /** 
         * returns the laddermap (maps key based on charge and series type to a vector of CLadder)
         */
        TLadderMap& SetLadderMap(void);

        /**
         * returns const laddermap
         */
        const TLadderMap& GetLadderMap(void) const;

        /**
         * return the list of charge, series type pairs that are used to
         * initialize the maps
         */
        TSeriesChargePairList& SetSeriesChargePairList(void);

        /**
          * return the list of charge, series type pairs that are used to
          * initialize the maps
          */
        const TSeriesChargePairList& GetSeriesChargePairList(void) const;

        /** 
         * iterate over the ladder map over the charge range and series type indicated
         * 
         * @param Iter the iterator
         * @param BeginCharge the minimum charge to look for (0=all)
         * @param EndCharge the maximum charge to look for (0=all)
         * @param SeriesType the type of the ion series
         */
        void Next(TLadderMap::iterator& Iter,
                  TMSCharge BeginCharge = 0,
                  TMSCharge EndCharge = 0,
                  TMSIonSeries SeriesType = eMSIonTypeUnknown);


        void Begin(TLadderMap::iterator& Iter,
                                   TMSCharge BeginCharge = 0,
                                   TMSCharge EndCharge = 0,
                                   TMSIonSeries SeriesType = eMSIonTypeUnknown);


        /**
         * populate the Ladder Map with arrays based on the ladder
         * 
         * @param MaxModPerPep size of the arrays
         * @param MaxLadderSize size of the ladders
         */
        void CreateLadderArrays(int MaxModPerPep, int MaxLadderSize);

        
    private:

        /**
         * see if key of the map element pointed to by Iter matches the conditions
         * 
         * @param Iter the iterator
         * @param BeginCharge the minimum charge to look for (0=all)
         * @param EndCharge the maximum charge to look for (0=all)
         * @param SeriesType the type of the ion series
         */
        bool MatchIter(TLadderMap::iterator& Iter,
                       TMSCharge BeginCharge = 0,
                       TMSCharge EndCharge = 0,
                       TMSIonSeries SeriesType = eMSIonTypeUnknown);
        /**     
         * contains a map from charge, ion series to the CLadderArrays 
         * the key is gotten from CMSMatchedPeakSetMap::ChargeSeries2Key 
         */
        TLadderMap LadderMap;
        
        /** 
         * contains the series, charge of the ion series to be generated 
         */
        TSeriesChargePairList SeriesChargePairList;
};  

inline
TLadderMap& 
CLadderContainer::SetLadderMap(void)
{
    return LadderMap;
}

inline
const TLadderMap& 
CLadderContainer::GetLadderMap(void) const
{
    return LadderMap;
}

inline
TSeriesChargePairList& 
CLadderContainer::SetSeriesChargePairList(void)
{
    return SeriesChargePairList;
}

inline
const TSeriesChargePairList& 
CLadderContainer::GetSeriesChargePairList(void) const
{
    return SeriesChargePairList;
}


END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

