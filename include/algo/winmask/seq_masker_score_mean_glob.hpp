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
 *   Header file for CSeqMaskerScoreMeanGlob class.
 *
 */

#ifndef C_SEQ_MASKER_SCORE_MEAN_GLOB_H
#define C_SEQ_MASKER_SCORE_MEAN_GLOB_H

#include <algo/winmask/seq_masker_score.hpp>

BEGIN_NCBI_SCOPE


/**
 **\brief Average unit score form the start of the sequence
 **       to the end of current window.
 **
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerScoreMeanGlob : public CSeqMaskerScore
{
public:

    /**
     **\brief Object constructor.
     **
     **\param arg_lstat unit length statistics used to unitalized 
     **                 the base class instance
     **
     **/
    CSeqMaskerScoreMeanGlob( const CSeqMasker::LStat & arg_lstat ) 
        : CSeqMaskerScore( arg_lstat ), num( 0 ), avg( 0.0 ) {}

    /**
     **\brief Object destructor.
     **
     **/
    virtual ~CSeqMaskerScoreMeanGlob() {}

    /**
     **\brief Access the current value of the score.
     **
     **\return the current average unit score over the interval
     **        starting at the start of the sequence and ending
     **        at the end of the current window
     **
     **/
    virtual Uint4 operator()() { return static_cast< Uint4 >( avg ); }

    /**
     **\brief Preprocessing before the window advancement.
     **
     **\param step the window is going to advance by that many bases
     **
     **/
    virtual void PreAdvance( Uint4 step ) {}

    /**
     **\brief Postprocessing after the window advancement.
     **
     **\param step the window has advanced by that many bases.
     **
     **/
    virtual void PostAdvance( Uint4 step );

protected:

    /**
     **\brief Score function initialization.
     **
     ** This method is called by SetWindow() method of the base
     ** class to precompute the average score of the initial window.
     **
     **/
    virtual void Init();

private:

    /**\internal
     **\brief Update the current value of the average assuming
     **       that the next unit in the sequence is unit.
     **
     **\param unit the next unit in the sequence
     **
     **/
    void update( Uint4 unit );

    /**\internal
     **\brief The total number of units already accounted for.
     **
     **/
    Uint4 num;

    /**\internal
     **\brief The current value of average unit score.
     **
     **/
    double avg;
};

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

#endif

