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
 *    code to do the ms/ms search and score matches
 *
 * ===========================================================================
 */

#ifndef OMSSA__HPP
#define OMSSA__HPP

#include <objects/omssa/omssa__.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>

#include "msms.hpp"
#include "msladder.hpp"
#include "mspeak.hpp"

#include <vector>
#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)

// max number missed cleavages + 1
#define MAXMISSEDCLEAVE 4
// max variable mods in peptide
// should be no bigger than the number of bits in unsigned
#define MAXMOD 32
// maximum number of calculable ladders
#define MAXMOD2 8192

// max length of sequence accession
const int kAccLen = 20;

// arbitrary high evalue
const double kHighEval = 1e50;

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

/**
 * class to hold various helper functions for CSearch
 */
class NCBI_XOMSSA_EXPORT CSearchHelper {
public:

    /**
     * read in modification files.  probably should be in some helper class
     * 
     * @param ModFileName mods.xml
     * @param UserModFileName usermods.xml
     * @param Path program path
     * @param Modset the data structure containing the mods
     * @return 1 on error
     */
    static int ReadModFiles(const string& ModFileName,
                            const string& UserModFileName, 
                            const string& Path,
                            CRef <CMSModSpecSet> Modset);

    /** 
     * read in taxonomy file
     * 
     * @param Filename filename
     * @param TaxNameMap maps taxid to friendly name
     */
    typedef map<int, string> TTaxNameMap;
    static void ReadTaxFile(string& Filename, TTaxNameMap& TaxNameMap);

    /**
     * correctly set up xml stream
     * 
     * @param xml_out xml output stream
     */
    static void ConditionXMLStream(CObjectOStreamXml *xml_out);

};



/////////////////////////////////////////////////////////////////////////////
//
//  TMassMask::
//
//  Handy container for holding masses and modification masks
//

typedef struct _MassMask {
    unsigned Mass, Mask;
} TMassMask;


/////////////////////////////////////////////////////////////////////////////
//
// for holding hits sorted by score
//

typedef multimap <double, CMSHit *> TScoreList;

/////////////////////////////////////////////////////////////////////////////
//
//  CSearch::
//
//  Performs the ms/ms search
//

class NCBI_XOMSSA_EXPORT CSearch {
public:
    CSearch(void);
    ~CSearch(void);

    
    /**
     *  init blast databases.  stream thru db if InitDB true
     * 
     * @param blastdb name of blast database
     */
    int InitBlast(const char *blastdb);


    /**
     *  Performs the ms/ms search
     *
     * @param MyRequestIn the user search params and spectra
     * @param MyResponseIn the results of the search
     * @param Modset list of modifications
     * @param SettingsIn the search settings
     */
    int Search(CRef<CMSRequest> MyRequestIn,
               CRef<CMSResponse> MyResponseIn,
               CRef <CMSModSpecSet> Modset,
               CRef <CMSSearchSettings> SettingsIn);


   /**
    * fill out MatchedPeakSet
    * 
    * @param Hit the match being evaluated
    * @param SeriesCharge charge of ion series
    * @param Ion which ion series
    * @param MinIntensity the minimum intensity to consider
    * @param Which which version of experimental peaks to use
    * @param Peaks experimental peaks
    * @param Maxproductions the number of ions in each series actually searched
    * @return MatchedPeakSet that was filled out
    */
    CMSMatchedPeakSet * PepCharge(CMSHit& Hit,
                                  int SeriesCharge,
                                  int Ion,
                                  int MinIntensity,
                                  int Which,
                                  CMSPeak *Peaks,
                                  int Maxproductions);

    /** 
     * Sets the scoring to use rank statistics
     */
    bool& SetRankScore(void);

    /** 
     * Sets iterate search 
     */
    bool& SetIterative(void);

    /** 
     * Gets iterate search 
     */
    const bool GetIterative(void) const;

protected:

    // loads spectra into peaks
    void Spectrum2Peak(CMSPeakSet& PeakSet);

