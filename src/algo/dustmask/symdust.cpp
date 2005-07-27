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

// NOTE: this function should be inlined. In majority of cases conditions
// in both ifs are false, so the execution path is really short.
//------------------------------------------------------------------------------
inline void CSymDustMasker::triplets::add_k_info( triplet_type t )
{
    inner_sum_ += inner_counts_[t];
    ++inner_counts_[t];

    // if we just reached or exceeded low_k_ elements, then update 
    // high_beg_ as necessary
    if( inner_counts_[t] > low_k_ )
    {
        Uint4 off = triplet_list_.size() - (high_beg_ - start_) - 1;

        do{
            rem_k_info( triplet_list_[off] );
            --off;
            ++high_beg_;
        }while( triplet_list_[off+1] != t );
    }
}

//------------------------------------------------------------------------------
CSymDustMasker::triplets::triplets( 
    triplet_type first, size_type window, Uint1 low_k,
    lcr_list_type & lcr_list, thres_table_type & thresholds )
    : start_( 0 ), stop_( 0 ), max_size_( window - 2 ), low_k_( low_k ),
      high_beg_( 0 ), lcr_list_( lcr_list ), thresholds_( thresholds ),
      outer_counts_( 64, 0 ), inner_counts_( 64, 0 ),
      outer_sum_( 0 ), inner_sum_( 0 )
{
    // update the data structures for the first triplet in the window
    triplet_list_.push_front( first );
    ++outer_counts_[first];
    add_k_info( first );
}

//------------------------------------------------------------------------------
inline void CSymDustMasker::triplets::rem_k_info( triplet_type t )
{
    --inner_counts_[t];
    inner_sum_ -= inner_counts_[t];
}

//------------------------------------------------------------------------------
inline void CSymDustMasker::triplets::push_triplet( triplet_type t )
{ 
    triplet_list_.push_front( t ); 
    outer_sum_ += outer_counts_[t];
    ++outer_counts_[t];
    ++stop_;
    add_k_info( t );
}

//------------------------------------------------------------------------------
inline void CSymDustMasker::triplets::pop_triplet()
{
    triplet_type t = triplet_list_.back();

    // remove from the position list only if the suffix spans the whole window
    if( inner_counts_[t] == outer_counts_[t] )
        rem_k_info( t );

    // make sure the high_beg_ is within the window
    if( start_ == high_beg_ )
        ++high_beg_;

    --outer_counts_[t];
    outer_sum_ -= outer_counts_[t];
    triplet_list_.pop_back();
    ++start_;
}

//------------------------------------------------------------------------------
inline bool CSymDustMasker::triplets::add( sequence_type::value_type n )
{
    typedef lcr_list_type::iterator lcr_iter_type;
    static counts_type counts( 64 );

    bool result = false;

    // shift the left end of the window, if necessary
    if( triplet_list_.size() >= max_size_ )
    {
        pop_triplet();
        result = true;
    }

    push_triplet( ((triplet_list_.front()<<2)&TRIPLET_MASK) + (n&3) );
    Uint4 count = stop_ - high_beg_ + 1; // count is the suffix length

    // if the condition does not hold then nothing in the window should be masked
    if( 10*outer_sum_ > thresholds_[count] )
    {
        // we need a local copy of triplet counts
        std::copy( inner_counts_.begin(), inner_counts_.end(), counts.begin() );

        Uint4 score = inner_sum_; // and of the partial sum
        lcr_iter_type lcr_iter = lcr_list_.begin();
        Uint4 max_lcr_score = 0;
        size_type max_len = 0;
        size_type pos = high_beg_ - 1; // skipping the suffix
        impl_citer_type it = triplet_list_.begin() + count; // skipping the suffix
        impl_citer_type iend = triplet_list_.end();

        for( ; it != iend; ++it, ++count, --pos )
        {
            Uint1 cnt = counts[*it];

            // If this triplet has not appeared before, then the partial sum
            //      does not change
            if( cnt > 0 )
            {
                score += cnt;

                if( score*10 > thresholds_[count] )
                {   // found the candidate for the perfect interval
                    // get the max score for the existing perfect intervals within
                    //      current suffix
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

                    // check if the current suffix score is at least as large
                    if(    max_lcr_score == 0 
                        || score*max_len >= max_lcr_score*count )
                    {
                        max_lcr_score = score;
                        max_len = count;
                        lcr_iter = lcr_list_.insert( 
                            lcr_iter, lcr( pos, stop_ + 2, 
                                           max_lcr_score, count ) );
                    }
                }
            }

            ++counts[*it];
        }
    }

    return result;
}
    
