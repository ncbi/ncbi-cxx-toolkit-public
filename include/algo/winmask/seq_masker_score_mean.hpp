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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   Header file for CSeqMaskerScoreMean class.
 *
 */

#ifndef C_SEQ_MASKER_SCORE_MEAN_H
#define C_SEQ_MASKER_SCORE_MEAN_H

#include <corelib/ncbitype.h>

#include <algo/winmask/seq_masker_score.hpp>


BEGIN_NCBI_SCOPE

/**
 **\brief Score function object computing mean of unit in a window.
 **
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerScoreMean : public CSeqMaskerScore
{
public:

    /**
     **\brief Object constructor.
     **
     **\param lstat unit score statistics; forwarded to the base class
     **             constructor
     **
     **/
    CSeqMaskerScoreMean( const CSeqMasker::LStat & lstat );

    /**
     **\brief Object destructor.
     **
     **/
    virtual ~CSeqMaskerScoreMean() {}

    /**
     **\brief Access the score of the current window.
     **
     ** The score is computed by averaging the scores of all units
     ** in the current window.
     **
     **\return the score of the current window
     **
     **/
    virtual Uint4 operator()(); 

    /**
     **\brief Preprocessing before window advancement.
     **
     **\param step window will shift by this many bases
     **
     **/
    virtual void PreAdvance( Uint4 step );

    /**
     **\brief Postprocessing after window advancement.
     **
     **\param step window has shifted by this many bases
     **
     **/
    virtual void PostAdvance( Uint4 step );

protected:

    /**
     **\brief Object initialization.
     **
     ** After the window object is set this function is called
     ** to precompute the score value for the initial window 
     ** of the sequence.
     **
     **/
    virtual void Init();

private:

    /**\internal
     **\brief Update the sum and scores array for "bad" windows.
     **/
    void FillScores();

    /**\internal
     **\brief The current total of unit scores in a window.
     **/
    Uint4 sum;

    /**\internal
     **\brief The starting position of the current window.
     **/
    Uint4 start;

    /**\internal
     **\brief The number of units in a window.
     **/
    Uint4 num;
 
    /**\internal
     **\brief Circular array of num elements containing scores of
     **       units in the current window.
     **/
    vector< Uint4 > scores;

    /**\internal
     **\brief Logical start of the scores array.
     **/
    Uint4 * scores_start;
};

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.3  2005/02/25 21:09:18  morgulis
 * 1. Reduced the number of binary searches by the factor of 2 by locally
 *    caching some search results.
 * 2. Automatically compute -lowscore value if it is not specified on the
 *    command line during the counts generation pass.
 *
 * Revision 1.2  2005/02/12 19:58:03  dicuccio
 * Corrected file type issues introduced by CVS (trailing return).  Updated
 * typedef names to match C++ coding standard.
 *
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

#endif

