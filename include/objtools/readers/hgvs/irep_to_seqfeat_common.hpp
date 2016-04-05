#ifndef _IREP_TO_SEQFEAT_COMMON_HPP_ 
#define _IREP_TO_SEQFEAT_COMMON_HPP_

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/varrep/varrep__.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/seq/Seq_literal.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CDeltaHelper
{
public:
    static CRef<CDelta_item> CreateSSR(const CCount& count,
                                       CRef<CSeq_literal> seq_literal,
                                       CVariationIrepMessageListener& listener);
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif // _IREP_TO_SEQFEAT_COMMON_HPP_
