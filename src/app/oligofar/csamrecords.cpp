#include <ncbi_pch.hpp>
#include "csamrecords.hpp"
#include "string-util.hpp"

USING_OLIGOFAR_SCOPES;

CSamRecord::~CSamRecord() 
{
    delete m_tags;
    delete [] m_qname;
}

CSamRecord::CSamRecord() :
    m_qname(new char[1]),
    m_rname(m_qname),
    m_qiupac(m_qname),
    m_qqual(m_qname),
    m_flags(0),
    m_mapq(0),
    m_pos(0),
    m_mpos(0),
    m_isize(0),
    m_tags(0)
{ *m_qname = 0; }

CSamRecord::CSamRecord( const string& line ) :
    m_qname(new char[1]),
    m_rname(m_qname),
    m_qiupac(m_qname),
    m_qqual(m_qname),
    m_flags(0),
    m_mapq(0),
    m_pos(0),
    m_mpos(0),
    m_isize(0),
    m_tags(0)
{
    *m_qname = 0;
    Init( line );
}

CSamRecord::CSamRecord( const CSamRecord& other ) :
    m_qname(new char[1]),
    m_rname(m_qname),
    m_qiupac(m_qname),
    m_qqual(m_qname),
    m_flags(0),
    m_mapq(0),
    m_pos(0),
    m_mpos(0),
    m_isize(0),
    m_tags(0)
{
    *m_qname = 0;
    *this = other;
}
void CSamRecord::x_InitStrings( const char * qname, const char * rname, const char * iupac, const char * qual ) 
{
    x_InitStrings( qname, strlen( qname ), rname, strlen( rname ), iupac, strlen( iupac ), qual, strlen( qual ) );
}

void CSamRecord::x_InitStrings( const string& qname, const string& rname, const string& iupac, const string& qual )
{
    x_InitStrings( qname.c_str(), qname.length(), rname.c_str(), rname.length(), iupac.c_str(), iupac.length(), qual.c_str(), qual.length() );
}

void CSamRecord::x_InitStrings( const char * qname, int qnlen, const char * rname, int rnlen, const char * iupac, int iulen, const char * qual, int qlen )
{
    if( m_qname ) delete [] m_qname;

    if( rnlen == 1 && *rname == '*' ) rnlen = 0;
    if( iulen == 1 && *iupac == '*' ) iulen = 0;
    if( qlen  == 1 && *qual  == '*' ) qlen  = 0;

    int sz = qnlen + 1;
    sz += rnlen + 1;
    sz += iulen + 1;
    sz += qlen + 1;

    m_qname = new char[sz];
    
    strncpy( m_qname, qname, qnlen ); 
    m_qname[qnlen] = 0;
    m_rname = m_qname + qnlen + 1;
    strncpy( m_rname, rname, rnlen );
    m_rname[rnlen] = 0;
    m_qiupac = m_rname + rnlen + 1;
    strncpy( m_qiupac, iupac, iulen );
    m_qiupac[iulen] = 0;
    m_qqual = m_qiupac + iulen + 1;
    strncpy( m_qqual, qual, qlen );
    m_qqual[qlen] = 0;
}

CSamRecord& CSamRecord::operator = ( const CSamRecord& other )
{
    SetNull();
    if( other.IsNull() ) return *this;
    m_tags = other.HasTags() ? new TTags( other.GetTags() ) : 0;
    x_InitStrings( other.m_qname, other.m_rname, other.m_qiupac, other.m_qqual );
    m_cigar =  other.GetImprovedCigar();
    m_flags = other.GetFlags();
    m_mapq  = other.GetMapQual();
    m_pos   = other.GetPos();
    m_mpos  = other.GetMatePos();
    m_isize = other.GetInsertSize();
    return *this;
}

void CSamRecord::SetNull() 
{
    delete m_tags;
    delete [] m_qqual;
    m_cigar.clear();
    m_qname = new char[1];
    m_rname = m_qname;
    m_qiupac = m_qname;
    m_qqual = m_qname;
    *m_qname = 0;
    m_flags = 0;
    m_mapq = 0;
    m_pos = 0;
    m_mpos = 0;
    m_isize = 0;
    m_tags = 0;
}

