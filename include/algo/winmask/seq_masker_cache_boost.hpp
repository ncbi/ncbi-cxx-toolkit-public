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
 *   CSeqMaskerCacheBoost class definition.
 *
 */

#ifndef C_SEQ_MASKER_CACHE_BOOST_H
#define C_SEQ_MASKER_CACHE_BOOST_H

#include <corelib/ncbiobj.hpp>

#include <algo/winmask/seq_masker_istat.hpp>
#include <algo/winmask/seq_masker_window.hpp>

BEGIN_NCBI_SCOPE

/**\brief Interface to the bit array used to check if the score of a unit is
 **       below t_extend.
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerCacheBoost
{
    public:

        /**\brief Object constructor.
         **\param window will advance the window if runs of low-value units
         **              are found
         **\param od pointer to the data structure containing the bit array
         **/
        CSeqMaskerCacheBoost( CSeqMaskerWindow & window,
                              const CSeqMaskerIstat::optimization_data * od )
            : window_( window ), od_( od ), last_checked_( 0 )
        { nu_ = window_.NumUnits(); }

        /**\brief Check if the current state of the window and advance.
         **
         ** If the current window has all units below the t_extend, then advance
         ** it until the above condition does not hold true.
         **
         **\return true if the end of the sequence has been reached; 
         **        false otherwise
         **/
        bool Check();

    private:

        /**\internal
         **\brief Type representing an Nmer.
         **/
        typedef CSeqMaskerWindow::TUnit TUnit;

        /**\internal
         **\brief Get the bit value corresponding to the given Nmer value.
         **\param pos the Nmer value
         **\return the bit value corresponding to pos
         **/
        Uint1 bit_at( TUnit pos ) const;

        /**\internal
         **\brief Check if all units of the window are below t_extend.
         **\return true if the above condition holds; false otherwise
         **/
        bool full_check() const;
        
        CSeqMaskerWindow & window_; /**<\internal Reference to the window object. */
        const CSeqMaskerIstat::optimization_data * od_; /**<\internal Structure containing the bit array. */

        TSeqPos last_checked_;  /**<\internal Last window state for which the check was done. */
        Uint8 nu_;  /**<\internal Number of units in a window. */
};

END_NCBI_SCOPE

#endif
