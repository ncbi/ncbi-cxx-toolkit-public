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

#ifndef MSMS__HPP
#define MSMS__HPP

#ifdef WIN32
#pragma warning(disable:4786)
#endif

#include <list>
#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <deque>
#include <map>
#include <objects/omssa/MSModSpecSet.hpp>
#include <objects/omssa/MSSearchSettings.hpp>
#include "Mod.hpp"
#include "SpectrumSet.hpp"

// #include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)


// non-redundified integer intervals of amino acids
const int kNumAAIntervals = 19;

// ABCXYZ ion mass calculation constants.  See Papayannopoulos, pg 63.

/** mass of water */
const double kWater = 18.010565;

/** neutron mass */
const double kNeutron = 1.008664904; 

// const double AAAbundance[] = {1.0, 0.0758, 1.0, 0.0167, 0.0528, 0.0635, 0.0408, 0.0683, 0.0224, 0.058, 0.0593, 0.0943, 0.0237, 0.0447, 0.0491, 0.0399, 0.0514, 0.0715, 0.0569, 0.0656, 0.0124, 1.0, 0.0318, 1.0, 1.0, 1.0, 0.0}; 

// masses taken from Papayannopoulos, IA, Mass Spectrometry Reviews, 1995, 14, 49-73.
// selenocysteine calculated by using cysteine mass and adding difference between Se and S from webelements.  
// monoisotopic mass
const double MonoMass[] = {0.0, 71.03711, 0.0, 103.00919, 115.02694, 129.04259, 147.06841, 57.02147, 137.05891, 113.08406, 128.09496, 113.08406, 131.04049, 114.04293, 97.05276, 128.05858, 156.10111, 87.03203, 101.04768, 99.06841, 186.07931, 0.0, 163.06333, 0.0, 149.903 , 0.0, 113.08406, 237.14776, 0.0 };
// average mass
const double AverageMass[] = {0.0, 71.08, 0.0, 103.15, 115.09, 129.12, 147.18, 57.05, 137.14, 113.16, 128.17, 113.16, 131.20, 114.10, 97.12, 128.13, 156.19, 87.08, 101.11, 99.13, 186.21, 0.0, 163.18, 0.0, 150.044, 0.0, 113.16, 237.30, 0.0 };
// n15 enriched monoisotopic mass
const double MonoN15Mass[] = {0.0, 72.034144893, 0.0, 104.006224893, 116.023974893, 130.039624893, 148.065444893, 58.018494893, 140.050014679, 114.081094893, 130.089029786, 114.081094893, 132.037524893, 116.036999786, 98.049794893, 130.052649786, 160.089239572, 88.029064893, 102.044714893, 100.065444893, 188.073379786, 0.0, 164.060364893, 0.0, 150.8964, 0.0, 114.081094893, 240.1388649, 0.0 };


// const int AAIntervals[] = { 57, 71, 87, 97, 99, 101, 103, 113, 114, 115, 128, 129, 131, 137, 147, 150, 156, 163, 186 };


const double kTermMass[] =  {1.007825, 1.007825, 1.007825, 17.00274, 17.00274, 17.00274};
const double kIonTypeMass[] = { -27.994915, 0.0, 17.02655, 27.994915, 2.01565, -14.003075 };

/////////////////////////////////////////////////////////////////////////////
//
//  CMassArray::
//
//  Holds AA indexed mass array
//

class NCBI_XOMSSA_EXPORT CMassArray {
public:
    CMassArray(void) {};

    const double * const GetMass(void) const;
    const int * const GetIntMass(void) const;

    //! initialize mass arrays with fixed mods
    void Init(const CMSSearchSettings::TProductsearchtype &SearchType);
    // initialize mass arrays with fixed mods
    void Init(const CMSMod &Mods, 
	      const CMSSearchSettings::TProductsearchtype &SearchType,
              CRef <CMSModSpecSet> Modset);
private:
    // inits mass arrays
    void x_Init(const CMSSearchSettings::TProductsearchtype &SearchType);
    // masses as doubles
    double CalcMass[kNumUniqueAA];
    // mass in scaled integer Daltons
    int IntCalcMass[kNumUniqueAA];
    // Se mass is 78.96, S is 32.066
};

