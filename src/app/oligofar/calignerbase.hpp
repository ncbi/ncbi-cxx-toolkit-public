#ifndef OLIGOFAR_CALIGNERBASE__HPP
#define OLIGOFAR_CALIGNERBASE__HPP

#include "cscoretbl.hpp"

BEGIN_OLIGOFAR_SCOPES

template <class CQuery, class CSubject>
class TAlignerBase;
class CAlignerBase
{
public:
    typedef vector<unsigned char> TTranscript;
    template<class T> static int sign( const T& t ) { return t > 0 ? 1 : t < 0 ? -1 : 0; } 

    enum EFlags {
        fComputeScore = 0x01,
        fComputeTranscript = 0x02,
        fComputePicture = 0x04,
        fPictureQueryStrand = 0x08,
        fPictureSubjectStrand = 0x00
    };

    static CScoreTbl& GetDefaultScoreTbl() { return s_defaultScoreTbl; } 

    CAlignerBase( CScoreTbl& scoretbl = GetDefaultScoreTbl() );
    CAlignerBase& SetScoreTbl( CScoreTbl& scoretbl ) { m_scoreTbl = &scoretbl; return *this; }

    int GetQueryAlignedLength() const { return m_qaligned; }
    int GetSubjectAlignedLength() const { return m_saligned; }
    int GetAlignmentLength() const { return m_identities + m_mismatches + m_insertions + m_deletions; }

    int GetIdentityCount() const { return m_identities; }
    int GetMismatchCount() const { return m_mismatches; }
    int GetIndelCount() const { return m_insertions + m_deletions; }

    bool PictureQueryStrand( int flags ) const { return (flags & fPictureQueryStrand) != 0; }
    bool PictureSubjectStrand( int flags ) const { return (flags & fPictureQueryStrand) == 0; }

    double GetScore() const { return 100.0 * m_score / m_bestScore; }
    double GetRawScore() const { return m_score; }
    double GetBestQueryScore() const { return m_bestScore; }

    const TTranscript& GetTranscript() const { return m_transcript; }
    const CScoreTbl& GetScoreTbl() const { return *m_scoreTbl; }

    const string& GetQueryString() const { return m_queryString; }
    const string& GetSubjectString() const { return m_subjectString; }
    const string& GetAlignmentString() const { return m_alignmentString; }

    int  GetBasicScoreTables() const { return m_scoreTbl->GetBasicScoreTables(); }
    void SelectBasicScoreTables( unsigned tbl ) { m_scoreTbl->SelectBasicScoreTables( tbl ); }
    void ParseConfig( const string& file );

    CAlignerBase& ResetScores();

    template <class CQuery, class CSubject>
    friend class TAlignerBase;

    static bool SetPrintDebug( bool on = true ) { return gsx_printDebug = on; }
    static bool GetPrintDebug() { return gsx_printDebug; }

protected:
    static CScoreTbl s_defaultScoreTbl;
    static bool gsx_printDebug;

    CScoreTbl * m_scoreTbl;
    const char * m_qbegin;
    const char * m_sbegin;
    const char * m_qend;
    const char * m_send;
    
    int m_qaligned;
    int m_saligned;

    int m_identities;
    int m_mismatches;
    int m_insertions;
    int m_deletions;

    double m_score;
    double m_bestScore;

    TTranscript m_transcript;

    string m_queryString;
    string m_subjectString;
    string m_alignmentString;
};

END_OLIGOFAR_SCOPES

#endif
