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
 *   Definition for CSeqMaskerIstatAscii class.
 *
 */

#ifndef C_SEQ_MASKER_ISTAT_ASCII_H
#define C_SEQ_MASKER_ISTAT_ASCII_H

#include <string>

#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>

#include <algo/winmask/seq_masker_istat.hpp>
#include <algo/winmask/seq_masker_uset_simple.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Unit counts read from an ascii file.
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerIstatAscii : public CSeqMaskerIstat
{
public:

    /** 
        **\brief Exceptions that CSeqMaskerIstatAscii might throw.
        **/
    class Exception : public CException
    {
        public:

            enum EErrCode
            {
                eStreamOpenFail,    /**< File open failure. */
                eSyntax,            /**< Wrong syntax. */
                eParam              /**< Unknown parameter name. */
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
        **/
    explicit CSeqMaskerIstatAscii(  const string & name,
                                    Uint4 arg_threshold,
                                    Uint4 arg_textend,
                                    Uint4 arg_max_count,
                                    Uint4 arg_use_max_count,
                                    Uint4 arg_min_count,
                                    Uint4 arg_use_min_count );

    /**
        **\brief Object destructor.
        **/
    virtual ~CSeqMaskerIstatAscii() {}

    /**
        **\brief Get the value of the unit size
        **\return unit size
        **/
    virtual Uint1 UnitSize() const { return uset.get_unit_size(); }

protected:

    /**
        **\brief Get the count of the given unit.
        **\param unit the unit to look up
        **\return the count value for the unit
        **/
    virtual Uint4 at( Uint4 unit ) const;

private:
    
    /**\internal
        **\brief The unit counts container.
        **/
    CSeqMaskerUsetSimple uset;
};

END_NCBI_SCOPE

#endif

/*
 * ========================================================================
 * $Log$
 * Revision 1.2  2005/04/13 13:47:48  dicuccio
 * Added export specifiers.  White space changes: reindented class body
 *
 * Revision 1.1  2005/04/04 14:28:46  morgulis
 * Decoupled reading and accessing unit counts information from seq_masker
 * core functionality and changed it to be able to support several unit
 * counts file formats.
 *
 * ========================================================================
 */
