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
#include <objmgr/util/sequence.hpp>
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

/**@name Useful constants from CDbIndex scope. */
/**@{*/
static const unsigned long CR         = CDbIndex::CR;
/**@}*/

/** Alias for CDbIndex::TWord type. */
typedef CDbIndex::TWord TWord;

/** Alias for index creation options. */
typedef CDbIndex::SOptions TOptions;

//-------------------------------------------------------------------------
/** Convert an integer to hex string representation.
    @param word [I]     the integer value
    @return string containing the hexadecimal representation of word.
*/
const std::string to_hex_str( TWord word )
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
/** Part of the CSubjectMap_Factory class that is independent of template 
    parameters.
*/
class CSubjectMap_Factory_Base
{
    public:

        typedef CSequenceIStream::TSeqData TSeqData;    /**< forwarded type */
        typedef CDbIndex::TSeqNum TSeqNum;              /**< forwarded type */

        /** Type used to store a masked segment internally. */
        struct SSeqSeg // public to compile under Solaris
        {
            TSeqPos start_;     /**< Start of the segment. */
            TSeqPos stop_;      /**< One past the end of the segment. */

            /** Object constructor.
                @param start start of the new segment
                @param stop one past the end of the new segment
              */
            SSeqSeg( TSeqPos start, TSeqPos stop = 0 )
                : start_( start ), stop_( stop )
            {}
        };

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

        /** Increment used to increase seqstore capacity. */
        static const TSeqStore::size_type SS_INCR = 100*1024*1024;

        /** Threshold for the difference between seqstore size and capacity. */
        static const TSeqStore::size_type SS_THRESH = 10*1024*1024;

        /** Type for storing mapping from subject oids to the chunk numbers. */
        typedef std::vector< TSeqNum > TSubjects;

        /** A helper class used when creating internal set masked locations
            in the process of converting the sequence data to NCBI2NA and
            storing it in seq_store_.
        */
        class CMaskHelper : public CObject
        {
            private:

                typedef CSequenceIStream::TMask TMask; /**< forwarded type */

                /** See documentation for CSubjectMap_Factory_Base::TLocs. */
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
        TSeqStore::size_type ss_cap_;           /**< Current seq_store capacity. */
        TSubjects subjects_;                    /**< Mapping from subject oid to chunk information. */
        CRef< CMaskHelper > mask_helper_;       /**< Auxiliary object used to compute unmasked parts of the sequences. */
        unsigned long stride_;                  /**< Stride selected in index creation options. */
        unsigned long min_offset_;              /**< Minimum offset value used by the index. */

        /** Object constructor. 
            @param options index creation options
        */
        CSubjectMap_Factory_Base( 
                const TOptions & options ) 
            : chunk_size_( options.chunk_size ), 
              chunk_overlap_( options.chunk_overlap ),
              report_level_( options.report_level ),
              committed_( 0 ), last_chunk_( 0 ),
              om_( objects::CObjectManager::GetInstance() ),
              seq_store_( options.stride, 0 ),
              ss_cap_( SS_INCR ),
              mask_helper_( null ),
              stride_( options.stride ),
              min_offset_( GetMinOffset( options.stride ) )
        {}

        /** Helper function used to extract CSeqVector instance from 
            a TSeqData object.
            The extracted CSeqVector is stored in c_seq_ data member.
            @param sd the object containing the input sequence data
        */
        string extractSeqVector( TSeqData & sd );

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
        string NewSequenceInit( TSeqData & sd, TSeqNum start_chunk );
};

/** To be merged with CSubjectMap_Factory_Base
*/
class CSubjectMap_Factory_TBase : public CSubjectMap_Factory_Base
{
    public:

        /** Object constructor.
            @param options index creation options
        */
        CSubjectMap_Factory_TBase( 
                const TOptions & options ) 
            : CSubjectMap_Factory_Base( options )
        {}

        /** Get the total memory usage by the subject map in bytes.
            @return memory usage by this instance
        */
        TWord total() const { return seq_store_.size(); }

        /** Append the next chunk of the input sequence currently being
            processed to the subject map.

            This function only computes the valid segments and decides whether
            iteration over chunks is complete.

            The return value of false should be used as iteration termination
            condition.

            @param seq_off The start of the chunk data.
            @return true for success; false if no more chunks were available
        */
        bool AddSequenceChunk( TSeqStore::size_type seq_off );

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

    protected:

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
        */
        void Save( CNcbiOstream & os ) const;

        /** Revert to the state before the start of processing of the 
            current input sequence.
        */
        void RollBack();
};

/** To be merged with CSubjectMap_Factory_Base.
  */
class CSubjectMap_Factory : public CSubjectMap_Factory_TBase
{
    public: // This section is for Solaris compilation.

