#include <ncbi_pch.hpp>
#include "oligofar-version.hpp"
#include "cprogressindicator.hpp"
#include "cbitmaskaccess.hpp"
#include "cshortreader.hpp"
#include "csamrecords.hpp"
#include "fourplanes.hpp"
#include "cgetopt.hpp"

USING_OLIGOFAR_SCOPES;
USING_SCOPE( fourplanes );

class CReadTagger
{
public:
    enum ETagValue { eTag_foreign, eTag_uncertain, eTag_human };
    enum EOutputFile {
        eOutput_report,
        eOutput_tag,
        eOutput_long1,
        eOutput_long2,
        eOutput_short1,
        eOutput_short2,
        eOutput_COUNT
    };
    enum EActions {
        fAction_tag = 0x01,
        fAction_post = 0x02,
        fAction_reportInputSeq = 0x10,
        fAction_reportClippedSeq = 0x20,
        fAction_reportInputLen = 0x40,
        fAction_reportClippedLen = 0x80,
        fAction_reportWordGraph = 0x100,
        fAction_reportWordTotal = 0x200,
        fAction_reportWordMatchCount = 0x400,
        fAction_reportWordMatchLongest = 0x800,
        fAction_reportNoseq = 0xfc0,
        fAction_reportALL = 0xfff0
    };
    typedef vector<pair<CBitmaskAccess*,bool> > TBitmaskAccessVector;
    typedef Uint8 TActions;

    CReadTagger();
    ~CReadTagger();

    void Complete();

    int GetMinWordsCutoff() const { return m_minWordsCutoff; }
    int GetLongWordsSwitch() const { return m_longWordsSwitch; }
    int GetShortSeqSwitch() const { return m_shortSeqSwitch; }
    int GetComponents() const { return m_components; }
    int GetQualityChannels() const { return m_qualityChannels; }
    int GetMaxAmbiguities() const { return m_maxAmb; }
    int GetClipNwindow() const { return m_clipNwindow; }
    int GetClipQuality() const { return m_clipQuality; }
    bool GetClipLowercase() const { return m_clipLowercase; }
    const string& GetOutputBasename() const { return m_basename; }
    const TActions& GetActions() const { return m_actions; }

    int& SetMinWordsCutoff() { return m_minWordsCutoff; }
    int& SetLongWordsSwitch() { return m_longWordsSwitch; }
    int& SetShortSeqSwitch() { return m_shortSeqSwitch; }
    int& SetComponents() { return m_components; }
    int& SetQualityChannels() { return m_qualityChannels; }
    int& SetMaxAmbiguities() { return m_maxAmb; }
    int& SetClipNwindow() { return m_clipNwindow; }
    int& SetClipQuality() { return m_clipQuality; }
    bool& SetClipLowercase() { return m_clipLowercase; }
    string& SetOutputBasename() { return m_basename; }
    TActions& SetActions() { return m_actions; }

    void SetMinWordsCutoff( int val ) { m_minWordsCutoff = val; }
    void SetLongWordsSwitch( int val ) { m_longWordsSwitch = val; }
    void SetShortSeqSwitch( int val ) { m_shortSeqSwitch = val; }
    void SetComponents( int c ) { m_components = c; }
    void SetQualityChannels( int q ) { m_qualityChannels = q; }
    void SetMaxAmbiguities( int a ) { m_maxAmb = a; }
    void SetClipNwindow( int w ) { m_clipNwindow = w; }
    void SetClipQuality( int c ) { m_clipQuality = c; }
    void SetClipLowercase( bool l ) { m_clipLowercase = l; }
    void SetOutputBasename( const string& o ) { m_basename = o; }
    void SetActions( const TActions& mask, bool on ) { if( on ) m_actions |= mask; else m_actions &= ~mask; }

    void AddBitmaskAccess( CBitmaskAccess * bmaccess, bool takeOwnership ) {
        if( bmaccess ) m_bmaccess.push_back( make_pair( bmaccess, takeOwnership ) );
    }
    
    ETagValue GetTagValue() const {
        if( m_humanCnt ) return eTag_human;
        if( m_foreignCnt && (m_uncertainCnt == 0 ) ) return eTag_foreign;
        return eTag_uncertain;
    }
    ETagValue ProcessReadData( const char * id, const char * read ); // if qual == 1 read1, read2 are concatenamed read,qual 
    ETagValue ProcessClippedRead( const char * iupac, int len, CSeqCoding::EStrand );
    ETagValue ProcessClippedRead( const vector<char>& ncbi8na, const CBitmaskAccess& bmaccess, CSeqCoding::EStrand );
    ETagValue PurgeRead();
    ETagValue Heuristic( int matchLongest, int matchCount, int wordCount, int wordSize );