void CSamRecord::Init( const string& line )
{
    SetNull();
    vector<string> fields;
    Split( line, "\t", back_inserter( fields ) );

    if( fields.size() < 11 )
        THROW( runtime_error, "SAM is supposed to have at least 11 columns, got " << fields.size() << " in [" << line << "] in line" );

    m_flags = NStr::StringToInt( fields[eCol_flags] );
    m_pos = NStr::StringToInt( fields[eCol_pos] ); 
    m_mapq = NStr::StringToInt( fields[eCol_mapq] );
    m_mpos = NStr::StringToInt( fields[eCol_mpos] );
    m_isize = NStr::StringToInt( fields[eCol_isize] );

    const string& qname = fields[eCol_qname];
    const string& rname = fields[eCol_rname];
    const string& cigar = fields[eCol_cigar];
    const string& mrnam = fields[eCol_mrnm];
    const string& qseq  = fields[eCol_seq];
    const string& qual  = fields[eCol_qual];

    if( mrnam == "*" ) m_mpos = m_isize = 0;
    else if( mrnam != "=" && rname != "*" && mrnam != rname )
        THROW( runtime_error, "Only paired hits on the same reference are supported" );

    x_InitStrings( qname, rname, qseq, qual );

    string mdtag = "";
    if( fields.size() >= eCol_tags ) m_tags = new TTags();
    for( size_t i = eCol_tags; i < fields.size(); ++i ) {
        if( fields[i].substr( 0, 5 ) == "MD:Z:" ) mdtag = fields[i].substr( 5 );
        const string& tag = fields[i];
        size_t x = tag.find( ':' );
        if( x == string::npos ) THROW( runtime_error, "Tag does not have ':' in " << line );
        if( tag.length() < (x + 2) || tag[x+2] != ':' ) THROW( runtime_error, "Tag does not have second ':' in " << line );
        switch( tag[x+1] ) {
            case 'A': case 'i': case 'f': case 'Z': case 'H': break;
            default: THROW( runtime_error, "Unknown tag value type '" << tag[x+1] << "' in " << line );
        }
        if( m_tags->find( tag.substr( 0, x ) ) != m_tags->end() )
            THROW( runtime_error, "Repeated tags are not implemented in line " << line );
        (*m_tags)[tag.substr( 0, x )] = tag.substr( x );
    }
    if( cigar != "*" ) {
        TCigar c( cigar );
        CSamMdParser samMdParser( c, mdtag );
        m_cigar = samMdParser.GetFullCigar();
    }
    if( ! ( m_flags & fPairedHit ) ) {
        m_mpos = m_isize = 0;
    } else {
        if( ! ( m_flags & fPairedQuery ) )
            THROW( runtime_error, "Invalid flags (paired hit for single query) in line " << line );
        if( m_flags & (fSeqUnmapped|fMateUnmapped) ) {
            THROW( runtime_error, "Invalid flags (paired hit has unmapped sequences) in line " << line );
        }
    }
    if( m_flags & fSeqUnmapped ) {
        if( *m_rname || m_pos ) 
            THROW( runtime_error, "Invalid flags: sequence is unmapped, but has mapping coords" );
    }
}

void CSamRecord::InitUnmapped( const string& qname, CSamRecord::TFlags flags, const string& qseq, const string& qual )
{
    SetNull();
    m_flags = flags;
    m_mapq = 0;
    m_pos = 0;
    m_mpos = 0;
    m_isize = 0;

    x_InitStrings( qname, string(""), qseq, qual );
}

void CSamRecord::InitSingleHit( const string& qname, TFlags flags, const string& rname, TPos pos, TMapQ mapq, const TCigar& cigar, const string& qseq, const string& qual )
{
    SetNull();
    m_flags = flags;
    m_mapq = mapq;
    m_mpos = 0;
    m_isize = 0;
    if( rname == "*" ) THROW( runtime_error, "InitSingleHit should not use RNAME = *" );
    x_InitStrings( qname, rname, qseq, qual );
}