    /** Base class. */
    typedef CSubjectMap_Factory_TBase TBase;

    /** @name Aliases to the names from the base class. */
    /**@{*/
    typedef TBase::TSeqNum TSeqNum;
    typedef TBase::TSeqData TSeqData;
    /**@}*/

    private:

        /** Type of lengths table. */
        typedef vector< TWord > TLengthTable;

        /** Element of mapping of local sequence ids to chunks. */
        struct SLIdMapElement
        {
            TSeqNum start_;     /**< First chunk. */
            TSeqNum end_;       /**< One past the last chunk. */
            TSeqPos seq_start_; /**< Start of the combined sequence in seq_store. */
            TSeqPos seq_end_;   /**< End of the combined sequence in seq_store. */
        };

        /** Type of mapping of local sequence ids to chunks. */
        typedef vector< SLIdMapElement > TLIdMap;

    public:

        /** Object constructor.
            @param options index creation options
        */
        CSubjectMap_Factory( 
                const TOptions & options );

        /** Start processing of the new input sequence.

            In addition to base class functionality this function adds
            an entry to the lengths table.

            @param sd new input sequence data
            @param start_chunk only store data related to chunks numbered
                               higher than the value of this parameter
        */
        string NewSequenceInit( TSeqData & sd, TSeqNum start_chunk )
        {
            string result = TBase::NewSequenceInit( sd, start_chunk );
            lengths_.push_back( this->c_seq_.size() );
            return result;
        }

        /** Append the next chunk of the input sequence currently being
            processed to the subject map.
            The return value of false should be used as iteration termination
            condition.
            @param overflow [O] returns true if lid overflow occured
            @return true for success; false if no more chunks were available
        */
        bool AddSequenceChunk( bool & overflow );

        /** Check if index information should be produced for this offset.
            
            Typically it computes the full offset in way typical for the
            corresponding version of index and checks if it is a multiple
            of stride.

            @param seq Start of the buffer containing the compressed sequence.
            @param off Offset relative to the start of seq.
            @return true if information about this offset should be in the index;
                    false otherwise.
        */
        bool CheckOffset( const Uint1 * seq, TSeqPos off ) const;

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

        /** Save the subject map and sequence info.
            @param os output stream open in binary mode
        */
        void Save( CNcbiOstream & os ) const;

    private:

        TLengthTable lengths_;  /**< The table of subject sequence lengths. */
        TLIdMap lid_map_;       /**< Maping of local sequence ids to chunks. */
        TSeqPos cur_lid_len_;   /**< Current length of local sequence. */
        Uint1 offset_bits_;     /**< Number of bits used to encode offset. */
};

//-------------------------------------------------------------------------
void CSubjectMap_Factory_Base::CMaskHelper::Init()
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
void CSubjectMap_Factory_Base::CMaskHelper::Advance()
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
void CSubjectMap_Factory_Base::CMaskHelper::Adjust( TSeqPos pos )
{
    bool notdone;

    do{
        notdone = Retreat();
    }while( notdone && pos < stop_ );
}

//-------------------------------------------------------------------------
bool CSubjectMap_Factory_Base::CMaskHelper::Retreat()
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
bool CSubjectMap_Factory_Base::CMaskHelper::In( TSeqPos pos )
{
    while( Good() && pos >= stop_ ) Advance();
    if( !Good() ) return false;
    return pos >= start_;
}

//-------------------------------------------------------------------------
string CSubjectMap_Factory_Base::extractSeqVector( TSeqData & sd )
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
    string idstr = objects::sequence::GetTitle( bsh );
    Uint4 pos = idstr.find_first_of( " \t" );
    idstr = idstr.substr( 0, pos );
    return idstr;
}

//-------------------------------------------------------------------------
string CSubjectMap_Factory_Base::NewSequenceInit(
        TSeqData & sd, TSeqNum start_chunk )
{
    string result = "unknown";
    subjects_.push_back( 0 );
    c_chunk_ = start_chunk;

    if( sd ) {
        result = extractSeqVector( sd );
        TMask & mask = sd.mask_locs_;
        mask_helper_.Reset( new CMaskHelper );

        for( TMask::const_iterator mask_it = mask.begin();
                mask_it != mask.end(); ++mask_it ) {
            mask_helper_->Add( *mask_it );
        }

        mask_helper_->Init();
    }

    return result;
}

//-------------------------------------------------------------------------
void CSubjectMap_Factory_TBase::Save( CNcbiOstream & os ) const
{
    TWord tmp = subjects_.size();
    TWord subject_map_size = 
        tmp*sizeof( TWord ) +
        chunks_.size()*sizeof( TWord );
    WriteWord( os, subject_map_size );

    for( TSubjects::const_iterator cit = subjects_.begin();
            cit != subjects_.end(); ++cit ) {
        WriteWord( os, (TWord)(*cit) );
    }

    for( TChunks::const_iterator cit = chunks_.begin();
            cit != chunks_.end(); ++cit ) {
        WriteWord( os, cit->seq_start_ );
    }

    WriteWord( os, (TWord)(seq_store_.size()) );
    WriteWord( os, (TWord)(seq_store_.size()) );
    os.write( (char *)(&seq_store_[0]), seq_store_.size() );
    os << std::flush;
}

