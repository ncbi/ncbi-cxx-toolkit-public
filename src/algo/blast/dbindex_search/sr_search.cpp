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
 *   Implementation file for CSRSearch and some related declarations.
 *
 */

#include <ncbi_pch.hpp>

#include <algo/blast/dbindex_search/sr_search.hpp>

#include "sr_search_impl.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE( dbindex_search )

USING_SCOPE( ncbi::objects );
USING_SCOPE( ncbi::blastdbindex );

//----------------------------------------------------------------------------
CRef< CSRSearch > CSRSearch::MakeSRSearch( 
        CRef< CDbIndex > index, TSeqPos d, TSeqPos dfuzz )
{
    if( index->isLegacy() ) {
        return CRef< CSRSearch >( 
                new CSRSearch_Impl< CDbIndex_Impl< true > >( 
                    index, d, dfuzz ) );
    }
    else {
        return CRef< CSRSearch >( 
                new CSRSearch_Impl< CDbIndex_Impl< false > >( 
                    index, d, dfuzz ) );
    }
}

//----------------------------------------------------------------------------
vector< TSeqPos > CSRSearch::GetQNmerPositions( TSeqPos sz ) const
{
    vector< TSeqPos > result;

    if( sz >= hkey_width_ ) {
        for( TSeqPos cpos = 0; cpos < sz; cpos += hkey_width_ ) {
            if( cpos + hkey_width_ <= sz ) {
                result.push_back( cpos );
            }
            else {
                result.push_back( sz - hkey_width_ );
                break;
            }
        }
    }

    return result;
}

//----------------------------------------------------------------------------
Uint4 CSRSearch::getNMer( 
        const CSeqVector & seq, TSeqPos pos, bool fw, bool & ambig ) const
{
    Uint4 result = 0;
    ambig = false;
    
    if( fw ) {
        TSeqPos end = pos + hkey_width_;

        for( ; pos < end ; ++pos ) {
            Uint4 letter = 0;

            switch( seq[pos] ) {
                case 'A': case 'a': letter = 0; break;
                case 'C': case 'c': letter = 1; break;
                case 'G': case 'g': letter = 2; break;
                case 'T': case 't': letter = 3; break;
                default: ambig = true; return result;
            }
    
            result = (result<<2) + letter;
        }
    }
    else {
        TSeqPos l = seq.size();
        TSeqPos end = pos + hkey_width_;

        for( ; pos < end ; ++pos ) {
            Uint4 letter = 0;

            switch( seq[l - pos - 1] ) {
                case 'A': case 'a': letter = 3; break;
                case 'C': case 'c': letter = 2; break;
                case 'G': case 'g': letter = 1; break;
                case 'T': case 't': letter = 0; break;
                default: ambig = true; return result;
            }

            result = (result<<2) + letter;
        }
    }

    return result;
}

//----------------------------------------------------------------------------
Uint4 CSRSearch::getNMer( 
        const CSeqVector & seq, TSeqPos pos, bool fw, bool & ambig,
        TSeqPos sub_pos, Uint1 sub_letter ) const
{
    Uint4 result = 0;
    ambig = false;
    
    if( fw ) {
        TSeqPos end = pos + hkey_width_;

        for( ; pos < end ; ++pos ) {
            Uint4 letter = 0;

            if( pos == sub_pos ) {
                switch( sub_letter ) {
                    case 'A': case 'a': letter = 0; break;
                    case 'C': case 'c': letter = 1; break;
                    case 'G': case 'g': letter = 2; break;
                    case 'T': case 't': letter = 3; break;
                    default: ambig = true; return result;
                }
            }
            else {
                switch( seq[pos] ) {
                    case 'A': case 'a': letter = 0; break;
                    case 'C': case 'c': letter = 1; break;
                    case 'G': case 'g': letter = 2; break;
                    case 'T': case 't': letter = 3; break;
                    default: ambig = true; return result;
                }
            }
    
            result = (result<<2) + letter;
        }
    }
    else {
        TSeqPos l = seq.size();
        TSeqPos end = pos + hkey_width_;

        for( ; pos < end ; ++pos ) {
            Uint4 letter = 0;

            if( sub_pos == pos ) {
                switch( sub_letter ) {
                    case 'A': case 'a': letter = 3; break;
                    case 'C': case 'c': letter = 2; break;
                    case 'G': case 'g': letter = 1; break;
                    case 'T': case 't': letter = 0; break;
                    default: ambig = true; return result;
                }
            }
            else {
                switch( seq[l - pos - 1] ) {
                    case 'A': case 'a': letter = 3; break;
                    case 'C': case 'c': letter = 2; break;
                    case 'G': case 'g': letter = 1; break;
                    case 'T': case 't': letter = 0; break;
                    default: ambig = true; return result;
                }
            }

            result = (result<<2) + letter;
        }
    }

    return result;
}

