#ifndef OLIGOFAR_CQUERY__HPP
#define OLIGOFAR_CQUERY__HPP

#include "cseqcoding.hpp"
#include "debug.hpp"
#include "chit.hpp"
#include <iterator>

#define DEVELOPMENT_VER 1

BEGIN_OLIGOFAR_SCOPES

class CHit;
class CScoreParam;
class CQuery
{
public:
    friend class CFilter;
    friend class CGuideFile;
    
    enum EConst { kDefaultQualityBase = 33 };
    enum EFlags {
        fCoding_ncbiqna = 0x00,
        fCoding_ncbi8na = 0x01,
        fCoding_ncbipna = 0x02,
        fCoding_colorsp = 0x03,
        fCoding_BITS = fCoding_ncbi8na|fCoding_ncbipna|fCoding_colorsp,
        fReject_short0 = 0x04,
        fReject_short1 = 0x08,
        fReject_short  = 0x0c,
        fReject_loCompl0 = 0x10,
        fReject_loCompl1 = 0x20,
        fReject_loCompl = 0x30,
        fReject_loQual0 = 0x40,
        fReject_loQual1 = 0x80,
        fReject_loQual  = 0xc0,
        fReject_BITS = 0xfc,
        fReject_BITS0 = 0x54,
        fReject_BITS1 = 0xa8,
        fStoresQuality0 = 0x100,
        fStoresQuality1 = 0x200,
        fNONE = 0
    };
    static Uint4 GetCount() { return s_count; }

    CQuery( CSeqCoding::ECoding coding, const string& id, const string& data1, const string& data2 = "", int base = kDefaultQualityBase );
    ~CQuery() { for( int i = 0; i < 3; ++i ) delete m_topHit[i]; delete [] m_data; --s_count; }

#if DEVELOPMENT_VER
    CQuery( CSeqCoding::ECoding coding, const string& id, const string& data1, const string& qual1, const string& data2, const string& qual2, int base = kDefaultQualityBase );
    int GetAppendedPhrapQualityLength(int i) const { return (m_flags & (fStoresQuality0 << i)) ? GetLength(i) : 0; }
    const char * GetAppendedPhrapQuality(int i) const { return GetData(i) + GetLength(i)*GetBytesPerBase(); }
#endif
    void SetRejectAsShort( int i, bool on = true ) { if( on ) m_flags |= (fReject_short0 << i ); else m_flags &= ~(fReject_short0 << i); }
    void SetRejectAsLoQual( int i, bool on = true ) { if( on ) m_flags |= (fReject_loQual0 << i ); else m_flags &= ~(fReject_loQual0 << i); }
    void SetRejectAsLoCompl( int i, bool on = true ) { if( on ) m_flags |= (fReject_loCompl0 << i ); else m_flags &= ~(fReject_loCompl0 << i); }
    void ClearRejectFlags( int i ) { m_flags &= ~(fReject_BITS0 << i ); }

    int GetRejectReasons() const { return m_flags & fReject_BITS; }
    int GetRejectFlags( int i ) const { return m_flags & (fReject_BITS0 << i); }

    bool IsPairedRead() const { return m_length[1] != 0; }
    bool HasComponent( int i ) const { return GetLength( i ) != 0; }

    const char* GetId() const { return (const char*)m_data; }
    const char* GetData( int i ) const { return (const char*)m_data + m_offset[i]; }
    const char* GetData( int i, int p ) const { return (const char*)m_data + m_offset[i] + p * GetBytesPerBase(); }

    unsigned GetLength( int i ) const { return m_length[i]; }
    unsigned GetDataSize( int i ) const { return GetBytesPerBase() * m_length[i]; }
    unsigned GetBytesPerBase() const { return GetCoding() == CSeqCoding::eCoding_ncbipna ? 5 : 1; }

    CSeqCoding::ECoding GetCoding() const;
    CHit * GetTopHit() const { return GetTopHit( IsPairedRead() ? 3 : 1 ); }
    CHit * GetTopHit( int components ) const { CheckMask( components ); return m_topHit[components-1]; }
    void ClearHits() { for( int i = 0; i < 3; ++i ) { delete m_topHit[i]; m_topHit[i] = 0; } }
    bool HasHits() const { return m_topHit[0] || m_topHit[1] || m_topHit[2]; }

