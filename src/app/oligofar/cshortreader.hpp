#ifndef OLIGOFAR_CSHORTREADER__HPP
#define OLIGOFAR_CSHORTREADER__HPP

#include "defs.hpp"
#include "cseqcoding.hpp"

BEGIN_OLIGOFAR_SCOPES

// GetSeqCoding() returns intended internal representation
// GetReadData() 
//  for iupac and solid returns as is
//  for fastq returns 2*l string ACGTIIII
//  for solexa returns as is - string of quads of signed integers

class IShortReader
{
public:
    virtual ~IShortReader() {}
    virtual CSeqCoding::ECoding GetSeqCoding() const = 0;
    virtual const char * GetReadData( int i ) const = 0;
    virtual const char * GetReadId() const = 0; 
    virtual bool NextRead() = 0;
protected:
    typedef pair<auto_ptr<ifstream>,string> TFileStream;
    static void SetStream( TFileStream& stream, const string& name );
};

class CReaderFactory
{
public:
    CReaderFactory() : m_colorSpace( false ), m_qualityChannels( 0 ) {}
    IShortReader * CreateReader();
    ostream& GetErrorMesage( ostream& out ) const;
    void SetColorspace( bool on ) { m_colorSpace = on; }
    void SetQualityChannels( int count ) { m_qualityChannels = count; }
    void SetReadIdFile( const string& file ) { m_readIdFile = file; }
    void SetReadDataFile1( const string& file ) { m_readDataFile1 = file; }
    void SetReadDataFile2( const string& file ) { m_readDataFile2 = file; }
protected:
    unsigned x_MkFlags() const;
    enum EFlags {
        fQuality_0      = 0x000,
        fQuality_1      = 0x001,
        fQuality_4      = 0x002,
        fQuality_FLAGS  = 0x003,
        fColorspace     = 0x004,
        fReadIdFileSet  = 0x008,
        fReadDataFile1  = 0x010,
        fReadDataFile2  = 0x020
    };
    bool m_colorSpace;
    int  m_qualityChannels;
    string m_readIdFile;
    string m_readDataFile1;
    string m_readDataFile2;
};

class CColFileReader : public IShortReader
{
public:
    CColFileReader( bool quality, bool colorspace, const string& filename );
    virtual CSeqCoding::ECoding GetSeqCoding() const { return m_coding; }
    virtual const char * GetReadData( int i ) const { return m_readData[i].c_str(); }
    virtual const char * GetReadId() const { return m_readId.c_str(); }
    virtual bool NextRead();
protected:
    TFileStream m_input;
    string m_readId;
    string m_readData[2];
    CSeqCoding::ECoding m_coding;
};

class CSolexaFileReader : public IShortReader
{
public:
    CSolexaFileReader( const string& readIdFile, const string& readDataFile1, const string& readDataFile2 );
    virtual CSeqCoding::ECoding GetSeqCoding() const { return CSeqCoding::eCoding_ncbipna; }
    virtual const char * GetReadData( int i ) const { return m_readData[i].c_str(); }
    virtual const char * GetReadId() const { return m_readId.c_str(); }
    virtual bool NextRead();
protected:
    bool x_PairedReads() const { return m_dataStream2.second.length() > 0; }
protected:
    TFileStream m_idStream;
    TFileStream m_dataStream1;
    TFileStream m_dataStream2;
    string m_readId;
    string m_readData[2];
};
    
class CFastqFileReader : public IShortReader
{
public:
    CFastqFileReader( bool quality, bool colorspace, const string& readDataFile1, const string& readDataFile2 );
    virtual CSeqCoding::ECoding GetSeqCoding() const { return m_coding; }
    virtual const char * GetReadData( int i ) const { return m_readData[i].c_str(); }
    virtual const char * GetReadId() const { return m_readId.c_str(); }
    virtual bool NextRead();
protected:
    bool x_PairedReads() const { return m_dataStream2.second.length() > 0; }
    bool x_FetchIdLine( TFileStream& stream, string& id, char type = '@', const string& compare = "", bool allowComponentDifference = false );
    bool x_FetchReadData( TFileStream& stream, const string& id, string& dest, int expectedLen );
    string& x_TrimTrailingSpaces( string& buff ) {
        int l = buff.size();
        while( l > 0 && isspace( buff[l-1] ) ) --l;
        buff.resize( l );
        return buff;
    }
protected:
    TFileStream m_dataStream1;
    TFileStream m_dataStream2;
    string m_readId;
    string m_readData[2];
    CSeqCoding::ECoding m_coding;
    bool m_clipReadId;
    string m_buff;
};
    

END_OLIGOFAR_SCOPES

#endif