//------------------------------------------------------------------------------
CSymDustMasker::CSymDustMasker( 
    Uint4 level, size_type window, size_type linker )
    : level_( (level >= 2 && level <= 64) ? level : DEFAULT_LEVEL ), 
      window_( (window >= 8 && window <= 64) ? window : DEFAULT_WINDOW ), 
      linker_( (linker >= 1 && linker <= 32) ? linker : DEFAULT_LINKER ),
      low_k_( level_/5 )
{
    thresholds_.reserve( window_ - 2 );
    thresholds_.push_back( 1 );

    for( size_type i = 1; i < window_ - 2; ++i )
        thresholds_.push_back( i*level_ );
}

//------------------------------------------------------------------------------
std::auto_ptr< CSymDustMasker::TMaskList > 
CSymDustMasker::operator()( const sequence_type & seq, 
                            size_type start, size_type stop )
{
    std::auto_ptr< TMaskList > res( new TMaskList );

    if( seq.empty() )
        return res;

    if( stop >= seq.size() )
        stop = seq.size() - 1;

    if( start > stop )
        start = stop;

    if( stop - start > 2 )    // there must be at least one triplet
    {
        // initializations
        lcr_list_.clear();
        triplet_type first_triplet
            = (converter_( seq[start] )<<4) 
            + (converter_( seq[start + 1] )<<2)
            + (converter_( seq[start + 2] ));
        triplets tris( first_triplet, window_, low_k_, lcr_list_, thresholds_ );
        seq_citer_type it = seq.begin() + start + tris.stop() + 3;
        const seq_citer_type seq_end = seq.begin() + stop + 1;

        while( it != seq_end )
        {
            // append all the perfect intervals outside of the current window
            //      to the result
            while( !lcr_list_.empty() )
            {
                TMaskedInterval b = lcr_list_.back().bounds_;
                TMaskedInterval b1( b.first + start, b.second + start );

                if( b.first >= tris.start() )
                    break;

                if( res->empty() )
                    res->push_back( TMaskedInterval( b1 ) );
                else
                {
                    TMaskedInterval last = res->back();

                    if( last.second + linker_ < b1.first )
                        res->push_back( b1 );
                    else if( last.second < b1.second )
                        res->back().second = b1.second;
                }

                lcr_list_.pop_back();
            }

            // shift the window
            tris.add( converter_( *it ) );
            ++it;
        }

        // append the rest of the perfect intervals to the result
        while( !lcr_list_.empty() )
        {
                TMaskedInterval b = lcr_list_.back().bounds_;
                TMaskedInterval b1( b.first + start, b.second + start );

                if( res->empty() )
                    res->push_back( b1 );
                else
                {
                    TMaskedInterval last = res->back();

                    if( last.second < b1.first )
                        res->push_back( b1 );
                    else if( last.second < b1.second )
                        res->back().second = b1.second;
                }

                lcr_list_.pop_back();
        }
    }

    return res;
}

//------------------------------------------------------------------------------
std::auto_ptr< CSymDustMasker::TMaskList > 
CSymDustMasker::operator()( const sequence_type & seq )
{ return (*this)( seq, 0, seq.size() - 1 ); }

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

/*
 * ========================================================================
 * $Log$
 * Revision 1.12  2005/07/27 18:40:49  morgulis
 * some code simplification
 *
 * Revision 1.11  2005/07/19 18:59:25  morgulis
 * Simplification of add_k_info().
 *
 * Revision 1.10  2005/07/18 14:55:59  morgulis
 * Removed position lists maintanance.
 *
 * Revision 1.9  2005/07/14 20:39:39  morgulis
 * Fixed offsets bug when masking part of the sequence.
 *
 * Revision 1.8  2005/07/13 18:29:50  morgulis
 * operator() can mask part of the sequence
 *
 *
 */