//-------------------------------------------------------------------------
bool CSubjectMap_Factory_TBase::AddSequenceChunk( 
        TSeqStore::size_type seq_off )
{
    TSeqPos chunk_start = (chunk_size_ - chunk_overlap_)*(c_chunk_++);

    if( chunk_start >= c_seq_.size() ) {
        --c_chunk_;
        return false;
    }

    TSeqPos chunk_end = 
        std::min( (TSeqPos)(chunk_start + chunk_size_), c_seq_.size() );
    TSeqPos chunk_len = chunk_end - chunk_start;
    SSeqInfo::TSegs segs;

    if( chunk_len > 0 ) {
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
    }

    chunks_.push_back( 
            TSeqInfo( seq_off, c_seq_.size(), segs ) );
    
    if( *subjects_.rbegin() == 0 ) {
        *subjects_.rbegin() = chunks_.size();
    }

    last_chunk_ = chunks_.size();
    return true;
}

//-------------------------------------------------------------------------
void CSubjectMap_Factory_TBase::RollBack()
{
    if( !subjects_.empty() ) {
        last_chunk_ = *subjects_.rbegin() - 1;
        c_chunk_ = 0;
        *subjects_.rbegin() = 0;
    }
}

//-------------------------------------------------------------------------
void CSubjectMap_Factory_TBase::Commit()
{
    if( last_chunk_ < chunks_.size() ) {
        TSeqStore::size_type newsize = 
            (TSeqStore::size_type)(chunks_[last_chunk_].seq_start_);
        seq_store_.resize( newsize );
        chunks_.resize( last_chunk_ );
    }

    committed_ = last_chunk_;
}

//-------------------------------------------------------------------------
CSubjectMap_Factory::CSubjectMap_Factory( 
        const TOptions & options ) 
    : TBase( options ),
      cur_lid_len_( 0 ), offset_bits_( 16 )
{
    unsigned long max_len = (1 + options.chunk_size/stride_) + min_offset_;
    while( (max_len>>offset_bits_) != 0 ) ++offset_bits_;
}

//-------------------------------------------------------------------------
bool CSubjectMap_Factory::AddSequenceChunk( bool & overflow )
{
    overflow = false;
    bool starting = (this->c_chunk_ == 0);
    TSeqPos chunk_start = 
        (this->chunk_size_ - this->chunk_overlap_)*this->c_chunk_;
    TBase::TSeqStore::size_type seq_off = 
        starting ? this->seq_store_.size() :
                   this->chunks_.rbegin()->seq_start_
                   + (this->chunk_size_ - this->chunk_overlap_)/CR;
    if( !TBase::AddSequenceChunk( seq_off ) ) return false;
    TBase::TSeq::size_type seqlen = this->c_seq_.size();
    
    // Combining sequences.
    //
    TSeqPos length_limit = (1<<(offset_bits_ - 1));
    TSeqPos chunk_end = std::min( 
            (TSeqPos)(chunk_start + this->chunk_size_), seqlen );
    TSeqPos chunk_len = chunk_end - chunk_start;
    
    if( lid_map_.empty() || cur_lid_len_ + chunk_len > length_limit ) {
        Uint1 lid_bits = 8*sizeof( TWord ) - offset_bits_;
        TSeqNum lid_limit = (1UL<<lid_bits);

        if( lid_map_.size() >= lid_limit ) {
            overflow = true;
            return true;
        }

        SLIdMapElement newlid = { this->chunks_.size() - 1, 0, seq_off };
        lid_map_.push_back( newlid );
        cur_lid_len_ = 0;
    }

    lid_map_.rbegin()->end_ = this->chunks_.size();
    cur_lid_len_ += chunk_len;
    lid_map_.rbegin()->seq_end_ = 
        lid_map_.rbegin()->seq_start_ + cur_lid_len_;
    
    if( starting && seqlen > 0 ) {
        if( this->ss_cap_ <= this->seq_store_.size() + TBase::SS_THRESH ) {
            this->ss_cap_ += TBase::SS_INCR; 
            this->seq_store_.reserve( this->ss_cap_ );
        }
        Uint1 accum = 0;
        unsigned int lc = 0;

        for( TSeqPos pos = 0; pos < seqlen; ++pos, lc = (lc + 1)%CR ) {
            Uint1 letter = base_value( this->c_seq_[pos] );
            if( letter != 0 ) --letter;
            accum = (accum << 2) + letter;
            if( lc == 3 ) this->seq_store_.push_back( accum );
        }

        if( lc != 0 ) {
            accum <<= (CR - lc)*2;
            this->seq_store_.push_back( accum );
        }
    }

    return true;
}

