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
 *   Header file for CSeqMaskerScore class.
 *
 */

#ifndef C_SEQ_MASKER_SCORE_H
#define C_SEQ_MASKER_SCORE_H

#include <corelib/ncbitype.h>

#include <algo/winmask/seq_masker.hpp>


BEGIN_NCBI_SCOPE

class CSeqMaskerWindow;

/**
 **\brief Abstract base class for score function objects.
 **
 ** The specific classes should be derived to provided
 ** different methods of computing a window score. It uses
 ** CSeqMaskerWindow interface to get access to information
 ** about units of the current window.
 **
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerScore
{
public:

    /**
     **\brief Object constructor
     **
     **\param arg_lstat determines which unit score statistics
     **                 should be used
     **
     **/
    CSeqMaskerScore( const CSeqMasker::LStat & arg_lstat ) 
        : window( 0 ), lstat( arg_lstat ) {}

    /**
     **\brief Object destructor
     **/
    virtual ~CSeqMaskerScore() {}

    /**
     **\brief Get the score of the current window.
     **
     **\return the score of the current window
     **
     **/
    virtual Uint4 operator()() = 0;

    /**
     **\brief Window advancement notification.
     **
     ** If the score function object has to perform some
     ** action in anticipation of window position advancement
     ** then PreAdvance() interface has to be called just
     ** prior to advancing the window with the argument
     ** indicating by how many base positions the window
     ** is going to be moved.
     **
     **\param step value of window advancement in bases
     **
     **/
    virtual void PreAdvance( Uint4 step ) = 0;

    /**
     **\brief Window advancement notification.
     **
     ** If the score function object has to perform some
     ** action after the window position advancement then
     ** PostAdvance() interface has to be called right
     ** after the advancement of the window with the argument
     ** indicating by how many base positions the window
     ** has been moved.
     **
     **\param step value of window advancement in bases
     **
     **/
    virtual void PostAdvance( Uint4 step ) = 0;

    /**
     **\brief Set the window object that should be used for
     **       score computation.
     **
     **\param new_window the object implementing CSeqMaskerWindow
     **                  window access interface
     **
     **/
    void SetWindow( const CSeqMaskerWindow & new_window )
    { window = &new_window; Init(); }

protected:

    /**
     **\brief Initialize the object.
     **
     ** Initialization should follow the call to SetWindow()
     ** and should take care of any computations necessary to
     ** initialize the score object.
     **
     **/
    virtual void Init() = 0;

    /**
     **\brief Points to the window information object.
     **/
    const CSeqMaskerWindow * window;

    /**
     **\brief Unit score statistics that should be used by
     **       the score function object.
     **/
    const CSeqMasker::LStat & lstat;
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

