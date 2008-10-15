#ifndef OLIGOFAR_ALIGNERS__HPP
#define OLIGOFAR_ALIGNERS__HPP

#include "daligner.hpp"
#include "taligner_sw.hpp"
#include "taligner_hsp.hpp"
#include "taligner_fast.hpp"
//#include "taligner_quick.hpp"

BEGIN_OLIGOFAR_SCOPES

DECLARE_AND_DEFINE_ALIGNER( SW, CAligner_SW_Base::TMatrix, Matrix )
DECLARE_AND_DEFINE_ALIGNER( HSP, int, foo )
DECLARE_AND_DEFINE_ALIGNER( fast, int, foo )
//DECLARE_AND_DEFINE_ALIGNER( quick, CAligner_quick_Base::TParam, Param )

END_OLIGOFAR_SCOPES

#endif
