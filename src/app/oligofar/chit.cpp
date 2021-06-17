#include <ncbi_pch.hpp>
#include "chit.hpp"
#include "cquery.hpp"
#include "cbithacks.hpp"
#include "csamrecords.hpp"
#include <set>

USING_OLIGOFAR_SCOPES;

Uint8 CHit::s_count = 0;

static char * gsx_CopyStr( const string& str ) 
{
    if( char * x = new char[str.length() + 1] ) {
        memcpy( x, str.c_str(), str.length() + 1 );
        x[str.length()] = 0;
        return x;
    } else throw bad_alloc();
}

//static int s_ordinal = 0;

void CHit::x_Register( const CHit * h, bool on )
{
    return;
    static set<const CHit*> hits;
    if( on ) {
        if( hits.find( h ) != hits.end() ) {
            THROW( logic_error, "Double CHit constructor" );
        }
        hits.insert( h );
    } else {
        if( hits.find( h ) == hits.end() ) {
            THROW( logic_error, "Deleting unexisting hit?" );
        }
        hits.erase( h );
    }
}

CHit::~CHit() { 
    ASSERT( !( IsNull() && m_next )); 
    ASSERT( (m_flags & fDeleted) == 0 );
    m_flags |= fDeleted;
    x_Register( this, false );
    delete m_next; 
    delete [] m_target[0];
    delete [] m_target[1];
//    if( !IsNull() ) 
        --s_count; 
}

CHit::CHit( CQuery* q ) : 
    m_query( q ), m_next( 0 ), m_seqOrd( ~0U ), m_fullFrom( 0 ), m_fullTo( 0 ), m_flags( fNONE )
{
    x_Register( this, true );
    m_score[0] = m_score[1] = 0;
    m_length[0] = m_length[1] = 0;
    m_target[0] = m_target[1] = 0;
    ++s_count;
}

CHit::CHit( CQuery* q, Uint4 seqOrd, int pairmate, double score, int from, int to, int convTbl, const TTranscript& trans ) : 
    m_query( q ), m_next( 0 ), m_seqOrd( seqOrd ), 
    m_fullFrom( from ), m_fullTo( to ),
    m_flags( ( fComponent_1 << pairmate ) | ( (3&convTbl) << kAlign_convTbl1_bit ) )
{
    x_Register( this, true );
    ASSERT( ( pairmate & ~1 ) == 0 );
    m_length[!pairmate] = 0;
    if( to < from ) {
        swap( m_fullFrom, m_fullTo );
        m_length[pairmate] = from - to + 1;
        m_flags |= pairmate ? fRead2_reverse : fRead1_reverse;
    } else {
        m_length[pairmate] = to - from + 1;
    }
    if( pairmate ) m_flags |= fOrder_reverse;
    m_score[pairmate] = float( score );
    m_score[!pairmate] = 0;
    m_target[0] = m_target[1] = 0;
    m_transcript[pairmate].Assign( trans );
    ++s_count;
}

CHit::CHit( CQuery* q, Uint4 seqOrd, double score1, int from1, int to1, int convTbl1, double score2, int from2, int to2, int convTbl2, const TTranscript& tr1, const TTranscript& tr2 ) : 
    m_query( q ), m_next( 0 ), m_seqOrd( seqOrd ),
    m_fullFrom( -1 ), m_fullTo( -1 ), // explicitely... x_SetValues should set it below in switch
    m_flags( fPairedHit | ((3&convTbl1) << kAlign_convTbl1_bit) | ((3&convTbl2) << kAlign_convTbl2_bit))
{
    x_Register( this, true );
    ASSERT( m_seqOrd != ~0U );
    m_score[0] = float( score1 );
    m_score[1] = float( score2 );
    m_target[0] = m_target[1] = 0;
    m_transcript[0].Assign( tr1 );
    m_transcript[1].Assign( tr2 );
    if( from1 > to1 ) {
        m_flags |= fRead1_reverse;
        m_length[0] = from1 - to1 + 1;
    } else {
        m_length[0] = to1 - from1 + 1;
    }
    if( from2 > to2 ) {
        m_flags |= fRead2_reverse;
        m_length[1] = from2 - to2 + 1;
    } else {
        m_length[1] = to2 - from2 + 1;
    }
    ASSERT( (m_flags&fReads_overlap) == 0 );
    switch( m_flags & (fRead2_reverse|fRead1_reverse) ) {
    case fRead1_forward|fRead2_forward: x_SetValues( from1, to1, from2, to2 ); break;
    case fRead1_reverse|fRead2_forward: x_SetValues( to1, from1, from2, to2 ); break;
    case fRead1_forward|fRead2_reverse: x_SetValues( from1, to1, to2, from2 ); break;
    case fRead1_reverse|fRead2_reverse: x_SetValues( to1, from1, to2, from2 ); break;
    default: THROW( logic_error, "Oops here!" );
    }
    ASSERT( (m_flags&fReads_overlap) == 0 );
    ++s_count;
}

