#ifndef OLIGOFAR__CBITMASKBUILDER__HPP
#define OLIGOFAR__CBITMASKBUILDER__HPP

#include "uinth.hpp"
#include "cbitmaskbase.hpp"
#include "cseqvecprocessor.hpp"

BEGIN_OLIGOFAR_SCOPES

class CBitmaskBuilder : public CBitmaskBase, public CSeqVecProcessor::ICallback
{
public:
    ~CBitmaskBuilder() { delete[] m_data; }
    CBitmaskBuilder( int maxamb, int wsize, int wstep );

    virtual void SequenceBegin( const TSeqIds& ids, int ) { m_seqId = ids.size() ? ids.front()->AsFastaString()  : string("unknown"); }
    virtual void SequenceBuffer( CSeqBuffer * ncbi8na );
    virtual void SequenceEnd() {}

    void SetBit( Uint8 word ) { m_data[word/32] |= (1<<(word%32)); }

    bool WriteByteOrder( ostream& o ) const { Uint8 order = 0x01234567; o.write( (const char*)&order, sizeof( order ) ); return o.good(); }
    bool WriteSignature( ostream& o ) const { o.write( (const char*)Signature(m_version), SignatureSize() ); return o.good(); }
    bool WriteData( ostream& o ) const { o.write( (const char*)m_data, sizeof( *m_data ) * m_size ); return o.good(); }
    
    void SetVersion( int ver ) { ASSERT( ver >= 0 && ver < eVersion_COUNT ); m_version = ver; }

    bool WriteHeader( ostream& o ) const { 
        Uint4 bpunit = 32;
        Uint4 headerSize = sizeof( headerSize ) + sizeof( bpunit ) + sizeof( m_size ) + sizeof( m_maxAmb ) + sizeof( m_wSize ) + sizeof( m_wStep ) + sizeof( m_bitCount );
        headerSize += SignatureSize();
        if( m_version == eVersion_0_1_0 ) headerSize += sizeof( m_wPattern );
        o.write( (const char*)&headerSize, sizeof( headerSize ) );
        o.write( (const char*)&bpunit,     sizeof( bpunit     ) ); 
        o.write( (const char*)&m_size,     sizeof( m_size     ) ); 
        o.write( (const char*)&m_maxAmb,   sizeof( m_maxAmb   ) );
        o.write( (const char*)&m_wSize,    sizeof( m_wSize    ) );
        o.write( (const char*)&m_wStep,    sizeof( m_wStep    ) );
        o.write( (const char*)&m_bitCount, sizeof( m_bitCount ) );
        if( m_version == eVersion_0_1_0 ) 
            o.write( (const char*)&m_wPattern, sizeof( m_wPattern ) );
        return o.good();
    }
    bool Write( ostream& out ) const { 
        return 
            WriteByteOrder( out ) && 
            WriteHeader( out ) && 
            WriteSignature( out ) && 
            WriteData( out ); 
    }
    bool Write( const string& name ) const;

    Uint8 CountBits();
    Uint8 GetBitcount() const { return m_bitCount; }
    Uint8 GetSize() const { return m_size; }
    Uint8 GetTotalBits() const { return 32 * m_size; }

protected:
    Uint8   m_bitCount;
    string  m_seqId;
};

END_OLIGOFAR_SCOPES

#endif

