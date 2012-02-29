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

static int s_MB_IdbCheckOid( Int4 oid );

}

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( blast )

USING_SCOPE( ncbi::objects );
USING_SCOPE( ncbi::blastdbindex );

/// Get the minimum acceptable word size to use with indexed search.
/// @return the minimum acceptable word size
int MinIndexWordSize() { return 16; }

/** No-op callback for setting query info. Used when index search is not enabled.
    @sa DbIndexSetQueryInfoFnType()
*/
static void NullSetQueryInfo(
        LookupTableWrap * , 
        CRef< CBlastSeqLocWrap > ) {}

/** No-op callback to run indexed search. Used when index search is not enabled.
    @sa DbIndexRunSearchFnType()
*/
static void NullRunSearch(
        BLAST_SequenceBlk * , 
        LookupTableOptions * , BlastInitialWordOptions * ) {}

/** Global pointer to the appropriate callback to set query info, based
    on whether or not index search is enabled.
*/
static DbIndexSetQueryInfoFnType SetQueryInfoFn = &NullSetQueryInfo;

/** Global pointer to the appropriate callback to run indexed search, based
    on whether or not index search is enabled.
*/
static DbIndexRunSearchFnType RunSearchFn = &NullRunSearch;

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

        TResultSet results_;    /**< Set of result sets, one per loaded index. */
        TSeqMap seqmap_;        /**< For each element of \e indices_ with index i
                                     seqmap_[i] contains one plus the last oid of
                                     that database index. */

        vector< string > index_names_;  /**< List of index volume names. */
        CRef< CDbIndex > index_;   /**< Currently loaded index */

        CRef< CBlastSeqLocWrap > locs_wrap_; /**< Current set of unmasked query locations. */

    public:

        static CRef< CIndexedDb > Index_Set_Instance; /**< Shared representation of
                                                           currently loaded index volumes. */

        /** Object constructor.
            
            @param indexname A string that is a comma separated list of index
                             file prefix, number of threads, first and
                             last chunks of the index.
        */
        explicit CIndexedDb( const string & indexname );

        /** Object destructor. */
        ~CIndexedDb();

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
};

//------------------------------------------------------------------------------
CRef< CIndexedDb > CIndexedDb::Index_Set_Instance;

//------------------------------------------------------------------------------
/// Run indexed search.
/// @param queries query data
/// @param lut_options lookup table parameters
/// @param word_options word parameters
static void IndexedDbRunSearch(
        BLAST_SequenceBlk * queries, 
        LookupTableOptions * lut_options, 
        BlastInitialWordOptions * word_options )
{
    CIndexedDb * idb( CIndexedDb::Index_Set_Instance.GetPointerOrNull() );
    if( idb == 0 ) return;
    idb->RunSearch( queries, lut_options, word_options );
}

//------------------------------------------------------------------------------
/// Set information about unmasked query segments.
/// @param lt_wrap lookup table information to update
/// @param locs_wrap set of unmasked query segments
static void IndexedDbSetQueryInfo(
        LookupTableWrap * lt_wrap, 
        CRef< CBlastSeqLocWrap > locs_wrap )
{
    CIndexedDb * idb( CIndexedDb::Index_Set_Instance.GetPointerOrNull() );
    if( idb == 0 ) return;
    lt_wrap->lut = (void *)idb;
    lt_wrap->read_indexed_db = (void *)(&s_MB_IdbGetResults);
    lt_wrap->check_index_oid = (void *)(&s_MB_IdbCheckOid);
    idb->SetQueryInfo( locs_wrap );
}

//------------------------------------------------------------------------------
CIndexedDb::CIndexedDb( const string & indexnames )
{
    if( !indexnames.empty() ) {
        vector< string > dbnames;
        string::size_type start = 0, end = 0;

        // Interpret indexname as a space separated list of database names.
        //
        while( start != string::npos ) {
            end = indexnames.find_first_of( " ", start );
            dbnames.push_back( indexnames.substr( start, end - start ) );
            start = indexnames.find_first_not_of( " ", end );
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
                start = end + 1;
    
                if( start < indexname.length() && end != string::npos ) {
                    end = indexname.find_first_of( ",", start );
                    string start_vol_str = 
                        indexname.substr( start, end - start );
    
                    if( !start_vol_str.empty() ) {
                        start_vol = atoi( start_vol_str.c_str() );
                    }
    
                    start = end + 1;
    
                    if( start < indexname.length() && end != string::npos ) {
                        end = indexname.find_first_of( ",", start );
                        string stop_vol_str = 
                            indexname.substr( start, end - start);
    
                        if( !stop_vol_str.empty() ) {
                            stop_vol = atoi( stop_vol_str.c_str() );
                        }
                    }
                }
            }
    
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

    SetQueryInfoFn = &IndexedDbSetQueryInfo;
    RunSearchFn = &IndexedDbRunSearch;
}

//------------------------------------------------------------------------------
CIndexedDb::~CIndexedDb()
{
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
            v < index_names_.size(); v += 1 ) {
        CRef< CDbIndex > index = CDbIndex::Load( index_names_[v] );

        if( index == 0 ) { 
            throw std::runtime_error( 
                    (string( "CIndexedDb: could not load index" ) 
                     + index_names_[v]).c_str() );
        }

        index_ = index;
        results_.push_back( CConstRef< CDbIndex::CSearchResults >( null ) );
        CDbIndex::TSeqNum s = seqmap_.empty() ? 0 : *seqmap_.rbegin();
        seqmap_.push_back( s + (index->StopSeq() - index->StartSeq()) );
        CConstRef< CDbIndex::CSearchResults > & results = results_[v];
        results = index_->Search( queries, locs, sopt );
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
std::string DbIndexInit( const string & indexname )
{
    try {
        CIndexedDb::Index_Set_Instance.Reset( new CIndexedDb( indexname ) );
        if( CIndexedDb::Index_Set_Instance != 0 ) return "";
        else return "index allocation error";
    }
    catch( CException & e ) {
        return e.what();
    }
}

//------------------------------------------------------------------------------
DbIndexSetQueryInfoFnType GetDbIndexSetQueryInfoFn() { return SetQueryInfoFn; }
DbIndexRunSearchFnType GetDbIndexRunSearchFn() { return RunSearchFn; }

END_SCOPE( blast )
END_NCBI_SCOPE

USING_SCOPE( ncbi );
USING_SCOPE( ncbi::blast );

extern "C" {

//------------------------------------------------------------------------------
static int s_MB_IdbCheckOid( Int4 oid )
{
    _ASSERT( oid >= 0 );
    return (CIndexedDb::Index_Set_Instance->CheckOid( oid ) ? 1 : 0);
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