    /**
     * set up modifications from both user input and mod file data
     * 
     * @param MyRequest the user search params and spectra
     * @param Modset list of modifications
     */
    void SetupMods(CRef <CMSModSpecSet> Modset);

    /**
     *  count the number of unique sites modified
     * 
     * @param NumModSites the number of unique mod sites
     * @param NumMod the number of mods
     * @param ModList modification information
     */
    void CountModSites(int &NumModSites,
                       int NumMod,
                       CMod ModList[]);


    /** 
     * compare ladders to experiment
     * 
     * @param iMod ladder to examine
     * @param Peaks the experimental values
     * @param MassPeak
     * @param N the number of experimental peaks
     * @param M the number of matched peaks
     * @param Sum the sum of the ranks of the matched peaks
     * 
     */
    void CompareLaddersRank(int iMod,
                            CMSPeak *Peaks,
                            const TMassPeak *MassPeak,
                            int& N,
                            int& M,
                            int& Sum);

    /**
     * set up the ions to use
     */
    void SetIons(int& ForwardIon, int& BackwardIon);

    /**
     *  create the ladders from sequence
     */
    int CreateLadders(const char *Sequence, int iSearch, int position,
		      int endposition,
		      int *Masses, int iMissed, CAA& AA, 
		      int iMod,
		      CMod ModList[],
		      int NumMod,
              int ForwardIon,  //!< a,b,c series
              int BackwardIon  //!< x,y,z series
              );

    /**
     * compare ladders to experiment
     */
    int CompareLadders(int iMod,
                       CMSPeak *Peaks,
                       bool OrLadders,
                       const TMassPeak *MassPeak);

    bool CompareLaddersTop(int iMod, 
                           CMSPeak *Peaks,
                           const TMassPeak *MassPeak);

    /**
    * Checks to see that given modindex points to a site shared by a fixed mod
    * 
    * @param i index into ModIndex
    * @param NumFixed number of fixed mods in ModIndex
    * @param ModList modification information
    * @param ModIndex array of iterators pointing to mod ordinals
    * @return true if shared
    * 
    */
    bool CheckFixed(int i,
                    int NumFixed, 
                    CMod ModList[], 
                    int *ModIndex );

    void InitModIndex(int *ModIndex, 
                      int& iMod, 
                      int NumMod, 
                      int NumModSites, 
                      CMod ModList[]);

    unsigned MakeBoolMask(int *ModIndex, int iMod);
    void MakeBoolMap(bool *ModMask, int *ModIndex, int& iMod, int& NumMod);

    bool CalcModIndex(int *ModIndex, 
                      int& iMod, 
                      int& NumMod, 
                      int NumFixed,
                      int NumModSites, 
                      CMod CModList[]);

    unsigned MakeIntFromBoolMap(bool *ModMask,  int& NumMod);
    const int Getnumseq(void) const;

    ///
    ///  Adds modification information to hitset
    ///
    void AddModsToHit(CMSHits *Hit, CMSHit *MSHit);

    ///
    ///  Adds ion information to hitset
    ///
    void AddIonsToHit(CMSHits *Hit, CMSHit *MSHit);

    ///
    ///  Makes a string hashed out of the sequence plus mods
    ///
    static void MakeModString(string& seqstring, string& modseqstring, CMSHit *MSHit);

    // take hitlist for a peak and insert it into the response
    void SetResult(CMSPeakSet& PeakSet);

    /**
     * write oidset to result
     */
    void WriteBioseqs(void);


    //! calculate the evalues of the top hits and sort
    void CalcNSort(TScoreList& ScoreList,  //<! the list of top hits to the spectrum
                   double Threshold,       //!< the noise threshold to apply to the peaks
                   CMSPeak* Peaks,         //!< the spectrum to be scored
                   bool NewScore           //!< use the new scoring
                   );

    /**
     *  delete variable mods that overlap with fixed mods
     * @param NumMod the number of modifications
     * @param ModList modification information
     */
    void DeleteVariableOverlap(int& NumMod,
                               CMod ModList[]);

