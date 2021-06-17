#ifndef OLIGOFAR_SPACIALRANGES__HPP
#define OLIGOFAR_SPACIALRANGES__HPP

#include "defs.hpp"
#include "array_set.hpp"
#include "string-util.hpp"

BEGIN_OLIGOFAR_SCOPES

class CSpacialRanges
{
public:
    enum EFlags { fUnique = 0x01 };
    enum EConstants { kHalo = 16 };

    CSpacialRanges( int min = 0, int max = numeric_limits<int>::max() - 1, CSpacialRanges * p = 0 ) : 
        m_parent( p ), m_left( 0 ), m_right( 0 ), m_flags( 0 ), m_halo(0),
        m_min( min ), m_max( max ), // m_mid( (m_min + m_max + 1)/2 ),
        m_from( GetMid() + 1 ), m_to( GetMid() ), m_count( 0 ),
        m_seqdata(0) {}
    ~CSpacialRanges();

    void AddRange( int from, int to, Uint4 flags = 0, int count = 1 );
    bool Empty() const { return m_count == 0 && (m_left == 0 || m_left->Empty()) && (m_right == 0 || m_right->Empty()); }
    CSpacialRanges * GetLeftmost() { return m_left ? m_left->GetLeftmost() : m_count ? this : m_right ? m_right->GetLeftmost() : 0; }
    CSpacialRanges * GetRightmost() { return m_right ? m_right->GetRightmost() : m_count ? this : m_left ? m_left->GetRightmost() : 0; }
    CSpacialRanges * GetTop() { return m_parent ? m_parent->GetTop() : this; }
    CSpacialRanges * FindContaining( int from, int to ); 
    CSpacialRanges * FindOverlapping( int from, int to ); 
    CSpacialRanges * Right();
    CSpacialRanges * Left();
    bool IsLeft() const { return m_parent && m_parent->m_left == this; }
    bool IsRight() const { return m_parent && m_parent->m_right == this; }
    bool IsTop() const { return m_parent == 0; }
    bool Contains( int pos ) const;
    bool Contains( int from, int to ) const;
    bool Overlaps( int from, int to ) const;
    int GetRank() const { return m_parent ? m_parent->GetRank() + 1 : 0; }
    int GetFrom() const { return m_from; }
    int GetTo() const { return m_to; }
    int GetCount() const { return m_count; }
    Uint4 GetFlags() const { return m_flags; }
    void Print( ostream& o, const string& rs = "; " ) const;
    void PrintTree( ostream& o ) const;
    void RemoveSingletons();

    const char * GetBase( int pos ) const;

    void SetSeqdataFrom( const char * begin, int length, int halo = kHalo );
    void AddDonor( int donor );
    void AddAcceptor( int acceptor );
    const char * GetSeqdata() const { return m_seqdata; }
    int GetSeqdataLength() const { return m_seqdata ? m_to - m_from + 1 + 2*m_halo : 0; }
    int GetSeqdataOffset() const { return m_from - m_halo; }
    int GetHalo() const { return m_halo; }

    typedef array_set<int> TSplices;
    const TSplices& GetSplices() const { return m_donors; }

    bool HasDonor( int donor ) const;
    bool HasAcceptor( int acceptor ) const { return HasDonor( -acceptor ); }

protected:
    int GetMid() const { return ( m_min + m_max + 1 ) / 2; } 
    CSpacialRanges * x_Right();
    CSpacialRanges * x_Left();
protected:
    CSpacialRanges * m_parent;
    CSpacialRanges * m_left;
    CSpacialRanges * m_right;
    Uint2 m_flags;
    Uint2 m_halo;
    int m_min;
    int m_max;
    int m_from;
    int m_to;
    int m_count;
    char * m_seqdata;
    TSplices m_donors; // acceptors are 'negated'
    //array_set<int> m_acceptors;
};

inline const char * CSpacialRanges::GetBase( int pos ) const
{ 
    if( GetCount() ) {
        if( pos < (m_from - m_halo/2) ) return m_left ? m_left->GetBase( pos ) : 0;
        if( pos > (m_to + m_halo/2) ) return m_right ? m_right->GetBase( pos ) : 0;
        return GetSeqdata() + pos - GetSeqdataOffset(); 
    } else {
        if( pos < GetMid() ) return m_left ? m_left->GetBase( pos ) : 0;
        if( pos > GetMid() ) return m_right ? m_right->GetBase( pos ) : 0;
        return 0;
    }
}

