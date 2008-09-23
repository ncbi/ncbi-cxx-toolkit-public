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
 *   Implementation of class CSRSearchApplication.
 *
 */

#include <ncbi_pch.hpp>

#include <algorithm>

#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>

#include <algo/blast/dbindex/sequence_istream_fasta.hpp>
#include <algo/blast/dbindex/dbindex.hpp>
#include <algo/blast/dbindex_search/sr_search.hpp>

#include "srsearch_app.hpp"

USING_NCBI_SCOPE;
USING_SCOPE( blastdbindex );
USING_SCOPE( dbindex_search );

void PrintResults( 
        CNcbiOstream & ostream,
        const vector< string > & idmap,
        CDbIndex::TSeqNum qnum, 
        const vector< CSRSearch::SResultData > & results,
        const string & idstr1, const string & idstr2 = "" );

//------------------------------------------------------------------------------
class CRCache
{
    public: // for Solaris only

        typedef CDbIndex::TSeqNum TSeqNum;

        struct SDataItem
        {
            CSRSearch::ELevel l1;
            CSRSearch::ELevel l2;
            vector< CSRSearch::SResultData > * r1;
            vector< CSRSearch::SResultData > * r2;
            string id1;
            string id2;
        };

    public:

        CRCache( Uint4 max_results_per_query )
            : max_res_( max_results_per_query ),
              rv_pool( max_results_per_query )
        {}

        void update( 
                TSeqNum query, 
                CSRSearch::TResults & results, 
                const string & idstr1, const string & idstr2 );

        void updateSIdMap( const vector< string > & idmap, TSeqNum start )
        {
            if( sidmap.size() < start + idmap.size() ) {
                sidmap.resize( start + idmap.size(), "unknown" );
            }

            for( TSeqNum i = start; i < start + idmap.size(); ++i ) {
                sidmap[i] = idmap[i - start];
            }
        }

        const vector< string > & getSIdMap() const { return sidmap; }

        void dump( CNcbiOstream & ostream );

        CSRSearch::ELevel getLevel1( TSeqNum q )
        { return data_pool[q].l1; }

        CSRSearch::ELevel getLevel2( TSeqNum q )
        { return data_pool[q].l2; }

        Uint4 getNRes1( TSeqNum q )
        { 
            vector< CSRSearch::SResultData > * r = data_pool[q].r1;
            return (r == 0) ? 0 : r->size();
        }

        Uint4 getNRes2( TSeqNum q )
        {
            vector< CSRSearch::SResultData > * r = data_pool[q].r2;
            return (r == 0) ? 0 : r->size();
        }

    private:

        class CRVPool
        {
                const static Uint4 BLOCK_SIZE = 1024UL*1024UL;
                const static Uint4 BLOCKS_RESERVE = 1024UL;

                typedef vector< CSRSearch::SResultData > TItem;
                typedef vector< TItem > TBlock;
                typedef vector< TBlock > TBlocks;

            public:

                CRVPool( Uint4 mres ) 
                    : max_res( mres ), num_items( BLOCK_SIZE )
                {
                    blocks.reserve( BLOCKS_RESERVE );
                }

                TItem * newItem()
                {
                    if( num_items == BLOCK_SIZE ) newBlock();
                    return &(*blocks.rbegin())[num_items++];
                }

            private:

                void newBlock()
                {
                    blocks.push_back( TBlock( BLOCK_SIZE ) );
                    TBlock & b = *blocks.rbegin();
                    
                    for( TBlock::iterator i = b.begin(); i != b.end(); ++i ) {
                        i->reserve( max_res );
                    }

                    num_items = 0;
                }

                Uint4 max_res;
                Uint4 num_items;
                TBlocks blocks;
        };

        class CDataPool
        {
                const static Uint4 BLOCK_SIZE = 1024UL*1024UL;
                const static Uint4 BLOCKS_RESERVE = 1024UL;

                const static Uint4 BLOCK_SHIFT = 20;
                const static Uint4 BLOCK_MASK  = ((1UL<<BLOCK_SHIFT) - 1);

                typedef vector< SDataItem > TBlock;
                typedef vector< TBlock > TBlocks;

            public:

                CDataPool() : q_max( 0 ), num_blocks( 0 ) 
                {
                    blocks.reserve( BLOCKS_RESERVE );
                }

