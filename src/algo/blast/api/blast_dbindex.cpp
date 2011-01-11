/* $Id$
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
* Author: Aleksandr Morgulis
*
*/

/// @file blast_dbindex.cpp
/// Functionality for indexed databases

#include <ncbi_pch.hpp>
#include <sstream>
#include <list>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_util.h>

#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>

#include <algo/blast/api/blast_dbindex.hpp>
#include <algo/blast/dbindex/dbindex.hpp>

#include "algo/blast/core/mb_indexed_lookup.h"

// Comment this out to continue with extensions.
// #define STOP_AFTER_PRESEARCH 1

/** @addtogroup AlgoBlast
 *
 * @{
 */

extern "C" {

/** Construct a new instance of index based subject sequence source.

    @param retval Preallocated instance of BlastSeqSrc structure.
    @param args Arguments for the constructor.
    @return \e retval with filled in fields.
*/
static BlastSeqSrc * s_IDbSrcNew( BlastSeqSrc * retval, void * args );

/** Construct a copy of BlastSeqSrc structure.

    @param retval Preallocated instance of BlastSeqSrc structure.
    @param args Arguments for the constructor.
    @return \e retval with filled in fields.
*/
static BlastSeqSrc * s_CloneSrcNew( BlastSeqSrc * retval, void * args );

/** Get the seed search results for a give subject id and chunk number.

    @param idb_v          [I]   Database and index data.
    @param oid_i          [I]   Subject id.
    @param chunk_i        [I]   Chunk number.
    @param init_hitlist [I/O] Results are returned here.

    @return Word size used for search.
*/
static unsigned long s_MB_IdbGetResults(
        void * idb_v,
        Int4 oid_i, Int4 chunk_i,
        BlastInitHitList * init_hitlist );
}

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blast )

USING_SCOPE( ncbi::objects );
USING_SCOPE( ncbi::blastdbindex );

/// Get the minimum acceptable word size to use with indexed search.
/// @return the minimum acceptable word size
int MinIndexWordSize() { return 16; }

/** No-op presearch function. Used when index search is not enabled.
    @sa DbIndexPreSearchFnType()
*/
static void NullPreSearch( 
        BlastSeqSrc *, LookupTableWrap *, 
        BLAST_SequenceBlk *, BlastSeqLoc *,
        LookupTableOptions *, BlastInitialWordOptions * ) {}

/** No-op callback for setting query info. Used when index search is not enabled.
    @sa DbIndexSetQueryInfoFnType()
*/
static void NullSetQueryInfo(
        BlastSeqSrc * , 
        LookupTableWrap * , 
        CRef< CBlastSeqLocWrap > ) {}

/** No-op callback to run indexed search. Used when index search is not enabled.
    @sa DbIndexRunSearchFnType()
*/
static void NullRunSearch(
        BlastSeqSrc * , BLAST_SequenceBlk * , 
        LookupTableOptions * , BlastInitialWordOptions * ) {}

/** No-op callback to set the number of threads for indexed search. 
    Used when index search is not enabled.
    @sa DbIndexSetNumThreadsFnType()
*/
static void NullSetNumThreads( BlastSeqSrc * seq_src, size_t ) {}

/** Global pointer to the appropriate pre-search function, based
    on whether or not index search is enabled.
*/
static DbIndexPreSearchFnType PreSearchFn = &NullPreSearch;

/** Global pointer to the appropriate callback to set query info, based
    on whether or not index search is enabled.
*/
static DbIndexSetQueryInfoFnType SetQueryInfoFn = &NullSetQueryInfo;

/** Global pointer to the appropriate callback to run indexed search, based
    on whether or not index search is enabled.
*/
static DbIndexRunSearchFnType RunSearchFn = &NullRunSearch;

/** Global pointer to the appropriate to set number of threads for indexed search, 
    based on whether or not index search is enabled.
*/
static DbIndexSetNumThreadsFnType SetNumThreadsFn = &NullSetNumThreads;
    
//------------------------------------------------------------------------------
/** This structure is used to transfer arguments to s_IDbSrcNew(). */
struct SIndexedDbNewArgs
{
    string indexname;   /**< Name of the index data file. */
    BlastSeqSrc * db;   /**< BLAST database sequence source instance. */
};

//------------------------------------------------------------------------------
/** This class is responsible for loading indices and doing the actual
    seed search.

    It acts as a middle man between the blast engine and dbindex library.
*/
class CIndexedDb : public CObject
{
    private:

        /** Type used to represent collections of search result sets. */
        typedef vector< CConstRef< CDbIndex::CSearchResults > > TResultSet;

        /** Type used to map loaded indices to subject ids. */
        typedef vector< CDbIndex::TSeqNum > TSeqMap;

        /** Type of the collection of currently loaded indices. */
        typedef vector< CRef< CDbIndex > > TIndexSet;

