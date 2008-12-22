#ifndef OLIGOFAR_CSAMFORMATTER__HPP
#define OLIGOFAR_CSAMFORMATTER__HPP

#include "cquery.hpp"
#include "cseqids.hpp"
#include "aoutputformatter.hpp"

BEGIN_OLIGOFAR_SCOPES

class IAligner;
class CGuideFile;
class CSamFormatter : public AOutputFormatter
{
public:
    CSamFormatter( ostream& out, const CSeqIds& seqIds ) : 
        AOutputFormatter( out, seqIds ) {}
    void FormatQueryHits( const CQuery * query );
    void FormatHit( const CHit * hit );
    bool ShouldFormatAllHits()  const { return false; }
    void FormatHeader( const string& version = "*" );
protected:
    void FormatHit( int rank, int rankSize, const CHit * hit, int pairmate );
};

END_OLIGOFAR_SCOPES

#endif