//----------------------------------------------------------------------------
pair< TSeqPos, TSeqPos > CSRSearch::Pos2Index( 
        TSeqPos pos, TSeqPos sz, SMismatchInfo & mismatch_info ) const
{
    pair< TSeqPos, TSeqPos > result;
    Uint4 div = pos/hkey_width_;

    if( sz%hkey_width_ == 0 ) {
        result.first = div*hkey_width_;
        result.second = result.first + hkey_width_;
        mismatch_info.idx = div;
        mismatch_info.num_keys = 1;
        mismatch_info.key_pos[0] = result.first;
    }
    else if( pos < sz - hkey_width_ ) {
        result.first = div*hkey_width_;
        result.second = 
            std::min( result.first + hkey_width_, sz - hkey_width_ );
        mismatch_info.idx = div;
        mismatch_info.num_keys = 1;
        mismatch_info.key_pos[0] = result.first;
    }
    else if( pos >= hkey_width_*(sz/hkey_width_) ) {
        result.first = hkey_width_*(sz/hkey_width_);
        result.second = sz;
        mismatch_info.idx = div;
        mismatch_info.num_keys = 1;
        mismatch_info.key_pos[0] = sz - hkey_width_;
    }
    else {
        result.first = sz - hkey_width_;
        result.second = hkey_width_*(sz/hkey_width_);
        mismatch_info.idx = (1 + sz/hkey_width_);
        mismatch_info.num_keys = 2;
        mismatch_info.key_pos[0] = div*hkey_width_;
        mismatch_info.key_pos[1] = result.first;
    }

    return result;
}

//----------------------------------------------------------------------------
void CSRSearch::mergeResults( 
        TSRResults & results, const TSRResults & src, Int4 step )
{
    if( results.size() > 0 ) {
        Uint4 put_ind = 0;

        if( src.size() > 0 ) {
            Uint4 test = (step < 0) ? -step : 0;
            TSRResults::const_iterator isrc = src.begin();

            for( TSRResults::iterator ires = results.begin();
                    ires != results.end(); ++ires ) {
                if( ires->soff < test ) continue;
                while( isrc != src.end() ) {
                    if( isrc->seqnum > ires->seqnum ||
                            ( isrc->seqnum == ires->seqnum &&
                              isrc->soff >= ires->soff + step ) )
                        break;

                    ++isrc;
                }

                if( isrc == src.end() ) break;

                if( isrc->seqnum == ires->seqnum &&
                        isrc->soff == ires->soff + step )
                    results[put_ind++] = *ires;
            }
        }

        results.resize( put_ind );
    }
}

//----------------------------------------------------------------------------
void CSRSearch::combine( 
        const TSRResults & results_1, const TSRResults & results_2,
        TSRPairedResults & results, Int4 adjust )
{
    unsigned long chunk_size = index_->getMaxChunkSize();
    unsigned long chunk_overlap = index_->getChunkOverlap();

    TSRResults::const_iterator i = results_1.begin();
    TSRResults::const_iterator j = results_2.begin();

    while( i != results_1.end() && j != results_2.end() ) {
        if( i->seqnum < j->seqnum ) { 
            if( i->seqnum + 1 == j->seqnum ) {
                TSeqNum is = index_->getSIdOffByLIdOff( i->seqnum, i->soff ).first;
                TSeqNum js = index_->getSIdOffByLIdOff( j->seqnum, j->soff ).first;
                
                if( is == js ) {
                    TSRResults::const_iterator k = j;

                    while( true ) {
                        TSeqPos s = k->soff + adjust + chunk_size - i->soff;

                        if( s <  chunk_overlap + dmax_ ) {
                            if( s >= chunk_overlap + dmin_ ) {
                                results.push_back( make_pair( *i, *k ) );
                            }

                            if( ++k == results_2.end() || 
                                k->seqnum != j->seqnum )
                            { break; }
                        }
                        else break;
                    }
                }
            }

            ++i; continue; 
        }
        else if( i->seqnum > j->seqnum ) { 
            if( i->seqnum == j->seqnum + 1 ) {
                TSeqNum is = index_->getSIdOffByLIdOff( i->seqnum, i->soff ).first;
                TSeqNum js = index_->getSIdOffByLIdOff( j->seqnum, j->soff ).first;

                if( is == js ) {
                    TSRResults::const_iterator k = i;

                    while( true ) {
                        TSeqPos s = k->soff + chunk_size - j->soff - adjust;

                        if( s <  chunk_overlap + dmax_ ) {
                            if( s >= chunk_overlap + dmin_ ) {
                                results.push_back( make_pair( *k, *j ) );
                            }

                            if( ++k == results_1.end() || 
                                k->seqnum != i->seqnum )
                            { break; }
                        }
                        else break;
                    }
                }
            }

            ++j; continue; 
        }
        else {
            TSeqPos jsoff = j->soff + adjust;

            while( i != results_1.end() && 
                   i->seqnum == j->seqnum && 
                   i->soff + dmax_ < jsoff ) 
                ++i;

            if( i == results_1.end() || i->seqnum != j->seqnum ) continue;
            TSRResults::const_iterator k = i;

            while( k != results_1.end() &&
                   k->seqnum == j->seqnum &&
                   k->soff + dmin_ <= jsoff ) {
                results.push_back( make_pair( *k, *j ) );
                ++k;
            }

            if( k == results_1.end() || k->seqnum != j->seqnum )
            { ++j; continue; }

            while( k != results_1.end() &&
                   k->seqnum == j->seqnum &&
                   k->soff < jsoff + dmin_ )
                ++k;

            if( k == results_1.end() || k->seqnum != j->seqnum )
            { ++j; continue; }

            while( k != results_1.end() &&
                   k->seqnum == j->seqnum &&
                   k->soff <= jsoff + dmax_ ) {
                results.push_back( make_pair( *k, *j ) );
                ++k;
            }

            ++j;
        }
    }
}

