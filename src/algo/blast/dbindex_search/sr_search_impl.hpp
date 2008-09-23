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
 *   Private declarations and template code for CSRSearch.
 *
 */

#ifndef C_SR_SEARCH_IMPL_HPP
#define C_SR_SEARCH_IMPL_HPP

#include <algo/blast/dbindex/dbindex_sp.hpp>
#include <algo/blast/dbindex_search/sr_search.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( dbindex_search )

USING_SCOPE( ncbi::objects );
USING_SCOPE( ncbi::blastdbindex );

//----------------------------------------------------------------------------
template< typename index_t >
class CSRSearch_Impl : public CSRSearch
{
    typedef index_t TIndex;

    public:

        CSRSearch_Impl( CRef< CDbIndex > index, TSeqPos d, TSeqPos dfuzz )
            : CSRSearch( index, d, dfuzz ),
              index_impl_( dynamic_cast< TIndex & >(*index) )
        {}

    private:

        typedef typename TIndex::TOffsetIterator TIter;

        virtual void search( const SSearchData & sdata, TResults & results );

        void copyOffsets( TSRResults & resutls, TIter & offsets );

        void mergeOffsets( 
                TSRResults & resutls, TIter & offsets, Uint4 step );

        void setResults4Idx( 
                Uint4 idx, bool fw_strand,
                CResCache & res_cache, vector< TIter > & single_res_cache,
                const vector< TSeqPos > & positions );

        bool searchExact( 
                const CSeqVector & seq,
                const vector< TSeqPos > & positions,
                bool fw_strand,
                TSRResults & results,
                TSeqPos & max_pos,
                vector< TIter > & cache,
                vector< Uint1 > & cache_set );

        bool searchOneMismatch(
                const CSeqVector & seq,
                const vector< TSeqPos > & positions,
                Uint4 max_pos,
                bool fw_strand,
                CMismatchResultsInfo & results_info,
                vector< TIter > & scache,
                const vector< Uint1 > & scache_set,
                CResCache & cache );

        TIndex & index_impl_;
        vector< TIter > single_res_cache[2];
        vector< Uint1 > single_res_cache_set[2];
        vector< TIter > scache1[2];
        vector< TIter > scache2[2];
        vector< Uint1 > scache_set1[2];
        vector< Uint1 > scache_set2[2];
};

//----------------------------------------------------------------------------
template< typename index_t >
void CSRSearch_Impl< index_t >::setResults4Idx( 
        Uint4 idx, bool fw_strand, CResCache & res_cache, 
        vector< TIter > & single_res_cache,
        const vector< TSeqPos > & positions )
{
    res_cache.set( idx, fw_strand );
    TSRResults & r = res_cache.at( idx, fw_strand );

    if( idx != single_res_cache.size() ) {
        for( Uint4 i = 0; i < single_res_cache.size(); ++i )
            if( i != idx ) {
                Uint4 pos = positions[i];
                TIter & s = single_res_cache[i];

                if( !s.end() ) {
                    if( r.empty() ) copyOffsets( r, s );
                    else {
                        Uint4 p = (idx == 0) ? pos - hkey_width_ : pos;
                        mergeOffsets( r, s, p );
                        if( r.empty() ) break;
                    }
                }
                else { r.clear(); break; }
            }
    }
    else {
        for( Uint4 i = 0, pos = 0; 
                i < single_res_cache.size() - 2; ++i, pos += hkey_width_ ) {
            TIter & s = single_res_cache[i];

            if( !s.end() ) {
                if( r.empty() ) copyOffsets( r, s );
                else {
                    mergeOffsets( r, s, pos );
                    if( r.empty() ) break;
                }
            }
            else { r.clear(); break; }
        }
    }
}

