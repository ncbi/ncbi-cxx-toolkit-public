#include <ncbi_pch.hpp>
#include "csamalignments.hpp"
#include "csammdformatter.hpp"
#include "cprogressindicator.hpp"
#include "string-util.hpp"
#include "ialigner.hpp"

USING_OLIGOFAR_SCOPES;

CSRead::CSRead( const string& id, CSeqCoding::ECoding coding, const char * sequence, int length, EPairType intended )
{
    ++s_count;
    ASSERT( coding == CSeqCoding::eCoding_ncbi8na || coding == CSeqCoding::eCoding_ncbiqna );
    m_data = new char[x_GetDataSize( id.length(), length, eRead_single )];
    m_data[0] = id.length();
    m_data[1] = length;
    m_data[2] = eRead_single | ( intended << 2 );
    m_data[3] = coding; // quality.length() ? CSeqCoding::eCoding_ncbiqna : CSeqCoding::eCoding_ncbi8na ;
    strcpy( m_data + x_GetIdOffset(), id.c_str() );
    char * d = m_data + x_GetSequenceOffset();
    copy( sequence, sequence + length, d );
}

CSRead::CSRead( const string& id, const string& sequence, const string& quality, EPairType intended ) 
{
    ++s_count;
    if( quality.length() && quality.length() != sequence.length() ) 
        THROW( runtime_error, "Sequence lendgth and quality length differ (" << sequence << ", " << quality << ") !" );
    m_data = new char[x_GetDataSize( id.length(), sequence.length(), eRead_single )];
    m_data[0] = id.length();
    m_data[1] = sequence.length();
    m_data[2] = eRead_single | ( intended << 2 );
    m_data[3] = quality.length() ? CSeqCoding::eCoding_ncbiqna : CSeqCoding::eCoding_ncbi8na ;
    strcpy( m_data + x_GetIdOffset(), id.c_str() );
    char * d = m_data + x_GetSequenceOffset();
    if( quality.length() ) {
        for( const char * s = sequence.c_str(), * q = quality.c_str(); *s; ++s, ++q ) {
            if( *s != '-' ) *d++ = CNcbiqnaBase( CIupacnaBase( *s ), *q - 33 );
        }
    } else {
        for( const char * s = sequence.c_str(); *s; ++s ) {
            if( *s != '-' ) *d++ = CNcbi8naBase( CIupacnaBase( *s ) );
        }
    }
}

CSRead::CSRead( const CSRead& r ) 
{
    ++s_count;
    int ds = x_GetDataSize( strlen(r.GetId()), r.x_GetSequenceLength(), r.x_GetPairType() );
    m_data = new char[ds];
    memcpy( m_data, r.m_data, ds );
}

CSRead& CSRead::operator = ( const CSRead& r ) 
{
    delete[] m_data;
    int ds = x_GetDataSize( strlen(r.GetId()), r.x_GetSequenceLength(), r.x_GetPairType() );
    m_data = new char[ds];
    memcpy( m_data, r.m_data, ds );
    return *this;
}

void CSRead::SetMate( CSRead * other ) 
{
    if( !other ) return;
    if( x_GetPairType() != eRead_single || other->x_GetPairType() != eRead_single ) {
        if( other->x_GetMate() == this ) return;
        THROW( logic_error, "Can't make read paired: it is already paired" );
    }
    if( GetIntendedPairType() != eRead_first || other->GetIntendedPairType() != eRead_second ) {
        THROW( logic_error, "Intender pair type of the read differs from what is getting to be actual" );
    }
    char * newdata = new char[x_GetDataSize( x_GetIdLength(), x_GetSequenceLength(), eRead_first )];
    const char * id = m_data + x_GetIdOffset();
    const char * seq = m_data + x_GetSequenceOffset();
    newdata[0] = x_GetIdLength();
    newdata[1] = x_GetSequenceLength();
    newdata[2] = eRead_first | (GetIntendedPairType() << 2);
    newdata[3] = x_GetSequenceCoding();
    swap( newdata, m_data );
    copy( id, id + x_GetIdLength() + 1, m_data + x_GetIdOffset() );
    copy( seq, seq + x_GetSequenceLength() + 1, m_data + x_GetSequenceOffset() );
    x_SetMate() = other;
    delete[] newdata;
    newdata = new char[other->x_GetDataSize( other->x_GetIdLength(), other->x_GetSequenceLength(), eRead_second )];
    id = other->m_data + other->x_GetIdOffset();
    seq = other->m_data + other->x_GetSequenceOffset();
    newdata[0] = 0; // other->x_GetIdLength();
    newdata[1] = other->x_GetSequenceLength();
    newdata[2] = eRead_second | (other->GetIntendedPairType() << 2);
    newdata[3] = other->x_GetSequenceCoding();
    swap( newdata, other->m_data );
    copy( seq, seq + other->x_GetSequenceLength() + 1, other->m_data + other->x_GetSequenceOffset() );
    other->x_SetMate() = this;
    delete[] newdata;
}

CContig::CContig( const string& name ) : 
    m_id( name ), m_data( 0 ), m_size( 0 ), m_offset( 0 ), m_coding( CSeqCoding::eCoding_ncbi8na ), m_owns( false ) 
{ 
    ++s_count; 
}

CContig::CContig( const CContig& other ) :
    m_id( other.m_id ), m_data( 0 ), m_size( other.m_size ), m_offset( other.m_offset ), m_coding( other.m_coding ), m_owns( other.m_owns )
{
    ++s_count;
    if( other.m_owns ) {
        m_data = new char[ other.m_size - other.m_offset ];
        memcpy( m_data, other.m_data, other.m_size - other.m_offset );
    } else {
        m_data = other.m_data;
    }
}

