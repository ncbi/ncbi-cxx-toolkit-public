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
 *   Definition of CSeqMaskerUStatAscii class.
 *
 */

#ifndef C_WIN_MASK_USTAT_ASCII_H
#define C_WIN_MASK_USTAT_ASCII_H

#include <string>

#include <corelib/ncbistre.hpp>

#include <algo/winmask/seq_masker_ostat.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Class responsible for creation of unit counts statistics
 **       in text format.
 **/
class CSeqMaskerOstatAscii : public CSeqMaskerOstat
{
    public:

        /** 
         **\brief Exceptions that CSeqMaskerOstatAscii might throw.
         **/
        class CSeqMaskerOstatAsciiException : public CException
        {
            public:
                
                enum EErrCode
                {
                    eBadOrder   /**< Unit information is not added in ascending order of units. */
                };

                /**
                 **\brief Get a description string for this exception.
                 **\return C-style description string
                 **/
                virtual const char * GetErrCodeString() const;

                NCBI_EXCEPTION_DEFAULT( CSeqMaskerOstatAsciiException, CException );
        };

        /**
         **\brief Object constructor.
         **\param name name of the output file containing the unit
         **            counts data
         **/
        explicit CSeqMaskerOstatAscii( const string & name );

        /**
         **\brief Object destructor.
         **/
        virtual ~CSeqMaskerOstatAscii();

    protected:

        /**
         **\brief Output a line containing the unit size.
         **\param us the value of the unit size
         **/
        virtual void doSetUnitSize( Uint4 us );

        /**
         **\brief Output a line with information about the given unit count.
         **
         ** The line contains two words: the unit value in hex format and
         ** the unit count in decimal format. The function checks that the
         ** unit value is greater than that of all previously written units.
         **
         **\param unit the unit value
         **\param count the number of times the unit and its reverse complement
         **             occur in the genome
         **/
        virtual void doSetUnitCount( Uint4 unit, Uint4 count );

        /**
         **\brief Prints msg as a comment line in the output file.
         **\param msg the comment message
         **/
        virtual void doSetComment( const string & msg );

        /**
         **\brief Output a line with information about the given parameter value.
         **
         ** The line starts with '>' and contains 2 words: the name of the
         ** parameter and the decimal integer value of the parameter.
         ** This method should be called after all unit counts are reported.
         **
         **\param name the parameter name
         **\param value the parameter value
         **/
        virtual void doSetParam( const string & name, Uint4 value );

        /**
         **\brief Create a blank line in the output file.
         **/
        virtual void doSetBlank();
};

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.3  2005/04/04 14:28:46  morgulis
 * Decoupled reading and accessing unit counts information from seq_masker
 * core functionality and changed it to be able to support several unit
 * counts file formats.
 *
 * Revision 1.2  2005/03/29 13:29:37  dicuccio
 * Use <> for includes
 *
 * Revision 1.1  2005/03/28 22:41:06  morgulis
 * Moved win_mask_ustat* files to library and renamed them.
 *
 * Revision 1.1  2005/03/28 21:33:26  morgulis
 * Added -sformat option to specify the output format for unit counts file.
 * Implemented framework allowing usage of different output formats for
 * unit counts. Rewrote the code generating the unit counts file using
 * that framework.
 *
 * ========================================================================
 */

#endif
