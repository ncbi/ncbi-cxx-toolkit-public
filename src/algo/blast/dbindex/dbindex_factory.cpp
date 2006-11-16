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
 *   Implementation of index creation functionality.
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

#endif

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

namespace{

/**@name Useful constants from CDbIndex scope. */
/**@{*/
static const unsigned long CR         = CDbIndex::CR;
static const unsigned long STRIDE     = CDbIndex::STRIDE;
static const unsigned long MIN_OFFSET = CDbIndex::MIN_OFFSET;
static const unsigned long CODE_BITS  = CDbIndex::CODE_BITS;
/**@}*/

//-------------------------------------------------------------------------
/** Class used to report progress during index creation.
*/
class CProgressReporter
{
    public:

        /** Object constructor.
            @param real_level           [I]     reporting level
            @param requested_level      [I]     reporting level threshold
                                                requested by the user
        */
        CProgressReporter( 
                unsigned long real_level,
                unsigned long requested_level )
            : real_level_( real_level ), 
              requested_level_( requested_level )
        {}

        /** Message output operator.
            Sends the given message to standard error stream if
            the reporting level requested by the user is bigger
            than that of the msg itself.
            @param t value to be sent to the error stream
        */
        template< typename T >
        const CProgressReporter & operator<<( const T & t ) const
        { 
            if( requested_level_ >= real_level_ ) 
                std::cerr << t << std::flush; 

            return *this;
        }

    private:

        unsigned long real_level_;      /** Message reporting level. */
        unsigned long requested_level_; /** User requested reporting level. */
};

//-------------------------------------------------------------------------
/** A simple function to report progress of the index creation.
  @param T type of the value being displayed
  @param real_level      the reporting level of <TT>msg</TT>
  @param requested_level the level of reporting requested by the user
  @param t               value to display 
*/
template< typename T >
inline void report_progress(
        unsigned long real_level,
        unsigned long requested_level,
        const T & t )
{ CProgressReporter( real_level, requested_level ) << t; }

//-------------------------------------------------------------------------
/** Convert an integer to hex string representation.
    @param word_t the type of the integer
    @param word [I]     the integer value
    @return string containing the hexadecimal representation of word.
*/
template< typename word_t >
const std::string to_hex_str( word_t word )
{
    std::ostringstream os;
    os << hex << word;
    return os.str();
}

//-------------------------------------------------------------------------
/** Write a word into a binary output stream.
    This functin is endian-dependant and not portable between platforms
    with different endianness.
    @param os output stream; must be open in binary mode
    @param word value to write to the stream
*/
template< typename word_t >
void WriteWord( CNcbiOstream & os, word_t word )
{ os.write( reinterpret_cast< char * >( &word ), sizeof( word_t ) ); }

//-------------------------------------------------------------------------
/** Convertion from IUPACNA to NCBI2NA (+1).
    @param r residue value in IUPACNA
    @return 1 + NCBI2NA value of r, if defined;
            0 otherwise
  */
inline Uint1 base_value( objects::CSeqVectorTypes::TResidue r )
{
    switch( r ) {
        case 'A': return 1;
        case 'C': return 2;
        case 'G': return 3;
        case 'T': return 4;
        default : return 0;
    }
}

//-------------------------------------------------------------------------
/** Part of the CSubjectMap class that is independent of template 
    parameters.
*/
class CSubjectMap_Base
{
    public:

        typedef CSequenceIStream::TSeqData TSeqData;    /**< forwarded type */
        typedef CDbIndex::TSeqNum TSeqNum;              /**< forwarded type */

    protected:

        /** Sequence data without masking. */
        typedef objects::CSeqVector TSeq;

        /** Masking information. */
        typedef CSequenceIStream::TMask TMask;

        /** The inner most type needed to access mask data in the
            representation returned by ReadFasta().
        */
        typedef objects::CSeq_loc::TPacked_int::Tdata TLocs;

        /** Container type used to store compressed sequence information. */
        typedef std::vector< Uint1 > TSeqStore;

        /** Type for storing mapping from subject oids to the chunk numbers. */
        typedef std::vector< TSeqNum > TSubjects;

        /** Type used to store a masked segment internally. */
        struct SSeqSeg
        {
            TSeqPos start_;     /**< Start of the segment. */
            TSeqPos stop_;      /**< One past the end of the segment. */

            /** Object constructor.
                @param start start of the new segment
                @param end one past the end of the new segment
              */
            SSeqSeg( TSeqPos start, TSeqPos stop = 0 )
                : start_( start ), stop_( stop )
            {}
        };

        /** A helper class used when creating internal set masked locations
            in the process of converting the sequence data to NCBI2NA and
            storing it in seq_store_.
        */
        class CMaskHelper : public CObject
        {
            private:

                typedef CSequenceIStream::TMask TMask; /**< forwarded type */

                /** See documentation for CSubjectMap_Base::TLocs. */
                typedef objects::CSeq_loc::TPacked_int::Tdata TLocs;

                /** Collection of TLocs extracted from 
                    CSequenceIStream::TSeqData. 
                */
                typedef std::vector< const TLocs * > TLocsVec;

            public:

                /** Default object constructor. */
                CMaskHelper() {}

                /** Initialize the iterators after the masked locations
                    are added.
                */
                void Init();

                /** Add a set of masked intervals.
                    The data must be in the form of packed intervals.
                    @param loc set of packed intervals to add
                  */
                void Add( const TMask::value_type & loc )
                {
                    if( loc->IsPacked_int() ) {
                        c_locs_.push_back( 
                                &( loc->GetPacked_int().Get() ) );
                    }
                }

                /** Check if a point falls within the intervals stored
                    in the object.
                    @param pos the coordinate in the sequence
                    @return true, if pos belongs to one of the intervals
                        added to the object; false otherwise
                */
                bool In( TSeqPos pos );

                /** Backtrack to the first interval to the left of pos
                    or to the beginning, if not possible.
                    @param pos  [I]     the target position
                */
                void Adjust( TSeqPos pos );

            private:

                /** Check if the end of iteration has been reached.
                    @return true if the end of iteration has not been reached;
                        false otherwise
                */
                bool Good() const { return vit_ != c_locs_.end(); }

                /** Iteration step. */
                void Advance();

