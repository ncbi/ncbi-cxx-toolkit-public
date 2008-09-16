#include <ncbi_pch.hpp>
#include "coutputformatter.hpp"
#include "cguidefile.hpp"
#include "ialigner.hpp"
#include "chit.hpp"

USING_OLIGOFAR_SCOPES;

class CNcbi8naPrinter
{
public:
	CNcbi8naPrinter( const string& s ) : m_data(s) {}
	const char * data() const { return m_data.c_str(); }
	unsigned size() const { return m_data.size(); }
protected:
	const string& m_data;
};

inline ostream& operator << ( ostream& o, const CNcbi8naPrinter& p ) 
{
	if( p.size() == 0 ) o.put('-'); 
	else for( unsigned i = 0; i < p.size(); ++i ) {
		o.put( Ncbi4naToIupac( p.data()[i] )[0] );
	}
	return o;
}

class CNcbiqnaPrinter
{
public:
	CNcbiqnaPrinter( const string& s ) : m_data(s) {}
	const char * data() const { return m_data.c_str(); }
	unsigned size() const { return m_data.size(); }
protected:
	const string& m_data;
};

inline ostream& operator << ( ostream& o, const CNcbiqnaPrinter& p ) 
{
	if( p.size() == 0 ) o.put('-'); 
	else for( unsigned i = 0; i < p.size(); ++i ) {
		o.put( CIupacnaBase( CNcbiqnaBase( p.data()[i] ) ) );
	}
	return o;
}

class CNcbipnaPrinter
{
public:
	CNcbipnaPrinter( const char * data, unsigned len ) : m_data( data ), m_len( len ) {}
	const char * data() const { return m_data; }
	unsigned size() const { return m_len; }
protected:
	const char * m_data;
	unsigned m_len;
};

inline ostream& operator << ( ostream& o, const CNcbipnaPrinter& p ) 
{
	if( p.size() == 0 ) o.put('-'); 
	else for( unsigned i = 0; i < p.size(); ++i ) {
		o.put( CIupacnaBase( CNcbipnaBase( p.data() + i*5 ) ) );
	}
	return o;
}

string COutputFormatter::GetSubjectId( int ord ) const 
{
	return m_seqIds.GetSeqDef( ord ).GetBestIdString();
}

void COutputFormatter::FormatCore( const CHit * hit ) const
{
	m_out 
        << hit->GetQuery()->GetId() << "\t"
        << GetSubjectId( hit->GetSeqOrd() ) << "\t" 
        << hit->GetComponents() << "\t"
        << hit->GetTotalScore() << "\t";
	if( hit->GetComponents() & 1 ) {
		m_out 
	        << hit->GetFrom(0) + 1 << "\t"
            << hit->GetTo(0) + 1 << "\t"
            << hit->GetScore(0) << "\t";
	} else {
		m_out << "-\t-\t-\t";
	}
	if( hit->GetComponents() & 2 ) {
		m_out 
	        << hit->GetFrom(1) + 1 << "\t"
            << hit->GetTo(1) + 1 << "\t"
            << hit->GetScore(1) << "\t";
	} else {
		m_out << "-\t-\t-\t";
	}
    m_out 
        << hit->GetStrand() << "\t"
        << CNcbi8naPrinter( hit->GetTarget(0) ) << "\t"
        << CNcbi8naPrinter( hit->GetTarget(1) ) << "\n";
}

void COutputFormatter::operator () ( const CQuery * query )
{
	if( query == 0 ) return;
    if( CHit * hit = query->GetTopHit() ) {
        unsigned rank = 0;
        for( ; hit; ++rank ) {
            if( hit->IsNull() ) { 
                if( m_flags & fReportTerminator ) 
                    m_out << (rank) << "\tno_more\t" << query->GetId() << "\t-\t0\t0\t-\t-\t0\t-\t-\t0\t*\t-\t-\n";
                m_out << QuerySeparator(); 
                delete query; 
                return; 
            }
            unsigned rcnt = 1;
            CHit * next = hit->GetNextHit();
            for( ; next && (!next->IsNull()) && next->GetTotalScore() == hit->GetTotalScore(); next = next->GetNextHit(), ++rcnt );
            string srcnt, snextScore;
            if( next || !(m_flags & fReportMany)) {
                srcnt = NStr::IntToString( rcnt );
                snextScore = next ? NStr::DoubleToString( next->GetTotalScore() ) : "any";
            } else {
                srcnt = "many";
                snextScore = "any";
            }
            for( ; hit != next; hit = hit->GetNextHit() ) {
                ASSERT( hit->IsNull() == false );
                m_out
                    << rank << "\t"
                    << srcnt << "\t";
				FormatCore( hit );
				if( m_flags & (fReportDifferences|fReportAlignment) ) {
					FormatDifferences( rank, hit, 0, m_flags );
					if( query->IsPairedRead() ) FormatDifferences( rank, hit, 1, m_flags );
				}
            }
            
        }
        if( m_flags & fReportMore ) 
            m_out << (rank-1) << "\tmore\t" << query->GetId() << "\t*\t*\t*\t*\t*\t*\t*\t*\t*\t*\t*\t*\n";
        m_out << QuerySeparator();
    } else if( m_flags & fReportUnmapped ) {
        m_out << "0\tnone\t"<< query->GetId() << "\t-\t0\t0\t-\t-\t0\t-\t-\t0\t*\t";
        
        if( query->GetCoding() == CSeqCoding::eCoding_ncbi8na ) {
            m_out << CNcbi8naPrinter( string(query->GetData(0), query->GetLength(0)) ) << "\t";
            if( query->HasComponent(1) ) 
                m_out << CNcbi8naPrinter( string(query->GetData(1), query->GetLength(1)) ) << "\n";
            else m_out << "-\n";
		} else if( query->GetCoding() == CSeqCoding::eCoding_ncbipna ) {
            m_out << CNcbipnaPrinter( query->GetData(0), query->GetLength(0) ) << "\t";
            if( query->HasComponent(1) ) 
                m_out << CNcbipnaPrinter( query->GetData(1), query->GetLength(1) ) << "\n";
            else m_out << "-\n";
		} else if( query->GetCoding() == CSeqCoding::eCoding_ncbiqna ) {
            m_out << CNcbiqnaPrinter( string(query->GetData(0), query->GetLength(0)) ) << "\t";
            if( query->HasComponent(1) ) 
                m_out << CNcbiqnaPrinter( string(query->GetData(1), query->GetLength(1)) ) << "\n";
            else m_out << "-\n";
        } else {
            // TODO: implement printer for Ncbipna
            m_out << "-\t-\n";
        }
        m_out << QuerySeparator();
    }
    delete query;
}