///////////////////   CMassArray inline methods

inline const double * const CMassArray::GetMass(void) const 
{ 
    return CalcMass; 
}

inline const int * const CMassArray::GetIntMass(void) const
{ 
    return IntCalcMass; 
}

/////////////////// end of CMassArray inline methods


/////////////////////////////////////////////////////////////////////////////
//
//  CAA::
//
//  lookup table for AA index
//

// lookup table for reversing an AA character to AA number
class NCBI_XOMSSA_EXPORT CAA {
public:
    CAA(void);

    /**
     * return the map for translating AA char to AA number
     */
    const char * const GetMap(void) const;

private:
    char AAMap[256];
};


///////////////////    CAA inline methods

inline 
const char * const CAA::GetMap(void) const
{
    return AAMap; 
}

/////////////////// end of CAA inline methods

/**
 * contains information for a post translational modification
 * at a particular sequence site
 */

class NCBI_XOMSSA_EXPORT CMod {
public:

    /**
     * type for a site on a sequence
     */

    typedef const char * TSite;

    /**
     *  type for masses
     */
    typedef int TMass;

    /**
     * what is the type of the mod?
     */
    typedef int TEnum;

    /**
     * is the mod fixed?
     */
    typedef int TFixed;

    /**
     * default constructor
     */
    CMod(void);

    /**
     * copy constructor
     */
    CMod(const CMod &Old);

    /**
     * assignment
     */
    const CMod& operator= (const CMod& rhs);

    /**
     * reset to default values
     */
    void Reset(void);

    /**
     * Get the site position
     */
    TSite GetSite(void) const;

    /**
     * Set the site postion
     */
    TSite& SetSite(void);

    /**
     * Get the mass to be added to the precursor mass
     */
    TMass GetPrecursorDelta(void) const;

    /**
     * Set the site postion
     */
    TMass& SetPrecursorDelta(void);

    /**
     * Get the mass to be added to the product mass
     */
    TMass GetProductDelta(void) const;

    /**
     * Set the site postion
     */
    TMass& SetProductDelta(void);

    /**
     * Get mod type
     */
    TEnum GetEnum(void) const;

    /**
     * Set the mod type
     */
    TEnum& SetEnum(void);

    /**
     * Is the mod fixed?
     */
    TFixed GetFixed(void) const;

    /**
     * set mod state (1 = fixed)
     */
    TFixed& SetFixed(void);

private:
	/**
     *  the position within the peptide of a variable modification
     */
	const char *Site;

	/**
     *  the modification mass for the precursor
     */
	int PrecursorDelta;

    /**
     *  the modification mass for the product
     */
    int ProductDelta;

	/**
     *  the modification type (used for saving for output)
     */
	int ModEnum;

	/**
     *  track fixed mods, 1 == fixed
     */
	int IsFixed;
};

/**
 * default constructor
 */
inline
CMod::CMod(void)
{
    Reset();
}

/**
 * reset to default values
 */
inline
void CMod::Reset(void)
{
    Site = (const char *)-1;
    PrecursorDelta = 0;
    ProductDelta = 0;
    ModEnum = 0;
    IsFixed = 0;
}

/**
 * copy constructor
 */
inline
CMod::CMod(const CMod &Old)
{
    *this = Old;
}

/**
 * assignment
 */
inline
const CMod& CMod::operator= (const CMod& rhs)
{
    Site = rhs.Site;
    PrecursorDelta = rhs.PrecursorDelta;
    ProductDelta = rhs.ProductDelta;
    ModEnum = rhs.ModEnum;
    IsFixed = rhs.IsFixed;

    return *this;
}

/**
 * Get the site position
 */
inline
CMod::TSite CMod::GetSite(void) const
{
    return Site;
}

/**
 * Set the site postion
 */
inline
CMod::TSite& CMod::SetSite(void)
{
    return Site;
}