    double ComputeBestScore( const CScoreParam * , int component ) const;
#if DEVELOPMENT_VER
    double ComputeBestScore( const CScoreParam * p ) const { return ComputeBestScore( p, 0 ) + ComputeBestScore( p, 1 ); }
#else
    double GetBestScore(int i) const { return m_bestScore[i]; }
    double GetBestScore() const { return GetBestScore(0) + GetBestScore(1); }
#endif

    void MarkPositionAmbiguous( int component, int pos );

    ostream& PrintIupac( ostream& dest, int component, CSeqCoding::EStrand = CSeqCoding::eStrand_pos ) const;
    template<typename Base>
    ostream& PrintSeq( ostream& out, int component, CSeqCoding::EStrand = CSeqCoding::eStrand_pos ) const;
    template<typename Base, typename Iter>
    Iter Export( Iter dest, int component, CSeqCoding::EStrand = CSeqCoding::eStrand_pos ) const;

    ostream& PrintDibaseAsIupac( ostream& dest, int component, CSeqCoding::EStrand ) const;

private:
    explicit CQuery( const CQuery& q );

protected:
    void x_InitNcbi8na( const string& id, const string& data1, const string& data2, int base );
    void x_InitNcbiqna( const string& id, const string& data1, const string& data2, int base );
    void x_InitNcbipna( const string& id, const string& data1, const string& data2, int base );
    void x_InitColorspace( const string& id, const string& data1, const string& data2, int base );
    void x_InitColorspace( const string& id, const string& data1, const string& qual1, const string& data2, const string& qual2, int base );
    unsigned x_ComputeInformativeLength( const string& seq ) const;

    template<class iterator>
    inline iterator Solexa2Ncbipna( iterator dest, const string& line, int );

    static void CheckMask( int mask ) { ASSERT( mask ); ASSERT( (mask & ~3) == 0 ); }
    void SetTopHit( CHit * hit ) { if( hit ) { CheckMask( hit->GetComponentMask() ); m_topHit[hit->GetComponentMask() - 1] = hit; } }
    void ResetTopHit( int cmask ) { CheckMask( cmask ); m_topHit[cmask - 1] = 0; }

protected:
    unsigned char * m_data; // contains id\0 data1 data2
    unsigned short  m_offset[2]; // m_offset[1] should always point to next byte after first seq data, unless quality scores are appended to the data
    unsigned char   m_length[2];
    unsigned short  m_flags;
#if DEVELOPMENT_VER

#else
    mutable float m_bestScore[2]; // according to new scoring approach, we may not need this
#endif
    CHit * m_topHit[3];
    static Uint4 s_count;
};

////////////////////////////////////////////////////////////////////////
// Implementation

#if DEVELOPMENT_VER

inline CQuery::CQuery( CSeqCoding::ECoding coding, const string& id, const string& data1, const string& qual1, const string& data2, const string& qual2, int base ) 
    : m_data(0), m_flags(0)
{
    m_topHit[0] = m_topHit[1] = m_topHit[2] = 0;
    switch( coding ) {
    case CSeqCoding::eCoding_ncbi8na: x_InitNcbi8na( id, data1, data2, base ); break; // ignore qualities: TODO: record them
    case CSeqCoding::eCoding_ncbiqna: x_InitNcbiqna( id, data1 + qual1, data2 + qual2, base ); break; // reuse existing function, use qna encoding
    case CSeqCoding::eCoding_ncbipna: x_InitNcbipna( id, data1, data2, base ); break; // ignore phrap - we use 4-channel
    case CSeqCoding::eCoding_colorsp: x_InitColorspace( id, data1, qual1, data2, qual2, base ); break; // that's goal: record qualities after data
    default: THROW( logic_error, "Invalid coding " << coding );
    }
    ++s_count;
}