                SDataItem & operator[]( TSeqNum q )
                {
                    ensure( q );
                    return at( q );
                }

                TSeqNum size() const { return q_max; }

            private:

                SDataItem & at( TSeqNum q )
                { return blocks[q>>BLOCK_SHIFT][q&BLOCK_MASK]; }

                void ensure( TSeqNum q )
                {
                    Uint4 block = (q>>BLOCK_SHIFT);

                    while( block >= num_blocks ) {
                        blocks.push_back( TBlock( BLOCK_SIZE ) );
                        ++num_blocks;
                    }

                    while( q >= q_max ) {
                        SDataItem & i = at( q_max );
                        i.l1 = i.l2 = CSRSearch::EM;
                        i.r1 = i.r2 = 0;
                        ++q_max;
                    }
                }

                TSeqNum q_max;
                Uint4 num_blocks;
                TBlocks blocks;
        };

        Uint4 max_res_;
        CDataPool data_pool;
        CRVPool rv_pool;
        vector< string > idmap1;
        vector< string > idmap2;
        vector< string > sidmap;
};

//------------------------------------------------------------------------------
void CRCache::dump( CNcbiOstream & ostream )
{
    for( TSeqNum i = 0; i < data_pool.size(); ++i ) {
        SDataItem & data = data_pool[i];

        if( data.r1 != 0 ) {
            PrintResults( ostream, sidmap, i, *data.r1, data.id1 );
        }
        
        if( data.r2 != 0 ) {
            PrintResults( ostream, sidmap, i, *data.r2, "", data.id2 );
        }
    }
}

//------------------------------------------------------------------------------
void CRCache::update( 
        TSeqNum query, CSRSearch::TResults & res, 
        const string & idstr1, const string & idstr2 )
{
    if( res.res.empty() ) return;

    SDataItem & data = data_pool[query];

    if( data.r1 == 0 ) data.r1 = rv_pool.newItem();
    if( data.r2 == 0 ) data.r2 = rv_pool.newItem();

    if( res.res.size() == res.nres_1 ) {
        if( data.l1 > res.level_1 ) data.r1->swap( res.res );
        else {
            for( Uint4 i = 0; i < res.res.size(); ++i ) {
                data.r1->push_back( res.res[i] );
            }
        }

        if( data.l2 > res.level_2 ) data.r2->clear();
        data.l1 = res.level_1;
    }
    else if( data.l1 < CSRSearch::SE ) {}
    else if( res.nres_1 == 0 ) {
        if( data.l2 > res.level_2 ) data.r2->swap( res.res );
        else {
            for( Uint4 i = 0; i < res.res.size(); ++i ) {
                data.r2->push_back( res.res[i] );
            }
        }

        if( data.l1 > res.level_1 ) data.r1->clear();
        data.l2 = res.level_2;
    }
    else {
        if( data.l1 > res.level_1 ) data.r1->clear();
        if( data.l2 > res.level_2 ) data.r2->clear();

        Uint4 sz = res.nres_1;

        for( Uint4 i = 0 ; i < sz; ++i ) {
            data.r1->push_back( res.res[i] );
        }

        sz = res.res.size();

        for( Uint4 i = res.nres_1; i < sz; ++i ) {
            data.r2->push_back( res.res[i] );
        }

        data.l1 = res.level_1;
        data.l2 = res.level_2;
    }

    if( !data.r1->empty() ) data.id1 = idstr1;
    if( !data.r2->empty() ) data.id2 = idstr2;
}

//------------------------------------------------------------------------------
const char * const CSRSearchApplication::USAGE_LINE = 
    "Search for close matches to short sequences.";

