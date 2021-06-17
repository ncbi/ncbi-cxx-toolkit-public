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
 *   Specialized index implementations.
 *
 */

#ifndef C_DB_INDEX_SP_HPP
#define C_DB_INDEX_SP_HPP

#include <corelib/ncbifile.hpp>
#include <algo/blast/dbindex/dbindex.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

#define INLINE NCBI_INLINE

/** Forwarding declarations for convenience. */
typedef CDbIndex::TSeqNum TSeqNum;
typedef CDbIndex::TWord TWord;

//-------------------------------------------------------------------------
/** Read a word from the input stream.
    @param word_t word type (must be an integer type)
    @param is   [I/O]   input stream
    @param data [O]     storage for the value obtained from the stream
*/
template< typename word_t >
void ReadWord( CNcbiIstream & is, word_t & data )
{
    word_t result;
    is.read( (char *)(&result), sizeof( word_t ) );
    data = result;
}

//-------------------------------------------------------------------------
/** Read the index header information from the given input stream.
    @param map           [I]   pointer to the start of the mapped index data
    @return the index header structure filled with the values read from map
*/
template< bool LEGACY >
const SIndexHeader ReadIndexHeader( void * map );

//-------------------------------------------------------------------------
/** Get the stride value associated with the index.

    @param header the index header structure

    @return the stride value
  */
template< bool LEGACY >
unsigned long GetIndexStride( const SIndexHeader & header );

//-------------------------------------------------------------------------
/** Get the ws_hint value associated with the index.

    @param header the index header structure

    @return the ws_hint value
  */
template< bool LEGACY >
unsigned long GetIndexWSHint( const SIndexHeader & header );

/** Iterator specific functionality of offset list manager class. */
template< typename iterator_t >
class COffsetData;

/** Iterator for 0-terminated pre-ordered offset lists.
  */
class CPreOrderedOffsetIterator
{
    /** Type of offset data class supported by this iterator. */
    typedef COffsetData< CPreOrderedOffsetIterator > TOffsetData;
    typedef COffsetData_Base::TOffsetValue TOffsetValue;

    public:

        CPreOrderedOffsetIterator() : end_( true ) {}

        void Reset(
                const TOffsetData & offset_data, TWord key, unsigned long ws );

        void Reset();

        /** Object constructor.
            @param offset_data  [I] offset data connected to the this object
            @param key          [I] nmer value identifying the offset list
            @param ws           [I] target word size
        */
        CPreOrderedOffsetIterator( 
                const TOffsetData & offset_data, TWord key, unsigned long ws )
        { Reset( offset_data, key, ws ); }

        /** Advance the iterator.
            @return false if the end of the list is reached; true otherwise
        */
        bool Next();

        /** Check if more data is available in the iterator.
            @return true if more data is available; false otherwise
        */
        bool More();

        /** Iterator dereference.
            @return the value pointed to by the interator
        */
        TWord Offset() const { return offset_; }

        bool Advance();

        TOffsetValue getOffsetValue() const
        {
            TOffsetValue r = { special_, (Uint4) (offset_ - min_offset_) };
            return r;
        };

        bool end() const { return end_; }

    private:

        TWord cache_;
        const TWord * start_;
        const TWord * curr_;    /**< Current position in the offset list. */
        TWord special_;
        TWord offset_;          /**< Current cached offset value. */
        unsigned long more_;    /**< Flag indicating that more values are available. */
        unsigned long init_more_;
        unsigned long mod_;     /**< Determines which offsets to skip. */
        bool boundary_;         /**< Flag indicating the current offset is actually
                                     a extra information for boundary cases. */

        unsigned long min_offset_; /**< Minimum offset used by the index. */
        bool end_;
};

//-------------------------------------------------------------------------
/** Type of objects maintaining the offset list data for all Nmers and
    the corresponding hash table.
*/
template< typename iterator_t >
class COffsetData : public COffsetData_Base
{
    friend class CPreOrderedOffsetIterator;

    typedef COffsetData_Base TBase;             /**< Base class alias. */
    typedef CVectorWrap< TWord > TOffsets;      /**< Type used to store offset lists. */

    public:

