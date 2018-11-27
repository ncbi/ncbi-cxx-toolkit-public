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
 *   Header file for CDbIndex and some related classes.
 *
 */

#ifndef C_DB_INDEX_HPP
#define C_DB_INDEX_HPP

#include <corelib/ncbiobj.hpp>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_extend.h>

#include "sequence_istream.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

// Compression types.
const unsigned long UNCOMPRESSED = 0UL; /**< No compression. */

// Encoding of entries in offset lists.
const unsigned long OFFSET_COMBINED = 1UL;      /**< Combination of chunk 
                                                     number and chunk-based 
                                                     offset. */

// Index bit width.
const unsigned long WIDTH_32    = 0UL;  /**< 32-bit index. */

// Switching between one-hit and two-hit searches.
const unsigned long ONE_HIT     = 0UL;  /**< Use one-hit search (normal). */
const unsigned long TWO_HIT     = 1UL;  /**< Use two-hit search. */

// Level of progress reporting.
const unsigned long REPORT_QUIET   = 0UL;       /**< No progress reporting. */
const unsigned long REPORT_NORMAL  = 1UL;       /**< Normal reporting. */
const unsigned long REPORT_VERBOSE = 2UL;       /**< Verbose reporting. */

/** Compute the number of bits to encode special offsets based on stride.
    
    @param stride the value of stride

    @return number of bits necessary to encode numbers [0 - stride].
  */
extern unsigned long GetCodeBits( unsigned long stride );

/** Compute the minimum offset value needed encode offsets based on stride.

    @param stride the value of stride

    @return minimum offset used by an index with the given stride
  */
unsigned long GetMinOffset( unsigned long stride );

/** Exceptions that superheader objects can throw. */
class NCBI_XBLAST_EXPORT CIndexSuperHeader_Exception : public CException
{
public:

    /** Numerical error codes. */
    enum EErrCode
    {
        eFile,      ///< filesystem error
        eRead,      ///< stream reading error
        eWrite,     ///< stream writing error
        eEndian,    ///< wrong index endianness
        eVersion,   ///< unrecognized index format version
        eSize       ///< wrong header size
    };