inline void CQuery::x_InitColorspace( const string& id, const string& data1, const string& qual1, const string& data2, const string& qual2, int base )
{
    m_flags &= ~fCoding_BITS;
    m_flags |= fCoding_colorsp;
    ASSERT( data1.length() < 55 );
    ASSERT( data2.length() < 255 );
    m_length[0] = data1.length();
    m_length[1] = data2.length();
    if( qual1.length() ) m_flags |= fStoresQuality0;
    if( qual2.length() ) m_flags |= fStoresQuality1;
    int q0 = qual1.length();
    int q1 = qual2.length();
    if( q0 ) {
        ASSERT( q0 == (int)data1.length() - 1 );
        ASSERT( q1 );
    }
    if( q1 ) {
        ASSERT( q1 == (int)data2.length() - 1 );
        ASSERT( q0 );
    }
    unsigned sz = id.length() + m_length[0] + m_length[1] + q0 + q1 + 1;
    m_data = new unsigned char[sz];
    memset( m_data, 0, sz );
    strcpy( (char*)m_data, id.c_str() );
    m_offset[0] = id.length() + 1;
    m_offset[1] = m_offset[0] + m_length[0] + q0;
    for( unsigned i = 0, j = m_offset[0]; i < m_length[0]; ) 
        m_data[j++] = CColorTwoBase( CColorTwoBase::eFromASCII, data1[i++] );
    for( unsigned i = 0, j = m_offset[0] + m_length[0]; i < qual1.length(); ) 
        m_data[j++] = qual1[i] + (33 - base); // SAM standard is 33
    for( unsigned i = 0, j = m_offset[1]; i < m_length[1]; )
        m_data[j++] = CColorTwoBase( CColorTwoBase::eFromASCII, data2[i++] );
    for( unsigned i = 0, j = m_offset[1] + m_length[1]; i < qual2.length(); ) 
        m_data[j++] = qual2[i] + (33 - base); // SAM standard is 33
    if( m_length[0] ) {
        m_data[m_offset[0]+1] |= CNcbi8naBase( CNcbi8naBase(  m_data[m_offset[0]] ), CColorTwoBase(m_data[m_offset[0] + 1]) );
        --m_length[0]; 
        ++m_offset[0]; 
    }
    if( m_length[1] ) {
        m_data[m_offset[1]+1] |= CNcbi8naBase( CNcbi8naBase(  m_data[m_offset[1]] ), CColorTwoBase(m_data[m_offset[1] + 1]) );
        --m_length[1]; 
        ++m_offset[1]; 
    }
}

#endif

inline CQuery::CQuery( CSeqCoding::ECoding coding, const string& id, const string& data1, const string& data2, int base ) : m_data(0), m_flags(0)
{
    m_topHit[0] = m_topHit[1] = m_topHit[2] = 0;
    switch( coding ) {
    case CSeqCoding::eCoding_ncbi8na: x_InitNcbi8na( id, data1, data2, base ); break;
    case CSeqCoding::eCoding_ncbiqna: x_InitNcbiqna( id, data1, data2, base ); break;
    case CSeqCoding::eCoding_ncbipna: x_InitNcbipna( id, data1, data2, base ); break;
#if DEVELOPMENT_VER
    case CSeqCoding::eCoding_colorsp: x_InitColorspace( id, data1, "", data2, "", base ); break;
#else
    case CSeqCoding::eCoding_colorsp: x_InitColorspace( id, data1, data2, base ); break;
#endif
    default: THROW( logic_error, "Invalid coding " << coding );
    }
    ++s_count;
}

inline CSeqCoding::ECoding CQuery::GetCoding() const 
{ 
    switch( m_flags & fCoding_BITS ) { 
    case fCoding_ncbi8na: return CSeqCoding::eCoding_ncbi8na;
    case fCoding_ncbipna: return CSeqCoding::eCoding_ncbipna; 
    case fCoding_ncbiqna: return CSeqCoding::eCoding_ncbiqna; 
    case fCoding_colorsp: return CSeqCoding::eCoding_colorsp;
    default: THROW( logic_error, "Invalid internal coding fields for query " << GetId() );
    }
}

inline unsigned CQuery::x_ComputeInformativeLength( const string& seq ) const
{
    unsigned l = seq.length();
    while( l && strchr(" \t\r\n\v\f",seq[l - 1]) ) --l; // removed clipping of trailing Ns and .s to keep coordinates correct
    return l;
}

