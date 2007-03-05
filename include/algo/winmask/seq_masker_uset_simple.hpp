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
 *   Definition for CSeqMaskerUsetSimple class.
 *
 */

#ifndef C_SEQ_MASKER_USET_SIMPLE_H
#define C_SEQ_MASKER_USET_SIMPLE_H

#include <vector>

#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief This class implements simple, unoptimized unit counts container.
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerUsetSimple
{
    public:

        /** 
         **\brief Exceptions that CSeqMaskerUsetSimple might throw.
         **/
        class Exception : public CException
        {
            public:

                enum EErrCode
                {
                    eBadOrder,      /**< Units are being appended out of order. */
                    eSizeMismatch   /**< When adding data in bulk (using 
                                         add_info( vector, vector )), parameter 
                                         vectors sizes are not equal */
                };

                /**
                 **\brief Get a description string for this exception.
                 **\return C-style description string
                 **/
                virtual const char * GetErrCodeString() const;
        
                NCBI_EXCEPTION_DEFAULT( Exception, CException );
        };

        /**
         **\brief Object constructor.
         **\param arg_unit_size the unit size
         **/
        CSeqMaskerUsetSimple( Uint1 arg_unit_size = 15 ) 
            : unit_size( arg_unit_size )
        {}

        /**
         **\brief Get the unit size.
         **\return the unit size
         **/
        Uint1 get_unit_size() const { return unit_size; }

        /**
         **\brief Set the unit size.
         **\param arg_unit_size the new value of unit size
         **/
        void set_unit_size( Uint1 arg_unit_size )
        { unit_size = arg_unit_size; }

        /**
         **\brief Add information about the given unit.
         **
         ** When this method is called multiple times, units should be
         ** in ascending order.
         **
         **\param unit the target unit
         **\param count number of times unit and its reverse complement
         **             appear in the genome
         **/
        void add_info( Uint4 unit, Uint4 count );

        /**
         **\brief Lookup the count value for a given unit.
         **\param unit the target unit
         **\return the count of the given unit, or 0 if unit was not
         **        found
         **/
        Uint4 get_info( Uint4 unit ) const;

    private:

        Uint1 unit_size;        /**<\internal The unit size. */

        vector< Uint4 > units;  /**<\internal The vector of units. */
        vector< Uint4 > counts; /**<\internal The vector of counts corresponding to units. */
};

END_NCBI_SCOPE

#endif
