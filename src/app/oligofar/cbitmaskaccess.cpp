#include <ncbi_pch.hpp>
#include "cbitmaskaccess.hpp"
#include "uinth.hpp"

USING_OLIGOFAR_SCOPES;

bool CBitmaskAccess::Open( const string& name ) 
{
    ifstream in( name.c_str() );
    if( in.fail() ) THROW( runtime_error, "Failed to open file "+name+": "+strerror( errno ) );
    Uint8 order;
    in.read( (char*)&order, sizeof( order ) );
    if( order != 0x01234567 )
        THROW( runtime_error, "Oops... file "+name+" has data with wrong byte order" );
    Uint4 headerSize = 0;
    Uint4 bpunit = 0;
    Uint8 bitCount = 0;
    vector<char> signature( SignatureSize() );
    in.read( (char*)&headerSize, sizeof( headerSize ) );
    in.read( (char*)&bpunit,     sizeof( bpunit ) );
    in.read( (char*)&m_size,     sizeof( m_size ) );
    in.read( (char*)&m_maxAmb,   sizeof( m_maxAmb ) );
    in.read( (char*)&m_wSize,    sizeof( m_wSize ) );
    in.read( (char*)&m_wStep,    sizeof( m_wStep ) );
    in.read( (char*)&bitCount,   sizeof( bitCount ) );
    /*
    cerr 
        << DISPLAY( order ) << "\n"
        << DISPLAY( headerSize ) << "\n"
        << DISPLAY( bpunit ) << "\n"
        << DISPLAY( m_size ) << "\n"
        << DISPLAY( m_maxAmb ) << "\n"
        << DISPLAY( m_wSize ) << "\n"
        << DISPLAY( m_wStep ) << "\n"
        << DISPLAY( bitCount ) << "\n";
        */
    if( in.fail() )
        THROW( runtime_error, "Oops... failed to read header from " << name << ": " << strerror( errno ) );
    if( headerSize < SignatureSize() + in.tellg() - sizeof( order ) ) 
        THROW( runtime_error, "Oops... short header in " << name );
    in.read( &signature[0], SignatureSize() );
    if( in.fail() ) 
        THROW( runtime_error, "Oops... failed to read signature from " << name << ": " << strerror( errno ) );
    if( strncmp( &signature[0], Signature(), signature.size() ) || signature.back() )
        THROW( runtime_error, "Oops... wrong signature or version in " << name << " expected [" << Signature() << "] got [" << string( &signature[0], signature.size() ) << "]" );
    Uint8 bcnt = bpunit * Uint8( m_size );
    if( bcnt != Uint8(UintH(1) << int(2*m_wSize)) )
        THROW( runtime_error, "Oops... inconsistent data in header: bits per unit " << bpunit << ", size " << m_size << " so total word count is " 
                << bcnt << ", and word size " << m_wSize << " so that total word count should be " << (Uint8(1) << m_wSize ) << " in file " << name );
    if( bpunit != 32 ) 
        THROW( runtime_error, "Oops... don't know how to deal with bits per unit other, than 32 (got " << bpunit << ") in file " << name );
    in.seekg( headerSize + sizeof( order ) );
    if( in.fail() )
        THROW( runtime_error, "Oops... failed seek to " << headerSize << " + " << sizeof( order ) << " in " << name << ": " << strerror( errno ) );
    m_data = new Uint4[Uint8( m_size )];
    in.read( (char*)m_data, m_size*sizeof( *m_data ) );
    if( in.fail() ) 
        THROW( runtime_error, "Oops... failed to read " << m_size << " * " << sizeof( *m_data ) << " bytes from " << name << ": " << strerror( errno ) );
    if( in.gcount() != int(m_size*sizeof( *m_data )))
        THROW( runtime_error, "Oops... got only " << in.gcount()  << " bytes (expected " << m_size << "*" << sizeof( *m_data ) << ") from " << name );
    m_wMask = ~((~Uint8(0)) << (m_wSize*2));
    //cerr << DISPLAY( m_data[0] ) << DISPLAY( m_data[1] ) << DISPLAY( m_data[m_size - 1] ) << DISPLAY( m_wMask ) << endl;
    return true;
}

void CBitmaskAccess::Close() 
{
    delete [] m_data; 
}


    
