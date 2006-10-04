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
 *   Implementation of index search functionality.
 *
 */

#include <ncbi_pch.hpp>

#include <algorithm>

#include <corelib/ncbifile.hpp>
#include <algo/blast/core/blast_extend.h>

#ifdef LOCAL_SVN
#include "dbindex.hpp"
#else
#include <algo/blast/dbindex/dbindex.hpp>
#endif

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

#ifdef DO_INLINE
#define INLINE inline
#else
#define INLINE 
#endif

namespace {

/** Forwarding declaration for convenience. */
typedef CDbIndex::TSeqNum TSeqNum;

/** This structure represents the header of the index being loaded. */
template< unsigned long VER >
struct SIndexHeader;

/** Header structure of index format version 1. */
template<>
struct SIndexHeader< 1 >
{
    unsigned long width_;       /**< Bit width of the index (32-bit or 64-bit). */
    unsigned long hkey_width_;  /**< Size in bp of the Nmer used as a hash key. */
    unsigned long off_type_;    /**< Offset type used (raw or subject encoded). */
    unsigned long compression_; /**< Offset list compression method used. */
    TSeqNum start_;             /**< OID of the first sequence in the index. */
    TSeqNum start_chunk_;       /**< Number of the first chunk of the first sequence in the index. */
    TSeqNum stop_;              /**< OID of the last sequence in the index. */
    TSeqNum stop_chunk_;        /**< Number of the last chunk of the last sequence in the index. */
};

/** Header structure of index format version 2. */
template<>
struct SIndexHeader< 2 > : public SIndexHeader< 1 >
{
};

//-------------------------------------------------------------------------
/** Memory map a file and return a pointer to the mapped area.
    @param fname        [I]     file name
    @return pointer to the start of the mapped memory area
*/
CMemoryFile * MapFile( const std::string & fname )
{
    CMemoryFile * result = new CMemoryFile( fname );

    if( result ) {
        if( !result->Map() ) {
            delete result;
            result = 0;
        }
    }

    return result;
}

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
/** Verify that index data represented by the input stream has the version
    consistent with this library.
    @param is   [I/O]   read the version information from this input 
                        stream
    @return true if the stream version is good; false otherwise
*/
bool CheckIndexVersion( CNcbiIstream & is )
{
    unsigned char version;
    ReadWord( is, version );
    return version == CDbIndex::VERSION;
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
/** Read the index header information from the given input stream.
    @param is           [I/O]   input stream for reading the index header data
    @return the index header structure filled with the values read from is
*/
template< unsigned long VER >
const SIndexHeader< VER > ReadIndexHeader( CNcbiIstream & is );

/** Read index header data for index format version 1. */
template<>
const SIndexHeader< 1 > ReadIndexHeader< 1 >( CNcbiIstream & is )
{
    SIndexHeader< 1 > result;

    {
        unsigned char tmp;
        ReadWord( is, tmp );
        result.width_ = tmp;
        ReadWord( is, tmp );
        result.hkey_width_ = tmp;
        ReadWord( is, tmp );
        result.off_type_ = tmp;
        ReadWord( is, tmp );
        result.compression_ = tmp;
    }

    ReadWord( is, result.start_ );
    ReadWord( is, result.start_chunk_ );
    ReadWord( is, result.stop_ );
    ReadWord( is, result.stop_chunk_ );
    return result;
}

/* Forward declaration. */
template< unsigned long VER >
const SIndexHeader< VER > ReadIndexHeader( void * map );

/** Read index header information from memory aread for 
    index format version 2.
    @param WIDTH index bit width
    @param map  [I]     memory area containing index data
    @return appropriate index header structure
*/
template< unsigned long WIDTH >
const SIndexHeader< 2 > ReadIndexHeader_2( void * map )
{
    typedef typename SWord< WIDTH >::type TWord;
    SIndexHeader< 2 > result;
    result.width_ = WIDTH;
    TWord * ptr = (TWord *)((Uint8 *)map + 2);
    result.hkey_width_  = (unsigned long)(*ptr++);
    result.off_type_    = (unsigned long)(*ptr++);
    result.compression_ = (unsigned long)(*ptr++);
    result.start_       = (TSeqNum)(*ptr++);
    result.start_chunk_ = (TSeqNum)(*ptr++);
    result.stop_        = (TSeqNum)(*ptr++);
    result.stop_chunk_  = (TSeqNum)(*ptr++);
    return result;
}

/** Read index header information from the memory area.
    @param VER          index format version
    @param WIDTH        index bit width
    @param map  [I]     memory area containing index data
    @return appropriate index header structure
*/
template< unsigned long VER, unsigned long WIDTH >
const SIndexHeader< VER > ReadIndexHeaderAux( void * map )
{
    switch( VER ) {
        case 2:  return ReadIndexHeader_2< WIDTH >( map );
        default: return ReadIndexHeader< VER >( map );
    }
}

/** Read index header information from the memory area.
    Only index format versions supporting this operation are
    specialized.
    @param VER          index format version
    @param map  [I]     memory area containing index data
    @return appropriate index header structure
*/
template< unsigned long VER >
const SIndexHeader< VER > ReadIndexHeader( void * map ) 
{
    Uint8 width = *(((Uint8 *)map) + 1);

    if( width == WIDTH_32 ) {
        return ReadIndexHeaderAux< VER, WIDTH_32 >( map );
    }
    else if( width == WIDTH_64 ) {
        return ReadIndexHeaderAux< VER, WIDTH_64 >( map );
    }

    return SIndexHeader< VER >();
}

//-------------------------------------------------------------------------
/** A vector or pointer based sequence wrapper.
    Serves as either a std::vector wrapper or holds a constant size
    sequence pointed to by an external pointer.
*/
template< typename T >
class CVectorWrap
{
    typedef std::vector< T > TVector;   /**< Sequence type being wrapped. */

    public:

        /**@name Declarations forwarded from TVector. */
        /**@{*/
        typedef typename TVector::size_type size_type;
        typedef typename TVector::value_type value_type;
        typedef typename TVector::reference reference;
        typedef typename TVector::const_reference const_reference;
        /**@}*/

        /** Iterator type pointing to const data. */
        typedef const T * const_iterator;

        /** Object constructor.
            Initializes the object as a std::vector wrapper.
            @param sz   [I]     initial size
            @param v    [I]     initial element value
        */
        CVectorWrap( size_type sz = 0, T v = T() )
            : base_( 0 ), data_( sz, v ), vec_( true )
        { if( !data_.empty() ) base_ = &data_[0]; }

        /** Make the object hold an external sequence.
            @param base [I]     pointer to the external sequence
            @param sz   [I]     size of the external sequence
        */
        void SetPtr( T * base, size_type sz ) 
        {
            base_ = base;
            vec_ = false;
            size_ = sz;
        }

        /** Indexing operator.
            @param n    [I]     index
            @return reference to the n-th element
        */
        reference operator[]( size_type n )
        { return base_[n]; }

        /** Indexing operator.
            @param n    [I]     index
            @return reference to constant value of the n-th element.
        */
        const_reference operator[]( size_type n ) const
        { return base_[n]; }

        /** Change the size of the sequence.
            Only works when the object holds a std::vector.
            @param n    [I]     new sequence size
            @param v    [I]     initial value for newly created elements
        */
        void resize( size_type n, T v = T() )
        { 
            if( vec_ ) {
                data_.resize( n, v ); 
                base_ = &data_[0];
            }
        }

        /** Get the sequence size.
            @return length of the sequence
        */
        size_type size() const
        { return vec_ ? data_.size() : size_; }

        /** Get the start of the sequence.
            @return iterator pointing to the beginning of the sequence.
        */
        const_iterator begin() const { return base_; }

        /** Get the end of the sequence.
            @return iterator pointing to past the end of the sequence.
        */
        const_iterator end() const
        { return vec_ ? base_ + data_.size() : base_ + size_; }

    private:

        T * base_;          /**< Pointer to the first element of the sequence. */
        TVector data_;      /**< std::vector object wrapped by this object. */
        bool vec_;          /**< Flag indicating whether it is a wrapper or a holder of external sequence. */
        size_type size_;    /**< Size of the external sequence. */
};

//-------------------------------------------------------------------------
/** Class representing index hash table and offset list database.
    This class implements the data structure functionality that is
    independent from the particulars of the compression method.
    @param word_t       word type (integer type) used by the data structure
    @param COMPRESSION  compression type to used for offset lists
*/
template< typename word_t, unsigned long COMPRESSION >
class COffsetData_Base
{
    public:

        typedef word_t TWord;   /**< Alias for the word type. */

        /** The type of the hash table.
            The hash table implements the mapping from Nmer values to
            the corresponding offset lists.
        */
        typedef CVectorWrap< TWord > THashTable;

        /** Type of compression used by this data structure. */
        static const unsigned long COMPRESSION_VALUE = COMPRESSION;

        /** Object constructor.
            Creates the object by reading data from the given input
            stream.
            @param is           [I/O]   the input stream containing the 
                                        index data
            @param hkey_width   [I]     width in bp of the hash key
        */
        COffsetData_Base( CNcbiIstream & is, unsigned long hkey_width );

        /** Object constructor.
            Creates the object by mapping data from a memory segment.
            @param map          [I/O]   pointer to the memory segment
            @param hkey_width   [I]     width in bp of the hash key
        */
        COffsetData_Base( TWord ** map, unsigned long hkey_width );

        /** Get the width of the hash key in base pairs.
            @return hash key width
        */
        unsigned long hkey_width() const { return hkey_width_; }

    protected:

        /** Auxiliary data member used for importing the offset 
            list data.
        */
        TWord total_;

        unsigned long hkey_width_;      /**< Hash key width in bp. */

    public:
        THashTable hash_table_;         /**< The hash table (mapping from
                                             Nmer values to the lists of
                                             offsets. */
};

//-------------------------------------------------------------------------
template< typename word_t, unsigned long COMPRESSION >
COffsetData_Base< word_t, COMPRESSION >::COffsetData_Base( 
        CNcbiIstream & is, unsigned long hkey_width )
    : total_( 0 ), hkey_width_( hkey_width )
{
    ReadWord( is, total_ );
    TWord hash_table_size = ((TWord)1)<<(2*hkey_width_);
    hash_table_.resize( 
            (typename THashTable::size_type)hash_table_size + 1, 0 );
    TWord * hash_table_start = &hash_table_[0];
    is.read( 
            (char *)hash_table_start, 
            (std::streamsize)(sizeof( TWord )*hash_table_size) );
    hash_table_[(typename THashTable::size_type)hash_table_size] = total_;
}

//-------------------------------------------------------------------------
template< typename word_t, unsigned long COMPRESSION >
COffsetData_Base< word_t, COMPRESSION >::COffsetData_Base( 
        TWord ** map, unsigned long hkey_width )
    : total_( 0 ), hkey_width_( hkey_width )
{
    if( *map ) {
        total_ = *(*map)++;
        TWord hash_table_size = ((TWord)1)<<(2*hkey_width_);
        hash_table_.SetPtr( 
                *map, 
                (typename THashTable::size_type)(hash_table_size + 1) );
        *map += hash_table_size + 1;
    }
}

//-------------------------------------------------------------------------
/** COMPRESSION specific functinality of offset list manager class. */
template< typename word_t, unsigned long COMPRESSION >
class COffsetData;

/** Interface for iterating over the offset lists.
    Normally uncompressed offset values are represented in word_t.
    @param word_t       word type (integer type)
    @param COMPRESSION  compression type to used for offset lists
*/
template< typename word_t, unsigned long COMPRESSION >
class COffsetIterator;

/** Specialization of offset offset list iterator for uncompressed offset
    lists.
*/
template< typename word_t >
class COffsetIterator< word_t, UNCOMPRESSED >
{
    /**@name Some convenient type declarations. */
    /**@{*/
    typedef COffsetData< word_t, UNCOMPRESSED > TOffsetData;
    typedef word_t TWord;
    /**@}*/

    public:

        /** Object constructor.
            @param offset_data  [I]     Combined offset data for all keys
            @param key          [I]     Hash key defining the offset list
        */
        COffsetIterator( const TOffsetData & offset_data, TWord key );

        /** Advance the iterator.
            @return false if the end of the offset list has been reached;
                    true otherwise
        */
        bool Next();

        /** Get the offset value corresponding to the current position of
            the iterator.
            @return the current offset value
        */
        TWord Offset() const { return offset_; }

    private:

        const TWord * start_;   /**< Current position in the offset list. */
        TWord len_;             /**< Number of offsets left in the list. */
        TWord offset_;          /**< Current offset value. */
};

//-------------------------------------------------------------------------
/** Type of objects maintaining the offset list data for all Nmers and
    the corresponding hash table.
    @param word_t word size used for uncompressed offsets
*/
template< typename word_t >
class COffsetData< word_t, UNCOMPRESSED >
    : public COffsetData_Base< word_t, UNCOMPRESSED >
{
    // friend class COffsetIterator< word_t, UNCOMPRESSED >;

    typedef COffsetData_Base< word_t, UNCOMPRESSED > TBase;     /**< Base class alias. */
    typedef typename TBase::TWord TWord;                        /**< Forward from TBase for convenience. */

    typedef CVectorWrap< TWord > TOffsets;      /**< Type used to store offset lists. */

    public:

        /** Type used to iterate over an offset list. */
        typedef COffsetIterator< word_t, UNCOMPRESSED > TIterator;

        /** Construct the object from the data in the given input stream.
            @param is           [I/O]   the input stream containing the object
                                        data
            @param hkey_width   [I]     hash key width
        */
        COffsetData( CNcbiIstream & is, unsigned long hkey_width );

        /** Constructs the object by mapping to the memory segment.
            @param map          [I/O]   points to the memory segment
            @param hkey_width   [I]     hash key width
        */
        COffsetData( TWord ** map, unsigned long hkey_width );

    // private:

        TOffsets offsets_;      /**< Concatenated offset list data. */
};

//-------------------------------------------------------------------------
template< typename word_t > 
INLINE
COffsetIterator< word_t, UNCOMPRESSED >::COffsetIterator(
        const TOffsetData & offset_data, TWord key )
{
    start_ = &offset_data.offsets_[0] + 
        offset_data.hash_table_[
                (typename TOffsetData::THashTable::size_type)key];
    len_ = 
        offset_data.hash_table_[
            (typename TOffsetData::THashTable::size_type)(key+1)] - 
        offset_data.hash_table_[
                (typename TOffsetData::THashTable::size_type)key];
}

//-------------------------------------------------------------------------
template< typename word_t > 
INLINE
bool COffsetIterator< word_t, UNCOMPRESSED >::Next()
{
    if( len_ == 0 ) return false;
    offset_ = *start_++;
    --len_;
    return true;
}

//-------------------------------------------------------------------------
template< typename word_t >
COffsetData< word_t, UNCOMPRESSED >::COffsetData( 
        CNcbiIstream & is, unsigned long hkey_width )
    : TBase( is, hkey_width ), offsets_( this->total_, 0 )
{
    is.read( 
            (char *)(&offsets_[0]), 
            sizeof( TWord )*(std::streamsize)(this->total_) );
}

//-------------------------------------------------------------------------
template< typename word_t >
COffsetData< word_t, UNCOMPRESSED >::COffsetData( 
        TWord ** map, unsigned long hkey_width )
    : TBase( map, hkey_width )
{
    if( *map ) {
        offsets_.SetPtr( 
                *map, (typename TOffsets::size_type)(this->total_) );
        *map += this->total_;
    }
}

//-------------------------------------------------------------------------
/** Type representing the part of the subject sequence data independent
    of the type of offset encoding.
    @param word_t       word type used to represent offsets
    @param OFF_TYPE     type of offset encoding
*/
template< typename word_t, unsigned long OFF_TYPE >
class CSubjectMap_Base
{
    protected:

        typedef word_t TWord;   /**< Convenience declaration. */

        /** Type used to map database oids to the chunk info. */
        typedef CVectorWrap< TWord > TSubjects;

        /** Type used for compressed subject sequence data storage. */
        typedef CVectorWrap< Uint1 > TSeqStore;

    public:

        /** Constructs object from the data in the given input stream.
            @param is           [I/O]   input stream containing the 
                                        object data
            @param start        [I]     database oid of the first sequence
                                        in the map
            @param stop         [I]     database oid of the last sequence
                                        in the map
        */
        CSubjectMap_Base( CNcbiIstream & is, TSeqNum start, TSeqNum stop );

        /** Constructs object by mapping to the memory segment.
            @param map          [I/O]   pointer to the memory segment
            @param start        [I]     database oid of the first sequence
                                        in the map
            @param stop         [I]     database oid of the last sequence
                                        in the map
        */
        CSubjectMap_Base( TWord ** map, TSeqNum start, TSeqNum stop );

        /** Return the start of the raw storage for compressed subject 
            sequence data.
            @return start of the sequence data storage
        */
        const Uint1 * GetSeqStoreBase() const { return &seq_store_[0]; }

    protected:

        /** Read the raw subject sequence data from the given input stream.
            @param is   [I/O]   stream for reading the sequence data
        */
        void ReadSeqData( CNcbiIstream & is );

        /** Set up the sequence store from the memory segment.
            @param map  [I/O]   points to the memory segment
        */
        void SetSeqDataFromMap( TWord ** map );

        TSubjects subjects_;    /**< Mapping from database oids to the chunk info. */
        TSeqStore seq_store_;   /**< Storage for the raw subject sequence data. */
        TWord total_;           /**< Size in bytes of the raw sequence storage.
                                     (only valid after the complete object has
                                     been constructed) */
};

//-------------------------------------------------------------------------
template< typename word_t, unsigned long OFF_TYPE >
CSubjectMap_Base< word_t, OFF_TYPE >::CSubjectMap_Base( 
        CNcbiIstream & is, TSeqNum start, TSeqNum stop )
    : total_( 0 )
{
    typename TSubjects::size_type n_subjects 
        = (typename TSubjects::size_type)(stop - start + 1);
    subjects_.resize( n_subjects, 0 );
    ReadWord( is, total_ );
    is.read( (char *)(&subjects_[0]), sizeof( TSeqNum )*n_subjects );
    total_ -= sizeof( TSeqNum )*n_subjects;
}

//-------------------------------------------------------------------------
template< typename word_t, unsigned long OFF_TYPE >
CSubjectMap_Base< word_t, OFF_TYPE >::CSubjectMap_Base( 
        TWord ** map, TSeqNum start, TSeqNum stop )
    : total_( 0 )
{
    if( *map ) {
        typename TSubjects::size_type n_subjects 
            = (typename TSubjects::size_type)(stop - start + 1);
        total_ = *(*map)++;
        subjects_.SetPtr( *map, n_subjects );
        total_ -= sizeof( TWord )*n_subjects;
        *map += n_subjects;
    }
}

//-------------------------------------------------------------------------
template< typename word_t, unsigned long OFF_TYPE >
void CSubjectMap_Base< word_t, OFF_TYPE >::ReadSeqData( 
        CNcbiIstream & is )
{
    ReadWord( is, total_ );
    seq_store_.resize( (TSeqStore::size_type)total_, 0 );
    is.read( (char *)(&seq_store_[0]), (std::streamsize)total_ );
}

//-------------------------------------------------------------------------
template< typename word_t, unsigned long OFF_TYPE >
void CSubjectMap_Base< word_t, OFF_TYPE >::SetSeqDataFromMap( 
        TWord ** map )
{
    if( *map ) {
        total_ = *(*map)++;
        seq_store_.SetPtr( (Uint1 *)*map, (TSeqStore::size_type)total_ );
        *map += 1 + total_/sizeof( TWord );
    }
}

//-------------------------------------------------------------------------
/** Subject mapper functinality dependant on the offset encoding. */
template< typename word_t, unsigned long OFF_TYPE >
class CSubjectMap {};

/** Details of the database to internal sequence mapping that are 
    specific for the raw offset encoding.
    @param word_t bit width of the corresponding index type
*/
template< typename word_t >
class CSubjectMap< word_t, OFFSET_RAW > 
    : public CSubjectMap_Base< word_t, OFFSET_RAW >
{
    typedef CSubjectMap_Base< word_t, OFFSET_RAW > TBase;       /**< Alias for convenience. */
    typedef typename TBase::TWord TWord;                        /**< Alias for convenience. */

    /** Type for storing the chunk data.
        For raw offset encoding the offset into the vector serves also
        as the internal logical sequence id.
    */
    typedef CVectorWrap< TWord > TChunks;

    public:

        /** Constructs the object from the data in the given input stream.
            @param is           [I/O]   input stream for reading the object data
            @param start        [I]     database oid of the first sequence in the map
            @param stop         [I]     database oid of the last sequence in the map
        */
        CSubjectMap( CNcbiIstream & is, TSeqNum start, TSeqNum stop );

        /** Constructs the object from the mapped memory segment.
            @param map          [I/O]   points to the memory segment
            @param start        [I]     database oid of the first sequence in the map
            @param stop         [I]     database oid of the last sequence in the map
        */
        CSubjectMap( TWord ** map, TSeqNum start, TSeqNum stop );

        /** Advance the current sequence id according to the given offset
            value.
            Searches forward starting with the curr_subj until it finds
            the subject id of the sequence that coveres position given
            by offset. Returns the first and last offsets of the sequence
            found in the 2nd and 3rd arguments.
            @param curr_subj    [I]     subject id from which to start searching
            @param start_off    [O]     starting offset of the result sequence
            @param end_off      [O]     ending offset of the result sequence
            @param start        [O]     starting position of the result sequence
            @param end          [O]     ending position of the result sequence
            @param offset       [I]     target raw offset value
            @return id of the sequence containing the target offset
        */
        TSeqNum Update( 
                TSeqNum curr_subj, TWord & start_off, TWord & end_off,
                TWord & start, TWord & end, TWord offset ) const;

        /** Return the subject information based on the given logical subject
            id.
            @param subj         [I]     logical subject id
            @param start_off    [O]     smallest offset value for subj
            @param end_off      [O]     smallest offset value for subj + 1
            @param start        [0]     starting offset of subj in the sequence store
            @param end          [0]     1 + ending offset of subj in the sequence store
        */
        void SetSubjInfo( 
                TSeqNum subj, TWord & start_off, TWord & end_off,
                TWord & start, TWord & end ) const;

        /** Get the total number of sequence chunks in the map.
            @return number of chunks in the map
        */
        TSeqNum NumSubjects() const { return (TSeqNum)(chunks_.size()); }

        /** Get the logical sequence id from the database oid and the
            chunk number.
            @param subject      [I]     database oid
            @param chunk        [I]     the chunk number
            @return logical sequence id corresponding to subject and chunk
        */
        TSeqNum MapSubject( TSeqNum subject, TSeqNum chunk ) const
        {
            if( subject < this->subjects_.size() ) {
                TSeqNum result = 
                    (TSeqNum)(this->subjects_[subject]) + chunk;

                if( result < chunks_.size() ) {
                    return result;
                }
            }

            return 0;
        }

    private:

        TChunks chunks_;        /**< Collection of individual chunk descriptors. */
        TChunks chunks_off_;    /**< Encoded starting offsets of the sequences in seq_store_. */
};

//-------------------------------------------------------------------------
template< typename word_t >
CSubjectMap< word_t, OFFSET_RAW >::CSubjectMap(
        CNcbiIstream & is, TSeqNum start, TSeqNum stop )
    : TBase( is, start, stop )
{
    if( this->total_%sizeof( TWord ) != 0 ) {
        NCBI_THROW( 
                CDbIndex_Exception, eBadData, 
                "bad alignment of subject map data" );
    }

    chunks_.resize( 
            (typename TChunks::size_type)(
                1 + this->total_/sizeof( TWord )), 
            0 );
    chunks_off_.resize( 
            (typename TChunks::size_type)(
                1 + this->total_/sizeof( TWord )), 
            0 );
    is.read( (char *)(&chunks_[0]), (std::streamsize)(this->total_) );

    for( typename TChunks::size_type i = 0; 
            i < chunks_.size() - 1; ++i ) {
        chunks_off_[i] = CDbIndex::MIN_OFFSET +
                chunks_[i]*(CDbIndex::CR)/(CDbIndex::STRIDE);
    }

    this->ReadSeqData( is );
    chunks_[chunks_.size() - 1] = this->seq_store_.size();
    chunks_off_[chunks_.size() - 1] = CDbIndex::MIN_OFFSET +
        chunks_[chunks_.size() - 1]*(CDbIndex::CR)/(CDbIndex::STRIDE);
}

//-------------------------------------------------------------------------
template< typename word_t >
CSubjectMap< word_t, OFFSET_RAW >::CSubjectMap(
        TWord ** map, TSeqNum start, TSeqNum stop )
    : TBase( map, start, stop )
{
    if( *map ){
        typename TChunks::size_type chunks_size = 
            (typename TChunks::size_type)(
                1 + this->total_/sizeof( TWord ));
        chunks_.SetPtr( *map, chunks_size ); 
        chunks_off_.resize( chunks_size );

        for( typename TChunks::size_type i = 0; 
                i < chunks_.size() - 1; ++i ) {
            chunks_off_[i] = CDbIndex::MIN_OFFSET +
                    chunks_[i]*(CDbIndex::CR)/(CDbIndex::STRIDE);
        }

        chunks_off_[chunks_.size() - 1] = CDbIndex::MIN_OFFSET +
            chunks_[chunks_.size() - 1]*(CDbIndex::CR)/(CDbIndex::STRIDE);
        *map += chunks_size;
        this->SetSeqDataFromMap( map );
    }
}

//-------------------------------------------------------------------------
template< typename word_t > 
INLINE
void CSubjectMap< word_t, OFFSET_RAW >::SetSubjInfo( 
        TSeqNum subj, TWord & start_off, TWord & end_off, 
        TWord & start, TWord & end ) const
{
    end       = chunks_[subj + 1];
    end_off   = chunks_off_[subj + 1];
    start     = chunks_[subj];
    start_off = chunks_off_[subj];
}

//-------------------------------------------------------------------------
template< typename word_t > 
INLINE
TSeqNum CSubjectMap< word_t, OFFSET_RAW >::Update(
        TSeqNum curr_subj, TWord & start_off, TWord & end_off,
        TWord & start, TWord & end, TWord offset ) const
{
    typename TChunks::const_iterator it = std::lower_bound( 
            chunks_off_.begin(), chunks_off_.end(), offset );
    ASSERT( it != chunks_off_.end() );
    curr_subj = it - chunks_off_.begin();

    end = chunks_[curr_subj];
    end_off = chunks_off_[curr_subj];
    start = chunks_[--curr_subj];
    start_off = chunks_off_[curr_subj];

    return curr_subj;
}

//-------------------------------------------------------------------------
/** Type used to iterate over the consecutive Nmer values of the query
    sequence.
    @param index_impl_t database index implementation type
*/
template< typename index_impl_t >
class CNmerIterator
{
    public:

        /**@name Some useful aliases. */
        /**@{*/
        typedef index_impl_t TIndex;
        typedef typename TIndex::TWord TWord;
        /**@}*/

        /** Object constructor.
            @param index_impl   [I]     index implementation object
            @param query        [I]     query sequence in BLASTNA encoding
            @param start        [I]     start position in the query
            @param stop         [I]     one past the last position in the query
        */
        CNmerIterator( 
                const TIndex & index_impl, 
                const Uint1 * query, TSeqPos start, TSeqPos stop );

        /** Advance the iterator.
            @return false if end of the sequence range has been reached,
                    true otherwise
        */
        bool Next();

        /** Get the current position in the query sequence.
            The position returned corresponds to the last base of the
            current Nmer value.
            @return position in the query corresponding to the current
                    state of the iterator object
        */
        TSeqPos Pos() const { return pos_ - 1; }

        /** Get the Nmer value corresponding to the current state of the
            iterator object.
            @return current Nmer value
        */
        TWord Nmer() const { return nmer_; }

    public:

        const Uint1 * query_;           /**< The query data (BLASTNA encoded). */
        bool state_;                    /**< false, if the end of the sequence has been reached. */
        TSeqPos pos_;                   /**< Position returned by Pos(). */
        TSeqPos stop_;                  /**< One past the last position in the query. */
        TWord nmer_;                    /**< Nmer value reported by Nmer(). */
        TSeqPos count_;                 /**< Auxiliary member used to determine the next valid position. */
        unsigned long hkey_width_;      /**< Hash key width (in base pairs). */
        TWord hkey_mask_;               /**< Hash key mask. */
};

//-------------------------------------------------------------------------
template< typename index_impl_t > 
INLINE
CNmerIterator< index_impl_t >::CNmerIterator(
        const TIndex & index_impl, const Uint1 * query, 
        TSeqPos start, TSeqPos stop )
    : query_( query ), state_( true ), 
      pos_( start ), stop_( stop ), nmer_( 0 ), count_( 0 ),
      hkey_width_( index_impl.hkey_width() )
{
    hkey_mask_ = (1<<(2*hkey_width_)) - 1;
    query_ += pos_;
}

//-------------------------------------------------------------------------
template< typename index_impl_t > 
INLINE
bool CNmerIterator< index_impl_t >::Next()
{
    if( state_ ) {
        while( pos_ < stop_ ) {
            TWord letter = (TWord)*(query_++);
            ++pos_;

            if( letter < 4 ) {
                nmer_ = ((nmer_<<2)&hkey_mask_) + letter;
                ++count_;
                if( count_ >= hkey_width_ ) return true;
            }else {
                count_ = 0;
                nmer_ = 0;
            }
        }

        state_ = false;
    }

    return state_;
}

//-------------------------------------------------------------------------
/** Representation of a seed root.
    A seed root is the initial match of Nmers found by a hash table lookup.
*/
struct SSeedRoot
{
    TSeqPos qoff_;      /**< Query offset. */
    TSeqPos soff_;      /**< Corresponding subject offset. */
    TSeqPos qstart_;    /**< Start of the corresponding query interval. */
    TSeqPos qstop_;     /**< 1 + end of the corresponding query interval. */
};

/** SSeedRoot container for one subject. */
struct SSubjRootsInfo
{
    /** Container implementation type. */
    typedef std::vector< SSeedRoot > TRoots; 

    /** Clean up extra allocated memory. */
    void CleanUp()
    {
        if( extra_roots_ != 0 ) {
            delete extra_roots_;
        }
    }

    unsigned int len_;          /**< Current number of stored roots. */
    TRoots * extra_roots_;      /**< Storage for extra roots. Allocated
                                     only if preallocated storage 
                                     overfills. */
};

/** Seed roots container for all subjects. */
class CSeedRoots
{
    /** Alias type for convenience. */
    typedef SSubjRootsInfo::TRoots TRoots;

    public:

        /** Object constructor.
            @param num_subjects [I]     number of subjects sequences
        */
        CSeedRoots( TSeqNum num_subjects = 0 );

        /** Object destructor. */
        ~CSeedRoots() { CleanUp(); }

        /** Append a normal (non boundary) root to the container.
            @param root         [I]     root to append
            @param subject      [I]     subject sequence containing
                                        root.soff_ of the root
        */
        void Add( const SSeedRoot & root, TSeqNum subject );

        /** Append a boundary root (both parts) to the container.
            @param root1        [I]     boundary root structure
            @param root2        [I]     real root data
            @param subject      [I]     subject sequence containing
                                        root2.soff_.
        */
        void Add2( 
                const SSeedRoot & root1, const SSeedRoot & root2,
                TSeqNum subject );

        /** Get the set of roots for a particular subject.
            @param subject      [I]     local subject id
            @return reference to SSubjRootsInfo describing the given
                    subject
        */
        const SSubjRootsInfo & GetSubjInfo( TSeqNum subject ) const
        { return rinfo_[subject]; }

        /** Return the preallocated array of roots for a particular
            subject.
            @param subject      [I]     local subject id
            @return preallocated array for storing roots for the
                    given subject
        */
        const SSeedRoot * GetSubjRoots( TSeqNum subject ) const
        { return roots_ + (subject<<subj_roots_len_bits_); }

    private:

        /** Assumption on the amound of cache in the system.
            (overly optimistic)
        */
        static const unsigned long TOTAL_CACHE = 4*1024*1024; 

        /** Clean up all the dynamically allocated memory. */
        void CleanUp()
        {
            for( TSeqNum i = 0; i < num_subjects_; ++i ) {
                rinfo_[i].CleanUp(); 
            }

            delete[] rinfo_;
            delete[] roots_;
        }

        TSeqNum num_subjects_;                  /**< Number of subjects in the index. */
        unsigned long subj_roots_len_bits_;     /**< Log_2 of n_subj_roots_. */
        unsigned long n_subj_roots_;            /**< Space is preallocated for this number of roots per subject. */
        SSeedRoot * roots_;                     /**< Roots array preallocated for all subjects. */
        SSubjRootsInfo * rinfo_;                /**< Array of root information structures for each subject.
                                                     Dynamically allocated. */
};

CSeedRoots::CSeedRoots( TSeqNum num_subjects )
    : num_subjects_( num_subjects ), subj_roots_len_bits_( 7 ), 
      roots_( 0 ), rinfo_( 0 )
{
    unsigned long total_roots = (num_subjects_<<subj_roots_len_bits_);

    while( total_roots*sizeof( SSeedRoot ) < TOTAL_CACHE ) {
        ++subj_roots_len_bits_;
        total_roots <<= 1;
    }

    n_subj_roots_ = (1<<subj_roots_len_bits_);

    try {
        roots_ = new SSeedRoot[total_roots];
        rinfo_ = new SSubjRootsInfo[num_subjects_];

        for( TSeqNum i = 0; i < num_subjects_; ++i ) {
            SSubjRootsInfo t = { 0, 0 };
            rinfo_[i] = t;
        }
    }catch( ... ) { 
        CleanUp(); 
        throw;
    }
}

INLINE
void CSeedRoots::Add( const SSeedRoot & root, TSeqNum subject )
{
    SSubjRootsInfo & rinfo = rinfo_[subject];

    if( rinfo.len_ < n_subj_roots_ ) {
        *(roots_ + (subject<<subj_roots_len_bits_) + (rinfo.len_++)) 
            = root;
    }else {
        if( rinfo.extra_roots_ == 0 ) {
            rinfo.extra_roots_ = new TRoots;
            rinfo.extra_roots_->reserve( n_subj_roots_<<2 );
        }

        rinfo.extra_roots_->push_back( root );
    }
}

INLINE
void CSeedRoots::Add2( 
        const SSeedRoot & root1, 
        const SSeedRoot & root2, 
        TSeqNum subject )
{
    SSubjRootsInfo & rinfo = rinfo_[subject];

    if( rinfo.len_ < n_subj_roots_ - 1 ) {
        *(roots_ + (subject<<subj_roots_len_bits_) + (rinfo.len_++)) 
            = root1;
        *(roots_ + (subject<<subj_roots_len_bits_) + (rinfo.len_++)) 
            = root2;
    }else {
        if( rinfo.extra_roots_ == 0 ) {
            rinfo.extra_roots_ = new TRoots;
            rinfo.extra_roots_->reserve( n_subj_roots_<<2 );
        }

        rinfo.extra_roots_->push_back( root1 );
        rinfo.extra_roots_->push_back( root2 );
    }
}

//-------------------------------------------------------------------------
/** Representation of a seed being tracked by the search algorithm.
*/
struct STrackedSeed
{
    TSeqPos qoff_;      /**< Query offset of the seed's origin. */
    TSeqPos soff_;      /**< Subject offset of the seed's origin. */
    TSeqPos len_;       /**< Length of the seed. */
    TSeqPos qright_;    /**< Offset of the rightmost position of the seed in the query. */
};

/** Representation of a collection of tacked seeds for a specific subject
    sequence.
*/
class CTrackedSeeds
{
    /**@name Some convenience type declaration. */
    /**@{*/
    typedef std::vector< STrackedSeed > TSeeds;
    typedef TSeeds::const_iterator TIter;
    /**@}*/

    public:

        /** Object constructor. */
        CTrackedSeeds() 
            : hitlist_( 0 )
        {
            seeds_ = &seeds_1_;
            new_seeds_ = &seeds_2_;
            it_ = seeds_->begin();
        }

        /** Object copy constructor.
            @param rhs  [I]     source object to copy
        */
        CTrackedSeeds( const CTrackedSeeds & rhs )
            : hitlist_( rhs.hitlist_ ), 
              seeds_1_( rhs.seeds_1_ ), seeds_2_( rhs.seeds_2_ )
        {
            if( rhs.seeds_ == &rhs.seeds_1_ ) {
                seeds_     = &seeds_1_;
                new_seeds_ = &seeds_2_;
            }else {
                seeds_     = &seeds_2_;
                new_seeds_ = &seeds_1_;
            }

            it_ = seeds_->begin();
        }

        /** Prepare for processing of the next query position. */
        void Reset();

        /** Process seeds on diagonals below or equal to the seed
            given as the parameter.
            @param seed [I]     possible candidate for a 'tracked' seed
            @return true if there is a tracked seed on the same diagonal
                    as seed; false otherwise
        */
        bool EvalAndUpdate( const STrackedSeed & seed );

        /** Add a seed to the set of tracked seeds.
            @param seed         [I]     seed to add
            @param word_size    [I]     minimum size of a valid seed
        */
        void Append( const STrackedSeed & seed, unsigned long word_size );

        /** Add a seed to the set of tracked seeds.
            No check for word size is performed.
            @param seed         [I]     seed to add
        */
        void AppendSimple( const STrackedSeed & seed );

        /** Save the remaining valid tracked seeds and clean up the 
            structure.
        */
        void Finalize();

        /** Save the tracked seed for reporting in the search result set.
            @param seed [I]     seed to save
        */
        void SaveSeed( const STrackedSeed & seed );

        /** Get the list of saved seeds.
            @return the results set for the subject sequence to which
                    this object corresponds
        */
        BlastInitHitList * GetHitList() const { return hitlist_; }

    private:

        BlastInitHitList * hitlist_;    /**< The result set. */
        TSeeds seeds_1_;                /**< Storage for tracked seeds. */
        TSeeds seeds_2_;                /**< Storage for tracked seeds. */
        TSeeds * seeds_;                /**< Seeds currently being tracked. */
        TSeeds * new_seeds_;            /**< Seeds to keep for the next iteration. */
        TIter it_;                      /**< Iterator pointing to the tracked seed that
                                             is about to be inspected. */
};

//-------------------------------------------------------------------------
INLINE
void CTrackedSeeds::Reset()
        {
            for( ; it_ != seeds_->end(); ++it_ ) {
                new_seeds_->push_back( *it_ );
            }

            std::swap( seeds_, new_seeds_ );
            new_seeds_->clear();
            it_ = seeds_->begin();
        }
//-------------------------------------------------------------------------
INLINE
void CTrackedSeeds::SaveSeed( const STrackedSeed & seed )
{
    if( seed.len_ > 0 ) {
        if( hitlist_ == 0 ) {
            hitlist_ = BLAST_InitHitListNew();
        }

        TSeqPos qoff = seed.qright_ - seed.len_ + 1;
        TSeqPos soff = seed.soff_ - (seed.qoff_ - qoff);
        BLAST_SaveInitialHit( hitlist_, (Int4)qoff, (Int4)soff, 0 );

#ifdef SEEDDEBUG
        cerr << "SEED: " << qoff << "\t" << soff << "\t"
            << seed.len_ << "\n";
#endif
    }
}

//-------------------------------------------------------------------------
INLINE
void CTrackedSeeds::Finalize()
{
    for( TSeeds::const_iterator cit = seeds_->begin(); 
            cit != seeds_->end(); ++cit ) {
        SaveSeed( *cit );
    }
}

//-------------------------------------------------------------------------
INLINE
void CTrackedSeeds::AppendSimple( const STrackedSeed & seed )
{ new_seeds_->push_back( seed ); }

//-------------------------------------------------------------------------
INLINE
void CTrackedSeeds::Append( 
        const STrackedSeed & seed, unsigned long word_size )
{
    if( !new_seeds_->empty() ) {
        STrackedSeed & bs = new_seeds_->back();
        TSeqPos step = seed.qoff_ - bs.qoff_;
        TSeqPos bs_soff_corr = bs.soff_ + step;

        if( bs_soff_corr == seed.soff_ ) {
            if( seed.qright_ < bs.qright_ ) {
                bs.len_ -= (bs.qright_ - seed.qright_);

                if( bs.len_ < word_size ) {
                    new_seeds_->pop_back();
                }else {
                    bs.qright_ = seed.qright_;
                }
            }
        }else if( seed.len_ >= word_size ) {
            new_seeds_->push_back( seed );
        }
    }else if( seed.len_ >= word_size ) {
        new_seeds_->push_back( seed );
    }
}

//-------------------------------------------------------------------------
INLINE
bool CTrackedSeeds::EvalAndUpdate( const STrackedSeed & seed )
{
    while( it_ != seeds_->end() ) {
        TSeqPos step = seed.qoff_ - it_->qoff_;
        TSeqPos it_soff_corr = it_->soff_ + step;
        if( it_soff_corr > seed.soff_ ) return true;

        if( it_->qright_ < seed.qoff_ ) {
            SaveSeed( *it_ );
            ++it_;
        }else {
            new_seeds_->push_back( *it_ );
            ++it_;
            if( it_soff_corr == seed.soff_ ) return false;
        }
    }

    return true;
}

//-------------------------------------------------------------------------
/** This is the object representing the state of a search over the index.
    Use of a separate class for searches allows for multiple simultaneous
    searches against the same index.
    @param index_impl_t index implementation type
*/
template< typename index_impl_t >
class CSearch
{
    typedef CDbIndex::SSearchOptions TSearchOptions;    /**< Alias for convenience. */

    public:

        typedef index_impl_t TIndex_Impl;       /**< Alias for convenience. */

        /** Object constructor.
            @param index_impl   [I]     the index implementation object
            @param query        [I]     query data encoded in BLASTNA
            @param locs         [I]     set of query locations to search
            @param options      [I]     search options
        */
        CSearch( 
                const TIndex_Impl & index_impl,
                const BLAST_SequenceBlk * query,
                const BlastSeqLoc * locs,
                const TSearchOptions & options );

        /** Performs the search.
            @return the set of seeds matching the query to the sequences
                    present in the index
        */
        CConstRef< CDbIndex::CSearchResults > operator()();

    private:

        typedef typename TIndex_Impl::TWord TWord;      /**< Alias for convenience. */

        /** Representation of the set of currently tracked seeds for
            all subject sequences. 
        */
        typedef std::vector< CTrackedSeeds > TTrackedSeeds;     

        /** Helper method to search a particular segment of the query. 
            The segment is taken from state of the search object.
        */
        void SearchInt();

        /** Process a seed candidate that is close to the masked out
            or ambigous region of the subject.
            The second parameter is encoded as follows: bits 3-5 (0-2) '
            is the distance to the left (right) boundary of the valid
            subject region plus 1. Value 0 in either field indicates that
            the corresponding distance is greater than 5.
            @param offset       [I]     uncompressed offset value
            @param bounds       [I]     distance to the left and/or right
                                        boundary of the valid subject
                                        region.
        */
        void ProcessBoundaryOffset( TWord offset, TWord bounds );

        /** Process a regular seed candidate.
            @param offset       [I]     uncompressed offset value
        */
        void ProcessOffset( TWord offset );

        /** Check if the offset value in the offset list needs 
            processing.
            @param offset       [I]     uncompressed offset value
            @return true if the offset can be skipped, false otherwise
        */
        bool Skip( TWord offset ) const { return offset%off_mod_ != 0; }

        /** Find the subject sequence containing the given offset value.
            @param offset       [I]     uncompressed offset value
        */
        void UpdateSubject( TWord offset );

        /** Extend a seed candidate to the left.
            No more than word_length - hkey_width positions are inspected.
            @param seed [I]     the seed candidate
            @param nmax [I]     if non-zero - additional restriction for 
                                the number of positions to consider
        */
        void ExtendLeft( 
                STrackedSeed & seed, TSeqPos nmax = ~(TSeqPos)0 ) const;

        /** Extend a seed candidate to the right.
            Extends as far right as possible, unless nmax parameter is
            non-zeroA
            @param seed [I]     the seed candidate
            @param nmax [I]     if non-zero - search no more than this
                                many positions
        */
        void ExtendRight( 
                STrackedSeed & seed, TSeqPos nmax = ~(TSeqPos)0 ) const;

        /** Compute the seeds after all roots are collected. */
        void ComputeSeeds();

        /** Process a single root.
            @param seeds        [I/O]   information on currently tracked seeds
            @param root         [I]     root to process
            @return 1 for normal offsets, 2 for boundary offsets
        */
        unsigned long ProcessRoot( CTrackedSeeds & seeds, const SSeedRoot * root );

        const TIndex_Impl & index_impl_;        /**< The index implementation object. */
        const BLAST_SequenceBlk * query_;       /**< The query sequence encoded in BLASTNA. */
        const BlastSeqLoc * locs_;              /**< Set of query locations to search. */
        TSearchOptions options_;                /**< Search options. */
        unsigned long off_mod_;                 /**< Only subject offsets that are 0 mod off_mod_
                                                     are considered as seed candidates. */

        TTrackedSeeds seeds_;   /**< The set of currently tracked seeds. */
        TSeqNum subject_;       /**< Logical id of the subject sequence containing the offset
                                     value currently being considered. */
        TWord subj_start_off_;  /**< Start offset of subject_. */
        TWord subj_end_off_;    /**< End offset of subject_. */
        TWord subj_start_;      /**< Start position of subject_. */
        TWord subj_end_;        /**< One past the end position of subject_. */
        TSeqPos qoff_;          /**< Current query offset. */
        TSeqPos qstart_;        /**< Start of the current query segment. */
        TSeqPos qstop_;         /**< One past the end of the current query segment. */
        CSeedRoots roots_;      /**< Collection of initial soff/qoff pairs. */
};

//-------------------------------------------------------------------------
template< typename index_impl_t >
CSearch< index_impl_t >::CSearch(
        const TIndex_Impl & index_impl,
        const BLAST_SequenceBlk * query,
        const BlastSeqLoc * locs,
        const TSearchOptions & options )
    : index_impl_( index_impl ), query_( query ), locs_( locs ),
      options_( options ), subject_( 0 ), subj_end_off_( 0 ),
      roots_( index_impl_.NumSubjects() )
{
    off_mod_ = 
        (options_.word_size - index_impl_.hkey_width() + 1)/
        CDbIndex::STRIDE;
    seeds_.resize( index_impl_.NumSubjects() );
}

//-------------------------------------------------------------------------
template< typename index_impl_t > 
INLINE
void CSearch< index_impl_t >::ExtendLeft( 
        STrackedSeed & seed, TSeqPos nmax ) const
{
    static const unsigned long CR = CDbIndex::CR;

    unsigned long hkey_width = index_impl_.hkey_width();
    const Uint1 * sstart     = index_impl_.GetSeqStoreBase() + subj_start_;
    const Uint1 * spos       = sstart + (seed.soff_ - (hkey_width - 1))/CR;
    const Uint1 * qstart     = query_->sequence;
    const Uint1 * qpos       = qstart + seed.qoff_ - (hkey_width - 1);
    unsigned int incomplete  = (seed.soff_ - (hkey_width - 1))%CR;

    qstart += qstart_;
    nmax = nmax < options_.word_size - hkey_width ?
        nmax : options_.word_size - hkey_width;

    while( nmax > 0 && incomplete > 0 && qpos > qstart ) {
        Uint1 sbyte = (((*spos)>>(2*(CR - incomplete--)))&0x3);
        if( *--qpos != sbyte ) return;
        --nmax;
        ++seed.len_;
    }

    nmax = (nmax < (TSeqPos)(qpos - qstart)) 
        ? nmax : qpos - qstart;
    nmax = (nmax < (TSeqPos)(CR*(spos - sstart))) 
        ? nmax : CR*(spos - sstart);
    --spos;

    while( nmax >= CR ) {
        Uint1 sbyte = *spos--;
        Uint1 qbyte = 0;

        for( unsigned int i = 0; i < CR; ++i ) {
            qbyte = qbyte + ((*--qpos)<<(2*i));
            if( *qpos > 3 ) return;
        }

        if( sbyte != qbyte ){
            ++spos;
            qpos += CR;
            break;
        }

        nmax -= CR;
        seed.len_ += CR;
    }

    unsigned int i = 0;

    while( nmax > 0 ) {
        Uint1 sbyte = (((*spos)>>(2*(i++)))&0x3);
        if( sbyte != *--qpos ) return;
        ++seed.len_;
        --nmax;
    }
}

//-------------------------------------------------------------------------
template< typename index_impl_t > 
INLINE
void CSearch< index_impl_t >::ExtendRight( 
        STrackedSeed & seed, TSeqPos nmax ) const
{
    static const unsigned long CR = CDbIndex::CR;

    const Uint1 * sbase      = index_impl_.GetSeqStoreBase();
    const Uint1 * send       = sbase + subj_end_;
    const Uint1 * spos       = sbase + subj_start_ + seed.soff_/CR;
    const Uint1 * qend       = query_->sequence + qstop_;
    const Uint1 * qpos       = query_->sequence + seed.qoff_ + 1;
    unsigned int incomplete  = seed.soff_%CR;

    while( nmax > 0 && (++incomplete)%CR != 0 && qpos < qend ) {
        Uint1 sbyte = (((*spos)>>(6 - 2*incomplete))&0x3);
        if( *qpos++ != sbyte ) return;
        ++seed.len_;
        ++seed.qright_;
        --nmax;
    }

    ++spos;
    nmax = (nmax < (TSeqPos)(qend - qpos)) ? 
        nmax : (TSeqPos)(qend - qpos);
    nmax = (nmax <= (send - spos)*CR) ?
        nmax : (send - spos)*CR;

    while( nmax >= CR ) {
        Uint1 sbyte = *spos++;
        Uint1 qbyte = 0;

        for( unsigned int i = 0; i < CR; ++i ) {
            if( *qpos > 3 ) return;
            qbyte = (qbyte<<2) + *qpos++;
        }

        if( sbyte != qbyte ) {
            --spos;
            qpos -= CR;
            break;
        }

        seed.len_ += CR;
        seed.qright_ += CR;
        nmax -= CR;
    }

    unsigned int i = 2*(CR - 1);

    while( nmax-- > 0 ) {
        Uint1 sbyte = (((*spos)>>i)&0x3);
        if( sbyte != *qpos++ ) break;
        ++seed.len_;
        ++seed.qright_;
        i -= 2;
    }

    return;
}

//-------------------------------------------------------------------------
template< typename index_impl_t > 
INLINE
void CSearch< index_impl_t >::UpdateSubject( TWord offset )
{
    if( offset >= subj_end_off_ ) {
        subject_ = index_impl_.UpdateSubject( 
                subject_, subj_start_off_, subj_end_off_, 
                subj_start_, subj_end_, offset );
    }
}

//-------------------------------------------------------------------------
template< typename index_impl_t > 
INLINE
void CSearch< index_impl_t >::ProcessBoundaryOffset( 
        TWord offset, TWord bounds )
{
    TSeqPos nmaxleft  = (TSeqPos)(bounds>>CDbIndex::CODE_BITS);
    TSeqPos nmaxright = (TSeqPos)(bounds&((1<<CDbIndex::CODE_BITS) - 1));
    STrackedSeed seed = 
        { qoff_, (TSeqPos)offset, index_impl_.hkey_width(), qoff_ };
    CTrackedSeeds & subj_seeds = seeds_[subject_];
    subj_seeds.EvalAndUpdate( seed );

    if( nmaxleft > 0 ) {
        ExtendLeft( seed, nmaxleft - 1 );
    }else {
        ExtendLeft( seed );
    }

    if( nmaxright > 0 ) {
        ExtendRight( seed, nmaxright - 1 );
    }else {
        ExtendRight( seed );
    }

    if( nmaxleft > 0 && 
            nmaxright == 0 && 
            seed.len_ < options_.word_size ) {
        seed.len_ = 0;
        subj_seeds.AppendSimple( seed );
    }else {
        subj_seeds.Append( seed, options_.word_size );
    }
}

//-------------------------------------------------------------------------
template< typename index_impl_t > 
INLINE
void CSearch< index_impl_t >::ProcessOffset( TWord offset )
{
    STrackedSeed seed = 
        { qoff_, (TSeqPos)offset, index_impl_.hkey_width(), qoff_ };
    CTrackedSeeds & subj_seeds = seeds_[subject_];

    if( subj_seeds.EvalAndUpdate( seed ) ) {
        ExtendLeft( seed );
        ExtendRight( seed );
        subj_seeds.Append( seed, options_.word_size );
    }
}

//-------------------------------------------------------------------------
template< typename index_impl_t > 
INLINE
unsigned long CSearch< index_impl_t >::ProcessRoot( 
        CTrackedSeeds & seeds, const SSeedRoot * root )
{
    if( qoff_ != root->qoff_ ) {
        seeds.Reset();
        qoff_ = root->qoff_;
    }

    qstart_ = root->qstart_;
    qstop_  = root->qstop_;

    if( root->soff_ < CDbIndex::MIN_OFFSET ) {
        TSeqPos boundary = (root++)->soff_;
        ProcessBoundaryOffset( 
                root->soff_ - CDbIndex::MIN_OFFSET, boundary );
        return 2;
    }else {
        ProcessOffset( root->soff_ - CDbIndex::MIN_OFFSET );
        return 1;
    }
}

//-------------------------------------------------------------------------
template< typename index_impl_t > 
INLINE
void CSearch< index_impl_t >::ComputeSeeds()
{
    TSeqNum num_subjects = index_impl_.NumSubjects() - 1;

    for( subject_ = 0; subject_ < num_subjects; ++subject_ ) {
        index_impl_.SetSubjInfo( 
                subject_, subj_start_off_, subj_end_off_,
                subj_start_, subj_end_ );
        CTrackedSeeds & seeds = seeds_[subject_];
        const SSubjRootsInfo & rinfo = roots_.GetSubjInfo( subject_ );

        if( rinfo.len_ > 0 ) {
            const SSeedRoot * roots = roots_.GetSubjRoots( subject_ );
            qoff_ = 0;

            for( unsigned long j = 0; j < rinfo.len_; ) {
                j += ProcessRoot( seeds, roots + j );
            }

            if( rinfo.extra_roots_ != 0 ) {
                typedef SSubjRootsInfo::TRoots TRoots;
                roots = &(*rinfo.extra_roots_)[0];

                for( TRoots::size_type j = 0; 
                        j < rinfo.extra_roots_->size(); ) {
                    j += ProcessRoot( seeds, roots + j );
                }
            }
        }

        seeds.Reset();
    }
}

//-------------------------------------------------------------------------
template< typename index_impl_t > 
INLINE
void CSearch< index_impl_t >::SearchInt()
{
    CNmerIterator< index_impl_t > nmer_it( 
            index_impl_, query_->sequence, qstart_, qstop_ );

    while( nmer_it.Next() ) {
        typename TIndex_Impl::TOffsetIterator off_it( 
                index_impl_.OffsetIterator( nmer_it.Nmer() ) );
        qoff_ = nmer_it.Pos();
        subject_ = 0;
        subj_end_off_ = 0;

        while( off_it.Next() ) {
            TWord offset = off_it.Offset();

            if( offset < CDbIndex::MIN_OFFSET ) {
                off_it.Next();
                TWord real_offset = off_it.Offset();
                UpdateSubject( real_offset );
                TSeqPos soff = CDbIndex::MIN_OFFSET + 
                    (TSeqPos)(real_offset - 
                              subj_start_off_)*CDbIndex::STRIDE;
                SSeedRoot r1 = { qoff_, (TSeqPos)offset, qstart_, qstop_ };
                SSeedRoot r2 = { qoff_, soff, qstart_, qstop_ };
                roots_.Add2( r1, r2, subject_ );
            }else if( !Skip( offset ) ) {
                UpdateSubject( offset );
                TSeqPos soff = CDbIndex::MIN_OFFSET + 
                    (TSeqPos)(offset - subj_start_off_)*CDbIndex::STRIDE;
                SSeedRoot r = { qoff_, soff, qstart_, qstop_ };
                roots_.Add( r, subject_ );
            }
        }
    }
}

//-------------------------------------------------------------------------
template< typename index_impl_t >
CConstRef< CDbIndex::CSearchResults > CSearch< index_impl_t >::operator()()
{
    const BlastSeqLoc * curloc = locs_;

    while( curloc != 0 ) {
        if( curloc->ssr != 0 ) {
            qstart_ = curloc->ssr->left;
            qstop_  = curloc->ssr->right + 1;
            // cerr << qstart_ << " - " << qstop_ << endl;
            SearchInt();
        }

        curloc = curloc->next;
    }

    ComputeSeeds();
    CRef< CDbIndex::CSearchResults > result( 
            new CDbIndex::CSearchResults( 0, index_impl_.NumSubjects() ) );

    for( TTrackedSeeds::iterator it = seeds_.begin(); 
            it != seeds_.end(); ++it ) {
        it->Finalize();
        result->SetResults( it - seeds_.begin() + 1, it->GetHitList() );
    }

    return result;
}

}

//-------------------------------------------------------------------------
/** Implementation of the BLAST database index
    @param word_t       bit width of the index
    @param OFF_TYPE     offset encoding type
    @param COMPRESSION  offset list compression type
    @param VER          index format version
*/
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER >
class CDbIndex_Impl {};

//-------------------------------------------------------------------------
/** Implementation of the BLAST database index specialized for
    given index parameters (index format version 1).
    @param word_t       bit width of the index
    @param OFF_TYPE     offset encoding type
    @param COMPRESSION  offset list compression type
*/
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION >
class CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, 1 > : public CDbIndex
{
    public:

