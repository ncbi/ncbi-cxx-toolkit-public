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
 *   Header file for CSequenceIStream class.
 *
 */

#ifndef C_SEQUENCE_I_STREAM_HPP
#define C_SEQUENCE_I_STREAM_HPP

#include <memory>
#include <vector>

#include <corelib/ncbiobj.hpp>
#include <algo/blast/core/blast_export.h>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

/** Class used to abstract reading nucleotide sequences from
    various sources.
*/
class NCBI_XBLAST_EXPORT CSequenceIStream
{
    protected:

        /** Type containing the sequence itself along with the
            masking information.
        */
        struct CSeqData : public CObject
        {
            /** Type representing masked portions of a sequence. */
            typedef std::vector< CConstRef< objects::CSeq_loc > > TMask;

            CRef< objects::CSeq_entry > seq_entry_;     /**< Sequence data. */
            mutable TMask mask_locs_;                   /**< Masked portion of the sequence. */

            /** Object constructor. */
            CSeqData()
                : seq_entry_( null )
            {}

            /** Conversion to bool.
                @return true if the object contains a valid sequence;
                        false otherwise
            */
            operator bool() const { return seq_entry_.GetPointerOrNull() != 0; }
        };

    public:

        /** Class representing error conditions in the sequence stream. */
        class CSequenceIStream_Exception : public CException
        {
            public:

                /** Numerical error codes. */
                enum EErrCode
                {
                    eOpNotSupported,    /**< The requested operation is not implemented
                                             by this kind of sequence stream (e.g. the
                                             stream can not rewind). */
                    eIO,                /**< System io error. */
                    eParam              /**< Parameter error. */
                };

                /** Get the exception description string.
                    @return the exception description text
                */
                virtual const char * GetErrCodeString() const
                {
                    switch( GetErrCode() ) {
                        case eOpNotSupported: 
                            return "stream operation is not supported";
                        case eIO:
                            return "I/O error";
                        case eParam:
                            return "database parameter error";
                        default: return CException::GetErrCodeString();
                    }
                }

                NCBI_EXCEPTION_DEFAULT( CSequenceIStream_Exception, CException );
        };

        typedef CSeqData TSeqData;      /**< Public alias for sequence info data type. */
        typedef TSeqData::TMask TMask;  /**< Public alias for type containing masking info. */
        typedef Uint4 TStreamPos;       /**< Type used to represent positions within a sequence stream. */

        /** Object destructor. */
        virtual ~CSequenceIStream() {}

        /** Extract the next sequence from the stream.
            (To be implemented by derived classes.)
            @return Smart pointer to the sequence data. The contents
                    of the value is convertible to false if no more
                    sequences are available.
        */
        virtual CRef< TSeqData > next() = 0;

        /** Roll back to the start of the previousely read sequence.
            (To be implemented by derived classes.).
            If a derived class does not support this operation it
            should throw CSequenceIStream_Exception with error code
            eOpNotSupported.
        */
        virtual void putback() = 0;
};

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

#endif

