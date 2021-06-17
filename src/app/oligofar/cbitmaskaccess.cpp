#include <ncbi_pch.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "cbitmaskaccess.hpp"
#include "uinth.hpp"

USING_OLIGOFAR_SCOPES;

bool CBitmaskAccess::Open( const string& name, EOpenMode openMode ) 
{
    Close();
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
    Uint4 pattern = 0;
    //cerr << "* Basic header read: " << DISPLAY( headerSize ) << DISPLAY( in.tellg() ) << DISPLAY( SignatureSize() ) << DISPLAY( sizeof( order ) ) << "\n";
    if( headerSize > (Uint8)in.tellg() + SignatureSize() - sizeof( order ) ) { 
        in.read( (char*)&pattern, sizeof( pattern ) );
        //cerr << "* Reading pattern from header: " << hex << pattern << dec << "\n";
    } else {
        pattern = CBitHacks::WordFootprint<Uint4>( m_wSize );
        //cerr << "* Setting pattern from wordsz: " << hex << pattern << dec << "\n";
    }
    /*
    cerr << "\x1b[31mDEBUG:\n"
        << DISPLAY( order ) << "\n"
        << DISPLAY( headerSize ) << "\n"
        << DISPLAY( bpunit ) << "\n"
        << DISPLAY( m_size ) << "\n"
        << DISPLAY( m_maxAmb ) << "\n"
        << DISPLAY( m_wSize ) << "\n"
        << DISPLAY( m_wStep ) << "\n"
        << DISPLAY( bitCount ) << "\n"
        << "\x1b[0m";
        */
    if( in.fail() )
        THROW( runtime_error, "Oops... failed to read header from " << name << ": " << strerror( errno ) );
    if( headerSize < SignatureSize() + in.tellg() - sizeof( order ) ) 
        THROW( runtime_error, "Oops... short header in " << name );
    in.read( &signature[0], SignatureSize() );
    if( in.fail() ) 
        THROW( runtime_error, "Oops... failed to read signature from " << name << ": " << strerror( errno ) );
    if( signature.back() )
        THROW( runtime_error, "Oops... too long signature in " << name << ": got [" << string( &signature[0], signature.size() ) << "]" );
    if( strncmp( &signature[0], Signature(eVersion_0_0_0), signature.size() ) == 0 ) {
        m_version = 0;
    } else if( strncmp( &signature[0], Signature(eVersion_0_1_0), signature.size() ) == 0 ) {
        m_version = 1;
    } else 
        THROW( runtime_error, "Oops... wrong signature or version in " << name << ": got [" << string( &signature[0], signature.size() ) << "]" );
    //if( 0 == strncmp( &signature[0], Signature(eVersion_0_1_0), signature.size() ) && pattern ) 
    SetWindowPattern( pattern );
    Uint8 bcnt = bpunit * Uint8( m_size );
    if( bcnt != Uint8(UintH(1) << int(2*m_wSize)) )
        THROW( runtime_error, "Oops... inconsistent data in header: bits per unit " << bpunit << ", size " << m_size << " so total word count is " 
                << bcnt << ", and word size " << m_wSize << " so that total word count should be " << (Uint8(1) << m_wSize ) << " in file " << name );
    if( bpunit != 32 ) 
        THROW( runtime_error, "Oops... don't know how to deal with bits per unit other, than 32 (got " << bpunit << ") in file " << name );
    m_wMask = ~((~Uint8(0)) << (m_wSize*2));
#ifndef _WIN32
    if( openMode == eOpen_mmap ) {
        in.close();
        int fd = open( name.c_str(), O_RDONLY );
        if( fd == -1 ) 
            THROW( runtime_error, "Oops... failed to open() file " << name << ": " << strerror( errno ) );
        m_offset = headerSize + sizeof( order );
        //cerr << DISPLAY( m_offset ) << DISPLAY( headerSize ) << DISPLAY( sizeof( order ) ) << "\n";
        Uint8 len = Uint8(m_size)*sizeof(*m_data) + m_offset;
        char * d = (char*)mmap( 0, len, PROT_READ, MAP_PRIVATE|MAP_NORESERVE, fd, 0 );
        if( d == MAP_FAILED ) {
            m_data = 0;
            close( fd );
            THROW( runtime_error, "Oops... failed to mmap() file " << name << ": " << strerror( errno ) );
        }
        madvise( d, len, MADV_RANDOM|MADV_WILLNEED );
        m_data = (Uint4*)(d + m_offset);
        close( fd );
        m_openMode = openMode;
        return true;
    }
#endif
    in.seekg( headerSize + sizeof( order ) );
    if( in.fail() )
        THROW( runtime_error, "Oops... failed seek to " << headerSize << " + " << sizeof( order ) << " in " << name << ": " << strerror( errno ) );
    m_data = new Uint4[Uint8( m_size )];
    in.read( (char*)m_data, m_size*sizeof( *m_data ) );
    if( in.fail() ) 
        THROW( runtime_error, "Oops... failed to read " << m_size << " * " << sizeof( *m_data ) << " bytes from " << name << ": " << strerror( errno ) );
    if( Uint8(in.gcount()) != Uint8(m_size*sizeof( *m_data )))
        THROW( runtime_error, "Oops... got only " << in.gcount()  << " bytes (expected " << m_size << "*" << sizeof( *m_data ) << ") from " << name );
    //cerr << DISPLAY( m_data[0] ) << DISPLAY( m_data[1] ) << DISPLAY( m_data[m_size - 1] ) << DISPLAY( m_wMask ) << endl;
    return true;
}

void CBitmaskAccess::Close() 
{
    if( m_data == 0 ) return;
    if( m_openMode == eOpen_mmap ) {
        munmap( ((char*)m_data) - m_offset, m_size*sizeof( *m_data ) );
        m_data = 0;
    }
    delete [] m_data; 
}


    