int CSRead::s_count = 0;
int CContig::s_count = 0;
int CMappedAlignment::s_count = 0;
bool CMappedAlignment::s_validate_consistency = true;

CMappedAlignment::CMappedAlignment( CSRead * query, CContig * subject, int from, const TTrVector& cigar, CSeqCoding::EStrand strand, int flags, TTags * tags ) : 
    m_mate(0), m_flags( flags ), m_tags( tags )
{
    ++s_count;
    m_query = query;
    m_subject = subject;
    m_sstart = from;
    m_slength = cigar.ComputeSubjectLength();
    m_qstart = 0;
    m_qlength = query->GetSequenceLength();
    m_cigar.Assign( cigar );
    switch( strand ) {
    case CSeqCoding::eStrand_pos:
        if( cigar.front().GetEvent() == CTrBase::eEvent_SoftMask ) {
            m_qstart += cigar.front().GetCount();
            m_qlength -= cigar.front().GetCount();
        }
        if( cigar.back().GetEvent() == CTrBase::eEvent_SoftMask ) {
            m_qlength -= cigar.back().GetCount();
        }
        break;
    case CSeqCoding::eStrand_neg:
        if( cigar.back().GetEvent() == CTrBase::eEvent_SoftMask ) {
            m_qstart += cigar.back().GetCount();
            m_qlength -= cigar.back().GetCount();
        }
        if( cigar.front().GetEvent() == CTrBase::eEvent_SoftMask ) {
            m_qlength -= cigar.front().GetCount();
        }
        m_qstart += m_qlength - 1;
        m_qlength *= -1;
        break;
        //m_qstart = query->GetSequenceLength() - m_qstart - m_qlength;
        //m_qstart += m_qlength - 1;
        //m_qlength *= -1;
    }
    if( s_validate_consistency ) ValidateConsistency( "CMappedAlignment constructor" );
//    cerr << __PRETTY_FUNCTION__ << DISPLAY( m_qstart ) << DISPLAY( m_qlength ) << "\n";
}

double CMappedAlignment::ScoreAlignment( double id, double mm, double go, double ge ) const
{
    return ScoreAlignment( m_cigar, id, mm, go, ge );
}

double CMappedAlignment::ScoreAlignment( const TTrSequence& tr, double id, double mm, double go, double ge )
{
    double score = 0;
    for( TTrSequence::const_iterator x = tr.begin(); x != tr.end(); ++x ) {
        switch( x->GetEvent() ) {
            case CTrBase::eEvent_Changed:
            case CTrBase::eEvent_Match: score += id * x->GetCount(); break;
            case CTrBase::eEvent_Replaced: score += mm * x->GetCount(); break;
            case CTrBase::eEvent_Deletion:
            case CTrBase::eEvent_Insertion: score += go + (x->GetCount() - 1) * ge; break;
            default:;
        }
    }
    return score;
}

void CMappedAlignment::SetMate( CMappedAlignment * other )
{
    if( other == 0 ) return;
    ASSERT( m_mate == 0 || m_mate == other );
    ASSERT( other->GetMate() == 0 || other->GetMate() == this );
    m_mate = other;
    other->m_mate = this;
}

CSamSource::~CSamSource()
{
    ITERATE( TAlignmentMap, a, m_alignmentsByContig ) 
        ITERATE( TAlignmentList, l, a->second )
            delete *l;
    ITERATE( TReads, r , m_reads ) { 
        if( CSRead * mate = dynamic_cast<CSRead*>( const_cast<IShortRead*>( r->second->GetMate() ) ) ) 
            delete mate; 
        delete r->second; 
    }
    ITERATE( TContigs, c, m_contigs ) delete c->second;
}

CMappedAlignment * CSamSource::ParseSamLine( const string& line, int reg )
{
    // This function should parse the line, create CShortRead and CContig if necessary, 
    // making sure that these objects are placed in maps, and create and store reference to
    // new CMappedAlignment object
    
    vector<string> fields;
    Split( line, "\t", back_inserter( fields ) );
    if( fields.size() < 6 ) 
        THROW( runtime_error, "SAM is supposed to have at least 6 columns, got " << 
                fields.size() << " in [" << line << "] in line" );

    return ParseSamLine( fields, reg );
}