//-------------------------------------------------------------------------
inline bool CSubjectMap_Factory::CheckOffset( 
        const Uint1 * seq, TSeqPos off ) const
{
    TSeqPos soff = seq - &(this->seq_store_[0]);
    TLIdMap::const_reverse_iterator iter = lid_map_.rbegin();
    while( iter != lid_map_.rend() && iter->seq_start_ > soff ) ++iter;
    ASSERT( iter->seq_start_ <= soff );
    off += (soff - iter->seq_start_)*CR;
    return (off%stride_ == 0);
}

//-------------------------------------------------------------------------
inline TWord CSubjectMap_Factory::MakeOffset(
        const Uint1 * seq, TSeqPos off ) const
{
    TSeqPos soff = seq - &(this->seq_store_[0]);
    TLIdMap::const_reverse_iterator iter = lid_map_.rbegin();
    while( iter != lid_map_.rend() && iter->seq_start_ > soff ) ++iter;
    ASSERT( iter->seq_start_ <= soff );
    off += (soff - iter->seq_start_)*CR;
    off /= stride_;
    off += min_offset_;
    TWord result = ((lid_map_.rend() - iter - 1)<<offset_bits_) + off;
    return result;
}

//-------------------------------------------------------------------------
inline TWord CSubjectMap_Factory::MakeOffset(
        TSeqNum seqnum, TSeqPos off ) const
{
    const Uint1 * seq = 
        &(this->seq_store_)[0] + (this->chunks_)[seqnum].seq_start_;
    return MakeOffset( seq, off );
}

//-------------------------------------------------------------------------
void CSubjectMap_Factory::Save( CNcbiOstream & os ) const
{
    TWord sz = sizeof( TWord )*lengths_.size();
    WriteWord( os, sz );
    WriteWord( os, (TWord)offset_bits_ );

    for( TLengthTable::const_iterator it = lengths_.begin();
            it != lengths_.end(); ++it ) {
        WriteWord( os, (TWord)(*it) );
    }

    sz = 4*sizeof( TWord )*lid_map_.size();
    WriteWord( os, sz );

    for( TLIdMap::const_iterator it = lid_map_.begin();
            it != lid_map_.end(); ++it ) {
        WriteWord( os, (TWord)(it->start_) );
        WriteWord( os, (TWord)(it->end_) );
        WriteWord( os, (TWord)(it->seq_start_) );
        WriteWord( os, (TWord)(it->seq_end_) );
    }

    TBase::Save( os );
}

//-------------------------------------------------------------------------
/** Type representing an offset list corresponding to an Nmer. 
    See documentation of COffsetData_Factory classes for the description 
    of template parameters.
*/
class COffsetList
{
    public:

        /** Set the index creation parameters. 
            
            @param options index creation options
         */
        void SetIndexParams( const TOptions & options )
        { 
            min_offset_ = GetMinOffset( options.stride );
            mult_ = (options.ws_hint - options.hkey_width + 1)/options.stride;
        }

        /** Add an offset to the list. Update the total.
            @param item  [I]   offset to be appended to the list
            @param total [I/O] change in the length of the list will
                               be applied to this argument
        */
        void AddData( TWord item, TWord & total );

        /** Truncate the list to the value of offset. Update the total.
            The function removes the tail of the list corresponding
            to elements that are at least as great as offset.
            @param offset [I]   offset value threshold
            @param total  [I/O] change in the length of the list will
                                be applied to this argument
        */
        void TruncateList( TWord offset, TWord & total );

        /** Return the size of the offset list in words.
            @return size of the list in words
        */
        TWord Size() const { return (TWord)(data_.size()); }

        /** Save the offset list.
            @param os output stream open in binary mode
        */
        void Save( CNcbiOstream & os ) const;

    public: // for Solaris

        struct SDataUnit;

        static const Uint4 DATA_UNIT_SIZE = 1 + 10*sizeof( SDataUnit * )/sizeof( TWord );

        struct SDataUnit
        {
            TWord data[DATA_UNIT_SIZE];
            SDataUnit * next;
        };

        class CDataPool
        {
                static const Uint4 BLOCK_SIZE     = 1024*1024ULL;
                static const Uint4 BLOCKS_RESERVE = 10*1024ULL;

                typedef vector< SDataUnit > TBlock;
                typedef vector< TBlock > TBlocks;

            public:

                CDataPool() : free_( 0 )
                {
                    pool_.reserve( BLOCKS_RESERVE );
                    new_block();
                }