        /** Data local to a running thread.
        */
        struct SThreadLocal 
        {
            CRef< CIndexedDb > idb_; /**< Pointer to the shared index data. */
        };

        /** Find an index corresponding to the given subject id.

            @param oid The subject sequence id.
            @return Index of the corresponding index data in 
                    \e this->indices_.
        */
        TSeqMap::size_type LocateIndex( CDbIndex::TSeqNum oid ) const
        {
            for( TSeqMap::size_type i = 0; i < seqmap_.size(); ++i ) {
                if( seqmap_[i] > oid ) return i;
            }

            assert( 0 );
            return 0;
        }

        BlastSeqSrc * db_;      /**< Points to the real BLAST database instance. */
        TResultSet results_;    /**< Set of result sets, one per loaded index. */
        TSeqMap seqmap_;        /**< For each element of \e indices_ with index i
                                     seqmap_[i] contains one plus the last oid of
                                     that database index. */

        unsigned long threads_;         /**< Number of threads to use for seed search. */
        bool threads_set_;              /**< True if number of threads was explicitly set by index name. */
        vector< string > index_names_;  /**< List of index volume names. */
        bool seq_from_index_;   /**< Indicates if it is OK to serve sequence data
                                     directly from the index. */
        bool index_preloaded_;  /**< True if indices are preloaded in constructor. */
        TIndexSet indices_;     /**< Currently loaded indices. */

        CRef< CBlastSeqLocWrap > locs_wrap_; /**< Current set of unmasked query locations. */

    public:

        typedef SThreadLocal TThreadLocal; /**< Type for thread local data. */

        typedef std::list< TThreadLocal * > TThreadDataSet; /**< Type of a set of allocated TThreadLocal objects. */

        static TThreadDataSet Thread_Data_Set; /**< Set of allocated TThreadLocal objects. */

        /** Object constructor.
            
            @param indexname A string that is a comma separated list of index
                             file prefix, number of threads, first and
                             last chunks of the index.
            @param db        Points to the open BLAST database object.
        */
        explicit CIndexedDb( const string & indexname, BlastSeqSrc * db );

        /** Object destructor. */
        ~CIndexedDb();

        /** Access a wrapped BLAST database object.

            @return The BLAST database object.
        */
        BlastSeqSrc * GetDb() const { return db_; }

        /** Get the data portion of the BLAST database object.

            \e this->db_ encapsulates an actual structure representing 
            a particular implementation of an open BLAST database. This
            function returns a typeless pointer to this data.

            @return Pointer to the data encapsulated by /e this=>db_.
        */
        void * GetSeqDb() const 
        { return _BlastSeqSrcImpl_GetDataStructure( db_ ); }

        /** Check whether any results were reported for a given subject sequence.

            @param oid The subject sequence id.
            @return True if the were seeds found for that subject;
                    false otherwise.
        */
        bool CheckOid( Int4 oid ) const
        {
            TSeqMap::size_type i = LocateIndex( oid );
            const CConstRef< CDbIndex::CSearchResults > & results = results_[i];
            if( i > 0 ) oid -= seqmap_[i-1];
            return results->CheckResults( oid );
        }

        /** Invoke the seed search procedure on each of the loaded indices.

            Each search is run in a separate thread. The function waits until
            all threads are complete before it returns.

            @param queries      Queries descriptor.
            @param locs         Unmasked intervals of queries.
            @param lut_options  Lookup table parameters, like target word size.
            @param word_options Contains window size of two-hits based search.
        */
        void PreSearch( 
                BLAST_SequenceBlk * queries, BlastSeqLoc * locs,
                LookupTableOptions * lut_options,
                BlastInitialWordOptions *  word_options );

        /** Wrapper around PreSearch().
            Runs PreSearch() and then frees locs_wrap_.
        */
        void RunSearch( 
                BLAST_SequenceBlk * queries, 
                LookupTableOptions * lut_options,
                BlastInitialWordOptions *  word_options )
        {
            PreSearch( 
                    queries, locs_wrap_->getLocs(), 
                    lut_options, word_options );
            locs_wrap_.Release();
        }

        /** Set the current set of unmasked query segments.
            @param locs_wrap unmasked query segments
        */
        void SetQueryInfo( CRef< CBlastSeqLocWrap > locs_wrap )
        { locs_wrap_ = locs_wrap; }

        /** Set the number of threads for indexed search.
            @param n_threads target number of threads
        */
        void SetNumThreads( size_t n_threads );

        /** Return results corresponding to a given subject sequence and chunk.

            @param oid          [I]   The subject sequence id.
            @param chunk        [I]   The chunk number.
            @param init_hitlist [I/O] The results are returned here.

            @return Word size used for search.
        */
        unsigned long GetResults( 
                CDbIndex::TSeqNum oid,
                CDbIndex::TSeqNum chunk,
                BlastInitHitList * init_hitlist ) const;