                /** Iteration step backwords.
                    @return true, if retreat was successful, false if there is
                            nowhere to retreat
                */
                bool Retreat();

                TLocsVec c_locs_;               /**< Container with sets of masked intervals. */
                TLocsVec::const_iterator vit_;  /**< State of the iterator over c_locs_ (outer iteration). */
                TLocs::const_iterator it_;      /**< State of the iterator over *vit_ (inner iteration). */
                TSeqPos start_;                 /**< Left end of *it_. */
                TSeqPos stop_;                  /**< One past the right end of *it_. */
        };

        /** Maximum internal sequence size.
            When the library is integrated with BLAST, this should
            correspond to the maximum subject chunk size used in BLAST.
          */
        unsigned long chunk_size_;

        /** Length of overlap between consequtive chunks of one sequence.
            When the library is integrated with BLAST, this should
            correspond to the subject chunk overlap length used in BLAST.
          */
        unsigned long chunk_overlap_;

        /** Level of reporting requested by the user. */
        unsigned long report_level_;

        TSeqNum committed_;                     /**< Logical number of the last committed sequence. */
        TSeqNum last_chunk_;                    /**< Logical number of last processed sequence. */
        TSeqNum c_chunk_;                       /**< Current chunk number of the sequence currently being processed. */
        TSeq c_seq_;                            /**< Sequence data of the sequence currently being processed. */
        CRef<objects::CObjectManager> om_;      /**< Reference to the ObjectManager instance. */
        TSeqStore seq_store_;                   /**< Container for storing the packed sequence data. */
        TSubjects subjects_;                    /**< Mapping from subject oid to chunk information. */
        CRef< CMaskHelper > mask_helper_;       /**< Auxiliary object used to compute unmasked parts of the sequences. */

        /** Object constructor. 
            @param chunk_size maximum internal sequecnce size
            @param chunk_overlap amount of overlap between consecutive
                                 chunks of one input sequence
            @param report_level [I]     level of progress reporting requested by the user
        */
        CSubjectMap_Base( 
                unsigned long chunk_size, 
                unsigned long chunk_overlap,
                unsigned long report_level ) 
            : chunk_size_( chunk_size ), chunk_overlap_( chunk_overlap ),
              report_level_( report_level ),
              committed_( 0 ), last_chunk_( 0 ),
              om_( objects::CObjectManager::GetInstance() ),
              seq_store_( STRIDE, 0 ),
              mask_helper_( null )
        {}

        /** Helper function used to extract CSeqVector instance from 
            a TSeqData object.
            The extracted CSeqVector is stored in c_seq_ data member.
            @param sd the object containing the input sequence data
        */
        void extractSeqVector( TSeqData & sd );

    public:

        /** Get the start of the compressed sequence storage space.
            @return start of seq_store_
        */
        const Uint1 * seq_store_start() const { return &seq_store_[0]; }

        /** Start processing of the new input sequence.
            @param sd new input sequence data
            @param start_chunk only store data related to chunks numbered
                               higher than the value of this parameter
        */
        void NewSequenceInit( TSeqData & sd, TSeqNum start_chunk );
};

/** Template declaration for the family of subject map classes.
    The correct specialization of this class is selected based on
    the options given to the index creation procedure.
    @param word_t type of the natural machine word. Used to support
                  32 and 64 bit wide indices.
    @param OFF_TYPE offset representation. This parameter controls
                    how the offset information is encoded in the
                    offset lists.
*/
template< typename word_t, unsigned long OFF_TYPE >
class CSubjectMap {};

/** Specialization of CSubjectMap for raw offsets.
    Raw offsets do not contain subject id information encoded into
    the offset data. This means that a variant of a binary search
    is needed to lookup the subject information for a given raw 
    offset.
*/
template< typename word_t >
class CSubjectMap< word_t, OFFSET_RAW > : public CSubjectMap_Base
{
    public:

        typedef word_t TWord;   /**< Bit width is provided as part of the interface. */

        /** Object constructor.
            @param chunk_size maximum internal sequecnce size
            @param chunk_overlap amount of overlap between consecutive
                                 chunks of one input sequence
            @param report_level [I]     level of progress reporting requested by the user
        */
        CSubjectMap( 
                unsigned long chunk_size, 
                unsigned long chunk_overlap,
                unsigned long report_level ) 
            : CSubjectMap_Base( chunk_size, chunk_overlap, report_level )
        {}

        /** Get the total memory usage by the subject map in bytes.
            @return memory usage by this instance
        */
        TWord total() const { return seq_store_.size(); }

        /** Append the next chunk of the input sequence currently being
            processed to the subject map.
            The return value of false should be used as iteration termination
            condition.
            @return true for success; false if no more chunks were available
        */
        bool AddSequenceChunk();

        /** Remove information about the last added chunk from the 
            subject map.
        */
        void DelLastChunk();

        /** Finalize processing of the current input sequence.
        */
        void Commit();

        /** Get the oid of the last processed sequence.
            This function is used to get the oid of the last added subject
            sequence after the index has been grown to the target size.
            @return oid of the last added (possibly partially) sequence
        */
        TSeqNum GetLastSequence() const { return subjects_.size(); }

        /** Get the oid of the last chunk number of the last processed sequence.
            @return the number of the last successfully added sequence chunk
        */
        TSeqNum GetLastSequenceChunk() const { return c_chunk_; }

        /** Get the internal oid of the last valid sequence.
            This function is used by the offset data management classes
            to see if some sequences need to be reevaluated.
            @return internal oid of the last valid sequence
        */
        TSeqNum LastGoodSequence() const { return last_chunk_; }

        /** Encode an offset given a pointer to the compressed sequence
            data and relative offset.
            @param seq start of the buffer containing the compressed sequence
            @param off offset relative to the start of seq
            @return encoded offset that can be added to an offset list
        */
        TWord MakeOffset( const Uint1 * seq, TSeqPos off ) const;

        /** Encode an offset given an internal oid and relative offset.
            @param seq internal oid of a sequence
            @param off offset relative to the start of seq
            @return encoded offset that can be added to an offset list
        */
        TWord MakeOffset( TSeqNum seq, TSeqPos off ) const;

    private:

        /** Information about the sequence chunk. */
        struct SSeqInfo
        {
            /** Type containing the valid intervals. */
            typedef std::vector< SSeqSeg > TSegs;

            /** Object constructor.
                @param start start of the compressed sequence data
                @param len   length of the sequence
                @param segs  valid intervals
            */
            SSeqInfo( 
                    TWord start = 0, 
                    TWord len = 0,
                    const TSegs & segs  = TSegs() )
                : seq_start_( start ), len_( len ), segs_( segs )
            {}

            TWord seq_start_;           /**< Start of the compressed sequence data. */
            TWord len_;                 /**< Sequence length. */
            TSegs segs_;                /**< Valid intervals, i.e. everything
                                             except masked and ambiguous bases. */
        };

        /** Type for the collection of sequence chunks. */
        typedef std::vector< SSeqInfo > TChunks;

        /** Collection of sequence chunks (or logical sequences).
            For raw offsets the logical oid of the sequence is
            its index in this collectin.
        */
        TChunks chunks_;

    public:

        typedef SSeqInfo TSeqInfo; /**< Type definition for external users. */
        typedef SSeqSeg TSeqSeg;   /**< Type definition for external users. */


        /** Get the chunk info by internal oid
            @param snum internal oid of the sequence
            @return requested sequence information or NULL if no sequence
                    corresponding to snum exists
        */
        const TSeqInfo * GetSeqInfo( TSeqNum snum ) const
        { 
            if( snum > last_chunk_ ) {
                return 0;
            }else {
                return &chunks_[snum - 1];
            }
        }

        /** Save the subject map and sequence info.
            @param os output stream open in binary mode
            @param version index format version
        */
        void Save( CNcbiOstream & os, unsigned long version ) const;

