#ifndef OLIGOFAR_CSAMFORMATTER__HPP
#define OLIGOFAR_CSAMFORMATTER__HPP

#include "csam.hpp"
#include "cquery.hpp"
#include "cseqids.hpp"
#include "aoutputformatter.hpp"

BEGIN_OLIGOFAR_SCOPES

class IAligner;
class CGuideFile;
class CSamFormatter : public AOutputFormatter, public CSamBase
{
public:
    CSamFormatter( ostream& out, const CSeqIds& seqIds ) : 
        AOutputFormatter( out, seqIds ), m_headerPrinted( false ) {}
    void FormatQueryHits( const CQuery * query );
    void FormatHit( const CHit * hit );
    int  ReportHits( int mask, const CQuery * query, const CHit * hit, int flags, int maxToReport );
    void FormatEmptyHit( const CQuery * query, int component );
    bool ShouldFormatAllHits()  const { return false; }
    void FormatHeader( const string& version = "*" );
    void PrintCorrectedIupac( ostream& o, const char * seqdata, int seqlength, const TTrSequence& tr, const char * target, CSeqCoding::EStrand strand );
protected:
    void FormatHit( int rank, int rankSize, const CHit * hit, int pairmate, int flags );
    bool m_headerPrinted;
};

END_OLIGOFAR_SCOPES

#endif
