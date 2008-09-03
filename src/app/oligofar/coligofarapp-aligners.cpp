#include <ncbi_pch.hpp>
#include "coligofarapp.hpp"
#include "aligners.hpp"

USING_OLIGOFAR_SCOPES;

IAligner * COligoFarApp::CreateAligner() const 
{
    if( m_xdropoff == 0 ) return new CAligner_HSP();

	switch( m_alignmentAlgo ) {
	case eAlignment_fast: return new CAligner_fast();
	case eAlignment_HSP:  return new CAligner_HSP();
    case eAlignment_SW:
		do {
        	auto_ptr<CAligner_SW> swalign( new CAligner_SW() );
        	swalign->SetMatrix().resize( 2*m_xdropoff + 1 );
        	return swalign.release();
    	} while(0); break;
// 	case eAlignment_quick: 
// 		do {
// 			auto_ptr<CAligner_quick> qalign( new CAligner_quick() );
// 			qalign->SetParam() = make_pair( m_xdropoff + 2, m_xdropoff + 1 );
// 			return qalign.release();
// 		} while(0); break;
	default: THROW( logic_error, "Bad value for m_alignmentAlgo" );
	}
}