CMappedAlignment * CSamSource::ParseSamLine( const vector<string>& fields, int reg ) 
{
    if( reg & fRegisterAlignmentByQuery ) reg |= fRegisterQuery;
    if( reg & fRegisterAlignmentBySubject ) reg |= fRegisterSubject;
    unsigned flags = NStr::StringToInt( fields[eCol_flags] );
    int pos1 = NStr::StringToInt( fields[eCol_pos] ) - 1;
    const string& id = fields[eCol_qname];
    const string& ctg = fields[eCol_rname];
    const string& cigar = fields[eCol_cigar];
    string  seq = fields[eCol_seq];
    string qual = fields[eCol_qual];
    if( qual == "*" ) qual = "";
    CSRead::EPairType ipt = flags & fPairedQuery ? flags & fSeqIsFirst ? CSRead::eRead_first : CSRead::eRead_second : CSRead::eRead_single;
    CSeqCoding::EStrand strand = flags & fSeqReverse ? CSeqCoding::eStrand_neg : CSeqCoding::eStrand_pos;
    if( flags & fSeqReverse ) { // since mapped reads are represented on genomic strand
        for( int i = 0, j = seq.length() - 1; i < j; ++i, --j ) {
            char x = CIupacnaBase( seq[i] ).Complement();
            seq[i] = CIupacnaBase( seq[j] ).Complement();
            seq[j] = x;
            if( qual.length() ) swap( qual[i], qual[j] );
        }
        if( seq.length() & 1 ) {
            int i = seq.length()/2;
            seq[i] = CIupacnaBase( seq[i] ).Complement();
        }
    }

    CContig * contig = 0;
    if( reg & fRegisterSubject ) {
        TContigs::iterator ictg = m_contigs.find( ctg );
        if( ictg == m_contigs.end() ) {
            ictg = m_contigs.insert( ictg, make_pair( ctg, contig = new CContig( ctg ) ) );
        }
    } else contig = new CContig( ctg );
    CSRead * read = 0; 
    if( reg & fRegisterQuery ) {
        TReads::iterator iread = m_reads.find( id );
        if( iread == m_reads.end() ) {
            iread = m_reads.insert( iread, make_pair( id, read = new CSRead( id, seq, qual, ipt ) ) );
        } else  {
            read = iread->second;
            if( read->GetPairType() != ipt ) {
                if( const IShortRead * rm = read->GetMate() ) {
                    if( rm->GetPairType() != ipt ) 
                        THROW( runtime_error, "Some strange problem with SAM flags: " << DISPLAY( read->GetPairType() ) << DISPLAY( rm->GetPairType() ) << DISPLAY( ipt ) );
                    else read = dynamic_cast<CSRead*>( const_cast<IShortRead*>( rm ) );
                } else 
                    THROW( runtime_error, "Some problem with hit SAM flags: sequence is marked both as paired and not paired" );
            } else if( read->GetIntendedPairType() != ipt ) {
                CSRead * mate = new CSRead( id, seq, qual, ipt );
                switch( ipt ) {
                    case CSRead::eRead_single: THROW( logic_error, "Really weird!" );
                    case CSRead::eRead_first: mate->SetMate( read ); iread->second = mate; read = mate; break;
                    case CSRead::eRead_second: read->SetMate( mate ); read = mate; break;
                }
            } else { /* nothing to do - we've found it */ }
        }
    } else read = new CSRead( id, seq, qual, ipt );

    string mism = "";

    map<string,string> * tags = 0;
    for( unsigned i = eCol_tags; i < fields.size(); ++i ) {
        // spaces are allowed in tags (see SAM 1.2)
        if( fields[i].substr( 0, 5 ) == "MD:Z:" ) mism = fields[i].substr( 5 );
        else {
            if( tags == 0 ) tags = new CMappedAlignment::TTags();
            const string& tag = fields[i];
            size_t col = tag.find( ':' );
            if( col == string::npos ) THROW( runtime_error, "Tag does not have `:' in " << Join( "\t", fields ) );
            tags->insert( make_pair( tag.substr( 0, col ), tag.substr( col + 1 ) ) );
        }
    }
    TTrVector fullcigar = GetFullCigar( cigar, mism );
#ifndef CORRECT_BAD_INPUT_CIGAR
#define CORRECT_BAD_INPUT_CIGAR 1
#endif
#if CORRECT_BAD_INPUT_CIGAR
    int al = fullcigar.ComputeLengths().first;
    int pl = fullcigar.front().GetEvent() == CTrBase::eEvent_SoftMask ? fullcigar.front().GetCount() : 0;
    int sl = fullcigar.back().GetEvent() == CTrBase::eEvent_SoftMask ? fullcigar.back().GetCount() : 0;
    int ql = seq.length();
    if( mism == "" && ((al + pl + sl - 1) == ql)) {
        TTrVector other;
        TTrVector::iterator x = fullcigar.begin();
        if( pl == 1 || sl == 1 ) {
            other.AppendItem( *x++ );
            if( pl == 1 ) { 
                other.AppendItem( x->GetEvent(), x->GetCount() - 1 ); ++x; 
                for( ; x != fullcigar.end(); ++x ) other.AppendItem( *x );
            }
            else {
                TTrVector::iterator y = x; ++y; ++y;
                for( ; y != fullcigar.end(); ++y, ++x ) other.AppendItem( *x );
                other.AppendItem( x->GetEvent(), x->GetCount() - 1 ); 
                other.AppendItem( *++x );
            }
        } else if( pl == 0 || sl == 0 ) {
            if( pl ) {
                other.AppendItem( *x++ );
                other.AppendItem( x->GetEvent(), x->GetCount() - 1 );
                for( ; x != fullcigar.end(); ++x ) other.AppendItem( *x );
            } else {
                TTrVector::iterator y = x; ++y; ++y;
                for( ; y != fullcigar.end(); ++y, ++x ) other.AppendItem( *x );
                other.AppendItem( x->GetEvent(), x->GetCount() - 1 ); 
                other.AppendItem( *++x );
            }
        }
        cerr << "WARNING: Correcting " << fullcigar.ToString() << " to " << other.ToString() << " for " << id 
            << " " << seq << " on " << ctg << " at " << pos1 << " " << (strand == CSeqCoding::eStrand_pos ? "fwd" : "rev" ) << "\n";
        fullcigar.swap( other );
    }
#endif
    CMappedAlignment * align = 0;
    try {
        align = new CMappedAlignment( read, contig, pos1, fullcigar, strand, 0, tags );
    } catch( exception& e ) {
        cerr << "ERROR: " << e.what() << " while parsing " << Join( "\t", fields ) << ": ignoring\n";
        delete align;
        delete read;
        delete contig;
        return 0;
    }
    if( reg & fRegisterQuery )   ; else align->SetFlags( CMappedAlignment::fOwnQuery );
    if( reg & fRegisterSubject ) ; else align->SetFlags( CMappedAlignment::fOwnSubject );
    // TODO: pairing of alignments is not ready yet

    const string& ctg2 = fields[eCol_mrnm];
    int pos2 = NStr::StringToInt( fields[eCol_mpos] );

    if( flags & fPairedHit ) {
        if( fields.size() < 7 )
            THROW( runtime_error, "OligoFAR needs mate position in SAM file for paired reads" );
        TPendingHits::iterator i = m_pendingHits.find( make_pair( id, make_pair( ctg2, pos2 ) ) );
        if( i == m_pendingHits.end() ) m_pendingHits.insert( make_pair( make_pair( id, make_pair( ctg, pos1 ) ), align ) );
        else {
            if( ipt == CSRead::eRead_second ) i->second->SetMate( align );
            else align->SetMate( i->second );
            m_pendingHits.erase( i );
        }
    }
    if( reg & fRegisterAlignmentBySubject ) m_alignmentsByContig[ctg].push_back( align ); 
    if( reg & fRegisterAlignmentByQuery )   m_alignmentsByRead[id].push_back( align ); 
    return align;
}