//----------------------------------------------------------------------------
template< typename index_t >
void CSRSearch_Impl< index_t >::copyOffsets( 
        TSRResults & results, TIter & offsets )
{
    while( offsets.Advance() ) {
        CDbIndex::TOffsetValue offset = offsets.getOffsetValue();
        SSRResult r = { index_impl_.getLId( offset ), 
                        index_impl_.getLOff( offset ) };
        results.push_back( r );
    }

    offsets.Reset();
}

//----------------------------------------------------------------------------
template< typename index_t >
void CSRSearch_Impl< index_t >::mergeOffsets( 
        TSRResults & results, TIter & offsets, Uint4 step )
{
    if( results.size() > 0 ) {
        Uint4 put_ind = 0;

        if( !offsets.end() ) {
            offsets.Advance();

            for( TSRResults::iterator ires = results.begin();
                    ires != results.end(); ++ires ) {
                SSRResult offres = { 0, 0 };
    
                while( true ){
                    CDbIndex::TOffsetValue o = offsets.getOffsetValue();
                    offres.seqnum = index_impl_.getLId( o );
                    offres.soff   = index_impl_.getLOff( o );

                    if( offres.seqnum > ires->seqnum ||
                            ( offres.seqnum == ires->seqnum &&
                            offres.soff >= ires->soff + step ) )
                        break;
                    else if( !offsets.Advance() ) break;
                }

                if( offsets.end() ) break;

                if( offres.seqnum == ires->seqnum &&
                        offres.soff == ires->soff + step )
                    results[put_ind++] = *ires;
            }
        }

        results.resize( put_ind );
    }

    offsets.Reset();
}

//----------------------------------------------------------------------------
template< typename index_t >
bool CSRSearch_Impl< index_t >::searchExact( 
        const CSeqVector & seq, 
        const vector< TSeqPos > & positions,
        bool fw_strand, TSRResults & results, TSeqPos & max_pos,
        vector< TIter > & cache, vector< Uint1 > & cache_set )
{
    bool ambig;
    Uint4 cache_idx = 0;
    TSeqPos i = 0, pos = positions[0];
    Uint4 key = getNMer( seq, pos, fw_strand, ambig );
    if( ambig ) return true;
    cache_set[cache_idx] = true;
    TIter & offsets = cache[cache_idx];
    offsets = index_impl_.OffsetIterator( key, hkey_width_ );

    if( !offsets.end() ) {
        ++cache_idx;
        copyOffsets( results, offsets );

        while( (++i) != positions.size() ) {
            pos = positions[i];
            key = getNMer( seq, pos, fw_strand, ambig );
            if( ambig ) return true;
            TIter & offsets = cache[cache_idx];
            offsets = index_impl_.OffsetIterator( key, hkey_width_ );
            mergeOffsets( results, offsets, pos );
            cache_set[cache_idx] = true;
            if( offsets.end() ) max_pos = pos + hkey_width_;
            ++cache_idx;
        }
    }
    else max_pos = hkey_width_;

    return false;
}

