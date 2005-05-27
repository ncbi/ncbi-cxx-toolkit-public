#include <ncbi_pch.hpp>

#include <inttypes.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <iterator>
#include <memory>
#include <deque>
#include <list>

typedef uint8_t  Uint1;
typedef uint32_t Uint4;

//------------------------------------------------------------------------------
template< typename seq_t >
class CDustMasker
{
    public:

        static const Uint4 DEFAULT_LEVEL  = 20;
        static const Uint4 DEFAULT_WINDOW = 64;
        static const Uint4 DEFAULT_LINKER = 1;

        typedef seq_t sequence_type;
        typedef typename sequence_type::size_type size_type;
        typedef std::pair< size_type, size_type > TMaskedInterval;
        typedef std::vector< TMaskedInterval > TMaskList;

        CDustMasker( Uint4 level = DEFAULT_LEVEL, 
                     size_type window 
                        = static_cast< size_type >( DEFAULT_WINDOW ),
                     size_type linker 
                        = static_cast< size_type >( DEFAULT_LINKER ) );

        std::auto_ptr< TMaskList > operator()( const sequence_type & seq );

    private:

        struct lcr
        {
            TMaskedInterval bounds_;
            Uint4 score_;
            size_type len_;

            lcr( size_type start, size_type stop, Uint4 score, size_type len )
                : bounds_( start, stop ), score_( score ), len_( len )
            {}
        };

        typedef Uint1 triplet_type;
        typedef typename sequence_type::const_iterator seq_citer_type;
        typedef std::list< lcr > lcr_list_type;
        typedef std::vector< Uint4 > thres_table_type;

        static const triplet_type TRIPLET_MASK = 0x3F;

        class triplets
        {
            public:
                
                triplets( const sequence_type & seq, 
                          size_type window, 
                          lcr_list_type & lcr_list,
                          thres_table_type & thresholds );

                size_type start() const { return start_; }
                size_type stop() const { return stop_; }
                size_type size() const { return triplet_list_.size(); }

                bool add( typename sequence_type::value_type n );
                          
            private:
                
                typedef std::deque< triplet_type > impl_type;
                typedef impl_type::const_iterator impl_citer_type;

                impl_type triplet_list_;

                size_type start_;
                size_type stop_;
                size_type max_size_;

                lcr_list_type & lcr_list_;
                thres_table_type & thresholds_;
        };

        Uint4 level_;
        size_type window_;
        size_type linker_;

        lcr_list_type lcr_list_;
        thres_table_type thresholds_;
};

template< typename seq_t >
CDustMasker< seq_t >::triplets::triplets( 
    const sequence_type & seq, size_type window, 
    lcr_list_type & lcr_list, thres_table_type & thresholds )
    : start_( 0 ), stop_( 0 ), max_size_( window - 2 ), 
      lcr_list_( lcr_list ), thresholds_( thresholds )
{
    triplet_type t = ((seq[0]&3)<<4) + ((seq[1]&3)<<2) + (seq[2]&3);
    triplet_list_.push_back( t );
}

template< typename seq_t >
inline bool CDustMasker< seq_t >::triplets::add( 
    typename sequence_type::value_type n )
{
    typedef std::vector< Uint1 > counts_type;
    typedef typename lcr_list_type::iterator lcr_iter_type;
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
    
template< typename seq_t >
inline CDustMasker< seq_t >::CDustMasker( 
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

template< typename seq_t >
std::auto_ptr< typename CDustMasker< seq_t >::TMaskList >
CDustMasker< seq_t >::operator()( const sequence_type & seq )
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

            tris.add( *it );
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

Uint1 to_blastna( Uint1 c )
{
    switch( c )
    {
        case 'c': case 'C': return 1;
        case 'g': case 'G': return 2;
        case 't': case 'T': return 3;
        default : return 0;
    }
}

typedef CDustMasker< std::vector< Uint1 > > duster_type;
typedef duster_type::TMaskList mask_list_type;
typedef mask_list_type::const_iterator mask_list_ci_type;

void process( std::vector< Uint1 > & data, duster_type duster )
{
    std::transform( data.begin(), data.end(), data.begin(), &to_blastna );
    std::auto_ptr< mask_list_type > mask_list( duster( data ) );
    
    for( mask_list_ci_type citer = mask_list->begin();
         citer != mask_list->end(); ++citer )
      std::cout << citer->first << " - " << citer->second << "\n";

    std::cerr << ".";
}

int main()
{
    std::string line;
    std::vector< Uint1 > data;
    duster_type duster;

    while( std::getline( std::cin, line ) )
    {
        if( line.empty() )
            continue;

        if( line[0] == '>' )
        {
            if( !data.empty() )
                process( data, duster );

            std::cout << line << "\n";
            data.clear();
        }
        else std::copy( line.begin(), line.end(), 
                        std::back_inserter( data ) );

        line.clear();
    }

    if( !data.empty() )
        process( data, duster );
}
