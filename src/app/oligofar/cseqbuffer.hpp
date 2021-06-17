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

    CSeqBuffer( const objects::CSeqVector& vect, CSeqCoding::ECoding );
    ~CSeqBuffer() { if( m_bufferAllocated ) delete [] m_begin; }

    CSeqCoding::ECoding GetCoding() const { return m_coding; }

protected:
    char * m_begin;
    char * m_end;
    int  m_bufferOffset;
    bool m_bufferAllocated;
    CSeqCoding::ECoding m_coding;

private:
    explicit CSeqBuffer( const CSeqBuffer& ) { THROW( logic_error, "CSeqBuffer copy constructor is prohibited!" ); } 
    CSeqBuffer& operator = ( const CSeqBuffer& );
};

////////////////////////////////////////////////////////////////////////
// implementation

inline CSeqBuffer::CSeqBuffer( const objects::CSeqVector& vect, CSeqCoding::ECoding tgtCoding )
    : m_begin( new char[vect.size() + 1] ),
      m_end( m_begin + vect.size() ),
      m_bufferOffset( 0 ),
      m_bufferAllocated( true ),
      m_coding( tgtCoding )
{
    string s; 
    vect.GetSeqData( 0, vect.size(), s ); 
    const char * a = s.c_str();
    char * b = m_begin;
    switch( tgtCoding ) {
    case CSeqCoding::eCoding_colorsp: 
    case CSeqCoding::eCoding_ncbi8na: while( b != m_end ) *b++ = CNcbi8naBase( CIupacnaBase( *a++ ) ); break;
                                          /*
    case CSeqCoding::eCoding_colorsp: 
        do {
            CNcbi8naBase prev('\x0');
            while( b != m_end ) {
                CNcbi8naBase n( CIupacnaBase( *a++ ) );
                *b++ = CColorTwoBase( prev, n );
                prev = n;
            }
        } while(0);
        break;
        */
    default: THROW( logic_error, "CSeqBuffer supports only NCBI8na and Colorspace target encodings" );
    }
}

END_OLIGOFAR_SCOPES

#endif
