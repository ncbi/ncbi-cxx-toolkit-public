#ifndef OLIGOFAR_COLIGOFARAPP__HPP
#define OLIGOFAR_COLIGOFARAPP__HPP

#include "capp.hpp"
#include "cqueryhash.hpp"
#include <string>
#include <set>

BEGIN_OLIGOFAR_SCOPES

class IAligner;
class COligoFarApp : public CApp
{
public:
    COligoFarApp( int argc, char ** argv );
protected:
    virtual void Help();
    virtual void Version();
    virtual int  Execute();
    virtual int  TestSuite();
    
    int RunTestSuite();
    int SetLimits();
    int ProcessData();

    int GetOutputFlags() const;

	IAligner * CreateAligner() const;

    virtual const option * GetLongOptions() const;
    virtual const char * GetOptString() const;
    virtual int ParseArg( int, const char *, int );
    enum {
        kKiloByte = 1024,
        kMegaByte = 1024*kKiloByte,
        kGigaByte = 1024*kMegaByte
    };
    enum EAlignmentAlgo {
        eAlignment_HSP   = 'h',
        eAlignment_SW    = 's',
        eAlignment_fast  = 'f',
        eAlignment_quick = 'q'
    };
protected:
    unsigned m_windowLength;
    unsigned m_maxHashMism;
    unsigned m_maxHashAlt;
    unsigned m_maxFastaAlt;
    unsigned m_strands;
    unsigned m_readsPerRun;
    unsigned m_solexaSensitivity;
    unsigned m_xdropoff;
    unsigned m_topCnt;
    double   m_topPct;
    double   m_minPctid;
    double   m_maxSimplicity;
    double   m_identityScore;
    double   m_mismatchScore;
    double   m_gapOpeningScore;
    double   m_gapExtentionScore;
    int      m_minPair;
    int      m_maxPair;
    int      m_pairMargin;
	int      m_qualityChannels;
//    bool     m_reversePair;
    Uint8    m_memoryLimit;
    bool     m_performTests;
	bool     m_maxMismOnly;
	EAlignmentAlgo m_alignmentAlgo;
	CQueryHash::EHashType m_hashType;
    string m_readFile;
    string m_gilistFile;
    string m_fastaFile;
    string m_snpdbFile;
    string m_vardbFile;
	string m_guideFile;
    string m_outputFile;
    string m_outputFlags;
	string m_read1qualityFile;
	string m_read2qualityFile;
};

END_OLIGOFAR_SCOPES

#endif