    /**
     *  update sites and masses for new peptide
     * 
     * @param Missed how many missed cleavages
     * @param PepStart array of peptide starts
     * @param PepEnd array of peptide ends
     * @param NumMod array of the number of mods
     * @param ModList modification info
     * @param Masses peptide masses
     * @param EndMasses peptide end masses
     * @param NumModSites array of number of unfixed mod sites
     * @param Modset modification specifications
     */
    void UpdateWithNewPep(int Missed,
              const char *PepStart[],
              const char *PepEnd[], 
              int NumMod[], 
              CMod ModList[][MAXMOD],
              int Masses[],
              int EndMasses[],
                          int NumModSites[],
                          CRef <CMSModSpecSet> Modset);

    // create the various combinations of mods
    void CreateModCombinations(int Missed,
                               const char *PepStart[],
                               int Masses[],
                               int EndMasses[],
                               int NumMod[],
                               unsigned NumMassAndMask[],
                               int NumModSites[],
                               CMod ModList[][MAXMOD]);

    /**
     * initialize mass ladders
     * 
     */
    void InitLadders(int ForwardIon, int BackwardIon);

    /**
     * makes map of oid from previous search
     * used in iterative searching
     */
    void MakeOidSet(void);

    /**
     * examines a hitset to see if any good hits
     * 
     * @param Number the spectrum number of the hitset
     * @return true if no good hits
     */
    const bool ReSearch(const int Number) const;

    /**
     * get the oidset
     */
    CMSResponse::TOidSet& SetOidSet(void);

    /**
     * get the oidset
     */
    const CMSResponse::TOidSet& GetOidSet(void) const;

    /**
     * is this search restricted to the oid set?
     */
    bool& SetRestrictedSearch(void);

    /**
     * is this search restricted to the oid set?
     */
    const bool GetRestrictedSearch(void) const;


    /**
     * Get the bit that indicates whether a ladder was calculated
     * 
     * @param i the index of the ladder
     */
    Int1 GetLadderCalc(int i) const;

    /**
     * Set the bit that indicates whether a ladder was calculated
     * 
     * @param i the index of the ladder
     */
    Int1& SetLadderCalc(int i);

    /**
     * Clear the ladder calc array up to max index
     * 
     * @param Max the number of indices to clear
     */
    void ClearLadderCalc(int Max);

    /**
     * Set the mask and mass of mod bit array
     * 
     * @param i the index for missed cleavages
     * @param j the index for modification combinations 
     */
    TMassMask& SetMassAndMask(int i, int j);

    /**
     * Set search settings
     */
    CRef<CMSSearchSettings>& SetSettings(void);

    /**
     * Get search settings
     */
    CConstRef<CMSSearchSettings> GetSettings(void) const;

    /**
     * Set search request
     */
    CRef<CMSRequest>& SetRequest(void);

    /**
     * Get search request
     */
    CConstRef<CMSRequest> GetRequest(void) const;

    /**
     * Set search response
     */
    CRef<CMSResponse>& SetResponse(void);

    /**
     * Get search response
     */
    CConstRef<CMSResponse> GetResponse(void) const;

    /**
     * Set search enzyme
     */
    CRef <CCleave>& SetEnzyme(void);

    /**
     * Get search enzyme
     */
    CConstRef<CCleave> GetEnzyme(void) const;


    /**
     * set the ladder container
     * 
     * this container holds theoretical mass ladders sorted by charge and series type
     */
    CLadderContainer& 
        SetLadderContainer(void);

    /**
     * get the ladder container
     */
    const CLadderContainer& 
        GetLadderContainer(void) const;


private:
    /** blast library */
    CRef <CSeqDB> rdfp;

    //    CMSHistSet HistSet;  // score arrays
    CMassArray MassArray;  // amino acid mass arrays
    CMassArray PrecursorMassArray;  // precursor mass arrays
    CMSMod VariableMods;  // categorized variable mods
    CMSMod FixedMods;  // categorized fixed mods
    int numseq; // number of sequences in blastdb

    /**
     * the enzyme in use
     */
    CRef <CCleave> Enzyme;

    /**
     * Search request
     */
    CRef<CMSRequest> MyRequest;

