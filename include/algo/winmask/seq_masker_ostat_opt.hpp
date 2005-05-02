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
 *   Definition of CSeqMaskerOStatOpt class.
 *
 */

#ifndef C_SEQ_MASKER_OSTAT_OPT_H
#define C_SEQ_MASKER_OSTAT_OPT_H

#include <corelib/ncbistre.hpp>

#include <vector>

#include <algo/winmask/seq_masker_ostat.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Class responsible for collecting unit counts statistics and
 **       representing it in optimized hash-based format.
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerOstatOpt : public CSeqMaskerOstat
{
    public:

        /** 
         **\brief Exceptions that CSeqMaskerOstatOpt might throw.
         **/
        class Exception : public CException
        {
            public:

                enum EErrCode
                {
                    eMemory     /**< Memory allocation problem. */
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
         **\param os output stream object, forwarded to CSeqMaskerOstream base
         **\param sz requested size of the unit counts file in megabytes
         **/
        explicit CSeqMaskerOstatOpt( CNcbiOstream & os, Uint2 sz );

        /**
         **\brief Object destructor.
         **/
        virtual ~CSeqMaskerOstatOpt() {}

    protected:

        /**
         **\brief Parameters of the optimized data structure.
         **/
        struct params
        {
            Uint4 M;    /**< Number of units that have a collision. */
            Uint1 k;    /**< The size of the hash key in bits. */
            Uint1 roff; /**< Right offset of the hash key in bits. */
            Uint1 bc;   /**< Size of the collisions field in the table in bits. */
            Uint4 * ht; /**< Hash table. */
            Uint2 * vt; /**< Secondary counts table. */
        };

        /**
         **\brief Dump the unit counts data to the output stream according
         **       to the requested format.
         **
         ** Derived classes should override this function to format the data.
         **
         **\param p data structure parameters
         **/
        virtual void write_out( const params & p ) const = 0;

        /**
         **\brief Get the unit size value in bases.
         **\return unit size
         **/
        Uint1 UnitSize() const;

        /**
         **\brief Get the values of masking parameters.
         **
         ** Masking parameters is a vector of 4 integers representing
         ** the values of T_low, T_extend, T_threshold, and T_high.
         **
         **\return vector of masking parameters
         **/
        const vector< Uint4 > & GetParams() const;

        /**
         **\brief Set the unit size value
         **\param us the unit size
         **/
        virtual void doSetUnitSize( Uint4 us );

        /**
         **\brief Set count information for the given unit.
         **\param unit the unit
         **\param count the number of times the unit and its reverse complement
         **             appears in the genome
         **/
        virtual void doSetUnitCount( Uint4 unit, Uint4 count );

        /**
         **\brief Noop.
         **/
        virtual void doSetComment( const string & msg ) {}

        /**
         **\brief Set a parameter value.
         **
         ** Only recognized parameters will be accepted.
         **
         **\param name the parameter name
         **\param value the parameter value
         **/
        virtual void doSetParam( const string & name, Uint4 value );

        /**
         **\brief Noop.
         **/
        virtual void doSetBlank() {}

        /**
         **\brief Generate a hash function and dump the optimized unit counts
         **       data to the output stream.
         **/
        virtual void doFinalize();

    private:

        /**\internal
         **\brief Find the best set of hash parameters
         **\param k the target hash key size
         **\param max_coll [out] returns the maximum number of collisions
         **\param M [out] returns the number of units with collisions 
         **\param ht pointer to the hash table area
         **\return the right offset corresponding to the best hash function
         **/
        Uint1 findBestRoff( Uint1 k, Uint1 & max_coll, Uint4 & M, Uint4 * ht );

        Uint2 size_requested;       /**<\internal User specified upper limit of the data structure size. */
        Uint1 unit_bit_size;        /**<\internal Unit size in bits. */

        vector< Uint4 > units;      /**<\internal Array of units with counts >= T_low. */
        vector< Uint2 > counts;     /**<\internal Array of corresponding counts. */

        vector< Uint4 > pvalues;    /**<\internal Array of threshold parameters. */
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
