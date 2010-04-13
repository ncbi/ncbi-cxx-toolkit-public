#ifndef OLIGOFAR_CSEQVECPROCESSOR__HPP
#define OLIGOFAR_CSEQVECPROCESSOR__HPP

#include "defs.hpp"
#include "cseqbuffer.hpp"

#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/readers/fasta.hpp>
//#include <objtools/readers/seqdb/seqdb.hpp>
///* TODO: replace it in newer version
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
//*/
#include <objtools/readers/reader_exception.hpp>

BEGIN_OLIGOFAR_SCOPES

class CSeqVecProcessor
{
public:
    typedef CBioseq::TId TSeqIds;
    class ICallback 
    {
    public:
        typedef CSeqVecProcessor::TSeqIds TSeqIds;
        virtual ~ICallback() {}
        virtual void SequenceBegin( const TSeqIds& seqIds, int oid ) = 0;
        virtual void SequenceBuffer( CSeqBuffer* iupacna ) = 0;
        virtual void SequenceEnd() = 0;
    };
    
    CSeqVecProcessor() : m_tgtCoding( CSeqCoding::eCoding_ncbi8na ), m_fastaReaderFlags(0), m_parseTitleForIds( false ) {}
    
    virtual ~CSeqVecProcessor() {}
    template<class Iterator> 
    Iterator ProcessList( Iterator begin, Iterator end );

    void Process( const char* name );
    void Process( const string& name );
    void Process( const objects::CBioseq& bioseq, int oid = -1, CSeqDBGiList* list = 0 );
    void Process( const objects::CSeq_entry& entry, int oid = -1, CSeqDBGiList* list = 0 );
    void Process( const objects::CBioseq_set& bioseqs, int oid = -1, CSeqDBGiList* list = 0 );
    void Process( CSeqDB& seqdb );
    void Process( CFastaReader& reader, CSeqDBGiList* list = 0 );
    void Process_SeqDB( const string& filename );
    void Process_fasta( const string& filename );

    virtual void Process( const TSeqIds&, const CSeqVector&, int oid = -1 );
    virtual CSeqDBGiList * CreateGiList();

    void SetGiListFile( const string& gilistfile );
    template<class container>
    void SetSeqIdList( const container& c ) { copy( c.begin(), c.end(), inserter( m_seqid, m_seqid.end() ) ); }
    void CleanSeqIdList() { m_seqid.clear(); }
    // call order 
    // - SequenceBegin (from higher to lower)
    // - SequenceBuffer (from higher to lower)
    // - SequenceEnd (from lower to higher)
    void AddCallback( int priority, ICallback * cbk ); 
    void SetFastaReaderFlags( unsigned flags ) { m_fastaReaderFlags = flags; }
    void SetFastaReaderFlags( unsigned flags, bool on ) { if( on ) m_fastaReaderFlags |= flags; else m_fastaReaderFlags &= ~flags; }
    void SetFastaReaderParseId( bool on ) { SetFastaReaderFlags( CFastaReader::fNoParseID, !on ); }

    CSeqCoding::ECoding GetTargetCoding() const { return m_tgtCoding; }
    void SetTargetCoding( CSeqCoding::ECoding c ) { m_tgtCoding = c; }
protected:
//    ICallback * m_callBack;
    typedef multimap<int,ICallback*> TCallbacks;
    TCallbacks m_callbacks;
    string m_giListFile;
	int    m_oid;
    CSeqCoding::ECoding m_tgtCoding;
    set<string> m_seqid;
    unsigned m_fastaReaderFlags;
    bool m_parseTitleForIds;
};

////////////////////////////////////////////////////////////////////////
// Implementation

template<class Iterator>
inline Iterator CSeqVecProcessor::ProcessList( Iterator begin, Iterator end )
{
    while( begin != end ) Process( *begin);
    return begin;
}

inline void CSeqVecProcessor::Process( const char* name )
{
    Process( string( name ) );
}

inline void CSeqVecProcessor::Process( const string& name )
{
    if( CFile(name + ".nin").Exists() || 
		CFile(name + ".nal").Exists() ) Process_SeqDB( name );
    else Process_fasta( name );
}

inline void CSeqVecProcessor::Process_SeqDB( const string& name )
{
    auto_ptr<CSeqDBGiList> giList( CreateGiList() );
    CSeqDB seqDb( name, CSeqDB::eNucleotide, giList.release() );
    Process( seqDb );
}

inline void CSeqVecProcessor::Process_fasta( const string& name )
{
    CFastaReader reader( name, m_fastaReaderFlags );
    auto_ptr<CSeqDBGiList> giList( CreateGiList() );
    bool save = m_parseTitleForIds;
    if( m_fastaReaderFlags & CFastaReader::fNoParseID ) m_parseTitleForIds = true;
    Process( reader, giList.get() );
    m_parseTitleForIds = save;
}