                SDataUnit * alloc()
                {
                    if( free_ != 0 ) {
                        SDataUnit * result = free_;
                        free_ = free_->next;
                        return result;
                    }

                    if( first_unused_ >= BLOCK_SIZE ) new_block();
                    return &(*pool_.rbegin())[first_unused_++];
                }

                void free( SDataUnit * d )
                {
                    if( d == 0 ) return;
                    SDataUnit * t = free_;
                    free_ = d;
                    while( d->next != 0 ) d = d->next;
                    d->next = t;
                }

                void clear()
                {
                    free_ = 0;
                    pool_.resize( 1 );
                    first_unused_ = 0;
                }

            private:

                void new_block()
                {
                    pool_.push_back( TBlock( BLOCK_SIZE ) );
                    first_unused_ = 0;
                }

                SDataUnit * free_;

                Uint4 first_unused_;

                TBlocks pool_;
        };

    private:

        class CData
        {
                class CDataIterator
                {
                    public:

                        CDataIterator( 
                                SDataUnit * cunit, 
                                Uint4 cindex, 
                                Uint4 size )
                            : cunit_( cunit ), cindex_( cindex ), 
                              size_( size ), prev_( 0 )
                        { ASSERT( cindex_ != 0 ); }

                        CDataIterator & operator++()
                        {
                            if( size_ != 0 ) {
                                if( cindex_ >= DATA_UNIT_SIZE ) {
                                    prev_ = &cunit_->data[cindex_ - 1];
                                    cunit_ = cunit_->next;
                                    cindex_ = 1;
                                }
                                else ++cindex_;

                                --size_;

                                if( size_ == 0 ) {
                                    cunit_  = 0;
                                    cindex_ = 1;
                                    prev_   = 0;
                                }
                            }

                            return *this;
                        }
                        
                        CDataIterator & operator--()
                        {
                            if( size_ != 0 ) {
                                ASSERT( cindex_ != 0 );
                                --cindex_;
                                ++size_;
                            }

                            return *this;
                        }

                        TWord operator*() const 
                        { 
                            ASSERT( size_ != 0 );
                            ASSERT( cindex_ != 0 || prev_ != 0 );
                            ASSERT( cindex_ == 0 || cunit_ != 0 );
                            return ( cindex_ != 0 ) ? cunit_->data[cindex_ - 1] 
                                                    : *prev_;
                        }

                        friend bool operator==( 
                                const CDataIterator & rhs,
                                const CDataIterator & lhs )
                        { 
                            return rhs.cunit_ == lhs.cunit_ ? 
                                    rhs.cunit_ == 0 ?
                                        true :
                                        rhs.cindex_ == lhs.cindex_ :
                                    false;
                        }

                        friend bool operator!=( 
                                const CDataIterator & rhs,
                                const CDataIterator & lhs )
                        { return !(rhs == lhs); }

                    private:

                        SDataUnit * cunit_;
                        Uint4 cindex_;
                        Uint4 size_;
                        TWord * prev_;
                };

            public:

                typedef CDataIterator const_iterator;
                typedef Uint4 size_type;

                CData() : start_( 0 ), curr_( 0 ), last_( 0 ), size_( 0 )
                {}

                const_iterator begin() const
                { return const_iterator( start_, 1, size_ ); }

                const_iterator end() const
                { return const_iterator( 0, 1, 0 ); }

                Uint4 size() const { return size_; }
                bool empty() const { return (size() == 0); }

                void push_back( const TWord & d )
                {
                    if( start_ == 0 ) {
                        start_ = curr_ = Pool_.alloc();
                        start_->next = 0;
                    }
                    
                    curr_->data[last_++] = d;

                    if( last_ >= DATA_UNIT_SIZE ) {
                        SDataUnit * t = Pool_.alloc();
                        t->next = 0;
                        curr_->next = t;
                        curr_ = t;
                        last_ = 0;
                    }

                    ++size_;
                }

                void resize( Uint4 newsize )
                {
                    if( newsize == 0 ) {
                        Pool_.free( start_ );
                        start_ = curr_ = 0;
                        size_ = last_ = 0;
                        return;
                    }

                    while( newsize > size() ) push_back( 0 );
                    Uint4 t = 0;
                    SDataUnit * tp = 0, * tn = start_;

                    while( t < newsize ) {
                        t += DATA_UNIT_SIZE;
                        tp = tn;
                        tn = tp->next;
                    }

                    Pool_.free( tn );
                    curr_ = tp;
                    last_ = DATA_UNIT_SIZE - (t - newsize) - 1;
                    size_ = newsize;
                }

                static void Clear() { Pool_.clear(); }

            private:

                static CDataPool Pool_;

                SDataUnit * start_;
                SDataUnit * curr_;
                Uint4 last_;
                Uint4 size_;
        };

