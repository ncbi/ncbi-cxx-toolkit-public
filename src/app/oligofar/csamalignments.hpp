#ifndef OLIGOFAR_CSAMALIGNMENTS__HPP
#define OLIGOFAR_CSAMALIGNMENTS__HPP

#include "ialignmentmap.hpp"
#include "cseqcoding.hpp"
#include "csam.hpp"

BEGIN_OLIGOFAR_SCOPES

class CSRead : public IShortRead
{
public:
    ~CSRead() { delete[] m_data; --s_count; }

    virtual EPairType GetPairType() const { return x_GetPairType(); }
    virtual CSeqCoding::ECoding GetSequenceCoding() const { return x_GetSequenceCoding(); }
    virtual int GetSequenceLength() const { return x_GetSequenceLength(); }
    const IShortRead * GetMate() const { return x_GetPairType() ? x_GetMate() : 0; }
    const char * GetSequenceData() const { return m_data + x_GetSequenceOffset(); }
    const char * GetId() const { return x_GetPairType() == eRead_second ? x_GetMate()->GetId() : m_data + x_GetIdOffset(); }

    CSRead( const string& id, CSeqCoding::ECoding coding, const char * sequence, int length, EPairType intended = eRead_single );
    CSRead( const string& id, const string& sequence, const string& quality = "", EPairType intended = eRead_single );
    CSRead( const CSRead& r );
    CSRead& operator = ( const CSRead& r );

    void SetMate( CSRead * other ); // other becomes second sequence

    EPairType GetIntendedPairType() const { return EPairType((m_data[2] >> 2)&3); }

    string GetIupacna( CSeqCoding::EStrand = CSeqCoding::eStrand_pos ) const;

    static int GetCount() { return s_count; }

protected:
    Uint2 x_GetIdLength() const { return Uint1( m_data[0] ); }
    Uint2 x_GetSequenceLength() const { return Uint1( m_data[1] ); }
    EPairType x_GetPairType() const { return EPairType( m_data[2] & 3 ); }
    CSeqCoding::ECoding x_GetSequenceCoding() const { return CSeqCoding::ECoding( Uint1( m_data[3] ) ); }

    Uint1 x_GetHeaderSize() const { return 4 + ( x_GetPairType() ? sizeof( void* ) : 0 ); }
    Uint2 x_GetIdOffset() const { return x_GetHeaderSize(); }
    Uint2 x_GetSequenceOffset() const { return x_GetIdOffset() + x_GetIdLength() + 1; }
    Uint2 x_GetDataSize() const { return x_GetSequenceOffset() + x_GetSequenceLength() + 1; }
    Uint2 x_GetDataSize( int idlen, int seqlen, EPairType type ) const {
        switch( type ) {
            case eRead_single: return 6 + idlen + seqlen;
            case eRead_first: return 6 + sizeof( void * ) + idlen + seqlen;
            case eRead_second: return 5 + sizeof( void * ) + seqlen;
        }
        THROW( logic_error, "Unknown read pair type" );
    }
    const CSRead * x_GetMate() const { return *(CSRead**)(m_data + 4); }
    CSRead * & x_SetMate() { return *(CSRead**)(m_data + 4); }
protected:
    char * m_data;
    static int s_count;
};

class CContig : public INucSeq
{
public:
    ~CContig() { if( m_owns ) delete[] m_data; --s_count; }
    CContig( const string& name );
    CContig( const CContig& ctg );
    void SetSequenceData( const char * data, int length, int offset = 0, CSeqCoding::ECoding coding = CSeqCoding::eCoding_ncbi8na, bool owns = false ) { 
        if( m_owns ) delete[] m_data;
        m_data = const_cast<char*>( data );  // if we own it - we can do anything, otherwise we don't touch
        m_size = length; m_coding = coding; m_owns = owns; 
        m_offset = offset;
    }
    CRange<int> GetAvailableRange() const { return CRange<int>( m_offset, m_size - 1 ); }
    virtual const char * GetId() const { return m_id.c_str(); }
    virtual const char * GetSequenceData() const { return m_data - m_offset; } //&m_data[0]; }
    virtual int GetSequenceLength() const { return m_size; } // m_data.size(); }
    virtual int GetSequenceOffset() const { return m_offset; }
    virtual CSeqCoding::ECoding GetSequenceCoding() const { return m_coding; } //CSeqCoding::eCoding_ncbi8na; }

    static int GetCount() { return s_count; }
private:
    CContig& operator = ( const CContig& );
protected:
    string m_id;
    char * m_data;
    int m_size;
    int m_offset;
    CSeqCoding::ECoding m_coding;
    bool m_owns;

