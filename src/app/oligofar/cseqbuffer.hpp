#ifndef OLIGOFAR_CSEQBUFFER__HPP
#define OLIGOFAR_CSEQBUFFER__HPP

#include "defs.hpp"
#include "cseqcoding.hpp"

#include <objmgr/seq_vector.hpp>

BEGIN_OLIGOFAR_SCOPES

class CSeqBuffer
{
public:
    const char * GetBeginPtr() const { return m_begin; }
    const char * GetEndPtr() const { return m_end; }

    int GetLength() const { return m_end - m_begin; }
    int GetBeginPos() const { return m_bufferOffset; }
    int GetEndPos() const { return m_bufferOffset + GetLength(); }

    CSeqBuffer( const objects::CSeqVector& vect );
    ~CSeqBuffer() { if( m_bufferAllocated ) delete m_begin; }

protected:
    char * m_begin;
    char * m_end;
    int  m_bufferOffset;
    bool m_bufferAllocated;

protected:
	explicit CSeqBuffer( const CSeqBuffer& ) { THROW( logic_error, "CSeqBuffer copy constructor is prohibited!" ); } 
};

////////////////////////////////////////////////////////////////////////
// implementation

inline CSeqBuffer::CSeqBuffer( const objects::CSeqVector& vect )
    : m_begin( new char[vect.size() + 1] ),
      m_end( m_begin + vect.size() ),
      m_bufferOffset( 0 ),
      m_bufferAllocated( true )
{
    string s; 
    vect.GetSeqData( 0, vect.size(), s ); 
	const char * a = s.c_str();
	for( char * b = m_begin; b != m_end; ) {
		*b++ = CNcbi8naBase( CIupacnaBase( *a++ ) );
	}
}

END_OLIGOFAR_SCOPES

#endif
