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
 *   Implementation file for part of CDbIndex and some related classes.
 *
 */

#include <ncbi_pch.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <corelib/ncbi_limits.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_hits.h>

#ifdef LOCAL_SVN

#include "sequence_istream_fasta.hpp"
#include "dbindex.hpp"

#else

#include <algo/blast/dbindex/sequence_istream_fasta.hpp>
#include <algo/blast/dbindex/dbindex.hpp>
#include <algo/blast/dbindex/dbindex_sp.hpp>

#endif

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

//-------------------------------------------------------------------------
namespace {
    void CheckIndexEndianness( void * map )
    {
        static const TWord MAX_KEY_SIZE = 15;
        TWord key_size( *((TWord *)map + 4) );
        if( key_size <= MAX_KEY_SIZE ) return;

        {
            TWord rev_key_size( 0 );
            rev_key_size += ((key_size&0xFF)<<24);
            rev_key_size += (((key_size>>8)&0xFF)<<16);
            rev_key_size += (((key_size>>16)&0xFF)<<8);
            rev_key_size += (key_size>>24);

            if( rev_key_size <= MAX_KEY_SIZE ) {
                NCBI_THROW( CDbIndex_Exception, 
                            CDbIndex_Exception::eBadData,
                            "possible index endianness mismatch: check "
                            "if the index was created for the "
                            "architecture with different endianness" );
            }

            NCBI_THROW( CDbIndex_Exception, CDbIndex_Exception::eBadData,
                        "index header validation failed" );
        }
    }
}

//-------------------------------------------------------------------------
template<>
const SIndexHeader ReadIndexHeader< true >( void * map )
{
    CheckIndexEndianness( map );
    SIndexHeader result;
    unsigned long tmp;
    TWord * ptr = (TWord *)((Uint8 *)map + 2);
    result.hkey_width_  = (unsigned long)(*ptr++);

    tmp = (unsigned long)(*ptr++);
    tmp = (unsigned long)(*ptr++);

    result.stride_  = CDbIndex::STRIDE;
    result.ws_hint_ = 28;

    result.max_chunk_size_ = MAX_DBSEQ_LEN;
    result.chunk_overlap_  = DBSEQ_CHUNK_OVERLAP;

    result.start_       = (TSeqNum)(*ptr++);
    result.start_chunk_ = (TSeqNum)(*ptr++);
    result.stop_        = (TSeqNum)(*ptr++);
    result.stop_chunk_  = (TSeqNum)(*ptr++);

    result.legacy_ = true;
    return result;
}

template<>
const SIndexHeader ReadIndexHeader< false >( void * map )
{
    CheckIndexEndianness( map );
    SIndexHeader result;
    TWord * ptr = (TWord *)((Uint8 *)map + 2);
    result.hkey_width_  = (unsigned long)(*ptr++);
    result.stride_      = (unsigned long)(*ptr++);
    result.ws_hint_     = (unsigned long)(*ptr++);

    result.max_chunk_size_ = MAX_DBSEQ_LEN;
    result.chunk_overlap_  = DBSEQ_CHUNK_OVERLAP;

    result.start_       = (TSeqNum)(*ptr++);
    result.start_chunk_ = (TSeqNum)(*ptr++);
    result.stop_        = (TSeqNum)(*ptr++);
    result.stop_chunk_  = (TSeqNum)(*ptr++);

    result.legacy_ = false;
    return result;
}

//-------------------------------------------------------------------------
template<>
unsigned long GetIndexStride< true >( const SIndexHeader & header )
{ return CDbIndex::STRIDE; }

template<>
unsigned long 
GetIndexStride< false >( const SIndexHeader & header )
{ return header.stride_; }

//-------------------------------------------------------------------------
template<>
unsigned long GetIndexWSHint< true >( const SIndexHeader & header )
{ return 28; }

template<>
unsigned long GetIndexWSHint< false >( const SIndexHeader & header )
{ return header.ws_hint_; }

//-------------------------------------------------------------------------
const char * CDbIndex_Exception::GetErrCodeString() const
{
    switch( GetErrCode() ) {
        case eBadOption:   return "bad index creation option";
        case eBadSequence: return "bad sequence data";
        case eBadVersion:  return "wrong versin";
        case eBadData:     return "corrupt index data";
        case eIO:          return "I/O error";
        default:           return CException::GetErrCodeString();
    }
}

//-------------------------------------------------------------------------
CDbIndex::SOptions CDbIndex::DefaultSOptions()
{
    SOptions result = {
        false,                  // do not create id map by default
        true,                   // for now, use legacy format by default
        STRIDE,                 // default stride for legacy indices
        28,                     // default word size for legacy indices
        12,                     // default Nmer size
        MAX_DBSEQ_LEN,          // defined by BLAST
        DBSEQ_CHUNK_OVERLAP,    // defined by BLAST
        REPORT_NORMAL,          // normal level of progress reporting
        1536,                   // max index size if 1.5 Gb by default
    };

    return result;
}

//-------------------------------------------------------------------------
/** Get the index format version for the index data represented by the
    input stream.
    @param is   [I/O]   read the version information from this input
                        stream
    @return index format version
*/
unsigned long GetIndexVersion( CNcbiIstream & is )
{
    unsigned char version;
    ReadWord( is, version );
    return (unsigned long)version;
}