        /** Type used to iterate over an offset list. */
        typedef iterator_t TIterator;

        /** Construct the object from the data in the given input stream.
            @param is           [I/O]   the input stream containing the object
                                        data
            @param hkey_width   [I]     hash key width
            @param min_offset   [I]     minimum offset used by the index
        */
        COffsetData( 
                CNcbiIstream & is, unsigned long hkey_width,
                unsigned long stride, unsigned long ws_hint );

        /** Constructs the object by mapping to the memory segment.
            @param map          [I/O]   points to the memory segment
            @param hkey_width   [I]     hash key width
            @param stride       [I]     stride of the index
            @param ws_hint      [I]     ws_hint value of the index
        */
        COffsetData( 
                TWord ** map, unsigned long hkey_width, 
                unsigned long stride, unsigned long ws_hint );

    private:

        TOffsets offsets_;      /**< Concatenated offset list data. */
        TWord * data_start_;    /**< Start of the offset data. */
};

//-------------------------------------------------------------------------
INLINE
void CPreOrderedOffsetIterator::Reset( 
        const TOffsetData & offset_data, TWord key, unsigned long ws )
{
    special_ = 0;
    boundary_ = false;
    min_offset_ = offset_data.getMinOffset();
    end_ = false;

    {
        unsigned long h = offset_data.hkey_width() - 1;
        unsigned long s = offset_data.getStride();
        unsigned long w = offset_data.getWSHint();

        init_more_ = more_ = (w - h)/s;
        mod_  = (ws - h)/s;
    }

    cache_ = offset_data.hash_table_[key];

    if( cache_ != 0 ) {
        start_ = curr_ = offset_data.data_start_ + cache_ - 1; 
    }
    else{ 
        curr_ = 0; 
        init_more_ = more_ = 0; 
        end_ = true;
    }
}

//-------------------------------------------------------------------------
INLINE
void CPreOrderedOffsetIterator::Reset()
{
    special_  = 0;
    boundary_ = false;
    end_      = false;
    more_     = init_more_;

    if( cache_ != 0 ) curr_ = start_;
    else {
        curr_ = 0;
        more_ = 0;
        end_  = true;
    }
}

//-------------------------------------------------------------------------
INLINE
bool CPreOrderedOffsetIterator::Next()
{
    if( curr_ == 0 ) return false;
    
    if( (offset_ = *++curr_) == 0 ) {
        more_ = 0;
        end_ = true;
        return false;
    }
    else {
        if( offset_ < min_offset_ ) {
            boundary_ = true;
            special_ = offset_;
            return true;
        }
        else if( boundary_ ) {
            boundary_ = false;
            return true;
        }
        else if( offset_%more_ == 0 ) {
            return true;
        }
        else {
            more_ = (more_ <= mod_) ? 0 : more_ - 1;
            --curr_;
            special_ = 0;
            end_ = true;
            return false;
        }
    }
}

//-------------------------------------------------------------------------
INLINE
bool CPreOrderedOffsetIterator::Advance()
{
    if( Next() ) {
        if( boundary_ ) Next();
        return true;
    }
    
    return false;
}

//-------------------------------------------------------------------------
INLINE
bool CPreOrderedOffsetIterator::More()
{ return more_ != 0; }

//-------------------------------------------------------------------------
template< typename iterator_t >
COffsetData< iterator_t >::COffsetData( 
        TWord ** map, unsigned long hkey_width, 
        unsigned long stride, unsigned long ws_hint )
    : TBase( map, hkey_width, stride, ws_hint )
{
    if( *map ) {
        offsets_.SetPtr( 
                *map, (typename TOffsets::size_type)(this->total_) );
        data_start_ = *map;
        *map += this->total_;
    }
}

//-------------------------------------------------------------------------
/** Some computed type definitions.
  */
template< bool LEGACY >
struct CDbIndex_Traits
{
    typedef COffsetData< CPreOrderedOffsetIterator > TOffsetData;
    typedef CSubjectMap TSubjectMap;
};

/** Implementation of the BLAST database index
*/
template< bool LEGACY >
class CDbIndex_Impl : public CDbIndex
{
    /** Offset data and subject map types computer. */
    typedef CDbIndex_Traits< LEGACY > TTraits;

