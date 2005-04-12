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
 *   Definition for CSeqMaskerUsetArray class.
 *
 */

#ifndef C_SEQ_MASKER_USET_ARRAY_H
#define C_SEQ_MASKER_USET_ARRAY_H

#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimisc.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Unit counts container based on simple arrays.
 **/
class CSeqMaskerUsetArray
{
    public:

        /** 
         **\brief Exceptions that CSeqMaskerUsetArray might throw.
         **/
        class Exception : public CException
        {
            public:

                enum EErrCode
                {
                    eSizeOdd   /**< The size of the data array is not even. */
                };

                /**
                 **\brief Get a description string for this exception.
                 **\return C-style description string
                 **/
                virtual const char * GetErrCodeString() const;
        
                NCBI_EXCEPTION_DEFAULT( Exception, CException );
        };

        /**\brief Trivial default object constructor. */
        CSeqMaskerUsetArray() {}

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
         **\brief Add unit counts information to the container.
         **
         ** sz must be an even number. arg_unit_data is 
         ** interpreted as an array of integer pairs with
         ** the first element being a unit value and the second
         ** element being the corresponding count.
         **
         ** NOTE: CSeqMaskerUsetArray assumes ownership of
         ** the arg_unit_data and will attempt to delete[]
         ** it during destruction.
         **
         **\param arg_unit_data array of Uint4 values representing 
         **                     unit counts information
         **\param sz number of Uint4 values in arg_unit_data
         **/
        void add_info( const Uint4 * arg_unit_data, Uint4 sz );

        /**
         **\brief Lookup the count value for a given unit.
         **\param unit the target unit
         **\return the count of the given unit, or 0 if unit was not
         **        found
         **/
        Uint4 get_info( Uint4 unit ) const;

    private:

        /**\name Provide reference semantics for the class. */
        /**@{*/
        CSeqMaskerUsetArray( const CSeqMaskerUsetArray & );
        CSeqMaskerUsetArray & operator=( const CSeqMaskerUsetArray & );
        /**@}*/

        /**\internal
         **\brief One entry of unit counts data.
         **/
        struct entry
        {
            Uint4 u;    /**\internal The unit value. */ 
            Uint4 c;    /**\internal The unit count. */
        };

        Uint1 unit_size;            /**<\internal The unit size. */
        Uint4 asize;                /**<\internal The size of unit_data. */
        
        /**<\internal 
         **\brief Unit counts data. 
         **/
        AutoPtr< const entry, ArrayDeleter< const entry > > unit_data;

        friend bool operator<( const CSeqMaskerUsetArray::entry & lhs, 
                               const CSeqMaskerUsetArray::entry & rhs );
};

/**\internal
 **\brief Comparison of unit data entries.
 **
 ** Compared by the unit value.
 **
 **\param lhs left operand
 **\param rhs right operand
 **\return true if lhs.u < rhs.u; false otherwise
 **/
inline bool operator<( const CSeqMaskerUsetArray::entry & lhs, 
                       const CSeqMaskerUsetArray::entry & rhs )
{ return lhs.u < rhs.u; }

END_NCBI_SCOPE

#endif

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/04/12 13:35:34  morgulis
 * Support for binary format of unit counts file.
 *
 * ========================================================================
 */