//----------------------------------------------------------------------------
template< typename index_t >
bool CSRSearch_Impl< index_t >::searchOneMismatch( 
        const CSeqVector & seq, const vector< TSeqPos > & positions,
        Uint4 max_pos, bool fw_strand, CMismatchResultsInfo & results_info,
        vector< TIter > & scache, const vector< Uint1 > & scache_set,
        CResCache & cache )
{
    bool ambig;
    results_info.clear();
    static const Uint1 letters[4] = {'A', 'C', 'G', 'T' };

    for( TSeqPos i = 0; i < positions.size(); ++i )
        if( !scache_set[i] ) {
            Uint4 key = getNMer( seq, positions[i], fw_strand, ambig );
            if( ambig ) return true;
            scache[i] = index_impl_.OffsetIterator( key, hkey_width_ );
        }

    for( TSeqPos i = 0; i < max_pos; ) {
        SMismatchInfo mismatch_info;
        pair< TSeqPos, TSeqPos > range = 
            Pos2Index( i, seq.size(), mismatch_info );

        if( !cache.is_set( mismatch_info.idx, fw_strand ) ) {
            setResults4Idx(
                    mismatch_info.idx, fw_strand, 
                    cache, scache, positions );
        }

        TSRResults & r = cache.at( mismatch_info.idx, fw_strand );
        if( r.empty() ) { i = range.second; continue; }

        for( i = range.first; i < range.second; ++i ) {
            TSeqPos p = fw_strand ? i : seq.size() - i - 1;
            Uint1 orig_letter = seq[p];

            for( Uint4 j = 0; j < 4; ++j )
                if( letters[j] != orig_letter ) {
                    Uint1 subst_letter = letters[j];
                    Uint4 key = getNMer( 
                            seq, mismatch_info.key_pos[0],
                            fw_strand, ambig, i, subst_letter );
                    if( ambig ) return true;
                    TIter offsets = 
                        index_impl_.OffsetIterator( key, hkey_width_ );

                    if( !offsets.end() ) {
                        Uint4 ris = results_info.size();
                        SMismatchResultsEntry & r_entry = results_info[ris];
                        r_entry.init( i, subst_letter, mismatch_info.key_pos[0] );
                        TSRResults & results = r_entry.results;
                        results.clear();
                        copyOffsets( results, offsets );

                        if( mismatch_info.num_keys == 2 ) {
                            key = getNMer( 
                                    seq, mismatch_info.key_pos[1],
                                    fw_strand, ambig, i, subst_letter );
                            if( ambig ) return true;
                            offsets = 
                                index_impl_.OffsetIterator( 
                                        key, hkey_width_ );
                            mergeOffsets( 
                                    results, offsets,
                                    mismatch_info.key_pos[1] -
                                        mismatch_info.key_pos[0] );
                        }

                        if( mismatch_info.idx == 0 )
                            mergeResults( results, r, hkey_width_ );
                        else {
                            mergeResults( 
                                    results, r, -mismatch_info.key_pos[0] );
                        }

                        if( results.empty() ) results_info.resize( ris );
                    }
                }
        }
    }

    return false;
}