    /**
     * Search response
     */
    CRef<CMSResponse> MyResponse;

    /**
     * Search params
     */
    CRef<CMSSearchSettings> MySettings;

    /**
     * ion series mass ladders
     */
    vector< CRef < CLadder > > BLadder,
        YLadder,
        B2Ladder,
        Y2Ladder;
    CLadderContainer LadderContainer;

	/**
     * bool array that indicates if the ladders been calculated
     */
    AutoPtr <Int1, ArrayDeleter<Int1> > LadderCalc;

	/**
     * contains bit mask of modifications and resulting mass
     */
	AutoPtr <TMassMask, ArrayDeleter<TMassMask> > MassAndMask;

    /**
     * maximum number of mod combinations per peptide
     */
    int MaxModPerPep;

    /**
     * maximum m/z value of all spectra precursors
     * used to bound non-specific cleavage searches
     */
    int MaxMZ;

    /**
     * boolean to turn on rank scoring
     */
    bool UseRankScore;

    /**
     * boolean to turn on iterative search
     */
    bool Iterative;

    /**
     * set of oids to be searched
     */
    CMSResponse::TOidSet OidSet;

    /**
     * is this a oid restricted search
     */
    bool RestrictedSearch;

};

///////////////////  CSearch inline methods



// ModIndex contains the positions of the modified sites (aka set bits).
// InitModIndex points ModIndex to all of the lower sites.
inline void CSearch::InitModIndex(int *ModIndex, 
                                  int& iMod, 
                                  int NumMod,
                                  int NumModSites, 
                                  CMod ModList[])
{
    // pack all the mods to the first possible sites
    int j(0), i, NumFixed;
    for(i = 0; i < NumMod; i++) {
        if(ModList[i].GetFixed() == 1) {
            ModIndex[j] = i;
            j++;
        }
    }
    NumFixed = j;
    const char *OldSite(0);
    for(i = 0; i < NumMod && j - NumFixed <= iMod; i++) {

        if(ModList[i].GetFixed() != 1 && ModList[i].GetSite() != OldSite) {
            ModIndex[j] = i;
            OldSite = ModList[i].GetSite();
            j++;
        }
    }
}

// makes a bool map where each bit represents a site that can be modified
// ModIndex contains the position of the set bit (aka modified sites)
inline void CSearch::MakeBoolMap(bool *ModMask, int *ModIndex, int& iMod, int& NumMod)
{
    int j;
    // zero out mask
    for(j = 0; j < NumMod; j++)
	ModMask[j] = false;
    // mask at the possible sites according to the index
    for(j = 0; j <= iMod; j++) 
	ModMask[ModIndex[j]] = true;
}

// makes a bool mask  where each bit represents a site that can be modified
inline unsigned CSearch::MakeBoolMask(int *ModIndex, int iMod)
{
    int j(0);
    unsigned retval(0);
    // mask at the possible sites according to the index
    for(; j <= iMod; j++) 
	retval |= 1 << ModIndex[j];
    return retval;
}

// creates a unique int for a given mod map.  used to track ladders
inline unsigned CSearch::MakeIntFromBoolMap(bool *ModMask,  int& NumMod)
{
    int j, retval(0);
    for(j = 0; j < NumMod; j++)
	if(ModMask[j]) retval |= 1 << j;
    return retval;
}