bool CMappedAlignment::ValidateConsistency( const string& context ) const
{
    pair<int,int> lengths = m_cigar.ComputeLengths();
    int fullLength = GetQuery()->GetSequenceLength();
    int prefix = m_cigar.front().GetEvent() == CTrBase::eEvent_SoftMask ? m_cigar.front().GetCount() : 0;
    int suffix = m_cigar.back().GetEvent() == CTrBase::eEvent_SoftMask ? m_cigar.back().GetCount() : 0;
    if( IsReverseComplement() ) swap( prefix, suffix );
    try {
        ASSERT( lengths.first == GetQueryBounding().GetLength() );
        ASSERT( lengths.second == GetSubjectBounding().GetLength() );
        ASSERT( prefix == GetQueryBounding().GetFrom() );
        ASSERT( fullLength - suffix - 1 == GetQueryBounding().GetTo() );
    } catch( exception& e ) {
        cerr 
            << "\x1b[34;42m" << context << "\x1b[0m\n"
            << "\x1b[33;41m"
            << GetQuery()->GetId() << "\t"
            << GetSubject()->GetId() << "\t"
            << m_query->GetIupacna() << "\t"
            << m_qstart << (m_qlength > 0 ? "+" : "") << m_qlength << "\t"
            << m_sstart << "+" << m_slength << "\t"
            << m_cigar.ToString() << "\t"
            << GetQueryBounding() << "\t"
            << GetSubjectBounding() << "\t"
            << (IsReverseComplement() ? "-" : "+")
            << "\x1b[0m"
            << "\n";
        cerr << "\x1b[32m"
            << DISPLAY( lengths.first )
            << DISPLAY( lengths.second )
            << DISPLAY( IsReverseComplement() )
            << DISPLAY( fullLength )
            << DISPLAY( prefix )
            << DISPLAY( suffix )
            << "\x1b[0m"
            << "\n";
        WriteAsSam( cerr );
        throw;
    }
    return true;
}


TTrVector CSamSource::GetFullCigar( const TTrVector& cigar, const string& mismatches )
{
    // cigar  = 10A5^C10
    // misms  = 16M1D4M1I6M
    // result = 10M1R5M1D4M1I6M
    
    TTrVector fullcigar;
    const char * misms = mismatches.c_str();
    if( misms[0] ) {
        const char * m = misms;
        TTrVector::const_iterator c = cigar.begin();
        
        while( c != cigar.end() && c->GetEvent() == CTrBase::eEvent_SoftMask || c->GetEvent() == CTrBase::eEvent_HardMask ) 
            fullcigar.AppendItem( *c++ );
        
        int acc = 0;
        
        string errmsgbase;
        do {
            ostringstream s;
            s << "Inconsistent cigar " << cigar.ToString() << " and mismatch string " << misms;
            errmsgbase = s.str();
        } while( 0 );
        
        while( *m ) {
            if( c == cigar.end() ) 
                THROW( runtime_error, errmsgbase << ": cigar is too short" );
            if( isdigit( *m ) ) {
                int count = strtol( m, (char**)&m, 10 );
                if( c->GetEvent() != CTrBase::eEvent_Match ) 
                    THROW( runtime_error, errmsgbase << ": cigar M is expected" );
                acc -= c->GetCount() - count;
                fullcigar.AppendItem( CTrBase::eEvent_Match, min( (int) c->GetCount(), count ) );
                if( acc == 0 ) { ++c; continue; }
                else if( acc < 0 ) {
                    ++c;
                    // NB: overlap is not supported here... it is not clear what to do according to SAM specs
                    if( c == cigar.end() || c->GetEvent() != CTrBase::eEvent_Insertion ) 
                        THROW( runtime_error, errmsgbase << ": cigar I is expected" );
                    fullcigar.AppendItem( CTrBase::eEvent_Insertion, c->GetCount() );
                    ++c;
                } else { // acc > 0
                    // do nothing
                }
            } else if( strchr( "ACGTN", *m ) ) {
                fullcigar.AppendItem( CTrBase::eEvent_Replaced, 1 ); 
                ++c;
            } else if( *m == '^' ) {
                if( acc != 0 ) 
                    THROW( runtime_error, errmsgbase << ": misplaced deletion" ); 
                int k = c->GetCount();
                if( c->GetEvent() != CTrBase::eEvent_Deletion && c->GetEvent() != CTrBase::eEvent_Splice ) 
                    THROW( runtime_error, errmsgbase << ": N or D is expected" );
                while( strchr( "ACGTN", *++m ) ) {
                    fullcigar.AppendItem( c->GetEvent(), 1 );
                    --k;
                }
                if( k != 0 ) 
                    THROW( runtime_error, errmsgbase << ": different sizes of deletion or splice" );
                ++c;
            }
        }
        while( c != cigar.end() ) {
            if( c->GetEvent() != CTrBase::eEvent_SoftMask && c->GetEvent() != CTrBase::eEvent_HardMask ) 
                THROW( runtime_error, errmsgbase << ": cigar is too long" );
            fullcigar.AppendItem( *c++ );
        }
    } else {
        fullcigar = cigar;
    }
    return fullcigar;
}

