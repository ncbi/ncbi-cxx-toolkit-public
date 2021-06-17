#include <ncbi_pch.hpp>
#include "csammdformatter.hpp"
#include "cseqcoding.hpp"

USING_OLIGOFAR_SCOPES;

bool CSamMdFormatter::Format()
{
    if( m_target == 0 || m_target[0] == 0 || m_target[1] == 0 ) return false; // tgt == "\xf" is possible

    const char * s = m_query;
    const char * t = m_target;
    const TTrSequence& tr = m_cigar;
    TTrSequence::const_iterator x = tr.begin();
    if( x != tr.end() ) {
        switch( x->GetEvent() ) {
        case CTrBase::eEvent_SoftMask: s += x->GetCount();
        case CTrBase::eEvent_HardMask: ++x; 
        default: break;
        }
    }
    for( ; x != tr.end(); ++x ) {
        switch( x->GetEvent() ) {
        case CTrBase::eEvent_Replaced:
        case CTrBase::eEvent_Changed:
        case CTrBase::eEvent_Match: 
            FormatAligned( s, t, x->GetCount() ); 
            s += x->GetCount(); 
            t += x->GetCount();
        case CTrBase::eEvent_SoftMask:
        case CTrBase::eEvent_HardMask:
        case CTrBase::eEvent_Padding: continue;
        default: break;
        case CTrBase::eEvent_Splice:
        case CTrBase::eEvent_Overlap: m_data.clear(); return false; // MD can't be produced
        }
        // Here we have I or D
        switch( x->GetEvent() ) {
            case CTrBase::eEvent_Insertion:
                FormatInsertion( s, x->GetCount() );
                s += x->GetCount();
                break;
            case CTrBase::eEvent_Deletion: 
                FormatDeletion( t, x->GetCount() );
                t += x->GetCount();
                break;
            default: THROW( logic_error, "Unexpected event " << x->GetEvent() << " @ " << __FILE__ << ":" << __LINE__ << DISPLAY( tr.ToString() ) );
        }
    }
    FormatTerminal();
    return m_data.length() > 0;
}

void CSamMdFormatter::FormatAligned( const char * s, const char * t, int count )
{
    while( count-- ) {
        char query = *s++;
        char subj = CIupacnaBase( CNcbi8naBase( t++ ) );
        if( query == subj ) { ++m_keepCount; }
        else {
            FormatTerminal();
            switch( subj ) {
            case 'A': case 'C': case 'G': case 'T': m_data += subj; break;
            default: m_data += 'N';
            }
        }
    }
}

void CSamMdFormatter::FormatTerminal()
{
    if( m_keepCount ) { m_data += NStr::IntToString( m_keepCount ); m_keepCount = 0; }
}

void CSamMdFormatter::FormatInsertion( const char * s, int count )
{
    // Nothing. At all.
}

void CSamMdFormatter::FormatDeletion( const char * t, int count ) 
{
    FormatTerminal();
    m_data += "^";
    while( count-- ) {
        char ref = CIupacnaBase( CNcbi8naBase( t++ ) );
        switch( ref ) {
        case 'A': case 'C': case 'G': case 'T': m_data += ref; break;
        default: m_data += 'N';
        }
    }
}