        /** Type used to store offset list data. */
        typedef CData TData;

        TData data_;               /**< Offset list data storage. */
        unsigned long min_offset_; /**< Minimum offset used by the index. */
        unsigned long mult_;       /**< Max multiple to use in list pre-ordering. */

    public:

        static void ClearAll() { TData::Clear(); }
};

COffsetList::CDataPool COffsetList::CData::Pool_;

//-------------------------------------------------------------------------
inline void COffsetList::Save( CNcbiOstream & os) const
{
    for( TData::const_iterator cit = data_.begin();
            cit != data_.end(); ++cit )
        if( *cit < min_offset_ ) {
            WriteWord( os, *cit );
            WriteWord( os, *(++cit) );
        }
        else if( (*cit)%mult_ == 0 ) WriteWord( os, *cit );

    unsigned long m = mult_;

    while( --m > 0 ) {
        for( TData::const_iterator cit = data_.begin();
                cit != data_.end(); ++cit ) {
            if( *cit < min_offset_ ) ++cit;
            else {
                bool skip = false;

                for( unsigned long n = mult_; n > m; --n )
                    if( (*cit)%n == 0 ) { skip = true; break; }

                if( !skip && (*cit)%m == 0 ) WriteWord( os, *cit );
            }
        }
    }

    if( !data_.empty() ) {
        WriteWord( os, (TWord)0 );
    }
}

//-------------------------------------------------------------------------
inline void COffsetList::AddData( TWord item, TWord & total )
{
    data_.push_back( item );
    ++total;
}

//-------------------------------------------------------------------------
inline void COffsetList::TruncateList( TWord offset, TWord & total )
{
    bool flag = false;
    TData::const_iterator it = data_.begin();

    for( TData::size_type i = 0; i < data_.size(); ++i, ++it ) {
        if( *it < min_offset_ ) {
            flag = true;
            continue;
        }

        if( *it >= offset ) {
            if( flag ) {
                --i; --it;
            }

            TData::size_type diff = data_.size() - i;
            data_.resize( i );
            total -= diff;
            return;
        }else {
            flag = false;
        }
    }
}

//-------------------------------------------------------------------------
/** A class responsible for creation and management of Nmer
    offset lists.
*/
class COffsetData_Factory 
{
    public:

        typedef CSubjectMap_Factory TSubjectMap;    /**< Rename for consistency. */

        /** Object constructor.
            @param subject_map structure to use to map logical oids to the
                               actual sequence data
            @param options index creation options
        */
        COffsetData_Factory( 
                TSubjectMap & subject_map, 
                const CDbIndex::SOptions & options )
            : subject_map_( subject_map ),
              hash_table_( 1<<(2*options.hkey_width) ),
              report_level_( options.report_level ),
              total_( 0 ),
              hkey_width_( options.hkey_width ),
              last_seq_( 0 ),
              options_( options ),
              code_bits_( GetCodeBits( options.stride ) )
        {
            for( THashTable::iterator i = hash_table_.begin();
                    i != hash_table_.end(); ++i ) {
                i->SetIndexParams( options_ );
            }
        }

        ~COffsetData_Factory() { COffsetList::ClearAll(); }

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
        */
        void Save( CNcbiOstream & os );

    private:

        /** Type used for individual offset lists. */
        typedef COffsetList TOffsetList;

        typedef CDbIndex::TSeqNum TSeqNum;             /**< Forwarding from CDbIndex. */
        typedef TSubjectMap::TSeqInfo TSeqInfo;        /**< Forwarding from TSubjectMap. */

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
        unsigned long code_bits_;            /**< Number of bits to encode special offset prefixes. */
};

