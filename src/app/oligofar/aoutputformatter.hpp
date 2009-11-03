#ifndef OLIGOFAR_AOUTPUTFORMATTER__HPP
#define OLIGOFAR_AOUTPUTFORMATTER__HPP

#include "cquery.hpp"
#include "cseqids.hpp"
#include "ioutputformatter.hpp"

BEGIN_OLIGOFAR_SCOPES

class IGuideFile;
class AOutputFormatter : public IOutputFormatter
{
public:
    AOutputFormatter( ostream& out, const CSeqIds& seqIds ) : 
        m_out( out ), m_seqIds( seqIds ) {}
    string GetSubjectId( int ord ) const;
    bool NeedSeqids() const { return m_seqIds.IsEmpty(); }
    void SetGuideFile( const IGuideFile& guideFile ) { m_guideFile = &guideFile; }
protected:
    ostream& m_out;
    const CSeqIds& m_seqIds;
    const IGuideFile * m_guideFile;
};

END_OLIGOFAR_SCOPES

#endif