void CSamSource::IndexFile( const string& samFile, unsigned start, unsigned limit )
{
    ifstream in( samFile.c_str() );
    string buff;
    string lastId;
    CProgressIndicator p( "Indexing " + samFile + " by contig" );
    unsigned cnt = 0;
    limit += start;
    for( Uint8 offset = 0; getline( in, buff ); offset = in.tellg() ) {
        if( buff[0] == '@' ) continue;
        if( cnt++ < start ) continue;
        if( cnt > limit ) break;
        vector<string> fields;
        Split( buff, "\t", back_inserter( fields ) );
        if( fields.size() < 6 ) 
            THROW( runtime_error, "SAM is supposed to have at least 6 columns, got " << 
                    fields.size() << " in [" << buff << "] in line" );
        if( fields[eCol_rname] != lastId ) {
            m_alignmentOffsets[lastId = fields[eCol_rname]].push_back( offset );
        }
        p.Increment();
    }
    p.Summary();
}

string CSRead::GetIupacna( CSeqCoding::EStrand strand ) const
{
    string iupac( x_GetSequenceLength(), '.' );
    const char * seqdata = m_data + x_GetSequenceOffset();
    int i = 0;
    int I = iupac.length();
    int j = strand == CSeqCoding::eStrand_pos ? 0 : iupac.size() - 1;
    int d = strand == CSeqCoding::eStrand_pos ? 1 : -1;
    switch( x_GetSequenceCoding() ) {
        case CSeqCoding::eCoding_ncbi8na:
            for( ; i < I; ++i, (j += d) ) { iupac[i] = CIupacnaBase( CNcbi8naBase( seqdata[j], strand ) ); }
            break;
        default:
            THROW( logic_error, __PRETTY_FUNCTION__ << " does not support sequence coding other then NCBI-8na" );
            break;
    }
    return iupac;
}

void CMappedAlignment::WriteAsSam( ostream& out ) const
{
    CSeqCoding::EStrand strand = IsReverseComplement() ? CSeqCoding::eStrand_neg : CSeqCoding::eStrand_pos;
    int flags = IsReverseComplement() ? CSamBase::fSeqReverse : 0;
    string iupac = m_query->GetIupacna( strand );
    TTags * tt = m_tags;
    if( GetSubject()->GetSequenceData() != 0 && GetSubject()->GetSequenceCoding() == CSeqCoding::eCoding_ncbi8na ) {
        CSamMdFormatter md(  GetSubject()->GetSequenceData() + GetSubjectBounding().GetFrom(), iupac.c_str(), GetCigar() );
        if( md.Format() ) {
            tt = new TTags();
            if( m_tags ) copy( m_tags->begin(), m_tags->end(), inserter( *tt, tt->end() ) );
            tt->insert( make_pair( "MD:Z", md.GetString() ) );
        }
    }
    out 
        << GetQuery()->GetId() << "\t"
        << flags << "\t"
        << GetSubject()->GetId() << "\t"
        << (GetSubjectBounding().GetFrom() + 1) << "\t"
        << 255 << "\t"
        << GetCigar().ToString() << "\t"
        << "*\t0\t0\t" << iupac << "\t*";
    if( tt ) {
        ITERATE( TTags, tag, (*tt) ) {
            out << "\t" << tag->first << ":" << tag->second;
        }
        if( tt != m_tags ) delete tt;
    }
    out << "\n";
}

CMappedAlignment::CMappedAlignment( const CMappedAlignment& other ) :
    m_query( other.m_flags & fOwnQuery ? new CSRead( *other.m_query ) : m_query ),
    m_subject( other.m_flags & fOwnSubject ? new CContig( *other.m_subject ) : m_subject ),
    m_mate( 0 ),
    m_sstart( other.m_sstart ),
    m_slength( other.m_slength ),
    m_qstart( other.m_qstart ),
    m_qlength( other.m_qlength ),
    m_flags( other.m_flags ),
    m_cigar( other.m_cigar ),
    m_tags( other.m_tags ? new TTags( *other.m_tags ) : 0 )
{
    ++s_count;
}

void CMappedAlignment::Assign( const CMappedAlignment * other ) 
{
    if( this == other ) { return; }
    ASSERT( other );
    if( m_query != other->m_query ) {
        if( m_flags & fOwnQuery ) delete m_query;
        if( other->m_flags & fOwnQuery ) {
            m_query = new CSRead( *other->m_query );
            m_flags |= fOwnQuery;
        } else {
            m_query = other->m_query;
            m_flags &= ~fOwnQuery;
        }
    }
    if( m_subject != other->m_subject ) {
        if( m_flags & fOwnSubject ) delete m_subject;
        if( other->m_flags & fOwnSubject ) {
            m_subject = new CContig( *other->m_subject );
            m_flags |= fOwnSubject;
        } else {
            m_subject = other->m_subject;
            m_flags &= ~fOwnSubject;
        }
    }
    if( m_tags ) delete m_tags;
    m_mate = 0;
    m_sstart = other->m_sstart;
    m_slength = other->m_slength;
    m_qstart = other->m_qstart;
    m_qlength = other->m_qlength;
    m_flags = other->m_flags; //(other->m_flags&~fOwnSeqs) | (m_flags&fOwnSeqs);
    m_cigar = other->m_cigar;
    m_tags = other->m_tags ? new TTags( *other->m_tags ) : 0;
}

