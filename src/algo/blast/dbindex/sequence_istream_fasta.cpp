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
 *   Implementation of class CSequenceIStreamFasta.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/readers/fasta.hpp>

#include <algo/blast/dbindex/sequence_istream_fasta.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

//------------------------------------------------------------------------------
/** Flags for opening FASTA files. */
static const objects::TReadFastaFlags READ_FASTA_FLAGS =
    objects::fReadFasta_AssumeNuc |
    objects::fReadFasta_ForceType |
    objects::fReadFasta_OneSeq    |
    objects::fReadFasta_AllSeqIds;

//------------------------------------------------------------------------------
CSequenceIStreamFasta::CSequenceIStreamFasta( 
        const std::string & name , std::size_t pos )
    : stream_allocated_( false ), istream_( 0 ), curr_seq_( 0 ), name_( name )
{
    istream_ = new CNcbiIfstream( name.c_str() );
    stream_allocated_ = true;
    seq_positions_.push_back( 0 );
    rewind( pos );
}

//------------------------------------------------------------------------------
CSequenceIStreamFasta::CSequenceIStreamFasta( 
        CNcbiIstream & input_stream, std::size_t pos )
    : stream_allocated_( false ), istream_( &input_stream ), curr_seq_( 0 )
{
    seq_positions_.push_back( 0 );
    rewind( pos );
}

//------------------------------------------------------------------------------
CSequenceIStreamFasta::~CSequenceIStreamFasta()
{
    if( stream_allocated_ ) {
        delete istream_;
    }
}

//------------------------------------------------------------------------------
CRef< CSequenceIStream::TSeqData > CSequenceIStreamFasta::next()
{
    CRef< TSeqData > result( new TSeqData );

    try {
        result->seq_entry_ = ncbi::objects::ReadFasta( *istream_, READ_FASTA_FLAGS, 0, &result->mask_locs_ );
    }catch( ... ) {}

    if( result->seq_entry_.GetPointerOrNull() != 0 ) {
        ++curr_seq_;

        if( curr_seq_ == seq_positions_.size() ) {
            pos_type cpos = istream_->tellg();
            seq_positions_.push_back( cpos );
        }
    }

    return result;
}

//------------------------------------------------------------------------------
void CSequenceIStreamFasta::rewind( TStreamPos pos )
{
    if( !*istream_ && stream_allocated_ ) {
            delete istream_;
            istream_ = new CNcbiIfstream( name_.c_str() );
    }

    if( !*istream_ ) {
        NCBI_THROW( CSequenceIStream_Exception, 
                eOpNotSupported, "can not rewind" );
    }

    if( pos >= seq_positions_.size() ) {
        while( next() && pos >= seq_positions_.size() );
    }
    else {
        istream_->seekg( seq_positions_[pos] );
        curr_seq_ = pos;
    }
}

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