CHit::CHit( CQuery * q, Uint4 seqord, double score1, double score2, CSamRecord * a, CSamRecord * b ) :
    m_query( q ),
    m_next( 0 ),
    m_seqOrd( seqord ),
    m_fullFrom( -1 ),
    m_fullTo( -1 ),
    m_flags( fCachesSAMlines )
{
    x_Register( this, true );
    m_score[0] = score1;
    m_score[1] = score2;
    m_target[0] = m_target[1] = 0;
    x_InitComponent( 0, a );
    x_InitComponent( 1, b );
    if( a && b ) {
        int from1 = a->GetRefBounding().GetFrom();
        int from2 = b->GetRefBounding().GetFrom();
        int to1 = a->GetRefBounding().GetTo();
        int to2 = b->GetRefBounding().GetTo();
        x_SetValues( from1, to1, from2, to2 ); // no switch is required since fromX and toX are already ordered
    } else if( a ) {
        m_fullFrom = a->GetRefBounding().GetFrom();
        m_fullTo = a->GetRefBounding().GetTo();
    } else if( b ) {
        m_fullFrom = b->GetRefBounding().GetFrom();
        m_fullTo = b->GetRefBounding().GetTo();
    }
    ++s_count;
}

void CHit::x_InitComponent( int mate, CSamRecord * a )
{
    delete [] m_target[mate];
    if( a ) {
        ostringstream o;
        a->WriteAsSam(o);
        m_target[mate] = new char[o.str().length() + 1];
        strcpy( m_target[mate], o.str().c_str() );
        m_transcript[mate].Assign( a->GetImprovedCigar() );
        m_length[mate] = a->GetRefBounding().GetLength();
        if( a->IsReverse() ) m_flags |= (fRead1_reverse<<mate);
        m_flags |= (fComponent_1 << mate);
    } else {
        m_length[mate] = 0;
        m_target[mate] = 0;
    }
}

