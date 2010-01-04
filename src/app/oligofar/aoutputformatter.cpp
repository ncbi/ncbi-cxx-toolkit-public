#include <ncbi_pch.hpp>
#include "coutputformatter.hpp"
#include "cguidefile.hpp"
#include "chit.hpp"

USING_OLIGOFAR_SCOPES;

string AOutputFormatter::GetSubjectId( int ord ) const 
{
    //return m_seqIds.GetSeqDef( ord ).GetBestIdString();
    string label;
    CSeq_id( m_seqIds.GetSeqDef( ord ).GetBestIdString() ).GetLabel( &label, CSeq_id::eContent );
    return label;
}