// CalcModIndex moves the set bits through all possible sites.  
// site position is given by ModIndex
inline bool CSearch::CalcModIndex(int *ModIndex, 
                                  int& iMod, int& NumMod,
                                  int NumFixed, 
                                  int NumModSites, 
                                  CMod ModList[])
{
    const char *TopSite;
    int j, OldIndex;

    // start at the lowest non fixed index to see if it can be moved
    for (j = NumFixed; j <= iMod + NumFixed; j++) {
        OldIndex = ModIndex[j];
        if (j == iMod + NumFixed) TopSite = 0;
        else TopSite = ModList[ModIndex[j+1]].GetSite();

        // move the low index. keep incrementing if pointing at fixed site
        do {
            ModIndex[j]++;
        } while (ModList[ModIndex[j]].GetFixed() == 1);

        // if the low index doesn't point to the top site and it isn't too big
        // allow the move
        if (ModIndex[j] < NumMod && ModList[ModIndex[j]].GetSite() != TopSite) {
            {
                // now push all of the indices lower than the low index to
                // their lowest possible value
                int i, Low(0);
                const char *OldSite(0);
                for (i = NumFixed; i < j; i++) {

                    // increment low until it doesn't point at the same site
                    // or a fixed site
                    while(ModList[Low].GetFixed() == 1 || 
                          OldSite == ModList[Low].GetSite())
                        Low++;

                    ModIndex[i] = Low;
                    OldSite = ModList[ModIndex[i]].GetSite();
                    Low++;
                }

            }
            return true;
        }
        // otherwise, restore the low index and look at the next highest index
        ModIndex[j] = OldIndex;
    }
    return false;
}

inline
Int1 CSearch::GetLadderCalc(int i) const
{
    return *(LadderCalc.get() + i);
}

inline
Int1& CSearch::SetLadderCalc(int i)
{
    return *(LadderCalc.get() + i);
}

inline
void CSearch::ClearLadderCalc(int Max)
{
	memset(LadderCalc.get(), 0, sizeof(Int1)*Max);
}

inline
TMassMask& CSearch::SetMassAndMask(int i, int j)
{    
    return *(MassAndMask.get() + i*MaxModPerPep+j);
}

inline
bool& CSearch::SetRankScore(void)
{
    return UseRankScore;
}

inline
bool& CSearch::SetIterative(void)
{   
    return Iterative;
}   
    
inline
const bool CSearch::GetIterative(void) const
{
    return Iterative;
}

inline
CRef<CMSSearchSettings>& CSearch::SetSettings(void)
{
    return MySettings;
}

inline
CConstRef<CMSSearchSettings> CSearch::GetSettings(void) const
{
    return MySettings;
}

inline
CRef<CMSRequest>& CSearch::SetRequest(void)
{
    return MyRequest;
}

inline
CConstRef<CMSRequest> CSearch::GetRequest(void) const
{
    return MyRequest;
}

inline
CRef<CMSResponse>& CSearch::SetResponse(void)
{
    return MyResponse;
}

inline
CConstRef<CMSResponse> CSearch::GetResponse(void) const
{
    return MyResponse;
}

inline
CMSResponse::TOidSet& CSearch::SetOidSet(void)
{
    return OidSet;
}

inline
const CMSResponse::TOidSet& CSearch::GetOidSet(void) const
{
    return OidSet;
}

inline
const int CSearch::Getnumseq(void) const 
{ 
    return numseq; 
}

inline
bool& CSearch::SetRestrictedSearch(void)
{
    return RestrictedSearch;
}

inline
const bool CSearch::GetRestrictedSearch(void) const
{
    return RestrictedSearch;
}

inline
CRef <CCleave>& CSearch::SetEnzyme(void)
{
    return Enzyme;
}

inline
CConstRef<CCleave> CSearch::GetEnzyme(void) const
{
    return Enzyme;
}

inline
CLadderContainer& 
CSearch::SetLadderContainer(void)
{
    return LadderContainer;
}

inline
const CLadderContainer& 
CSearch::GetLadderContainer(void) const
{
    return LadderContainer;
}


/////////////////// end of CSearch inline methods


END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

