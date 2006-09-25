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
 *   Implementation file for CSequenceIStreamBlastDB class.
 *
 */

#include <ncbi_pch.hpp>

#include <algo/blast/dbindex/sequence_istream_bdb.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

//------------------------------------------------------------------------------
CSequenceIStreamBlastDB::CSequenceIStreamBlastDB( const string & dbname )
    : seqdb_( new CSeqDB( dbname, CSeqDB::eNucleotide ) ), oid_( 0 )
{
}

//------------------------------------------------------------------------------
CRef< CSequenceIStream::TSeqData > CSequenceIStreamBlastDB::next()
{
    CRef< CSeq_entry > entry( null );

    if( oid_ < seqdb_->GetNumOIDs() ) {
        CRef< CBioseq > seq = seqdb_->GetBioseq( oid_++ );
        entry.Reset( new CSeq_entry );
        entry->SetSeq( *seq );
    }

    CRef< TSeqData > data( new TSeqData );
    data->seq_entry_ = entry;
    return data;
}

//------------------------------------------------------------------------------
void CSequenceIStreamBlastDB::rewind( TStreamPos pos )
{
    oid_ = (CSeqDB::TOID)pos;
}

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

