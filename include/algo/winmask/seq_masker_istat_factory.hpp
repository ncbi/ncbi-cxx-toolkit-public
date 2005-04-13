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
        **/
    static CSeqMaskerIstat * create( const string & name,
                                        Uint4 threshold,
                                        Uint4 textend,
                                        Uint4 max_count,
                                        Uint4 use_max_count,
                                        Uint4 min_count,
                                        Uint4 use_min_count );

private:
};

END_NCBI_SCOPE

#endif

/*
 * ========================================================================
 * $Log$
 * Revision 1.3  2005/04/13 13:47:48  dicuccio
 * Added export specifiers.  White space changes: reindented class body
 *
 * Revision 1.2  2005/04/12 13:35:34  morgulis
 * Support for binary format of unit counts file.
 *
 * Revision 1.1  2005/04/04 14:28:46  morgulis
 * Decoupled reading and accessing unit counts information from seq_masker
 * core functionality and changed it to be able to support several unit
 * counts file formats.
 *
 * ========================================================================
 */