inline void CSpacialRanges::SetSeqdataFrom( const char * begin, int length, int halo )
{
    delete [] m_seqdata;
    if( m_count ) {
        m_halo = halo;
        int len = m_to - m_from + 1 + 2*halo;
        m_seqdata = new char[len];
        const char * end = begin + length;
        const char * b = begin + m_from - halo;
        const char * e = begin + m_to + 1 + halo;
        int x = 0;
        if( b < begin ) {
            memset( m_seqdata, '\xf', x = (begin - b) );
            b = begin;
        }
        if( e > end ) {
            memset( m_seqdata + len - (e - end), '\xf', e - end );
            e = end;
        }
        memcpy( m_seqdata + x, b, e - b );
    } else { m_seqdata = 0; m_halo = 0; }
    if( m_left ) m_left->SetSeqdataFrom( begin, length, halo );
    if( m_right ) m_right->SetSeqdataFrom( begin, length, halo );
}

inline void CSpacialRanges::AddDonor( int donor ) 
{
    if( donor < m_from ) {
        if( m_left ) { m_left->AddDonor( donor ); }
        //else cerr << this << "(" << m_from << ".." << m_to << "} skips donor " << donor << "\n";
        return;
    } else if( donor > m_to ) {
        if( m_right ) { m_right->AddDonor( donor ); }
        // else cerr << this << "(" << m_from << ".." << m_to << "} skips donor " << donor << "\n";
        if( donor > m_to + m_halo/2 ) return;
    }
        //cerr << this << "(" << m_from << ".." << m_to << "} adds donor " << donor << "\n";
    m_donors.insert( donor );
}

inline void CSpacialRanges::AddAcceptor( int acceptor ) 
{
    if( acceptor < m_from ) {
        if( m_left ) { m_left->AddAcceptor( acceptor ); }
//        else cerr << this << "(" << m_from << ".." << m_to << "} skips acceptor " << acceptor << "\n";
        if( acceptor < m_from - m_halo/2 || acceptor <= 0 ) return;
    } else if( acceptor > m_to ) {
        if( m_right ) { m_right->AddAcceptor( acceptor ); }
//        else cerr << this << "(" << m_from << ".." << m_to << "} skips acceptor " << acceptor << "\n";
        return;
    }
    //    cerr << this << "(" << m_from << ".." << m_to << "} adds acceptor " << acceptor << "\n";
    m_donors.insert( - acceptor );
}

inline bool CSpacialRanges::Contains( int pos ) const
{
    if( pos < GetMid() ) {
        if( m_count && pos >= m_from ) return true;
        else return m_left && m_left->Contains( pos );
    } else {
        if( m_count && pos <= m_to ) return true;
        else return m_right && m_right->Contains( pos );
    }
}

inline bool CSpacialRanges::HasDonor( int pos ) const
{
    int apos = abs( pos );
    if( apos < GetMid() ) {
        if( m_count && apos >= m_from ) return m_donors.find( pos ) != m_donors.end();
        else return m_left && m_left->HasDonor( pos );
    } else {
        if( m_count && apos <= m_to ) return m_donors.find( pos ) != m_donors.end();
        else return m_right && m_right->HasDonor( pos );
    }
    return false;
}

inline bool CSpacialRanges::Contains( int from, int to ) const
{
    if( m_count && from >= m_from && to <= m_to ) return true;
    if( to < GetMid() && m_left ) return m_left->Contains( from, to );
    if( from > GetMid() && m_right ) return m_right->Contains( from, to );
    return false;
}

inline bool CSpacialRanges::Overlaps( int from, int to ) const 
{
    if( m_count && to >= m_from && from <= m_to ) return true;
    if( to < GetMid() && m_left ) return m_left->Overlaps( from, to );
    if( from > GetMid() && m_right ) return m_right->Overlaps( from, to );
    return false;
}

