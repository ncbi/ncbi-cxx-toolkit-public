#ifndef _SPECIAL_IREP_TO_SEQFEAT_HPP_
#define _SPECIAL_IREP_TO_SEQFEAT_HPP_

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/varrep/varrep__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CRef<CSeq_feat> g_CreateSpecialSeqfeat(ESpecialVariant variant, const CSeq_id& seq_id, string hgvs_expression);

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
