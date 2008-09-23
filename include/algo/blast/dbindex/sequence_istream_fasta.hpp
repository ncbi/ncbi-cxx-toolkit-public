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
 *   Header file for CSequenceIStreamFasta class.
 *
 */

#ifndef C_SEQUENCE_I_STREAM_FASTA_HPP
#define C_SEQUENCE_I_STREAM_FASTA_HPP

#include <string>
#include <corelib/ncbistre.hpp>
#include <objtools/readers/fasta.hpp>

#include "sequence_istream.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

/** Sequence stream for reading FASTA formatted files. */
class NCBI_XBLAST_EXPORT CSequenceIStreamFasta : public CSequenceIStream
{
    private:

        /** Alias for the standard IO stream position type. */
        typedef CT_POS_TYPE pos_type;

        bool stream_allocated_;         /**< Whether to deallocate the stream at destruction. */
        CNcbiIstream * istream_;        /**< Standard IO stream for reading FASTA data. */
        size_t curr_seq_;               /**< Current sequence number. */

        objects::CFastaReader * fasta_reader_; /**< Object to read fasta files. */

        /** Starting positions of sequences withing the FASTA stream. */
        std::vector< pos_type > seq_positions_; 

        std::string name_;       /**< FASTA file name, if available. */
        CRef< TSeqData > cache_; /**< Last read sequence. */
        bool use_cache_;         /**< Next time read from cache. */

    public:

        /** Object constructor.
            Creates a FASTA sequence stream by the file name. The stream
            is rewound to the start of the given sequence.
            @param name FASTA file name
            @param pos starting sequence
        */
        CSequenceIStreamFasta( 
                const std::string & name, size_t pos = 0 );

        /** Object constructor.
            Creates a FASTA sequence stream from the standard IO stream.
            The stream is rewound to the start of the given sequence.
            @param input_stream C++ iostream containing the FASTA data
            @param pos starting sequence
        */
        CSequenceIStreamFasta( 
                CNcbiIstream & input_stream, size_t pos = 0 );

        /** Object destructor. */
        virtual ~CSequenceIStreamFasta();

        /** Retrieve and return the next sequence from the sequnce stream.
            @sa CSequenceIStream::next()
        */
        virtual CRef< TSeqData > next();

        /** Roll back to the start of the previous sequence.
            @sa CSequenceIStream::putback()
        */
        virtual void putback();
};

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

#endif