inline CSpacialRanges * CSpacialRanges::FindContaining( int from, int to )
{
    if( to > from ) swap( from, to );
    ASSERT( to <= m_max && from >= m_min );
    if( m_count && from >= m_from && to <= m_to ) return this;
    if( to < GetMid() && m_left ) return m_left->FindContaining( from, to );
    if( from > GetMid() && m_right ) return m_right->FindContaining( from, to );
    return (from >= m_from && to <= m_to && m_count > 0) ? this : 0;
}

inline CSpacialRanges * CSpacialRanges::FindOverlapping( int from, int to ) 
{
    /*
    if( m_count ) cerr << "\x1b[1m";
    CRange<int> fullrange( m_min, m_max );
    CRange<int> islerange( m_from, m_to );
    CRange<int> lookrange( from, to );
    //cerr << this << DISPLAY( m_min ) << DISPLAY( from ) << DISPLAY( m_left ) << DISPLAY( GetMid() ) << DISPLAY( m_right ) << DISPLAY( to ) << DISPLAY( m_max ) << "\n";
    cerr << this << DISPLAY( fullrange ) << DISPLAY( islerange ) << DISPLAY( lookrange ) << DISPLAY( GetMid() ) << DISPLAY( m_left ) << DISPLAY( m_right ) << "\n";
    if( m_count ) cerr << "\x1b[0m";
    */
    //ASSERT( from <= to );
    if( to > from ) swap( from, to );
    ASSERT( to <= m_max && from >= m_min );
    if( m_count && to >= m_from && from <= m_to ) return this;
    if( to < GetMid() && m_left ) return m_left->FindOverlapping( from, to );
    if( from > GetMid() && m_right ) return m_right->FindOverlapping( from, to );
    return 0;
}

inline void CSpacialRanges::RemoveSingletons()
{
    if( m_left ) {
        m_left->RemoveSingletons();
        if( m_left->Empty() ) delete m_left;
    }
    if( m_right ) {
        m_right->RemoveSingletons();
        if( m_right->Empty() ) delete m_right;
    }
    if( m_count == 1 && ( m_flags & fUnique ) == 0 ) {
        m_from = GetMid() + 1; 
        m_to = GetMid();
        m_count = 0;
        m_flags = 0;
    }
}

inline void CSpacialRanges::Print( ostream& o, const string& rs ) const 
{
    if( m_left ) m_left->Print( o, rs );
    if( m_count ) {
        o << m_from << ".." << m_to << "(" << m_count << ")" << (m_flags & fUnique ? "u" : "." );
        if( m_donors.size() ) o << " {" << Join( ",", m_donors ) << "}"; 
        o <<  rs;
    }
    if( m_right ) m_right->Print( o, rs );
}

inline void CSpacialRanges::PrintTree( ostream& o ) const 
{
    string prefix = string( GetRank(), ' ' );
    o << prefix << this << " {" << m_min << ".." << m_max << "\t" << GetMid() << "\t" << m_left << "\t" << m_right << "\n";
    if( m_left ) m_left->PrintTree( o );
    o << prefix;
    if( m_count ) o << "[" << m_from << ".." << m_to << "]";
    o << "(" << m_count << ")" << (m_flags & fUnique ? "u" : "") << "\n";
    if( m_right ) m_right->PrintTree( o );
    o << prefix << "}\n";
}

inline CSpacialRanges * CSpacialRanges::Right()
{
    for( CSpacialRanges * r = x_Right() ; r ; r = r->x_Right() ) {
        if( r->GetCount() ) return r;
    }
    return 0;
}

inline CSpacialRanges * CSpacialRanges::x_Right()
{
    if( m_right ) return m_right->GetLeftmost();
    if( IsLeft() ) return m_parent;
    CSpacialRanges * r = m_parent;
    while( r && r->IsRight() ) r = r->m_parent;
    return r ? r->m_parent : 0;
}

inline CSpacialRanges * CSpacialRanges::Left()
{
    for( CSpacialRanges * r = x_Left() ; r ; r = r->x_Left() ) {
        if( r->GetCount() ) return r;
    }
    return 0;
}