        /**@name Some convenience alias declarations. */
        /**@{*/
        typedef word_t TWord;
        typedef COffsetData< TWord, COMPRESSION > TOffsetData;
        typedef CSubjectMap< TWord, OFF_TYPE > TSubjectMap;
        typedef typename TOffsetData::TIterator TOffsetIterator;
        /**@}*/

        /** Size of the index file header for index format version 2. */
        static const unsigned long HEADER_SIZE = 16 + 7*sizeof( TWord );

        /** Load an index data from the given input stream.
            @param is           [I/O]   input stream containing the index data
            @param header       [I]     index header information
        */
        CDbIndex_Impl( CNcbiIstream & is, const SIndexHeader< 1 > & header );

        /** Create an index object from mapped memory segment.
            @param map          [I/O]   points to the memory segment
            @param header       [I]     index header information
        */
        CDbIndex_Impl( CMemoryFile * map, const SIndexHeader< 1 > & header );

        /** Object destructor. */
        ~CDbIndex_Impl() 
        { 
            if( mapfile_ != 0 ) mapfile_->Unmap(); 
            delete offset_data_;
            delete subject_map_;
        }

        /** Get the hash key width of the index.
            @return the hash key width of the index in base pairs
        */
        unsigned long hkey_width() const 
        { return offset_data_->hkey_width(); }

