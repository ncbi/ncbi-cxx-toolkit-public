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
 * Authors:  Lewis Y. Geer, Douglas J. Slotta
 *
 * File Description:
 *    code to do the ms/ms search and score matches
 *
 * ===========================================================================
 */

#ifndef OMSSA__HPP
#define OMSSA__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbifloat.h>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbithr.hpp>

#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/iterator.hpp>
#include <serial/objostrxml.hpp>

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

/** progress callback for CSearch */
typedef void (*TOMSSACallback)(int TotalSeq, int Completed, void* Anything);


/////////////////////////////////////////////////////////////////////////////
//
//  TMassMask::
//
//  Handy container for holding masses and modification masks
//

typedef struct _MassMask {
    int Mass, Mask;
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

class NCBI_XOMSSA_EXPORT CSearch : public CThread {
public:


    CSearch(int tNum);
    ~CSearch(void);

    
    /**
     *  init blast databases.  stream thru db if InitDB true
     * 
     * @param blastdb name of blast database
     */
    int InitBlast(const char *blastdb, bool use_mmap = false);

    /**
      *  Reset global parameters used in threaded search.
      *  Call whenever rerunning a search.
      */
    static void ResetGlobals(void);

    /**
     *  Performs the ms/ms search
     *
     * @param MyRequestIn the user search params and spectra
     * @param MyResponseIn the results of the search
     * @param Modset list of modifications
     * @param SettingsIn the search settings
     * @param Callback callback function for progress meter
     * @param CallbackData data passed back to callback fcn
     */
    void Search(CRef<CMSRequest> MyRequestIn,
                CRef<CMSResponse> MyResponseIn,
                CRef <CMSModSpecSet> Modset,
                CRef <CMSSearchSettings> SettingsIn,
                TOMSSACallback Callback = 0,
                void *CallbackData = 0);

    /**
     *  Setup the ms/ms search
     *
     * @param MyRequestIn the user search params and spectra
     * @param MyResponseIn the results of the search
     * @param Modset list of modifications
     * @param SettingsIn the search settings
     * @param Callback callback function for progress meter
     * @param CallbackData data passed back to callback fcn
     */
    void SetupSearch(CRef<CMSRequest> MyRequestIn,
                     CRef<CMSResponse> MyResponseIn,
                     CRef <CMSModSpecSet> Modset,
                     CRef <CMSSearchSettings> SettingsIn,
                     TOMSSACallback Callback = 0,
                     void *CallbackData = 0);


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

    virtual void* Main(void);
    virtual void OnExit(void);
    void CopySettings(CRef <CSearch> fromObj);
    void TestPersist(void) {
        cout << "Still availiable!\n";
    }
    
    // take hitlist for a peak and insert it into the response
    void SetResult(CRef<CMSPeakSet> PeakSet);
    static CRef <CMSPeakSet> SharedPeakSet;

protected:

    /**
     * 
     * @param SingleForward singly charged NC ions
     * @param SingleBackward singly charged CN ions
     * @param Double double charged product ions
     * @param DoubleForward is the double series a forward series?
     */
    void DoubleCompare(list<CMSMatchedPeakSet *> &SingleForward,
                       list<CMSMatchedPeakSet *> &SingleBackward,
                       list<CMSMatchedPeakSet *> &Double,
                       bool DoubleForward);

    /**
     * Creates match ion match lists
     * 
     * @param Peaks peak list
     * @param Hit the hit
     * @param Which which peak list to use
     * @param minintensity minimum intensity
     * @param iPairList
     * @param Forward NC ions
     * @param Backward CN ions
     */
    void MatchAndSort(CMSPeak * Peaks,
                      CMSHit& Hit,
                      EMSPeakListTypes Which, 
                      int minintensity, 
                      const TSeriesChargePairList::const_iterator &iPairList,
                      list<CMSMatchedPeakSet *> &SingleForward,
                      list<CMSMatchedPeakSet *> &SingleBackward);

    // loads spectra into peaks
    //void Spectrum2Peak(CMSPeakSet& PeakSet);
    void Spectrum2Peak(CRef<CMSPeakSet> PeakSet);

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
     * set up the ions to use
     * 
     * @param Ions list of ions to be set up
     */
    void SetIons(list <EMSIonSeries> & Ions);

    /**
     *  create the ladders from sequence
     */
    int CreateLadders(const char *Sequence, int iSearch, int position,
		      int endposition,
		      int *Masses, int iMissed, CAA& AA, 
		      int iMod,
		      CMod ModList[],
		      int NumMod
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

    /**
     * Generate a peptide sequence
     * 
     * @param Start start of sequence
     * @param Stop stop of sequence
     * @param seqstring sequence string
     * @param Sequence sequence object
     */
    void CreateSequence(int Start, int Stop, string &seqstring, CSeqDBSequence &Sequence);

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
                          CRef <CMSModSpecSet> &Modset);

    // create the various combinations of mods
    void CreateModCombinations(int Missed,
                               const char *PepStart[],
                               int Masses[],
                               int EndMasses[],
                               int NumMod[],
                               int NumMassAndMask[],
                               int NumModSites[],
                               CMod ModList[][MAXMOD]);

    /**
     * initialize mass ladders
     * 
     * @param Ions list of ion types
     */
    void InitLadders(std::list <EMSIonSeries> & Ions);

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
    static int MaxMZ;

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

    /**
     * Tracks the iSearch number for all search threads
     */
    static int iSearchGlobal;
    
    /**
     * The threadid number
     */
    int ThreadNum;
    
    /**
     * These are so CSearch::Main() can call CSearch::Search() in a threaded run,
     * this requires CSearch::SetupSearch() to be called before CSearch::Run()
     */
    CRef <CMSRequest> initRequestIn;
    CRef <CMSResponse> initResponseIn;
    CRef <CMSModSpecSet> initModset;
    CRef <CMSSearchSettings> initSettingsIn;
    TOMSSACallback initCallback;
    void *initCallbackData;
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

/**
 * class to hold various helper functions for CSearch
 */
class NCBI_XOMSSA_EXPORT CSearchHelper {
public:
    /**
     * 
     * @param MySearch
     * @param iOutFile
     * @param Filename
     * @param FileFormat
     * @param txt_out
     */
    static void SaveOneFile(CMSSearch &MySearch, 
                            const string Filename, 
                            ESerialDataFormat FileFormat, 
                            bool IncludeRequest,
                            bool bz2);

    /**
     * read in modification files.  probably should be in some helper class
     * 
     * @param ModFileName mods.xml
     * @param UserModFileName usermods.xml
     * @param Path program path
     * @param Modset the data structure containing the mods
     * @param bz2 use bzip2 compression
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

    /**
     * Read in a spectrum file
     * 
     * @param Filename name of file
     * @param FileType type of file to be read in
     * @param MySearch the search
     * @return 1, -1 = error, 0 = ok
     */
    static int ReadFile(const string& Filename, 
                        const EMSSpectrumFileType FileType, 
                        CMSSearch& MySearch);


    /**
      * Read in any input file
      * 
      * @param InFile type and name of file to be read in
      * @param MySearch the search i/o object
      * @param SearchEngineIterative set to true if this should be
      *                              an iterative search
      * @return 1, -1 = error, 0 = ok
      */
    static int LoadAnyFile(CMSSearch& MySearch, 
                           CConstRef <CMSInFile> InFile,
                           bool* SearchEngineIterative = 0);

    /** 
      * Read in an MSRequest
      * @param Filename name of file
      * @param Dataformat xml or asn.1
      * @param MySearch the search
      * @return 0 if OK
      */
    static int ReadSearchRequest(const string& Filename,
                                 const ESerialDataFormat DataFormat,
                                 CMSSearch& MySearch);

    /** 
     * Read in a complete search (typically for an iterative search)
     * @param Filename name of file
     * @param Dataformat xml or asn.1
     * @param bz2 is the file bzip2 compressed?
     * @param MySearch the search
     * @return 0 if OK
     */
    static int ReadCompleteSearch(const string& Filename,
                                  const ESerialDataFormat DataFormat,
                                  bool bz2,
                                  CMSSearch& MySearch);

    /**
      * Write out a complete search
      * @param Modset modifications used in search
      * @param OutFiles list of output files
      * @param MySearch the search
      * @return 0 if OK
      */
    static int SaveAnyFile(CMSSearch& MySearch, 
                           CMSSearchSettings::TOutfiles OutFiles,
                           CRef <CMSModSpecSet> Modset);


    /**
     * Validates Search Settings
     * @param Settings
     */
    static void ValidateSearchSettings(CRef<CMSSearchSettings> &Settings);


    /**
     * create search setting object from file or brand new
     * @param FileName name of search settings file
     * @param Settings output
     */
    static void CreateSearchSettings(string FileName,
                              CRef<CMSSearchSettings> &Settings);

};  


END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