void CHit::SetPairmate( int pairmate, double score, int from, int to, int convTbl, const TTranscript& tr )
{
    ASSERT( m_score[pairmate] <= score );
    ASSERT( (pairmate&~1) == 0 );
    ASSERT( (m_flags & fPurged) == 0 );
    convTbl &= 3;
    m_flags |= pairmate ? ((3&convTbl) << kAlign_convTbl2_bit) : ((2&convTbl) << kAlign_convTbl1_bit);
    if( m_length[pairmate] == 0 ) {
        //ASSERT( (m_flags & (fOrder_reverse|fReads_overlap)) == 0 );
        m_flags |= fComponent_1 << pairmate;
        if( from > to ) {
            m_flags |= pairmate ? fRead2_reverse : fRead1_reverse;
            m_length[pairmate] = from - to + 1;
            if( pairmate ) x_SetValues( m_fullFrom, m_fullTo, to, from );
            else x_SetValues( to, from, m_fullFrom, m_fullTo );
        } else {
            m_length[pairmate] = to - from + 1;
            if( pairmate ) x_SetValues( m_fullFrom, m_fullTo, from, to );
            else x_SetValues( from, to, m_fullFrom, m_fullTo );
        }
        // add hit
    } else if( m_length[!pairmate] == 0 ) {
        // hmmm... should not happen! 
        THROW( logic_error, "CHit::SetPairmate( ... ) is not supposed for hit replacement" );
    } else {
        ASSERT( (m_flags & fPairedHit) == fPairedHit );
        // clone hit
        // we have three cases:
        // 1) hits overlap - this is ERROR case
        // 2) the mate hit to the new one is at the lower bound
        // 3) the mate hit to the new one is at the upper bound - should never happen
        ASSERT( (m_flags & fReads_overlap) == 0 ); // case (1); to handle it we need to store offset
        // first reset all flags except other's strand
        if( pairmate ^ ((m_flags&fOrder_reverse) >> kOrder_strand_bit) ) { // case (2): r1 && 21 || r2 && 12
            if( from > to ) {
                m_flags |= pairmate ? fRead2_reverse : fRead1_reverse;
                m_length[pairmate] = from - to + 1;
                x_SetValues( m_fullFrom, m_fullFrom + m_length[!pairmate] - 1, to, from );
            } else {
                m_flags &= pairmate ? ~fRead2_reverse : ~fRead1_reverse;
                m_length[pairmate] = to - from + 1;
                x_SetValues( m_fullFrom, m_fullFrom + m_length[!pairmate] - 1, from, to );
            }
        } else { // case (3) r1 && 12 || r2 && 21
            if( from > to ) {
                m_flags |= pairmate ? fRead2_reverse : fRead1_reverse;
                m_length[pairmate] = from - to + 1;
                x_SetValues( m_fullTo - m_length[!pairmate] + 1, m_fullTo, to, from );
            } else {
                m_flags &= pairmate ? ~fRead2_reverse : ~fRead1_reverse;
                m_length[pairmate] = to - from + 1;
                x_SetValues( m_fullTo - m_length[!pairmate] + 1, m_fullTo, from, to );
            }
        }
    }
    m_score[pairmate] = float( score );
    m_transcript[pairmate] = tr;
    ASSERT( (m_flags&fReads_overlap) == 0 );
}

void CHit::x_SetValues( int min1, int max1, int min2, int max2 ) 
{
    ASSERT( min1 < max1 && min2 < max2 );
    if( min1 < min2 && max1 < max2 ) {
        m_fullFrom = min1;
        m_fullTo = max2;
        m_flags &= ~fOrder_reverse;
    } else if ( min2 < min1 && max2 < max1 ) {
        m_fullFrom = min2;
        m_fullTo = max1;
        m_flags |= fOrder_reverse;
    } else {
        m_fullFrom = min( min1, min2 );
        m_fullTo = max( max1, max2 );
        m_flags |= fReads_overlap;
        cerr << "\n\x1b[31mReads overlap: one is subsequence of the other: [" << min1 << ".." << max1 << "] & [" << min2 << ".." << max2 << "]\x1b[0m\n";
    }
}

void CHit::SetTarget( int pairmate, const char * begin, const char * end ) 
{ 
    if( m_flags & fCachesSAMlines ) return;
    if( m_target[pairmate] ) { delete [] m_target[pairmate]; m_target[pairmate] = 0; }
    if( begin == 0 || !HasComponent( pairmate ) ) { return; }
    int b = min( GetFrom( pairmate ), GetTo( pairmate ) );
    int l = abs( GetFrom( pairmate ) - GetTo( pairmate ) ) + 1;
    int p = 0, s = 0;
    if( b < 0 ) { 
        p = -b;
        l -= p;
        b = 0;
    }
    if( b + l > end - begin ) {
        s = b + l - (end - begin);
        l -= s;
    }
    if( l < 0 || l > 2*(int)m_query->GetLength( pairmate ) ) {
        m_target[pairmate] = gsx_CopyStr( "\xf" ); // indication of bad alignment
    } else {
        string x = string( p, '\xf' ) + string( b + begin, l ) + string( s, '\xf' );
        ASSERT( x.length() );
        m_target[pairmate] = gsx_CopyStr( x );
    }
}

int CHit::ComputeRangeLength( int a, int b, int c, int d ) 
{
    if( a > b ) a = b;
    if( c > d ) d = c;
    return max( a, d );
}