void CSamRecord::InitPairedHit( const string& qname, TFlags flags, const string& rname, TPos pos, TMapQ mapq, const TCigar& cigar, TPos mpos, TOff isize, const string& qseq, const string& qual )
{
    SetNull();
    m_flags = flags;
    m_mapq = mapq;
    m_mpos = mpos;
    m_isize = isize;
    if( rname == "*" ) THROW( runtime_error, "InitPairedHit should not use RNAME = *" );
    x_InitStrings( qname, rname, qseq, qual );
}

CSamRecord::TCigar CSamRecord::GetBasicCigar() const 
{
    TCigar ret;
    for( TCigar::const_iterator x = m_cigar.begin(); x != m_cigar.end(); ++x ) {
        switch( x->GetEvent() ) {
            case CTrBase::eEvent_Changed: 
            case CTrBase::eEvent_Replaced: ret.AppendItem( CTrBase::eEvent_Match, x->GetCount() ); break;
            case CTrBase::eEvent_Overlap: ret.AppendItem( CTrBase::eEvent_Insertion, x->GetCount() ); break;
            default: ret.AppendItem( *x ); break;
        }
    }
    return ret;
}

const char * CSamRecord::GetMateRefName() const
{
    if( m_flags & fPairedHit ) return m_rname; else return "";
}

CSamRecord::ESamTagType CSamRecord::GetTagType( const string& name ) const
{
    if( m_tags == 0 ) return eTag_none;
    TTags::const_iterator x = m_tags->find( name );
    if( x == m_tags->end() ) return eTag_none;
    return ESamTagType( x->second[0] );
}

string CSamRecord::GetTagString( const string& name ) const
{
    if( m_tags == 0 ) THROW( runtime_error, "No tag " << name << " found" );
    TTags::const_iterator x = m_tags->find( name );
    if( x == m_tags->end() ) THROW( runtime_error, "No tag " << name << " found" );
    return x->second.substr(2);
}

int CSamRecord::GetTagInt( const string& name ) const
{
    return NStr::StringToInt( GetTagString( name ) );
}

float CSamRecord::GetTagFloat( const string& name ) const
{
    return NStr::StringToDouble( GetTagString( name ) );
}

void CSamRecord::DropTag( const string& name ) 
{
    if( m_tags ) {
        m_tags->erase( name );
        if( m_tags->size() == 0 ) { delete m_tags; m_tags = 0; }
    }
}

void CSamRecord::SetTagString( const string& name, const string& value )
{
    if( m_tags == 0 ) m_tags = new TTags();
    (*m_tags)[name] = "Z:" + value;
}

void CSamRecord::SetTagInt( const string& name, int value )
{
    if( m_tags == 0 ) m_tags = new TTags();
    (*m_tags)[name] = "i:" + NStr::IntToString( value );
}

void CSamRecord::SetTagFloat( const string& name, float value )
{
    if( m_tags == 0 ) m_tags = new TTags();
    (*m_tags)[name] = "f:" + NStr::DoubleToString( value );
}

CSamRecord::TRange CSamRecord::GetQryBounding() const
{
    if( IsUnmapped() ) return TRange();
    if( int l = m_cigar.ComputeLengths().first ) {
        if( !IsReverse() ) {
            TCigar::const_iterator x = m_cigar.begin();
            int h = ( x->GetEvent() == CTrBase::eEvent_SoftMask || x->GetEvent() == CTrBase::eEvent_HardMask ) ? x->GetCount() : 0;
            return TRange( h, h + l - 1 );
        } else {
            TCigar::const_reverse_iterator x = m_cigar.rbegin();
            int h = ( x->GetEvent() == CTrBase::eEvent_SoftMask || x->GetEvent() == CTrBase::eEvent_HardMask ) ? x->GetCount() : 0;
            return TRange( h, h + l - 1 );
        }
    }
    return TRange();
}

CSamRecord::TRange CSamRecord::GetRefBounding() const
{
    return TRange( m_pos, m_pos + m_cigar.ComputeLengths().second - 1 );
}

bool CSamRecord::IsQueryMateOf( const CSamRecord& other ) const
{
    if( IsNull() ) return false;
    if( !IsPairedRead() ) return false;
    if( !other.IsPairedRead() ) return false;
    if( m_flags & fSeqIsFirst && other.m_flags & fSeqIsFirst ) return false;
    if( m_flags & fSeqIsSecond && other.m_flags & fSeqIsSecond ) return false;
    if( strcmp( m_qname, other.m_qname ) ) return false;
    return true;
}

