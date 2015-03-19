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
 *   Definition for CSeqMaskerIstatOBinary class.
 *
 */

#ifndef C_SEQ_MASKER_ISTAT_OBINARY_H
#define C_SEQ_MASKER_ISTAT_OBINARY_H

#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>

#include <algo/winmask/seq_masker_istat.hpp>
#include <algo/winmask/seq_masker_uset_hash.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Read counts information from a binary file in optimized format.
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerIstatOBinary 
    : public CSeqMaskerIstat
{
    public:
        
        /** 
         **\brief Exceptions that CSeqMaskerIstatOBinary might throw.
         **/
        class Exception : public CException
        {
            public:

                enum EErrCode
                {
                    eStreamOpenFail,   /**< File open failure. */
                    eBadHashParam,     /**< Bad hash parameter. */
                    eBadParam,         /**< Bad parameter. */
                    eFormat,           /**< File does not follow the format spec. */
                    eAlloc             /**< Allocation failure. */
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
         **
         ** arg_threshold, arg_textend, arg_max_count, and arg_min_count, if
         ** non zero, override the values in the input file.
         **
         **\param name file name
         **\param arg_threshold T_threshold
         **\param arg_textend T_extend
         **\param arg_max_count T_high
         **\param arg_use_max_count value to use for units with count > T_high
         **\param arg_min_count T_low
         **\param arg_use_min_count value to use for units with count < T_low
         **\param arg_use_ba use bit array optimization if available
         **\param skip skip this many bytes in the beginning
         **/
        explicit CSeqMaskerIstatOBinary( const string & name,
                                         Uint4 arg_threshold,
                                         Uint4 arg_textend,
                                         Uint4 arg_max_count,
                                         Uint4 arg_use_max_count,
                                         Uint4 arg_min_count,
                                         Uint4 arg_use_min_count,
                                         bool arg_use_ba,
                                         Uint4 skip = 0 );

        /**
         **\brief Object destructor.
         **/
        virtual ~CSeqMaskerIstatOBinary() {}

        /**
         **\brief Get the value of the unit size
         **\return unit size
         **/
        virtual Uint1 UnitSize() const { return uset.UnitSize(); }

    protected:

        /**
         **\brief Get the count of the given unit.
         **\param unit the unit to look up
         **\return the count value for the unit
         **/
        virtual Uint4 at( Uint4 unit ) const;

        /**
            \brief Get the true count for an n-mer.
    
            \param unit the n-mer value

            \return n-mer count not corrected for t_low
                    and t_high values
        **/
        virtual Uint4 trueat( Uint4 unit ) const;

    private:

        /**\internal
         **\brief Read a 4-byte unsigned integer from a binary stream.
         **\param is the input stream
         **\return 4 byte unsigned integer read from is
         **/
        Uint4 readWord( CNcbiIstream & is ) const;

        /**\internal
         **\brief The unit counts container.
         **/
        CSeqMaskerUsetHash uset;
};

END_NCBI_SCOPE

#endif