    bool DecideEarly() const;

    size_t GetBitmaskCount() const { return m_bmaccess.size(); }
    const CBitmaskAccess& GetBitmask( int i ) { return *m_bmaccess[i].first; }

    ofstream& GetOutFile( EOutputFile );
    void PrintHeader( EOutputFile );
    void PrintFasta( const string& read, int component );
protected:
    int m_maxAmb;
    int m_components;
    int m_qualityChannels;
    int m_clipNwindow;
    int m_clipQuality;
    bool m_clipLowercase;
    string m_basename;

    int m_minWordsCutoff;
    int m_longWordsSwitch;
    int m_shortSeqSwitch;

    int m_humanCnt;
    int m_uncertainCnt;
    int m_foreignCnt;
    TBitmaskAccessVector m_bmaccess;

    TActions m_actions;

    string m_id;
    ETagValue m_tagValue;
    vector<string> m_reads;
    vector<string> m_quals;

    auto_ptr<ofstream> m_outputFile[eOutput_COUNT];
};

int main( int argc, char ** argv )
{
    set<string> bmask;
    list<string> in1;
    list<string> in2;
    CReadTagger tagger;
    CGetOpt getopt( argc, argv );
    getopt
        .AddGroup( "general" )
        .AddHelp( argv[0], "Filter reads based on word bitmask matches" )
        .AddVersion( argv[0], OLIGOFAR_VERSION, __DATE__ )
        .AddGroup( "input" )
        .AddArg( tagger.SetQualityChannels(), 'q', "quality-channels", "Number of quality channers for reads (0|1)" )
        .AddList( in1, '1', "read-1", "Fasta or fastq (for -q1) file with reads, may be repeated" )
        .AddList( in2, '2', "read-2", "Fasta or fastq (for -q1) file with read pair mates, if used should be repeated as many times as -1 is" )
        .AddSet( bmask, 'b', "word-bitmask", "Word bitmask file (may be repeated)" )
        .AddGroup( "input-filters" )
        .AddArg( tagger.SetMaxAmbiguities(), 'a', "max-ambiguities", "Maximal number of ambiguities per word" )
        .AddFlag( tagger.SetClipLowercase(), 'l', "clip-lowercase", "Should lowercase head and tail of each read be clipped" )
        .AddArg( tagger.SetClipNwindow(), 'N', "clip-N-win", "Clip sequence head or tail as long as it has at least one N per this long window" )
        .AddArg( tagger.SetClipQuality(), 'Q', "clip-quality", "Clip sequence head or tail with quality lower then this (for fastq input)" )
        .AddGroup( "output" )
        .AddArg( tagger.SetOutputBasename(), 'o', "output", "Output base name (suffixes will be added to it)" )
        .AddFlag( (Uint8)CReadTagger::fAction_tag, tagger.SetActions(), 'T', "tag", "Produce .tag file" )
        .AddFlag( (Uint8)CReadTagger::fAction_post, tagger.SetActions(), 'P', "post", "Produce .short?.fa and .long?.fa files" )
        .AddFlag( (Uint8)CReadTagger::fAction_reportNoseq, tagger.SetActions(), 'R', "report", "Produce .report file" )
        .AddArg( tagger.SetShortSeqSwitch(), 'L', "short-seq", "Set sequence length to consider it as short for postprocessing" )
        .AddGroup( "heuristics" )
        .AddArg( tagger.SetMinWordsCutoff(), 0, "min-words", "Set minimal word count to apply heuristics" )
        .AddArg( tagger.SetLongWordsSwitch(), 0, "many-words", "Set number of words which switches watermarks" )
        .Parse();
    if( getopt.Done() ) return getopt.GetResultCode();
    
    if( in2.size() ) {
        if( in2.size() != in1.size() ) 
            THROW( runtime_error, "if -2 is used it should be repeated exactly as many times as -1 is, usage is (" << in1.size() << ", " << in2.size() << ")" );
    }
    
    ITERATE( set<string>, bma, bmask ) {
        cerr << "* Attaching " << *bma << "...";
        cerr.flush();
        auto_ptr<CBitmaskAccess> b( new CBitmaskAccess( *bma ) );
        cerr << "\b\b\b (pattern = "
            << "0b" << CBitHacks::AsBits( b->GetWindowPattern(), b->GetWindowLength() ) 
            << " of len " << b->GetWindowLength() << " and " << (2*b->GetWordSize()) << " bits)\n";
        tagger.AddBitmaskAccess( b.release(), true );
    }

    CProgressIndicator p( "Processing input reads" );
    list<string>::const_iterator i1 = in1.begin();
    list<string>::const_iterator i2 = in2.begin();
    while( i1 != in1.end() ) {
        if( i1 != in1.begin() ) cerr << "\n"; //p.Summary();
        CReaderFactory rfactory;
        rfactory.SetQualityChannels( tagger.GetQualityChannels() );
        rfactory.SetReadDataFile1( *i1 );
        if( i2 != in2.end() ) { rfactory.SetReadDataFile2( *i2 ); tagger.SetComponents( 2 ); }
        else tagger.SetComponents( 1 );

        auto_ptr<IShortReader> reader( rfactory.CreateReader() );
        if( reader.get() == 0 ) {
            THROW( runtime_error, "Failed to create short read file reader" );
            return 10;
        }

        while( reader->NextRead() ) {
            tagger.ProcessReadData( reader->GetReadId(), reader->GetReadData(0) );
            if( tagger.GetComponents() > 1 )
                tagger.ProcessReadData( reader->GetReadId(), reader->GetReadData(1) );
            p.Increment();
        }
        ++i1;
        if( i2 != in2.end() ) ++i2;
    }
    tagger.PurgeRead();
    tagger.Complete();
    p.Summary();

    return 0;
}

