#ifndef OLIGOFAR_COUTPUTFORMATTER__HPP
#define OLIGOFAR_COUTPUTFORMATTER__HPP

#include "cquery.hpp"
#include "cseqids.hpp"
#include "aoutputformatter.hpp"

BEGIN_OLIGOFAR_SCOPES

class IAligner;
class CGuideFile;
class COutputFormatter : public AOutputFormatter
{
public:
    enum EFlags {
        fReportEmptyLines = 0x001,
        fReportUnmapped = 0x002,
        fReportMore = 0x004,
        fReportMany = 0x008,
        fReportTerminator = 0x010,
        fReportDifferences = 0x020,
//        fReportAlignment = 0x040,
        fReportAllHits = 0x080,
        fReportRawScore = 0x100,
        fDefault = 0x0f
    };
    // comment
    void AssignFlags( int flags ) { m_flags = flags; }
    COutputFormatter( ostream& out, const CSeqIds& seqIds ) : 
        AOutputFormatter( out, seqIds ), m_flags( fDefault ), m_topCount(numeric_limits<int>::max()), m_topPct(0) {}
    const char * QuerySeparator() const { return m_flags&fReportEmptyLines?"\n":""; }
    void FormatQueryHits( const CQuery * query );
    void FormatHit( const CHit * hit );
    bool ShouldFormatAllHits()  const { return ( m_flags & fReportAllHits ) != 0; }
    void SetTopCount( int cnt ) { m_topCount = cnt; }
    void SetTopPct( double pct ) { m_topPct = pct; }

protected:
    int  FormatQueryHits( const CQuery * query, int mask, int topCount );
    void FormatCore( const CHit * hit ) const;
    void FormatDifferences( int rank, const CHit* hit, int matepair, int flags ) const;
    void FormatDifference( int rank, const CHit* hit, int matepair, const char * qry, const char * sbj, const char * aln, int pos, int count, int flags ) const;
    template<typename QCoding>
    void CoTranslate( const char * q, const char * s, const CHit::TTranscript& t, int rank, const CHit * hit, int matepair, int flags ) const ;
protected:
    int m_flags;
    int m_topCount;
    double m_topPct;
};

END_OLIGOFAR_SCOPES

#endif
