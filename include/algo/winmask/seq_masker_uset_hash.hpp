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
 *   Definition for CSeqMaskerUsetHash class.
 *
 */

#ifndef C_SEQ_MASKER_USET_HASH_H
#define C_SEQ_MASKER_USET_HASH_H

#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimisc.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief This class encapsulates the implementation of the hash
 **       based container for unit counts.
 **/
class CSeqMaskerUsetHash
{
    public:
        
        /**
         **\brief Exceptions that CSeqMaskerUsetHash can throw.
         **/
        class Exception : public CException
        {
            public:

                enum EErrCode
                {
                    eBadIndex   /**< Bad index into the secondary table was formed. */
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
         **/
        CSeqMaskerUsetHash() : htp( 0 ), vtp( 0 ) {}

        /**
         **\brief Add hash table information to the container.
         **\param arg_k the hash key length in bits
         **\param arg_roff the right offset of the hash key in bits
         **\param arg_bc size of the "number of collisions" field in bits
         **\param ht array containing the hash table
         **/
        void add_ht_info( Uint1 arg_k, Uint1 arg_roff, Uint1 arg_bc,
                          const Uint4 * ht );

        /**
         **\brief Add secondary table information to the container.
         **\param M size of the secondary table
         **\param vt array containing the secondary table
         **/
        void add_vt_info( Uint4 M, const Uint2 * vt );

        /**
         **\brief Look up the unit count in the data structure.
         **\param unit the unit value
         **\return the number of times the unit and its reverse complement
         **        are present in the genome
         **/
        Uint4 get_info( Uint4 unit ) const;

        /**
         **\brief Get the unit size in bases.
         **\return the unit size
         **/
        Uint1 UnitSize() const { return unit_size; }

        /**
         **\brief Set the unit size.
         **\param us the unit size in bases
         **/
        void setUnitSize( Uint1 us ) { unit_size = us; }

    private:

        /**@name Provide reference semantics for CSeqMaskerUsetHash. */
        /**@{*/
        CSeqMaskerUsetHash( const CSeqMaskerUsetHash & );
        CSeqMaskerUsetHash & operator=( const CSeqMaskerUsetHash & );
        /**@}*/

        Uint1 unit_size;    /**<\internal Unit size in bases. */

        Uint1 k;            /**<\internal Hash key size in bits. */
        Uint1 roff;         /**<\internal Right offset in bits. */
        Uint1 bc;           /**<\internal Bit size of "number of collisions" field. */
        Uint4 M;            /**<\internal Size of the secondary table (in 2-byte words). */
        Uint4 cmask;        /**<\internal Mask used to extract the "number of collisions" field. */

        AutoPtr< const Uint4, ArrayDeleter< const Uint4 > > ht; /**<\internal The hash table. */
        AutoPtr< const Uint2, ArrayDeleter< const Uint2 > > vt; /**<\internal The secondary table. */

        const Uint4 * htp;  /**<\internal The actual pointer to the hash table. */
        const Uint2 * vtp;  /**<\internal The actual pointer to the secondary table. */
};

END_NCBI_SCOPE

#endif

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/05/02 14:27:46  morgulis
 * Implemented hash table based unit counts formats.
 *
 * ========================================================================
 */
