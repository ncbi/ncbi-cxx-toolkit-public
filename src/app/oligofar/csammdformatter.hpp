#ifndef OLIGOFAR_CSAMMDFORMATTER__HPP
#define OLIGOFAR_CSAMMDFORMATTER__HPP

#include "ctranscript.hpp"

BEGIN_OLIGOFAR_SCOPES

class CSamMdFormatter
{
public:
    // queryS is query on the subject strand
    CSamMdFormatter( const char * target_ncbi8na, const char * queryS_iupacna, const TTrSequence& cigar ) :
        m_target( target_ncbi8na ), m_query( queryS_iupacna ), m_cigar( cigar ), m_keepCount(0) {}
    const string& GetString() const { return m_data; }
    bool Format();
    bool Ok() const { return m_data.length(); }
protected:
    void FormatAligned( const char * s, const char * t, int cnt );
    void FormatInsertion( const char * s, int cnt );
    void FormatDeletion( const char * t, int cnt );
    void FormatTerminal();
protected:
    string m_data;
    const char * m_target;
    const char * m_query;
    TTrSequence m_cigar;
    int m_keepCount;
};

END_OLIGOFAR_SCOPES

#endif
