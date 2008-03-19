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
 *   Header for counts format converter class.
 *
 */

#ifndef C_WIN_MASK_COUNTS_CONVERTER_H
#define C_WIN_MASK_COUNTS_CONVERTER_H

#include <corelib/ncbistre.hpp>
#include <corelib/ncbiexpt.hpp>

#include <algo/winmask/seq_masker_istat.hpp>

BEGIN_NCBI_SCOPE

/** 
    \brief Class responsible for converting unit counts between different formats.
 **/
class CWinMaskCountsConverter
{
    public:

        /**
            \brief Class defining exceptions specific to CWinMaskCountsConverter.
         **/
        class Exception : public CException
        {
            public:

                /**\brief Error codes.
                */
                enum EErrCode
                {
                    eBadOption  ///< Command line options inconsistency.
                };

                /**
                    \brief Return description string corresponding to an error code.

                    \return error string
                 **/
                virtual const char * GetErrCodeString() const;
    
                NCBI_EXCEPTION_DEFAULT( Exception, CException );
        };

        /**
            \brief Instance constructor.

            \param input_fname   input file name
            \param output_fname  output file name
            \param counts_format desired format for the output
         **/
        CWinMaskCountsConverter(
                const string & input_fname,
                const string & output_fname,
                const string & counts_oformat );

        /**
            \brief Method performing the actual conversion.

            \return 0 on success; 1 on failure.
         **/
        int operator()();

    private:

        CRef< CSeqMaskerIstat > istat;  ///< object containing unit counts read from the input
        string ofname;                  ///< output file name
        string oformat;                 ///< target n-mer counts format for the output
};

END_NCBI_SCOPE

#endif