    public:

        /**@name Some convenience alias declarations. */
        /**@{*/
        typedef typename TTraits::TOffsetData TOffsetData;
        typedef typename TTraits::TSubjectMap TSubjectMap;
        typedef typename TOffsetData::TIterator TOffsetIterator;
        /**@}*/

        /** Size of the index file header for index format version >= 2. */
        static const unsigned long HEADER_SIZE = 16 + 7*sizeof( TWord );

        /** Create an index object from mapped memory segment.
            @param map          [I/O]   points to the memory segment
            @param header       [I]     index header information
            @param idmap        [I]     mapping from ordinal source ids to bioseq ids
            @param data         [I]     index data read from the file in
                                            the case mmap() is not selected.
        */
        CDbIndex_Impl( 
                CMemoryFile * map, 
                const SIndexHeader & header,
                const vector< string > & idmap,
                TWord * data = 0 );

        /** Object destructor. */
        ~CDbIndex_Impl() 
        { 
            delete subject_map_;
            delete offset_data_;
            if( mapfile_ != 0 ) { mapfile_->Unmap(); delete mapfile_; }
            else if ( map_start_ != 0 ) delete[] map_start_;
        }

        /** Get the hash key width of the index.
            @return the hash key width of the index in base pairs
        */
        unsigned long hkey_width() const 
        { return offset_data_->hkey_width(); }

        /** Create an offset list iterator corresponding to the given
            Nmer value.
            @param nmer [I]     the Nmer value
            @param mod  [I]     determines the stride size
            @return the iterator over the offset list corresponding to nmer
        */
        const TOffsetIterator OffsetIterator( TWord nmer, unsigned long mod ) const
        { return TOffsetIterator( *offset_data_, nmer, mod ); }

        /** Get the total number of sequence chunks in the index.
            @sa CSubjectMap::NumChunks()
        */
        TSeqNum NumChunks() const { return subject_map_->NumChunks(); }

        /** Get the total number of logical sequences in the index.
            @sa CSubjectMap::NumSubjects()
        */
        TSeqNum NumSubjects() const { return subject_map_->NumSubjects(); }

        /** Get the subject map instance from the index object.

            @return The subject map instance.
        */
        const TSubjectMap & GetSubjectMap() const
        { return *subject_map_; }

        /** Get the start of compressed raw sequence data.
            @sa CSubjectMap::GetSeqStoreBase()
        */
        const Uint1 * GetSeqStoreBase() const 
        { return subject_map_->GetSeqStoreBase(); }

        /** Get the length of the subject sequence.
            
            @param oid Ordinal id of the subject sequence.

            @return Length of the sequence in bases.
        */
        virtual TSeqPos GetSeqLen( TSeqNum oid ) const
        {
            return subject_map_->GetSeqLen( oid - this->start_ );
        }

        /** Get the sequence data of the subject sequence.
            
            @param oid Ordinal id of the subject sequence.

            @return Pointer to the sequence data.
        */
        virtual const Uint1 * GetSeqData( TSeqNum oid ) const
        {
            return subject_map_->GetSeqData( oid - this->start_ );
        }

        /** Get the index format version.

            @return The index format version.
        */
        virtual unsigned long Version() const { return version_; }

        /** If possible reduce the index footpring by unmapping
            the portion that does not contain sequence data.
        */
        virtual void Remap();

    private:

        /** The search procedure for this specialized index implementation.
            @param query                [I]     the query sequence encoded as BLASTNA
            @param locs                 [I]     set of query locations to search
            @param search_options       [I]     search options
            @return the set of matches of query to sequences present in the
                    index
        */
        virtual CConstRef< CSearchResults > DoSearch(
                const BLAST_SequenceBlk * query, 
                const BlastSeqLoc * locs,
                const SSearchOptions & search_options );

        CMemoryFile * mapfile_;         /**< Memory mapped file. */
        TWord * map_;                   /**< Start of memory mapped file data. */
        TWord * map_start_;             /**< Start of the index data, when not mapped. */
        TOffsetData * offset_data_;     /**< Offset lists. */
        size_t subject_map_offset_;     /**< Offset of the subject map in the index file. */
        unsigned long version_;         /**< Index format version. */
        unsigned long stride_;          /**< Stride value used during index creation. */
};