inline void CQuery::MarkPositionAmbiguous( int component, int pos ) 
{
    if( pos < 0 || pos >= m_length[component] ) return;
    switch( m_flags & fCoding_BITS ) { 
    case fCoding_ncbi8na: m_data[m_offset[component] + pos] = 0x0f; break; 
    case fCoding_ncbiqna: m_data[m_offset[component] + pos] = 0x00; break; 
    case fCoding_ncbipna: memset( m_data + m_offset[component] + pos*5, '\xff', 5 ); break; 
    default:;
    }
}

inline void CQuery::x_InitNcbi8na( const string& id, const string& data1, const string& data2, int )
{
    m_flags &= ~fCoding_BITS;
    m_flags |= fCoding_ncbi8na;
    ASSERT( data1.length() < 255 );
    ASSERT( data2.length() < 255 );
    m_length[0] = x_ComputeInformativeLength( data1 );
    m_length[1] = x_ComputeInformativeLength( data2 );
    unsigned sz = id.length() + m_length[0] + m_length[1] + 1;
    m_data = new unsigned char[sz];
    memset( m_data, 0, sz );
    strcpy( (char*)m_data, id.c_str() );
    m_offset[0] = id.length() + 1;
    m_offset[1] = m_offset[0] + m_length[0];
    for( unsigned i = 0, j = m_offset[0]; i < m_length[0]; )
        m_data[j++] = CNcbi8naBase( CIupacnaBase( data1[i++] ) );
    for( unsigned i = 0, j = m_offset[1]; i < m_length[1]; )
        m_data[j++] = CNcbi8naBase( CIupacnaBase( data2[i++] ) );
}

inline void CQuery::x_InitNcbiqna( const string& id, const string& data1, const string& data2, int base )
{
    m_flags &= ~fCoding_BITS;
    m_flags |= fCoding_ncbiqna;
    ASSERT( data1.length() < 255 );
    ASSERT( data2.length() < 255 );
    m_length[0] = x_ComputeInformativeLength( data1.substr( 0, data1.length()/2 ) );
    m_length[1] = x_ComputeInformativeLength( data2.substr( 0, data2.length()/2 ) );
    unsigned sz = id.length() + m_length[0] + m_length[1] + 1;
    m_data = new unsigned char[sz];
    memset( m_data, 0, sz );
    strcpy( (char*)m_data, id.c_str() );
    m_offset[0] = id.length() + 1;
    m_offset[1] = m_offset[0] + m_length[0];
    for( unsigned i = 0, j = m_offset[0], k = m_length[0]; i < m_length[0]; )
        m_data[j++] = CNcbiqnaBase( CNcbi8naBase( CIupacnaBase( data1[i++] ) ), data1[k++] - base );
    for( unsigned i = 0, j = m_offset[1], k = m_length[1]; i < m_length[1]; )
        m_data[j++] = CNcbiqnaBase( CNcbi8naBase( CIupacnaBase( data2[i++] ) ), data2[k++] - base );
}

template<class iterator>
inline iterator CQuery::Solexa2Ncbipna( iterator dest, const string& line, int )
{
    istringstream in( line );
    while( !in.eof() ) {
        int a,c,g,t;
        in >> a >> c >> g >> t;
        if( in.fail() ) throw runtime_error( "Bad solexa line format: [" + line + "]" );
        double exp = max( max( a, c ), max( g, t ) );
        double base = 255 / std::pow( 2., exp );
        // TODO: correct
        *dest++ = unsigned (base * std::pow(2., a));
        *dest++ = unsigned (base * std::pow(2., c));
        *dest++ = unsigned (base * std::pow(2., g));
        *dest++ = unsigned (base * std::pow(2., t));
        *dest++ = '\xff';
    }
    return dest;
}