        /** Create an offset list iterator corresponding to the given
            Nmer value.
            @param nmer [I]     the Nmer value
            @return the iterator over the offset list corresponding to nmer
        */
        const TOffsetIterator OffsetIterator( TWord nmer ) const
        { return TOffsetIterator( *offset_data_, nmer ); }

        /** Advance the current sequence id according to the given offset
            value.
            @param curr_subj    [I]     subject id from which to start searching
            @param start_off    [O]     starting offset of the result sequence
            @param end_off      [O]     ending offset of the result sequence
            @param start        [O]     starting position of the result sequence
            @param end          [O]     ending position of the result sequence
            @param offset       [I]     target raw offset value
            @return id of the sequence containing the target offset
            @sa CSubjectMap::Update()
        */
        TSeqNum UpdateSubject( 
                TSeqNum curr_subj, TWord & start_off, TWord & end_off, 
                TWord & start, TWord & end, TWord offset ) const
        { 
            return subject_map_->Update( 
                curr_subj, start_off, end_off, start, end, offset ); 
        }

        /** Return the subject information based on the given logical subject
            id.
            @param subj         [I]     logical subject id
            @param start_off    [O]     smallest offset value for subj
            @param end_off      [O]     smallest offset value for subj + 1
            @param start        [0]     starting offset of subj in the sequence store
            @param end          [0]     1 + ending offset of subj in the sequence store
        */
        void SetSubjInfo( 
                TSeqNum subject, TWord & start_off, TWord & end_off,
                TWord & start, TWord & end ) const
        { 
            subject_map_->SetSubjInfo( 
                    subject, start_off, end_off, start, end ); 
        }