CMappedAlignment * CMappedAlignment::MakeExtended( IAligner * aligner ) const
{
    if( GetCigar().front().GetEvent() != CTrBase::eEvent_SoftMask &&
            GetCigar().back().GetEvent() != CTrBase::eEvent_SoftMask ) return 0;
    CSeqCoding::EStrand strand = IsReverseComplement() ? CSeqCoding::eStrand_neg : CSeqCoding::eStrand_pos;
    TRange qbb( GetQueryBounding() );
    TRange sbb( GetSubjectBounding() );

    aligner->SetSubjectStrand( strand );
    aligner->SetWordDistance( ComputeDistance( GetCigar() ) );
    aligner->SetQuery( m_query->GetSequenceData(), m_query->GetSequenceLength() );
    aligner->SetQueryCoding( m_query->GetSequenceCoding() );
    aligner->SetQueryAnchor( qbb.GetFrom(), qbb.GetTo() );
    aligner->SetSubjectAnchor( sbb.GetFrom(), sbb.GetTo() );
    aligner->SetSubjectGuideTranscript( GetCigar() );
    if( aligner->Align() ) 
        if( ScoreAlignment() <= ScoreAlignment( aligner->GetSubjectTranscript() ) ) // ( aligner->GetSubjectTo() - aligner->GetSubjectFrom() + 1 ) >= sbb.GetLength() ) 
            return new CMappedAlignment( 
                m_flags & fOwnQuery ? new CSRead( *m_query ) : m_query, 
                m_flags & fOwnSubject ? new CContig( *m_subject ) : m_subject, 
                aligner->GetSubjectFrom(), aligner->GetSubjectTranscript(), strand, 
                m_flags, m_tags ? new TTags( *m_tags ) : 0 );
        else 
            cerr << "WARNING: " << m_cigar.ToString() 
                << " scores " << ScoreAlignment() << ", " << aligner->GetSubjectTranscript().ToString() 
                << " scores " << ScoreAlignment( aligner->GetSubjectTranscript() ) << "\n";
    else cerr << "WARNING: alignment failed (distance = " << ComputeDistance( GetCigar() ) << ")\n";

    return 0;
}

