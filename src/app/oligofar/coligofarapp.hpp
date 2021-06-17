#ifndef OLIGOFAR_COLIGOFARAPP__HPP
#define OLIGOFAR_COLIGOFARAPP__HPP

#include "capp.hpp"
#include "util.hpp"
#include "string-util.hpp"
#include "array_set.hpp"
#include "cpassparam.hpp"
#include "cshortreader.hpp"
#include "coligofarcfg.hpp"
#include "cintron.hpp"
#include <util/value_convert.hpp>
#include <corelib/ncbireg.hpp>
#include <string>
#include <set>

BEGIN_OLIGOFAR_SCOPES

class IAligner;
class CScoreTbl;
class CQueryHash;
class COligoFarApp : public CApp, public COligofarCfg
{
public:
    COligoFarApp( int argc, char ** argv );
    static int RevNo();
    enum {
        kKiloByte = 1024,
        kMegaByte = 1024*kKiloByte,
        kGigaByte = 1024*kMegaByte
    };
    typedef CHashParam::TSkipPositions TSkipPositions;

    string ReportSplices( int pass ) const;
    void AddSplice( const char * arg );

    bool ValidateSplices( CQueryHash& );

protected:
    virtual void Help( const char * );
    virtual void Version( const char * );
    virtual int  Execute();
    virtual int  TestSuite();
    
    void InitConfig();
    void SetPass0( const char * );
    void SetPass1( const char * );
    void ParseConfig( const char * cfg ) { ParseConfig( string( cfg ) ); }
    void SetWindowSize( const char * );
    void PrintWindowSize( ostream& );


    int RunTestSuite();
    int SetLimits();
    int ProcessData();
    void ParseConfig( const string& cfg );
    void ParseConfig( IRegistry * );
    bool HasConfigEntry( IRegistry *, const string& section, const string& name ) const;
    void WriteConfig( const char * );

    int GetOutputFlags();

    virtual int ParseArg( int, const char *, int );
    virtual void PrintArgValue( int opt, ostream&, int );
    virtual int  GetPassCount() const { return m_passParam.size(); }
    virtual const option * COligoFarApp::GetLongOptions() const;
    virtual const char * GetOptString() const;

    void SetupGeometries( map<string,int>& );

    char HashTypeChar( int  = 0 ) const;
    void SetHashType( int, char );

    static void ParseRange( int& a, int& b, const string& str, const string& delim = "-," ) {
        pair<int,int> val = NUtil::ParseRange( str, delim );
        a = val.first;
        b = val.second;
    }
    static void ParseRange( unsigned& a, unsigned& b, const string& str, const string& delim = "-," ) {
        pair<int,int> val = NUtil::ParseRange( str, delim );
        a = val.first;
        b = val.second;
    }

protected:
    vector<CPassParam> m_passParam;
    unsigned m_hashPass;
    unsigned m_strands;
    unsigned m_readsPerRun;
    unsigned m_minRun;
    unsigned m_maxRun;
    unsigned m_topCnt;
    double   m_topPct;
    double   m_minPctid;
    double   m_identityScore;
    double   m_mismatchScore;
    double   m_gapOpeningScore;
    double   m_gapExtentionScore;
    double   m_extentionPenaltyDropoff;
    int      m_qualityChannels;
    int      m_qualityBase;
    int      m_minBlockLength;
    int      m_guideFilemaxMismatch;
    Uint8    m_memoryLimit;
    bool     m_performTests;
    bool     m_colorSpace;
    bool     m_sodiumBisulfiteCuration;
    bool     m_outputSam;
    bool     m_printStatistics;
    bool     m_fastaParseIDs;
    string m_readFile;
    string m_gilistFile;
    string m_fastaFile;
    string m_snpdbFile;
    string m_guideFile;
    string m_featFile;
    string m_outputFile;
    string m_outputFlags;
    string m_read1qualityFile;
    string m_read2qualityFile;
    string m_geometry;
    string m_hashBitMask;
    list<string> m_seqIds;
};

END_OLIGOFAR_SCOPES

#endif