bool CSamRecord::IsHitMateOf( const CSamRecord& other ) const
{
    if( IsNull() ) return false;
    if( !IsPairedHit() ) return false;
    if( !other.IsPairedHit() ) return false;
    if( m_flags & fSeqIsFirst && other.m_flags & fSeqIsFirst ) return false;
    if( m_flags & fSeqIsSecond && other.m_flags & fSeqIsSecond ) return false;
    if( strcmp( m_qname, other.m_qname ) ) return false;
    if( strcmp( m_rname, other.m_rname ) ) return false;
    if( IsReverse() != bool( other.m_flags & fMateReverse ) ) return false;
    if( other.IsReverse() != bool( m_flags & fMateReverse ) ) return false;
    if( m_pos != other.m_mpos ) return false;
    if( m_mpos != other.m_pos ) return false;
    if( m_isize != -other.m_isize ) return false;
    // TODO: check isize (???)
    return true;
}

namespace {

    class CChkNull
    {
    public:
        CChkNull( const string& c ) : m_data(c.c_str()) {}
        CChkNull( const char * c ) : m_data(c) {}
        void Write( ostream& out ) const { if( m_data && *m_data ) out << m_data; else out << "*"; }
    protected:
        const char * m_data;
    };
    ostream& operator << ( ostream& out, const CChkNull& c ) { c.Write( out ); return out; }

    void CSamRecord::WriteAsSam( ostream& out, bool basic ) const
    {
        if( IsNull() ) return;
        out << GetQName() << "\t"
            << GetFlags() << "\t"
            << CChkNull( GetRName() ) << "\t"
            << GetPos() << "\t"
            << (int)GetMapQual() << "\t"
            << CChkNull((basic?GetBasicCigar().ToString():GetImprovedCigar().ToString())) << "\t"
            << (IsPairedHit()? "=":"*") << "\t"
            << GetMatePos() << "\t"
            << GetInsertSize() << "\t"
            << CChkNull(GetQueryIupac()) << "\t"
            << CChkNull(GetQueryQual());
        if( m_tags && m_tags->size() ) {
            ITERATE( TTags, t, (*m_tags ) ) 
                out << "\t" << t->first << t->second;
        }
        out << "\n";
    }
}

double CSamRecord::ComputeScore( const TCigar& cigar, double id, double mm, double go, double ge )
{
    double score = 0;
    ITERATE( TCigar, x, cigar ) {
        if( x->GetCount() < 1 ) continue;
        switch( x->GetEvent() ) {
            case CTrBase::eEvent_Changed:
            case CTrBase::eEvent_Match: score += id * x->GetCount(); break;
            case CTrBase::eEvent_Replaced: score += mm * x->GetCount(); break;
            case CTrBase::eEvent_Deletion:
            case CTrBase::eEvent_Insertion: score += go + ge * (x->GetCount() - 1); break;
            default: break;
        }
    }
    return score;
}

void CSamRecord::SetPaired( CSamRecord& other )
{
    if( strcmp( other.GetRName(), GetRName() ) ) 
        THROW( runtime_error, "Can't pair hits which are on different reference sequences" );
    if( strcmp( other.GetQName(), GetQName() ) )
        THROW( runtime_error, "Can't pair hits for queries with different IDs" );
    if( GetFlags( fSeqIsFirst ) && other.GetFlags( fSeqIsFirst ) ||
            GetFlags( fSeqIsSecond ) && other.GetFlags( fSeqIsSecond ) )
        THROW( runtime_error, "Can't pair hits if sequence role (first/second) is the same for both hits" );
    
    m_flags |= fPairedHit|fPairedQuery;
    m_flags &= ~(fSeqUnmapped|fMateUnmapped);
    m_mpos = other.m_pos;
    m_isize = other.m_pos - m_pos;

    other.m_flags |= fPairedHit|fPairedQuery;
    other.m_flags &= ~(fSeqUnmapped|fMateUnmapped);
    other.m_mpos = m_pos;
    other.m_isize = m_pos - other.m_pos;
}

