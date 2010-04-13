#include <ncbi_pch.hpp>
#include "coutputformatter.hpp"
#include "cguidefile.hpp"
#include "chit.hpp"

USING_OLIGOFAR_SCOPES;

string AOutputFormatter::GetSubjectId( int ord ) const 
{
    // removes 'lcl|' part
    string label;
    if( m_flags & fReportSeqidsAsis ) {
        label = m_seqIds.GetSeqDef( ord ).GetBestIdString();
        if( label.substr(0,4) == "lcl|" ) label = label.substr(4);
    } else {
        CSeq_id( m_seqIds.GetSeqDef( ord ).GetBestIdString() ).GetLabel( &label, CSeq_id::eContent );
    }
    return label;
    //return m_seqIds.GetSeqDef( ord ).GetBestIdString();
}