        /** Check if sequences can be supplied directly from the index.

            @return true if sequences can be provided from the index;
                    false otherwise.
        */
        bool SeqFromIndex() const { return seq_from_index_; }

        /** Get the length of the subject sequence.

            @param oid Subject ordinal id.

            @return Length of the subject sequence in bases.
        */
        TSeqPos GetSeqLength( CDbIndex::TSeqNum oid ) const
        { return indices_[LocateIndex( oid )]->GetSeqLen( oid ); }

        /** Get the subject sequence data.

            @param oid Subject ordinal id.

            @return Pointer to the start of the subject sequence data.
        */
        Uint1 * GetSeqData( CDbIndex::TSeqNum oid ) const
        { 
            return const_cast< Uint1 * >(
                    indices_[LocateIndex( oid )]->GetSeqData( oid )); 
        }
};

//------------------------------------------------------------------------------
CIndexedDb::TThreadDataSet CIndexedDb::Thread_Data_Set; 

//------------------------------------------------------------------------------
/// Set the number of treads for indexed search.
/// @param seq_src pointer to index structure
/// @param n_threads target number of threads
static void IndexedDbSetNumThreads( BlastSeqSrc * seq_src, size_t n_threads )
{
    CIndexedDb::TThreadLocal * idb_tl = 
        static_cast< CIndexedDb::TThreadLocal * >(
                _BlastSeqSrcImpl_GetDataStructure( seq_src ) );
    bool found = false;

    for( CIndexedDb::TThreadDataSet::iterator i = 
            CIndexedDb::Thread_Data_Set.begin();
         i != CIndexedDb::Thread_Data_Set.end(); ++i ) {
        if( *i == idb_tl ) {
            found = true;
            break;
        }
    }

    if( !found ) return;

    CIndexedDb * idb = idb_tl->idb_.GetPointerOrNull();
    idb->SetNumThreads( n_threads );
}

//------------------------------------------------------------------------------
/// Run indexed search.
/// @param seq_src pointer to the index structure
/// @param queries query data
/// @param lut_options lookup table parameters
/// @param word_options word parameters
static void IndexedDbRunSearch(
        BlastSeqSrc * seq_src, BLAST_SequenceBlk * queries, 
        LookupTableOptions * lut_options, 
        BlastInitialWordOptions * word_options )
{
    CIndexedDb::TThreadLocal * idb_tl = 
        static_cast< CIndexedDb::TThreadLocal * >(
                _BlastSeqSrcImpl_GetDataStructure( seq_src ) );
    bool found = false;

    for( CIndexedDb::TThreadDataSet::iterator i = 
            CIndexedDb::Thread_Data_Set.begin();
         i != CIndexedDb::Thread_Data_Set.end(); ++i ) {
        if( *i == idb_tl ) {
            found = true;
            break;
        }
    }

    if( !found ) return;

    CIndexedDb * idb = idb_tl->idb_.GetPointerOrNull();
    idb->RunSearch( queries, lut_options, word_options );
}

//------------------------------------------------------------------------------
/// Set information about unmasked query segments.
/// @param seq_src pointer to index structure
/// @param lt_wrap lookup table information to update
/// @param locs_wrap set of unmasked query segments
static void IndexedDbSetQueryInfo(
        BlastSeqSrc * seq_src, 
        LookupTableWrap * lt_wrap, 
        CRef< CBlastSeqLocWrap > locs_wrap )
{
    CIndexedDb::TThreadLocal * idb_tl = 
        static_cast< CIndexedDb::TThreadLocal * >(
                _BlastSeqSrcImpl_GetDataStructure( seq_src ) );
    bool found = false;

    for( CIndexedDb::TThreadDataSet::iterator i = 
            CIndexedDb::Thread_Data_Set.begin();
         i != CIndexedDb::Thread_Data_Set.end(); ++i ) {
        if( *i == idb_tl ) {
            found = true;
            break;
        }
    }

    if( !found ) return;

    CIndexedDb * idb = idb_tl->idb_.GetPointerOrNull();
    lt_wrap->lut = (void *)idb;
    lt_wrap->read_indexed_db = (void *)(&s_MB_IdbGetResults);
    idb->SetQueryInfo( locs_wrap );
}