inline void CQuery::x_InitNcbipna( const string& id, const string& data1, const string& data2, int base )
{
    m_flags &= ~fCoding_BITS;
    m_flags |= fCoding_ncbipna;
    vector<unsigned char> buff1, buff2;

    Solexa2Ncbipna( back_inserter( buff1 ), data1, base );

    if( data2.length() ) Solexa2Ncbipna( back_inserter( buff2 ), data2, base );
    m_data = new unsigned char[id.length() + 1 + buff1.size() + buff2.size()];
    ASSERT( buff1.size()%5 == 0 );
    ASSERT( buff2.size()%5 == 0 );
    ASSERT( buff1.size()/5 < 255 );
    ASSERT( buff2.size()/5 < 255 );
    m_length[0] = buff1.size()/5;
    m_length[1] = buff2.size()/5;
    strcpy( (char*)m_data, id.c_str());
    memcpy( m_data + ( m_offset[0] = id.length() + 1 ), &buff1[0], buff1.size() );
    memcpy( m_data + ( m_offset[1] = m_offset[0] + buff1.size() ), &buff2[0], buff2.size() );
}

inline void CQuery::x_InitColorspace( const string& id, const string& data1, const string& data2, int )
{
    m_flags &= ~fCoding_BITS;
    m_flags |= fCoding_colorsp;
    ASSERT( data1.length() < 255 );
    ASSERT( data2.length() < 255 );
    m_length[0] = data1.length();
    m_length[1] = data2.length();
    unsigned sz = id.length() + m_length[0] + m_length[1] + 1;
    m_data = new unsigned char[sz];
    memset( m_data, 0, sz );
    strcpy( (char*)m_data, id.c_str() );
    m_offset[0] = id.length() + 1;
    m_offset[1] = m_offset[0] + m_length[0];
    for( unsigned i = 0, j = m_offset[0]; i < m_length[0]; ) 
        m_data[j++] = CColorTwoBase( CColorTwoBase::eFromASCII, data1[i++] );
    for( unsigned i = 0, j = m_offset[1]; i < m_length[1]; )
        m_data[j++] = CColorTwoBase( CColorTwoBase::eFromASCII, data2[i++] );
}

inline ostream& CQuery::PrintIupac( ostream& dest, int component, CSeqCoding::EStrand strand ) const 
{
    switch( GetCoding() ) {
        case CSeqCoding::eCoding_ncbi8na: return PrintSeq<CNcbi8naBase>( dest, component, strand );
        case CSeqCoding::eCoding_ncbiqna: return PrintSeq<CNcbiqnaBase>( dest, component, strand );
        case CSeqCoding::eCoding_ncbipna: return PrintSeq<CNcbipnaBase>( dest, component, strand );
        case CSeqCoding::eCoding_colorsp: return PrintDibaseAsIupac( dest, component, strand );
        default: THROW( logic_error, "Unexpected encoding of sequence: " << GetCoding() );
    }
}

template<typename Base>
inline ostream& CQuery::PrintSeq( ostream& out, int component, CSeqCoding::EStrand strand ) const 
{
    Export<Base>( ostream_iterator<CIupacnaBase>( out ), component, strand );
    return out;
}

template<typename Base, typename Iter>
inline Iter CQuery::Export( Iter dest, int component, CSeqCoding::EStrand strand ) const 
{
    int incr = GetBytesPerBase();
    const char * x = GetData( component );
    const char * X = x + GetLength( component ) * incr;
    if( strand ==  CSeqCoding::eStrand_neg ) { incr = - incr; swap( x, X ); x += incr; X += incr; }
    for(; x != X; x += incr ) { *dest++ = CIupacnaBase( Base( x, strand ) ); }
    return dest;
}

inline ostream& CQuery::PrintDibaseAsIupac( ostream& dest, int component, CSeqCoding::EStrand strand ) const 
{
    vector<char> o(GetLength(component));
    int incr = GetBytesPerBase();
    const char * x = GetData( component );
    const char * X = x + GetLength( component ) * incr;
    o[0] = CIupacnaBase( CColorTwoBase( x ) ); x += incr;
    for( int i = 0; x != X; ++i, (x += incr) ) o[i+1] = CIupacnaBase( o[i], CColorTwoBase( x ) );
    if( strand ==  CSeqCoding::eStrand_neg ) {
        for( vector<char>::reverse_iterator t = o.rbegin(); t != o.rend(); ++t )
            dest << CIupacnaBase( *t ).Complement();
    } else {
        for( vector<char>::const_iterator t = o.begin(); t != o.end(); ++t )
            dest << CIupacnaBase( *t );
    }
    return dest;
}

END_OLIGOFAR_SCOPES

#endif
