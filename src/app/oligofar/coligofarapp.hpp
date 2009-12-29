#ifndef OLIGOFAR_COLIGOFARAPP__HPP
#define OLIGOFAR_COLIGOFARAPP__HPP

#include "capp.hpp"
#include "util.hpp"
#include "array_set.hpp"
#include "cpassparam.hpp"
#include "cshortreader.hpp"
#include "cintron.hpp"
#include <corelib/ncbireg.hpp>
#include <string>
#include <set>

BEGIN_OLIGOFAR_SCOPES

class IAligner;
class CScoreTbl;
class CQueryHash;
class COligoFarApp : public CApp
{
public:
    COligoFarApp( int argc, char ** argv );
    static int RevNo();
    enum {
        kKiloByte = 1024,
        kMegaByte = 1024*kKiloByte,
        kGigaByte = 1024*kMegaByte
    };
    enum ELongOpt { 
        kLongOptBase = 0x100,
        kLongOpt_pass0 = kLongOptBase + 0x00,
        kLongOpt_pass1 = kLongOptBase + 0x01,
        kLongOpt_min_block_length = kLongOptBase + 0x02,
        kLongOpt_NaHSO3 = kLongOptBase + 0x03,
        kLongOpt_maxInsertions = kLongOptBase + 0x04,
        kLongOpt_maxDeletions = kLongOptBase + 0x05,
        kLongOpt_maxInsertion = kLongOptBase + 0x07,
        kLongOpt_maxDeletion = kLongOptBase + 0x08,
        kLongOpt_addSplice = kLongOptBase + 0x09,
        kLongOpt_hashBitMask = kLongOptBase + 0x0a,
        kLongOpt_batchRange = kLongOptBase + 0x0b,
        kLongOpt_printStatistics = kLongOptBase + 0x0c,
        kLongOptEnd
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
    
    int RunTestSuite();
    int SetLimits();
    int ProcessData();
    void ParseConfig( const string& cfg );
    void ParseConfig( IRegistry * );

    int GetOutputFlags();

    virtual const option * GetLongOptions() const;
    virtual const char * GetOptString() const;
    virtual int ParseArg( int, const char *, int );

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