//-------------------------------------------------------------------------
template< bool LEGACY >
CDbIndex_Impl< LEGACY >::CDbIndex_Impl(
        CMemoryFile * map, const SIndexHeader & header, 
        const vector< string > & idmap, TWord * data )
    : mapfile_( map ), map_start_( 0 ), version_( VERSION ),
      stride_( GetIndexStride< LEGACY >( header ) )
{
    header_ = header;

    start_ = header.start_;
    stop_  = header.stop_;
    start_chunk_ = header.start_chunk_;
    stop_chunk_  = header.stop_chunk_;

    idmap_ = idmap;

    if( mapfile_ != 0 ) {
        map_ = (TWord *)(((char *)(mapfile_->GetPtr())) + HEADER_SIZE);
        offset_data_ = new TOffsetData( 
                &map_, header.hkey_width_, 
                stride_, GetIndexWSHint< LEGACY >( header ) );
        Uint1 * map_start = (Uint1 *)(mapfile_->GetPtr());
        subject_map_offset_ = (Uint1 *)map_ - map_start;
        subject_map_ = new TSubjectMap( &map_, header );
    }
    else if( data != 0 ) {
        map_start_ = data;
        Uint1 * map_start = (Uint1 *)data;
        map_ = (TWord *)((char *)data + HEADER_SIZE);
        offset_data_ = new TOffsetData( 
                &map_, header.hkey_width_, 
                stride_, GetIndexWSHint< LEGACY >( header ) );
        subject_map_offset_ = (Uint1 *)map_ - map_start;
        subject_map_ = new TSubjectMap( &map_, header );
    }
}

//-------------------------------------------------------------------------
template< bool LEGACY >
void CDbIndex_Impl< LEGACY >::Remap()
{
    if( mapfile_ != 0 ) {
        delete subject_map_; subject_map_ = 0;
        delete offset_data_; offset_data_ = 0;
        mapfile_->Unmap();
        map_ = (TWord *)(mapfile_->Map( subject_map_offset_ ));
        subject_map_ = new TSubjectMap( &map_, start_, stop_, stride_ );
    }
}

//-------------------------------------------------------------------------
template< bool LEGACY >
CConstRef< CDbIndex::CSearchResults > 
CDbIndex_Impl< LEGACY >::DoSearch( 
        const BLAST_SequenceBlk * query, 
        const BlastSeqLoc * locs,
        const SSearchOptions & search_options )
{
    return CConstRef< CDbIndex::CSearchResults >( null );
}

//-------------------------------------------------------------------------
CMemoryFile * MapFile( const std::string & fname );

template< bool LEGACY >
CRef< CDbIndex > CDbIndex::LoadIndex( 
        const std::string & fname, bool nomap )
{
    vector< string > idmap;
    string idmap_fname = fname + ".map";
    CNcbiIfstream idmap_stream( idmap_fname.c_str() );

    while( idmap_stream ) {
        string line;
        idmap_stream >> line;
        idmap.push_back( line );
    }

    CRef< CDbIndex > result( null );
    CMemoryFile * map = 0;
    SIndexHeader header;
    TWord * data = 0;

    if( nomap ) {
        Int8 l = CFile( fname ).GetLength();
        CNcbiIfstream s( fname.c_str() );

        try {
            data = new TWord[1 + l/sizeof( TWord )];
        }
        catch( ... ) {
            ERR_POST( "not enough memory for index" );
            NCBI_THROW( 
                    CDbIndex_Exception, eIO, 
                    "not enough memory for index" );
        }

        s.read( (char *)data, l );
        header = ReadIndexHeader< LEGACY >( data );
    }
    else {
        map = MapFile( fname );
        if( map != 0 ) {
            header = ReadIndexHeader< LEGACY >( map->GetPtr() );
        }
    }

    result.Reset( new CDbIndex_Impl< LEGACY >( map, header, idmap, data ) );
    return result;
}

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

#endif