//------------------------------------------------------------------------------
/** Callback that is called for index based seed search.
    @sa For the meaning of parameters see DbIndexPreSearchFnType.
*/
static void IndexedDbPreSearch( 
        BlastSeqSrc * seq_src, 
        LookupTableWrap * lt_wrap,
        BLAST_SequenceBlk * queries, 
        BlastSeqLoc * locs,
        LookupTableOptions * lut_options, 
        BlastInitialWordOptions * word_options )
{
    CIndexedDb::TThreadLocal * idb_tl = 
        static_cast< CIndexedDb::TThreadLocal * >(
                _BlastSeqSrcImpl_GetDataStructure( seq_src ) );
    bool found = false;

    for( CIndexedDb::TThreadDataSet::iterator i = 
            CIndexedDb::Thread_Data_Set.begin();
         i != CIndexedDb::Thread_Data_Set.end(); ++i ) {
        if( *i == idb_tl ) {
            found = true;
            break;
        }
    }

    if( !found ) return;

    CIndexedDb * idb = idb_tl->idb_.GetPointerOrNull();
    lt_wrap->lut = (void *)idb;
    lt_wrap->read_indexed_db = (void *)(&s_MB_IdbGetResults);
    idb->PreSearch( queries, locs, lut_options, word_options );

#ifdef STOP_AFTER_PRESEARCH
    exit( 0 );
#endif
}

//------------------------------------------------------------------------------
CIndexedDb::CIndexedDb( const string & indexnames, BlastSeqSrc * db )
    : db_( db ), threads_( 1 ), threads_set_( false ),
      seq_from_index_( false ), index_preloaded_( false )
{
    if( !indexnames.empty() ) {
        vector< string > dbnames;
        string::size_type start = 0, end = 0;

        // Interpret indexname as a space separated list of database names.
        //
        while( end != string::npos ) {
            end = indexnames.find_first_of( " ", start );
            dbnames.push_back( indexnames.substr( start, end ) );
            start = end + 1;
            end = indexnames.find_first_not_of( " ", end );
        }

        std::sort( dbnames.begin(), dbnames.end(), &SeqDB_CompareVolume );

        for( vector< string >::const_iterator dbni = dbnames.begin();
                dbni != dbnames.end(); ++dbni ) {
            const string & indexname = *dbni;

            // Parse the indexname as a comma separated list
            unsigned long start_vol = 0, stop_vol = 99;
            start = 0;
            end = indexname.find_first_of( ",", start );
            string index_base = indexname.substr( start, end );
            start = end + 1;
    
            if( start < indexname.length() && end != string::npos ) {
                end = indexname.find_first_of( ",", start );
                string threads_str = indexname.substr( start, end );
                
                if( !threads_str.empty() ) {
                    threads_set_ = true;
                    unsigned long th_tmp = atoi( threads_str.c_str() );
                    if( th_tmp > threads_ ) threads_ = th_tmp;
                }
    
                start = end + 1;
    
                if( start < indexname.length() && end != string::npos ) {
                    end = indexname.find_first_of( ",", start );
                    string start_vol_str = indexname.substr( start, end );
    
                    if( !start_vol_str.empty() ) {
                        start_vol = atoi( start_vol_str.c_str() );
                    }
    
                    start = end + 1;
    
                    if( start < indexname.length() && end != string::npos ) {
                        end = indexname.find_first_of( ",", start );
                        string stop_vol_str = indexname.substr( start, end );
    
                        if( !stop_vol_str.empty() ) {
                            stop_vol = atoi( stop_vol_str.c_str() );
                        }
                    }
                }
            }
    
            if( threads_ == 0 ) threads_ = 1;
    
            if( start_vol <= stop_vol ) {
                long last_i = -1;
    
                for( long i = start_vol; (unsigned long)i <= stop_vol; ++i ) {
                    ostringstream os;
                    os << index_base << "." << setw( 2 ) << setfill( '0' )
                       << i << ".idx";
                    string name = SeqDB_ResolveDbPath( os.str() );
    
                    if( !name.empty() ){
                        if( i - last_i > 1 ) {
                            for( long j = last_i + 1; j < i; ++j ) {
                                ERR_POST( Error << "Index volume " 
                                                << j << " not resolved." );
                            }
                        }
    
                        index_names_.push_back( name );
                        last_i = i;
                    }
                }
            }
        }
    }

    if( index_names_.empty() ) {
        string msg("no index file specified or index '");
        msg += indexnames + "*' not found.";
        NCBI_THROW(CDbIndex_Exception, eBadOption, msg);
    }

    PreSearchFn = &IndexedDbPreSearch;
    SetQueryInfoFn = &IndexedDbSetQueryInfo;
    RunSearchFn = &IndexedDbRunSearch;
    SetNumThreadsFn = &IndexedDbSetNumThreads;
    
    /*
    if( threads_ >= index_names_.size() ) {
        seq_from_index_ = true;
        index_preloaded_ = true;

        for( vector< string >::size_type ind = 0; 
                ind < index_names_.size(); ++ind ) {
            CRef< CDbIndex > index = CDbIndex::Load( index_names_[ind] );

            if( index == 0 ) {
                throw std::runtime_error(
                        (string( "CIndexedDb: could not load index" ) 
                        + index_names_[ind]).c_str() );
            }

            if( index->Version() != 5 ) seq_from_index_ = false;
            indices_.push_back( index );
            results_.push_back( CConstRef< CDbIndex::CSearchResults >( null ) );
            CDbIndex::TSeqNum s = seqmap_.empty() ? 0 : *seqmap_.rbegin();
            seqmap_.push_back( s + (index->StopSeq() - index->StartSeq()) );
        }
    }
    */
}

