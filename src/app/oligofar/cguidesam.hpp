#ifndef OLIGOFAR_CGUIDEFILE__HPP
#define OLIGOFAR_CGUIDEFILE__HPP

#include "defs.hpp"
#include "csam.hpp"
#include "cfilter.hpp"
#include "iguidefile.hpp"

BEGIN_OLIGOFAR_SCOPES

class CSeqIds;
class CGuideSamFile : public IGuideFile, public CSamBase
{
public:
    CGuideSamFile( const string& fileName, CFilter& filter, CSeqIds& seqIds, CScoreParam& scoreParam );
    
    bool NextHit( Uint8 ordinal, CQuery * query );

    class C_Key
    {
    public:
        static Uint8 NullSeq() { return ~Uint8(0); }
        C_Key( Uint8 seq1, int pos1, Uint8 seq2, int pos2, Uint1 flags, int lineno );
        bool  GetSwap() const { return m_swap; }
        Uint1 GetFlags() const { return m_flags; }
        Uint8 GetSeq( int mate ) const { return m_seq[mate]; }
        int   GetPos( int mate ) const { return m_pos[mate]; }
        bool  IsReverse( int mate ) const { return mate == 0 ? m_flags & fSeqReverse : m_flags & fMateReverse; }
        bool  IsPairedHit() const { return m_flags & fPairedHit; }
        bool  IsOrderReverse() const { return m_seq[0] > m_seq[1]; }
        bool operator < ( const C_Key& other ) const {
#define C_Key_CMP( a ) if( a < other.a ) return true; if( a > other.a ) return false; 
            C_Key_CMP( m_seq[0] );
            C_Key_CMP( m_pos[0] );
            C_Key_CMP( m_seq[1] );
            C_Key_CMP( m_pos[1] );
#undef  C_Key_CMP
            return m_flags < other.m_flags; 
        }
        bool operator == ( const C_Key& other ) const {
#define C_Key_CMP( a ) if( a != other.a ) return false
            C_Key_CMP( m_seq[0] );
            C_Key_CMP( m_pos[0] );
            C_Key_CMP( m_seq[1] );
            C_Key_CMP( m_pos[1] );
#undef  C_Key_CMP
            return m_flags == other.m_flags; 
        }
    protected:
        Uint8 m_seq[2];
        int   m_pos[2];
        Uint1 m_flags;
        bool  m_swap;
    };

    class C_Value : public CSamBase
    {
    public:
        C_Value() {}
        C_Value( const C_Key& key, const string& cigar, const string& mismatches, const string& sequence = "", const string& qual = "" ) {
            m_cigar[key.GetSwap()] = cigar;
            m_mismatches[key.GetSwap()] = mismatches;
            m_sequence[key.GetSwap()] = sequence;
            m_qual[key.GetSwap()] = qual; 
        }
        const string& GetCigar( int mate ) const { return m_cigar[mate]; }
        const string& GetMismatches( int mate ) const { return m_mismatches[mate]; }
        const string& GetSequence( int mate ) const { return m_sequence[mate]; }
        const string& GetQuality( int mate ) const { return m_qual[mate]; }
        void Adjust( const C_Key& key, const string& cigar, const string& mismatches, const string& sequence = "", const string& qual ) {
            m_cigar[key.GetSwap()] = cigar;
            m_mismatches[key.GetSwap()] = mismatches;
            m_sequence[key.GetSwap()] = sequence;
            m_qual[key.GetSwap()] = qual;
        }
        bool IsValid() const { return m_cigar[0].length(); }
        bool IsPairedHit() const { return m_cigar[1].length(); } 
    protected:
        string m_cigar[2];
        string m_mismatches[2];
        string m_sequence[2];
        string m_qual[2];
    };
    typedef multimap<C_Key,C_Value> THitMap;
    void   AddHit( CQuery * query, const C_Key& key, const C_Value& value );
    double ComputeScore( CQuery * query, const C_Value& value, int mate );

    static TTrVector GetFullCigar( const TTrVector& cigar, const string& mism );

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