////////////////////////////////////////////////////////////////////////
// CSamMdParser

CSamMdParser::TCigar CSamMdParser::GetFullCigar()
{
    if( m_mdtag.length() == 0 ) return m_cigar;
    if( m_cigar.size() == 0 ) return m_cigar;
    m_current = m_cigar.begin();
    m_mdptr = m_mdtag.c_str();
    m_ccount = m_current->GetCount();
    m_cevent = m_current->GetEvent();
    TCigar fullcigar;
    // first skip masked part
    while( GetEvent() == CTrBase::eEvent_SoftMask || GetEvent() == CTrBase::eEvent_HardMask ) {
        fullcigar.AppendItem( *m_current++ );
        if( m_current == m_cigar.end() ) return fullcigar;
        m_ccount = m_current->GetCount();
        m_cevent = m_current->GetEvent();
    }
    while( *m_mdptr ) {
        if( isdigit( *m_mdptr ) ) {
            int count = strtol( m_mdptr, (char**)&m_mdptr, 10 );
            while( count ) {
                while( GetEvent() == eEvent_Insertion ) {
                    fullcigar.AppendItem( GetEvent(), GetCount() );
                    if( ++m_current == m_cigar.end() ) THROW( runtime_error, "Unexpected end of CIGAR " << m_cigar.ToString() << ", MD:Z:" << m_mdtag );
                    m_cevent = m_current->GetEvent();
                    m_ccount = m_current->GetCount();
                }
                if( GetCount() == 0 && *m_mdptr == 0 ) return fullcigar;
                if( GetEvent() == eEvent_Match ) {
                    fullcigar.AppendItem( GetEvent(), min( GetCount(), count ) );
                    if( count < GetCount() ) { m_ccount -= count; count = 0; }
                    else if( count >= GetCount() ) {
                        count -= GetCount();
                        if( ++m_current == m_cigar.end() ) {
                            if( *m_mdptr || count > 0 ) 
                                THROW( runtime_error, "too short CIGAR " << m_cigar.ToString() << " for MD:Z:" << m_mdtag << " (" << m_mdptr << ")" );
                            else break;
                        }
                        m_ccount = m_current->GetCount();
                        m_cevent = m_current->GetEvent();
                    }
                } else break;
            }
        } else if( strchr( "ACGTN", *m_mdptr ) ) {
            again:
            switch( GetEvent() ) {
                case eEvent_Match:
                case eEvent_Changed:
                case eEvent_Replaced: break;
                case eEvent_Insertion: fullcigar.AppendItem( GetEvent(), GetCount() ); NextEvent(); goto again;
                default: THROW( runtime_error, "Unexpected CIGAR event `" << CTrBase::Event2Char( GetEvent() ) << "' (" << int(GetEvent()) << ") when parsing MD mismatch for CIGAR " << m_cigar.ToString() << " (ptr=" << (m_current - m_cigar.begin()) << ", cnt=" << m_ccount << ") MD:Z:" << m_mdtag << " (off=" << (m_mdptr-m_mdtag.c_str()) << ") " );
            }
            fullcigar.AppendItem( eEvent_Replaced, 1 );
            ++m_mdptr;
            while( NextEvent() == eEvent_Insertion ) fullcigar.AppendItem( eEvent_Insertion, 1 );
        } else if( *m_mdptr == '^' ) {
            if( GetEvent() != eEvent_Deletion && GetEvent() != eEvent_Splice )
                THROW( runtime_error, "Unexpected CIGAR event " << CTrBase::Event2Char( GetEvent() ) << " when parsing MD deletion for CIGAR " << m_cigar.ToString() << " MD:Z:" << m_mdtag << " (" << m_mdptr << ")"  );
            while( strchr( "ACGTN", *++m_mdptr ) ) {
                fullcigar.AppendItem( GetEvent(), 1 );
                NextEvent();
            }
            while( NextEvent() == eEvent_Insertion ) fullcigar.AppendItem( eEvent_Insertion, 1 );
        } else THROW( runtime_error, "Unexpected MD item '" << *m_mdptr << "' for CIGAR " << m_cigar.ToString() << " and MD:Z:" << m_mdtag );
    }
    return fullcigar;
};

