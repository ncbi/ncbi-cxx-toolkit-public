#ifndef OLIGOFAR_CGUIDEFILE__HPP
#define OLIGOFAR_CGUIDEFILE__HPP

#include "defs.hpp"
#include "cfilter.hpp"
#include "iguidefile.hpp"
#include "csamrecords.hpp"

BEGIN_OLIGOFAR_SCOPES

class CSeqIds;
class CGuideSamFile : public IGuideFile, public CSamBase
{
public:
    CGuideSamFile( const string& fileName, CFilter& filter, CSeqIds& seqIds, CScoreParam& scoreParam );
    
    bool NextHit( Uint8 ordinal, CQuery * query );

    void AddQuery( CQuery * query, CSamRecord * hit ); // should delete hit
    void AddSingle( CQuery * query, CSamRecord * hit ); // should delete hit
    void AddPair( CQuery * query, CSamRecord * ahit, CSamRecord * bhit ); // should delete hit
protected:
    void x_ParseHeader();
    
    ifstream m_in;
    string m_buff;
    int m_linenumber;
    CFilter * m_filter;
    CSeqIds * m_seqIds;
    CScoreParam * m_scoreParam;
    string m_samVersion;
    string m_sortOrder;
    string m_groupOrder;
    TTrVector m_cigar[2];
    int m_len[2];
};

inline CGuideSamFile::CGuideSamFile( const string& fileName, CFilter& filter, CSeqIds& seqIds, CScoreParam& scoreParam )
    : m_linenumber( 0 ), m_filter( &filter ), m_seqIds( &seqIds ), m_scoreParam( &scoreParam )
{
    ASSERT( m_filter );
    ASSERT( m_seqIds );
    if( fileName.length() ) {
        m_in.open( fileName.c_str() );
        if( m_in.fail() ) THROW( runtime_error, "Failed to open file " << fileName << ": " << strerror(errno) );
        x_ParseHeader();
    }
}

END_OLIGOFAR_SCOPES

#endif