void COutputFormatter::FormatHit( const CHit * hit ) const
{
	if( ! hit ) return;
	m_out 
		<< "*\t" 
		<< "hit\t";
	FormatCore( hit );
	if( m_flags & fReportAlignment ) {
		FormatDifferences( -1, hit, 0, fReportAlignment );
		FormatDifferences( -1, hit, 1, fReportAlignment );
	}
}

void COutputFormatter::FormatDifferences( int rank, const CHit * hit, int matepair, int flags ) const
{
	if( hit->HasComponent( matepair ) == false ) return;
//	if( hit->GetScore( matepair ) == 100 ) return;
    ASSERT( m_aligner );
    const CQuery * query = hit->GetQuery();
    const string& target = hit->GetTarget( matepair );
    const char * s = target.data();
    int length = target.length();
    const int aflags = CAlignerBase::fComputePicture | CAlignerBase::fComputeScore | CAlignerBase::fPictureSubjectStrand;
	m_aligner->SetBestPossibleQueryScore( query->GetBestScore( matepair ) );
    if( hit->IsRevCompl( matepair ) == false ) {
        m_aligner->Align( query->GetCoding(),
                          query->GetData( matepair ),
                          query->GetLength( matepair ),
                          CSeqCoding::eCoding_ncbi8na,
                          s, length, aflags );
                          
    } else {
        m_aligner->Align( query->GetCoding(),
                          query->GetData( matepair ),
                          query->GetLength( matepair ),
                          CSeqCoding::eCoding_ncbi8na,
                          s + length - 1, 0 - length, aflags );
    }

    const CAlignerBase& abase = m_aligner->GetAlignerBase();
    
 	if( flags & fReportAlignment ) {
 		m_out 
 			<< "# " << ( hit->IsRevCompl( matepair )?3:5 ) << "'=" << abase.GetQueryString() << "=" << ( hit->IsRevCompl( matepair )?5:3 ) << "' query[" << (matepair + 1) << "]\n"
 			<< "#    " << abase.GetAlignmentString() << "    i:" << abase.GetIdentityCount() << ", m:" << abase.GetMismatchCount() << ", g:" << abase.GetIndelCount() << "\n"
 			<< "# 5'=" << abase.GetSubjectString() << "=3' subject\n"; 
 	}
 	if( flags & fReportDifferences ) {
 		int b = 0; 
 		int e = abase.GetQueryString().length();
 		int m = -1;
 		for( int i = b ; i < e ; ++i ) {
 			if( abase.GetQueryString()[i] == abase.GetSubjectString()[i] ) {
 				if( m >= 0 ) {
 					FormatDifference( rank, hit, matepair, abase.GetQueryString(), abase.GetSubjectString(), m, i - 1 );
 					m = -1;
 				} 
 			} else {
 				if( m == -1 ) m = i;
 			}
 		}
 		if( m >= 0 ) FormatDifference( rank, hit, matepair, abase.GetQueryString(), abase.GetSubjectString(), m, e - 1 );
 	}
}
	
void COutputFormatter::FormatDifference( int rank, const CHit * hit, int matepair, const string& queryAlignment, const string& subjAlignment, int from, int to ) const
{
	m_out 
		<< rank << "\tdiff\t" 
		<< hit->GetQuery()->GetId() << "\t" 
		<< GetSubjectId( hit->GetSeqOrd() ) << "\t" 
		<< hit->GetComponents() << "\t" 
		<< hit->GetTotalScore() << "\t";

	if( matepair != 0 ) m_out << "-\t-\t-\t";

	int pos1 = min( hit->GetFrom( matepair ), hit->GetTo( matepair ) );
	for( int i = 0; i < from; ++i )
		if( subjAlignment[i] != '-' ) pos1++;
	int pos2 = pos1;
	for( int i = from; i <= to; ++i )
		if( subjAlignment[i] != '-' ) pos2++;
	
	m_out << ( pos1 + 1 ) << "\t";
	if( pos2 > pos1 ) m_out << pos2 << "\t";
	else m_out << "-\t";

	m_out << hit->GetScore( matepair ) << "\t";

	if( matepair == 0 ) m_out << "-\t-\t-\t";
	
	m_out << hit->GetStrand() << "\t";

	if( matepair != 0 ) m_out << "-\t";

	m_out << subjAlignment.substr( from, to - from + 1 ) << "=>" << queryAlignment.substr( from, to - from + 1 );

	if( matepair == 0 ) m_out << "\t-\n";
	else m_out << "\n";
}

