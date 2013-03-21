/*  $Id$
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
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Chris Lanczycki
*
* File Description:
*      Given some integer N (in this context, number of rows from an alignment), build
*      a sequence of psuedo-random integers in the half-open range [0, N).
*      By default, each integer is used once unless request a sequence w/ > N 
*      elements.  Sequence of integers is precomputed at instantiation, and may
*      be reset or shuffled thereafter.
*
* ===========================================================================
*/

#ifndef AR_ROW_SELECTOR__HPP
#define AR_ROW_SELECTOR__HPP

#include <string>
#include <vector>
#include <corelib/ncbistd.hpp>
#include <algo/structure/struct_util/struct_util.hpp>

BEGIN_NCBI_SCOPE

class CRandom;

END_NCBI_SCOPE

USING_NCBI_SCOPE;
USING_SCOPE(struct_util);

BEGIN_SCOPE(align_refine)

// pure virtual base class;  SetSequence() must be overloaded in subclasses.
class NCBI_BMAREFINE_EXPORT CRowSelector
{
public:

    static const unsigned int INVALID_ROW;

    CRowSelector(unsigned int nRows, bool unique);
    CRowSelector(unsigned int nRows, unsigned int nTotal, bool unique);

    bool HasNext(void);           // if extend this class, make HasNext virtual
    unsigned int  GetNext(void);  // if extend this class, make GetNext virtual
    void Reset();  // start from begining; keeps the same sequence

//    virtual void Shuffle(bool clearExcludedRows = false);// using the same parameters, make a new sequence & reset the selector

    unsigned int GetNumRows(void) const;  
    unsigned int GetNumSelected(void) const;
    unsigned int GetSequenceSize(void) const;

    bool ExcludeRow(unsigned int row);
    unsigned int  GetNumExcluded() const;

    //  By default, print the whole sequence.  Start/end are one-based.
    string PrintSequence(unsigned int first = 0, unsigned int last = 0, bool sorted = false) const;

    //  Print the entire state (except for the AlignmentUtility object)
    virtual string Print() const;

    virtual ~CRowSelector() {};

protected:

    //  Determine the order in which rows will be selected.
    virtual void SetSequence() = 0;


    void Init(unsigned int nRows, unsigned int nSelections);
    void ClearExclusions();   //  also resets m_nSelections to original value



    bool  m_unique;      //  flag:  true indicates that when possible, integers in the sequence are unique

    unsigned int   m_nRows;       //  number of possible values
    unsigned int   m_nSelections; //  total number of selections allowed; equals m_nRows unless specified
                         //  or there are excluded row numbers.
    unsigned int   m_nSelected;       //  counts how many selections have been requested by clients
    unsigned int   m_origNSelections; //  number of selections allowed as specified in initial construction;
                             //  may use if want to unset some exclusions

    vector<unsigned int> m_sequence;  //  the order of rows selected (created in ctor or by Shuffle)
    vector<unsigned int> m_excluded;  //  values in [0, m_nRows) not allowed to be selected

};


class NCBI_BMAREFINE_EXPORT CRandomRowSelector : public CRowSelector
{
public:

    CRandomRowSelector(unsigned int nRows, bool unique  = true, unsigned int seed = 0);
    CRandomRowSelector(unsigned int nRows, unsigned int nTotal, bool unique = true, unsigned int seed = 0);

    // using the same parameters, make a new sequence & reset the selector
    void Shuffle(bool clearExcludedRows = false);

    //  Print the entire state (except for the AlignmentUtility object)
    //virtual string Print() const;

    virtual ~CRandomRowSelector();

private:

    //  Manditory overload...
    virtual void SetSequence();

    void InitRNG(unsigned int seed);  // called only from c-tors

    CRandom* m_rng;  //  random number generator; must be initialized in ctor
};


class NCBI_BMAREFINE_EXPORT CAlignmentBasedRowSelector : public CRowSelector
{
public:

    CAlignmentBasedRowSelector(const AlignmentUtility* au, bool unique, bool bestToWorst);
    //  nTotal is constrained to be no more than the number of rows in 'au'.
    CAlignmentBasedRowSelector(const AlignmentUtility* au, unsigned int nTotal, bool unique, bool bestToWorst);

    // *** Does this method make sense here??? ***
    // using the same parameters, make a new sequence & reset the selector  
    //  void Shuffle(bool clearExcludedRows = false);

//    void ExcludeStructureRows(bool skipMaster, vector<unsigned int>* excludedStructureRows = NULL);  

    //  Print the entire state (except for the AlignmentUtility object)
    //virtual string Print() const;

    //  Replace the m_au object with a clone of the newAU.  Recompute a new order and score->row mapping.
    //  Any existing row number exclusions are still respected when possible.
    bool Update(const AlignmentUtility* newAU, unsigned int nTotal = 0, bool bestToWorst = false);

    virtual ~CAlignmentBasedRowSelector() { delete m_au;}

private:

    //  Manditory overload...
    virtual void SetSequence();

    void InitAU(const AlignmentUtility* au, unsigned int nSelections);  // if zero is passed, # selections == # rows in m_au.

    //  the alignment on which selection order is based; class has ownership of this object
    AlignmentUtility* m_au;  

    typedef multimap<double, unsigned int> ScoreMap;
    typedef ScoreMap::iterator ScoreMapIt;
    typedef ScoreMap::reverse_iterator ScoreMapRit;
    typedef ScoreMap::value_type ScoreMapVT;

    //  Maintain a map of scores to row numbers in the AU.  The flag m_sortBestToWorst tells if 
    //  the selection order is in terms of decreasing (true) or increasing (false) score.
    bool m_sortBestToWorst;
    ScoreMap m_scoresToRow;
};


END_SCOPE(align_refine)

#endif // AR_ROW_SELECTOR__HPP