        /** Revert to the state before the start of processing of the 
            current input sequence.
        */
        void RollBack();
};

//-------------------------------------------------------------------------
void CSubjectMap_Base::CMaskHelper::Init()
{
    vit_ = c_locs_.begin();

    while( vit_ != c_locs_.end() ) {
        it_ = (*vit_)->begin();

        if( it_ != (*vit_)->end() ) {
            start_ = (*it_)->GetFrom();
            stop_  = (*it_)->GetTo() + 1;
            break;
        }

        ++vit_;
    }
}

//-------------------------------------------------------------------------
void CSubjectMap_Base::CMaskHelper::Advance()
{
    while( Good() ) {
        if( ++it_ != (*vit_)->end() ) {
            start_ = (*it_)->GetFrom();
            stop_  = (*it_)->GetTo() + 1;
            return;
        }

        ++vit_;
        if( Good() ) it_ = (*vit_)->begin();
    }
}

//-------------------------------------------------------------------------
void CSubjectMap_Base::CMaskHelper::Adjust( TSeqPos pos )
{
    bool notdone;

    do{
        notdone = Retreat();
    }while( notdone && pos < stop_ );
}

//-------------------------------------------------------------------------
bool CSubjectMap_Base::CMaskHelper::Retreat()
{
    if( c_locs_.empty() ) return false;

    if( !Good() ) {
        --vit_;

        while( vit_ != c_locs_.begin() && (*vit_)->empty() ) {
            --vit_;
        }

        if( !(*vit_)->empty() ) {
            it_ = (*vit_)->end();
            --it_;
            start_ = (*it_)->GetFrom();
            stop_  = (*it_)->GetTo() + 1;
            return true;
        }

        vit_ = c_locs_.end();
        return false;
    }

    if( it_ != (*vit_)->begin() ) {
        --it_;
        start_ = (*it_)->GetFrom();
        stop_  = (*it_)->GetTo() + 1;
        return true;
    }

    if( vit_ == c_locs_.begin() ) {
        Init();
        return false;
    }

    --vit_;

    while( vit_ != c_locs_.begin() && (*vit_)->empty() ) {
        --vit_;
    }

    if( !(*vit_)->empty() ) {
        it_ = (*vit_)->end();
        --it_;
        start_ = (*it_)->GetFrom();
        stop_  = (*it_)->GetTo() + 1;
        return true;
    }

    Init();
    return false;
}

//-------------------------------------------------------------------------
bool CSubjectMap_Base::CMaskHelper::In( TSeqPos pos )
{
    while( Good() && pos >= stop_ ) Advance();
    if( !Good() ) return false;
    return pos >= start_;
}

//-------------------------------------------------------------------------
void CSubjectMap_Base::extractSeqVector( TSeqData & sd )
{
    objects::CSeq_entry * entry = sd.seq_entry_.GetPointerOrNull();

    if( entry == 0 || 
            entry->Which() != objects::CSeq_entry_Base::e_Seq ) {
        NCBI_THROW( 
                CDbIndex_Exception, eBadOption, 
                "input seq-entry is NULL or not a sequence" );
    }

    objects::CScope scope( *om_ );
    objects::CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry( *entry );
    objects::CBioseq_Handle bsh = seh.GetSeq();
    c_seq_ = bsh.GetSeqVector( objects::CBioseq_Handle::eCoding_Iupac );
}

//-------------------------------------------------------------------------
void CSubjectMap_Base::NewSequenceInit(
        TSeqData & sd, TSeqNum start_chunk )
{
    subjects_.push_back( 0 );
    c_chunk_ = start_chunk;

    if( sd ) {
        extractSeqVector( sd );
        TMask & mask = sd.mask_locs_;
        mask_helper_.Reset( new CMaskHelper );

        for( TMask::const_iterator mask_it = mask.begin();
                mask_it != mask.end(); ++mask_it ) {
            mask_helper_->Add( *mask_it );
        }

        mask_helper_->Init();
    }

    CProgressReporter( REPORT_VERBOSE, report_level_ ) 
        << "SN: " << subjects_.size() - 1 << "\n";
}

//-------------------------------------------------------------------------
template< typename word_t >
void CSubjectMap< word_t, OFFSET_RAW >::Save( 
        CNcbiOstream & os, unsigned long version ) const
{
    TWord tmp = subjects_.size();
    TWord subject_map_size = 
        tmp*sizeof( TWord ) +
        chunks_.size()*sizeof( TWord );
    WriteWord( os, subject_map_size );

    for( TSubjects::const_iterator cit = subjects_.begin();
            cit != subjects_.end(); ++cit ) {
        if( version == 1 ) WriteWord( os, *cit );
        else if( version >= 2 ) WriteWord( os, (TWord)(*cit) );
    }

    for( typename TChunks::const_iterator cit = chunks_.begin();
            cit != chunks_.end(); ++cit ) {
        WriteWord( os, cit->seq_start_ );
    }

    if( version >= 2 ) WriteWord( os, (TWord)(seq_store_.size()) );

    if( version == 1 ) WriteWord( os, seq_store_.size() );
    else if( version >= 2 ) WriteWord( os, (TWord)(seq_store_.size()) );

    os.write( (char *)(&seq_store_[0]), seq_store_.size() );
    os << std::flush;
}

//-------------------------------------------------------------------------
template< typename word_t >
bool CSubjectMap< word_t, OFFSET_RAW >::AddSequenceChunk()
{
    TSeqPos chunk_start = (chunk_size_ - chunk_overlap_)*(c_chunk_++);
    TSeq::size_type seqlen = c_seq_.size();

    if( chunk_start >= seqlen ) {
        --c_chunk_;
        return false;
    }

    TSeqStore::size_type seq_off = seq_store_.size();
    TSeqPos chunk_end = 
        std::min( (TSeqPos)(chunk_start + chunk_size_), c_seq_.size() );
    TSeqPos chunk_len = chunk_end - chunk_start;
    typename SSeqInfo::TSegs segs;

    if( chunk_len > 0 ) {
        seq_store_.reserve( seq_store_.size() + (chunk_len - 1)/CR + 1 );
        Uint1 accum = 0;
        unsigned int lc = 0;
        bool in = false, in1;
        mask_helper_->Adjust( chunk_start );

        for( TSeqPos pos = chunk_start; 
                pos < chunk_end; ++pos, lc = (lc + 1)%CR ) {
            Uint1 letter = base_value( c_seq_[pos] );

            if( letter == 0 ) {
                in1 = true;
            }else {
                in1 = false;
                --letter;
            }

            accum = (accum << 2) + letter;

            if( lc == 3 ) {
                seq_store_.push_back( accum );
            }

            in1 = (in1 || mask_helper_->In( pos ));

            if( in1 && !in ) {
                if( segs.empty() ) {
                    segs.push_back( SSeqSeg( 0 ) );
                }

                segs.rbegin()->stop_ = pos - chunk_start;
                in = true;
            }else if( !in1 && in ) {
                segs.push_back( SSeqSeg( pos - chunk_start ) );
                in = false;
            }
        }

        if( !in ) {
            if( segs.empty() ) {
                segs.push_back( SSeqSeg( 0 ) );
            }

            segs.rbegin()->stop_ = chunk_end - chunk_start;
        }

        if( lc != 0 ) {
            accum <<= (CR - lc)*2;
            seq_store_.push_back( accum );
        }

        while( seq_store_.size()%STRIDE != 0 ) {
            seq_store_.push_back( 0 );
        }
    }

    chunks_.push_back( 
            TSeqInfo( seq_off, seqlen, segs ) );
    
    if( *subjects_.rbegin() == 0 ) {
        *subjects_.rbegin() = chunks_.size();
        CProgressReporter( REPORT_VERBOSE, report_level_ ) 
            << "SC: " << subjects_.size() - 1 << " "
            << chunks_.size() << " " 
            << seq_off*CR << "\n";
    }

    last_chunk_ = chunks_.size();
    CProgressReporter( REPORT_VERBOSE, report_level_ ) 
        << "CN: " << last_chunk_ 
        << " " << chunk_overlap_*(c_chunk_ - 1) << "\n";
    return true;
}

//-------------------------------------------------------------------------
template< typename word_t >
void CSubjectMap< word_t, OFFSET_RAW >::RollBack()
{
    CProgressReporter( REPORT_VERBOSE, report_level_ ) << "ROLLBACK: ";

    if( !subjects_.empty() ) {
        last_chunk_ = *subjects_.rbegin() - 1;
        c_chunk_ = 0;
        *subjects_.rbegin() = 0;
        CProgressReporter( REPORT_VERBOSE, report_level_ ) 
            << subjects_.size() - 1 << " "
            << last_chunk_ << "\n";
    }
}

//-------------------------------------------------------------------------
template< typename word_t >
void CSubjectMap< word_t, OFFSET_RAW >::DelLastChunk()
{
    CProgressReporter( REPORT_VERBOSE, report_level_ ) << "DC: ";

    if( !subjects_.empty() ) {
        if( *subjects_.rbegin() == chunks_.size() ) {
            RollBack();
        }else {
            last_chunk_ = chunks_.size() - 1;
            --c_chunk_;
            CProgressReporter( REPORT_VERBOSE, report_level_ ) 
                << last_chunk_;
        }
    }

    CProgressReporter( REPORT_VERBOSE, report_level_ ) << "\n";
}

//-------------------------------------------------------------------------
template< typename word_t >
void CSubjectMap< word_t, OFFSET_RAW >::Commit()
{
    if( last_chunk_ < chunks_.size() ) {
        TSeqStore::size_type newsize = 
            (TSeqStore::size_type)(chunks_[last_chunk_].seq_start_);
        seq_store_.resize( newsize );
        chunks_.resize( last_chunk_ );
    }

    committed_ = last_chunk_;
    CProgressReporter( REPORT_VERBOSE, report_level_ )
        << "COMMIT: " << subjects_.size() - 1 << " "
        << chunks_.size() << " " << seq_store_.size() << "\n";
}

//-------------------------------------------------------------------------
template< typename word_t >
inline word_t CSubjectMap< word_t, OFFSET_RAW >::MakeOffset(
        const Uint1 * seq, TSeqPos off ) const
{ return MIN_OFFSET + (CR*((TWord)(seq - &seq_store_[0])) + off)/STRIDE; }

//-------------------------------------------------------------------------
template< typename word_t >
inline word_t CSubjectMap< word_t, OFFSET_RAW >::MakeOffset(
        TSeqNum seqnum, TSeqPos off ) const
{
    const Uint1 * seq = &seq_store_[0] + chunks_[seqnum].seq_start_;
    return MakeOffset( seq, off );
}

//-------------------------------------------------------------------------
/** Type representing an offset list corresponding to an Nmer. 
    See documentation of COffsetData classes for the description 
    of template parameters.
*/
template< typename word_t, unsigned long COMPRESSION >
class COffsetList {};

/** Specialized version for uncompressed offsets. */
template< typename word_t >
class COffsetList< word_t, UNCOMPRESSED >
{
    private:

        typedef word_t TWord;   /**< Renamed for consistency. */

    public:

        /** Add an offset to the list. Update the total.
            @param item offset to be appended to the list
            @total [I/O] change in the length of the list will
                         be applied to this argument
        */
        void AddData( TWord item, TWord & total );

        /** Truncate the list to the value of offset. Update the total.
            The function removes the tail of the list corresponding
            to elements that are at least as great as offset.
            @param offset offset value threshold
            @total [I/O] change in the length of the list will
                         be applied to this argument
        */
        void TruncateList( TWord offset, TWord & total );

        /** Return the size of the offset list in words.
            @return size of the list in words
        */
        TWord Size() const { return (TWord)(data_.size()); }

        /** Save the offset list.
            @param os output stream open in binary mode
            @param version index format version
        */
        void Save( CNcbiOstream & os, unsigned long version ) const;

    private:

        /** Type used to store offset list data. */
        typedef std::vector< TWord > TData;

        TData data_;    /**< Offset list data storage. */
};

//-------------------------------------------------------------------------
template< typename word_t >
inline void COffsetList< word_t, UNCOMPRESSED >::Save( 
        CNcbiOstream & os, unsigned long version ) const
{
    if( version <= 3 ) {
        for( typename TData::const_iterator cit = data_.begin(); 
                cit != data_.end(); ++cit ) {
            WriteWord( os, *cit );
        }
    }
    else if( version == 4 ) {
        for( typename TData::const_iterator cit = data_.begin(); 
                cit != data_.end(); ++cit ) {
            if( *cit < MIN_OFFSET ) {
                WriteWord( os, *cit );
                WriteWord( os, *(++cit) );
            }
            else if( (*cit)%3 == 0 ) {
                WriteWord( os, *cit );
            }
        }

        for( typename TData::const_iterator cit = data_.begin(); 
                cit != data_.end(); ++cit ) {
            if( (*cit) < MIN_OFFSET ) {
                ++cit;
            }
            else if( (*cit)%3 != 0 && (*cit)%2 == 0 ) {
                WriteWord( os, *cit );
            }
        }

        for( typename TData::const_iterator cit = data_.begin(); 
                cit != data_.end(); ++cit ) {
            if( (*cit) < MIN_OFFSET ) {
                ++cit;
            }
            else if( (*cit)%3 != 0 && (*cit)%2 != 0 ) {
                WriteWord( os, *cit );
            }
        }
    }

    if( version >= 3 && !data_.empty() ) {
        WriteWord( os, (word_t)0 );
    }
}

//-------------------------------------------------------------------------
template< typename word_t >
inline void COffsetList< word_t, UNCOMPRESSED >::AddData( 
        TWord item, TWord & total )
{
    data_.push_back( item );
    ++total;
}

//-------------------------------------------------------------------------
template< typename word_t >
inline void COffsetList< word_t, UNCOMPRESSED >::TruncateList(
        TWord offset, TWord & total )
{
    bool flag = false;

    for( typename TData::size_type i = 0; i < data_.size(); ++i ) {
        if( data_[i] < MIN_OFFSET ) {
            flag = true;
            continue;
        }

        if( data_[i] >= offset ) {
            if( flag ) {
                --i;
            }

            typename TData::size_type diff = data_.size() - i;
            data_.resize( i );
            total -= diff;
            return;
        }else {
            flag = false;
        }
    }
}

//-------------------------------------------------------------------------
/** Family of classes responsible for creation and management of Nmer
    offset lists.
    The correct specialization of this class is selected based on
    the options given to the index creation procedure.
    @param word_t type of the natural machine word. Used to support
                  32 and 64 bit wide indices.
    @param subject_map_t type prividing mapping from internal oids to
                         sequence data
    @param COMPRESSION type of compression used for offset lists
*/
template< 
    typename word_t, typename subject_map_t, unsigned long COMPRESSION >
class COffsetData 
{
    public:

        typedef subject_map_t TSubjectMap;      /**< Rename for consistency. */
        typedef word_t TWord;                   /**< Rename for consistency. */

        /** Object constructor.
            @param hkey_width size of the Nmer in bases
            @param subject_map structure to use to map logical oids to the
                               actual sequence data
            @param report_level level of verbosity requested by the user
        */
        COffsetData( 
                TSubjectMap & subject_map, 
                const CDbIndex::SOptions & options )
            : subject_map_( subject_map ),
              hash_table_( 1<<(2*options.hkey_width) ),
              report_level_( options.report_level ),
              total_( 0 ),
              hkey_width_( options.hkey_width ),
              last_seq_( 0 ),
              options_( options )
        {}

        /** Get the total memory usage by offset lists in bytes.
            @return memory usage by this instance
        */
        const TWord total() const { return total_; }

        /** Bring offset lists up to date with the corresponding
            subject map instance.
        */
        void Update();

        /** Save the offset lists into the binary output stream.
            @param os output stream; must be open in binary mode
            @param version index format version
        */
        void Save( CNcbiOstream & os, unsigned long version );

    private:

        /** Type used for individual offset lists. */
        typedef COffsetList< word_t, COMPRESSION > TOffsetList;

        typedef CDbIndex::TSeqNum TSeqNum;                      /**< Forwarding from CDbIndex. */
        typedef typename TSubjectMap::TSeqInfo TSeqInfo;        /**< Forwarding from TSubjectMap. */

        /** Type used for mapping Nmer values to corresponding 
            offset lists. 
        */
        typedef std::vector< TOffsetList > THashTable;

        /** Truncate the offset lists according to the information
            from the subject map.
            Checks if the last oid for which information is added
            to the offset lists is more than the last valid oid
            in the subject map and erases extraenious information.
        */
        void Truncate();