inline CSeqDBGiList * CSeqVecProcessor::CreateGiList() 
{
    return ( m_giListFile.length() ? new CSeqDBFileGiList( m_giListFile ) : 0 );
}

inline void CSeqVecProcessor::Process( CSeqDB& seqdb )
{
    for( CSeqDBIter i = seqdb.Begin(); i; ++i ) {
        int oid = i.GetOID();
        CRef<CBioseq> bioseq = seqdb.GetBioseq( oid );
        Process( *bioseq, oid );
    }
}

inline void CSeqVecProcessor::Process( CFastaReader& reader, CSeqDBGiList * gilist )
{
    try {
		int oid = -1;
        while( CRef<CSeq_entry> seq = reader.ReadOneSeq() ) Process( *seq, ++oid, gilist );
    }
    catch( const CObjReaderParseException& e ) {
        if( e.GetErrCode() != CObjReaderParseException::eEOF ) throw;
    }
}

inline void CSeqVecProcessor::Process( const CBioseq& bioseq, int oid, CSeqDBGiList * gilist ) 
{
    const TSeqIds& ids( bioseq.GetId() );
    if( gilist ) {
        bool found = false;
        for( TSeqIds::const_iterator i = ids.begin(); i != ids.end(); ++i ) {
            if( gilist->FindSeqId(**i) ) { 
				found = true; 
				if( oid == -1 ) {
					if( !gilist->SeqIdToOid( **i, oid ) ) oid = -1;
				}
				break; 
			}
		}
        if( !found ) return;
    } else if( m_seqid.size() ) {
        bool found = false;
        for( TSeqIds::const_iterator i = ids.begin(); i != ids.end(); ++i ) {
            if( m_seqid.find((*i)->AsFastaString()) != m_seqid.end() ) { 
				found = true; 
				break; 
			}
		}
        if( !found ) return;
    }
    const CBioseq_Handle::EVectorCoding coding = CBioseq_Handle::eCoding_Iupac;
    CSeqVector vec( bioseq, 0, coding, eNa_strand_plus );
    if( m_parseTitleForIds || ids.size() == 0 ) {
        TSeqIds newIds;
        if( bioseq.IsSetDescr() == false || 
            bioseq.GetDescr().Get().size() == 0 )
            THROW( runtime_error, "Failed to get sequence IDS for oid=" << oid );
        ITERATE( CBioseq::TDescr::Tdata, d, bioseq.GetDescr().Get() ) {
            if( (*d)->IsTitle() ) {
                const char * x = bioseq.GetDescr().Get().front()->GetTitle().c_str();
                while( isspace( *x ) ) ++x;
                const char * y = x;
                while( *y > ' ' ) ++y;
                newIds.push_back( CRef<CSeq_id>( new CSeq_id( CSeq_id::e_Local, string( x, y ) ) ) );
                break;
            }
        }
        Process( newIds, vec, oid );
    } else {
        Process( ids, vec, oid );
    }
}

inline void CSeqVecProcessor::Process( const CSeq_entry& entry, int oid, CSeqDBGiList * gilist )
{
    if( entry.IsSeq() )
        Process( entry.GetSeq(), oid, gilist );
    else Process( entry.GetSet(), oid, gilist );
}

inline void CSeqVecProcessor::Process( const CBioseq_set& bioseqs, int oid, CSeqDBGiList * gilist )
{
    if( bioseqs.CanGetSeq_set() ) {
        const CBioseq_set::TSeq_set& sset = bioseqs.GetSeq_set();
        for( CBioseq_set::TSeq_set::const_iterator i = sset.begin(); i != sset.end(); ++i ) {
            Process( **i, oid, gilist );
        }
    }
}

inline void CSeqVecProcessor::SetGiListFile( const string& file )
{
    m_giListFile = file;
}

inline void CSeqVecProcessor::AddCallback( int prio, ICallback * cbk ) 
{
    if( cbk ) m_callbacks.insert( make_pair( prio, cbk ) );
}

inline void CSeqVecProcessor::Process( const TSeqIds& seqIds, const CSeqVector& vect, int oid )
{
    CSeqBuffer buffer( vect, m_tgtCoding );
    for( TCallbacks::reverse_iterator c = m_callbacks.rbegin(); c != m_callbacks.rend(); ++c ) {
        c->second->SequenceBegin( seqIds, oid );
    }
    for( TCallbacks::reverse_iterator c = m_callbacks.rbegin(); c != m_callbacks.rend(); ++c ) {
        c->second->SequenceBuffer( &buffer );
    }
    for( TCallbacks::const_iterator c = m_callbacks.begin(); c != m_callbacks.end(); ++c ) {
        c->second->SequenceEnd();
    }
}

END_OLIGOFAR_SCOPES

#endif