CReadTagger::CReadTagger() :
    m_maxAmb( 0 ),
    m_components( 1 ),
    m_qualityChannels( 0 ),
    m_clipNwindow( 0 ),
    m_clipQuality( 0 ),
    m_clipLowercase( false ),
    m_basename( "" ),
    m_minWordsCutoff( 10 ),
    m_longWordsSwitch( 200 ),
    m_shortSeqSwitch( 101 ),
    m_actions( 0 )
{}

CReadTagger::~CReadTagger()
{
    Complete();
}

void CReadTagger::Complete() 
{
    ITERATE( TBitmaskAccessVector, bma, m_bmaccess )
        if( bma->second ) delete bma->first;
    m_bmaccess.clear();
    int errors = 0;
    for( unsigned i = 0; i < sizeof( m_outputFile )/sizeof( m_outputFile[0] ); ++i ) {
        if( m_outputFile[i].get() == 0 ) continue;
        m_outputFile[i]->flush();
        m_outputFile[i]->close();
        if( m_outputFile[i]->fail() ) ++errors;
        m_outputFile[i].reset(0);
    }
    if( errors )
        THROW( runtime_error, "There was a write error for " << errors << " output file" << (errors>1?"s":"") );
}

CReadTagger::ETagValue CReadTagger::ProcessReadData( const char * id, const char * data ) 
{
    bool newRead = false;
    if( id != m_id ) {
        PurgeRead();
        m_id = id;
        m_reads.clear();
        m_quals.clear();
        m_humanCnt = m_uncertainCnt = m_foreignCnt = 0;
        newRead = true;
    }
    if( DecideEarly() ) return GetTagValue();

    if( data == 0 ) return GetTagValue();
    int readLen = strlen( data );
    if( readLen == 0 ) return GetTagValue();
    
    // separate read and quality
    string read, qual;
    int l = strlen( data );
    if( m_qualityChannels > 1 ) {
        read.assign( data, l/2 );
        qual.assign( data + l/2 );
    } else read.assign( data );

    if( m_actions & fAction_post ) {
        m_reads.push_back( read );
        m_quals.push_back( qual );
    }
    
    // clip read as needed
    int b = 0; 
    int e = read.length();
    if( m_clipLowercase ) {
        while( b < e && islower( read[b] ) ) ++b;
        while( b < e && islower( read[e-1] ) ) --e;
    }
    if( m_clipQuality && qual.length() == read.length() ) {
        int qb = 0;
        int qe = read.length();
        while( qb < qe && qual[qb] - 33 < m_clipQuality ) ++qb;
        while( qb < qe && qual[qe - 1] - 33 < m_clipQuality ) --qe;
        if( qb > b ) b = qb;
        if( qe < e ) e = qe;
        if( b > e ) e = b;
    }
    if( m_clipNwindow > 0 ) {
        int bb = b;
        int ee = e;
        while( bb < e && bb - b < m_clipNwindow ) { if( toupper( read[bb] ) == 'N' ) b = bb + 1; ++bb; }
        while( b < ee && e - ee < m_clipNwindow ) { if( toupper( read[ee - 1] ) == 'N' ) e = ee - 2; --ee; }
    }
    if( e < b ) e = b;

    if( m_actions & fAction_reportALL ) {
        ofstream& o = GetOutFile( eOutput_report );
        if( newRead ) o << id;
        if( m_actions & fAction_reportInputSeq ) o << "\t" << read;
        if( m_actions & fAction_reportInputLen ) o << "\t" << read.size();
        if( m_actions & fAction_reportClippedSeq ) o << "\t" << read.substr( b, e-b );
        if( m_actions & fAction_reportClippedLen ) o << "\t" << ( e-b );
    }

    ProcessClippedRead( read.c_str() + b, (e-b), CSeqCoding::eStrand_pos );
    if( DecideEarly() ) return GetTagValue();
    ProcessClippedRead( read.c_str() + b, (e-b), CSeqCoding::eStrand_neg );
    return GetTagValue();
}