//-------------------------------------------------------------------------
CRef< CDbIndex > CDbIndex::Load( const std::string & fname, bool nomap )
{
    CNcbiIfstream index_stream( fname.c_str() );

    if( !index_stream ) {
        NCBI_THROW( CDbIndex_Exception, eIO, "can not open index" );
    }
    
    unsigned long version = GetIndexVersion( index_stream );
    index_stream.close();

    switch( version ) {
        case VERSION:     return LoadIndex< true >( fname, nomap );
        case VERSION + 1: return LoadIndex< false >( fname, nomap );
        default: 
            
            NCBI_THROW( 
                    CDbIndex_Exception, eBadVersion,
                    "wrong index version" );
    }
}

//-------------------------------------------------------------------------
unsigned long GetCodeBits( unsigned long stride )
{
    unsigned long result = 0;
    do { stride >>= 1; ++result; } while( stride != 0 );
    return result;
}

//-------------------------------------------------------------------------
unsigned long GetMinOffset( unsigned long stride )
{ return (1<<(2*GetCodeBits( stride ))); }

//-------------------------------------------------------------------------
void CSubjectMap::Load( 
        TWord ** map, TSeqNum start, TSeqNum stop, unsigned long stride )
{
    if( *map ) {
        stride_ = stride;
        min_offset_ = GetMinOffset( stride_ );
        TSubjects::size_type n_subjects 
            = (TSubjects::size_type)(stop - start + 1);
        total_ = *(*map)++;
        subjects_.SetPtr( *map, n_subjects );
        total_ -= sizeof( TWord )*n_subjects;
        *map += n_subjects;

#ifdef PRINTSUBJMAP
        for( unsigned long i = 0; i < n_subjects; ++i ) {
            cerr << i << " ---> " << subjects_[i] - 1 << endl;
        }
#endif

        TChunks::size_type chunks_size = 
            (TChunks::size_type)( 1 + total_/sizeof( TWord ));
        chunks_.SetPtr( *map, chunks_size ); 
        *map += chunks_.size();
        SetSeqDataFromMap( map );

        /* Initialize c2s map. */
        TSeqNum j = 0;

        for( TSeqNum i = 1; i < subjects_.size() - 1; ++i )
            for( TSeqNum chunk = 0 ; j < subjects_[i] - 1 ; ++chunk, ++j )
                c2s_map_.push_back( make_pair( i - 1, chunk ) );

        for( TSeqNum chunk = 0 ; j < chunks_.size() ; ++chunk, ++j )
            c2s_map_.push_back( 
                    make_pair( (unsigned)subjects_.size() - 2, chunk ) );
    }
}

//-------------------------------------------------------------------------
void CSubjectMap::SetSeqDataFromMap( TWord ** map )
{
    if( *map ) {
        total_ = *(*map)++;
        seq_store_.SetPtr( (Uint1 *)*map, (TSeqStore::size_type)total_ );
        *map += 1 + total_/sizeof( TWord );
    }
}

//-------------------------------------------------------------------------
CSubjectMap::CSubjectMap( TWord ** map, const SIndexHeader & header )
{
    TWord nlengths = *(*map)++;
    nlengths /= sizeof( TWord );
    offset_bits_ = (Uint1)(*(*map)++);
    offset_mask_ = (1<<offset_bits_) - 1;
    lengths_.SetPtr( *map, nlengths );
    *map += nlengths;

#ifdef PRINTSUBJMAP
    for( unsigned long i = 0; i < nlengths; ++i ) {
        cerr << "length( " << i << ") = " << lengths_[i] << endl;
    }
#endif

    TWord nlidmap = *(*map)++;
    nlidmap /= sizeof( TWord );
    lid_map_.SetPtr( *map, nlidmap );
    *map += nlidmap;

    Load( map, header.start_, header.stop_, header.stride_ );
    max_chunk_size_ = header.max_chunk_size_;
    chunk_overlap_  = header.chunk_overlap_;
}

//-------------------------------------------------------------------------
CSubjectMap::CSubjectMap( 
        TWord ** map, TSeqNum start, TSeqNum stop, unsigned long stride )
{
    TWord nlengths = *(*map)++;
    nlengths /= sizeof( TWord );
    offset_bits_ = (Uint1)(*(*map)++);
    offset_mask_ = (1<<offset_bits_) - 1;
    lengths_.SetPtr( *map, nlengths );
    *map += nlengths;

#ifdef PRINTSUBJMAP
    for( unsigned long i = 0; i < nlengths; ++i ) {
        cerr << "length( " << i << ") = " << lengths_[i] << endl;
    }
#endif

    TWord nlidmap = *(*map)++;
    nlidmap /= sizeof( TWord );
    lid_map_.SetPtr( *map, nlidmap );
    *map += nlidmap;

    Load( map, start, stop, stride );
}

//-------------------------------------------------------------------------
COffsetData_Base::COffsetData_Base( 
        TWord ** map, unsigned long hkey_width, 
        unsigned long stride, unsigned long ws_hint )
    : total_( 0 ), hkey_width_( hkey_width ), stride_( stride ),
      ws_hint_( ws_hint ), min_offset_( GetMinOffset( stride ) )
{
    if( *map ) {
        total_ = *(*map)++;
        TWord hash_table_size = ((TWord)1)<<(2*hkey_width_);
        hash_table_.SetPtr( 
                *map, 
                (THashTable::size_type)(hash_table_size + 1) );
        *map += hash_table_size + 1;
    }
}

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