        /** Update offset lists with information corresponding to
            the given sequence.
            @param sinfo new sequence information
        */
        void AddSeqInfo( const TSeqInfo & sinfo );

        /** Update offset lists with information corresponding to
            the given valid segment of a sequence.
            @param seq points to the start of the sequence
            @param seqlen length of seq
            @param start start of the segment
            @param stop one past the end of the segment
        */
        void AddSeqSeg( 
                const Uint1 * seq, TWord seqlen,
                TSeqPos start, TSeqPos stop );

        /** Encode the offset data and add to the offset list 
            corresponding to the given Nmer value.
            @param nmer the Nmer value
            @param start start of the current valid segment
            @param stop one past the end of the current valid segment
            @param curr end of the Nmer within the sequence
            @param offset offset encoded with subject map instance
        */
        void EncodeAndAddOffset( 
                TWord nmer,
                TSeqPos start, TSeqPos stop,
                TSeqPos curr, TWord offset );

        TSubjectMap & subject_map_;     /**< Instance of subject map structure. */
        THashTable hash_table_;         /**< Mapping from Nmer values to the corresponding offset lists. */
        unsigned long report_level_;    /**< Level of reporting requested by the user. */
        TWord total_;                   /**< Current size of the structure in bytes. */
        unsigned long hkey_width_;      /**< Nmer width in bases. */
        TSeqNum last_seq_;              /**< Logical oid of last processed sequence. */

        const CDbIndex::SOptions & options_; /**< Index options. */
};

//-------------------------------------------------------------------------
template< 
    typename word_t, typename subject_map_t, unsigned long COMPRESSION >
void COffsetData< word_t, subject_map_t, COMPRESSION >::Save(
        CNcbiOstream & os, unsigned long version ) 
{
    if( version >= 3 ) { // Adjust total_ to include end zeroes.
        ++this->total_;

        for( typename THashTable::const_iterator cit = hash_table_.begin();
                cit != hash_table_.end(); ++cit ) {
            if( cit->Size() > 0 ) ++this->total_;
        }
    }

    bool stat = !options_.stat_file_name.empty();
    std::auto_ptr< CNcbiOfstream > stats;

    if( stat ) {
        stats.reset( 
                new CNcbiOfstream( options_.stat_file_name.c_str() ) );
    }

    WriteWord( os, total() );
    TWord tot = 0;
    unsigned long nmer = 0;

    for( typename THashTable::const_iterator cit = hash_table_.begin();
            cit != hash_table_.end(); ++cit, ++nmer ) {
        if( version >= 3 && cit->Size() != 0 ) {
            ++tot;
        }

        if( version < 3 || (version >= 3 && cit->Size() != 0) ) 
            WriteWord( os, tot );
        else WriteWord( os, (word_t)0 );

        tot += cit->Size();

        if( stat && cit->Size() > 0 ) {
            *stats << hex << setw( 10 ) << nmer 
                   << " " << dec << cit->Size() << endl;
        }
    }

    if( version >= 2 ) WriteWord( os, total() );
    if( version >= 3 ) WriteWord( os, (word_t)0 ); // offset list data starts
                                                   //     zero entry

    for( typename THashTable::const_iterator cit = hash_table_.begin();
            cit != hash_table_.end(); ++cit ) {
        cit->Save( os, version );
    }

    os << std::flush;
}

//-------------------------------------------------------------------------
template< 
    typename word_t, typename subject_map_t, unsigned long COMPRESSION >
void 
COffsetData< word_t, subject_map_t, COMPRESSION >::EncodeAndAddOffset(
        TWord nmer, TSeqPos start, TSeqPos stop, 
        TSeqPos curr, TWord offset )
{
    CProgressReporter( REPORT_VERBOSE, report_level_ ) << "OA: ";
    TSeqPos start_diff = curr + 2 - hkey_width_ - start;
    TSeqPos end_diff = stop - curr;

    if( start_diff <= STRIDE || end_diff <= STRIDE ) {
        if( start_diff > STRIDE ) start_diff = 0;
        if( end_diff > STRIDE ) end_diff = 0;
        TWord code = (start_diff<<CODE_BITS) + end_diff;
        hash_table_[(typename THashTable::size_type)nmer].AddData( 
                code, total_ );
        CProgressReporter( REPORT_VERBOSE, report_level_ ) 
            << to_hex_str( code );
    }

    hash_table_[(typename THashTable::size_type)nmer].AddData( 
            offset, total_ );
    CProgressReporter( REPORT_VERBOSE, report_level_ ) 
        << " " << offset << " " << to_hex_str( nmer ) << "\n";
}

//-------------------------------------------------------------------------
template< 
    typename word_t, typename subject_map_t, unsigned long COMPRESSION >
void COffsetData< word_t, subject_map_t, COMPRESSION >::AddSeqSeg(
        const Uint1 * seq, TWord , TSeqPos start, TSeqPos stop )
{
    CProgressReporter( REPORT_VERBOSE, report_level_ ) 
        << "SEGMENT: " << start << " " << stop << "\n";
    const TWord nmer_mask = (((TWord)1)<<(2*hkey_width_)) - 1;
    const Uint1 letter_mask = 0x3;
    TWord nmer = 0;
    unsigned long count = 0;

    for( TSeqPos curr = start; curr < stop; ++curr, ++count ) {
        Uint1 unit = seq[curr/CR];
        Uint1 letter = ((unit>>(6 - 2*(curr%CR)))&letter_mask);
        nmer = ((nmer<<2)&nmer_mask) + letter;

        if( count >= hkey_width_ - 1 ) {
            if( curr%STRIDE == 0 ) {
                TWord offset = subject_map_.MakeOffset( seq, curr );
                EncodeAndAddOffset( nmer, start, stop, curr, offset );
            }
        }
    }
}

//-------------------------------------------------------------------------
template< 
    typename word_t, typename subject_map_t, unsigned long COMPRESSION >
void COffsetData< word_t, subject_map_t, COMPRESSION >::AddSeqInfo(
        const TSeqInfo & sinfo )
{
    for( typename TSeqInfo::TSegs::const_iterator it = sinfo.segs_.begin();
            it != sinfo.segs_.end(); ++it ) {
        AddSeqSeg( 
                subject_map_.seq_store_start() + sinfo.seq_start_, 
                sinfo.len_, it->start_, it->stop_ );
    }
}

//-------------------------------------------------------------------------
template< 
    typename word_t, typename subject_map_t, unsigned long COMPRESSION >
void COffsetData< word_t, subject_map_t, COMPRESSION >::Truncate()
{
    last_seq_ = subject_map_.LastGoodSequence();
    TWord offset = subject_map_.MakeOffset( last_seq_, 0 );
    CProgressReporter( REPORT_VERBOSE, report_level_ ) 
        << "TR: " << offset << "\n";

    for( typename THashTable::iterator it = hash_table_.begin();
            it != hash_table_.end(); ++it ) {
        it->TruncateList( offset, total_ );
    }
}

//-------------------------------------------------------------------------
template< 
    typename word_t, typename subject_map_t, unsigned long COMPRESSION >
void COffsetData< word_t, subject_map_t, COMPRESSION >::Update()
{
    if( subject_map_.LastGoodSequence() < last_seq_ ) {
        Truncate();
        report_progress( 
                REPORT_NORMAL, report_level_, 
                std::string( 
                    last_seq_ - subject_map_.LastGoodSequence(), '-' ) );
    }

    const TSeqInfo * sinfo;

    while( (sinfo = subject_map_.GetSeqInfo( last_seq_ + 1 )) != 0 ) {
        CProgressReporter( REPORT_VERBOSE, report_level_ ) 
            << "AS: " << last_seq_ + 1 << "\n";
        AddSeqInfo( *sinfo );
        ++last_seq_;
        report_progress( REPORT_NORMAL, report_level_, "+" );
    }
}

} // Anonymous namespace

