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
 *   Definition of CSeqMaskerUStatFactory class.
 *
 */

#ifndef C_WIN_MASK_USTAT_FACTORY_H
#define C_WIN_MASK_USTAT_FACTORY_H

#include <string>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiargs.hpp>

BEGIN_NCBI_SCOPE

class CSeqMaskerOstat;

/**
 **\brief Factory of CSeqMaskerOstat objects.
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerOstatFactory
{
public:

    /**
    **\brief Exceptions that CSeqMaskerOstatFactory might throw.
    **/
    class CSeqMaskerOstatFactoryException : public CException
    {
        public:
        
            enum EErrCode
            {
                eBadName,   /**< Unknown format name. */
                eCreateFail /**< Failure to create the requested object. */
            };

            /**
                **\brief Get a description string for this exception.
                **\return C-style description string
                **/
            virtual const char * GetErrCodeString() const;

            NCBI_EXCEPTION_DEFAULT( 
                CSeqMaskerOstatFactoryException, CException );
    };

    /**
        **\brief Method used to create a CSeqMakserOstat object by format name.
        **
        ** The method instantiates the appropriate subclass of CSeqMaskerOstat
        ** corresponding to the name of the file format, or throws an exception
        ** if the format name is not recognized.
        **
        **\param ustat_type the name of the unit counts file format
        **\param name the name of the file to save unit counts data to
        **\return pointer to the newly created object
        **/
    static CSeqMaskerOstat * create( 
        const string & ustat_type, const string & name );
};

END_NCBI_SCOPE

#endif

/*
 * ========================================================================
 * $Log$
 * Revision 1.4  2005/04/13 13:47:48  dicuccio
 * Added export specifiers.  White space changes: reindented class body
 *
 * Revision 1.3  2005/04/04 14:28:46  morgulis
 * Decoupled reading and accessing unit counts information from seq_masker
 * core functionality and changed it to be able to support several unit
 * counts file formats.
 *
 * Revision 1.2  2005/03/30 17:53:54  ivanov
 * Added export specifiers
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