CReadTagger::ETagValue CReadTagger::ProcessClippedRead( const char * iupac, int len, CSeqCoding::EStrand strand )
{
    vector<char> read( len );
    switch( strand ) {
    case CSeqCoding::eStrand_pos: 
        for( unsigned i = 0; i < read.size(); ++i ) 
            read[i] = CNcbi8naBase( CIupacnaBase( iupac[i] ) );
        break;
    case CSeqCoding::eStrand_neg: 
        for( unsigned i = 0, j = read.size()-1; i < read.size(); ++i, --j ) 
            read[i] = CNcbi8naBase( CIupacnaBase( iupac[j] ) ).Complement();
        break;
    }
    ITERATE( TBitmaskAccessVector, bma, m_bmaccess ) {
        switch( ProcessClippedRead( read, *bma->first, strand ) ) {
        case eTag_human: m_humanCnt++; break;
        case eTag_foreign: m_foreignCnt++; break;
        case eTag_uncertain: m_uncertainCnt++; break;
        default: THROW( logic_error, "ProcessClippedRead() returned invalid value" );
        }
        if( DecideEarly() ) break;
    }
    return GetTagValue();
}

CReadTagger::ETagValue CReadTagger::ProcessClippedRead( const vector<char>& read, const CBitmaskAccess& bmaccess, CSeqCoding::EStrand strand )
{
    vector<bool> matches;
    int winsz = bmaccess.GetWindowLength();
    int i0 = winsz - 1;
    int wordCount = 0;
    int matchRun = 0;
    int matchCount = 0;
    int matchLongest = 0;
    if( int(read.size()) <= i0 ) { return eTag_uncertain; }
    if( m_actions & fAction_reportWordGraph ) matches.resize( read.size() - i0 );
    CHashGenerator hgen( winsz );
    for( unsigned i = 0, j = -i0; i < read.size(); ++i, ++j ) {
        hgen.AddBaseMask( read[i] );
        bool found = false;
        if( hgen.GetAmbiguityCount() <= GetMaxAmbiguities() ) {
            for( CHashIterator h( hgen ); h && !found; ++h ) {
                if( bmaccess.HasWindow( *h ) ) found = true;
            }
            if( matches.size() ) matches[j] = found;
            ++wordCount;
        }
        if( found ) {
            ++matchCount;
            ++matchRun;
        } else {
            if( matchRun > matchLongest ) matchLongest = matchRun;
            matchRun = 0;
        }
    }
    if( matchRun > matchLongest ) matchLongest = matchRun;
    if( m_actions & fAction_reportALL ) {
        // here report part wil be generated
        ofstream& o = GetOutFile( eOutput_report );
        if( m_actions & fAction_reportWordGraph ) {
            o << "\t";
            ITERATE( vector<bool>, g, matches ) o.put( (strand == CSeqCoding::eStrand_pos ? ".>" : ".<" )[*g] );
        }
        if( m_actions & fAction_reportWordTotal ) o << "\t" << wordCount;
        if( m_actions & fAction_reportWordMatchCount ) o << "\t" << matchCount;
        if( m_actions & fAction_reportWordMatchLongest ) o << "\t" << matchLongest;
    }
    return Heuristic( matchLongest, matchCount, wordCount, bmaccess.GetWordSize() );
}

