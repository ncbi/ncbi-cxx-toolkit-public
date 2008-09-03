#ifndef OLIGOFAR_CQUERY__HPP
#define OLIGOFAR_CQUERY__HPP

#include "iupac-util.hpp"
#include "cseqcoding.hpp"
#include "debug.hpp"
#include "chit.hpp"

BEGIN_OLIGOFAR_SCOPES

class CHit;
class CScoreTbl;
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
		fNONE = 0
	};
    static Uint4 GetCount() { return s_count; }

	CQuery( CSeqCoding::ECoding coding, const string& id, const string& data1, const string& data2 = "", int base = kDefaultQualityBase );
	~CQuery() { delete m_topHit; delete [] m_data; --s_count; }

	bool IsPairedRead() const { return m_length[1] != 0; }
	bool HasComponent( int i ) const { return GetLength( i ) != 0; }

	const char* GetId() const { return (const char*)m_data; }
	const char* GetData( int i ) const { return (const char*)m_data + m_offset[i]; }

	unsigned GetLength( int i ) const { return m_length[i]; }
	unsigned GetDataSize( int i ) const { return ( GetCoding() == CSeqCoding::eCoding_ncbipna ? 5 : 1 ) * m_length[i]; }

	CSeqCoding::ECoding GetCoding() const;
    CHit * GetTopHit() const { return m_topHit; }
	void ClearHits() { delete m_topHit; m_topHit = 0; }

	double ComputeBestScore( const CScoreTbl& scoring, int component );
	double GetBestScore(int i) const { return m_bestScore[i]; }

private:
    explicit CQuery( const CQuery& q );

	template<class CBase, int incr, CSeqCoding::ECoding coding>
	double x_ComputeBestScore( const CScoreTbl& scoring, int component );
    
protected:
	void x_InitNcbi8na( const string& id, const string& data1, const string& data2, int base );
	void x_InitNcbiqna( const string& id, const string& data1, const string& data2, int base );
	void x_InitNcbipna( const string& id, const string& data1, const string& data2, int base );
	void x_InitColorspace( const string& id, const string& data1, const string& data2, int base );
    unsigned x_ComputeInformativeLength( const string& seq ) const;

protected:
	unsigned char * m_data; // contains id\0 data1 data2
	unsigned short  m_offset[2]; // m_offset[1] should always point to next byte after first seq data
	unsigned char   m_length[2];
	unsigned short  m_flags;
	float m_bestScore[2];
    CHit * m_topHit;
    static Uint4 s_count;
};

////////////////////////////////////////////////////////////////////////
// Implementation

inline CQuery::CQuery( CSeqCoding::ECoding coding, const string& id, const string& data1, const string& data2, int base ) : m_data(0), m_flags(0), m_topHit(0)
{
	switch( coding ) {
	case CSeqCoding::eCoding_ncbi8na: x_InitNcbi8na( id, data1, data2, base ); break;
	case CSeqCoding::eCoding_ncbiqna: x_InitNcbiqna( id, data1, data2, base ); break;
	case CSeqCoding::eCoding_ncbipna: x_InitNcbipna( id, data1, data2, base ); break;
	case CSeqCoding::eCoding_colorsp: x_InitColorspace( id, data1, data2, base ); break;
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
    while( l && strchr("N. \t",seq[l - 1]) ) --l;
    return l;
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

END_OLIGOFAR_SCOPES

#endif