void CMappedAlignment::AdjustAlignment( EAdjustMode amode, int fromBy, int toBy )
    // NB: this procedure is well debugged (for case when there are no indels) on Oct 15 2009 for all combinations of cases where fromBy * toBy = 0 
    // Also versions which increase alignment length should work fine with indels (since indels are not touched)
    // Tested all versions, should not throw an exception, but will warn to stderr and overadjust in subj coords alignments like 1M2D10M3D2M
    // TODO: a version of this function may use subject sequence to mark some 'replacement' positions as 'match' where there is actual match
{
    if( s_validate_consistency ) ValidateConsistency( "AdjustAlignment start" );
    enum EFlags {
        fFromGt0 = 0x01,
        fFromLt0 = 0x02,
        fToLess0 = 0x04,
        fToMore0 = 0x08,
        fQryMode = 0x10,
        fSbjMode = 0x00,
        fReverse = 0x20,
        fForward = 0x00,
        fNULL = 0
    };
    //ASSERT(fromBy == 0 || toBy == 0);
    unsigned flags = 0;
    if( fromBy > 0 ) flags |= fFromGt0; else if( fromBy < 0 ) flags |= fFromLt0;
    if( toBy > 0 ) flags |= fToMore0; else if( toBy < 0 ) flags |= fToLess0;
    if( amode == eAdjust_query ) flags |= fQryMode;
    if( IsReverseComplement() ) flags |= fReverse;
    int qlength = GetQuery()->GetSequenceLength();
    switch( flags ) {
        case fSbjMode|fForward:
        case fSbjMode|fReverse:
        case fQryMode|fForward:
        case fQryMode|fReverse: return; // do nothing - no offsets
        case fSbjMode|fForward|fFromLt0:
        case fSbjMode|fReverse|fFromLt0:
        case fQryMode|fForward|fFromLt0:
        case fQryMode|fReverse|fToMore0: 
            do {
                int by = toBy - fromBy; // >0
                switch( flags ) {
                case fSbjMode|fForward|fFromLt0:
                case fQryMode|fForward|fFromLt0: if( by > GetQueryBounding().GetFrom() ) by = GetQueryBounding().GetFrom(); break;
                case fSbjMode|fReverse|fFromLt0:
                case fQryMode|fReverse|fToMore0: if( by > qlength - 1 - GetQueryBounding().GetTo() ) by = qlength - 1 - GetQueryBounding().GetTo(); break;
                }
                if( by == 0 ) break;
                ASSERT( by > 0 );
                TTrSequence result;
                TTrSequence::const_iterator x = m_cigar.begin();
                ASSERT( x != m_cigar.end() );
                if( x->GetEvent() != CTrBase::eEvent_SoftMask ) {
                    cerr << "Warning: attempt to extend alignment with no soft mask, ignoring\n";
                    by = 0;
                } else if( (int)x->GetCount() < by ) {
                    cerr << "Warning: by of " << by << " is too large for CIGAR " << m_cigar.ToString() << ", reducing\n";
                    by = x->GetCount();
                }
                result.AppendItem( CTrBase::eEvent_SoftMask, x->GetCount() - by );
                result.AppendItem( CTrBase::eEvent_Replaced, by );
                for( ++x; x != m_cigar.end(); ++x ) result.AppendItem( *x );
                m_cigar = result;
                m_sstart -= by;
                m_slength += by;
                if( IsReverseComplement() ) by = -by;
                m_qstart -= by;
                m_qlength += by;
            } while(0);
            break;
        case fSbjMode|fForward|fToMore0:
        case fSbjMode|fReverse|fToMore0:
        case fQryMode|fForward|fToMore0:
        case fQryMode|fReverse|fFromLt0:
            do {
                int by = toBy - fromBy; // >0
                switch( flags ) {
                case fSbjMode|fForward|fToMore0:
                case fQryMode|fForward|fToMore0: if( by > qlength - 1 - GetQueryBounding().GetTo() ) by = qlength - 1 - GetQueryBounding().GetTo(); break;
                case fSbjMode|fReverse|fToMore0:
                case fQryMode|fReverse|fFromLt0: if( by > GetQueryBounding().GetFrom() ) by = GetQueryBounding().GetFrom(); break;
                }
                if( by == 0 ) break;
                ASSERT( by > 0 );
                TTrSequence result;
                TTrSequence::const_iterator x = m_cigar.begin();
                ASSERT( x != m_cigar.end() );
                result.AppendItem( *x );
                for( ++x ; x != m_cigar.end(); ++x ) {
                    if( x->GetEvent() == CTrBase::eEvent_SoftMask ) {
                        if( (int)x->GetCount() < by ) {
                            cerr << "Warning: by of " << by << " is too large for CIGAR " << m_cigar.ToString() << ", reducing\n";
                            by = x->GetCount();
                        }
                        result.AppendItem( CTrBase::eEvent_Replaced, by );
                        result.AppendItem( CTrBase::eEvent_SoftMask, x->GetCount() - by );
                    } else result.AppendItem( *x );
                }
                m_cigar = result;
                m_slength += by;
                if( IsReverseComplement() ) by = -by;
                m_qlength += by;
            } while(0);
            break;
        case fSbjMode|fForward|fFromGt0:
        case fSbjMode|fReverse|fFromGt0:
        case fQryMode|fForward|fFromGt0:
        case fQryMode|fReverse|fToLess0:
            do {
                int by = fromBy - toBy; // >0
                ASSERT( by > 0 );
                pair<int,int> ll = m_cigar.ComputeLengths();
                if( ll.first <= by || ll.second <= by ) {
                    cerr << "Warning: too short alignment to clip it by " << by << ": " << m_cigar.ToString() << "\n";
                    return;
                }
                vector<CTrBase::EEvent> result;
                result.reserve( by );
                TTrSequence::const_iterator x = m_cigar.begin();
                ASSERT( x != m_cigar.end() );
                if( x->GetEvent() == CTrBase::eEvent_SoftMask ) {
                    for( int i = 0; i < x->GetCount(); ++i ) 
                        result.push_back( x->GetEvent() );
                    ++x;
                }
                int count = x->GetCount();
                ASSERT( count > 0 );
                int d = IsReverseComplement() ? -1 : 1;
                for( ; by > 0 ; ) {
                    switch( x->GetEvent() ) {
                        case CTrBase::eEvent_Match:
                        case CTrBase::eEvent_Replaced:
                        case CTrBase::eEvent_Changed:
                            --by;
                            result.push_back( CTrBase::eEvent_SoftMask );
                            m_sstart ++;
                            m_slength --;
                            m_qstart += d;
                            m_qlength -= d;
                            break;
                        case CTrBase::eEvent_Insertion:
                            if( amode == eAdjust_query ) --by;
                            result.push_back( CTrBase::eEvent_SoftMask );
                            m_qstart += d;
                            m_qlength -= d;
                            break;
                        case CTrBase::eEvent_Deletion:
                            if( amode == eAdjust_subj ) --by;
                            m_sstart ++;
                            m_slength --;
                            break;
                        case CTrBase::eEvent_Padding:
                            break;
                        default: THROW( logic_error, "Procedure does not support CIGAR like " << m_cigar.ToString() << " in " __FILE__ ":" << __LINE__ );
                    }
                    if( --count == 0 ) count = (++x)->GetCount();
                    if( x == m_cigar.end() ) THROW( logic_error, "Oops... unexpected CIGAR end (forward) " << m_cigar.ToString() );
                }
                switch( x->GetEvent() ) {
                    case CTrBase::eEvent_Insertion:
                        if( amode == eAdjust_subj ) {
                            for( int i = 0; i < count; ++i ) result.push_back( CTrBase::eEvent_SoftMask ); 
                            m_qstart += d * count;
                            m_qlength -= d * count;
                        } else {
                            for( int i = 0; i < count; ++i ) result.push_back( CTrBase::eEvent_Replaced );
                            m_slength += count;
                            m_sstart -= count;
                        }
                        count = 0;
                        break;
                    case CTrBase::eEvent_Deletion:
                        if( amode == eAdjust_subj ) {
                            if( (int)result.size() < count ) {
                                m_sstart += count - result.size();
                                m_slength -= count - result.size();
                                count = result.size();
                                cerr << "WARNING: "__FILE__":" << __LINE__ << " can't adjust precisely strange alignment of " << m_cigar.ToString() << "\n";
                            }
                            //ASSERT( (int)result.size() >= count );
                            for( int i = (int)result.size() - count; i < (int)result.size(); ++i )
                                result[i] = CTrBase::eEvent_Replaced;
                            m_qstart -= d * count;
                            m_qlength += d * count;
                        } else {
                            m_slength -= count;
                            m_sstart += count;
                        }
                        count = 0;
                        break;
                    default:;
                }
                TTrSequence tmp;
                ITERATE( vector<CTrBase::EEvent>, r, result ) tmp.AppendItem( *r );
                while( count-- > 0 ) tmp.AppendItem( x->GetEvent() );
                for( ++x ; x != m_cigar.end(); ++x ) tmp.AppendItem( *x );
                m_cigar = tmp;
            } while(0);
            break;
        case fSbjMode|fForward|fToLess0:
        case fSbjMode|fReverse|fToLess0:
        case fQryMode|fForward|fToLess0:
        case fQryMode|fReverse|fFromGt0:
            do {
                int by = fromBy - toBy; // >0
                ASSERT( by > 0 );
                pair<int,int> ll = m_cigar.ComputeLengths();
                if( ll.first <= by || ll.second <= by ) {
                    cerr << "Warning: too short alignment to clip it by " << by << ": " << m_cigar.ToString() << "\n";
                    return;
                }
                vector<CTrBase::EEvent> result;
                result.reserve( by );
                TTrSequence::reverse_iterator x = m_cigar.rbegin();
                ASSERT( x != m_cigar.rend() );
                if( x->GetEvent() == CTrBase::eEvent_SoftMask ) {
                    for( int i = 0; i < x->GetCount(); ++i ) 
                        result.push_back( x->GetEvent() );
                    ++x;
                }
                int count = x->GetCount();
                ASSERT( count > 0 );
                int d = IsReverseComplement() ? -1 : 1;
                for( ; by > 0 ; ) {
                    switch( x->GetEvent() ) {
                        case CTrBase::eEvent_Match:
                        case CTrBase::eEvent_Replaced:
                        case CTrBase::eEvent_Changed:
                            --by;
                            result.push_back( CTrBase::eEvent_SoftMask );
                            m_slength --;
                            m_qlength -= d;
                            break;
                        case CTrBase::eEvent_Insertion:
                            if( amode == eAdjust_query ) --by;
                            result.push_back( CTrBase::eEvent_SoftMask );
                            m_qlength -= d;
                            break;
                        case CTrBase::eEvent_Deletion:
                            if( amode == eAdjust_subj ) --by;
                            m_slength --;
                            break;
                        case CTrBase::eEvent_Padding:
                            break;
                        default: THROW( logic_error, "Procedure does not support CIGAR like " << m_cigar.ToString() << " in " __FILE__ ":" << __LINE__ );
                    }
                    if( --count == 0 ) count = (++x)->GetCount();
                    ASSERT( x != m_cigar.rend() );
                }
                switch( x->GetEvent() ) {
                    case CTrBase::eEvent_Insertion:
                        if( amode == eAdjust_subj ) {
                            for( int i = 0; i < count; ++i ) result.push_back( CTrBase::eEvent_SoftMask ); 
                            m_qlength -= d * count;
                        } else {
                            for( int i = 0; i < count; ++i ) result.push_back( CTrBase::eEvent_Replaced );
                            m_slength += count;
                        }
                        count = 0;
                        break;
                    case CTrBase::eEvent_Deletion:
                        if( amode == eAdjust_subj ) {
                            if( (int)result.size() < count ) {
                                m_slength -= count - result.size();
                                count = result.size();
                                cerr << "WARNING: "__FILE__":" << __LINE__ << " can't adjust precisely strange alignment of " << m_cigar.ToString() << "\n";
                            }
                            // ASSERT( (int)result.size() >= count );
                            for( int i = (int)result.size() - count; i < (int)result.size(); ++i )
                                result[i] = CTrBase::eEvent_Replaced;
                            m_qlength += d * count;
                        } else {
                            m_slength -= count;
                        }
                        count = 0;
                        break;
                    default:;
                }
                ASSERT( x != m_cigar.rend() );
                TTrSequence tmp;
                ITERATE( vector<CTrBase::EEvent>, r, result ) tmp.AppendItem( *r );
                while( count-- > 0 ) tmp.AppendItem( x->GetEvent() );
                for( ++x; x != m_cigar.rend(); ++x ) tmp.AppendItem( *x );
                m_cigar.clear();
                for( x = tmp.rbegin(); x != tmp.rend(); ++x ) m_cigar.AppendItem( *x );
            } while(0);
            break;
        default:
            THROW( logic_error, "flags " << hex << flags << dec << "h can not be processed in " __FILE__ ":" << __LINE__ );
    }
    if( s_validate_consistency ) {
        ostringstream o;
        o << hex << flags;
        ValidateConsistency( "AdjustAlignment end " + o.str() );
    }
}

