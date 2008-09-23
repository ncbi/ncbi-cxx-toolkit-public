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
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_hits.h>

#ifdef LOCAL_SVN
#include "dbindex.hpp"
#else
#include <algo/blast/dbindex/dbindex.hpp>
#endif

#include <algo/blast/dbindex/dbindex_sp.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blastdbindex )

// Comment this out for production.
// #define SEEDDEBUG 1
// #define PRINTSUBJMAP 1

/** Forwarding declarations for convenience. */
typedef CDbIndex::TSeqNum TSeqNum;
typedef CDbIndex::TWord TWord;

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
/** Type used to iterate over the consecutive Nmer values of the query
    sequence.
*/
class CNmerIterator
{
    public:

        /** Object constructor.
            @param hkey_width   [I]     nmer width
            @param query        [I]     query sequence in BLASTNA encoding
            @param start        [I]     start position in the query
            @param stop         [I]     one past the last position in the query
        */
        CNmerIterator( 
                unsigned long hkey_width,
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
INLINE
CNmerIterator::CNmerIterator(
        unsigned long hkey_width, const Uint1 * query, 
        TSeqPos start, TSeqPos stop )
    : query_( query ), state_( true ), 
      pos_( start ), stop_( stop ), nmer_( 0 ), count_( 0 ),
      hkey_width_( hkey_width )
{
    hkey_mask_ = (1<<(2*hkey_width_)) - 1;
    query_ += pos_;
}

//-------------------------------------------------------------------------
INLINE
bool CNmerIterator::Next()
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
template< unsigned long NHITS >
class CTrackedSeeds_Base
{
    protected:

        /**@name Some convenience type declaration. */
        /**@{*/
        typedef CSubjectMap TSubjectMap;
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
template< unsigned long NHITS >
INLINE
void CTrackedSeeds_Base< NHITS >::Reset()
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
template< unsigned long NHITS >
INLINE
void CTrackedSeeds_Base< NHITS >::SaveSeed( const TTrackedSeed & seed )
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
template< unsigned long NHITS >
INLINE
void CTrackedSeeds_Base< NHITS >::AppendSimple( const TTrackedSeed & seed )
{ seeds_.insert( it_, seed ); }

//-------------------------------------------------------------------------
template< unsigned long NHITS >
INLINE
void CTrackedSeeds_Base< NHITS >::Append( 
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
template< unsigned long NHITS >
class CTrackedSeeds;

//-------------------------------------------------------------------------
/** Specialization for one-hit searches. */
template<>
class CTrackedSeeds< ONE_HIT > : public CTrackedSeeds_Base< ONE_HIT >
{
    /** @name Types forwarded from the base class. */
    /**@{*/
    typedef CTrackedSeeds_Base< ONE_HIT > TBase;
    typedef TBase::TSubjectMap TSubjectMap;
    typedef TBase::TTrackedSeed TTrackedSeed;
    typedef TBase::TSeeds TSeeds;
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
INLINE
void CTrackedSeeds< ONE_HIT >::Finalize()
{
    for( TSeeds::const_iterator cit = this->seeds_.begin(); 
            cit != this->seeds_.end(); ++cit ) {
        SaveSeed( *cit );
    }
}

//-------------------------------------------------------------------------
INLINE
bool CTrackedSeeds< ONE_HIT >::EvalAndUpdate( const TTrackedSeed & seed )
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
template<>
class CTrackedSeeds< TWO_HIT > : public CTrackedSeeds_Base< TWO_HIT >
{
    /** @name Types forwarded from the base class. */
    /**@{*/
    typedef CTrackedSeeds_Base< TWO_HIT > TBase;
    typedef TBase::TSubjectMap TSubjectMap;
    typedef TBase::TTrackedSeed TTrackedSeed;
    typedef TBase::TSeeds TSeeds;
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
              word_size_( options.word_size ),
              stride_( subject_map.GetStride() )
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
        unsigned long stride_;     /**< Stride value used by the index. */
};


//-------------------------------------------------------------------------
INLINE
bool CTrackedSeeds< TWO_HIT >::CheckAndSaveSeed( 
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
INLINE
void CTrackedSeeds< TWO_HIT >::Finalize()
{
    for( TSeeds::const_iterator cit = this->seeds_.begin();
            cit != this->seeds_.end(); ++cit ) {
        CheckAndSaveSeed( *cit );
    }
}

//-------------------------------------------------------------------------
INLINE
bool CTrackedSeeds< TWO_HIT >::EvalAndUpdate( TTrackedSeed & seed )
{
    while( this->it_ != this->seeds_.end() ) {
        TSeqPos step = seed.qoff_ - this->it_->qoff_;
        TSeqPos it_soff_corr = this->it_->soff_ + step;
        if( it_soff_corr > seed.soff_ ) return true;

        if( this->it_->qright_ + seed.len_ + window_ + 3*stride_
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

//-------------------------------------------------------------------------
// Forward declaration.
//
template< bool LEGACY > class CDbIndex_Impl;

/** This is the object representing the state of a search over the index.
    Use of a separate class for searches allows for multiple simultaneous
    searches against the same index.
*/
template< bool LEGACY, unsigned long NHITS, typename derived_t >
class CSearch_Base
{
    protected:

        typedef CDbIndex::SSearchOptions TSearchOptions;    /**< Alias for convenience. */

    public:

        /** @name Aliases for convenience. */
        /**@{*/
        typedef CDbIndex_Impl< LEGACY > TIndex_Impl;
        typedef typename TIndex_Impl::TSubjectMap TSubjectMap;
        typedef CTrackedSeeds< NHITS > TTrackedSeeds;
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

        unsigned long code_bits_;  /**< Number of bits to represent special offset prefix. */
        unsigned long min_offset_; /**< Minumum offset used by the index. */
};

//-------------------------------------------------------------------------
template< bool LEGACY, unsigned long NHITS, typename derived_t >
CSearch_Base< LEGACY, NHITS, derived_t >::CSearch_Base(
        const TIndex_Impl & index_impl,
        const BLAST_SequenceBlk * query,
        const BlastSeqLoc * locs,
        const TSearchOptions & options )
    : index_impl_( index_impl ), query_( query ), locs_( locs ),
      options_( options ), subject_( 0 ), subj_end_off_( 0 ),
      roots_( index_impl_.NumSubjects() ),
      code_bits_( GetCodeBits( index_impl.GetSubjectMap().GetStride() ) ),
      min_offset_( GetMinOffset( index_impl.GetSubjectMap().GetStride() ) )
{
    seeds_.resize( 
            index_impl_.NumSubjects() - 1, 
            TTrackedSeeds( index_impl_.GetSubjectMap(), options ) );
    for( typename TTrackedSeedsSet::size_type i = 0; i < seeds_.size(); ++i ) {
        seeds_[i].SetLId( (TSeqNum)i );
    }
}

//-------------------------------------------------------------------------
template< bool LEGACY, unsigned long NHITS, typename derived_t >
INLINE
void CSearch_Base< LEGACY, NHITS, derived_t >::ExtendLeft( 
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
template< bool LEGACY, unsigned long NHITS, typename derived_t >
INLINE
void CSearch_Base< LEGACY, NHITS, derived_t >::ExtendRight( 
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
template< bool LEGACY, unsigned long NHITS, typename derived_t >
INLINE
void CSearch_Base< LEGACY, NHITS, derived_t >::ProcessBoundaryOffset( 
        TWord offset, TWord bounds )
{
    TSeqPos nmaxleft  = (TSeqPos)(bounds>>code_bits_);
    TSeqPos nmaxright = (TSeqPos)(bounds&((1<<code_bits_) - 1));
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
template< bool LEGACY, unsigned long NHITS, typename derived_t >
INLINE
void CSearch_Base< LEGACY, NHITS, derived_t >::ProcessOffset( TWord offset )
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
template< bool LEGACY, unsigned long NHITS, typename derived_t >
INLINE
unsigned long CSearch_Base< LEGACY, NHITS, derived_t >::ProcessRoot( 
        TTrackedSeeds & seeds, const SSeedRoot * root )
{
    if( qoff_ != root->qoff_ ) {
        seeds.Reset();
        qoff_ = root->qoff_;
    }else if( root->soff_ >= min_offset_ && 
                root->soff_ < soff_ ) {
        seeds.Reset();
    }

    qstart_ = root->qstart_;
    qstop_  = root->qstop_;

    if( root->soff_ < min_offset_ ) {
        TSeqPos boundary = (root++)->soff_;
        ProcessBoundaryOffset( root->soff_ - min_offset_, boundary );
                // root->soff_ - CDbIndex::MIN_OFFSET, boundary );
        soff_ = root->soff_;
        return 2;
    }else {
        ProcessOffset( root->soff_ - min_offset_ );
        soff_ = root->soff_;
        return 1;
    }
}

//-------------------------------------------------------------------------
template< bool LEGACY, unsigned long NHITS, typename derived_t >
INLINE
void CSearch_Base< LEGACY, NHITS, derived_t >::ComputeSeeds()
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
template< bool LEGACY, unsigned long NHITS, typename derived_t >
INLINE
void CSearch_Base< LEGACY, NHITS, derived_t >::SearchInt()
{
    CNmerIterator nmer_it( 
            index_impl_.hkey_width(), query_->sequence, qstart_, qstop_ );

    while( nmer_it.Next() ) {
        typename TIndex_Impl::TOffsetIterator off_it( 
                index_impl_.OffsetIterator( 
                    nmer_it.Nmer(), options_.word_size ) );
        qoff_ = nmer_it.Pos();

        while( off_it.More() ) {
            subject_ = 0;
            subj_end_off_ = 0;

            while( off_it.Next() ) {
                TWord offset = off_it.Offset();
                TDerived * self = static_cast< TDerived * >( this );

                if( offset < min_offset_ ) {
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
template< bool LEGACY, unsigned long NHITS, typename derived_t >
CConstRef< CDbIndex::CSearchResults > 
CSearch_Base< LEGACY, NHITS, derived_t >::operator()()
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
/** CSearch CRTP (to be removed). */
template< bool LEGACY, unsigned long NHITS >
class CSearch;

//-------------------------------------------------------------------------
template< bool LEGACY, unsigned long NHITS >
class CSearch
    : public CSearch_Base< LEGACY, NHITS, CSearch< LEGACY, NHITS > >
{
    /** @name Convenience declarations. */
    /**@{*/
    typedef CSearch_Base< LEGACY, NHITS, CSearch > TBase;
    typedef typename TBase::TIndex_Impl TIndex_Impl;
    typedef typename TBase::TSearchOptions TSearchOptions;
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
CConstRef< CDbIndex::CSearchResults > CDbIndex::Search( 
        const BLAST_SequenceBlk * query, const BlastSeqLoc * locs, 
        const SSearchOptions & search_options )
{
    if( search_options.two_hits == 0 )
        if( header_.legacy_ ) {
            CSearch< true, ONE_HIT > searcher(
                    dynamic_cast< CDbIndex_Impl< true > & >(*this), query, locs, search_options );
            return searcher();
        }
        else {
            CSearch< false, ONE_HIT > searcher(
                    dynamic_cast< CDbIndex_Impl< false > & >(*this), query, locs, search_options );
            return searcher();
        }
    else
        if( header_.legacy_ ) {
            CSearch< true, TWO_HIT > searcher(
                    dynamic_cast< CDbIndex_Impl< true > & >(*this), query, locs, search_options );
            return searcher();
        }
        else {
            CSearch< false, TWO_HIT > searcher(
                    dynamic_cast< CDbIndex_Impl< false > & >(*this), query, locs, search_options );
            return searcher();
        }
}

END_SCOPE( blastdbindex )
END_NCBI_SCOPE

