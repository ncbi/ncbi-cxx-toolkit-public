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
 *   Implementation file for CSymDustMasker class.
 *
 */

#include <ncbi_pch.hpp>

#include <algo/dustmask/symdust.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
CSymDustMasker::triplets::triplets( 
    const sequence_type & seq, size_type window, 
    lcr_list_type & lcr_list, thres_table_type & thresholds )
    : start_( 0 ), stop_( 0 ), max_size_( window - 2 ), 
      lcr_list_( lcr_list ), thresholds_( thresholds )
{
    triplet_type t = ((seq[0]&3)<<4) + ((seq[1]&3)<<2) + (seq[2]&3);
    triplet_list_.push_back( t );
}

//------------------------------------------------------------------------------
inline bool CSymDustMasker::triplets::add( sequence_type::value_type n )
{
    typedef std::vector< Uint1 > counts_type;
    typedef lcr_list_type::iterator lcr_iter_type;
    static counts_type counts( 64 );

    bool result = false;

    triplet_list_.push_front( 
        ((triplet_list_.front()<<2)&TRIPLET_MASK) + (n&3) );
    ++stop_;

    if( triplet_list_.size() > max_size_ )
    {
        triplet_list_.pop_back();
        ++start_;
        result = true;
    }

    std::fill_n( counts.begin(), 64, 0 );
    Uint4 score = 0, count = 0;
    lcr_iter_type lcr_iter = lcr_list_.begin();
    Uint4 max_lcr_score = 0;
    size_type max_len = 0;
    size_type pos = stop_;

    for( impl_citer_type it = triplet_list_.begin(), 
                         iend = triplet_list_.end();
         it != iend; ++it, ++count, --pos )
    {
        Uint1 cnt = counts[*it];

        if( cnt > 0 )
        {
            score += cnt;

            if( score*10 > thresholds_[count] )
            {
                while(    lcr_iter != lcr_list_.end() 
                       && pos <= lcr_iter->bounds_.first )
                {
                    if(    max_lcr_score == 0 
                        || max_len*lcr_iter->score_ 
                           > max_lcr_score*lcr_iter->len_ )
                    {
                        max_lcr_score = lcr_iter->score_;
                        max_len = lcr_iter->len_;
                    }

                    ++lcr_iter;
                }

                if( max_lcr_score == 0 || score*max_len >= max_lcr_score*count )
                {
                    max_lcr_score = score;
                    max_len = count;
                    lcr_iter = lcr_list_.insert( 
                        lcr_iter, lcr( pos, stop_ + 2, max_lcr_score, count ) );
                }
            }
        }

        ++counts[*it];
    }

    return result;
}
    
//------------------------------------------------------------------------------
CSymDustMasker::CSymDustMasker( 
    Uint4 level, size_type window, size_type linker )
    : level_( (level >= 2 && level <= 64) ? level : DEFAULT_LEVEL ), 
      window_( (window >= 8 && window <= 64) ? window : DEFAULT_WINDOW ), 
      linker_( (linker >= 1 && linker <= 32) ? linker : DEFAULT_LINKER )
{
    thresholds_.reserve( window_ - 2 );
    thresholds_.push_back( 1 );

    for( size_type i = 1; i < window_ - 2; ++i )
        thresholds_.push_back( i*level_ );
}

//------------------------------------------------------------------------------
std::auto_ptr< CSymDustMasker::TMaskList > 
CSymDustMasker::operator()( const sequence_type & seq )
{
    std::auto_ptr< TMaskList > res( new TMaskList );

    if( seq.size() > 3 )
    {
        lcr_list_.clear();
        triplets tris( seq, window_, lcr_list_, thresholds_ );
        seq_citer_type it = seq.begin() + tris.stop() + 3;
        const seq_citer_type seq_end = seq.end();

        while( it != seq_end )
        {
            while( !lcr_list_.empty() )
            {
                TMaskedInterval b = lcr_list_.back().bounds_;

                if( b.first >= tris.start() )
                    break;

                if( res->empty() )
                    res->push_back( b );
                else
                {
                    TMaskedInterval last = res->back();

                    if( last.second < b.first )
                        res->push_back( b );
                    else if( last.second < b.second )
                        res->back().second = b.second;
                }

                lcr_list_.pop_back();
            }

            tris.add( converter_( *it ) );
            ++it;
        }

        while( !lcr_list_.empty() )
        {
                TMaskedInterval b = lcr_list_.back().bounds_;

                if( res->empty() )
                    res->push_back( b );
                else
                {
                    TMaskedInterval last = res->back();

                    if( last.second < b.first )
                        res->push_back( b );
                    else if( last.second < b.second )
                        res->back().second = b.second;
                }

                lcr_list_.pop_back();
        }
    }

    return res;
}

//------------------------------------------------------------------------------
void CSymDustMasker::GetMaskedLocs( 
    objects::CSeq_id & seq_id,
    const sequence_type & seq, 
    std::vector< CConstRef< objects::CSeq_loc > > & locs )
{
    typedef std::vector< CConstRef< objects::CSeq_loc > > locs_type;
    std::auto_ptr< TMaskList > res = (*this)( seq );
    locs.clear();
    locs.reserve( res->size() );

    for( TMaskList::const_iterator it = res->begin(); it != res->end(); ++it )
        locs.push_back( CConstRef< objects::CSeq_loc >( 
            new objects::CSeq_loc( seq_id, it->first, it->second ) ) );
}

END_NCBI_SCOPE

