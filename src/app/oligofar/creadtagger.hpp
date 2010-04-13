#ifndef OLIGOFAR_CREADTAGGER__HPP
#define OLIGOFAR_CREADTAGGER__HPP

#include "cbitmaskaccess.hpp"
#include "cshortreader.hpp"

BEGIN_OLIGOFAR_SCOPES

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

END_OLIGOFAR_SCOPES

#endif