//------------------------------------------------------------------------------
void CIndexedDb::SetNumThreads( size_t n_threads )
{
    if( !threads_set_ && n_threads > 0 ) threads_ = n_threads;

    if( threads_ >= index_names_.size() && !index_preloaded_ ) {
        seq_from_index_ = true;
        index_preloaded_ = true;

        for( vector< string >::size_type ind = 0; 
                ind < index_names_.size(); ++ind ) {
            CRef< CDbIndex > index = CDbIndex::Load( index_names_[ind] );

            if( index == 0 ) {
                throw std::runtime_error(
                        (string( "CIndexedDb: could not load index" ) 
                        + index_names_[ind]).c_str() );
            }

            indices_.push_back( index );
            results_.push_back( CConstRef< CDbIndex::CSearchResults >( null ) );
            CDbIndex::TSeqNum s = seqmap_.empty() ? 0 : *seqmap_.rbegin();
            seqmap_.push_back( s + (index->StopSeq() - index->StartSeq()) );
        }
    }
    else if( !index_preloaded_ ) {
        results_.clear();
        seqmap_.clear();
    }
}

//------------------------------------------------------------------------------
CIndexedDb::~CIndexedDb()
{
    PreSearchFn = &NullPreSearch;
    BlastSeqSrcFree( db_ );
}

//------------------------------------------------------------------------------
/** One thread of the indexed seed search. */
class CPreSearchThread : public CThread
{
    public:

        /** Object constructor.

            @param queries Query descriptor.
            @param locs Valid query intervals.
            @param sopt Search options.
            @param index Database index reference.
            @param results Search results are returned in this object.
        */
        CPreSearchThread(
                BLAST_SequenceBlk * queries,
                BlastSeqLoc * locs,
                const CDbIndex::SSearchOptions & sopt,
                CRef< CDbIndex > & index,
                CConstRef< CDbIndex::CSearchResults > & results )
            : queries_( queries ), locs_( locs ), sopt_( sopt ),
              index_( index ), results_( results )
        {}

        /** Main procedure of the thread.

            @return Search results.
        */
        virtual void * Main( void );

        /** Procedure to invoke on thread exit. */
        virtual void OnExit( void ) {}

    private:

        BLAST_SequenceBlk * queries_;   /**< Query descriptor. */
        BlastSeqLoc * locs_;            /**< Valid query intervals. */
        const CDbIndex::SSearchOptions & sopt_; /**< Search options. */
        CRef< CDbIndex > & index_;      /**< Database index reference. */
        CConstRef< CDbIndex::CSearchResults > & results_; /**< Search results. */
};

//------------------------------------------------------------------------------
void * CPreSearchThread::Main( void )
{
    results_ = index_->Search( queries_, locs_, sopt_ );
    // index_->Remap();
    return 0;
}

//------------------------------------------------------------------------------
void CIndexedDb::PreSearch( 
        BLAST_SequenceBlk * queries, BlastSeqLoc * locs,
        LookupTableOptions * lut_options , 
        BlastInitialWordOptions * word_options )
{
    CDbIndex::SSearchOptions sopt;
    sopt.word_size = lut_options->word_size;
    sopt.two_hits = word_options->window_size;

    for( vector< string >::size_type v = 0; 
            v < index_names_.size(); v += threads_ ) {
        if( !index_preloaded_ ) {
            indices_.clear();

            for( vector< string >::size_type ind = 0; 
                ind < threads_ && v + ind < index_names_.size(); ++ind ) {
                CRef< CDbIndex > index = CDbIndex::Load( index_names_[v+ind] );

                if( index == 0 ) {
                    throw std::runtime_error(
                            (string( "CIndexedDb: could not load index" ) 
                            + index_names_[v+ind]).c_str() );
                }

                indices_.push_back( index );
                results_.push_back( CConstRef< CDbIndex::CSearchResults >( null ) );
                CDbIndex::TSeqNum s = seqmap_.empty() ? 0 : *seqmap_.rbegin();
                seqmap_.push_back( s + (index->StopSeq() - index->StartSeq()) );
            }
        }

        if( indices_.size() > 1 ) {
            vector< CPreSearchThread * > threads( indices_.size(), 0 );

            for( vector< CPreSearchThread * >::size_type i = 0;
                    i < threads.size(); ++i ) {
                threads[i] = new CPreSearchThread(
                        queries, locs, sopt, indices_[i], results_[v+i] );
                threads[i]->Run();
            }

            for( vector< CPreSearchThread * >::iterator i = threads.begin();
                    i != threads.end(); ++i ) {
                void * result;
                (*i)->Join( &result );
                *i = 0;
            }
        }
        else {
            CRef< CDbIndex > & index = indices_[0];
            CConstRef< CDbIndex::CSearchResults > & results = results_[v];
            results = index->Search( queries, locs, sopt );
            // index->Remap();
        }
    }
}

