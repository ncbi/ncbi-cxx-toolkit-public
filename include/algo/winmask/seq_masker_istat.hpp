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
 *   Definition for CSeqMaskerIstat class.
 *
 */

#ifndef C_SEQ_MASKER_ISTAT_H
#define C_SEQ_MASKER_ISTAT_H

#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>

#include <algo/winmask/seq_masker_window.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Defines an interface for accessing the unit counts information.
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerIstat : public CObject
{
    public:

        /**
         **\brief Object constructor.
         **\param arg_threshold the value of t_threshold to use instead of
         **                     the one supplied in the unit counts file
         **\param arg_textend the value of t_textend to use instead of
         **                     the one supplied in the unit counts file
         **\param arg_max_count the value of t_high to use instead of
         **                     the one supplied in the unit counts file
         **\param arg_use_max_count the count to use if the unit count is
         **                         greater than t_high
         **\param arg_min_count the value of t_low to use instead of
         **                     the one supplied in the unit counts file
         **\param arg_use_min_count the count to use if the unit count is
         **                         less than t_low
         **/
        explicit CSeqMaskerIstat(   Uint4 arg_threshold,
                                    Uint4 arg_textend,
                                    Uint4 arg_max_count,
                                    Uint4 arg_use_max_count,
                                    Uint4 arg_min_count,
                                    Uint4 arg_use_min_count )
            :   threshold( arg_threshold ),
                textend( arg_textend ),
                max_count( arg_max_count ),
                use_max_count( arg_use_max_count ),
                min_count( arg_min_count ),
                use_min_count( arg_use_min_count ),
                ambig_unit( 0 )
        {}

        /**
         **\brief Object destructor.
         **/
        virtual ~CSeqMaskerIstat() {}

        /**
         **\brief Look up the count value of a given unit.
         **\param unit the target unit
         **\return the count of the unit
         **/
        Uint4 operator[]( Uint4 unit ) const
        { return at( unit ); }

        /**
         **\brief Get the unit size.
         **\return the unit size
         **/
        virtual Uint1 UnitSize() const = 0;

        /**
         **\brief Get the value of the unit used to represent an ambuguity.
         **\return ambiguity unit value
         **/
        const CSeqMaskerWindow::TUnit AmbigUnit() const
        { return ambig_unit; }

        /**
         **\brief Get the value of T_threshold.
         **\return T_threshold value
         **/
        Uint4 get_threshold() const { return threshold; }

        /**
         **\brief Get the value of T_extend.
         **\return T_extend value
         **/
        Uint4 get_textend() const { return textend; }

    protected:

        /**
         **\brief Get the unit count of a given unit.
         **
         ** Derived classes should override this function
         ** to provide access to the unit counts.
         **
         **\param unit the unit value being looked up
         **\return count corrseponding to unit
         **/
        virtual Uint4 at( Uint4 unit ) const = 0;

        /**
         **\brief Set the value of T_threshold.
         **\param arg_threshold new T_threshold value
         **/
        void set_threshold( Uint4 arg_threshold )
        { threshold = arg_threshold; }

        /**
         **\brief Set the value of T_extend.
         **\param arg_textend new T_extend value
         **/
        void set_textend( Uint4 arg_textend )
        { textend = arg_textend; }

        /**
         **\brief Get the current value of T_high.
         **\return current T_high value
         **/
        Uint4 get_max_count() const { return max_count; }

        /**
         **\brief Set the value of T_high.
         **\param arg_max_count new T_high value
         **/
        void set_max_count( Uint4 arg_max_count )
        { max_count = arg_max_count; }

        /**
         **\brief Get the count value for units with actual counts 
         **       above T_high.
         **\return value to use for units with count > T_high
         **/
        Uint4 get_use_max_count() const { return use_max_count; }

        /**
         **\brief Set the count value for units with actual counts
         **       above T_high.
         **\param arg_use_max_count new value to use for units with 
         **                         counts > T_high
         **/
        void set_use_max_count( Uint4 arg_use_max_count )
        { use_max_count = arg_use_max_count; }

        /**
         **\brief Get the value of T_low.
         **\return current T_low value
         **/
        Uint4 get_min_count() const { return min_count; }

        /**
         **\brief Set the value of T_low.
         **\param arg_min_count new T_low value
         **/
        void set_min_count( Uint4 arg_min_count )
        { min_count = arg_min_count; }

        /**
         **\brief Get the count value for units with actual counts
         **       below T_low.
         **\return value to use for units with counts < T_low
         **/
        Uint4 get_use_min_count() const { return use_min_count; }

        /**
         **\brief Set the count value for units with actual counts
         **       below T_low.
         **\param arg_use_min_count new value to use for units with
         **                         counts < T_low
         **/
        void set_use_min_count( Uint4 arg_use_min_count )
        { use_min_count = arg_use_min_count; }

        /**
         **\brief Set the unit size.
         **\param arg_unit_size new unit size value
         **/
        void set_unit_size( Uint1 arg_unit_size )
        { unit_size = arg_unit_size; }

        /**
         **\brief Set the ambiguity unit value
         **\param arg_ambig_unit new ambiguity unit
         **/
        void set_ambig_unit( 
            const CSeqMaskerWindow::TUnit & arg_ambig_unit )
        { ambig_unit = arg_ambig_unit; }

    private:

        /**\name Provide reference semantics for CSeqMaskerOstat. */
        /**@{*/
        CSeqMaskerIstat( const CSeqMaskerIstat & );
        CSeqMaskerIstat & operator=( const CSeqMaskerIstat & );
        /**@}*/

        Uint4 threshold;        /**<\internal T_threshold */
        Uint4 textend;          /**<\internal T_extend */
        Uint4 max_count;        /**<\internal T_high */
        Uint4 use_max_count;    /**<\internal Count to use for units with actual count > T_high. */
        Uint4 min_count;        /**<\internal T_low */
        Uint4 use_min_count;    /**<\internal Count to use for units with actual count < T_low. */
        Uint1 unit_size;        /**<\internal The unit size. */

        CSeqMaskerWindow::TUnit ambig_unit; /**<\internal Unit value to represent ambiguities. */
};

END_NCBI_SCOPE

#endif

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/04/04 14:28:46  morgulis
 * Decoupled reading and accessing unit counts information from seq_masker
 * core functionality and changed it to be able to support several unit
 * counts file formats.
 *
 * ========================================================================
 */