/**
 * Get the mass to be added to the precursor mass
 */
inline
CMod::TMass CMod::GetPrecursorDelta(void) const
{
    return PrecursorDelta;
}

/**
 * Set the site postion
 */
inline
CMod::TMass& CMod::SetPrecursorDelta(void)
{
    return PrecursorDelta;
}

/**
 * Get the mass to be added to the product mass
 */
inline
CMod::TMass CMod::GetProductDelta(void) const
{
    return ProductDelta;
}

/**
 * Set the site postion
 */
inline
CMod::TMass& CMod::SetProductDelta(void)
{
    return ProductDelta;
}

/**
 * Get mod type
 */
inline
CMod::TEnum CMod::GetEnum(void) const
{
    return ModEnum;
}

/**
 * Set the mod type
 */
inline
CMod::TEnum& CMod::SetEnum(void)
{
    return ModEnum;
}

/**
 * Is the mod fixed?
 */
inline
CMod::TFixed CMod::GetFixed(void) const
{
    return IsFixed;
}

/**
 * set mod state (1 = fixed)
 */
inline
CMod::TFixed& CMod::SetFixed(void)
{
    return IsFixed;
}



/** 
 * generic exception class for omssa
 */

class COMSSAException: EXCEPTION_VIRTUAL_BASE public CException {
    public:
    /// Error types that subsystem can generate.
    enum EErrCode {
        eMSParseException,		///< unable to parse COMSSASearch
        eMSNoMatchException,	///< unmatched sequence library
        eMSLadderNotFound	    ///< ladder not found in CLadderContainer
    };

    /// Translate from the error code value to its string representation.   
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eMSParseException: return "unable to parse COMSSASearch";
        case eMSNoMatchException: return "unmatched sequence library";
        case eMSLadderNotFound: return "ladder not found in CLadderContainer";
        default:     return CException::GetErrCodeString();
        }
    }
    
    // Standard exception boilerplate code.    
    NCBI_EXCEPTION_DEFAULT(COMSSAException, CException);
}; 


/////////////////////////////////////////////////////////////////////////////
//
//  CCleave::
//
//  Classes for cleaving sequences quickly and computing masses
//
	
typedef std::deque <int> TCleave;

class NCBI_XOMSSA_EXPORT CCleave : public CObject {
public:
    CCleave(void);

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
     * 
     * @return are we at the end of the sequence?
     */
    bool CalcAndCut(const char *SeqStart, 
                    const char *SeqEnd,  // the end, not beyond the end
                    const char **PepStart,  // return value
                    int *Masses,  // Masses, indexed by miss cleav, mods
                    int& NumMod,   // num Mods
                    int MaxNumMod, // max num mods 
                    int *EndMasses,
                    CMSMod &VariableMods,
                    CMSMod &FixedMods,
                    CMod ModList[],
                    const int *IntCalcMass,  // array of int AA masses
                    const int *PrecursorIntCalcMass, // precursor masses
                    CRef <CMSModSpecSet> &Modset,
                    int Maxproductions
                    );


    /**
     *  Check to see if we are at a cleavage point
     *  Used by CalcAndCut
     * 
     * @param iPepStart pointer to location of sequence cursor
     * @param iSeqStart points to start of the sequence
     */

    bool CheckCleave(const char *iPepStart, const char *iSeqStart);


    /**
     * is the character given one of the cleavage chars?
     * 
     * @param iPepStart position in the sequence
     * 
     */
    bool CheckCleaveChar(const char *iPepStart) const;


    void CalcMass(char SeqChar,
		  int *Masses,
		  const int *IntCalcMass
		  );
    
    void EndMass(int *Masses
		 );

    int findfirst(char* Seq, int Pos, int SeqLen);

    ///
    /// looks for non-specific ptms
    ///
    void CheckNonSpecificMods(EMSModType ModType, // the type of mod
                              CMSMod &VariableMods, // list of mods to look for
                              int& NumMod, // number of mods applied to peptide
                              int MaxNumMod,  // maximum mods for a peptide
                              CMod ModList[],  // list of mod sites
                              const char *iPepStart, // position in protein
                              bool setfixed,
                              CRef <CMSModSpecSet> &Modset
                  ); 

