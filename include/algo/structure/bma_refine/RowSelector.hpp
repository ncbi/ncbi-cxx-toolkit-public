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
#include <util/random_gen.hpp>
#include <algo/structure/struct_util/struct_util.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(struct_util);

BEGIN_SCOPE(align_refine)

// class for standalone application
class CRowSelector
{
public:

    static const unsigned int INVALID_ROW;

    CRowSelector(unsigned int nRows, bool unique  = true, unsigned int seed = 0);
    CRowSelector(unsigned int nRows, unsigned int nTotal, bool unique = true, unsigned int seed = 0);
    CRowSelector(const AlignmentUtility* au, bool unique = true, unsigned int seed = 0);
    CRowSelector(const AlignmentUtility* au, unsigned int nTotal, bool unique = true, unsigned int seed = 0);

    bool HasNext(void);           // if extend this class, make HasNext virtual
    unsigned int  GetNext(void);  // if extend this class, make GetNext virtual
    void Reset();  // start from begining; keeps the same sequence
    void Shuffle(bool clearExcludedRows = false);// using the same parameters, make a new sequence & reset the selector

    unsigned int GetNumRows(void) const;  
    unsigned int GetNumSelected(void) const;
    unsigned int GetSequenceSize(void) const;

    //  Rows corresponding to structures in an alignment are excluded from the possible 
    //  selections; only has an effect if m_au is non-Null.  Also adjusts m_nSelections.
    void ExcludeStructureRows();  
    bool ExcludeRow(unsigned int row);
    unsigned int  GetNumExcluded() const;

    //  By default, print the whole sequence.  Start/end are one-based.
    string PrintSequence(unsigned int first = 0, unsigned int last = 0, bool sorted = false) const;

    //  Print the entire state (except for the AlignmentUtility object)
    string Print() const;

    virtual ~CRowSelector() {};

private:

    void Init(unsigned int nSelections, unsigned int seed);  // called only from c-tors
    void SetSequence();
    void ClearExclusions();   //  also resets m_nSelections to original value

    unsigned int   m_nRows;       //  number of possible values
    unsigned int   m_nSelections; //  total number of selections allowed; equals m_nRows unless specified
                         //  or there are excluded row numbers.
    unsigned int   m_nSelected;   //  counts how many selections have been requested by clients of this instance

    //  optionally, based the row selector on properties of a given alignment
    const AlignmentUtility* m_au;  

    bool  m_unique;      //  flag:  true indicates that when possible, integers in the sequence are unique

    CRandom* m_rng;  //  random number generator; must be initialized in ctor


    unsigned int   m_origNSelections; //  number of selections allowed as specified in initial construction;
                             //  may use if want to unset some exclusions

    vector<unsigned int> m_sequence;  //  the order of rows selected (created in ctor or by Shuffle)
    vector<unsigned int> m_excluded;  //  values in [0, m_nRows) not allowed to be selected

};

END_SCOPE(align_refine)

#endif // AR_ROW_SELECTOR__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2005/06/28 13:45:25  lanczyck
* block multiple alignment refiner code from internal/structure/align_refine
*
* Revision 1.6  2004/11/02 23:52:08  lanczyck
* remove CCdCore in favor of CCdd & AlignmentUtility anticipating deployment w/i cn3d
*
* Revision 1.5  2004/10/04 19:11:44  lanczyck
* add shift analysis; switch to use of cd_utils vs my own class; rename output CDs w/ trial number included; default T = 3e6
*
* Revision 1.4  2004/07/27 16:53:35  lanczyck
* give RowSelector a CRandom member; change seed parameter handling in main app
*
* Revision 1.3  2004/07/21 22:11:31  lanczyck
* allow for row exclusions; not yet used in the main app for blocking LOO for structure rows
*
* Revision 1.2  2004/06/22 19:53:01  lanczyck
* fix message reporting; add a couple cmd options
*
* Revision 1.1  2004/06/21 20:09:02  lanczyck
* Initial version w/ LOO & scoring vs. PSSM
*
*
*/