    static int s_count;
};

class IAligner;
class CMappedAlignment : public IMappedAlignment
{
public:
    typedef map<string,string> TTags;
    enum EFlags { fOwnQuery = 0x01, fOwnSubject = 0x02, fOwnSeqs = fOwnQuery|fOwnSubject };
    enum ETagType { eType_int = 'i', eType_float = 'f', eType_string = 'Z', eType_NONE = 0 };
    virtual const char * GetId() const { return 0; }
    virtual const IShortRead * GetQuery() const { return m_query; }
    virtual const INucSeq * GetSubject() const { return m_subject; }
    virtual TRange GetSubjectBounding() const { return TRange( m_sstart, m_sstart + m_slength - 1 ); }
    virtual TRange GetQueryBounding() const { return m_qlength > 0 ? TRange( m_qstart, m_qstart + m_qlength - 1 ) : TRange( m_qstart + m_qlength + 1, m_qstart ); }
    virtual const TTrSequence GetCigar() const { return m_cigar; }
    virtual bool IsReverseComplement() const { return m_qlength < 0; }
    virtual const IMappedAlignment * GetMate() const { return m_mate; }
    CSRead * SetQuery() { return m_query; }
    CContig * SetSubject() { return m_subject; }
    CSeqCoding::EStrand GetStrand() const { return IsReverseComplement() ? CSeqCoding::eStrand_neg : CSeqCoding::eStrand_pos; }
    void SetMate( CMappedAlignment * other );
    void WriteAsSam( ostream& out ) const;
    void SetFlags( int flags, bool on = true ) { if( on ) m_flags |= flags; else m_flags &= ~flags; }
    void AdjustQueryFromBy( int );
    void AdjustQueryToBy( int  );
    void AdjustSubjectFromBy( int  );
    void AdjustSubjectToBy( int  );
    void AdjustQueryFromTo( int );
    void AdjustQueryToTo( int  );
    void AdjustSubjectFromTo( int  );
    void AdjustSubjectToTo( int  );
    void SetTagS( const string& name, const string& val );
    void SetTagI( const string& name, int val );
    void SetTagF( const string& name, float val );
    char GetTagType( const string& name ) const;
    string GetTagS( const string& name, char type = 'Z' ) const;
    int GetTagI( const string& name ) const;
    float GetTagF( const string& name ) const;
    void RemoveTag( const string& name );
    void Assign( const CMappedAlignment * other );
    void SetTags( TTags * tags ) { delete m_tags; m_tags = tags; }
    const TTags& GetTags() const { static TTags notags; if( m_tags ) return *m_tags; else return notags; }
    CMappedAlignment * MakeExtended( IAligner * aligner ) const;
    CMappedAlignment * Clone() const { return new CMappedAlignment( *this ); }
    CMappedAlignment( CSRead * query, CContig * subject, int from, const TTrVector& cigar, CSeqCoding::EStrand strand, int flags = 0, TTags * tags = 0 );
    ~CMappedAlignment() { if( m_flags & fOwnQuery ) delete m_query; if( m_flags & fOwnSubject ) delete m_subject; delete m_tags; --s_count; }
    void PrintDebug( ostream& out ) const {
        out << m_cigar.ToString() << DISPLAY( m_sstart ) << DISPLAY( m_slength ) << DISPLAY( m_qstart ) << DISPLAY( m_qlength ) << hex << DISPLAY( m_flags ) << dec;
    }
    bool ValidateConsistency( const string& context = "" ) const;
    bool operator == ( const CMappedAlignment& other ) const { 
        return 
            strcmp( m_query->GetId(), other.m_query->GetId() ) == 0 &&
            strcmp( m_subject->GetId(), other.m_subject->GetId() ) == 0 &&
            m_sstart == other.m_sstart && m_slength == other.m_slength &&
            m_qstart == other.m_qstart && m_qlength == other.m_qlength &&
            m_cigar == other.m_cigar;
    }
    bool operator != ( const CMappedAlignment& other ) const { return !operator == ( other ); }

    static int GetCount() { return s_count; }