//------------------------------------------------------------------------------
void CSRSearchApplication::Init()
{
    auto_ptr< CArgDescriptions > arg_desc( new CArgDescriptions );
    arg_desc->SetUsageContext( 
            GetArguments().GetProgramBasename(), USAGE_LINE );
    arg_desc->AddKey( 
            "input", "input_file_name", "input file name",
            CArgDescriptions::eString );
    arg_desc->AddOptionalKey(
            "input1", "paired_input_file_name",
            "file containing query sequence pairs",
            CArgDescriptions::eString );
    arg_desc->AddOptionalKey(
            "output", "output_file_name", "output file name",
            CArgDescriptions::eString );
    arg_desc->AddOptionalKey(
            "pair_distance", "pair_distance",
            "distance between query pairs",
            CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey(
            "pair_distance_fuzz", "pair_distance_fuzz",
            "how much deviation from pair_distance is allowed",
            CArgDescriptions::eInteger );
    arg_desc->AddDefaultKey(
            "mismatch", "allow_mismatch",
            "flag to allow one mismatch",
            CArgDescriptions::eBoolean, "false" );
    arg_desc->AddDefaultKey(
            "nomap", "no_mmap_index",
            "read index rather than mmap()'ing it.",
            CArgDescriptions::eBoolean, "false" );
    arg_desc->AddKey(
            "index", "index_name", "index file name",
            CArgDescriptions::eString );
    arg_desc->AddDefaultKey(
            "start_vol", "index_volume",
            "the first index volume to process",
            CArgDescriptions::eInteger, "0" );
    arg_desc->AddDefaultKey(
            "end_vol", "index_volume",
            "one past the last index volume to process",
            CArgDescriptions::eInteger, "0" );
    arg_desc->AddDefaultKey(
            "restrict_matches", "number_of_matches",
            "restrict the number of matches per query to at most this number",
            CArgDescriptions::eInteger, "3" );
    arg_desc->AddDefaultKey(
            "noid", "use_ordinal",
            "use ordinal numbers for queries and database in output",
            CArgDescriptions::eBoolean, "false" );
    SetupArgDescriptions( arg_desc.release() );
}

typedef CSequenceIStream::TSeqData TSeqData;

CRef<objects::CObjectManager> om;

//------------------------------------------------------------------------------
string MakeIndexName( const string & prefix, Uint4 vol )
{
    char volstr[3];
    volstr[2] = 0;
    snprintf( volstr, 3, "%02d", vol );
    return prefix + "." + volstr + ".idx";
}

//------------------------------------------------------------------------------
void PrintResults( 
        CNcbiOstream & ostream, const vector< string > & idmap,
        CDbIndex::TSeqNum qnum, 
        const vector< CSRSearch::SResultData > & results, 
        const string & qidstr1, const string & qidstr2 )
{
    typedef vector< CSRSearch::SResultData > TRes;

    for( TRes::const_iterator i = results.begin();
            i != results.end(); ++i ) {
        const string & qidstr = (i->type == 2) ? qidstr2 : qidstr1;
        ostream << (int)i->type << "\t";

        if( qidstr.empty() ) {
            ostream << qnum << "\t" << i->snum << "\t";
        }
        else {
            ostream << qidstr << "\t";

            if( i->snum < idmap.size() ) ostream << idmap[i->snum] << "\t";
            else ostream << "unknown" << "\t";
        }

        ostream << i->spos_1      << "\t"
                << ((i->fw_strand_1 == 0) ? '-' : '+') << "\t"
                << i->mpos_1      << "\t"
                << (char)i->mbase_1;

        if( i->pair ) {
            ostream                   << "\t"
                    << i->spos_2      << "\t"
                    << ((i->fw_strand_2 == 0) ? '-' : '+') << "\t"
                    << i->mpos_2      << "\t"
                    << (char)i->mbase_2;
        }

        ostream << "\n";
    }
}

//------------------------------------------------------------------------------
CSeqVector ExtractSeqVector( TSeqData & sd, bool noid, string & idstr )
{
    objects::CSeq_entry * entry = sd.seq_entry_.GetPointerOrNull();

    if( entry == 0 || 
            entry->Which() != objects::CSeq_entry_Base::e_Seq ) {
        NCBI_THROW( 
                CDbIndex_Exception, eBadOption, 
                "input seq-entry is NULL or not a sequence" );
    }

    if( !noid ){
        objects::CScope scope( *om );
        objects::CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry( *entry );
        objects::CBioseq_Handle bsh = seh.GetSeq();
        idstr = objects::sequence::GetTitle( bsh );
        Uint4 pos = idstr.find_first_of( " \t" );
        idstr = idstr.substr( 0, pos );
    }

    return CSeqVector( 
            entry->SetSeq(), 0, objects::CBioseq_Handle::eCoding_Iupac );
}

//------------------------------------------------------------------------------
int CSRSearchApplication::Run()
{ 
    om.Reset( objects::CObjectManager::GetInstance() );
    CNcbiOstream * ostream = 0;

    if( GetArgs()["output"] ) {
        ostream = new CNcbiOfstream( GetArgs()["output"].AsString().c_str() );
    }
    else ostream = &NcbiCout;

    Uint4 pd = 0, pdfuzz = 0;

    if( GetArgs()["input1"] ) {
        if( !GetArgs()["pair_distance"] ) {
            ERR_POST( Error << "-pair_distance must be provided for paired input" );
            exit( 1 );
        }

        pd = GetArgs()["pair_distance"].AsInteger();

        if( GetArgs()["pair_distance_fuzz"] )
            pdfuzz = GetArgs()["pair_distance_fuzz"].AsInteger();
        else pdfuzz = pd/2;

        if( pdfuzz > pd ) {
            ERR_POST( Error << "the value of -pair_distance_fuzz can not be greater "
                            << "than the value of -pair_distance" );
            exit( 1 );
        }
    }

    string index_prefix = GetArgs()["index"].AsString();

    Uint4 start_vol = GetArgs()["start_vol"].AsInteger();
    Uint4 end_vol   = GetArgs()["end_vol"].AsInteger();

    bool noid = GetArgs()["noid"].AsBoolean();

    Uint4 nr = GetArgs()["restrict_matches"].AsInteger();
    bool use_cache = (nr != 0);
    if( nr == 0 ) nr = 0xFFFFFFFF;
    bool mismatch = GetArgs()["mismatch"].AsBoolean();
    bool nomap = GetArgs()["nomap"].AsBoolean();

    CRCache rcache( nr );

    while( true ) {
        string index_name = MakeIndexName( index_prefix, start_vol );
        cerr << "searching volume " << index_name << endl;
        CRef< CDbIndex > index( null );
        try {
            index = CDbIndex::Load( index_name, nomap );
        }
        catch( ... ) {}
        if( index == 0 ) break;
        rcache.updateSIdMap( index->getIdMap(), index->getStartOId() );
        CRef< CSRSearch > search_obj = 
            CSRSearch::MakeSRSearch( index, pd, pdfuzz );
        CSequenceIStream * iseqstream = new CSequenceIStreamFasta( 
                ( GetArgs()["input"].AsString() ) );
        CSequenceIStream * iseqstream1 = 0;

        if( GetArgs()["input1"] ) {
            iseqstream1 = new CSequenceIStreamFasta(
                    GetArgs()["input1"].AsString() );
        }

        bool paired = (iseqstream1 != 0);
        CDbIndex::TSeqNum seq_counter = 0;

        while( true ) {
            CRef< TSeqData > seq_data( iseqstream->next() );
            TSeqData * sd = seq_data.GetNonNullPointer();
            if( !*sd ) break;
            CSRSearch::TResults results;
            string qidstr1, qidstr2;
            CSeqVector seq = ExtractSeqVector( *sd, noid, qidstr1 );
            CSeqVector seq1;
            Uint4 s1 = rcache.getNRes1( seq_counter );
            Uint4 s2 = rcache.getNRes2( seq_counter );
            CSRSearch::ELevel l1 = rcache.getLevel1( seq_counter );
            CSRSearch::ELevel l2 = rcache.getLevel2( seq_counter );
            
            if( paired ) {
                CRef< TSeqData > seq_data1( iseqstream1->next() );
                TSeqData * sd1 = seq_data1.GetNonNullPointer();

                if( !*sd1 ) {
                    ERR_POST( Error << "failed to read a pair to sequence " 
                                    << seq_counter );
                    exit( 1 );
                }

                seq1 = ExtractSeqVector( *sd1, noid, qidstr2 );
                CSRSearch::SSearchData sdata( 
                        seq, seq1, nr, s1, s2, l1, l2, !mismatch );
                search_obj->search( sdata, results );
            }
            else {
                CSRSearch::SSearchData sdata( 
                        seq, nr, s1, s2, l1, l2, !mismatch );
                search_obj->search( sdata, results );
            }

            if( use_cache ) {
                rcache.update( seq_counter, results, qidstr1, qidstr2 );
            }
            else {
                PrintResults( *ostream, rcache.getSIdMap(), seq_counter, results.res, qidstr1, qidstr2 );
            }

            ++seq_counter;

            if( seq_counter%100000 == 0 ) {
                cerr << seq_counter << " sequences processed" << endl;
            }
        }

        if( ++start_vol == end_vol ) break;
    }

    if( use_cache ) rcache.dump( *ostream );
    *ostream << flush;
    return 0;
}