//----------------------------------------------------------------------------
template< typename index_t >
void CSRSearch_Impl< index_t >::search( 
        const SSearchData & sdata, TResults & outres )
{
    bool paired = (sdata.seq_2 != 0);
    hk_data_.reset();

    if( paired ) {
        CResCache & res_cache1 = hk_data_.rc1;
        CResCache & res_cache2 = hk_data_.rc2;
        const CSeqVector & seq1 = *sdata.seq_1;
        const CSeqVector & seq2 = *sdata.seq_2;
        TSeqPos sz1 = seq1.size();
        TSeqPos sz2 = seq2.size();
        vector< TSeqPos > positions1 = GetQNmerPositions( sz1 );
        vector< TSeqPos > positions2 = GetQNmerPositions( sz2 );

        if( positions1.empty() || positions2.empty() ) return;

        TSeqPos maxpos1[2] = { sz1, sz1 };
        TSeqPos maxpos2[2] = { sz2, sz2 };
        bool ambig;

        ELevel l = PE;
        Uint4 nr = (l < sdata.rlevel_1) ? sdata.num_res 
                                        : sdata.num_res - sdata.nr_1;
        outres.level_1 = l;
        outres.level_2 = l;
        outres.nres_1 = 0;

        for( Uint1 strand = 0; strand < 2; ++strand ) {
            scache1[strand].clear();
            scache_set1[strand].clear();
            scache1[strand].resize( positions1.size() );
            scache_set1[strand].resize( positions1.size(), 0 );
            hk_data_.exact_1[strand].clear();
            ambig = searchExact( 
                    seq1, positions1, strand, 
                    hk_data_.exact_1[strand], maxpos1[strand], 
                    scache1[strand], scache_set1[strand] );
            if( ambig ) break;
            scache2[strand].clear();
            scache_set2[strand].clear();
            scache2[strand].resize( positions2.size() );
            scache_set2[strand].resize( positions2.size(), 0 );
            hk_data_.exact_2[strand].clear();
            ambig = searchExact( 
                    seq2, positions2, strand, 
                    hk_data_.exact_2[strand], maxpos2[strand], 
                    scache2[strand], scache_set2[strand] );
            if( ambig ) break;
        }

        if( ambig ) return;

        bool matches_found = false;

        for( Uint1 i = 0; i < 2; ++i )
            if( !hk_data_.exact_1[i].empty() )
                for( Uint1 j = 0; j < 2; ++j )
                    if( !hk_data_.exact_2[j].empty() ) {
                        TSRPairedResults & results = hk_data_.pres;
                        results.clear();
                        combine( 
                                hk_data_.exact_1[i], hk_data_.exact_2[j], 
                                results );

                        if( !results.empty() ) {
                            matches_found = true;

                            if( reportResults( 
                                        outres, nr, 
                                        sz1, sz2, 
                                        results, i, j ) ) {
                                outres.nres_1 = outres.res.size();
                                return;
                            }
                        }
                    }

        const bool & mismatch = sdata.mismatch;

        if( mismatch && !matches_found ) {
            matches_found = false;
            res_cache1.init( positions1.size() + 1 );
            res_cache2.init( positions2.size() + 1 );
            bool ambig;

            l = PM;
            outres.level_1 = l;
            outres.nres_1 = 0;
            if( l > sdata.rlevel_1 ) return;
            nr = (l < sdata.rlevel_1) ? sdata.num_res 
                                      : sdata.num_res - sdata.nr_1;

            for( Uint1 strand = 0; strand < 2; ++strand ) {
                ambig = searchOneMismatch(
                        seq1, positions1, maxpos1[strand], strand,
                        hk_data_.mm_1[strand],
                        scache1[strand], scache_set1[strand],
                        res_cache1 );
                if( ambig ) break;
                ambig = searchOneMismatch(
                        seq2, positions2, maxpos2[strand], strand,
                        hk_data_.mm_2[strand],
                        scache2[strand], scache_set2[strand],
                        res_cache2 );
                if( ambig ) break;
            }

            if( ambig ) return;

            for( Uint1 i = 0; i < 2; ++i ) {
                if( !hk_data_.exact_1[i].empty() ) {
                    for( Uint1 j = 0; j < 2; ++j ) {
                        if( !hk_data_.mm_2[j].empty() ) {
                            // exact1 vs mm2
                            for( Uint4 ind = 0; ind < hk_data_.mm_2[j].size(); ++ind ) {
                                SMismatchResultsEntry & it = hk_data_.mm_2[j][ind];
                                TSRPairedResults & results = hk_data_.pres;
                                results.clear();
                                combine( 
                                        hk_data_.exact_1[i], it.results, 
                                        results, -it.adjustment );

                                if( !results.empty() ) {
                                    matches_found = true;   

                                    if( reportResults( 
                                            outres, nr,
                                            sz1, sz2, results, i, j,
                                            false, true,
                                            0, it.mismatch_position,
                                            (Uint1)'-', it.mismatch_letter,
                                            0, it.adjustment ) ) {
                                        outres.nres_1 = outres.res.size();
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }

                if( !hk_data_.mm_1[i].empty() ) {
                    for( Uint1 j = 0; j < 2; ++j ) {
                        if( !hk_data_.exact_2[j].empty() ) {
                            // mm1 vs exact2
                            for( Uint4 ind = 0; ind < hk_data_.mm_1[i].size(); ++ind ) {
                                SMismatchResultsEntry & it = hk_data_.mm_1[i][ind];
                                TSRPairedResults & results = hk_data_.pres;
                                results.clear();
                                combine( 
                                        it.results, hk_data_.exact_2[j],
                                        results, it.adjustment );

                                if( !results.empty() ) {
                                    matches_found = true;   

                                    if( reportResults( 
                                            outres, nr,
                                            sz1, sz2, results, i, j,
                                            true, false,
                                            it.mismatch_position, 0,
                                            it.mismatch_letter, (Uint1)'-',
                                            it.adjustment, 0 ) ) {
                                        outres.nres_1 = outres.res.size();
                                        return;
                                    }
                                }
                            }
                        }

                        if( !hk_data_.mm_2[j].empty() ) {
                            // mm1 vs mm2
                            for( Uint4 iind = 0; iind < hk_data_.mm_1[i].size(); ++iind ) {
                                for( Uint4 jind = 0; jind < hk_data_.mm_2[j].size(); ++jind ) {
                                    SMismatchResultsEntry & it = hk_data_.mm_1[i][iind];
                                    SMismatchResultsEntry & jt = hk_data_.mm_2[j][jind];
                                    TSRPairedResults & results = hk_data_.pres;
                                    results.clear();
                                    combine(
                                            it.results, jt.results,
                                            results, 
                                            it.adjustment - jt.adjustment );

                                    if( !results.empty() ) {
                                        matches_found = true;   

                                        if( reportResults(
                                                outres, nr,
                                                sz1, sz2, results, i, j,
                                                true, true,
                                                it.mismatch_position,
                                                jt.mismatch_position,
                                                it.mismatch_letter,
                                                jt.mismatch_letter,
                                                it.adjustment,
                                                jt.adjustment ) ) {
                                            outres.nres_1 = outres.res.size();
                                            return;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if( !matches_found ) {
            matches_found = false;

            l = SE;
            outres.level_1 = l;
            outres.level_2 = EM;
            outres.nres_1 = 0;

            if( l <= sdata.rlevel_1 ) {
                nr = (l < sdata.rlevel_1) ? sdata.num_res 
                                          : sdata.num_res - sdata.nr_1;

                for( Uint1 i = 0; i < 2; ++i )
                    if( !hk_data_.exact_1[i].empty() ) {
                        matches_found = true;

                        if( reportResults( 
                                outres, nr,
                                sz1, hk_data_.exact_1[i], i, 
                                false, 0, (Uint1)'-', 0, 1 ) ) {
                            break;
                        }
                    }

                if( !matches_found ) {
                    l = SM;
                    outres.level_1 = l;

                    if( l <= sdata.rlevel_1 ) {
                        nr = (l < sdata.rlevel_1) ? sdata.num_res 
                                                  : sdata.num_res - sdata.nr_1;

                        for( Uint1 i = 0; i < 2; ++i ) {
                            if( !hk_data_.mm_1[i].empty() ) {
                                matches_found = true;

                                for( Uint4 ind = 0; ind < hk_data_.mm_1[i].size(); ++ind ) {
                                    SMismatchResultsEntry & it = hk_data_.mm_1[i][ind];

                                    if( reportResults(
                                            outres, nr,
                                            sz1, it.results, i, 
                                            mismatch, it.mismatch_position, 
                                            it.mismatch_letter, it.adjustment, 
                                            1 ) ) {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if( !matches_found ) outres.level_1 = EM;
            outres.nres_1 = outres.res.size();
            matches_found = false;

            l = SE;
            outres.level_2 = l;
            if( l > sdata.rlevel_2 ) return;
            nr = (l < sdata.rlevel_2) ? sdata.num_res 
                                      : sdata.num_res - sdata.nr_2;
            nr += outres.res.size();

            for( Uint1 i = 0; i < 2; ++i )
                if( !hk_data_.exact_2[i].empty() ) {
                    if( reportResults( 
                            outres, nr, 
                            sz2, hk_data_.exact_2[i], i,
                            false, 0, (Uint1)'-', 0, 2 ) ) {
                        return;
                    }

                    matches_found = true;
                }

            if( !matches_found ) {
                l = SM;
                outres.level_2 = l;
                if( l > sdata.rlevel_2 ) return;
                nr = (l < sdata.rlevel_2) ? sdata.num_res 
                                          : sdata.num_res - sdata.nr_2;
                nr += outres.res.size();

                for( Uint1 i = 0; i < 2; ++i ) {
                    if( !hk_data_.mm_2[i].empty() )
                        for( Uint4 ind = 0; ind < hk_data_.mm_2[i].size(); ++ind ) {
                            SMismatchResultsEntry & it = hk_data_.mm_2[i][ind];

                            if( reportResults(
                                    outres, nr,
                                    sz2, it.results, i, 
                                    mismatch, it.mismatch_position, 
                                    it.mismatch_letter, it.adjustment, 
                                    2 ) ) {
                                return;
                            }
                        }
                }
            }
        }
        else {
            outres.nres_1 = outres.res.size();
        }
    }
    else { // not paired
        CResCache & res_cache = hk_data_.rc1;
        const bool & mismatch = sdata.mismatch;
        const CSeqVector & seq = *sdata.seq_1;
        TSeqPos sz = seq.size();
        vector< TSeqPos > positions = GetQNmerPositions( sz );
        bool exact_matches_found = false;
        TSeqPos max_mismatch_pos[2] = { sz, sz };

        ELevel l = SE;
        outres.level_1 = l;
        outres.level_2 = EM;
        outres.nres_1 = 0;
        if( l > sdata.rlevel_1 ) return;
        Uint4 nr = (l < sdata.rlevel_1) ? sdata.num_res 
                                        : sdata.num_res - sdata.nr_1;

        if( !positions.empty() ) {
            bool fw_strand = true;
            res_cache.init( positions.size() + 1 );
            bool ambig;

            do {
                single_res_cache[fw_strand].clear();
                single_res_cache_set[fw_strand].clear();
                single_res_cache[fw_strand].resize( positions.size() );
                single_res_cache_set[fw_strand].resize( 
                        positions.size(), false );

                TSRResults & results = hk_data_.exact_1[0];
                results.clear();
                ambig = searchExact(
                        seq, positions, fw_strand, results, 
                        max_mismatch_pos[fw_strand],
                        single_res_cache[fw_strand],
                        single_res_cache_set[fw_strand] );
                if( ambig ) break;

                if( reportResults( outres, nr, sz, results, fw_strand ) ) {
                    outres.nres_1 = outres.res.size();
                    return;
                }

                fw_strand = !fw_strand;
                if( !results.empty() ) exact_matches_found = true;
            }while( !fw_strand );

            if( ambig ) return;
        }

        if( mismatch && !exact_matches_found ) {
            bool fw_strand = true;
            bool ambig;

            l = SM;
            outres.level_1 = l;
            outres.nres_1 = 0;
            if( l > sdata.rlevel_1 ) return;
            nr = (l < sdata.rlevel_1) ? sdata.num_res 
                                      : sdata.num_res - sdata.nr_1;

            do {
                fw_strand = !fw_strand;
                CMismatchResultsInfo & results_info = hk_data_.mm_1[fw_strand];
                ambig = searchOneMismatch( 
                        seq, positions, 
                        max_mismatch_pos[fw_strand], fw_strand,
                        results_info,
                        single_res_cache[fw_strand],
                        single_res_cache_set[fw_strand],
                        res_cache );
                if( ambig ) break;

                for( Uint4 ind = 0; ind < results_info.size(); ++ind ) {
                    SMismatchResultsEntry & it = results_info[ind];
                    if( reportResults( 
                            outres, nr,
                            sz, it.results, fw_strand, mismatch, 
                            it.mismatch_position, it.mismatch_letter,
                            it.adjustment ) ) {
                        outres.nres_1 = outres.res.size();
                        return;
                    }
                }
            }while( !fw_strand );

            if( ambig ) return;
        }

        outres.nres_1 = outres.res.size();
    }
}

END_SCOPE( dbindex_search )
END_NCBI_SCOPE

#endif

