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
 *   Definition of CSeqMaskerUStat class.
 *
 */

#ifndef C_WIN_MASK_USTAT_H
#define C_WIN_MASK_USTAT_H

#include <string>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiargs.hpp>

BEGIN_NCBI_SCOPE

/**
 **\brief Base class for computing and saving unit counts data.
 **/
class NCBI_XALGOWINMASK_EXPORT CSeqMaskerOstat : public CObject
{
    public:

        /**
         **\brief Exceptions that CSeqMaskerOstat can throw.
         **/
        class CSeqMaskerOstatException : public CException
        {
            public:

                enum EErrCode
                {
                    eBadState   /**< Operation can not be performed in the current state. */
                };

                /**
                 **\brief Get a description string for this exception.
                 **\return C-style description string
                 **/
                virtual const char * GetErrCodeString() const;

                NCBI_EXCEPTION_DEFAULT( CSeqMaskerOstatException, CException );
        };

        /**
         **\brief Object constructor.
         **\param os C++ stream that should be used to save the unit counts data
         **/
        explicit CSeqMaskerOstat( CNcbiOstream & os )
            : out_stream( os ), state( start )
        {}

        /**
         **\brief Trivial object destructor.
         **/
        virtual ~CSeqMaskerOstat() {}

        /**
         **\brief Set the unit size value.
         **
         ** This method must be called before any call to setUnitCount().
         **
         **\param us new value of unit size 
         **/
        void setUnitSize( Uint1 us );

        /**
         **\brief Add count value for a particular unit.
         **
         ** This method can not be called before setUnitSize() and after
         ** the first call to setParam().
         **
         **\param unit unit value
         **\param count number of times the unit and its reverse complement
         **             occur in the genome
         **/
        void setUnitCount( Uint4 unit, Uint4 count );

        /**
         **\brief Add a comment to the unit counts file.
         **
         ** It is possible that this method is NOP for some unit counts
         ** formats.
         **
         **\param msg comment message
         **/
        void setComment( const string & msg ) { doSetComment( msg ); }

        /**
         **\brief Set a value of a WindowMasker parameter.
         **
         ** This method only can be called after all setUnitCount() calls.
         **
         **\param name the name of the parameter
         **\param value the value of the parameter
         **/
        void setParam( const string & name, Uint4 value );

        /**
         **\brief Create a blank line in the unit counts file.
         **
         ** It is possible that this method is NOP for some unit counts
         ** formats.
         **/
        void setBlank() { doSetBlank(); }

    protected:

        /**\name Methods used to delegate functionality to derived classes */
        /**@{*/
        virtual void doSetUnitSize( Uint4 us ) = 0;
        virtual void doSetUnitCount( Uint4 unit, Uint4 count ) = 0;
        virtual void doSetComment( const string & msg ) = 0;
        virtual void doSetParam( const string & name, Uint4 value ) = 0;
        virtual void doSetBlank() = 0;
        /**@}*/

        /**
         **\brief Refers to the C++ stream that should be used to write
         **       out the unit counts data.
         **/
        CNcbiOstream& out_stream;

    private:

        /**\name Provide reference semantics for the class */
        /**@{*/
        CSeqMaskerOstat( const CSeqMaskerOstat & );
        CSeqMaskerOstat& operator=( const CSeqMaskerOstat & );
        /**@}*/

        /**\internal
         **\brief Possible object states.
         **/
        enum 
        {
            start, /**<\internal The object has just been created. */
            ulen,  /**<\internal The unit size has been set. */
            udata, /**<\internal The unit counts data is being added. */
            thres  /**<\internal The parameters values are being set. */
        } state;
};

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.4  2005/04/04 14:28:46  morgulis
 * Decoupled reading and accessing unit counts information from seq_masker
 * core functionality and changed it to be able to support several unit
 * counts file formats.
 *
 * Revision 1.3  2005/03/30 17:53:54  ivanov
 * Added export specifiers
 *
 * Revision 1.2  2005/03/29 13:29:25  dicuccio
 * Drop multiple copy ctors; hide assignment operator
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
