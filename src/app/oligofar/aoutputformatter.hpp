#ifndef OLIGOFAR_AOUTPUTFORMATTER__HPP
#define OLIGOFAR_AOUTPUTFORMATTER__HPP

#include "cquery.hpp"
#include "cseqids.hpp"
#include "ioutputformatter.hpp"

BEGIN_OLIGOFAR_SCOPES

class IAligner;
class CGuideFile;
class AOutputFormatter : public IOutputFormatter
{
public:
    AOutputFormatter( ostream& out, const CSeqIds& seqIds ) : 
        m_out( out ), m_seqIds( seqIds ), m_aligner( 0 ) {}
    string GetSubjectId( int ord ) const;
    bool NeedSeqids() const { return m_seqIds.IsEmpty(); }
    void SetGuideFile( const CGuideFile& guideFile ) { m_guideFile = &guideFile; }
    void SetAligner( IAligner * aligner ) { m_aligner = aligner; }
protected:
    ostream& m_out;
    const CSeqIds& m_seqIds;
    const CGuideFile * m_guideFile;
    IAligner * m_aligner;
};

END_OLIGOFAR_SCOPES

#endif