//------------------------------------------------------------------------------
unsigned long CIndexedDb::GetResults( 
        CDbIndex::TSeqNum oid, CDbIndex::TSeqNum chunk, 
        BlastInitHitList * init_hitlist ) const
{
    BlastInitHitList * res = 0;
    TSeqMap::size_type i = LocateIndex( oid );
    const CConstRef< CDbIndex::CSearchResults > & results = results_[i];
    if( i > 0 ) oid -= seqmap_[i-1];

    if( (res = results->GetResults( oid, chunk )) != 0 ) {
        BlastInitHitListMove( init_hitlist, res );
        return results->GetWordSize();
    }else {
        BlastInitHitListReset( init_hitlist );
        return 0;
    }
}

//------------------------------------------------------------------------------
BlastSeqSrc * DbIndexSeqSrcInit( const string & indexname, BlastSeqSrc * db )
{
    BlastSeqSrcNewInfo bssn_info;
    SIndexedDbNewArgs idb_args = { indexname, db };
    bssn_info.constructor = &s_IDbSrcNew;
    bssn_info.ctor_argument = (void *)&idb_args;
    return BlastSeqSrcNew( &bssn_info );
}

//------------------------------------------------------------------------------
BlastSeqSrc * CloneSeqSrcInit( BlastSeqSrc * src )
{
    BlastSeqSrcNewInfo bssn_info;
    bssn_info.constructor = &s_CloneSrcNew;
    bssn_info.ctor_argument = (void *)src;
    return BlastSeqSrcNew( &bssn_info );
}

//------------------------------------------------------------------------------
void CloneSeqSrc( BlastSeqSrc * dst, BlastSeqSrc * src )
{
    _BlastSeqSrcImpl_SetNewFnPtr           ( dst, _BlastSeqSrcImpl_GetNewFnPtr( src ) );
    _BlastSeqSrcImpl_SetDeleteFnPtr        ( dst, _BlastSeqSrcImpl_GetDeleteFnPtr( src ) );
    _BlastSeqSrcImpl_SetCopyFnPtr          ( dst, _BlastSeqSrcImpl_GetCopyFnPtr( src ) );
    _BlastSeqSrcImpl_SetDataStructure      ( dst, _BlastSeqSrcImpl_GetDataStructure( src ) );
    _BlastSeqSrcImpl_SetGetNumSeqs         ( dst, _BlastSeqSrcImpl_GetGetNumSeqs( src ) );
    _BlastSeqSrcImpl_SetGetNumSeqsStats    ( dst, _BlastSeqSrcImpl_GetGetNumSeqsStats( src ) );
    _BlastSeqSrcImpl_SetGetMaxSeqLen       ( dst, _BlastSeqSrcImpl_GetGetMaxSeqLen( src ) );
    _BlastSeqSrcImpl_SetGetAvgSeqLen       ( dst, _BlastSeqSrcImpl_GetGetAvgSeqLen( src ) );
    _BlastSeqSrcImpl_SetGetTotLen          ( dst, _BlastSeqSrcImpl_GetGetTotLen( src ) );
    _BlastSeqSrcImpl_SetGetTotLenStats     ( dst, _BlastSeqSrcImpl_GetGetTotLenStats( src ) );
    _BlastSeqSrcImpl_SetGetName            ( dst, _BlastSeqSrcImpl_GetGetName( src ) );
    _BlastSeqSrcImpl_SetGetIsProt          ( dst, _BlastSeqSrcImpl_GetGetIsProt( src ) );
    _BlastSeqSrcImpl_SetGetSupportsPartialFetching  ( dst, _BlastSeqSrcImpl_GetGetSupportsPartialFetching( src ) );
    _BlastSeqSrcImpl_SetSetSeqRange        ( dst, _BlastSeqSrcImpl_GetSetSeqRange(src ) );
    _BlastSeqSrcImpl_SetGetSequence        ( dst, _BlastSeqSrcImpl_GetGetSequence( src ) );
    _BlastSeqSrcImpl_SetGetSeqLen          ( dst, _BlastSeqSrcImpl_GetGetSeqLen( src ) );
    _BlastSeqSrcImpl_SetIterNext           ( dst, _BlastSeqSrcImpl_GetIterNext( src ) );
    _BlastSeqSrcImpl_SetReleaseSequence    ( dst, _BlastSeqSrcImpl_GetReleaseSequence( src ) );
    _BlastSeqSrcImpl_SetResetChunkIterator ( dst, _BlastSeqSrcImpl_GetResetChunkIterator( src ) );
}

