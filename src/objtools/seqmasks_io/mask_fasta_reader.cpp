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
 *   CMaskFastaReader class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>
#include <objtools/readers/fasta.hpp>
#include <objects/seq/Bioseq.hpp>

#include <objtools/seqmasks_io/mask_fasta_reader.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


//-------------------------------------------------------------------------
CRef< CSeq_entry > CMaskFastaReader::GetNextSequence()
{
    if( input_stream.eof() ) return CRef< CSeq_entry >( null );
    CStreamLineReader line_reader( input_stream );

    CFastaReader::TFlags flags = 
        CFastaReader::fForceType |
        CFastaReader::fOneSeq    |
        CFastaReader::fAllSeqIds;
    if ( !parse_seqids_ ) {
        flags |= CFastaReader::fNoParseID;
    }

    flags += is_nucleotide_ 
        ? CFastaReader::fAssumeNuc 
        : CFastaReader::fAssumeProt;

    CFastaReader fasta_reader( line_reader, flags );

    while( !input_stream.eof() )
    {
        CRef< CSeq_entry > aSeqEntry( null );
        aSeqEntry = fasta_reader.ReadSet( 1 );

        if( !input_stream && !input_stream.eof() ) {
            NCBI_THROW( Exception, eBadStream,
                    "error reading input stream" );
        }

        if( aSeqEntry != 0 && aSeqEntry->IsSeq() )
            return aSeqEntry;
        else break;
    }

    return CRef< CSeq_entry >( null );
}


END_NCBI_SCOPE
