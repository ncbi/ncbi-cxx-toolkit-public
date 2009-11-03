#ifndef OLIGOFAR_CSCORINGFACTORY__HPP
#define OLIGOFAR_CSCORINGFACTORY__HPP

#include "cscore-impl.hpp"
#include "calignparam.hpp"

BEGIN_OLIGOFAR_SCOPES

////////////////////////////////////////////////////////////////////////
// Factory to access appropriate scoring functions
class CScoringFactory
{
public:
    CScoringFactory( const CScoreParam * sp, const CAlignParam * ap ) : 
        m_scoreParam( sp ), m_alignParam( ap ), m_sel(0), m_methods( 1 << 6 ) {}
    enum EConvertionTable { eNoConv, eConvTC, eConvAG };
    static unsigned Sel( CSeqCoding::ECoding c, CSeqCoding::EStrand s, EConvertionTable t ) 
        { return ( c << 3 ) | s | ( t << 1 ); }
    void SetQueryCoding( CSeqCoding::ECoding c ) { m_sel = ( m_sel &  7 ) | ( c << 3 ); }
    void SetSubjectStrand( CSeqCoding::EStrand s ) { m_sel = ( m_sel & ~1 ) | s; }
    void SetConvertionTable( EConvertionTable t) { m_sel = ( m_sel & ~6 ) | ( t << 1 ); }
    void SetAlignParam( const CAlignParam * ap ) { m_alignParam = ap; }

    CSeqCoding::ECoding GetQueryCoding() const { return CSeqCoding::ECoding( m_sel >> 3 ); }
    CSeqCoding::EStrand GetSubjectStrand() const { return CSeqCoding::EStrand( m_sel & 1 ); }
    EConvertionTable GetConvertionTable() const { return EConvertionTable( ( m_sel >> 1 ) & 3 ); }
    IScore * GetScoreMethod() { 
        if( m_methods.size() < m_sel ) m_methods.resize( m_sel+1 );
        if( m_methods[m_sel] == 0 ) x_Construct();
        return m_methods[m_sel];
    }
    const CScoreParam * GetScoreParam() const { return m_scoreParam; }
    const CAlignParam * GetAlignParam() const { return m_alignParam; }
    ~CScoringFactory() { ITERATE( vector<IScore*>, v, m_methods ) delete *v; }
protected:
    void x_Construct();
protected:
    const CScoreParam * m_scoreParam;
    const CAlignParam * m_alignParam;
    unsigned m_sel;
    vector<IScore*> m_methods;
};
    