    ///
    /// looks for amino acid specific ptms
    ///
    void CheckAAMods(EMSModType ModType, // the type of mod
                     CMSMod &VariableMods, // list of mods to look for
                     int& NumMod, // number of mods applied to peptide
                     char SeqChar,  // the amino acid
                     int MaxNumMod,  // maximum mods for a peptide
                     CMod ModList[],  // list of mod sites
                     const char *iPepStart,  // position in protein
                     bool setfixed,
                     CRef <CMSModSpecSet> &Modset
             );

    /**
     * checks all mods for a particular type
     */
    void CheckMods(EMSModType NonSpecificIn, EMSModType Specific,
                   CMSMod &VariableMods, CMSMod &FixedMods,
				   int& NumMod, char SeqChar, int MaxNumMod,
				   CMod ModList[],
				   const char *iPepStart,
                   CRef <CMSModSpecSet> &Modset);

    /**
     * Is the enzyme really a top-down search?
     */
    bool GetTopDown(void) const;

    /**
     * Get the enzyme stop value
     */
    const char * GetStop(void) const;
    
    /**
     * Set the enzyme stop value
     */
    const char * & SetStop(void);

    /**
     * Is this a non-specific search?
     */
    bool GetNonSpecific(void) const;

    /**
      * Get the number of cleavage chars
      */
    int GetCleaveNum(void) const;

    /**
     * Get the the cleave offset, 0 = cterm, 1 = nterm
     */
    const char * GetCleaveOffset(void) const;

    /**
     * Is there n-term methionine cleavage?
     */
    bool GetNMethionine(void) const;

    /**
     * Set n-term methionine cleavage
     */
    bool& SetNMethionine(void);

protected:
    int ProtonMass; // mass of the proton
    int TermMass;  // mass of h2o
    CAA ReverseAA;

    /**
     *  where to cleave.  last two letters are in readdb format, assuming 
     * it uses the UniqueAA alphabet
     */
    char *CleaveAt;

    /**
     *  what is the cleavage offset
     */
    char *CleaveOffset;

    /**
     *  How many cleavage characters
     */
    int kCleave;

    /**
     * TopDown
     * does this signify a top-down search
     */
    bool TopDown;

    /**
     * Stop
     * Stop position for no-enzyme and semi-tryptic searches
     */
    const char *Stop;

    /**
     * Is this a non-specific search?
     */
    bool NonSpecific;

    /**
     * Should we apply the proline rule (no cleavage before proline)
     */
    bool CheckProline;

    /**
     * n-terminal methionine cleavage
     */
    bool NMethionine;
};    


///////////////////    CCleave inline methods

/**
 * is the character given one of the cleavage chars?
 * 
 * @param iPepStart position in the sequence
 * 
 */
inline
bool CCleave::CheckCleaveChar(const char *iPepStart) const
{
    int j;
    for(j = 0; j < kCleave; j++) 
        if(*(iPepStart + CleaveOffset[j]) == CleaveAt[j]) return true;
    return false;
}


/**
 *  Check to see if we are at a cleavage point
 *  Used by CalcAndCut
 * 
 * @param iPepStart pointer to location of sequence cursor
 */
inline
bool CCleave::CheckCleave(const char *iPepStart, const char *iSeqStart)
{
    // methionine cleavage
    // (allowed even if TopDown or NonSpecific)
    if(iPepStart == iSeqStart && NMethionine && *(iPepStart) == '\x0c') {
        return true;
    }

    if(TopDown) return false; // todo: methionine cleavage allowed

    if(NonSpecific) {
        if(iPepStart == GetStop()) return true;
        return false; // todo: methionine cleavage allowed
    }

    // check specific cleave amino acids
    if(CheckCleaveChar(iPepStart)) {
        if(CheckProline && *(iPepStart+1) == '\x0e' )  
            return false;  // not before proline
        return true;
    }
    return false;
}