inline CSpacialRanges * CSpacialRanges::x_Left()
{
    if( m_left ) return m_left->GetRightmost();
    if( IsRight() ) return m_parent;
    CSpacialRanges * r = m_parent;
    while( r && r->IsLeft() ) r = r->m_parent;
    return r ? r->m_parent : 0;
}

inline CSpacialRanges::~CSpacialRanges()
{
    delete m_seqdata;
    if( m_parent ) { 
        if( m_parent->m_left == this ) m_parent->m_left = 0; 
        else if( m_parent->m_right == this ) m_parent->m_right = 0; 
    }
    delete m_left; delete m_right; 
}

inline void CSpacialRanges::AddRange( int from, int to, Uint4 flags, int count ) 
{
    ASSERT( from >= m_min );
    ASSERT( to <= m_max );
    ASSERT( from <= to );
    bool tryMergeLeft = false;
    bool tryMergeRight = false;
    
    if( m_count ) {
        if( to < m_from - 1 ) {
            if( m_left == 0 ) m_left = new CSpacialRanges( m_min, GetMid() - 1, this );
            m_left->AddRange( from, to, flags, count );
            return;
        } else if( from > m_to + 1 ) {
            if( m_right == 0 ) m_right = new CSpacialRanges( GetMid(), m_max, this );
            m_right->AddRange( from, to, flags, count );
            return;
        } else {
            ++m_count;
            m_flags |= flags;
            if( from < m_from ) {
                m_from = from;
                tryMergeLeft = true;
            }
            if( to > m_to ) {
                m_to = to;
                tryMergeRight = true;
            }
        }
    } else {
        if( to < GetMid() ) {
            if( m_left == 0 ) m_left = new CSpacialRanges( m_min, GetMid() - 1, this );
            m_left->AddRange( from, to, flags, count );
            return;
        } else if( from > GetMid() ) {
            if( m_right == 0 ) m_right = new CSpacialRanges( GetMid(), m_max, this );
            m_right->AddRange( from, to, flags, count );
            return;
        } else {
            m_from = from;
            m_to = to;
            m_flags |= flags;
            m_count += count;
            tryMergeLeft = true;
            tryMergeRight = true;
        }
    }

    while( tryMergeLeft && m_left ) {
        if( CSpacialRanges * r = m_left->GetRightmost() ) {
            ASSERT( r->GetCount() );
            if( r->GetTo() >= m_from - 1 ) {
                if( m_from >= r->GetFrom() ) { m_from = r->GetFrom(); tryMergeLeft = false; }
                m_count += r->GetCount();
                m_flags |= r->m_flags;
                ASSERT( r->m_right == 0 );
                r->m_from = r->GetMid() + 1;
                r->m_to = r->GetMid();
                r->m_count = 0;
                r->m_flags = 0;
                if( ! r->m_left ) {
                    while( r != this && r->Empty() ) {
                        CSpacialRanges * x = r->m_parent;
                        delete r;
                        r = x;
                    }
                }
            } else tryMergeLeft = false;
        } else tryMergeLeft = false;
    }
    while( tryMergeRight && m_right ) {
        if( CSpacialRanges * l = m_right->GetLeftmost() ) {
            ASSERT( l->GetCount() );
            if( l->GetFrom() <= m_to + 1 ) {
                if( m_to <= l->GetTo() ) { m_to = l->GetTo(); tryMergeRight = false; }
                m_count += l->GetCount();
                m_flags |= l->m_flags;
                ASSERT( l->m_left == 0 );
                l->m_from = l->GetMid() + 1;
                l->m_to = l->GetMid();
                l->m_count = 0;
                l->m_flags = 0;
                if( ! l->m_right ) {
                    while( l != this && l->Empty() ) {
                        CSpacialRanges * x = l->m_parent;
                        delete l;
                        l = x;
                    }
                }
            } else tryMergeRight = false;
        } else tryMergeRight = false;
    }
    ASSERT( m_from >= m_min );
    ASSERT( m_to <= m_max );
    ASSERT( m_from <= GetMid() );
    ASSERT( m_to >= GetMid() );
    ASSERT( m_from < m_to );
    ASSERT( m_min < m_max );
}
        
END_OLIGOFAR_SCOPES

#endif
