#ifndef CSAMRECORDS__HPP
#define CSAMRECORDS__HPP

#include "csam.hpp"
#include "ctranscript.hpp"
#include <util/range.hpp>

BEGIN_OLIGOFAR_SCOPES

class CSamRecord : public CSamBase
{
public:
    typedef TTrSequence TCigar;
    typedef unsigned short TFlags;
    typedef unsigned char  TMapQ;
    typedef int TPos; // 29 bits in SAM 1.2
    typedef int TOff; // 29 bits in SAM 1.2
    typedef CRange<int> TRange;
    typedef map<string,string> TTags;

    const char * GetQName() const { return m_qname; }
    const char * GetRName() const { return m_rname; }
    TFlags GetFlags() const { return m_flags; }
    TMapQ  GetMapQual() const { return m_mapq; }
    TPos   GetPos() const { return m_pos; }
    const TCigar& GetImprovedCigar() const { return m_cigar; } 
    TCigar GetBasicCigar() const; // is generated from m_cigar 
    const char * GetMateRefName() const; // should be 'refname' (for '=') or '' (for '*') in this implementation, and not '' ONLY for paired hits
    TPos   GetMatePos() const { return m_mpos; }
    TOff   GetInsertSize() const { return m_isize; }
    const char * GetQueryIupac() const { return m_qiupac; }
    const char * GetQueryQual() const { return m_qqual; }
    
    bool HasTags() const { return m_tags && m_tags->size(); }
    const TTags& GetTags() const { return *m_tags; }
    ESamTagType GetTagType( const string& tname ) const;
    string GetTagString( const string& name ) const;
    int GetTagInt( const string& name ) const;
    float GetTagFloat( const string& name ) const;
    
    void DropTag( const string& name );
    void SetTagString( const string& name, const string& val );
    void SetTagInt( const string& name, int val );
    void SetTagFloat( const string& name, float val );

    bool IsPairedRead() const { return m_flags & fPairedQuery; }
    bool IsPairedHit() const { return m_flags & fPairedHit; }
    int  GetComponent() const { return m_flags & fSeqIsSecond ? 1 : 0; }
    bool IsUnmapped() const { return m_flags & fSeqUnmapped; }
    bool IsReverse() const { return m_flags & fSeqReverse; }
    bool IsNull() const { return m_qname == 0 || *m_qname == 0; }

    TRange GetQryBounding() const;
    TRange GetRefBounding() const;

    bool IsQueryMateOf( const CSamRecord& other ) const;
    bool IsHitMateOf( const CSamRecord& other ) const;

    void WriteAsSam( ostream& out, bool basic = false ) const;
    CSamRecord();
    CSamRecord( const string& samline );
    CSamRecord( const CSamRecord& other );
    ~CSamRecord();

    CSamRecord& operator = ( const CSamRecord& other );

    void SetNull();
    void Init( const string& line );
    void InitUnmapped(  const string& qname, TFlags flags, const string& seq, const string& qual = "" );
    void InitSingleHit( const string& qname, TFlags flags, const string& rname, TPos pos, TMapQ mapq, const TCigar& cigar, const string& seq, const string& qual = "" );
    void InitPairedHit( const string& qname, TFlags flags, const string& rname, TPos pos, TMapQ mapq, const TCigar& cigar, TPos mpos, TOff isize, 
            const string& seq, const string& qual = "" );

    TFlags GetFlags( TFlags mask ) const { return m_flags & mask; }
    void SetFlags( TFlags mask, bool on = true ) { if( on ) m_flags |= mask; else m_flags &= ~mask; }
    bool IsSetAnyFlag( TFlags mask ) const { return m_flags & mask; }
    bool AreSetAllFlags( TFlags mask ) const { return (m_flags & mask) == mask; }
    bool FlagsEqual( TFlags value, TFlags mask = ~TFlags(0) ) const { return (m_flags & mask) == (value & mask); } 

    double ComputeScore( double id = 1, double mm = -1, double go = -3, double ge = -1.5 ) const 
    { return ComputeScore( m_cigar, id, mm, go, ge ); }
    static double ComputeScore( const TCigar& cigar, double id = 1, double mm = -1, double go = -3, double ge = -1.5 );

    void SetSingle() { m_flags &= ~fPairedHit; m_mpos = m_isize = 0; }
    void SetPaired( CSamRecord& other );
protected:
    void x_InitStrings( const char * qname, int qnlen, const char * rname, int rnlen, const char * iupac, int iulen, const char * qual, int qlen );
    void x_InitStrings( const char * qname, const char * rname, const char * iupac, const char * qual );
    void x_InitStrings( const string& qname, const string& rname, const string& iupac, const string& qual );

protected:
    char * m_qname;
    char * m_rname;
    char * m_qiupac;
    char * m_qqual;
    TFlags m_flags;
    TMapQ  m_mapq;
    TPos   m_pos;
    TPos   m_mpos;
    TOff   m_isize;
    TCigar m_cigar;
    TTags * m_tags;
};

class CSamMdParser : public CTrBase
{
public:
    typedef CSamRecord::TCigar TCigar;
    CSamMdParser( const TCigar& cigar, const string& mdtag ) 
        : m_cigar( cigar ), m_mdtag( mdtag ), m_current( cigar.begin() ) {}
    TCigar GetFullCigar();
    int GetCount() const { return m_ccount; }
    EEvent GetEvent() const { return m_cevent; }
    EEvent NextEvent() { 
        if( m_current == m_cigar.end() ) return m_cevent = eEvent_NULL;
        --m_ccount; 
        if( m_ccount == 0 ) {
            if( ++m_current == m_cigar.end() ) return m_cevent = eEvent_NULL;
            else {
                m_ccount = m_current->GetCount();
                return m_cevent = m_current->GetEvent();
            }
        } else return m_cevent;
    }
protected:
    const TCigar& m_cigar;
    const string& m_mdtag;
    TCigar::const_iterator m_current;
    const char * m_mdptr;
    EEvent m_cevent;
    int m_ccount;
    int m_mcount;
};

END_OLIGOFAR_SCOPES

#endif
