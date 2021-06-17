#ifndef OLIGOFAR_CHASHPARAM__HPP
#define OLIGOFAR_CHASHPARAM__HPP

#include "array_set.hpp"

BEGIN_OLIGOFAR_SCOPES

class CHashParam 
{
public:
    typedef array_set<int> TSkipPositions;
    
    enum EIndelLocation { eIndel_anywhere = 0 };

    CHashParam() : 
        m_wordSize( 11 ), 
        m_strideSize( 1 ),
        m_windowSize( 22 ), 
        m_windowCount( 1 ),
        m_windowStep( 0 ),
        m_windowStart( 0 ),
        m_hashBits( 22 ),
        m_hashMismatches( 1 ), 
        m_maxHashAmb( 4 ),
        m_maxHashIns( 0 ),
        m_maxHashDel( 0 ),
        m_maxHashDist( 1 ),
        m_hashIndelPos( eIndel_anywhere ),
        m_phrapCutoff( 4 ),
        m_maxSimplicity( 2.0 )
    {}
    
    int  GetWindowSize() const { return m_windowSize; }
    int  GetWordSize() const { return m_wordSize; }
    int  GetStrideSize() const { return m_strideSize; }
    int  GetWindowCount() const { return m_windowCount; }
    int  GetWindowStep() const { return m_windowStep; }
    int  GetWindowStart() const { return m_windowStart; }
    int  GetHashBits() const { return m_hashBits; }
    int  GetHashMismatches() const { return m_hashMismatches; }
    int  GetHashMaxAmb() const { return m_maxHashAmb; }
    int  GetHashInsertions() const { return m_maxHashIns; }
    int  GetHashDeletions() const { return m_maxHashDel; }
    int  GetHashMaxDistance() const { return m_maxHashDist; }
    int  GetHashIndelPosition() const { return m_hashIndelPos; }
    int  GetPhrapCutoff() const { return m_phrapCutoff; }
    double GetMaxSimplicity() const { return m_maxSimplicity; }
    
    bool GetHashIndels() const { return m_maxHashDel|m_maxHashIns; }

    void SetWindowSize( int x ) { m_windowSize = x; }
    void SetWordSize( int x ) { m_wordSize = x; }
    void SetWindowCount( int x ) { m_windowCount = x; }
    void SetWindowStep( int x ) { m_windowStep = x; }
    void SetWindowStart( int x ) { m_windowStart = x; }
    void SetStrideSize( int x ) { m_strideSize = x; }
    void SetHashBits( int x ) { m_hashBits = x; }
    void SetHashMismatches( int x ) { m_maxHashDist = max( m_maxHashDist, m_hashMismatches = x ); }
    void SetHashMaxAmb( int x ) { m_maxHashAmb = x; }
    void SetHashIndels( int x ) { m_maxHashDist = max( m_maxHashDist, m_maxHashDel = m_maxHashIns = x ); }
    void SetHashInsertions( int x ) { m_maxHashDist = max( m_maxHashDist, m_maxHashIns = x ); }
    void SetHashDeletions( int x ) { m_maxHashDist = max( m_maxHashDist, m_maxHashDel = x ); }
    void SetMaxHashDistance( int x ) { m_maxHashDist = max( max( x, m_hashMismatches ), max( m_maxHashIns, m_maxHashDel ) ); }
    void SetHashIndelPosition( int x ) { m_hashIndelPos = x; }
    void SetPhrapCutoff( int x ) { m_phrapCutoff = x; }
    void SetMaxSimplicity( double x ) { m_maxSimplicity = x; }
    
    bool ValidateParam( string& msg ) const { /* TODO: implement */ return true; }
    bool Validate( ostream& out, const string& prefix );

    const TSkipPositions& GetSkipPositions() const { return m_skipPositions; }
    TSkipPositions& SetSkipPositions() { return m_skipPositions; }
    template<class iterator> 
    void SetSkipPositions( const iterator& a, const iterator& b ) { m_skipPositions.clear(); copy( a, b, inserter( m_skipPositions, m_skipPositions.end() ) ); }
    template<class container>
    void SetSkipPositions( const container& c ) { SetSkipPositions( c.begin(), c.end() ); }

protected:
    int  m_wordSize;
    int  m_strideSize;
    int  m_windowSize;
    int  m_windowCount;
    int  m_windowStep;
    int  m_windowStart;
    int  m_hashBits;
    int  m_hashMismatches;

    int  m_maxHashAmb;
    int  m_maxHashIns;
    int  m_maxHashDel;
    int  m_maxHashDist;
    int  m_hashIndelPos;

    int  m_phrapCutoff;

    double m_maxSimplicity;

    TSkipPositions m_skipPositions;
};

inline bool CHashParam::Validate( ostream& out, const string& prefix ) 
{
    if( GetWindowSize() + GetStrideSize() - 1 + GetHashDeletions() > 32 ) {
        THROW( runtime_error, "Sorry, constraint ($w + $S + $e - 1) <= 32 is violated, can't proceed ($w - winsz, $S - strides, $e - indels)" );
    }
    if( GetWordSize() * 2 < GetWindowSize() ) {
        SetWordSize( ( GetWindowSize() + 1 ) / 2 );
        cerr << prefix << "WARNING: Increasing word size to " << GetWordSize() << " bases to cover window\n";
    }
    if( GetWordSize()*2 - 16 >= 32 ) {
        SetWordSize( 23 );
        cerr << prefix << "WARNING: Decreasing word size to " << GetWordSize() << " as required for hashing algorithm\n";
    }
    if( GetWordSize()*2 - GetHashBits() > 16 ) {
        SetHashBits( GetWordSize()*2 - 16 );
        cerr << prefix << "WARNING: Increasing hash bits to " << GetHashBits() << " to fit " << GetWordSize() << "-base word\n";
    }
    return true;
}

END_OLIGOFAR_SCOPES

#endif