int CHit::GetUpperHitMinPos() const
{
    return m_fullTo - m_length[( m_flags & fOrder_reverse )>>kOrder_strand_bit] + 1;
}

Uint2 CHit::ComputeGeometry( int from1, int to1, int from2, int to2 )
{
    Uint2 rc = 0;
    if( from1 > to1 ) { rc |= fRead1_reverse; swap( from1, to1 ); }
    if( from2 > to2 ) { rc |= fRead2_reverse; swap( from2, to2 ); }
    if( from1 < from2 && to1 < to2 );
    else if ( from2 < from1 && to2 < to1 ) rc |= fOrder_reverse;
    else return fReads_overlap; // overlapping
    return rc;
}

bool CHit::Equals( const CHit * other ) const 
{
    return m_query == other->m_query && EqualsSameQ( other );
}

bool CHit::EqualsSameQ( const CHit * other ) const
{
    return
        m_seqOrd   == other->m_seqOrd &&
        m_fullFrom == other->m_fullFrom &&
        m_fullTo   == other->m_fullTo &&
        m_score[0] == other->m_score[0] &&
        m_score[1] == other->m_score[1];
}

bool CHit::ClustersWith( const CHit * other ) const
{
    return m_query == other->m_query && ClustersWithSameQ( other );
}

bool CHit::ClustersWithSameS( const CHit * other ) const
{
    return m_query == other->m_query && ClustersWithSameQS( other );
}

bool CHit::ClustersWithSameQ( const CHit * other ) const
{
    return m_seqOrd == other->m_seqOrd && ClustersWithSameQS( other );
}

bool CHit::ClustersWithSameQS( const CHit * other ) const
{
    if( m_flags != other->m_flags ) return false;
    int delta = 0;
    switch( m_flags & fPairedHit ) {
    case 0: THROW( logic_error, "It is a bad idea to compare null reads" );
    case fComponent_1: delta = min( m_length[0], other->m_length[0] ); break;
    case fComponent_2: delta = min( m_length[1], other->m_length[1] ); break;
    case fPairedHit:   delta = min( min( m_length[0], other->m_length[0] ), min( m_length[1], other->m_length[1] ) ); break;
    default: THROW( logic_error, "Oops here!" );
    }
    delta /= 2;
    if( abs( m_fullFrom - other->m_fullFrom ) < delta && abs( m_fullTo - other->m_fullTo ) < delta ) return true;
    return false;
}

void CHit::PrintDebug( ostream& out ) const 
{
    out << m_query->GetId() << " on seq#" << m_seqOrd << "\t";
    if( m_length[0] == 0 ) {
        out << "2" << (m_flags & fRead2_reverse ? "-" : "+") 
            << "[" << m_fullFrom << ".." << m_fullTo << "]=" << GetTotalScore();
    } else if( m_length[1] == 0 ) {
        out << "1" << (m_flags & fRead1_reverse ? "-" : "+") 
            << "[" << m_fullFrom << ".." << m_fullTo << "]=" << GetTotalScore();
    } else {
        out << ( m_flags & fOrder_reverse ? "2" : "1" )
            << ( m_flags & fOrder_reverse ? m_flags & fRead2_reverse ? "-" : "+" : m_flags & fRead1_reverse ? "-" : "+" )
            << "[" << m_fullFrom << ".." << ( m_fullFrom + m_length[ m_flags & fOrder_reverse ? 1 : 0 ] - 1 ) << "]=" 
            << m_score[m_flags & fOrder_reverse ? 1 : 0] << "\t";
        out << ( m_flags & fOrder_reverse ? "1" : "2" )
            << ( m_flags & fOrder_reverse ? m_flags & fRead1_reverse ? "-" : "+" : m_flags & fRead2_reverse ? "-" : "+" )
            << "[" << ( m_fullTo - m_length[ m_flags & fOrder_reverse ? 0 : 1 ] + 1 ) << ".." << m_fullTo << "]="
            << m_score[m_flags & fOrder_reverse ? 0 : 1] << "\t";
        out << GetTotalScore();
    }
}

