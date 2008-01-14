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

#include <list>
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
#define INLINE NCBI_INLINE
#else
#define INLINE 
#endif

// Comment this out for production.
// #define SEEDDEBUG 1
// #define PRINTSUBJMAP 1

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

/** Header structure of index format version 3. */
template<>
struct SIndexHeader< 3 > : public SIndexHeader< 1 >
{
};

/** Header structure of index format version 4. */
template<>
struct SIndexHeader< 4 > : public SIndexHeader< 1 >
{
};

/** Header structure of index format version 5. */
template<>
struct SIndexHeader< 5 > : public SIndexHeader< 1 >
{
};

//-------------------------------------------------------------------------
/** Memory map a file and return a pointer to the mapped area.
    @param fname        [I]     file name
    @return pointer to the start of the mapped memory area
*/
CMemoryFile * MapFile( const std::string & fname )
{
    CMemoryFile * result = 0;

    try {
        result = new CMemoryFile( fname );
    }
    catch( ... ) { result = 0; }

    if( result ) {
        if( !result->Map() ) {
            delete result;
            result = 0;
        }
    }

    if( result == 0 ) {
        ERR_POST( 
            "Index memory mapping failed.\n"
            "It is possible that an index volume is missing or is too large.\n"
            "Please, consider using -volsize option of makeindex utility to\n"
            "reduce the size of index volumes." );
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

/** Read index header information from memory area for 
    index format version 2 or higher.
    @param WIDTH index bit width
    @param map  [I]     memory area containing index data
    @return appropriate index header structure
*/
template< unsigned long WIDTH >
const SIndexHeader< 1 > ReadIndexHeader_2( void * map )
{
    typedef typename SWord< WIDTH >::type TWord;
    SIndexHeader< 1 > result;
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
    @param WIDTH        index bit width
    @param map  [I]     memory area containing index data
    @return appropriate index header structure
*/
template< unsigned long WIDTH >
const SIndexHeader< 1 > ReadIndexHeaderAux( void * map )
{ return ReadIndexHeader_2< WIDTH >( map ); }

/** Read index header information from the memory area.
    @param map  [I]     memory area containing index data
    @return appropriate index header structure
*/
const SIndexHeader< 1 > ReadIndexHeader( void * map ) 
{
    Uint8 width = *(((Uint8 *)map) + 1);

    if( width == WIDTH_32 ) {
        return ReadIndexHeaderAux< WIDTH_32 >( map );
    }
    else if( width == WIDTH_64 ) {
        return ReadIndexHeaderAux< WIDTH_64 >( map );
    }

    return SIndexHeader< 1 >();
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
template< typename word_t, typename iterator_t, unsigned long COMPRESSION >
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
    typedef COffsetData< 
        word_t, COffsetIterator, UNCOMPRESSED > TOffsetData;
    typedef word_t TWord;
    /**@}*/

    public:

        /** Object constructor.
            @param offset_data  [I]     Combined offset data for all keys
            @param key          [I]     Hash key defining the offset list
        */
        COffsetIterator( 
                const TOffsetData & offset_data, 
                TWord key, 
                unsigned long mod );

        /** Advance the iterator.
            @return false if the end of the offset list has been reached;
                    true otherwise
        */
        bool Next();

        /** Check if more data is available in the iterator.
            @return true if more data is available; false otherwise
        */
        bool More();

        /** Get the offset value corresponding to the current position of
            the iterator.
            @return the current offset value
        */
        TWord Offset() const { return offset_; }

    private:

        const TWord * start_;   /**< Current position in the offset list. */
        TWord len_;             /**< Number of offsets left in the list. */
        TWord offset_;          /**< Current offset value. */
        bool more_;             /**< Flag indicating that more data is available. */
        unsigned long mod_;     /**< Determines which offsets to skip. */
        bool boundary_;         /**< Flag indicating the current offset is actually
                                     a extra information for boundary cases. */
};

/** Iterate over zero terminated offset lists. 
    @param word_t       type of the index word
    @param COMPRESSION  type of compression
*/
template< typename word_t, unsigned long COMPRESSION >
class CZeroEndOffsetIterator;

/** Specialization for uncompressed offset lists. */
template< typename word_t >
class CZeroEndOffsetIterator< word_t, UNCOMPRESSED >
{
    /** Type of offset data class supported by this iterator. */
    typedef COffsetData< 
        word_t, CZeroEndOffsetIterator, UNCOMPRESSED > TOffsetData;

    /** Index word type. */
    typedef word_t TWord;

    public:

        /** Object constructor.
            @param offset_data  [I] offset data connected to the this object
            @param key          [I] nmer value identifying the offset list
        */
        CZeroEndOffsetIterator( 
                const TOffsetData & offset_data, 
                TWord key, 
                unsigned long mod );

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

    private:

        const TWord * curr_;    /**< Points to the current position in the offset list. */
        TWord offset_;          /**< Current offset value. */
        bool more_;             /**< Flag indicating the more data is available. */
        unsigned long mod_;     /**< Determines which offsets to skip. */
        bool boundary_;         /**< Flag indicating the current offset is actually
                                     a extra information for boundary cases. */
};

/** Iterate over zero terminated reordered offset lists. 
    @param word_t       type of the index word
    @param COMPRESSION  type of compression
*/
template< typename word_t, unsigned long COMPRESSION >
class CPreOrderedOffsetIterator;

/** Specialization for uncompressed offset lists. */
template< typename word_t >
class CPreOrderedOffsetIterator< word_t, UNCOMPRESSED >
{
    /** Type of offset data class supported by this iterator. */
    typedef COffsetData< 
        word_t, CPreOrderedOffsetIterator, UNCOMPRESSED > TOffsetData;

    /** Index word type. */
    typedef word_t TWord;

    public:

        /** Object constructor.
            @param offset_data  [I] offset data connected to the this object
            @param key          [I] nmer value identifying the offset list
            @param mod          [I] only boundary offsets and offsets that are
                                    0 modulo mod will be reported
        */
        CPreOrderedOffsetIterator( 
                const TOffsetData & offset_data, TWord key, unsigned long mod );

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

    private:

        const TWord * curr_;    /**< Current position in the offset list. */
        TWord offset_;          /**< Current cached offset value. */
        unsigned long more_;    /**< Flag indicating that more values are available. */
        unsigned long mod_;     /**< Determines which offsets to skip. */
        bool boundary_;         /**< Flag indicating the current offset is actually
                                     a extra information for boundary cases. */
};

//-------------------------------------------------------------------------
/** Type of objects maintaining the offset list data for all Nmers and
    the corresponding hash table.
    @param word_t word size used for uncompressed offsets
*/
template< typename word_t, typename iterator_t >
class COffsetData< word_t, iterator_t, UNCOMPRESSED >
    : public COffsetData_Base< word_t, UNCOMPRESSED >
{

    typedef COffsetData_Base< word_t, UNCOMPRESSED > TBase;     /**< Base class alias. */
    typedef typename TBase::TWord TWord;                        /**< Forward from TBase for convenience. */

    typedef CVectorWrap< TWord > TOffsets;      /**< Type used to store offset lists. */

    public:

        /** Type used to iterate over an offset list. */
        typedef iterator_t TIterator;

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
        TWord * data_start_;    /**< Start of the offset data. */
};

//-------------------------------------------------------------------------
template< typename word_t > 
INLINE
COffsetIterator< word_t, UNCOMPRESSED >::COffsetIterator(
        const TOffsetData & offset_data, TWord key, unsigned long mod )
    : more_( true ), mod_( mod ), boundary_( false )
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

    if( boundary_ ) {
        offset_ = *start_++;
        --len_;
        boundary_ = false;
        return true;
    }

    do {
        offset_ = *start_++;
        --len_;

        if( offset_ < CDbIndex::MIN_OFFSET ) {
            boundary_ = true;
            return true;
        }
    }while( offset_%mod_ != 0 && len_ > 0 );

    return offset_%mod_ == 0;
}

//-------------------------------------------------------------------------
template< typename word_t > 
INLINE
bool COffsetIterator< word_t, UNCOMPRESSED >::More()
{
    if( more_ ) {
        more_ = false;
        return true;
    }else {
        return false;
    }
}

//-------------------------------------------------------------------------
template< typename word_t >
INLINE
CZeroEndOffsetIterator< word_t, UNCOMPRESSED >::CZeroEndOffsetIterator(
        const TOffsetData & offset_data, TWord key, unsigned long mod )
    : more_( true ), mod_( mod ), boundary_( false )
{ 
    TWord tmp = offset_data.hash_table_[key];

    if( tmp != 0 ) curr_ = offset_data.data_start_ + tmp - 1; 
    else curr_ = 0;
}

//-------------------------------------------------------------------------
template< typename word_t >
INLINE
bool CZeroEndOffsetIterator< word_t, UNCOMPRESSED >::Next()
{ 
    if( curr_ == 0 ) return false;

    if( boundary_ ) {
        offset_ = *++curr_;
        boundary_ = false;
        return offset_ != 0;
    }

    while( (offset_ = *++curr_) != 0 ) {
        if( offset_ < CDbIndex::MIN_OFFSET ) {
            boundary_ = true;
            return true;
        }

        if( offset_%mod_ == 0 ) return true;
    }

    return false;
}

//-------------------------------------------------------------------------
template< typename word_t > 
INLINE
bool CZeroEndOffsetIterator< word_t, UNCOMPRESSED >::More()
{
    if( more_ ) {
        more_ = false;
        return true;
    }else {
        return false;
    }
}

//-------------------------------------------------------------------------
template< typename word_t >
INLINE
CPreOrderedOffsetIterator< 
    word_t, UNCOMPRESSED 
>::CPreOrderedOffsetIterator( const TOffsetData & offset_data, TWord key, unsigned long mod )
    : more_( 3 ), mod_( mod ), boundary_( false )
{
    TWord tmp = offset_data.hash_table_[key];

    if( tmp != 0 ) curr_ = offset_data.data_start_ + tmp - 1; 
    else{ 
        curr_ = 0; 
        more_ = 0; 
    }
}

//-------------------------------------------------------------------------
template< typename word_t >
INLINE
bool CPreOrderedOffsetIterator< word_t, UNCOMPRESSED >::Next()
{
    if( curr_ == 0 ) return false;
    
    if( (offset_ = *++curr_) == 0 ) {
        more_ = 0;
        return false;
    }
    else {
        if( offset_ < CDbIndex::MIN_OFFSET ) {
            boundary_ = true;
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
            return false;
        }
    }
}

//-------------------------------------------------------------------------
template< typename word_t >
INLINE
bool CPreOrderedOffsetIterator< word_t, UNCOMPRESSED >::More()
{ return more_ != 0; }

//-------------------------------------------------------------------------
template< typename word_t, typename iterator_t >
COffsetData< word_t, iterator_t, UNCOMPRESSED >::COffsetData( 
        CNcbiIstream & is, unsigned long hkey_width )
    : TBase( is, hkey_width ), offsets_( this->total_, 0 )
{
    is.read( 
            (char *)(&offsets_[0]), 
            sizeof( TWord )*(std::streamsize)(this->total_) );
    data_start_ = &offsets_[0];
}

//-------------------------------------------------------------------------
template< typename word_t, typename iterator_t >
COffsetData< word_t, iterator_t, UNCOMPRESSED >::COffsetData( 
        TWord ** map, unsigned long hkey_width )
    : TBase( map, hkey_width )
{
    if( *map ) {
        offsets_.SetPtr( 
                *map, (typename TOffsets::size_type)(this->total_) );
        data_start_ = *map;
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

        /** Type for storing the chunk data.
            For raw offset encoding the offset into the vector serves also
            as the internal logical sequence id.
        */
        typedef CVectorWrap< TWord > TChunks;

    public:

        /** Trivial constructor. */
        CSubjectMap_Base() : total_( 0 ) {}

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

        /** Loads index by mapping to the memory segment.
            @param map          [I/O]   pointer to the memory segment
            @param start        [I]     database oid of the first sequence
                                        in the map
            @param stop         [I]     database oid of the last sequence
                                        in the map
        */
        void Load( TWord ** map, TSeqNum start, TSeqNum stop );

        /** Provides a mapping from real subject ids and chunk numbers to
            internal logical subject ids.
            @return start of the (subject,chunk)->id mapping
        */
        const TWord * GetSubjectMap() const { return &subjects_[0]; }

        /** Return the start of the raw storage for compressed subject 
            sequence data.
            @return start of the sequence data storage
        */
        const Uint1 * GetSeqStoreBase() const { return &seq_store_[0]; }

        /** Return the size in bytes of the eaw sequence storage.

            @return Size of the sequence data storage.
        */
        TWord GetSeqStoreSize() const { return total_; }

        /** Get the total number of sequence chunks in the map.
            @return number of chunks in the map
        */
        TSeqNum NumChunks() const { return (TSeqNum)(chunks_.size()); }

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
        TChunks chunks_;        /**< Collection of individual chunk descriptors. */
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

    if( total_%sizeof( TWord ) != 0 ) {
        NCBI_THROW( 
                CDbIndex_Exception, eBadData, 
                "bad alignment of subject map data" );
    }

    chunks_.resize( 
            (typename TChunks::size_type)(
                1 + this->total_/sizeof( TWord )), 
            0 );
    is.read( (char *)(&chunks_[0]), (std::streamsize)(total_) );
    ReadSeqData( is );
    chunks_[chunks_.size() - 1] = seq_store_.size();
}

//-------------------------------------------------------------------------
template< typename word_t, unsigned long OFF_TYPE >
CSubjectMap_Base< word_t, OFF_TYPE >::CSubjectMap_Base( 
        TWord ** map, TSeqNum start, TSeqNum stop )
    : total_( 0 )
{
    Load( map, start, stop );
}

//-------------------------------------------------------------------------
template< typename word_t, unsigned long OFF_TYPE >
void CSubjectMap_Base< word_t, OFF_TYPE >::Load( 
        TWord ** map, TSeqNum start, TSeqNum stop )
{
    if( *map ) {
        typename TSubjects::size_type n_subjects 
            = (typename TSubjects::size_type)(stop - start + 1);
        total_ = *(*map)++;
        subjects_.SetPtr( *map, n_subjects );
        total_ -= sizeof( TWord )*n_subjects;
        *map += n_subjects;

#ifdef PRINTSUBJMAP
        for( unsigned long i = 0; i < n_subjects; ++i ) {
            cerr << i << " ---> " << subjects_[i] - 1 << endl;
        }
#endif

        typename TChunks::size_type chunks_size = 
            (typename TChunks::size_type)( 1 + total_/sizeof( TWord ));
        chunks_.SetPtr( *map, chunks_size ); 
        *map += chunks_.size();
        SetSeqDataFromMap( map );
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
    typedef typename TBase::TChunks TChunks;                    /**< Alias for convenience. */

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

        /** Decode offset.

            @param subject Chunk number.
            @param offset The offset value.

            @return Corresponding position in the subject sequence.
        */
        TSeqPos DecodeOffset( TSeqNum subject, TWord offset ) const;

        /** Unused. */
        TSeqPos DecodeOffset( TWord offset ) const { ASSERT(0); return 0; }

        /** Map logical sequence id and logical sequence offset to 
            relative chunk number and chunk offset.

            @param lid The logical sequence id.
            @param soff The logical sequence offset.

            @return Pair of relative chunk number and chunk offset.
        */
        std::pair< TSeqNum, TSeqPos > MapSubjOff( 
            TSeqNum lid, TSeqPos soff ) const
        { return std::make_pair( (TSeqNum)0, soff ); }

        TSeqNum MapLId2Chunk( TSeqNum lid, TSeqNum lchunk ) const
        { return lid; }

        /** Get number of chunks combined into a given logical sequence.

            @param lid The logical sequence id.

            @return Corresponding number of chunks.
        */
        TSeqNum GetNumChunks( TSeqNum lid ) const { return 1; }

        /** Get the total number of logical sequences in the map.
            @return number of chunks in the map
        */
        TSeqNum NumSubjects() const { return this->NumChunks(); }

        /** Unused. */
        TSeqPos GetSeqLen( TSeqNum oid ) const { return 0; }

        /** Unused. */
        const Uint1 * GetSeqData( TSeqNum oid ) const { return 0; }

    private:

        TChunks chunks_off_;    /**< Encoded starting offsets of the sequences in seq_store_. */
};

//-------------------------------------------------------------------------
template< typename word_t >
CSubjectMap< word_t, OFFSET_RAW >::CSubjectMap(
        CNcbiIstream & is, TSeqNum start, TSeqNum stop )
    : TBase( is, start, stop )
{
    chunks_off_.resize( 
            (typename TChunks::size_type)(
                1 + this->total_/sizeof( TWord )), 
            0 );

    for( typename TChunks::size_type i = 0; 
            i < this->chunks_.size() - 1; ++i ) {
        chunks_off_[i] = CDbIndex::MIN_OFFSET +
                this->chunks_[i]*(CDbIndex::CR)/(CDbIndex::STRIDE);
    }

    chunks_off_[this->chunks_.size() - 1] = CDbIndex::MIN_OFFSET +
        this->chunks_[this->chunks_.size() - 1]*
            (CDbIndex::CR)/(CDbIndex::STRIDE);
}

//-------------------------------------------------------------------------
template< typename word_t >
CSubjectMap< word_t, OFFSET_RAW >::CSubjectMap(
        TWord ** map, TSeqNum start, TSeqNum stop )
    : TBase( map, start, stop )
{
    if( *map ){
        chunks_off_.resize( this->chunks_.size() );

        for( typename TChunks::size_type i = 0; 
                i < this->chunks_.size() - 1; ++i ) {
            chunks_off_[i] = CDbIndex::MIN_OFFSET +
                    this->chunks_[i]*(CDbIndex::CR)/(CDbIndex::STRIDE);
        }

        chunks_off_[this->chunks_.size() - 1] = CDbIndex::MIN_OFFSET +
            this->chunks_[this->chunks_.size() - 1]*
                (CDbIndex::CR)/(CDbIndex::STRIDE);
    }
}

//-------------------------------------------------------------------------
template< typename word_t > 
INLINE
void CSubjectMap< word_t, OFFSET_RAW >::SetSubjInfo( 
        TSeqNum subj, TWord & start_off, TWord & end_off, 
        TWord & start, TWord & end ) const
{
    end       = this->chunks_[subj + 1];
    end_off   = chunks_off_[subj + 1];
    start     = this->chunks_[subj];
    start_off = chunks_off_[subj];
}

//-------------------------------------------------------------------------
template< typename word_t >
INLINE
TSeqPos CSubjectMap< word_t, OFFSET_RAW >::DecodeOffset(
        TSeqNum subject, TWord offset ) const
{
    TWord start = chunks_off_[subject];
    return CDbIndex::MIN_OFFSET + 
        (TSeqPos)(offset - start)*CDbIndex::STRIDE;
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

    end = this->chunks_[curr_subj];
    end_off = chunks_off_[curr_subj];
    start = this->chunks_[--curr_subj];
    start_off = chunks_off_[curr_subj];

    return curr_subj;
}

//-------------------------------------------------------------------------
/** Details of the database to internal sequence mapping that are 
    specific for the combined offset encoding.
    @param word_t bit width of the corresponding index type
*/
template< typename word_t >
class CSubjectMap< word_t, OFFSET_COMBINED > 
    : public CSubjectMap_Base< word_t, OFFSET_COMBINED >
{
    typedef CSubjectMap_Base< word_t, OFFSET_COMBINED > TBase;  /**< Alias for convenience. */
    typedef typename TBase::TWord TWord;                        /**< Alias for convenience. */
    typedef typename TBase::TChunks TChunks;                    /**< Alias for convenience. */

    typedef CVectorWrap< TWord > TLengths;      /**< Subject lengths storage type. */
    typedef CVectorWrap< TWord > TLIdMap;       /**< Local id -> chunks map storage type. */

    public:

        /** Constructs the object from the mapped memory segment.
            @param map          [I/O]   points to the memory segment
            @param start        [I]     database oid of the first sequence in the map
            @param stop         [I]     database oid of the last sequence in the map
        */
        CSubjectMap( TWord ** map, TSeqNum start, TSeqNum stop );

        /** Get number of chunks combined into a given logical sequence.

            @param lid The logical sequence id.

            @return Corresponding number of chunks.
        */
        TSeqNum GetNumChunks( TSeqNum lid ) const 
        {
            TWord * ptr = (TWord *)&lid_map_[0] + (lid<<2);
            return *(ptr + 1) - *ptr;
        }

        /** Decode offset.

            @param offset The encoded offset value.

            @return A pair with first element being the local subject sequence
                    id and the second element being the subject offset.
        */
        std::pair< TSeqNum, TSeqPos > DecodeOffset( TWord offset ) const 
        {
            offset -= CDbIndex::MIN_OFFSET;
            return std::make_pair( 
                    (TSeqNum)(offset>>offset_bits_),
                    (TSeqPos)(CDbIndex::MIN_OFFSET + 
                              (offset&offset_mask_)*CDbIndex::STRIDE) );
        }

        /** Return the subject information based on the given logical subject
            id.
            @param subj         [I]     logical subject id
            @param start        [0]     starting offset of subj in the sequence store
            @param end          [0]     1 + ending offset of subj in the sequence store
        */
        void SetSubjInfo( 
                TSeqNum subj, TWord & start, TWord & end ) const
        {
            TWord * ptr = (TWord *)&lid_map_[0] + (subj<<2) + 2;
            start = *ptr++;
            end   = *ptr;
        }

        /** Map logical sequence id and logical sequence offset to 
            relative chunk number and chunk offset.

            @param lid The logical sequence id.
            @param soff The logical sequence offset.

            @return Pair of relative chunk number and chunk offset.
        */
        std::pair< TSeqNum, TSeqPos > MapSubjOff( 
            TSeqNum lid, TSeqPos soff ) const
        {
            static const unsigned long CR = CDbIndex::CR;

            TWord * ptr = (TWord *)&lid_map_[0] + (lid<<2);
            TSeqNum start = (TSeqNum)*ptr++;
            TSeqNum end   = (TSeqNum)*ptr++;
            TWord lid_start = *ptr;
            TWord abs_offset = lid_start + (TWord)soff/CR;

            typedef typename TChunks::const_iterator TChunksIter;
            TChunksIter siter = this->chunks_.begin() + start;
            TChunksIter eiter = this->chunks_.begin() + end;
            ASSERT( siter != eiter );
            TChunksIter res = std::upper_bound( siter, eiter, abs_offset );
            ASSERT( res != siter );
            --res;

            return std::make_pair( 
                    (TSeqNum)(res - siter), 
                    (TSeqPos)(soff - (*res - lid_start)*CR) );
        }

        TSeqNum MapLId2Chunk( TSeqNum lid, TSeqNum lchunk ) const
        {
            TWord * ptr = (TWord *)&lid_map_[0] + (lid<<2);
            TSeqNum start = (TSeqNum)*ptr++;
            return start + lchunk;
        }

        /** Get the total number of logical sequences in the map.
            @return number of chunks in the map
        */
        TSeqNum NumSubjects() const
        { return 1 + (lid_map_.size()>>2); }

        /** Get the length of the subject sequence.
            
            @param oid Ordinal id of the subject sequence.

            @return Length of the sequence in bases.
        */
        TSeqPos GetSeqLen( TSeqNum oid ) const
        { return lengths_[oid]; }

        /** Get the sequence data of the subject sequence.
            
            @param oid Ordinal id of the subject sequence.

            @return Pointer to the sequence data.
        */
        const Uint1 * GetSeqData( TSeqNum oid ) const
        {
            TWord chunk = this->subjects_[oid] - 1;
            TWord start_index = this->chunks_[chunk];
            return &this->seq_store_[0] + start_index;
        }

    private:

        TLengths lengths_;      /** Subject lengths storage. */
        TLIdMap lid_map_;       /** Local id -> chunk map storage. */
        Uint1 offset_bits_;     /** Number of bits used to encode offset. */
        TWord offset_mask_;     /** Mask to extract offsets. */
};

//-------------------------------------------------------------------------
template< typename word_t >
CSubjectMap< word_t, OFFSET_COMBINED >::CSubjectMap(
        TWord ** map, TSeqNum start, TSeqNum stop )
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

    TBase::Load( map, start, stop );
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

        /** Check if the max number of elements is reached.
            @return true if LIM_ROOTS is exceeded, false otherwise
        */
        bool Overflow() const { return total_ > LIMIT_ROOTS; }

        /** Reinitialize the structure. */
        void Reset();

    private:

        /** Assumption on the amound of cache in the system.
            (overly optimistic)
        */
        static const unsigned long TOTAL_CACHE = 4*1024*1024; 

        /** Max number of roots before triggering overflow. */
        static const unsigned long LIMIT_ROOTS = 16*1024*1024;

        /** Clean up all the dynamically allocated memory. */
        void CleanUp()
        {
            for( TSeqNum i = 0; i < num_subjects_; ++i ) {
                rinfo_[i].CleanUp(); 
            }

            delete[] rinfo_;
            delete[] roots_;
        }

        /** Reallocate all the storage. Used by constructor and
            Reset().
        */
        void Allocate();

        TSeqNum num_subjects_;                  /**< Number of subjects in the index. */
        unsigned long subj_roots_len_bits_;     /**< Log_2 of n_subj_roots_. */
        unsigned long n_subj_roots_;            /**< Space is preallocated for this number of roots per subject. */
        SSeedRoot * roots_;                     /**< Roots array preallocated for all subjects. */
        SSubjRootsInfo * rinfo_;                /**< Array of root information structures for each subject.
                                                     Dynamically allocated. */
        unsigned long total_;                   /**< Currenr total number of elements. */
        unsigned long total_roots_;             /**< Max number of roots in preallocated storage. */
};

void CSeedRoots::Allocate()
{
    try {
        roots_ = new SSeedRoot[total_roots_];
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

void CSeedRoots::Reset()
{
    CleanUp();
    roots_ = 0; rinfo_ = 0; total_ = 0;
    Allocate();
}

CSeedRoots::CSeedRoots( TSeqNum num_subjects )
    : num_subjects_( num_subjects ), subj_roots_len_bits_( 7 ), 
      roots_( 0 ), rinfo_( 0 ), total_( 0 )
{
    total_roots_ = (num_subjects_<<subj_roots_len_bits_);

    while( total_roots_*sizeof( SSeedRoot ) < TOTAL_CACHE ) {
        ++subj_roots_len_bits_;
        total_roots_ <<= 1;
    }

    n_subj_roots_ = (1<<subj_roots_len_bits_);
    Allocate();
}

INLINE
void CSeedRoots::Add( const SSeedRoot & root, TSeqNum subject )
{
    SSubjRootsInfo & rinfo = rinfo_[subject];

    if( rinfo.len_ < n_subj_roots_ - 1 ) {
        *(roots_ + (subject<<subj_roots_len_bits_) + (rinfo.len_++)) 
            = root;
    }else {
        if( rinfo.extra_roots_ == 0 ) {
            rinfo.extra_roots_ = new TRoots;
            rinfo.extra_roots_->reserve( n_subj_roots_<<2 );
        }

        rinfo.extra_roots_->push_back( root );
    }

    ++total_;
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

    total_ += 2;
}

//-------------------------------------------------------------------------
/** Representation of a seed being tracked by the search algorithm.
*/
template< unsigned long NHITS >
struct STrackedSeed;

/** Specialization for one-hit based search. */
template<>
struct STrackedSeed< ONE_HIT >
{
    /** Instance constructor.

        @param qoff Query offset.
        @param soff Subject offset.
        @param len  Seed length.
        @param qright Rightmost position of the seed in query's coordinates.
    */
    STrackedSeed( 
            TSeqPos qoff, TSeqPos soff, TSeqPos len, TSeqPos qright )
        : qoff_( qoff ), soff_( soff ), len_( len ), qright_( qright )
    {}

    TSeqPos qoff_;      /**< Query offset of the seed's origin. */
    TSeqPos soff_;      /**< Subject offset of the seed's origin. */
    TSeqPos len_;       /**< Length of the seed. */
    TSeqPos qright_;    /**< Offset of the rightmost position of the seed in the query. */
};

/** Specializarion for two-hit based search. */
template<>
struct STrackedSeed< TWO_HIT >
{
    /** Instance constructor.

        @param qoff Query offset.
        @param soff Subject offset.
        @param len  Seed length.
        @param qright Rightmost position of the seed in query's coordinates.
    */
    STrackedSeed( 
            TSeqPos qoff, TSeqPos soff, TSeqPos len, TSeqPos qright )
        : qoff_( qoff ), soff_( soff ), len_( len ), qright_( qright ),
          second_hit_( 0 )
    {}

    TSeqPos qoff_;         /**< Query offset of the seed's origin. */
    TSeqPos soff_;         /**< Subject offset of the seed's origin. */
    TSeqPos len_;          /**< Length of the seed. */
    TSeqPos qright_;       /**< Offset of the rightmost position of the seed in the query. */
    TSeqPos second_hit_;   /**< Right end of the first hit. */
};

/** Representation of a collection of tacked seeds for a specific subject
    sequence.
*/
template< typename subject_map_t, unsigned long NHITS >
class CTrackedSeeds_Base
{
    protected:

        /**@name Some convenience type declaration. */
        /**@{*/
        typedef subject_map_t TSubjectMap;
        typedef STrackedSeed< NHITS > TTrackedSeed;
        typedef std::list< TTrackedSeed > TSeeds;
        typedef typename TSeeds::iterator TIter;
        typedef std::vector< BlastInitHitList * > THitLists;
        /**@}*/

    public:

        /** Object constructor. 

            @param subject_map The subject map instance.
        */
        CTrackedSeeds_Base( const TSubjectMap & subject_map ) 
            : subject_map_( &subject_map ), lid_( 0 )
        { it_ = seeds_.begin(); }

        /** Object copy constructor.
            @param rhs  [I]     source object to copy
        */
        CTrackedSeeds_Base( const CTrackedSeeds_Base & rhs )
            : hitlists_( rhs.hitlists_ ), 
              seeds_( rhs.seeds_ ), subject_map_( rhs.subject_map_ ),
              lid_( rhs.lid_ )
        { it_ = seeds_.begin(); }

        /** Set the correspondence between this object and a
            logical sequence.

            @param lid The logical sequence id.
        */
        void SetLId( TSeqNum lid )
        { 
            lid_ = lid; 
            hitlists_.resize( subject_map_->GetNumChunks( lid_ ), 0 );
        }

        /** Prepare for processing of the next query position. */
        void Reset();

        /** Add a seed to the set of tracked seeds.
            @param seed         [I]     seed to add
            @param word_size    [I]     minimum size of a valid seed
        */
        void Append( const TTrackedSeed & seed, unsigned long word_size );

        /** Add a seed to the set of tracked seeds.
            No check for word size is performed.
            @param seed         [I]     seed to add
        */
        void AppendSimple( const TTrackedSeed & seed );

        /** Save the remaining valid tracked seeds and clean up the 
            structure.
        */
        // void Finalize();

        /** Save the tracked seed for reporting in the search result set.
            @param seed [I]     seed to save
        */
        void SaveSeed( const TTrackedSeed & seed );

        /** Get the list of saved seeds.

            @param num The relative chunk number.

            @return the results set for the subject sequence to which
                    this object corresponds
        */
        BlastInitHitList * GetHitList( TSeqNum num ) const 
        { return hitlists_[num]; }

    protected:

        THitLists hitlists_;              /**< The result sets (one per chunk). */
        TSeeds seeds_;                    /**< List of seed candidates. */
        TIter it_;                        /**< Iterator pointing to the tracked seed that
                                               is about to be inspected. */
        const TSubjectMap * subject_map_; /**< The subject map object. */
        TSeqNum lid_;                     /**< Logical sequence number. */
};

//-------------------------------------------------------------------------
template< typename subject_map_t, unsigned long NHITS >
INLINE
void CTrackedSeeds_Base< subject_map_t, NHITS >::Reset()
{ it_ = seeds_.begin(); }

/* This code is for testing purposes only.
{
    unsigned long soff = 0, qoff = 0;
    bool good = true;

    for( TSeeds::iterator i = seeds_.begin(); i != seeds_.end(); ++i ) {
        if( i != seeds_.begin() ) {
            unsigned long s;

            if( i->qoff_ > qoff ) {
                unsigned long step = i->qoff_ - qoff;
                s = soff + step;
                if( s > i->soff_ ) { good = false; break; }
            }else {
                unsigned long step = qoff - i->qoff_;
                s = i->soff_ + step;
                if( s < soff ) { good = false; break; }
            }
        }

        soff = i->soff_;
        qoff = i->qoff_;
    }

    if( !good ) {
        cerr << "Bad List at " << qoff << " " << soff << endl;

        for( TSeeds::iterator i = seeds_.begin(); i != seeds_.end(); ++i ) {
            cerr << i->qoff_ << " " << i->soff_ << " "
                 << i->qright_ << " " << i->len_ << endl;
        }
    }

    it_ = seeds_.begin();
}
*/

//-------------------------------------------------------------------------
template< typename subject_map_t, unsigned long NHITS >
INLINE
void CTrackedSeeds_Base< subject_map_t, NHITS >::SaveSeed( 
        const TTrackedSeed & seed )
{
    if( seed.len_ > 0 ) {
        TSeqPos qoff = seed.qright_ - seed.len_ + 1;
        TSeqPos soff = seed.soff_ - (seed.qoff_ - qoff);
        std::pair< TSeqNum, TSeqPos > mapval = 
            subject_map_->MapSubjOff( lid_, soff );
        BlastInitHitList * hitlist = hitlists_[mapval.first];
        
        if( hitlist == 0 ) {
            hitlists_[mapval.first] = hitlist = BLAST_InitHitListNew();
        }

        BLAST_SaveInitialHit( hitlist, (Int4)qoff, (Int4)mapval.second, 0 );

#ifdef SEEDDEBUG
        TSeqNum chunk = subject_map_->MapLId2Chunk( lid_, mapval.first );
        cerr << "SEED: " << qoff << "\t" << mapval.second << "\t"
            << seed.len_ << "\t" << chunk << "\n";
#endif
    }
}

//-------------------------------------------------------------------------
template< typename subject_map_t, unsigned long NHITS >
INLINE
void CTrackedSeeds_Base< subject_map_t, NHITS >::AppendSimple( 
        const TTrackedSeed & seed )
{ seeds_.insert( it_, seed ); }

//-------------------------------------------------------------------------
template< typename subject_map_t, unsigned long NHITS >
INLINE
void CTrackedSeeds_Base< subject_map_t, NHITS >::Append( 
        const TTrackedSeed & seed, unsigned long word_size )
{
    if( it_ != seeds_.begin() ) {
        TIter tmp_it = it_; tmp_it--;
        TSeqPos step = seed.qoff_ - tmp_it->qoff_;
        TSeqPos bs_soff_corr = tmp_it->soff_ + step;

        if( bs_soff_corr == seed.soff_ ) {
            if( seed.qright_ < tmp_it->qright_ ) {
                if( tmp_it->len_ > 0 ) {
                    tmp_it->len_ -= (tmp_it->qright_ - seed.qright_ );
                }

                if( tmp_it->len_ < word_size ) {
                    seeds_.erase( tmp_it );
                }else {
                    tmp_it->qright_ = seed.qright_;
                }
            }
        }else if( seed.len_ >= word_size ) {
            seeds_.insert( it_, seed );
        }
    }else if( seed.len_ >= word_size ) {
        seeds_.insert( it_, seed );
    }
}

//-------------------------------------------------------------------------
/** CTrackedSeeds functionality that is different depending on
    whether a one-hit or two-hit based search is used.
*/
template< typename subject_map_t, unsigned long NHITS >
class CTrackedSeeds;

//-------------------------------------------------------------------------
/** Specialization for one-hit searches. */
template< typename subject_map_t >
class CTrackedSeeds< subject_map_t, ONE_HIT > 
    : public CTrackedSeeds_Base< subject_map_t, ONE_HIT >
{
    /** @name Types forwarded from the base class. */
    /**@{*/
    typedef CTrackedSeeds_Base< subject_map_t, ONE_HIT > TBase;
    typedef typename TBase::TSubjectMap TSubjectMap;
    typedef typename TBase::TTrackedSeed TTrackedSeed;
    typedef typename TBase::TSeeds TSeeds;
    /**@}*/

    public:

        /** Object constructor. 

            @param subject_map The subject map instance.
        */
        CTrackedSeeds( 
                const TSubjectMap & subject_map, 
                const CDbIndex::SSearchOptions & options ) 
            : TBase( subject_map )
        {}

        /** Object copy constructor.
            @param rhs  [I]     source object to copy
        */
        CTrackedSeeds( const CTrackedSeeds & rhs )
            : TBase( rhs )
        {}

        /** Process seeds on diagonals below or equal to the seed
            given as the parameter.
            @param seed [I]     possible candidate for a 'tracked' seed
            @return true if there is a tracked seed on the same diagonal
                    as seed; false otherwise
        */
        bool EvalAndUpdate( const TTrackedSeed & seed );

        /** Save the remaining valid tracked seeds and clean up the 
            structure.
        */
        void Finalize();
};

//-------------------------------------------------------------------------
template< typename subject_map_t >
INLINE
void CTrackedSeeds< subject_map_t, ONE_HIT >::Finalize()
{
    for( typename TSeeds::const_iterator cit = this->seeds_.begin(); 
            cit != this->seeds_.end(); ++cit ) {
        SaveSeed( *cit );
    }
}

//-------------------------------------------------------------------------
template< typename subject_map_t >
INLINE
bool CTrackedSeeds< subject_map_t, ONE_HIT >::EvalAndUpdate( 
        const TTrackedSeed & seed )
{
    while( this->it_ != this->seeds_.end() ) {
        TSeqPos step = seed.qoff_ - this->it_->qoff_;
        TSeqPos it_soff_corr = this->it_->soff_ + step;

        if( it_soff_corr > seed.soff_ ) {
            return true;
        }

        if( this->it_->qright_ < seed.qoff_ ) {
            SaveSeed( *this->it_ );
            this->it_ = this->seeds_.erase( this->it_ );
        }
        else {
            ++this->it_;
            
            if( it_soff_corr == seed.soff_ ) {
                return false;
            }
        }
    }

    return true;
}

//-------------------------------------------------------------------------
/** Specialization for two-hit searches. */
template< typename subject_map_t >
class CTrackedSeeds< subject_map_t, TWO_HIT > 
    : public CTrackedSeeds_Base< subject_map_t, TWO_HIT >
{
    /** @name Types forwarded from the base class. */
    /**@{*/
    typedef CTrackedSeeds_Base< subject_map_t, TWO_HIT > TBase;
    typedef typename TBase::TSubjectMap TSubjectMap;
    typedef typename TBase::TTrackedSeed TTrackedSeed;
    typedef typename TBase::TSeeds TSeeds;
    /**@}*/

    public:

        /** Object constructor. 

            @param subject_map The subject map instance.
        */
        CTrackedSeeds( 
                const TSubjectMap & subject_map,
                const CDbIndex::SSearchOptions & options ) 
            : TBase( subject_map ), 
              window_( options.two_hits ),
              contig_len_( 2*options.word_size ),
              word_size_( options.word_size )
        {}

        /** Process seeds on diagonals below or equal to the seed
            given as the parameter.
            @param seed [I]     possible candidate for a 'tracked' seed
            @return true if there is a tracked seed on the same diagonal
                    as seed; false otherwise
        */
        bool EvalAndUpdate( TTrackedSeed & seed );

        /** Save the remaining valid tracked seeds and clean up the 
            structure.
        */
        void Finalize();

    private:

        /** Verify two-seed criterion and save the seed if it is satisfied.

            @param seed Seed to check and save.

            @return true if seed was saved; false otherwise.
        */
        bool CheckAndSaveSeed( const TTrackedSeed & seed );

        unsigned long window_;     /**< Window for two-hit based search. */
        unsigned long contig_len_; /**< Min continuous length to save unconditionally. */
        unsigned long word_size_;  /**< Target word size. */
};


//-------------------------------------------------------------------------
template< typename subject_map_t >
INLINE
bool CTrackedSeeds< subject_map_t, TWO_HIT >::CheckAndSaveSeed( 
        const TTrackedSeed & seed )
{
    if( (seed.second_hit_ > 0 && 
                seed.qright_ <= seed.second_hit_ + seed.len_ + window_ ) ||
        seed.len_ >= contig_len_ ) {
        SaveSeed( seed );
        return true;
    }
    else return false;
}

//-------------------------------------------------------------------------
template< typename subject_map_t >
INLINE
void CTrackedSeeds< subject_map_t, TWO_HIT >::Finalize()
{
    for( typename TSeeds::const_iterator cit = this->seeds_.begin();
            cit != this->seeds_.end(); ++cit ) {
        CheckAndSaveSeed( *cit );
    }
}

//-------------------------------------------------------------------------
template< typename subject_map_t >
INLINE
bool CTrackedSeeds< subject_map_t, TWO_HIT >::EvalAndUpdate( 
        TTrackedSeed & seed )
{
    while( this->it_ != this->seeds_.end() ) {
        TSeqPos step = seed.qoff_ - this->it_->qoff_;
        TSeqPos it_soff_corr = this->it_->soff_ + step;
        if( it_soff_corr > seed.soff_ ) return true;

        if( this->it_->qright_ + seed.len_ + window_ + 3*CDbIndex::STRIDE 
                < seed.qright_ ) {
            CheckAndSaveSeed( *this->it_ );
            this->it_ = this->seeds_.erase( this->it_ );
        }
        else if( this->it_->qright_ < seed.qoff_ ) {
            if( CheckAndSaveSeed( *this->it_ ) ) {
                this->it_ = this->seeds_.erase( this->it_ );
            }
            else if( it_soff_corr == seed.soff_ &&
                     this->it_->len_ > 0 ) {
                seed.second_hit_ = this->it_->qright_;
                ++this->it_;
            }
            else { ++this->it_; }
        }
        else {
            ++this->it_;
            if( it_soff_corr == seed.soff_ ) return false;
        }
    }

    return true;
}

}

//-------------------------------------------------------------------------
// Forward declaration.
//
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER >
class CDbIndex_Impl;

/** This is the object representing the state of a search over the index.
    Use of a separate class for searches allows for multiple simultaneous
    searches against the same index.

    @param word_t      Index word type.
    @param OFF_TYPE    Offset encoding.
    @param COMPRESSION Offset compresion type.
    @param VER         Index version used.
    @param NHITS       Number of hits needed to save a seed.
    @param derived_t   Actual instance type.
*/
template<
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS,
    typename derived_t
>
class CSearch_Base
{
    protected:

        typedef CDbIndex::SSearchOptions TSearchOptions;    /**< Alias for convenience. */

    public:

        /** @name Aliases for convenience. */
        /**@{*/
        typedef CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, VER > TIndex_Impl;
        typedef typename TIndex_Impl::TSubjectMap TSubjectMap;
        typedef CTrackedSeeds< TSubjectMap, NHITS > TTrackedSeeds;
        typedef derived_t TDerived;
        /**@}*/

        /** Object constructor.
            @param index_impl   [I]     the index implementation object
            @param query        [I]     query data encoded in BLASTNA
            @param locs         [I]     set of query locations to search
            @param options      [I]     search options
        */
        CSearch_Base( 
                const TIndex_Impl & index_impl,
                const BLAST_SequenceBlk * query,
                const BlastSeqLoc * locs,
                const TSearchOptions & options );

        /** Performs the search.
            @return the set of seeds matching the query to the sequences
                    present in the index
        */
        CConstRef< CDbIndex::CSearchResults > operator()();

    protected:

        typedef typename TIndex_Impl::TWord TWord;      /**< Alias for convenience. */
        typedef STrackedSeed< NHITS > TTrackedSeed;     /**< Alias for convenience. */

        /** Representation of the set of currently tracked seeds for
            all subject sequences. 
        */
        typedef std::vector< TTrackedSeeds > TTrackedSeedsSet;     

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

        /** Extend a seed candidate to the left.
            No more than word_length - hkey_width positions are inspected.
            @param seed [I]     the seed candidate
            @param nmax [I]     if non-zero - additional restriction for 
                                the number of positions to consider
        */
        void ExtendLeft( 
                TTrackedSeed & seed, TSeqPos nmax = ~(TSeqPos)0 ) const;

        /** Extend a seed candidate to the right.
            Extends as far right as possible, unless nmax parameter is
            non-zeroA
            @param seed [I]     the seed candidate
            @param nmax [I]     if non-zero - search no more than this
                                many positions
        */
        void ExtendRight( 
                TTrackedSeed & seed, TSeqPos nmax = ~(TSeqPos)0 ) const;

        /** Compute the seeds after all roots are collected. */
        void ComputeSeeds();

        /** Process a single root.
            @param seeds        [I/O]   information on currently tracked seeds
            @param root         [I]     root to process
            @return 1 for normal offsets, 2 for boundary offsets
        */
        unsigned long ProcessRoot( TTrackedSeeds & seeds, const SSeedRoot * root );

        const TIndex_Impl & index_impl_;        /**< The index implementation object. */
        const BLAST_SequenceBlk * query_;       /**< The query sequence encoded in BLASTNA. */
        const BlastSeqLoc * locs_;              /**< Set of query locations to search. */
        TSearchOptions options_;                /**< Search options. */
        unsigned long off_mod_;                 /**< Only subject offsets that are 0 mod off_mod_
                                                     are considered as seed candidates. */

        TTrackedSeedsSet seeds_; /**< The set of currently tracked seeds. */
        TSeqNum subject_;        /**< Logical id of the subject sequence containing the offset
                                      value currently being considered. */
        TWord subj_start_off_;   /**< Start offset of subject_. */
        TWord subj_end_off_;     /**< End offset of subject_. */
        TWord subj_start_;       /**< Start position of subject_. */
        TWord subj_end_;         /**< One past the end position of subject_. */
        TSeqPos qoff_;           /**< Current query offset. */
        TSeqPos soff_;           /**< Current subject offset. */
        TSeqPos qstart_;         /**< Start of the current query segment. */
        TSeqPos qstop_;          /**< One past the end of the current query segment. */
        CSeedRoots roots_;       /**< Collection of initial soff/qoff pairs. */
};

//-------------------------------------------------------------------------
template<
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS,
    typename derived_t
>
CSearch_Base< word_t, OFF_TYPE, COMPRESSION, VER, NHITS, derived_t >::CSearch_Base(
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
    seeds_.resize( 
            index_impl_.NumSubjects() - 1, 
            TTrackedSeeds( index_impl_.GetSubjectMap(), options ) );
    for( typename TTrackedSeedsSet::size_type i = 0; i < seeds_.size(); ++i ) {
        seeds_[i].SetLId( (TSeqNum)i );
    }
}

//-------------------------------------------------------------------------
template<
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS,
    typename derived_t
>
INLINE
void CSearch_Base< word_t, OFF_TYPE, COMPRESSION, VER, NHITS, derived_t >::ExtendLeft( 
        TTrackedSeed & seed, TSeqPos nmax ) const
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
        unsigned int i = 0;

        for( ; i < CR; ++i ) {
            qbyte = qbyte + ((*--qpos)<<(2*i));

            if( *qpos > 3 ) {
                ++qpos;
                sbyte = qbyte + 1;
                break;
            }
        }

        if( sbyte != qbyte ){
            ++spos;
            qpos += i;
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
template<
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS,
    typename derived_t
>
INLINE
void CSearch_Base< word_t, OFF_TYPE, COMPRESSION, VER, NHITS, derived_t >::ExtendRight( 
        TTrackedSeed & seed, TSeqPos nmax ) const
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
template<
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS,
    typename derived_t
>
INLINE
void CSearch_Base< word_t, OFF_TYPE, COMPRESSION, VER, NHITS, derived_t >::ProcessBoundaryOffset( 
        TWord offset, TWord bounds )
{
    TSeqPos nmaxleft  = (TSeqPos)(bounds>>CDbIndex::CODE_BITS);
    TSeqPos nmaxright = (TSeqPos)(bounds&((1<<CDbIndex::CODE_BITS) - 1));
    TTrackedSeed seed( 
            qoff_, (TSeqPos)offset, index_impl_.hkey_width(), qoff_ );
    TTrackedSeeds & subj_seeds = seeds_[subject_];
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
template<
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS,
    typename derived_t
>
INLINE
void CSearch_Base< word_t, OFF_TYPE, COMPRESSION, VER, NHITS, derived_t >::ProcessOffset( TWord offset )
{
    TTrackedSeed seed(
        qoff_, (TSeqPos)offset, index_impl_.hkey_width(), qoff_ );
    TTrackedSeeds & subj_seeds = seeds_[subject_];

    if( subj_seeds.EvalAndUpdate( seed ) ) {
        ExtendLeft( seed );
        ExtendRight( seed );
        if( seed.len_ >= options_.word_size )
            subj_seeds.AppendSimple( seed );
    }
}

//-------------------------------------------------------------------------
template<
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS,
    typename derived_t
>
INLINE
unsigned long 
CSearch_Base< word_t, OFF_TYPE, COMPRESSION, VER, NHITS, derived_t >::ProcessRoot( 
        TTrackedSeeds & seeds, const SSeedRoot * root )
{
    if( qoff_ != root->qoff_ ) {
        seeds.Reset();
        qoff_ = root->qoff_;
    }else if( root->soff_ >= CDbIndex::MIN_OFFSET && 
                root->soff_ < soff_ ) {
        seeds.Reset();
    }

    qstart_ = root->qstart_;
    qstop_  = root->qstop_;

    if( root->soff_ < CDbIndex::MIN_OFFSET ) {
        TSeqPos boundary = (root++)->soff_;
        ProcessBoundaryOffset( 
                root->soff_ - CDbIndex::MIN_OFFSET, boundary );
        soff_ = root->soff_;
        return 2;
    }else {
        ProcessOffset( root->soff_ - CDbIndex::MIN_OFFSET );
        soff_ = root->soff_;
        return 1;
    }
}

//-------------------------------------------------------------------------
template<
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS,
    typename derived_t
>
INLINE
void CSearch_Base< word_t, OFF_TYPE, COMPRESSION, VER, NHITS, derived_t >::ComputeSeeds()
{
    TSeqNum num_subjects = index_impl_.NumSubjects() - 1;

    for( subject_ = 0; subject_ < num_subjects; ++subject_ ) {
        TDerived * self = static_cast< TDerived * >( this );
        self->SetSubjInfo();
        TTrackedSeeds & seeds = seeds_[subject_];
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
template<
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS,
    typename derived_t
>
INLINE
void CSearch_Base< word_t, OFF_TYPE, COMPRESSION, VER, NHITS, derived_t >::SearchInt()
{
    CNmerIterator< TIndex_Impl > nmer_it( 
            index_impl_, query_->sequence, qstart_, qstop_ );

    while( nmer_it.Next() ) {
        typename TIndex_Impl::TOffsetIterator off_it( 
                index_impl_.OffsetIterator( nmer_it.Nmer(), off_mod_ ) );
        qoff_ = nmer_it.Pos();

        while( off_it.More() ) {
            subject_ = 0;
            subj_end_off_ = 0;

            while( off_it.Next() ) {
                TWord offset = off_it.Offset();
                TDerived * self = static_cast< TDerived * >( this );

                if( offset < CDbIndex::MIN_OFFSET ) {
                    off_it.Next();
                    TWord real_offset = off_it.Offset();
                    TSeqPos soff = self->DecodeOffset( real_offset );
                    SSeedRoot r1 = { qoff_, (TSeqPos)offset, qstart_, qstop_ };
                    SSeedRoot r2 = { qoff_, soff, qstart_, qstop_ };
                    roots_.Add2( r1, r2, subject_ );
                }else {
                    TSeqPos soff = self->DecodeOffset( offset );
                    SSeedRoot r = { qoff_, soff, qstart_, qstop_ };
                    roots_.Add( r, subject_ );
                }
            }
        }

        if( roots_.Overflow() ) {
            TSeqPos old_qstart = qstart_;
            TSeqPos old_qstop  = qstop_;

            ComputeSeeds();
            roots_.Reset();

            qstart_ = old_qstart;
            qstop_  = old_qstop;
        }
    }
}

//-------------------------------------------------------------------------
template<
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS,
    typename derived_t
>
CConstRef< CDbIndex::CSearchResults > 
CSearch_Base< word_t, OFF_TYPE, COMPRESSION, VER, NHITS, derived_t >::operator()()
{
    const BlastSeqLoc * curloc = locs_;

    while( curloc != 0 ) {
        if( curloc->ssr != 0 ) {
            qstart_ = curloc->ssr->left;
            qstop_  = curloc->ssr->right + 1;
            // cerr << "SEGMENT: " << qstart_ << " - " << qstop_ << endl;
            SearchInt();
        }

        curloc = curloc->next;
    }

    ComputeSeeds();
    const TSubjectMap & subject_map = index_impl_.GetSubjectMap();
    CRef< CDbIndex::CSearchResults > result( 
            new CDbIndex::CSearchResults( 
                options_.word_size,
                0, index_impl_.NumChunks(), subject_map.GetSubjectMap(), 
                index_impl_.StopSeq() - index_impl_.StartSeq() ) );

    for( typename TTrackedSeedsSet::size_type i = 0, k = 1; 
            i < seeds_.size(); ++i ) {
        seeds_[i].Finalize();
        TSeqNum nchunks = subject_map.GetNumChunks( (TSeqNum)i );

        for( TSeqNum j = 0; j < nchunks; ++j ) {
            result->SetResults( 
                    (TSeqNum)(k++), seeds_[i].GetHitList( j ) );
        }
    }

    return result;
}

//-------------------------------------------------------------------------
/** CSearch CRTP. */
template<
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS
>
class CSearch;

/** Implementation for OFFSET_RAW. */
template<
    typename word_t, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS
>
class CSearch< word_t, OFFSET_RAW, COMPRESSION, VER, NHITS > : public CSearch_Base< 
    word_t, OFFSET_RAW, COMPRESSION, VER, NHITS,
    CSearch< word_t, OFFSET_RAW, COMPRESSION, VER, NHITS > >
{
    /** @name Convenience declarations. */
    /**@{*/
    typedef CSearch_Base< word_t, OFFSET_RAW, COMPRESSION, VER, NHITS, CSearch > TBase;
    typedef typename TBase::TIndex_Impl TIndex_Impl;
    typedef typename TBase::TSearchOptions TSearchOptions;
    typedef typename TBase::TWord TWord;
    /**@}*/

    public:

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
                const TSearchOptions & options )
            : TBase( index_impl, query, locs, options )
        {}

        /** Find the subject sequence containing the given offset value.
            @param offset       [I]     uncompressed offset value
        */
        void UpdateSubject( TWord offset );

        /** Set the parameters of the current subject sequence. */
        void SetSubjInfo();

        /** Decode offset value into subject position.

            @param offset Offset value.

            @return Corresponding position in the subject.
        */
        TSeqPos DecodeOffset( TWord offset ) 
        { 
            UpdateSubject( offset );
            return this->index_impl_.DecodeOffset( 
                    this->subject_, offset ); 
        }
};

//-------------------------------------------------------------------------
template<
    typename word_t, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS
>
INLINE
void CSearch< word_t, OFFSET_RAW, COMPRESSION, VER, NHITS >::UpdateSubject( TWord offset )
{
    if( offset >= this->subj_end_off_ ) {
        this->subject_ = this->index_impl_.UpdateSubject( 
                this->subject_, this->subj_start_off_, this->subj_end_off_, 
                this->subj_start_, this->subj_end_, offset );
    }
}

//-------------------------------------------------------------------------
template<
    typename word_t, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS
>
INLINE
void CSearch< word_t, OFFSET_RAW, COMPRESSION, VER, NHITS >::SetSubjInfo()
{
    this->index_impl_.SetSubjInfo( 
            this->subject_, this->subj_start_off_, this->subj_end_off_,
            this->subj_start_, this->subj_end_ );
}

//-------------------------------------------------------------------------
/** Implementation for OFFSET_COMBINED. */
template<
    typename word_t, 
    unsigned long COMPRESSION,
    unsigned long VER,
    unsigned long NHITS
>
class CSearch< word_t, OFFSET_COMBINED, COMPRESSION, VER, NHITS > 
    : public CSearch_Base< 
        word_t, OFFSET_COMBINED, COMPRESSION, VER, NHITS,
        CSearch< word_t, OFFSET_COMBINED, COMPRESSION, VER, NHITS > >
{
    /** @name Convenience declarations. */
    /**@{*/
    typedef CSearch_Base< 
        word_t, OFFSET_COMBINED, COMPRESSION, VER, NHITS, CSearch > TBase;
    typedef typename TBase::TIndex_Impl TIndex_Impl;
    typedef typename TBase::TSearchOptions TSearchOptions;
    typedef typename TBase::TWord TWord;
    /**@}*/

    public:

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
                const TSearchOptions & options )
            : TBase( index_impl, query, locs, options )
        {}


        /** Set the parameters of the current subject sequence. */
        void SetSubjInfo()
        {
            typedef typename TIndex_Impl::TSubjectMap TSubjectMap;
            const TSubjectMap & subject_map = 
                this->index_impl_.GetSubjectMap();
            subject_map.SetSubjInfo( 
                    this->subject_, this->subj_start_, this->subj_end_ );
        }

        /** Decode offset value into subject position.

            @param offset Offset value.

            @return Corresponding position in the subject.
        */
        TSeqPos DecodeOffset( TWord offset )
        {
            typedef typename TIndex_Impl::TSubjectMap TSubjectMap;
            const TSubjectMap & subject_map = 
                this->index_impl_.GetSubjectMap();
            std::pair< TSeqNum, TSeqPos > decoded = 
                subject_map.DecodeOffset( offset );
            this->subject_ = decoded.first;
            SetSubjInfo();
            return decoded.second;
        }
};

//-------------------------------------------------------------------------
/** This class computes the values of subject map and offset data types
    from index header values.
    @param word_t       index word type
    @param OFF_TYPE     offset encoding type
    @param COMPRESSION  offset list compression type
    @param VER          index format version
*/
template<
    typename word_t,
    unsigned long OFF_TYPE,
    unsigned long COMPRESSION,
    unsigned long VER >
struct CDbIndex_Traits
{
    /** Computed offset data type. */
    typedef COffsetData< 
        word_t, COffsetIterator< word_t, UNCOMPRESSED >, COMPRESSION 
    > TOffsetData;

    /** Computed subject map type. */
    typedef CSubjectMap< word_t, OFF_TYPE > TSubjectMap;
};

/** Specialization for index format version 3. */
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION 
>
struct CDbIndex_Traits< word_t, OFF_TYPE, COMPRESSION, 3 >
{
    /** Offset data type. */
    typedef COffsetData< 
        word_t, 
        CZeroEndOffsetIterator< word_t, UNCOMPRESSED >, 
        COMPRESSION 
    > TOffsetData;
    /** Subject map type. */
    typedef CSubjectMap< word_t, OFF_TYPE > TSubjectMap;
};

/** Specialization for index format version 4. */
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION 
>
struct CDbIndex_Traits< word_t, OFF_TYPE, COMPRESSION, 4 >
{
    /** Offset data type. */
    typedef COffsetData< 
        word_t, 
        CPreOrderedOffsetIterator< word_t, UNCOMPRESSED >, 
        COMPRESSION 
    > TOffsetData;
    /** Subject map type. */
    typedef CSubjectMap< word_t, OFF_TYPE > TSubjectMap;
};

/** Specialization for index format version 4. */
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION 
>
struct CDbIndex_Traits< word_t, OFF_TYPE, COMPRESSION, 5 >
{
    /** Offset data type. */
    typedef COffsetData< 
        word_t, 
        CPreOrderedOffsetIterator< word_t, UNCOMPRESSED >, 
        COMPRESSION 
    > TOffsetData;
    /** Subject map type. */
    typedef CSubjectMap< word_t, OFF_TYPE > TSubjectMap;
};

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
class CDbIndex_Impl : public CDbIndex
{
    /** Offset data and subject map types computer. */
    typedef CDbIndex_Traits< word_t, OFF_TYPE, COMPRESSION, VER > TTraits;

    public:

        /**@name Some convenience alias declarations. */
        /**@{*/
        typedef word_t TWord;
        typedef typename TTraits::TOffsetData TOffsetData;
        typedef typename TTraits::TSubjectMap TSubjectMap;
        typedef typename TOffsetData::TIterator TOffsetIterator;
        /**@}*/

        /** Size of the index file header for index format version >= 2. */
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
            delete subject_map_;
            delete offset_data_;
            if( mapfile_ != 0 ) mapfile_->Unmap(); 
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
            @param subject      [I]     logical subject id
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

        /** Decode offset.

            @param subject Chunk number.
            @param offset The offset value.

            @return Corresponding position in the subject sequence.
        */
        TSeqPos DecodeOffset( TSeqNum subject, TWord offset ) const
        { return subject_map_->DecodeOffset( subject, offset ); }

        /** Decode offset.

            @param offset The offset value.

            @return Corresponding position in the subject sequence.
        */
        TSeqPos DecodeOffset( TWord offset ) const
        { return subject_map_->DecodeOffset( offset ); }

        /** Get the length of the subject sequence.
            
            @param oid Ordinal id of the subject sequence.

            @return Length of the sequence in bases.
        */
        virtual TSeqPos GetSeqLen( TSeqNum oid ) const
        {
            if( VER != 5 ) {
                NCBI_THROW( 
                        CDbIndex_Exception, eBadVersion,
                        "GetSeqLen() is not supported in this index version." );
                return 0;
            }
            else return subject_map_->GetSeqLen( oid - this->start_ );
        }

        /** Get the sequence data of the subject sequence.
            
            @param oid Ordinal id of the subject sequence.

            @return Pointer to the sequence data.
        */
        virtual const Uint1 * GetSeqData( TSeqNum oid ) const
        {
            if( VER != 5 ) {
                NCBI_THROW( 
                        CDbIndex_Exception, eBadVersion,
                        "GetSeqData() is not supported in this index version." );
                return 0;
            }
            else return subject_map_->GetSeqData( oid - this->start_ );
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
        size_t subject_map_offset_;     /**< Offset of the subject map in the index file. */
        unsigned long version_;         /**< Index format version. */
};

//-------------------------------------------------------------------------
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER >
CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, VER >::CDbIndex_Impl(
        CNcbiIstream & is, const SIndexHeader< 1 > & header )
    : mapfile_( 0 ), map_( 0 ), version_( VER )
{
    start_ = header.start_;
    stop_  = header.stop_;
    start_chunk_ = header.start_chunk_;
    stop_chunk_  = header.stop_chunk_;

    offset_data_ = new TOffsetData( is, header.hkey_width_ );
    subject_map_ = new TSubjectMap( is, header.start_, header.stop_ );
}

//-------------------------------------------------------------------------
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER >
CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, VER >::CDbIndex_Impl(
        CMemoryFile * map, const SIndexHeader< 1 > & header )
    : mapfile_( map ), version_( VER )
{
    start_ = header.start_;
    stop_  = header.stop_;
    start_chunk_ = header.start_chunk_;
    stop_chunk_  = header.stop_chunk_;

    if( mapfile_ != 0 ) {
        map_ = (TWord *)(((char *)(mapfile_->GetPtr())) + HEADER_SIZE);
        offset_data_ = new TOffsetData( &map_, header.hkey_width_ );
        Uint1 * map_start = (Uint1 *)(mapfile_->GetPtr());
        subject_map_offset_ = (Uint1 *)map_ - map_start;
        subject_map_ = new TSubjectMap( &map_, header.start_, header.stop_ );
    }
}

//-------------------------------------------------------------------------
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER >
void CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, VER >::Remap()
{
    if( mapfile_ != 0 ) {
        delete subject_map_; subject_map_ = 0;
        delete offset_data_; offset_data_ = 0;
        mapfile_->Unmap();
        map_ = (TWord *)(mapfile_->Map( subject_map_offset_ ));
        subject_map_ = new TSubjectMap( &map_, start_, stop_ );
    }
}

//-------------------------------------------------------------------------
template< 
    typename word_t, 
    unsigned long OFF_TYPE, 
    unsigned long COMPRESSION,
    unsigned long VER >
INLINE
BlastInitHitList * 
CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, VER >::DoExtractResults(
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
    unsigned long COMPRESSION,
    unsigned long VER >
INLINE bool 
CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, VER >::DoCheckResults( 
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
    unsigned long COMPRESSION,
    unsigned long VER >
CConstRef< CDbIndex::CSearchResults > 
CDbIndex_Impl< word_t, OFF_TYPE, COMPRESSION, VER >::DoSearch( 
        const BLAST_SequenceBlk * query, 
        const BlastSeqLoc * locs,
        const SSearchOptions & search_options )
{
    if( search_options.two_hits == 0 ) {
        CSearch< word_t, OFF_TYPE, COMPRESSION, VER, ONE_HIT > searcher( 
                *this, query, locs, search_options );
        return searcher();
    }
    else {
        CSearch< word_t, OFF_TYPE, COMPRESSION, VER, TWO_HIT > searcher( 
                *this, query, locs, search_options );
        return searcher();
    }
}

//-------------------------------------------------------------------------
template< unsigned long VER >
CRef< CDbIndex > CDbIndex::LoadIndex( CNcbiIstream & is )
{
    CRef< CDbIndex > result( null );
    SIndexHeader< 1 > header = ReadIndexHeader< 1 >( is );

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
    CRef< CDbIndex > result( null );
    if( map ==0 ) return result;
    SIndexHeader< 1 > header = ReadIndexHeader( map->GetPtr() );

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
            if( header.compression_ == UNCOMPRESSED ) {
                result.Reset(
                        new CDbIndex_Impl<
                            TWord, OFFSET_COMBINED, UNCOMPRESSED, VER
                        >( map, header )
                );
            }
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

    switch( version ) {
        case 1: return LoadIndex< 1 >( index_stream );
        case 2:
                index_stream.close();
                return LoadIndex< 2 >( fname );
        case 3:
                index_stream.close();
                return LoadIndex< 3 >( fname );
        case 4:
                index_stream.close();
                return LoadIndex< 4 >( fname );
        case 5:
                index_stream.close();
                return LoadIndex< 5 >( fname );
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

