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
 *   Header file for CSeqMaskerWindowPattern class.
 *
 */

#ifndef C_SEQ_MASKER_WINDOW_PATTERN_H
#define C_SEQ_MASKER_WINDOW_PATTERN_H

#include <algo/winmask/seq_masker_window.hpp>

BEGIN_NCBI_SCOPE


/**
 **\brief Window iterator used for discontiguous units.
 **
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerWindowPattern : public CSeqMaskerWindow
{
public:

    /**
     **\brief Object constructor.
     **
     **\param arg_data the base sequence
     **\param arg_unit_size the value of the unit size to use
     **\param arg_window_size the value of the window size to use
     **\param window_step the number of bases by which the window
     **                   advances when operator++() is applied
     **\param arg_pattern pattern to construct discontiguous units
     **\param arg_unit_step the distance between consequtive units
     **                     in a window
     **
     **/
    CSeqMaskerWindowPattern( const string & arg_data, 
                             Uint1 arg_unit_size, Uint1 arg_window_size,
                             Uint4 window_step, Uint4 arg_pattern,
                             Uint1 arg_unit_step = 1 );

    /**
     **\brief Object destructor.
     **
     **/
    virtual ~CSeqMaskerWindowPattern() {}

protected:

    /**
     **\brief Slide the window by the given number of bases.
     **
     **\param step the number of bases by which the window should
     **            slide
     **
     **/
    virtual void Advance( Uint4 step );

    /**
     **\brief Return the compressed value of discontiguous unit
     **       starting at the given position.
     **
     **\param ustart starting position of the unit
     **\param result the value of the discontiguous unit
     **\return true, if result is valid, i.e. the unit does not
     **        contain ambiguities at unmasked positions; false
     **        otherwise
     **
     **/
    bool MakeUnit( Uint4 ustart, t_unit & result ) const;

    /**\internal
     **\brief Fill the array of units for a window that starts
     **       at the given position.
     **
     **\param winstart the start of the window
     **
     **/
    void FillWindow( Uint4 winstart );

private:

    /**\internal
     **\brief Pattern to construct discontiguous units.
     **/
    Uint4 pattern;
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