    /** Get a human readable description of the exception type.

        @return string describing the exception type
    */
    virtual const char * GetErrCodeString() const override
    {
        switch( GetErrCode() ) {
            case eFile:    return "access failure";
            case eRead:    return "read failure";
            case eWrite:   return "write failure";
            case eEndian:  return "endianness mismatch";
            case eVersion: return "unknown index format version";
            case eSize:    return "wrong header size";
            default: return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT( CIndexSuperHeader_Exception, CException );
};

/** Base class for index superheaders. */
class NCBI_XBLAST_EXPORT CIndexSuperHeader_Base : public CObject
{
public:

    /** Old style index without superheader.

        This should never appear in the 'version' field of superheader.
    */
    static const Uint4 INDEX_FORMAT_VERSION_0 = 0;

    /** Old style index with superheader. */
    static const Uint4 INDEX_FORMAT_VERSION_1 = 1;

    /** Symbolic values for endianess. */
    enum EEndianness { eLittleEndian = 0, eBigEndian };

    /** Get the endianness of the host system. */
    static Uint4 GetSystemEndianness( void );

    /** Generate index volume file name from the index base name.

        @param idxname index base name
        @param volume volume ordinal number

        @return corresponding index volume file name
    */
    static std::string GenerateIndexVolumeName(
            const std::string & idxname, size_t volume );

    /** Object constructor.

        Reads the superheader structure from the file.

        @param size actual size of the superheader file
        @param endianness superheader file endianness
        @param version index format version

        @throw CIndexSuperHeader_Exception
    */
    CIndexSuperHeader_Base( size_t size, Uint4 endianness, Uint4 version );

    /* Object constructor.

       Used to create a superheader object for saving.

       @param version index format version
    */
    CIndexSuperHeader_Base( Uint4 version );

    /** Object destructor. */
    virtual ~CIndexSuperHeader_Base() {}
    
    /** Get the endianness of the superheader. */
    Uint4 GetEndianness( void );

    /** Get the index format version. */
    Uint4 GetVersion( void );

    /** Get number of sequences in the index (total of all volumes). */
    virtual Uint4 GetNumSeq( void ) const = 0;

    /** Get number of volumes in the index. */
    virtual Uint4 GetNumVol( void ) const = 0;

    /** Save the superheader into the file.

        @param fname output file name

        @throw CIndexSuperHeader_Exception
    */
    virtual void Save( const std::string & fname ) = 0;

protected:

    // Size in bytes of the common part of superheader file  for all versions.
    static const size_t COMMON_SIZE = 2*sizeof( Uint4 );

    /** Save common part to the given stream.

        @param os output stream
        @param fname file name (for reporting)

        @throw CIndexSuperHeader_Exception
    */
    void Save( std::ostream & os, const std::string & fname );

    size_t actual_size_; //< superheader file size reported by OS

private:

    Uint4 endianness_;  //< superheader endianness
    Uint4 version_;     //< index format version
};

/** Superheader derived classes parametrized by index format version. */
template< Uint4 INDEX_FORMAT_VERSION > class NCBI_XBLAST_EXPORT CIndexSuperHeader;

/** Superheader for old style indices. */
template<> class NCBI_XBLAST_EXPORT 
CIndexSuperHeader< CIndexSuperHeader_Base::INDEX_FORMAT_VERSION_1 >
    : public CIndexSuperHeader_Base
{
public:

    /** Object constructor.

        Reads the superheader structure from the file.

        @param size actual size of the superheader file
        @param endianness superheader file endianness
        @param version index format version
        @param fname index superheader file name
        @param is input stream corresponding to superheader file

        @throw CIndexSuperHeader_Exception
    */
    CIndexSuperHeader( 
            size_t size, Uint4 endianness, Uint4 version, 
            const std::string & fname, std::istream & is );

    /** Object constructor.

        Used to create a superheader object for saving.

        @param n_seq number of sequences in the database volume
        @param n_vol number of index volumes in the index for a given
                     database volume.

        @throw CIndexSuperHeader_Exception
    */
    CIndexSuperHeader( Uint4 n_seq, Uint4 n_vol );

    /** Get number of sequences in the index (total of all volumes). 

        @note Overrides CIndexSuperHeader_Base::GetNumSeq().
    */
    virtual Uint4 GetNumSeq( void ) const { return num_seq_; }

    /** Get number of volumes in the index.
    */
    virtual Uint4 GetNumVol( void ) const { return num_vol_; }

    /** Save the superheader into the file.

        @param fname output file name

        @throw CIndexSuperHeader_Exception
    */
    virtual void Save( const std::string & fname );

private:

    /// Expected size of the superheader file.
    static const size_t EXPECTED_SIZE = COMMON_SIZE + 2*sizeof( Uint4 );

    Uint4 num_seq_; //< total number of sequences in all index volumes
    Uint4 num_vol_; //< total number of volumes in the index
};

/** Read superheader structure from the file.

    @param fname superheader file name
    
    @return shared pointer to the superheader object

    @throw CIndexSuperHeader_Exception
*/
NCBI_XBLAST_EXPORT CRef< CIndexSuperHeader_Base > 
GetIndexSuperHeader( const std::string & fname );

/** Structure into which an index header is loaded. */
struct SIndexHeader
{
    bool legacy_;               /**< This is a legacy index format. */

    unsigned long hkey_width_;  /**< Size in bp of the Nmer used as a hash key. */
    unsigned long stride_;      /**< Stride used to index database locations. */
    unsigned long ws_hint_;     /**< Word size hint used during index creation. */

    unsigned long max_chunk_size_; /**< Chunk size used to split subjects. */
    unsigned long chunk_overlap_;  /**< Overlap of neighboring chunks. */

    CSequenceIStream::TStreamPos start_;             /**< OID of the first sequence in the index. */
    CSequenceIStream::TStreamPos start_chunk_;       /**< Number of the first chunk of the first sequence in the index. */
    CSequenceIStream::TStreamPos stop_;              /**< OID of the last sequence in the index. */
    CSequenceIStream::TStreamPos stop_chunk_;        /**< Number of the last chunk of the last sequence in the index. */
};

/** Read the index header information from the given file.
    @param fname        [I]   name of the index volume file
    @return the number of subjects in the index volume (from the volume header)
*/
size_t GetIdxVolNumOIDs( const std::string & fname );

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

/** Types of exception the indexing library can throw.
  */
class NCBI_XBLAST_EXPORT CDbIndex_Exception : public CException
{
    public:

        /** Numerical error codes. */
        enum EErrCode
        {
            eBadOption,         /**< Bad index creation/search option. */
            eBadSequence,       /**< Bad input sequence data. */
            eBadVersion,        /**< Wrong index version. */
            eBadData,           /**< Bad index data. */
            eIO                 /**< I/O error. */
        };

        /** Get a human readable description of the exception type.
            @return string describing the exception type
          */
        virtual const char * GetErrCodeString() const override;

        NCBI_EXCEPTION_DEFAULT( CDbIndex_Exception, CException );
};

class CSubjectMap;

/** Base class providing high level interface to index objects.
  */
class NCBI_XBLAST_EXPORT CDbIndex : public CObject
{
    public:

        /** Letters per byte in the sequence store.
            Sequence data is stored in the index packed 4 bases per byte.
        */
        static const unsigned long CR = 4;

        /** Only process every STRIDEth nmer.
            STRIDE value of 5 allows for search of contiguous seeds of
            length >= 16.
        */
        static const unsigned long STRIDE = 5;          

        /** Offsets below this are reserved for special purposes.
            Bits 0-2 of such an offset represent the distance from
            the start of the Nmer to the next invalid base to the left of
            the Nmer. Bits 3-5 represent the distance from the end of the 
            Nmer to the next invalid base to the right of the Nmer.
        */
        static const unsigned long MIN_OFFSET = 64;     

        /** How many bits are used for special codes for first/last nmers.
            See comment to MIN_OFFSET.
        */
        static const unsigned long CODE_BITS = 3;       

        /** Index version that this library handles. */
        static const unsigned char VERSION = (unsigned char)5;

        /** Simple record type used to specify index creation parameters.
          */
        struct SOptions
        {
            bool idmap;                         /**< Indicator of the index map creation. */
            bool legacy;                        /**< Indicator of the legacy index format. */
            unsigned long stride;               /**< Stride to use for stored database locations. */
            unsigned long ws_hint;              /**< Most likely word size to use for searches. */
            unsigned long hkey_width;           /**< Width of the hash key in bits. */
            unsigned long chunk_size;           /**< Long sequences are split into chunks
                                                     of this size. */
            unsigned long chunk_overlap;        /**< Amount by which individual chunks overlap. */
            unsigned long report_level;         /**< Verbose index creation. */
            unsigned long max_index_size;       /**< Maximum index size in megabytes. */

            std::string stat_file_name;         /**< File to write index statistics into. */
        };

        /** Type used to enumerate sequences in the index. */
        typedef CSequenceIStream::TStreamPos TSeqNum;

        /** Type representing main memory unit of the index structure. */
        typedef Uint4 TWord;

        /** This class represents a set of seeds obtained by searching
            all subjects represented by the index.
        */
        class CSearchResults : public CObject
        {
            /** Each vector item points to results for a particular 
                logical subject. 
            */
            typedef vector< BlastInitHitList * > TResults;

            public:

                /** Convenience declaration */
                typedef CDbIndex::TWord TWord;

                /** Object constructor.
                    @param word_size    [I]     word size used for the search
                    @param start        [I]     logical subject corresponding to the
                                                first element of the result set
                    @param size         [I]     number of logical subjects covered by
                                                this result set
                    @param map          [I]     mapping from (subject, chunk) pairs to
                                                logical sequence ids
                    @param map_size     [I]     number of elements in map
                */
                CSearchResults( 
                        unsigned long word_size,
                        TSeqNum start, TSeqNum size,
                        const TWord * map, size_t map_size )
                    : word_size_( word_size ), start_( start ), results_( size, 0 )
                {
                    for( size_t i = 0; i < map_size; ++i ) {
                        map_.push_back( map[i] );
                    }
                }

                /** Get the result set for a particular logical subject.
                    @param seq  [I]     logical subject number
                    @return pointer to a C structure describing the set of seeds
                */
                BlastInitHitList * GetResults( TSeqNum seq ) const
                {
                    if( seq == 0 ) return 0;
                    else if( seq - start_ - 1 >= results_.size() ) return 0;
                    else return results_[seq - start_ - 1];
                }

                /** Get the search word size.
                    
                    @return Word size value used for the search.
                */
                unsigned long GetWordSize() const { return word_size_; }

            private:

                /** Map a subject sequence and a chunk number to 
                    internal logical id.
                    @param subj  The subject id.
                    @param chunk The chunk number.
                    @return Internal logical id of the given sequence.
                */
                TSeqNum MapSubject( TSeqNum subj, TSeqNum chunk ) const
                {
                    if( subj >= map_.size() ) return 0;
                    return (TSeqNum)(map_[subj]) + chunk;
                }

            public:

                /** Get the result set for a particular subject and chunk.
                    @param subj  The subject id.
                    @param chunk The chunk number.
                    @return pointer to a C structure describing the set of seeds
                */
                BlastInitHitList * GetResults( TSeqNum subj, TSeqNum chunk ) const
                { return GetResults( MapSubject( subj, chunk ) ); }

                /** Check if any results are available for a given subject sequence.

                    @param subj The subject id.

                    @return true if there are seeds available for this subject,
                            false otherwise.
                */
                bool CheckResults( TSeqNum subj ) const
                {
                    if( subj >= map_.size() ) return false;
                    bool res = false;

                    TSeqNum start = MapSubject( subj, 0 );
                    TSeqNum end   = MapSubject( subj + 1, 0 );
                    if( end == 0 ) end = start_ + results_.size() + 1;
                    
                    for( TSeqNum chunk = start; chunk < end; ++chunk ) {
                        if( GetResults( chunk ) != 0 ) {
                            res = true;
                            break;
                        }
                    }

                    return res;
                }

                /** Set the result set for a given logical subject.
                    @param seq  [I]     logical subject number
                    @param res  [I]     pointer to the C structure describing
                                        the set of seeds
                */
                void SetResults( TSeqNum seq, BlastInitHitList * res )
                {
                    if( seq > 0 && seq - start_ - 1 < results_.size() ) {
                        results_[seq - start_ - 1] = res;
                    }
                }

                /** Object destructor. */
                ~CSearchResults()
                {
                    for( TResults::iterator it = results_.begin();
                            it != results_.end(); ++it ) {
                        if( *it ) {
                            BLAST_InitHitListFree( *it );
                        }
                    }
                }

                /** Get the number of logical sequences in the results set.
                    @return number of sequences in the result set
                */
                TSeqNum NumSeq() const { return results_.size(); }

            private:

                unsigned long word_size_;       /**< Word size used for the search. */
                TSeqNum start_;                 /**< Starting logical subject number. */
                TResults results_;              /**< The combined result set. */
                vector< Uint8 > map_;           /**< (subject,chunk)->(logical id) map. */
        };

        /** Creates an SOptions instance initialized with default values.

          @return instance of SOptions filled with default option values
          */
        static SOptions DefaultSOptions();

        /** Simple record type used to specify index search parameters.
            For description of template types see documentation for
            CDbIndex::SOptions.
          */
        struct SSearchOptions
        {
            unsigned long word_size;            /**< Target seed length. */
            unsigned long two_hits;             /**< Window for two-hit method (see megablast docs). */
        };

        /** Create an index object.

          Creates an instance of CDbIndex using the named resource as input.
          The name of the resource is given by the <TT>fname</TT> parameter. 

          @param fname          [I]     input file name
          @param oname          [I]     output file name
          @param start          [I]     number of the first sequence in the index
          @param start_chunk    [I]     number of the first chunk at which the starting
                                        sequence should be processed
          @param stop           [I/O]   number of the last sequence in the index;
                                        returns the number of the actual last sequece
                                        stored
          @param stop_chunk     [I/O]   number of the last chunk of the last sequence
                                        in the index
          @param options        [I]     index creation parameters
          */
        static void MakeIndex(
                const std::string & fname, 
                const std::string & oname,
                TSeqNum start, TSeqNum start_chunk,
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options
        );

        /** Create an index object.

          This function is the same as 
          CDbIndex::MakeIndex( fname, start, start_chunk, stop, stop_chunk, options )
          with start_chunk set to 0.
          */
        static void MakeIndex(
                const std::string & fname, 
                const std::string & oname,
                TSeqNum start, 
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options )
        { MakeIndex( fname, oname, start, 0, stop, stop_chunk, options ); }

        /** Create an index object.

          This function is the same as 
          CDbIndex::MakeIndex( fname, start, stop, stop_chunk, options )
          except that it does not need <TT>stop_chunk</TT> parameter and
          can only be used to create indices containing whole sequences.
          */
        static void MakeIndex(
                const std::string & fname, 
                const std::string & oname,
                TSeqNum start, TSeqNum & stop,
                const SOptions & options
        );

        /** Create an index object.

          Creates an instance of CDbIndex using a given stream as input.

          @param input          [I]     stream for reading sequence and mask information
          @param oname          [I]     output file name
          @param start          [I]     number of the first sequence in the index
          @param start_chunk    [I]     number of the first chunk at which the starting
                                        sequence should be processed
          @param stop           [I/O]   number of the last sequence in the index;
                                        returns the number of the actual last sequece
                                        stored
          @param stop_chunk     [I/O]   number of the last chunk of the last sequence
                                        in the index
          @param options        [I]     index creation parameters
          */
        static void MakeIndex(
                CSequenceIStream & input,
                const std::string & oname,
                TSeqNum start, TSeqNum start_chunk,
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options
        );

        /** Create an index object.

          This function is the same as 
          CDbIndex::MakeIndex( input, start, start_chunk, stop, stop_chunk, options )
          with start_chunk set to 0.
          */
        static void MakeIndex(
                CSequenceIStream & input, 
                const std::string & oname,
                TSeqNum start, 
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options )
        { MakeIndex( input, oname, start, 0, stop, stop_chunk, options ); }

        /** Create an index object.

          This function is the same as 
          CDbIndex::MakeIndex( input, start, stop, stop_chunk, options )
          except that it does not need <TT>stop_chunk</TT> parameter and
          can only be used to create indices containing whole sequences.
          */
        static void MakeIndex(
                CSequenceIStream & input,
                const std::string & oname,
                TSeqNum start, TSeqNum & stop, 
                const SOptions & options
        );

        /** Load index.

          @param fname  [I]     file containing index data

          @return CRef to the loaded index
          */
        static CRef< CDbIndex > Load( const std::string & fname, bool nomap = false );

        /** Search the index.

          @param query          [I]     the query sequence in BLASTNA format
          @param locs           [I]     which parts of the query to search
          @param search_options [I]     search parameters
          */
        CConstRef< CSearchResults > Search( 
                const BLAST_SequenceBlk * query, 
                const BlastSeqLoc * locs,
                const SSearchOptions & search_options
        );

        /** Index object destructor. */
        virtual ~CDbIndex() {}

        /** Get the OID of the first sequence in the index.
            @return OID of the first sequence in the index
        */
        TSeqNum StartSeq() const { return start_; }

        /** Get the number of the first chunk of the first sequence 
            in the index.
            @return the number of the first sequence chunk in the index
        */
        TSeqNum StartChunk() const { return start_chunk_; }

        /** Get the OID of the last sequence in the index.
            @return OID of the last sequence in the index
        */
        TSeqNum StopSeq() const { return stop_; }

        /** Get the number of the last chunk of the last sequence 
            in the index.
            @return the number of the last sequence chunk in the index
        */
        TSeqNum StopChunk() const { return stop_chunk_; }

        /** Get the length of the subject sequence.
            
            @param oid Ordinal id of the subject sequence.

            @return Length of the sequence in bases.
        */
        virtual TSeqPos GetSeqLen( TSeqNum /*oid*/ ) const
        {
            NCBI_THROW( 
                    CDbIndex_Exception, eBadVersion,
                    "GetSeqLen() is not supported in this index version." );
            return 0;
        }

        /** Get the sequence data of the subject sequence.
            
            @param oid Ordinal id of the subject sequence.

            @return Pointer to the sequence data.
        */
        virtual const Uint1 * GetSeqData( TSeqNum /*oid*/ ) const
        {
            NCBI_THROW( 
                    CDbIndex_Exception, eBadVersion,
                    "GetSeqData() is not supported in this index version." );
            return 0;
        }

        /** If possible reduce the index footpring by unmapping
            the portion that does not contain sequence data.
        */
        virtual void Remap() {}

    private:

        /** Load index from an open stream.
            @param is   [I]     stream containing index data
            @return object containing loaded index data
        */
        static CRef< CDbIndex > LoadIndex( CNcbiIstream & is );

        /** Load index from a named file.
            Usually this is used to memmap() the file data into
            the index structure.
            @param fname        [I]     index file name
            @param nomap        [I]     if 'true', then read the the file
                                        instead of mmap()'ing it
            @return object containing loaded index data
        */
        template< bool LEGACY >
        static CRef< CDbIndex > LoadIndex( 
                const std::string & fname, bool nomap = false );

        /** Actual implementation of seed searching.
            Must be implemented by child classes.
            @sa Search
        */
        virtual CConstRef< CSearchResults > DoSearch(
                const BLAST_SequenceBlk *, 
                const BlastSeqLoc *,
                const SSearchOptions & )
        { return CConstRef< CSearchResults >( null ); }

    public:

        struct SOffsetValue
        {
            TWord special;
            TWord offset;
        };

        typedef SOffsetValue TOffsetValue;

        typedef CSubjectMap TSubjectMap;

        TSeqNum getStartOId() const { return header_.start_; }
        TSeqNum getStopOId() const  { return header_.stop_; }

        TSeqNum getSIdByOId( TSeqNum oid ) const 
        { 
            ASSERT( oid >= getStartOId() );
            return oid - getStartOId(); 
        }

        TSeqNum getOIdBySId( TSeqNum sid ) const
        {
            ASSERT( sid <= getStopOId() - getStartOId() );
            return sid + getStartOId();
        }

        unsigned long getHKeyWidth() const { return header_.hkey_width_; }
        unsigned long getStride() const    { return header_.stride_; }
        unsigned long getWSHint() const    { return header_.ws_hint_; }

        unsigned long getMaxChunkSize() const { return header_.max_chunk_size_; }
        unsigned long getChunkOverlap() const { return header_.chunk_overlap_; }

        bool isLegacy() const { return header_.legacy_; }

        TWord getSubjectLength( TSeqNum sid ) const;
        TSeqNum getCId( TSeqNum sid, TSeqNum rcid ) const;
        TSeqNum getCId( TSeqNum sid ) const { return getCId( sid, 0 ); }
        pair< TSeqNum, TSeqNum > getSRCId( TSeqNum cid ) const;
        TSeqNum getSIdByCId( TSeqNum cid ) const { return getSRCId( cid ).first; }
        TWord getChunkLength( TSeqNum cid ) const;
        TWord getChunkLength( TSeqNum sid, TSeqNum rcid ) const
        { return getChunkLength( getCId( sid, rcid ) ); }
        TSeqNum getCIdByLRCId( TSeqNum lid, TSeqNum rcid ) const;
        TSeqNum getSIdByLRCId( TSeqNum lid, TSeqNum rcid ) const
        { return getSIdByCId( getCIdByLRCId( lid, rcid ) ); }
        pair< TSeqNum, TSeqPos > getRCIdOffByLIdOff( TSeqNum lid, TSeqPos loff ) const;

        pair< TSeqNum, TSeqPos > getCIdOffByLIdOff( TSeqNum lid, TSeqPos loff ) const
        { 
            pair< TSeqNum, TSeqPos > t = getRCIdOffByLIdOff( lid, loff );
            return make_pair( getCIdByLRCId( lid, t.first ), t.second );
        }

        TSeqPos getSOff( TSeqNum sid, TSeqNum rcid, TSeqPos coff ) const;

        pair< TSeqNum, TSeqPos > getSIdOffByCIdOff( TSeqNum cid, TSeqPos coff ) const
        {
            pair< TSeqNum, TSeqNum > t = getSRCId( cid );
            return make_pair( t.first, getSOff( t.first, t.second, coff ) );
        }

        pair< TSeqNum, TSeqPos > getSIdOffByLIdOff( TSeqNum lid, TSeqPos loff ) const
        { 
            pair< TSeqNum, TSeqPos > t = getCIdOffByLIdOff( lid, loff );
            return getSIdOffByCIdOff( t.first, t.second );
        }

        TSeqNum getNumSubjects() const;
        TSeqNum getNumChunks() const;
        TSeqNum getNumChunks( TSeqNum sid ) const;

        const Uint1 * getSeqData( TSeqNum sid ) const;

        TSeqNum getLId( const TOffsetValue & v ) const;
        TSeqPos getLOff( const TOffsetValue & v ) const;

        const string getBioseqIdBySId( TSeqNum sid ) const
        {
            if( sid < idmap_.size() ) return idmap_[sid];
            else return "unknown";
        }

        const vector< string > & getIdMap() const { return idmap_; }

    protected:

        TSeqNum start_;         /**< OID of the first sequence in the index. */
        TSeqNum start_chunk_;   /**< Number of the first chunk of the first sequence. */
        TSeqNum stop_;          /**< OID of the last sequence in the inex. */
        TSeqNum stop_chunk_;    /**< Number of the last chunk of the last sequence. */

        SIndexHeader header_;       /**< The index header structure. */
        TSubjectMap * subject_map_; /**< The subject map object. */
        vector< string > idmap_;    /**< Mapping from source ids to bioseq ids. */
};

/** Class representing index hash table and offset list database.
*/
class COffsetData_Base
{
    friend class CPreOrderedOffsetIterator;

    public:

        /** Index word type (public to support Solaris). */
        typedef CDbIndex::TWord TWord;

        typedef CDbIndex::SOffsetValue TOffsetValue;

        /** The type of the hash table.
            The hash table implements the mapping from Nmer values to
            the corresponding offset lists.
        */
        typedef CVectorWrap< TWord > THashTable;

        /** Object constructor.
            Creates the object by mapping data from a memory segment.
            @param map          [I/O]   pointer to the memory segment
            @param hkey_width   [I]     width in bp of the hash key
            @param stride       [I]     stride of the index
            @param ws_hint      [I]     ws_hint value of the index
        */
        COffsetData_Base( 
                TWord ** map, unsigned long hkey_width, 
                unsigned long stride, unsigned long ws_hint );

        /** Get the width of the hash key in base pairs.
            @return hash key width
        */
        unsigned long hkey_width() const { return hkey_width_; }

        /** Accessor for minimum offset value.

            @return the minimum offset value
          */
        unsigned long getMinOffset() const { return min_offset_; }

        /** Accessor for stride value.

            @return the stride value
          */
        unsigned long getStride() const { return stride_; }

        /** Accessor for ws_hint value.

            @return the ws_hint value
          */
        unsigned long getWSHint() const { return ws_hint_; }

    protected:

        /** Auxiliary data member used for importing the offset 
            list data.
        */
        TWord total_;

        unsigned long hkey_width_;      /**< Hash key width in bp. */
        unsigned long stride_;          /**< Stride value used by the index. */
        unsigned long ws_hint_;         /**< ws_hint values used by the index. */
        unsigned long min_offset_;      /**< Minimum offset value used by the index. */

        THashTable hash_table_;         /**< The hash table (mapping from
                                             Nmer values to the lists of
                                             offsets. */
};

/** Type representing subject map data.
*/
class CSubjectMap
{
    private:

        typedef CDbIndex::TSeqNum TSeqNum;
        typedef CDbIndex::TWord TWord;
        typedef CDbIndex::TOffsetValue TOffsetValue;

        /** Type used to map database oids to the chunk info. */
        typedef CVectorWrap< TWord > TSubjects;

        /** Type used for compressed subject sequence data storage. */
        typedef CVectorWrap< Uint1 > TSeqStore;

        /** Type for storing the chunk data.
            For raw offset encoding the offset into the vector serves also
            as the internal logical sequence id.
        */
        typedef CVectorWrap< TWord > TChunks;

        typedef CVectorWrap< TWord > TLengths;      /**< Subject lengths storage type. */
        typedef CVectorWrap< TWord > TLIdMap;       /**< Local id -> chunks map storage type. */

    public:

        /** Trivial constructor. */
        CSubjectMap() : total_( 0 ) {}

        /** Constructs object by mapping to the memory segment.
            @param map          [I/O]   pointer to the memory segment
            @param start        [I]     database oid of the first sequence
                                        in the map
            @param stop         [I]     database oid of the last sequence
                                        in the map
            @param stride       [I]     index stride value
        */
        CSubjectMap( 
                TWord ** map, TSeqNum start, TSeqNum stop,
                unsigned long stride );

        CSubjectMap( TWord ** map, const SIndexHeader & header );

        /** Loads index by mapping to the memory segment.
            @param map          [I/O]   pointer to the memory segment
            @param start        [I]     database oid of the first sequence
                                        in the map
            @param stop         [I]     database oid of the last sequence
                                        in the map
            @param stride       [I]     index stride value
        */
        void Load( 
                TWord ** map, TSeqNum start, TSeqNum stop, 
                unsigned long stride );

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

        /** Get number of chunks combined into a given logical sequence.

            @param lid The logical sequence id.

            @return Corresponding number of chunks.
        */
        TSeqNum GetNumChunks( TSeqNum lid ) const 
        {
            TWord * ptr = (TWord *)&lid_map_[0] + (lid<<2);
            return *(ptr + 1) - *ptr;
        }

        /** Get the logical sequence id from the database oid and the
            chunk number.
            @param subject      [I]     database oid
            @param chunk        [I]     the chunk number
            @return logical sequence id corresponding to subject and chunk
        */
        TSeqNum MapSubject( TSeqNum subject, TSeqNum chunk ) const
        {
            if( subject < subjects_.size() ) {
                TSeqNum result = 
                    (TSeqNum)(subjects_[subject]) + chunk;

                if( result < chunks_.size() ) {
                    return result;
                }
            }

            return 0;
        }

        /** Accessor for stride value.
            
            @return the stride value used by the index
          */
        unsigned long GetStride() const { return stride_; }

        /** Decode offset.

            @param offset The encoded offset value.

            @return A pair with first element being the local subject sequence
                    id and the second element being the subject offset.
        */
        std::pair< TSeqNum, TSeqPos > DecodeOffset( TWord offset ) const 
        {
            offset -= min_offset_;
            return std::make_pair( 
                    (TSeqNum)(offset>>offset_bits_),
                    (TSeqPos)(min_offset_ + 
                              (offset&offset_mask_)*stride_) );
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

            typedef TChunks::const_iterator TChunksIter;
            TChunksIter siter = chunks_.begin() + start;
            TChunksIter eiter = chunks_.begin() + end;
            ASSERT( siter != eiter );
            TChunksIter res = std::upper_bound( siter, eiter, abs_offset );
            ASSERT( res != siter );
            --res;

            return std::make_pair( 
                    (TSeqNum)(res - siter), 
                    (TSeqPos)(soff - (*res - lid_start)*CR) );
        }

        /** Map logical id and relative chunk to absolute chunk id.

            @param lid logical sequence id
            @param lchunk chunk number within the logical sequence

            @return chunk id of the corresponding chunk
        */
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
            TWord chunk = subjects_[oid] - 1;
            TWord start_index = chunks_[chunk];
            return &seq_store_[0] + start_index;
        }

        TWord getSubjectLength( TSeqNum sid ) const
        {
            ASSERT( sid <= subjects_.size() );
            return lengths_[sid];
        }

        TSeqNum getCId( TSeqNum sid, TSeqNum rcid ) const
        {
            ASSERT( sid <= subjects_.size() );
            TSeqNum result = subjects_[sid] + rcid - 1;
            ASSERT( result <= chunks_.size() );
            return result;
        }

        typedef pair< TSeqNum, TSeqPos > TSOPair;
        typedef pair< TSeqNum, TSeqNum > TSCPair;
        typedef vector< TSCPair > TSCPairMap;

        TSCPair getSRCId( TSeqNum cid ) const
        { 
            ASSERT( cid < chunks_.size() );
            return c2s_map_[cid];
        }

        TWord getChunkLength( TSeqNum cid ) const
        {
            ASSERT( cid < chunks_.size() );
            if( cid < chunks_.size() - 1 ) {
                TSCPair t = getSRCId( cid );
                
                if( t.first < subjects_.size() - 1 ) {
                    TSeqNum nc = subjects_[t.first + 1] - subjects_[t.first];
                    return (t.second == nc - 1) ?  max_chunk_size_ : 
                        getSubjectLength( t.first )%(
                                max_chunk_size_ - chunk_overlap_ );
                }
                else return max_chunk_size_;
            }
            else {
                return getSubjectLength( subjects_.size() - 2 )%(
                        max_chunk_size_ - chunk_overlap_ );
            }
        }

        TSeqNum getCIdByLRCId( TSeqNum lid, TSeqNum rcid ) const
        {
            ASSERT( lid < lid_map_.size() );
            TWord * ptr = (TWord *)&lid_map_[0] + (lid<<2);
            TSeqNum start = (TSeqNum)*ptr++;
            ASSERT( rcid < (TSeqNum)*ptr - start );
            return start + rcid;
        }

        TSOPair getRCIdOffByLIdOff( TSeqNum lid, TSeqPos loff ) const
        {
            ASSERT( lid < lid_map_.size() );
            static const unsigned long CR = CDbIndex::CR;

            TWord * ptr = (TWord *)&lid_map_[0] + (lid<<2);
            TSeqNum start = (TSeqNum)*ptr++;
            TSeqNum end   = (TSeqNum)*ptr++;
            ASSERT( start < chunks_.size() );
            ASSERT( end <= chunks_.size() );
            TWord lid_start = *ptr;
            TWord abs_offset = lid_start + (TWord)loff/CR;
            ASSERT( abs_offset < seq_store_.size() );

            typedef TChunks::const_iterator TChunksIter;
            TChunksIter siter = chunks_.begin() + start;
            TChunksIter eiter = chunks_.begin() + end;
            ASSERT( siter != eiter );
            TChunksIter res = std::upper_bound( siter, eiter, abs_offset );
            ASSERT( res != siter );
            --res;

            return std::make_pair( 
                    (TSeqNum)(res - siter), 
                    (TSeqPos)(loff - (*res - lid_start)*CR) );
        }

        TSeqPos getSOff( TSeqNum _DEBUG_ARG(sid),
                         TSeqNum rcid, TSeqPos coff ) const
        {
            ASSERT( sid < subjects_.size() - 1 );
            ASSERT( subjects_[sid] - 1 + rcid < chunks_.size() );
            TSeqPos res = rcid*(max_chunk_size_ - chunk_overlap_) + coff;
            ASSERT( res < lengths_[sid] );
            return res;
        }

        TSeqNum getNumSubjects() const { return subjects_.size() - 1; }
        TSeqNum getNumChunks() const { return chunks_.size(); }

        TSeqNum getNumChunks( TSeqNum sid ) const
        {
            ASSERT( sid < subjects_.size() -1 );
            if( sid < subjects_.size() - 2 ) {
                return subjects_[sid + 1] - subjects_[sid];
            }
            else return chunks_.size() + 1 - subjects_[sid];
        }

        const Uint1 * getSeqData( TSeqNum sid ) const
        {
            ASSERT( sid < subjects_.size() - 1 );
            TWord chunk = subjects_[sid] - 1;
            TWord start_index = chunks_[chunk];
            return &seq_store_[0] + start_index;
        }

        TSeqNum getLId( const TOffsetValue & v ) const
        { return (TSeqNum)(v.offset>>offset_bits_); }

        TSeqPos getLOff( const TOffsetValue & v ) const
        { return (TSeqPos)((v.offset&offset_mask_)*stride_); }

    private:

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

        unsigned long stride_;     /**< Index stride value. */
        unsigned long min_offset_; /**< Minimum offset used by the index. */

        TLengths lengths_;      /**< Subject lengths storage. */
        TLIdMap lid_map_;       /**< Local id -> chunk map storage. */
        Uint1 offset_bits_;     /**< Number of bits used to encode offset. */
        TWord offset_mask_;     /**< Mask to extract offsets. */
        TSCPairMap c2s_map_;    /**< CId -> (SId, RCId) map. */

        unsigned long max_chunk_size_;
        unsigned long chunk_overlap_;
};

inline CDbIndex::TWord 
CDbIndex::getSubjectLength( CDbIndex::TSeqNum sid ) const
{ return subject_map_->getSubjectLength( sid ); }

inline CDbIndex::TSeqNum 
CDbIndex::getCId( CDbIndex::TSeqNum sid, CDbIndex::TSeqNum rcid ) const
{ return subject_map_->getCId( sid, rcid ); }

inline pair< CDbIndex::TSeqNum, CDbIndex::TSeqNum > 
CDbIndex::getSRCId( CDbIndex::TSeqNum cid ) const
{ return subject_map_->getSRCId( cid ); }

inline CDbIndex::TWord CDbIndex::getChunkLength( CDbIndex::TSeqNum cid ) const
{ return subject_map_->getChunkLength( cid ); }

inline CDbIndex::TSeqNum 
CDbIndex::getCIdByLRCId( CDbIndex::TSeqNum lid, CDbIndex::TSeqNum rcid ) const
{ return subject_map_->getCIdByLRCId( lid, rcid ); }

inline pair< CDbIndex::TSeqNum, TSeqPos >
CDbIndex::getRCIdOffByLIdOff( CDbIndex::TSeqNum lid, TSeqPos loff ) const
{ return subject_map_->getRCIdOffByLIdOff( lid, loff ); }

inline TSeqPos CDbIndex::getSOff( 
        CDbIndex::TSeqNum sid, CDbIndex::TSeqNum rcid, TSeqPos coff ) const
{ return subject_map_->getSOff( sid, rcid, coff ); }

inline CDbIndex::TSeqNum CDbIndex::getNumSubjects() const
{ return subject_map_->getNumSubjects(); }

inline CDbIndex::TSeqNum CDbIndex::getNumChunks() const
{ return subject_map_->getNumChunks(); }

inline CDbIndex::TSeqNum CDbIndex::getNumChunks( CDbIndex::TSeqNum sid ) const
{ return subject_map_->getNumChunks( sid ); }

inline const Uint1 * CDbIndex::getSeqData( CDbIndex::TSeqNum sid ) const
{ return subject_map_->getSeqData( sid ); }

inline CDbIndex::TSeqNum CDbIndex::getLId( 
        const CDbIndex::TOffsetValue & v ) const
{ return subject_map_->getLId( v ); }

inline TSeqPos CDbIndex::getLOff( const CDbIndex::TOffsetValue & v ) const
{ return subject_map_->getLOff( v ); }

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

#endif