inline
void CCleave::CalcMass(char SeqChar,
		       int *Masses,
		       const int *IntCalcMass
		       )
{
    *Masses += IntCalcMass[ReverseAA.GetMap()[SeqChar]];
}


inline
void CCleave::EndMass( int *EndMasses
		       )
{
    *EndMasses = TermMass;
}


inline
void CCleave::CheckAAMods(EMSModType ModType, CMSMod &VariableMods, int& NumMod,
                          char SeqChar, int MaxNumMod, CMod ModList[],
                          const char *iPepStart,
                          bool setfixed,
                          CRef <CMSModSpecSet> &Modset)
{
    // iterator thru mods
    CMSSearchSettings::TVariable::const_iterator iMods;
    int iChar;

    for (iMods = VariableMods.GetAAMods(ModType).begin();
        iMods !=  VariableMods.GetAAMods(ModType).end(); iMods++) {
        for (iChar = 0; iChar < Modset->GetModNumChars(*iMods); iChar++) {
            if (SeqChar == Modset->GetModChar(*iMods, iChar) && NumMod < MaxNumMod) {
                ModList[NumMod].SetSite() = iPepStart;
                ModList[NumMod].SetPrecursorDelta() = Modset->GetModMass(*iMods);
                ModList[NumMod].SetProductDelta() = Modset->GetNeutralLoss(*iMods);
                ModList[NumMod].SetEnum() = *iMods;
                if (setfixed) ModList[NumMod].SetFixed() = 1;
                else ModList[NumMod].SetFixed() = 0;
                NumMod++; 
            }
        }
    }
}


inline
void CCleave::CheckNonSpecificMods(EMSModType ModType, CMSMod &VariableMods,
                                   int& NumMod, int MaxNumMod,
                                   CMod ModList[],
                                   const char *iPepStart,
                                   bool setfixed,
                                   CRef <CMSModSpecSet> &Modset)
{
    // iterator thru mods
    CMSSearchSettings::TVariable::const_iterator iMods;

    for (iMods = VariableMods.GetAAMods(ModType).begin();
        iMods !=  VariableMods.GetAAMods(ModType).end(); iMods++) {
        if (NumMod < MaxNumMod) {
            ModList[NumMod].SetSite() = iPepStart;
            ModList[NumMod].SetPrecursorDelta() = Modset->GetModMass(*iMods);
            ModList[NumMod].SetProductDelta() = Modset->GetNeutralLoss(*iMods);
            ModList[NumMod].SetEnum() = *iMods;
            if (setfixed) ModList[NumMod].SetFixed() = 1;
            else  ModList[NumMod].SetFixed() = 0;
            NumMod++; 
        }
    }
}

inline
void CCleave::CheckMods(EMSModType NonSpecificIn, EMSModType Specific,
                        CMSMod &VariableMods, CMSMod &FixedMods,
                        int& NumMod, char SeqChar, int MaxNumMod,
                        CMod ModList[],
                        const char *iPepStart,
                        CRef <CMSModSpecSet> &Modset)
{
    // check non-specific mods
    CheckNonSpecificMods(NonSpecificIn, VariableMods, NumMod, MaxNumMod, ModList,
                         iPepStart, false, Modset);
    CheckNonSpecificMods(NonSpecificIn, FixedMods, NumMod, MaxNumMod, ModList,
                         iPepStart, true, Modset);
    // check specific mods
    CheckAAMods(Specific, VariableMods, NumMod, SeqChar, MaxNumMod, ModList, 
                iPepStart, false, Modset);
    // fix
    CheckAAMods(Specific, FixedMods, NumMod, SeqChar, MaxNumMod, ModList, 
                iPepStart, true, Modset);
}

inline
bool CCleave::GetTopDown(void) const
{
    return TopDown;
}

inline
bool CCleave::GetNonSpecific(void) const
{
    return NonSpecific;
}

inline
const char * CCleave::GetStop(void) const
{
    return Stop;
}

inline
const char * & CCleave::SetStop(void)
{
    return Stop;
}

