#include <ncbi_pch.hpp>
#include "coutputformatter.hpp"
#include "cguidefile.hpp"
#include "chit.hpp"

USING_OLIGOFAR_SCOPES;

string AOutputFormatter::GetSubjectId( int ord ) const 
{
    return m_seqIds.GetSeqDef( ord ).GetBestIdString();
}