//-------------------------------------------------------------------------
/** Index implementation.
  */
template< unsigned long WIDTH >
class CDbIndex_Factory : public CDbIndex
{
    private:

        /** Integral type representing the unit of the index hash table. */
        typedef typename SWord< WIDTH >::type TWord;

        static const TWord MEGABYTE = 1024*1024;        /**< Obvious... */

    public:

        /** Create an index implementation object.

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
        static void Create( 
                CSequenceIStream & input,
                const std::string & oname,
                TSeqNum start, TSeqNum start_chunk,
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options
        );

        /** Object destructor. */
        virtual ~CDbIndex_Factory();

    private:

        /** Save the index header.
            @param os          output stream open in binary mode
            @param version     index format version
            @param hkey_width  length of the hash key in bases
            @param start       oid of the first sequence in the index
            @param start_chunk chunk number of the first chunk of the first sequence
            @param stop        oid of the last sequence in the index
            @param stop_chunk  chunk number of the last chunk of the last sequence
        */
        template< unsigned long OFF_TYPE, unsigned long COMPRESSION >
        static void SaveHeader(
                CNcbiOstream & os,
                unsigned long version,
                unsigned long hkey_width,
                TSeqNum start,
                TSeqNum start_chunk,
                TSeqNum stop,
                TSeqNum stop_chunk );

        /** Specialization of the index creation function for given
            offset type and compression method.
            For description of parameters see CDbIndex_Factory::Create().
        */
        template< unsigned long OFF_TYPE, unsigned long COMPRESSION >
        static void do_create(
                CSequenceIStream & input, const std::string & oname,
                TSeqNum start, TSeqNum start_chunk,
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options
        );

        /** Index creation function for index format version 1 and 2.
        */
        template< unsigned long OFF_TYPE, unsigned long COMPRESSION >
        static void do_create_1_2(
                CSequenceIStream & input, const std::string & oname,
                TSeqNum start, TSeqNum start_chunk,
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options
        );
};

//-------------------------------------------------------------------------
template< unsigned long WIDTH >
template< unsigned long OFF_TYPE, unsigned long COMPRESSION >
void CDbIndex_Factory< WIDTH >::SaveHeader(
                CNcbiOstream & os,
                unsigned long version,
                unsigned long hkey_width,
                TSeqNum start,
                TSeqNum start_chunk,
                TSeqNum stop,
                TSeqNum stop_chunk )
{
    ASSERT( version == 1 || version == 2 || version == 3 || version == 4 );

    switch( version ) {
        case 1:

        WriteWord( os, (unsigned char)version );
        WriteWord( os, (unsigned char)WIDTH );
        WriteWord( os, (unsigned char)hkey_width );
        WriteWord( os, (unsigned char)OFF_TYPE );
        WriteWord( os, (unsigned char)COMPRESSION );
        WriteWord( os, start );
        WriteWord( os, start_chunk );
        WriteWord( os, stop );
        WriteWord( os, stop_chunk );
        os << std::flush;
        break;

        case 2: case 3: case 4:

        {
            WriteWord( os, (unsigned char)version );
            for( int i = 0; i < 7; ++i ) WriteWord( os, (unsigned char)0 );
            WriteWord( os, (Uint8)WIDTH );
            WriteWord( os, (TWord)hkey_width );
            WriteWord( os, (TWord)OFF_TYPE );
            WriteWord( os, (TWord)COMPRESSION );
            WriteWord( os, (TWord)start );
            WriteWord( os, (TWord)start_chunk );
            WriteWord( os, (TWord)stop );
            WriteWord( os, (TWord)stop_chunk );
            os << std::flush;
        }

        break;

        default: break;
    }
}

//-------------------------------------------------------------------------
/* The method performs initial analysis of the input data and requested
   index options and chooses the appropriately optimized index creation
   routine.
 */
template< unsigned long WIDTH >
void CDbIndex_Factory< WIDTH >::Create(
        CSequenceIStream & input, const std::string & oname,
        TSeqNum start, TSeqNum start_chunk,
        TSeqNum & stop, TSeqNum & stop_chunk, const SOptions & options )
{
    report_progress( 
            REPORT_VERBOSE, options.report_level,
            std::string( "Creating " ) +
            (WIDTH == WIDTH_32 ? "32" : "64") +
            "-bit wide index.\n" );
    do_create< OFFSET_RAW, UNCOMPRESSED >(
            input, oname, start, start_chunk, stop, stop_chunk, options );
}