    double ScoreAlignment( double id = 1, double mm = -1, double go = -3, double ge = -1.5 ) const;
    static double ScoreAlignment( const TTrSequence& cigar, double id = 1, double mm = -1, double go = -3, double ge = -1.5 );

private:
    CMappedAlignment& operator = ( const CMappedAlignment& );
    enum EAdjustMode { eAdjust_query, eAdjust_subj };
    void AdjustAlignment( EAdjustMode, int fromBy, int toBy  );
protected:
    //int x_AdjustCigar( int advanceFrontBy, EAdjustMode );
    CMappedAlignment( const CMappedAlignment& a );
protected:
    CSRead * m_query;
    CContig * m_subject;
    const CMappedAlignment * m_mate;
    Int4 m_sstart;
    Int2 m_slength;
    Int2 m_qstart;
    Int2 m_qlength;
    Int2 m_flags;
    TTrSequence m_cigar;
    TTags * m_tags;
    static int s_count;
public:
    static bool s_validate_consistency;
};

class CSamSource : public IAlignmentMap, public CSamBase
{
public:
    typedef vector<CMappedAlignment*> TAlignmentList;
    typedef vector<Uint8> TFileOffsets;
    typedef map<string,CSRead*> TReads;
    typedef map<string,CContig*> TContigs;
    typedef map<string,TAlignmentList> TAlignmentMap;
    typedef map<string,TFileOffsets> TAlignmentOffsetMap;
    typedef map<pair<string, pair<string,int> >, CMappedAlignment* > TPendingHits;

    enum ERegisterFlags { fRegisterQuery = 0x01, fRegisterSubject = 0x02, fRegisterAlignmentByQuery = 0x04, fRegisterAlignmentBySubject = 0x08 };

    ~CSamSource();
    CMappedAlignment * ParseSamLine( const vector<string>& samLine, int flags = 0 );
    CMappedAlignment * ParseSamLine( const string& samLine, int flags = 0 );

    void IndexFile( const string& samFile, unsigned start, unsigned limit );
    
    static TTrVector GetFullCigar( const TTrVector& cigar, const string& mismatches );

    virtual void GetAlignmentsForQueryId( ICallback *, const char * id, int mates = 3 ) {}
    virtual void GetAlignmentsForSubjectId( ICallback *, const char * id, int strands = 3, int from = kSequenceBegin, int to = kSequenceEnd ) {}
    virtual void GetAllAlignments( ICallback * ) {}
    virtual void GetAllQueries( ICallback * ) {}
    virtual void GetAllSubjects( ICallback * ) {}

    virtual void AddAlignment( IMappedAlignment * ma ) {}
    virtual void AddSubject( INucSeq * ma ) {}
    virtual void AddQuery( IShortRead * ma ) {}

    const TAlignmentOffsetMap& GetAlignmentOffsetMap() const { return m_alignmentOffsets; }
    const TFileOffsets& GetAlignmentOffsets( const string& ctg ) const { 
        TAlignmentOffsetMap::const_iterator x = m_alignmentOffsets.find( ctg ); 
        if( x == m_alignmentOffsets.end() ) {
            static TFileOffsets null;
            return null;
        }
        else return x->second;
    }
protected:
    TContigs m_contigs;
    TReads m_reads;
    TAlignmentMap m_alignmentsByContig;
    TAlignmentMap m_alignmentsByRead;
    TAlignmentOffsetMap m_alignmentOffsets;
    TPendingHits m_pendingHits;
    auto_ptr<ifstream> m_samFile;
};

inline void CMappedAlignment::AdjustQueryFromBy( int by ) 
{
    AdjustAlignment( eAdjust_query, by, 0 );
}

inline void CMappedAlignment::AdjustQueryToBy( int by ) 
{
    AdjustAlignment( eAdjust_query, 0, by );
}

inline void CMappedAlignment::AdjustSubjectFromBy( int by ) 
{
    AdjustAlignment( eAdjust_subj, by, 0 );
}

inline void CMappedAlignment::AdjustSubjectToBy( int by ) 
{
    AdjustAlignment( eAdjust_subj, 0, by );
}

inline void CMappedAlignment::AdjustQueryFromTo( int newFrom ) 
{
    AdjustQueryFromBy( newFrom - GetQueryBounding().GetFrom() );
}

inline void CMappedAlignment::AdjustQueryToTo( int newTo ) 
{
    AdjustQueryToBy( newTo - GetQueryBounding().GetTo() );
}

inline void CMappedAlignment::AdjustSubjectFromTo( int newFrom ) 
{
    AdjustSubjectFromBy( newFrom - GetSubjectBounding().GetFrom() );
}

inline void CMappedAlignment::AdjustSubjectToTo( int newTo ) 
{
    AdjustSubjectFromBy( newTo - GetSubjectBounding().GetTo() );
}

END_OLIGOFAR_SCOPES

#endif