char CMappedAlignment::GetTagType( const string& name ) const
{
    if( m_tags == 0 ) return 0;
    TTags::const_iterator i = m_tags->find( name );
    if( i == m_tags->end() ) return 0;
    return i->second[0];
}

void CMappedAlignment::SetTagS( const string& name, const string& value ) 
{
    if( value.length() ) {
        if( m_tags == 0 ) m_tags = new TTags();
        (*m_tags)[name] = "Z:" + value;
    } else RemoveTag( name );
}

void CMappedAlignment::RemoveTag( const string& name )
{
    if( m_tags ) {
        m_tags->erase( name );
        if( m_tags->size() == 0 ) SetTags( 0 );
    }
}

void CMappedAlignment::SetTagI( const string& name, int value )
{
    if( m_tags == 0 ) m_tags = new TTags();
    (*m_tags)[name] = "i:" + NStr::IntToString( value );
}

void CMappedAlignment::SetTagF( const string& name, float value )
{
    if( m_tags == 0 ) m_tags = new TTags();
    (*m_tags)[name] = "f:" + NStr::DoubleToString( value );
}

string CMappedAlignment::GetTagS( const string& name, char type ) const
{
    if( m_tags == 0 ) THROW( runtime_error, "No tag " << name << " is found" );
    TTags::const_iterator i = m_tags->find( name );
    if( i == m_tags->end() ) THROW( runtime_error, "No tag " << name << " is found" );
    if( i->second[1] != ':' ) THROW( runtime_error, "Bad tag format for " << name << ":" << i->second );
    if( i->second[0] != type ) THROW( runtime_error, "Bad tag type: " << i->second[0] << " while " << type << " is expected" );
    return i->second.substr(2);
}

int CMappedAlignment::GetTagI( const string& name ) const
{
    return NStr::StringToInt( GetTagS( name, 'i' ) );
}

float CMappedAlignment::GetTagF( const string& name ) const
{
    return NStr::StringToDouble( GetTagS( name, 'f' ) );
}

