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
 *   Header file for CSeqMaskerWindow class.
 *
 */

#ifndef C_SEQ_MASKER_WINDOW_H
#define C_SEQ_MASKER_WINDOW_H

#include <vector>

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Sliding window skipping over the ambiguities.
 **
 ** This class represents a window consisting totally of
 ** unambiguous bases. It provides access to each 
 ** of the window units and operators for sliding the
 ** window forward.
 **
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerWindow
{
public:

    /**
     **\brief Integer type used to represent units within a window.
     **
     **/
    typedef Uint4 t_unit;

    /**
     **\brief type representing an array of consecutive units.
     **
     **/
    typedef vector< t_unit > t_units;

    /**
     **\brief Table used to translate bases from iupacna to 
     **       ncbi2na format.
     **
     **/
    static Uint1 LOOKUP[];

    /**
     **\brief Object constructor.
     **
     **\param arg_data the base sequence
     **\param arg_unit_size the value of the unit size to use
     **\param arg_window_size the value of the window size to use
     **\param window_step the number of bases by which the window
     **                   advances when operator++() is applied
     **\param unit_step the number of bases between consequtive
     **                 units in a window
     **
     **/
    CSeqMaskerWindow( const string & arg_data, 
                      Uint1 arg_unit_size, Uint1 arg_window_size,
                      Uint4 window_step, Uint1 unit_step = 1 );

    /**
     **\brief Object destructor.
     **
     **/
    virtual ~CSeqMaskerWindow() {}

    /**
     **\brief Check if the end of the sequence has been reached.
     **
     **\return true if not at the end of the sequence; 
     **        false otherwise
     **
     **/
    operator bool() const { return state; }

    /**
     **\brief Advance the window.
     **
     ** The window will slide by the predefined number of bases.
     ** If the result contains ambiguities it will slide further
     ** by the minimal number of bases such that the resulting
     ** window does not have ambiguities.
     **
     **/
    void operator++() { Advance( window_step ); }

    /**
     **\brief Access units of the current window by index.
     **
     **\param index which unit to access
     **\return the value of the requested unit
     **
     **/
    t_unit operator[]( Uint1 index ) const
    {
        return first_unit + index < NumUnits() ? 
            units[first_unit + index] :
            units[first_unit + index - NumUnits()];
    }

    /**
     **\brief Get the current starting position of the window.
     **
     **\return the offset of the start of the window in the sequence
     **
     **/
    Uint4 Start() const { return start; }

    /**
     **\brief Get the current ending position of the window.
     **
     **\return the offset of the end of the window in the sequence
     **
     **/
    Uint4 End() const { return end; }

    /**
     **\brief Get the current value of the window step.
     **
     **\return the least number of bases by which the window slides
     **        when operator++() is applied
     **
     **/
    Uint4 Step() const { return window_step; }

    /**
     **\brief Get the current value of the unit step.
     **
     **\return the distance between any 2 consequtive units within
     **        a window
     **
     **/
    Uint1 UnitStep() const { return unit_step; }

    /**
     **\brief Get the number of units in a window.
     **
     **\return the number of units in a window (usually is equal
     **        to window_size - unit_size + 1)
     **
     **/
    Uint1 NumUnits() const{ return (window_size - unit_size)/unit_step + 1; }

protected:

    /**
     **\brief Slide the window by the given number of bases.
     **
     **\param step the number of bases by which the window should
     **            slide
     **
     **/
    virtual void Advance( Uint4 step );

    const string & data;        /**< The sequence data in iupacna format. */
    bool state;             /**< true, if the end of the sequence has not been reached. */
    Uint1 unit_size;            /**< The unit size. */
    Uint1 unit_step;            /**< The distance between consequtive units within a window. */
    Uint1 window_size;          /**< The window size. */
    Uint4 window_step;          /**< The amount of bases by which the window advances under operator++() */
    Uint4 start;            /**< The start of the current window. */
    Uint4 end;              /**< The end if the current window. */
    t_units::size_type first_unit;  /**< The position in the array of units of the first unit of the current window. */
    t_units units;          /**< The array of units. */
    t_unit unit_mask;           /**< The mask to use when accessing the integer value of a unit. */

private:

    /**\internal
     **\brief Fill the array of units for a window that starts
     **       at the given position.
     **
     **\param winstart the start of the window
     **
     **/
    void FillWindow( Uint4 winstart );
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

