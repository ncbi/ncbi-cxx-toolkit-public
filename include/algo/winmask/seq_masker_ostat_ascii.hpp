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
 *   Definition of CSeqMaskerOStatAscii class.
 *
 */

#ifndef C_WIN_MASK_USTAT_ASCII_H
#define C_WIN_MASK_USTAT_ASCII_H

#include <string>
#include <vector>

#include <corelib/ncbistre.hpp>

#include <algo/winmask/seq_masker_ostat.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Class responsible for creation of unit counts statistics
 **       in text format.
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerOstatAscii : public CSeqMaskerOstat
{
public:

    /// Format version.
    static CSeqMaskerVersion FormatVersion;

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
    explicit CSeqMaskerOstatAscii( 
            const string & name, string const & metadat );

    /**
        **\brief Object constructor.
        **\param os the output stream
        **/
    explicit CSeqMaskerOstatAscii( CNcbiOstream & os, string const & metadata );

    /**
        **\brief Object destructor.
        **/
    virtual ~CSeqMaskerOstatAscii();

    /** Get actual counts format version. */
    virtual CSeqMaskerVersion const & GetStatFmtVersion() const { 
        return FormatVersion; 
    }

protected:

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
     **\brief Write data to output file.
     **/
    virtual void doFinalize();

private:

    /** Set of comments to print in the end of the stats file. */
    std::vector< std::string > comments;
};

END_NCBI_SCOPE

#endif
