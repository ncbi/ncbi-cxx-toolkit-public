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
 *   Definition of CSeqMaskerOstatBin class.
 *
 */

#ifndef C_SEQ_MASKER_OSTAT_BIN_H
#define C_SEQ_MASKER_OSTAT_BIN_H

#include <string>
#include <vector>

#include <algo/winmask/seq_masker_ostat.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Compute and save counts information in simple binary format.
 **/
class CSeqMaskerOstatBin : public CSeqMaskerOstat
{
    public:

        /**
         **\brief Object constructor.
         **\param name the name of the output file stream
         **/
        explicit CSeqMaskerOstatBin( const string & name );

        /**
         **\brief Object destructor.
         **/
        virtual ~CSeqMaskerOstatBin();

    protected:

        /**
         **\brief Noop - comments are not available in the binary format.
         **/
        virtual void doSetComment( const string & msg ) {}

        /**
         **\brief Noop - blank lines do not make sense in the binary format.
         **/
        virtual void doSetBlank() {}

        /**
         **\brief Write the unit size value to the binary output.
         **\param us the unit size
         **/
        virtual void doSetUnitSize( Uint4 us );

        /**
         **\brief Write count information for the unit to the binary output.
         **\param unit the unit
         **\param count the number of times the unit and its reverse complement
         **             appears in the genome
         **/
        virtual void doSetUnitCount( Uint4 unit, Uint4 count );

        /**
         **\brief Write a parameter value to the binary output.
         **
         ** Only recognized parameters will be written.
         **
         **\param name the parameter name
         **\param value the parameter value
         **/
        virtual void doSetParam( const string & name, Uint4 value );

    private:

        /**\internal
         **\brief Write a 32 bit value to the output binary stream.
         **\param word a 32 bit value
         **/
        void write_word( Uint4 word );

        /**\internal
         **\brief Parameter values.
         **/
        vector< Uint4 > pvalues;
};

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
