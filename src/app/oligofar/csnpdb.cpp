#include <ncbi_pch.hpp>
#include "csnpdb.hpp"

USING_OLIGOFAR_SCOPES;

CSnpDbBase::CSnpDbBase( ESeqIdType type, const string& idprefix, const string& idsuffix ) :
    m_seqIdType( type ),
    m_prefix( idprefix ),
    m_suffix( idsuffix )
{
    switch( type ) {
    case eSeqId_integer: BindKey( "intId", &m_intId ); break;
    case eSeqId_string:  BindKey( "strId", &m_stringId ); break;
    }
    BindKey( "pos", &m_pos );
    BindData( "a", m_prob + 0 );
    BindData( "c", m_prob + 1 );
    BindData( "g", m_prob + 2 );
    BindData( "t", m_prob + 3 );
}

string CSnpDbBase::GetSeqId() const
{ 
    switch( m_seqIdType ) {
    case eSeqId_string: return m_prefix + string(m_stringId) + m_suffix;
    case eSeqId_integer: return m_prefix + NStr::IntToString( m_intId ) + m_suffix;
    }
	throw logic_error( "Undefined m_seqIdType in CSnpDbBase::GetSeqId()" );
}

void CSnpDbBase::ParseSeqId( const string& id )
{
    const char * idb = id.c_str();
    const char * ide = id.c_str() + id.length();
    if( m_prefix.length() ) {
        idb = strstr( idb, m_prefix.c_str() );
        if( idb == 0 )
            throw runtime_error( "Failed to parse "+id+": no prefix "+m_prefix+" found" );
        idb += m_prefix.length();
    }
    if( m_suffix.length() ) {
        ide = strstr( idb, m_suffix.c_str() );
        if( ide == 0 )
            ide = id.c_str() + id.length();
    }
    switch( m_seqIdType ) {
    case eSeqId_string: m_stringId = string( idb, ide ); break;
    case eSeqId_integer: m_intId = NStr::StringToInt( string( idb, ide ) ); break;
    }
}    

bool CSnpDbCreator::Insert( const string& fullId, int pos, unsigned base, EStrand strand )
{
    base &= 0x0f;
    if( strand == eStrand_reverse ) base = CNcbi8naBase( base ).Complement(); // complement
    return Insert( fullId, pos, 
                   base & 0x01 ? 1.0 : 0.0,
                   base & 0x02 ? 1.0 : 0.0,
                   base & 0x04 ? 1.0 : 0.0,
                   base & 0x08 ? 1.0 : 0.0 ); // strand is positive
}

bool CSnpDbCreator::Insert( const string& id, int pos, double a, double c, double g, double t, EStrand strand )
{
    if( strand == eStrand_reverse ) {
        swap( a, t );
        swap( c, g );
    }
    double total = a + c + g + t;
    if( total == 0 ) 
        throw logic_error( "CSnpDbCreator::Insert(): Have zero probabilities for base in "+id+" at "+NStr::IntToString( pos ) );
    
    ParseSeqId( id );

    m_pos = pos;
    m_prob[0] = float( a/total );
    m_prob[1] = float( c/total );
    m_prob[2] = float( g/total );
    m_prob[3] = float( t/total );
    return CBDB_File::Insert() == eBDB_Ok;
}

int CSnpDb::First( const string& seqId )
{
    ParseSeqId( seqId );
    
    m_cursor.reset( new CBDB_FileCursor( *this ) );
    m_cursor->SetCondition( CBDB_FileCursor::eGE, CBDB_FileCursor::eLE );

    switch( m_seqIdType ) {
    case eSeqId_string: m_cursor->From << m_stringId << 0; m_cursor->To << m_stringId; break;
    case eSeqId_integer: m_cursor->From << m_intId << 0; m_cursor->To << m_intId; break;
    }

    if( m_cursor->FetchFirst() != eBDB_Ok ) { m_cursor.reset(0); return -1; }
    
    return m_pos;
}

int CSnpDb::First( int seqId )
{
    ASSERT( m_seqIdType == eSeqId_integer );
    
    m_intId = seqId;
    
    m_cursor.reset( new CBDB_FileCursor( *this ) );
    m_cursor->SetCondition( CBDB_FileCursor::eGE, CBDB_FileCursor::eLE );

    m_cursor->From << m_intId << 0; 
    m_cursor->To << m_intId; 

    if( m_cursor->FetchFirst() != eBDB_Ok ) { m_cursor.reset(0); return -1; }
    
    return m_pos;
}

int CSnpDb::Next() 
{
    if( m_cursor.get() == 0 ) return -1;
    if( m_cursor->Fetch( CBDB_FileCursor::eForward ) != eBDB_Ok ) { m_cursor.reset(0); return -1; }
    return m_pos;
}

    
    