//----------------------------------------------------------------------------
pair< TSeqNum, TSeqPos > CSRSearch::MapSOff( 
        TSeqNum lid, TSeqPos loff, TSeqPos sz, bool & overlap ) const
{
    pair< TSeqNum, TSeqPos > cd = index_->getCIdOffByLIdOff( lid, loff );
    pair< TSeqNum, TSeqPos > sd = 
        index_->getSIdOffByCIdOff( cd.first, cd.second );
    overlap = false;

    if( cd.second + sz <= index_->getChunkOverlap() && 
        cd.first > 0 && 
        index_->getSIdByCId( cd.first - 1 ) == sd.first )
    {
        overlap = true;
    }

    sd.first = index_->getOIdBySId( sd.first );
    return sd;
}

//----------------------------------------------------------------------------
bool CSRSearch::reportResults( 
    TResults & outres, Uint4 nr,
    TSeqPos qsz, const TSRResults & results, bool fw, bool mismatch, 
    TSeqPos mismatch_pos, Uint1 mismatch_letter,
    Uint4 adjustment, Uint4 pos_in_pair )
{
    if( outres.res.size() == nr ) return true;

    if( mismatch ) mismatch_pos += 1;

    for( TSRResults::const_iterator cires = results.begin();
            cires != results.end(); ++cires ) {
        TSeqPos soff = cires->soff + 1 - hkey_width_ - adjustment;
        bool overlap;
        pair< TSeqNum, TSeqPos > m = 
            MapSOff( cires->seqnum, soff, qsz, overlap );
        if( overlap ) continue;
        Uint4 subj = m.first;
        SResultData r( 
                subj, 1 + m.second, fw, mismatch_pos,
                mismatch_letter, pos_in_pair );
        outres.res.push_back( r );
        if( outres.res.size() == nr ) return true;
    }

    return false;
}

//----------------------------------------------------------------------------
bool CSRSearch::reportResults(
    TResults & outres, Uint4 nr,
    TSeqPos qsz1, TSeqPos qsz2, const TSRPairedResults & results, 
    bool fw1, bool fw2, bool mismatch1, bool mismatch2,
    TSeqPos mismatch_pos1, TSeqPos mismatch_pos2,
    Uint1 mismatch_letter1, Uint1 mismatch_letter2,
    Uint4 adjustment1, Uint4 adjustment2 )
{
    if( outres.res.size() == nr ) return true;
    
    if( mismatch1 ) mismatch_pos1 += 1;
    if( mismatch2 ) mismatch_pos2 += 1;

    for( TSRPairedResults::const_iterator cires = results.begin();
            cires != results.end(); ++cires ) {
        const SSRResult & r1 = cires->first;
        const SSRResult & r2 = cires->second;
        TSeqPos soff1 = r1.soff + 1 - hkey_width_ - adjustment1;
        TSeqPos soff2 = r2.soff + 1 - hkey_width_ - adjustment2;
        bool overlap1, overlap2;
        pair< TSeqNum, TSeqPos > m1 = 
            MapSOff( r1.seqnum, soff1, qsz1, overlap1 );
        pair< TSeqNum, TSeqPos > m2 = 
            MapSOff( r2.seqnum, soff2, qsz2, overlap2 );
        if( overlap1 || overlap2 ) continue;
        
        if( m1.first == m2.first ) {
            SResultData r( 
                    0,
                    m1.first,
                    1 + m1.second, 1 + m2.second,
                    fw1, fw2, mismatch_pos1, mismatch_pos2,
                    mismatch_letter1, mismatch_letter2 );
            outres.res.push_back( r );
            if( outres.res.size() == nr ) return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------
const char * CSRSearch::InternalException::GetErrCodeString() const
{
    switch( GetErrCode() ) {
        case eAmbig: return "ambiguous base in the query";
        default:     return CException::GetErrCodeString();
    }
}

END_SCOPE( dbindex_search )
END_NCBI_SCOPE