inline
int CCleave::GetCleaveNum(void) const
{
    return kCleave;
}

inline
const char * CCleave::GetCleaveOffset(void) const
{
    return CleaveOffset;
}

inline
bool CCleave::GetNMethionine(void) const
{
    return NMethionine;
}


inline
bool& CCleave::SetNMethionine(void)
{
    return NMethionine;
}


/////////////////// end of CCleave inline methods



class NCBI_XOMSSA_EXPORT CCNBr: public CCleave {
public:
    CCNBr(void);
};


class NCBI_XOMSSA_EXPORT CFormicAcid: public CCleave {
public:
    CFormicAcid(void);
};


class NCBI_XOMSSA_EXPORT CTrypsin: public CCleave {
public:
    CTrypsin(void);
};


class NCBI_XOMSSA_EXPORT CArgC: public CCleave {
public:
    CArgC(void);
};


class NCBI_XOMSSA_EXPORT CChymotrypsin: public CCleave {
public:
    CChymotrypsin(void);
};


class NCBI_XOMSSA_EXPORT CLysC: public CCleave {
public:
    CLysC(void);
};


class NCBI_XOMSSA_EXPORT CLysCP: public CCleave {
public:
    CLysCP(void);
};


class NCBI_XOMSSA_EXPORT CPepsinA: public CCleave {
public:
    CPepsinA(void);
};


class NCBI_XOMSSA_EXPORT CTrypCNBr: public CCleave {
public:
    CTrypCNBr(void);
};


class NCBI_XOMSSA_EXPORT CTrypChymo: public CCleave {
public:
    CTrypChymo(void);
};


class NCBI_XOMSSA_EXPORT CTrypsinP: public CCleave {
public:
    CTrypsinP(void);
};


//! whole protein (no cleavage)
class NCBI_XOMSSA_EXPORT CWholeProtein: public CCleave {
public:
    CWholeProtein(void);
};


//! Asp-N, Nterm of D
class NCBI_XOMSSA_EXPORT CAspN: public CCleave {
public:
    CAspN(void);
};


//! Glu-C, Cterm of E
class NCBI_XOMSSA_EXPORT CGluC: public CCleave {
public:
    CGluC(void);
};

//! Glu-C and Asp-N
class NCBI_XOMSSA_EXPORT CGluCAspN: public CCleave {
public:
    CGluCAspN(void);
};

 
/**
 * eMSEnzymes_top_down
 * top-down search of ETD spectra
 * 
 */

class NCBI_XOMSSA_EXPORT CTopDown: public CCleave {
public:
    CTopDown(void);
};


/**
 * eMSEnzymes_semi_tryptic
 * semi tryptic search (one end of peptide has to be tryptic)
 * 
 */

class NCBI_XOMSSA_EXPORT CSemiTryptic: public CCleave {
public:
    CSemiTryptic(void);
};


/**
 * eMSEnzymes_no_enzyme
 * search without enzyme (precursor mass only)
 * 
 */

class NCBI_XOMSSA_EXPORT CNoEnzyme: public CCleave {
public:
    CNoEnzyme(void);
};


/**
 * eMSEnzymes_chymotrypsin_p
 * chymotrypsin without proline rule
 * 
 */

class NCBI_XOMSSA_EXPORT CChymoP: public CCleave {
public:
    CChymoP(void);
};

/**
 * eMSEnzymes_aspn_de
 * Asp-N that cuts at D and E
 * 
 */

class NCBI_XOMSSA_EXPORT CAspNDE: public CCleave {
public:
    CAspNDE(void);
};


/**
 * eMSEnzymes_gluc_de
 * Glu-C that cuts at D and E
 * 
 */

class NCBI_XOMSSA_EXPORT CGluCDE: public CCleave {
public:
    CGluCDE(void);
};


///
/// factory to return back object for enzyme
///

class NCBI_XOMSSA_EXPORT CCleaveFactory
{
public:
  static CRef <CCleave> CleaveFactory(const EMSEnzymes enzyme);

};


END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif
