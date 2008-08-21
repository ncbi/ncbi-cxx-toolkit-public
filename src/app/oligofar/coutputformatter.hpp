#ifndef OLIGOFAR_COUTPUTFORMATTER__HPP
#define OLIGOFAR_COUTPUTFORMATTER__HPP

#include "cquery.hpp"
#include "cseqids.hpp"

BEGIN_OLIGOFAR_SCOPES

class IAligner;
class CGuideFile;
class COutputFormatter
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
    void AssignFlags( int flags ) { m_flags = flags; }
    COutputFormatter( ostream& out, const CSeqIds& seqIds ) : 
        m_flags( fDefault ), m_out( out ), m_seqIds( seqIds ), m_aligner( 0 ) {}
    void operator () ( const CQuery * query );
    const char * QuerySeparator() const { return m_flags&fReportEmptyLines?"\n":""; }
    bool NeedSeqids() const { return m_seqIds.IsEmpty(); }
	void SetGuideFile( const CGuideFile& guideFile ) { m_guideFile = &guideFile; }
	string GetSubjectId( int ord ) const;
	void FormatHit( const CHit * hit ) const;
	bool ShouldFormatAllHits()  const { return ( m_flags & fReportAllHits ) != 0; }
    void SetAligner( IAligner * aligner ) { m_aligner = aligner; }
protected:
	void FormatCore( const CHit * hit ) const;
	void FormatDifferences( int rank, const CHit* hit, int matepair, int flags ) const;
	void FormatDifference( int rank, const CHit* hit, int matepair, const string& qa, const string& sa, int from, int to ) const;
protected:
    int m_flags;
    ostream& m_out;
    const CSeqIds& m_seqIds;
	const CGuideFile * m_guideFile;
    IAligner * m_aligner;
};



END_OLIGOFAR_SCOPES

#endif