//-------------------------------------------------------------------------
void COffsetData_Factory::Save( CNcbiOstream & os ) 
{
    ++this->total_;

    for( THashTable::const_iterator cit = hash_table_.begin();
            cit != hash_table_.end(); ++cit ) {
        if( cit->Size() > 0 ) ++this->total_;
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

    for( THashTable::const_iterator cit = hash_table_.begin();
            cit != hash_table_.end(); ++cit, ++nmer ) {
        if( cit->Size() != 0 ) {
            ++tot;
        }

        if( cit->Size() != 0 ) 
            WriteWord( os, tot );
        else WriteWord( os, (TWord)0 );

        tot += cit->Size();

        if( stat && cit->Size() > 0 ) {
            *stats << hex << setw( 10 ) << nmer 
                   << " " << dec << cit->Size() << endl;
        }
    }

    WriteWord( os, total() );
    WriteWord( os, (TWord)0 );

    for( THashTable::const_iterator cit = hash_table_.begin();
            cit != hash_table_.end(); ++cit ) {
        cit->Save( os );
    }

    os << std::flush;
}

//-------------------------------------------------------------------------
void COffsetData_Factory::EncodeAndAddOffset(
        TWord nmer, TSeqPos start, TSeqPos stop, 
        TSeqPos curr, TWord offset )
{
    TSeqPos start_diff = curr + 2 - hkey_width_ - start;
    TSeqPos end_diff = stop - curr;

    if( start_diff <= options_.stride || end_diff <= options_.stride ) {
        if( start_diff > options_.stride ) start_diff = 0;
        if( end_diff > options_.stride ) end_diff = 0;
        TWord code = (start_diff<<code_bits_) + end_diff;
        hash_table_[(THashTable::size_type)nmer].AddData( 
                code, total_ );
    }

    hash_table_[(THashTable::size_type)nmer].AddData( 
            offset, total_ );
}

//-------------------------------------------------------------------------
void COffsetData_Factory::AddSeqSeg(
        const Uint1 * seq, TWord , TSeqPos start, TSeqPos stop )
{
    const TWord nmer_mask = (((TWord)1)<<(2*hkey_width_)) - 1;
    const Uint1 letter_mask = 0x3;
    TWord nmer = 0;
    unsigned long count = 0;

    for( TSeqPos curr = start; curr < stop; ++curr, ++count ) {
        Uint1 unit = seq[curr/CR];
        Uint1 letter = ((unit>>(6 - 2*(curr%CR)))&letter_mask);
        nmer = ((nmer<<2)&nmer_mask) + letter;

        if( count >= hkey_width_ - 1 ) {
            if( subject_map_.CheckOffset( seq, curr ) ) {
                TWord offset = subject_map_.MakeOffset( seq, curr );
                EncodeAndAddOffset( nmer, start, stop, curr, offset );
            }
        }
    }
}

//-------------------------------------------------------------------------
void COffsetData_Factory::AddSeqInfo( const TSeqInfo & sinfo )
{
    for( TSeqInfo::TSegs::const_iterator it = sinfo.segs_.begin();
            it != sinfo.segs_.end(); ++it ) {
        AddSeqSeg( 
                subject_map_.seq_store_start() + sinfo.seq_start_, 
                sinfo.len_, it->start_, it->stop_ );
    }
}

//-------------------------------------------------------------------------
void COffsetData_Factory::Truncate()
{
    last_seq_ = subject_map_.LastGoodSequence();
    TWord offset = subject_map_.MakeOffset( last_seq_, 0 );

    for( THashTable::iterator it = hash_table_.begin();
            it != hash_table_.end(); ++it ) {
        it->TruncateList( offset, total_ );
    }
}

//-------------------------------------------------------------------------
void COffsetData_Factory::Update()
{
    if( subject_map_.LastGoodSequence() < last_seq_ ) {
        Truncate();
    }

    const TSeqInfo * sinfo;

    while( (sinfo = subject_map_.GetSeqInfo( last_seq_ + 1 )) != 0 ) {
        AddSeqInfo( *sinfo );
        ++last_seq_;
    }
}

//-------------------------------------------------------------------------
/** Index factory implementation.
  */
class CDbIndex_Factory : public CDbIndex
{
    private:

        static const Uint8 MEGABYTE = 1024*1024ULL;        /**< Obvious... */

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
            @param options     index creation options
            @param start       oid of the first sequence in the index
            @param start_chunk chunk number of the first chunk of the first sequence
            @param stop        oid of the last sequence in the index
            @param stop_chunk  chunk number of the last chunk of the last sequence
        */
        static void SaveHeader(
                CNcbiOstream & os,
                const SOptions & options,
                TSeqNum start,
                TSeqNum start_chunk,
                TSeqNum stop,
                TSeqNum stop_chunk );

        /** Called by CDbIndex::Create() (should be merged?).
        */
        static void do_create(
                CSequenceIStream & input, const std::string & oname,
                TSeqNum start, TSeqNum start_chunk,
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options
        );

        /** Another forward from do_create() (should be merged?).
        */
        static void do_create_1_2(
                CSequenceIStream & input, const std::string & oname,
                TSeqNum start, TSeqNum start_chunk,
                TSeqNum & stop, TSeqNum & stop_chunk,
                const SOptions & options
        );
};

//-------------------------------------------------------------------------
void CDbIndex_Factory::SaveHeader(
                CNcbiOstream & os,
                const SOptions & options,
                TSeqNum start,
                TSeqNum start_chunk,
                TSeqNum stop,
                TSeqNum stop_chunk )
{
    if( options.legacy ) {
        WriteWord( os, (unsigned char)VERSION );
        for( int i = 0; i < 7; ++i ) WriteWord( os, (unsigned char)0 );
        WriteWord( os, (Uint8)WIDTH_32 );
        WriteWord( os, (TWord)options.hkey_width );
        WriteWord( os, (TWord)OFFSET_COMBINED );
        WriteWord( os, (TWord)UNCOMPRESSED );
    }
    else {
        WriteWord( os, (unsigned char)(VERSION + 1) );
        for( int i = 0; i < 7; ++i ) WriteWord( os, (unsigned char)0 );
        WriteWord( os, (Uint8)WIDTH_32 );
        WriteWord( os, (TWord)options.hkey_width );
        WriteWord( os, (TWord)options.stride );
        WriteWord( os, (TWord)options.ws_hint );
    }

    WriteWord( os, (TWord)start );
    WriteWord( os, (TWord)start_chunk );
    WriteWord( os, (TWord)stop );
    WriteWord( os, (TWord)stop_chunk );
    os << std::flush;
}

//-------------------------------------------------------------------------
void CDbIndex_Factory::Create(
        CSequenceIStream & input, const std::string & oname,
        TSeqNum start, TSeqNum start_chunk,
        TSeqNum & stop, TSeqNum & stop_chunk, const SOptions & options )
{
    do_create( 
            input, oname, start, start_chunk, stop, stop_chunk, options );
}

//-------------------------------------------------------------------------
void CDbIndex_Factory::do_create(
        CSequenceIStream & input, const std::string & oname,
        TSeqNum start, TSeqNum start_chunk,
        TSeqNum & stop, TSeqNum & stop_chunk, const SOptions & options )
{
    do_create_1_2( 
            input, oname, start, start_chunk, stop, stop_chunk, options );
}

//-------------------------------------------------------------------------
void CDbIndex_Factory::do_create_1_2(
        CSequenceIStream & input, const std::string & oname,
        TSeqNum start, TSeqNum start_chunk,
        TSeqNum & stop, TSeqNum & stop_chunk, const SOptions & options )
{
    typedef CSubjectMap_Factory TSubjectMap;
    typedef COffsetData_Factory TOffsetData;

    TSubjectMap subject_map( options );
    TOffsetData offset_data( subject_map, options );

    TSeqNum i = start;

    if( i >= stop ) {
        stop = start;
        return;
    }

    vector< string > idmap;

    while( i < stop ) {
        typedef CSequenceIStream::TSeqData TSeqData;

        CRef< TSeqData > seq_data( input.next() );
        TSeqData * sd = seq_data.GetNonNullPointer();
        string idstr = subject_map.NewSequenceInit( *sd, start_chunk );
        idmap.push_back( idstr );

        if( !*sd ) {
            if( i == start ) {
                stop = start;
                return;
            }

            stop = i;
            stop_chunk = 0;
            break;
        }

        bool overflow;

        while( subject_map.AddSequenceChunk( overflow ) ) {
            if( !overflow ) {
                offset_data.Update();
            }
            else {
                std::cerr << "WARNING: logical sequence id overflow. "
                          << "Starting new volume." << std::endl;
            }

            Uint8 total = (Uint8)subject_map.total() + 
                ((Uint8)sizeof( TWord ))*offset_data.total();

            if( total > MEGABYTE*options.max_index_size || overflow ) {
                input.putback();
                subject_map.RollBack();
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

    {
        std::ostringstream os;
        os << "Last processed: sequence " 
           << start + subject_map.GetLastSequence() - 1
           << " ; chunk " << subject_map.GetLastSequenceChunk() 
           << std::endl;
    }

    {
        std::ostringstream os;
        os << "Index size: " 
           << subject_map.total() + sizeof( TWord )*offset_data.total() 
           << " bytes (not counting the hash table)." << std::endl;
    }

    CNcbiOfstream os( oname.c_str(), IOS_BASE::binary );
    SaveHeader( os, options, start, start_chunk, stop, stop_chunk );
    offset_data.Save( os );
    subject_map.Save( os );
    
    if( options.idmap ) {
        string mapname = oname + ".map";
        CNcbiOfstream maps( mapname.c_str() );
        
        for( vector< string >::const_iterator i = idmap.begin();
                i != idmap.end(); ++i ) {
            maps << *i << "\n";
        }

        maps << flush;
    }
}

//-------------------------------------------------------------------------
void CDbIndex::MakeIndex(
    const std::string & fname, const std::string & oname, 
    TSeqNum start, TSeqNum start_chunk, 
    TSeqNum & stop, TSeqNum & stop_chunk, const SOptions & options )
{
    // Make an CSequenceIStream out of fname and forward to
    // MakeIndex( CSequenceIStream &, ... ).
    CSequenceIStreamFasta input( fname ); 
    MakeIndex( 
            input, oname, start, start_chunk, 
            stop, stop_chunk, options );
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
    typedef CDbIndex_Factory TIndex_Impl;
    TIndex_Impl::Create( 
            input, oname, start, start_chunk, stop, stop_chunk, options );
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

