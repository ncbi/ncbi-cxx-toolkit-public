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
 *   Definition for CSeqMaskerIstatFactory class.
 *
 */

#ifndef C_SEQ_MASKER_ISTAT_FACTORY_H
#define C_SEQ_MASKER_ISTAT_FACTORY_H

#include <string>

#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

class CSeqMaskerIstat;

/**
 **\brief Factory class to generate an appropriate CSeqMaskerIstat
 **       derived class based on the format name.
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerIstatFactory
{
public:

    /// Counts statistics formats.
    ///
    enum EStatType {
        eAscii,
        eBinary,
        eOAscii,
        eOBinary,
        eUnknown
    };

    /** 
        **\brief Exceptions that CSeqMaskerIstatFactory might throw.
        **/
    class Exception : public CException
    {
        public:

            enum EErrCode
            {
                eBadFormat,     /**< Unknown file format. */
                eCreateFail,    /**< Could not create the CSeqMaskerIstat object. */
                eOpen           /**< Could not open file. */
            };

            /**
                **\brief Get a description string for this exception.
                **\return C-style description string
                **/
            virtual const char * GetErrCodeString() const;

            NCBI_EXCEPTION_DEFAULT( Exception, CException );

    };

    /// Return the format of the counts statistics file.
    ///
    static EStatType DiscoverStatType( string const & name );

    /**
        **\brief Create a unit counts container from a file.
        **
        ** All parameters after name are forwarded to the constructor of the
        ** proper subclass of CSeqMaskerIstat.
        **
        **\param name name of the file containing the unit counts information
        **\param threshold T_threshold
        **\param textend T_extend
        **\param max_count T_high
        **\param use_max_count value to use for units with count > T_high
        **\param min_count T_low
        **\param use_min_count value to use for units with count < T_low
        **\param use_ba use bit array optimization if available
        **/
    static CSeqMaskerIstat * create( const string & name,
                                        Uint4 threshold,
                                        Uint4 textend,
                                        Uint4 max_count,
                                        Uint4 use_max_count,
                                        Uint4 min_count,
                                        Uint4 use_min_count,
                                        bool use_ba );

private:

    /// Return the format of the counts statistics file. At the same time
    /// return the number of metadata lines (for text formats) and size
    /// of metadata in bytes (for binary formats).
    ///
    static EStatType DiscoverStatType( 
            string const & name, vector< string > & md, size_t & skip );
};

END_NCBI_SCOPE

#endif
