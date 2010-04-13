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
    enum EFlags {
        fReportUnmapped = 0x002,
        fReportPairesOnly = 0x200,
        fReportSeqidsAsis = 0x400,
        fDefault = 0x0f
    };
    AOutputFormatter( ostream& out, const CSeqIds& seqIds ) : 
        m_out( out ), m_seqIds( seqIds ), m_guideFile(0), m_topCount(numeric_limits<int>::max()), m_flags( fDefault ) {}
    string GetSubjectId( int ord ) const;
    bool NeedSeqids() const { return m_seqIds.IsEmpty(); }
    void SetGuideFile( const IGuideFile& guideFile ) { m_guideFile = &guideFile; }
    void SetTopCount( int cnt ) { m_topCount = cnt; }
    void AssignFlags( int flags ) { m_flags = flags; }
protected:
    ostream& m_out;
    const CSeqIds& m_seqIds;
    const IGuideFile * m_guideFile;
    int m_topCount;
    int m_flags;
};

END_OLIGOFAR_SCOPES

#endif