inline void CScoringFactory::x_Construct()
{
    if( IScore * s = m_methods[m_sel] ) {
        delete s;
        m_methods[m_sel] = 0;
    }
    IScore * s = 0;
#define CScoringFactory__SEL( c, s, t ) (( (c) << 3 ) | (s) | ((t) << 1) )
    switch( m_sel ) {
    // NCBI8na
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbi8na, CSeqCoding::eStrand_pos, eNoConv ): 
        s = new CNcbi8naScore<CCvtQryNcbi8naEquiv,CCvtSubjNcbi8naFwd>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbi8na, CSeqCoding::eStrand_neg, eNoConv ): 
        s = new CNcbi8naScore<CCvtQryNcbi8naEquiv,CCvtSubjNcbi8naRev>( m_scoreParam );
        break;
    // NCBI2na + phrap
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbiqna, CSeqCoding::eStrand_pos, eNoConv ): 
        s = new CNcbiqnaScore<CCvtQryNcbi8naEquiv,CCvtSubjNcbi8naFwd>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbiqna, CSeqCoding::eStrand_neg, eNoConv ): 
        s = new CNcbiqnaScore<CCvtQryNcbi8naEquiv,CCvtSubjNcbi8naRev>( m_scoreParam );
        break;
    // NCBIpna
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbipna, CSeqCoding::eStrand_pos, eNoConv ): 
        s = new CNcbipnaScore<CCvtQryNcbipnaEquiv,CCvtSubjNcbi8naFwd>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbipna, CSeqCoding::eStrand_neg, eNoConv ): 
        s = new CNcbipnaScore<CCvtQryNcbipnaEquiv,CCvtSubjNcbi8naRev>( m_scoreParam );
        break;
    // dibase colorspace
    case CScoringFactory__SEL( CSeqCoding::eCoding_colorsp, CSeqCoding::eStrand_pos, eNoConv ): 
        s = new CDibaseScore<CCvtQryNcbi8naEquiv,CCvtSubjNcbi8naFwd>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_colorsp, CSeqCoding::eStrand_neg, eNoConv ): 
        s = new CDibaseScore<CCvtQryNcbi8naEquiv,CCvtSubjNcbi8naRev>( m_scoreParam );
        break;
    // NCBI8na + bisulfite treatment
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbi8na, CSeqCoding::eStrand_pos, eConvTC ):
        s = new CNcbi8naScore<CCvtQryNcbi8naTC,CCvtSubjNcbi8naFwd>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbi8na, CSeqCoding::eStrand_neg, eConvTC ): 
        s = new CNcbi8naScore<CCvtQryNcbi8naTC,CCvtSubjNcbi8naRev>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbi8na, CSeqCoding::eStrand_pos, eConvAG ):
        s = new CNcbi8naScore<CCvtQryNcbi8naAG,CCvtSubjNcbi8naFwd>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbi8na, CSeqCoding::eStrand_neg, eConvAG ): 
        s = new CNcbi8naScore<CCvtQryNcbi8naAG,CCvtSubjNcbi8naRev>( m_scoreParam );
        break;
    // NCBIpna + bisulfite treatment
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbipna, CSeqCoding::eStrand_pos, eConvTC ): 
        s = new CNcbipnaScore<CCvtQryNcbipnaTC,CCvtSubjNcbi8naFwd>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbipna, CSeqCoding::eStrand_neg, eConvTC ): 
        s = new CNcbipnaScore<CCvtQryNcbipnaTC,CCvtSubjNcbi8naRev>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbipna, CSeqCoding::eStrand_pos, eConvAG ): 
        s = new CNcbipnaScore<CCvtQryNcbipnaAG,CCvtSubjNcbi8naFwd>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_ncbipna, CSeqCoding::eStrand_neg, eConvAG ): 
        s = new CNcbipnaScore<CCvtQryNcbipnaAG,CCvtSubjNcbi8naRev>( m_scoreParam );
        break;
    // Colorspace + bisulfite treatment - no actual conversion
    case CScoringFactory__SEL( CSeqCoding::eCoding_colorsp, CSeqCoding::eStrand_pos, eConvTC ): 
        s = new CDibaseScore<CCvtQryNcbi8naEquiv,CCvtSubjNcbi8naFwd>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_colorsp, CSeqCoding::eStrand_neg, eConvTC ): 
        s = new CDibaseScore<CCvtQryNcbi8naEquiv,CCvtSubjNcbi8naRev>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_colorsp, CSeqCoding::eStrand_pos, eConvAG ): 
        s = new CDibaseScore<CCvtQryNcbi8naEquiv,CCvtSubjNcbi8naFwd>( m_scoreParam );
        break;
    case CScoringFactory__SEL( CSeqCoding::eCoding_colorsp, CSeqCoding::eStrand_neg, eConvAG ): 
        s = new CDibaseScore<CCvtQryNcbi8naEquiv,CCvtSubjNcbi8naRev>( m_scoreParam );
        break;

    default: THROW( logic_error, "Scoring can not be initialized for the given combination of "
                     "encoding, bisulfite treatment and strand" ); 
    }
#undef CScoringFactory__SEL
    m_methods[m_sel] = s;
}

END_OLIGOFAR_SCOPES

#endif