/*
  $Log$
  Revision 1.40  2006/07/20 21:00:21  lewisg
  move functions out of COMSSA, create laddercontainer

  Revision 1.39  2006/05/25 17:11:56  lewisg
  one filtered spectrum per precursor charge state

  Revision 1.38  2006/03/13 15:48:11  lewisg
  omssamerge and intermediate score fixes

  Revision 1.37  2006/01/23 17:47:37  lewisg
  refactor scoring

  Revision 1.36  2005/11/07 19:57:20  lewisg
  iterative search

  Revision 1.35  2005/09/20 21:07:57  lewisg
  get rid of c-toolkit dependencies and nrutil

  Revision 1.34  2005/09/15 21:29:24  lewisg
  filter out n-term protein mods

  Revision 1.33  2005/09/14 15:30:17  lewisg
  neutral loss

  Revision 1.32  2005/09/07 21:30:50  lewisg
  force fixed and variable mods not to overlap

  Revision 1.31  2005/08/15 14:24:56  lewisg
  new mod, enzyme; stat test

  Revision 1.30  2005/08/01 13:44:18  lewisg
  redo enzyme classes, no-enzyme, fix for fixed mod enumeration

  Revision 1.29  2005/05/23 19:07:34  lewisg
  improve perf of ladder calculation

  Revision 1.28  2005/05/19 22:17:16  lewisg
  move arrays to AutoPtr

  Revision 1.27  2005/05/19 21:19:28  lewisg
  add ncbifloat.h include for msvc

  Revision 1.26  2005/05/19 16:59:17  lewisg
  add top-down searching, fix variable mod bugs

  Revision 1.25  2005/05/13 17:58:52  lewisg
  *** empty log message ***

  Revision 1.24  2005/05/13 17:57:17  lewisg
  one mod per site and bug fixes

  Revision 1.23  2005/04/22 15:31:21  lewisg
  fix tax filter on output

  Revision 1.22  2005/04/12 00:32:06  lewisg
  fix modification enumeration

  Revision 1.21  2005/04/05 21:02:52  lewisg
  increase number of mods, fix gi problem, fix empty scan bug

  Revision 1.20  2005/03/14 22:29:54  lewisg
  add mod file input

  Revision 1.19  2005/01/11 21:08:43  lewisg
  average mass search

  Revision 1.18  2004/11/30 23:39:57  lewisg
  fix interval query

  Revision 1.17  2004/11/22 23:10:36  lewisg
  add evalue cutoff, fix fixed mods

  Revision 1.16  2004/09/29 19:43:09  lewisg
  allow setting of ions

  Revision 1.15  2004/09/15 18:35:00  lewisg
  cz ions

  Revision 1.14  2004/07/22 22:22:58  lewisg
  output mods

  Revision 1.13  2004/07/06 22:38:05  lewisg
  tax list input and user settable modmax

  Revision 1.12  2004/06/21 21:19:27  lewisg
  new mods (including n term) and sample perl parser

  Revision 1.11  2004/06/08 19:46:21  lewisg
  input validation, additional user settable parameters

  Revision 1.10  2004/04/05 20:49:16  lewisg
  fix missed mass bug and reorganize code

  Revision 1.9  2004/03/30 19:36:59  lewisg
  multiple mod code

  Revision 1.8  2004/03/16 20:18:54  gorelenk
  Changed includes of private headers.

  Revision 1.7  2004/03/12 16:25:07  lewisg
  add comments

  Revision 1.6  2004/03/01 18:24:08  lewisg
  better mod handling

  Revision 1.5  2003/12/22 21:58:00  lewisg
  top hit code and variable mod fixes

  Revision 1.4  2003/12/04 23:39:08  lewisg
  no-overlap hits and various bugfixes

  Revision 1.3  2003/10/24 21:28:41  lewisg
  add omssa, xomssa, omssacl to win32 build, including dll

  Revision 1.2  2003/10/21 21:12:17  lewisg
  reorder headers

  Revision 1.1  2003/10/20 21:32:13  lewisg
  ommsa toolkit version

  Revision 1.8  2003/10/07 18:02:28  lewisg
  prep for toolkit

  Revision 1.7  2003/10/06 18:14:17  lewisg
  threshold vary

  Revision 1.6  2003/08/14 23:49:22  lewisg
  first pass at variable mod

  Revision 1.5  2003/07/17 18:45:50  lewisg
  multi dta support

  Revision 1.4  2003/07/07 16:17:51  lewisg
  new poisson distribution and turn off histogram

  Revision 1.3  2003/04/18 20:46:52  lewisg
  add graphing to omssa

  Revision 1.2  2003/04/02 18:49:51  lewisg
  improved score, architectural changes

  Revision 1.1  2003/02/03 20:39:03  lewisg
  omssa cgi

  *
  */