//------------------------------------------------------------------------------
DbIndexPreSearchFnType GetDbIndexPreSearchFn() { return PreSearchFn; }
DbIndexSetQueryInfoFnType GetDbIndexSetQueryInfoFn() { return SetQueryInfoFn; }
DbIndexRunSearchFnType GetDbIndexRunSearchFn() { return RunSearchFn; }
DbIndexSetNumThreadsFnType GetDbIndexSetNumThreadsFn() { return SetNumThreadsFn; }

END_SCOPE( blast )
END_NCBI_SCOPE

USING_SCOPE( ncbi );
USING_SCOPE( ncbi::blast );

extern "C" {

//------------------------------------------------------------------------------
/** C language wrapper around CIndexedDb::GetDb(). */
static BlastSeqSrc * s_GetForwardSeqSrc( void * handle )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    return idb_handle->idb_->GetDb();
}

//------------------------------------------------------------------------------
/** C language wrapper around CIndexedDb::GetSeqDb(). */
static void * s_GetForwardSeqDb( void * handle )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    return idb_handle->idb_->GetSeqDb();
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int4 s_IDbGetNumSeqs( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetNumSeqs( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int4 s_IDbGetNumSeqsStats( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetNumSeqsStats( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int4 s_IDbGetMaxLength( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetMaxSeqLen( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int4 s_IDbGetAvgLength( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetAvgSeqLen( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int8 s_IDbGetTotLen( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetTotLen( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int8 s_IDbGetTotLenStats( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetTotLenStats( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static const char * s_IDbGetName( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetName( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Boolean s_IDbGetIsProt( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetIsProt( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int2 s_IDbGetSequence( void * handle, BlastSeqSrcGetSeqArg * seq_arg )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    CRef< CIndexedDb > idb = idb_handle->idb_;
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetSequence( fw_seqsrc )( fw_handle, seq_arg );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static Int4 s_IDbGetSeqLen( void * handle, void * x )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    return _BlastSeqSrcImpl_GetGetSeqLen( fw_seqsrc )( fw_handle, x );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_ but skip over the ones for
    which no results were poduces by pre-search. 
*/
static Int4 s_IDbIteratorNext( void * handle, BlastSeqSrcIterator * itr )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    CIndexedDb * idb = idb_handle->idb_.GetPointerOrNull();
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    Int4 oid = BLAST_SEQSRC_EOF;

    do {
        oid = _BlastSeqSrcImpl_GetIterNext( fw_seqsrc )( fw_handle, itr );
    }while( oid != BLAST_SEQSRC_EOF && !idb->CheckOid( oid ) );

    return oid;
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static void s_IDbReleaseSequence( void * handle, BlastSeqSrcGetSeqArg * getseq_arg )
{
    CIndexedDb::TThreadLocal * idb_handle = (CIndexedDb::TThreadLocal *)handle;
    CRef< CIndexedDb > idb = idb_handle->idb_;
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    _BlastSeqSrcImpl_GetReleaseSequence( fw_seqsrc )( fw_handle, getseq_arg );
}

//------------------------------------------------------------------------------
/** Forwards the call to CIndexedDb::db_. */
static void s_IDbResetChunkIterator( void * handle )
{
    BlastSeqSrc * fw_seqsrc = s_GetForwardSeqSrc( handle );
    void * fw_handle = s_GetForwardSeqDb( handle );
    _BlastSeqSrcImpl_GetResetChunkIterator( fw_seqsrc )( fw_handle );
}

//------------------------------------------------------------------------------
/** Destroy the sequence source.

    Destroys and frees CIndexedDb instance and the wrapped BLAST database.

    @param seq_src BlastSeqSrc wrapper around CIndexedDb object.
    @return NULL.
*/
static BlastSeqSrc * s_IDbSrcFree( BlastSeqSrc * seq_src )
{
    if( seq_src ) {
        CIndexedDb::TThreadLocal * idb = 
            static_cast< CIndexedDb::TThreadLocal * >( 
                    _BlastSeqSrcImpl_GetDataStructure( seq_src ) );

        for( CIndexedDb::TThreadDataSet::iterator i = 
                CIndexedDb::Thread_Data_Set.begin();
             i != CIndexedDb::Thread_Data_Set.end(); ++i ) {
            if( *i == idb ) {
                CIndexedDb::Thread_Data_Set.erase( i );
                break;
            }
        }

        delete idb;
    }

    return 0;
}

//------------------------------------------------------------------------------
/** Fill the BlastSeqSrc data with a copy of its own contents.

    @param seq_src A BlastSeqSrc instance to fill.
    @return seq_src if copy was successfull, NULL otherwise.
*/
static BlastSeqSrc * s_IDbSrcCopy( BlastSeqSrc * seq_src )
{
    if( !seq_src ) {
        return 0;
    }else {
        CIndexedDb::TThreadLocal * idb =
            static_cast< CIndexedDb::TThreadLocal * >(
                    _BlastSeqSrcImpl_GetDataStructure( seq_src ) );
        try {
            CIndexedDb::TThreadLocal * idb_copy = 
                new CIndexedDb::TThreadLocal( *idb );
            CIndexedDb::Thread_Data_Set.push_back( idb_copy );
            _BlastSeqSrcImpl_SetDataStructure( seq_src, (void *)idb_copy );
            return seq_src;
        }catch( ... ) {
            return 0;
        }
    }
}

//------------------------------------------------------------------------------
/** Initialize the BlastSeqSrc data structure with the appropriate callbacks. */
static void s_IDbSrcInit( BlastSeqSrc * retval, CIndexedDb::TThreadLocal * idb )
{
    _ASSERT( retval );
    _ASSERT( idb );

    _BlastSeqSrcImpl_SetDeleteFnPtr        (retval, & s_IDbSrcFree);
    _BlastSeqSrcImpl_SetCopyFnPtr          (retval, & s_IDbSrcCopy);
    _BlastSeqSrcImpl_SetDataStructure      (retval, (void*) idb);
    _BlastSeqSrcImpl_SetGetNumSeqs         (retval, & s_IDbGetNumSeqs);
    _BlastSeqSrcImpl_SetGetNumSeqsStats    (retval, & s_IDbGetNumSeqsStats);
    _BlastSeqSrcImpl_SetGetMaxSeqLen       (retval, & s_IDbGetMaxLength);
    _BlastSeqSrcImpl_SetGetAvgSeqLen       (retval, & s_IDbGetAvgLength);
    _BlastSeqSrcImpl_SetGetTotLen          (retval, & s_IDbGetTotLen);
    _BlastSeqSrcImpl_SetGetTotLenStats     (retval, & s_IDbGetTotLenStats);
    _BlastSeqSrcImpl_SetGetName            (retval, & s_IDbGetName);
    _BlastSeqSrcImpl_SetGetIsProt          (retval, & s_IDbGetIsProt);
    _BlastSeqSrcImpl_SetGetSequence        (retval, & s_IDbGetSequence);
    _BlastSeqSrcImpl_SetGetSeqLen          (retval, & s_IDbGetSeqLen);
    _BlastSeqSrcImpl_SetIterNext           (retval, & s_IDbIteratorNext);
    _BlastSeqSrcImpl_SetReleaseSequence    (retval, & s_IDbReleaseSequence);
    _BlastSeqSrcImpl_SetResetChunkIterator (retval, & s_IDbResetChunkIterator);
}

//------------------------------------------------------------------------------
static BlastSeqSrc * s_CloneSrcNew( BlastSeqSrc * retval, void * args )
{
    BlastSeqSrc * src = (BlastSeqSrc *)args;
    CloneSeqSrc( retval, src );
    return retval;
}

//------------------------------------------------------------------------------
static BlastSeqSrc * s_IDbSrcNew( BlastSeqSrc * retval, void * args )
{
    _ASSERT( retval );
    _ASSERT( args );
    SIndexedDbNewArgs * idb_args = (SIndexedDbNewArgs *)args;
    CIndexedDb::TThreadLocal * idb = 0;
    bool success = false;

    try {
        idb = new CIndexedDb::TThreadLocal;
        idb->idb_.Reset( new CIndexedDb( idb_args->indexname, idb_args->db ) );
        success = true;
    } catch (const ncbi::CException& e) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, 
                        strdup(e.ReportThis(eDPF_ErrCodeExplanation).c_str()));
    } catch (const std::exception& e) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, strdup(e.what()));
    } catch (...) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, 
             strdup("Caught unknown exception from CSeqDB constructor"));
    }

    if( success ) {
        CIndexedDb::Thread_Data_Set.push_back( idb );
        s_IDbSrcInit( retval, idb );
    }else {
        delete idb;
    }

    return retval;
}

//------------------------------------------------------------------------------
static unsigned long s_MB_IdbGetResults(
        void * idb_v,
        Int4 oid_i, Int4 chunk_i,
        BlastInitHitList * init_hitlist )
{
    _ASSERT( idb_v != 0 );
    _ASSERT( oid_i >= 0 );
    _ASSERT( chunk_i >= 0 );
    _ASSERT( init_hitlist != 0 );

    CIndexedDb * idb = (CIndexedDb *)idb_v;
    CDbIndex::TSeqNum oid = (CDbIndex::TSeqNum)oid_i;
    CDbIndex::TSeqNum chunk = (CDbIndex::TSeqNum)chunk_i;

    return idb->GetResults( oid, chunk, init_hitlist );
}

} /* extern "C" */

/* @} */