        /** Get the total number of logical chunks in the index.
            @sa CSubjectMap::NumSubjects()
        */
        TSeqNum NumSubjects() const { return subject_map_->NumSubjects(); }

        /** Get the start of compressed raw sequence data.
            @sa CSubjectMap::GetSeqStoreBase()
        */
        const Uint1 * GetSeqStoreBase() const 
        { return subject_map_->GetSeqStoreBase(); }

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

        /** Extract the results for a specific database subject sequence.
            @param all_results  [I]     the combined results set
            @param subject      [I]     the database oid
            @param chunk        [I]     the chunk number
            @return the result list corresponding to the given chunk
                    of the given subject
        */
        virtual BlastInitHitList * DoExtractResults( 
                CConstRef< CSearchResults > all_results, 
                TSeqNum subject, TSeqNum chunk ) const;

        /** Check if the there are results available for the subject sequence.
            @param all_results  [I]     combined result set for all subjects
            @param oid          [I]     real subject id
            @return     true if there are results for the given oid; 
                        false otherwise
        */
        virtual bool DoCheckResults(
                CConstRef< CSearchResults > all_results, 
                TSeqNum oid ) const;

        CMemoryFile * mapfile_;         /**< Memory mapped file. */
        TWord * map_;                   /**< Start of memory mapped file data. */
        TOffsetData * offset_data_;     /**< Offset lists. */
        TSubjectMap * subject_map_;     /**< Subject sequence information. */
};

//-------------------------------------------------------------------------
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION >
CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, 1 >::CDbIndex_Impl(
        CNcbiIstream & is, const SIndexHeader< 1 > & header )
    : mapfile_( 0 ), map_( 0 )
{
    offset_data_ = new TOffsetData( is, header.hkey_width_ );
    subject_map_ = new TSubjectMap( is, header.start_, header.stop_ );
}

//-------------------------------------------------------------------------
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION >
CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, 1 >::CDbIndex_Impl(
        CMemoryFile * map, const SIndexHeader< 1 > & header )
    : mapfile_( map )
{
    if( mapfile_ != 0 ) {
        map_ = (TWord *)(((char *)(mapfile_->GetPtr())) + HEADER_SIZE);
        offset_data_ = new TOffsetData( &map_, header.hkey_width_ );
        subject_map_ = new TSubjectMap( &map_, header.start_, header.stop_ );
    }
}

//-------------------------------------------------------------------------
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION >
INLINE
BlastInitHitList * 
CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, 1 >::DoExtractResults(
        CConstRef< CSearchResults > all_results,
        TSeqNum subject, TSeqNum chunk ) const
{
    TSeqNum local_subj = subject_map_->MapSubject( subject, chunk );
    return all_results->GetResults( local_subj );
}

//-------------------------------------------------------------------------
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION >
INLINE bool 
CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, 1 >::DoCheckResults( 
        CConstRef< CSearchResults > all_results, TSeqNum oid ) const
{
    TSeqNum local_subj = subject_map_->MapSubject( oid, 0 );
    TSeqNum next_subj  = subject_map_->MapSubject( oid + 1, 0 );

    if( next_subj == 0 ) next_subj = all_results->NumSeq();

    for( ; local_subj < next_subj; ++local_subj ) {
        if( all_results->GetResults( local_subj ) != 0 ) return true;
    }

    return false;
}

//-------------------------------------------------------------------------
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION >
CConstRef< CDbIndex::CSearchResults > 
CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, 1 >::DoSearch( 
        const BLAST_SequenceBlk * query, 
        const BlastSeqLoc * locs,
        const SSearchOptions & search_options )
{
    CSearch< CDbIndex_Impl > searcher( 
            *this, query, locs, search_options );
    return searcher();
}

//-------------------------------------------------------------------------
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION >
class CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, 2 > 
    : public CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, 1 >
{
    typedef CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, 1 > TBase;

    public:

        CDbIndex_Impl( 
                CMemoryFile * map, const SIndexHeader< 2 > & header )
            : TBase( map, header )
        {}
};

//-------------------------------------------------------------------------
template< unsigned long VER >
CRef< CDbIndex > CDbIndex::LoadIndex( CNcbiIstream & is )
{
    CRef< CDbIndex > result( null );
    SIndexHeader< VER > header = ReadIndexHeader< VER >( is );

    // Make a choice of CDbIndex_Impl specialization
    if( header.width_ == WIDTH_32 ) {
        typedef Uint4 TWord;

        if( header.off_type_ == OFFSET_RAW ) {
            if( header.compression_ == UNCOMPRESSED ) {
                result.Reset( 
                        new CDbIndex_Impl< 
                            TWord, OFFSET_RAW, UNCOMPRESSED, VER
                        >( is, header )
                );
            }
        }else if( header.off_type_ == OFFSET_COMBINED ) {
        }
    }else if( header.width_ == WIDTH_64 ) {
        typedef Uint8 TWord;

        if( header.off_type_ == OFFSET_RAW ) {
            if( header.compression_ == UNCOMPRESSED ) {
                result.Reset( 
                        new CDbIndex_Impl< 
                            TWord, OFFSET_RAW, UNCOMPRESSED, VER
                        >( is, header )
                );
            }
        }else if( header.off_type_ == OFFSET_COMBINED ) {
        }
    }

    return result;
}

//-------------------------------------------------------------------------
template< unsigned long VER >
CRef< CDbIndex > CDbIndex::LoadIndex( const std::string & fname )
{
    CMemoryFile * map = MapFile( fname );
    SIndexHeader< VER > header = ReadIndexHeader< VER >( map->GetPtr() );
    CRef< CDbIndex > result( null );
    if( map ==0 ) return result;

    // Make a choice of CDbIndex_Impl specialization
    if( header.width_ == WIDTH_32 ) {
        typedef Uint4 TWord;

        if( header.off_type_ == OFFSET_RAW ) {
            if( header.compression_ == UNCOMPRESSED ) {
                result.Reset( 
                        new CDbIndex_Impl< 
                            TWord, OFFSET_RAW, UNCOMPRESSED, VER
                        >( map, header )
                );
            }
        }else if( header.off_type_ == OFFSET_COMBINED ) {
        }
    }else if( header.width_ == WIDTH_64 ) {
        typedef Uint8 TWord;

        if( header.off_type_ == OFFSET_RAW ) {
            if( header.compression_ == UNCOMPRESSED ) {
                result.Reset( 
                        new CDbIndex_Impl< 
                            TWord, OFFSET_RAW, UNCOMPRESSED, VER
                        >( map, header )
                );
            }
        }else if( header.off_type_ == OFFSET_COMBINED ) {
        }
    }

    return result;
}

//-------------------------------------------------------------------------
CRef< CDbIndex > CDbIndex::LoadByVersion( 
        CNcbiIstream & is, unsigned long version )
{
    switch( version ) {
        case 1:  return LoadIndex< 1 >( is );
        default: 
            
            NCBI_THROW( 
                    CDbIndex_Exception, eBadVersion,
                    "wrong index version" );
    }
}

//-------------------------------------------------------------------------
CRef< CDbIndex > CDbIndex::Load( const std::string & fname )
{
    CNcbiIfstream index_stream( fname.c_str() );

    if( !index_stream ) {
        NCBI_THROW( CDbIndex_Exception, eIO, "can not open index" );
    }

    unsigned long version = GetIndexVersion( index_stream );
    //return LoadByVersion( index_stream, version );

    switch( version ) {
        case 1: return LoadIndex< 1 >( index_stream );
        case 2:
                index_stream.close();
                return LoadIndex< 2 >( fname );
        default: 
            
            NCBI_THROW( 
                    CDbIndex_Exception, eBadVersion,
                    "wrong index version" );
    }
}

//-------------------------------------------------------------------------
CConstRef< CDbIndex::CSearchResults > CDbIndex::Search( 
        const BLAST_SequenceBlk * query, const BlastSeqLoc * locs, 
        const SSearchOptions & search_options )
{
    return DoSearch( query, locs, search_options );
}

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