void CReadTagger::PrintHeader( EOutputFile of )
{
    switch( of ) {
        case eOutput_tag: GetOutFile( of ) << "#read-id\t#tag\n"; return;
        case eOutput_report: break;
        default: return;
    }
    ofstream& o = GetOutFile( of );
    int col = 0;
    o << "#read-id:" << ++col;
    for( int c = 1; c <= m_components; ++c ) {
        if( m_actions & fAction_reportInputSeq ) o << "\t#input-seq-" << c << ":" << ++col;
        if( m_actions & fAction_reportInputLen ) o << "\t#input-len-" << c << ":" << ++col;
        if( m_actions & fAction_reportClippedSeq ) o << "\t#clipped-seq-" << c << ":" << ++col;
        if( m_actions & fAction_reportClippedLen ) o << "\t#clipped-len-" << c << ":" << ++col;
        for( int s = 0; s <= 1; ++s ) { // by strand
            char strand = "+-"[s];
            for( unsigned i = 0; i < m_bmaccess.size(); ++i ) { // by bitmask
                if( m_actions & fAction_reportWordGraph ) o << "\t#words-graph-" << c << strand << i << ":" << ++col;
                if( m_actions & fAction_reportWordTotal ) o << "\t#words-total-" << c << strand << i << ":" << ++col;
                if( m_actions & fAction_reportWordMatchCount ) o << "\t#words-matched-" << c << strand << i << ":" << ++col;
                if( m_actions & fAction_reportWordMatchLongest ) o << "\t#words-run-" << c << strand << i << ":" << ++col;
            }
        }
    }
    o << "\n";
}

CReadTagger::ETagValue CReadTagger::Heuristic( int matchLongest, int matchCount, int wordCount, int wordSize )
{
    if( wordCount < m_minWordsCutoff ) return eTag_uncertain;
    matchCount *= 100;
    matchLongest *= 100;
    if( wordCount >= m_longWordsSwitch ) {
        if( matchCount <  wordCount*20 && matchLongest <  wordCount*10 ) return eTag_foreign;
        if( matchCount >= wordCount*60 && matchLongest >= wordCount*20 ) return eTag_human;
    } else {
        if( matchCount <  wordCount*20 && matchLongest <  wordCount*10 ) return eTag_foreign;
        if( matchCount >= wordCount*80 && matchLongest >= wordCount*40 ) return eTag_human;
    }
    return eTag_uncertain;
}

bool CReadTagger::DecideEarly() const
{
    if( m_actions & fAction_reportALL ) return false;
    if( m_humanCnt > 0 ) return true;
    else return false;
}

ofstream& CReadTagger::GetOutFile( EOutputFile of )
{ 
    static const char * suffixes[] = { ".report", ".tag", ".long.fa", ".long2.fa", ".short.fa", ".short2.fa" };
    if( unsigned(of) >= eOutput_COUNT ) THROW( logic_error, "Oops... wrong file identifier of " << of );
    if( unsigned(of) >= sizeof( m_outputFile )/sizeof( m_outputFile[0] ) ) THROW( logic_error, "Oops... wrong m_outputFile[] size of " << of );
    if( unsigned(of) >= sizeof( suffixes )/sizeof( suffixes[0] ) ) THROW( logic_error, "Oops... wrong suffixes[] size of " << of );
    if( m_outputFile[of].get() == 0 ) {
        m_outputFile[of].reset( new ofstream( (m_basename + suffixes[of]).c_str() ) ); 
        PrintHeader( of );
    }
    return *m_outputFile[of]; 
}

CReadTagger::ETagValue CReadTagger::PurgeRead()
{
    if( m_id.length() == 0 ) return eTag_uncertain;
    if( m_actions & fAction_tag ) {
        ETagValue tag = GetTagValue();
        //if( tag == eTag_uncertain && (m_actions & fAction_post) ) {}
        //else {
            ofstream& o = GetOutFile( eOutput_tag );
            o << m_id << "\t" << "120"[tag] << "\n"; // << "FUH"[tag] << "\n";
        //}
    }
    if( (m_actions & fAction_post) && GetTagValue() == eTag_uncertain ) {
        if( m_reads.size() >= 1 ) PrintFasta( m_reads[0], 0 );
        if( m_reads.size() >= 2 ) PrintFasta( m_reads[1], 1 );
    }
    if( m_actions & fAction_reportALL ) {
        ofstream& o = GetOutFile( eOutput_report );
        o << "\n";
    }
    m_reads.clear();
    m_quals.clear();
    return GetTagValue();
}

void CReadTagger::PrintFasta( const string& read, int component )
{
    ofstream& o = GetOutFile( EOutputFile( ( int(read.length()) > m_shortSeqSwitch ? eOutput_long1 : eOutput_short1 ) + component ) );
    o << ">" << m_id << "\n" << read << "\n";
}

// END
