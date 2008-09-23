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

#ifdef LOCAL_SVN
#include "sequence_istream_fasta.hpp"
#else
#include <algo/blast/dbindex/sequence_istream_fasta.hpp>
#endif

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

//------------------------------------------------------------------------------
/** Flags for opening FASTA files. */
static objects::CFastaReader::TFlags CFASTAREADER_FLAGS = 
        objects::CFastaReader::fForceType |
        objects::CFastaReader::fAssumeNuc |
        objects::CFastaReader::fNoParseID |
        objects::CFastaReader::fAllSeqIds;

//------------------------------------------------------------------------------
CSequenceIStreamFasta::CSequenceIStreamFasta( 
        const std::string & name , size_t pos )
    : stream_allocated_( false ), istream_( 0 ), curr_seq_( 0 ), 
      fasta_reader_( 0 ), name_( name ),
      cache_( null ), use_cache_( false )
{
    istream_ = new CNcbiIfstream( name.c_str() );
    
    if( !*istream_ ) {
        NCBI_THROW( CSequenceIStream_Exception, eIO,
                "failed to open input stream" );
    }

    stream_allocated_ = true;
    CRef< CStreamLineReader > line_reader( new CStreamLineReader( *istream_ ) );
    fasta_reader_ = new objects::CFastaReader( *line_reader, CFASTAREADER_FLAGS );
}

//------------------------------------------------------------------------------
CSequenceIStreamFasta::CSequenceIStreamFasta( 
        CNcbiIstream & input_stream, size_t pos )
    : stream_allocated_( false ), istream_( &input_stream ), 
      curr_seq_( 0 ), fasta_reader_( 0 ),
      cache_( null ), use_cache_( false )
{
    if( !*istream_ ) {
        NCBI_THROW( CSequenceIStream_Exception, eIO,
                "failed to open input stream" );
    }

    CRef< CStreamLineReader > line_reader( new CStreamLineReader( *istream_ ) );
    fasta_reader_ = new objects::CFastaReader( *line_reader, CFASTAREADER_FLAGS );
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
    if( use_cache_ ) {
        use_cache_ = false;
        return cache_;
    }

    CRef< TSeqData > result( new TSeqData );

    try {
        while( !fasta_reader_->AtEOF() )
        {
            result->mask_locs_.push_back( fasta_reader_->SaveMask() );
            result->seq_entry_ = fasta_reader_->ReadOneSeq();
            if( result->seq_entry_ != 0 && result->seq_entry_->IsSeq() ) break;
        }
    }catch( ... ) {}

    cache_ = result;
    return result;
}

//------------------------------------------------------------------------------
void CSequenceIStreamFasta::putback()
{ use_cache_ = true; }

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

