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
        fReportAlignment = 0x040,
        fReportAllHits = 0x080,
        fDefault = 0x0f
    };
    // comment
    void AssignFlags( int flags ) { m_flags = flags; }
    COutputFormatter( ostream& out, const CSeqIds& seqIds ) : 
        AOutputFormatter( out, seqIds ), m_flags( fDefault ) {}
    const char * QuerySeparator() const { return m_flags&fReportEmptyLines?"\n":""; }
    void FormatQueryHits( const CQuery * query );
    void FormatHit( const CHit * hit );
    bool ShouldFormatAllHits()  const { return ( m_flags & fReportAllHits ) != 0; }
protected:
    void FormatCore( const CHit * hit ) const;
    void FormatDifferences( int rank, const CHit* hit, int matepair, int flags ) const;
    void FormatDifference( int rank, const CHit* hit, int matepair, const string& qa, const string& sa, int from, int to ) const;
protected:
    int m_flags;
};

END_OLIGOFAR_SCOPES

#endif