//-------------------------------------------------------------------------
template< unsigned long WIDTH >
template< unsigned long OFF_TYPE, unsigned long COMPRESSION >
void CDbIndex_Factory< WIDTH >::do_create(
        CSequenceIStream & input, const std::string & oname,
        TSeqNum start, TSeqNum start_chunk,
        TSeqNum & stop, TSeqNum & stop_chunk, const SOptions & options )
{
    switch( options.version ) {
        case 1: case 2: case 3: case 4:

            do_create_1_2< OFF_TYPE, COMPRESSION >(
                input, oname, start, start_chunk, 
                stop, stop_chunk, options );
            break;

        default: break;
    }
}

//-------------------------------------------------------------------------
template< unsigned long WIDTH >
template< unsigned long OFF_TYPE, unsigned long COMPRESSION >
void CDbIndex_Factory< WIDTH >::do_create_1_2(
        CSequenceIStream & input, const std::string & oname,
        TSeqNum start, TSeqNum start_chunk,
        TSeqNum & stop, TSeqNum & stop_chunk, const SOptions & options )
{
    typedef CSubjectMap< TWord, OFF_TYPE > TSubjectMap;
    typedef COffsetData< TWord, TSubjectMap, COMPRESSION > TOffsetData;

    TSubjectMap subject_map( 
            options.chunk_size, options.chunk_overlap, options.report_level );
    TOffsetData offset_data( subject_map, options );

    input.rewind( start );
    TSeqNum i = start;

    if( i >= stop ) {
        stop = start;
        return;
    }

    while( i < stop ) {
        CProgressReporter( REPORT_VERBOSE, options.report_level ) 
            << "SUBJECT: " << i - 1 << "\n";

        typedef CSequenceIStream::TSeqData TSeqData;

        CRef< TSeqData > seq_data( input.next() );
        TSeqData * sd = seq_data.GetNonNullPointer();
        subject_map.NewSequenceInit( *sd, start_chunk );

        if( !*sd ) {
            if( i == start ) {
                stop = start;
                return;
            }

            stop = i;
            stop_chunk = 0;
            break;
        }

        while( subject_map.AddSequenceChunk() ) {
            offset_data.Update();
            TWord total = subject_map.total() + 
                sizeof( TWord )*offset_data.total();

            if( total > MEGABYTE*options.max_index_size ) {
                if( options.whole_seq == WHOLE_SEQ ) {
                    subject_map.RollBack();
                }else {
                    subject_map.DelLastChunk();
                }

                offset_data.Update();
                subject_map.Commit();
                stop = start + subject_map.GetLastSequence() - 1;
                stop_chunk = subject_map.GetLastSequenceChunk();
                break;
            }
        }

        subject_map.Commit();
        start_chunk = 0;
        ++i;
    }

    report_progress( REPORT_NORMAL, options.report_level, "\n" );

    {
        std::ostringstream os;
        os << "Last processed: sequence " 
           << start + subject_map.GetLastSequence() - 1
           << " ; chunk " << subject_map.GetLastSequenceChunk() 
           << std::endl;
        report_progress( REPORT_NORMAL, options.report_level, os.str() );
    }

    {
        std::ostringstream os;
        os << "Index size: " 
           << subject_map.total() + sizeof( TWord )*offset_data.total() 
           << " bytes (not counting the hash table)." << std::endl;
        report_progress( REPORT_NORMAL, options.report_level, os.str() );
    }

    report_progress( REPORT_NORMAL, options.report_level, "Saving.. " );
    CNcbiOfstream os( oname.c_str(), IOS_BASE::binary );
    SaveHeader< OFF_TYPE, COMPRESSION >( 
            os, options.version, options.hkey_width, 
            start, start_chunk, stop, stop_chunk );
    report_progress( REPORT_NORMAL, options.report_level, "header.." );
    offset_data.Save( os, options.version );
    report_progress( 
            REPORT_NORMAL, options.report_level, "offset data.." );
    subject_map.Save( os, options.version );
    report_progress( 
            REPORT_NORMAL, options.report_level, "sequence data.\n" );
}

//-------------------------------------------------------------------------
void CDbIndex::MakeIndex(
    const std::string & fname, const std::string & oname, 
    TSeqNum start, TSeqNum start_chunk, 
    TSeqNum & stop, TSeqNum & stop_chunk, const SOptions & options )
{
    // Make an CSequenceIStream out of fname and forward to
    // MakeIndex( CSequenceIStream &, ... ).
    switch( options.input_type ) {
        case FASTA:

            {
                CSequenceIStreamFasta input( fname ); 
                MakeIndex( 
                        input, oname, start, start_chunk, 
                        stop, stop_chunk, options );
            }

            break;

        default:

            {
                std::ostringstream str;
                str << "unknown option " << options.input_type;
                NCBI_THROW( CDbIndex_Exception, eBadOption, str.str() );
            }
    }
}

//-------------------------------------------------------------------------
void CDbIndex::MakeIndex( 
        const std::string & fname, const std::string & oname,
        TSeqNum start, TSeqNum & stop, const SOptions & options )
{
    TSeqNum t;  // unused 
    MakeIndex( fname, oname, start, stop, t, options );
}

//-------------------------------------------------------------------------
void CDbIndex::MakeIndex(
    CSequenceIStream & input, const std::string & oname, 
    TSeqNum start, TSeqNum start_chunk,
    TSeqNum & stop, TSeqNum & stop_chunk, const SOptions & options )
{
    if( options.bit_width == WIDTH_32 ) {
        typedef CDbIndex_Factory< WIDTH_32 > TIndex_Impl;
        TIndex_Impl::Create( 
                input, oname,
                start, start_chunk, stop, stop_chunk, options );
    }else if( options.bit_width == WIDTH_64 ) {
        typedef CDbIndex_Factory< WIDTH_64 > TIndex_Impl;
        TIndex_Impl::Create( 
                input, oname, 
                start, start_chunk, stop, stop_chunk, options );
    }else {
        NCBI_THROW( CDbIndex_Exception, eBadOption, "wrong index width" );
    }
}

//-------------------------------------------------------------------------
void CDbIndex::MakeIndex( 
    CSequenceIStream & input, const std::string & oname,
    TSeqNum start, TSeqNum & stop, const SOptions & options )
{
    TSeqNum t; // unused
    MakeIndex( input, oname, start, stop, t, options );
}

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

